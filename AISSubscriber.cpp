#include "AISSubscriber.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <QDebug>
#include "mainwindow.h"
#include "SettingsManager.h"

AISSubscriber::AISSubscriber(QObject* parent)
    : QObject(parent),
    socket(nullptr),
    countdownTimer(new QTimer(this)),
    reconnectAttempts(0),
    baseDelay(5000),         // 5 detik
    maxDelay(3600000),        // 1 jam
    dialogIsOpen(false)
{
    countdownTimer->setInterval(1000); // 1 detik per hitungan mundur
    connect(countdownTimer, &QTimer::timeout, this, [this]() {
        if (--countdownSeconds > 0) {
            QString message = "Reconnect in " + formatCountdownTime(countdownSeconds);
            //qDebug() << message;
            if (mainWindow) {
                mainWindow->setReconnectStatusText(message);
            }
        } else {
            if (dialogIsOpen){
                // jangan reconnect, tunggu dialog ditutup
                countdownSeconds = 1; // tahan di 1 detik
                QString message = "Reconnect paused (settings dialog open)";
                if (mainWindow) {
                    mainWindow->setReconnectStatusText(message);
                }
            }
            else {
                countdownTimer->stop();
                this->connectToHost(lastHost, lastPort);
            }
        }
    });

    // No-data watchdog: if no data arrives for a while, mark disconnected
    noDataTimer = new QTimer(this);
    noDataTimer->setInterval(noDataIntervalMs);
    connect(noDataTimer, &QTimer::timeout, this, [this]() {
        if (socket && socket->state() == QAbstractSocket::ConnectedState) {
            qWarning() << "[AISSubscriber] No data for" << noDataIntervalMs << "ms. Forcing disconnect.";
            hasReceivedData = false;
            dataFlag = false;
            emit connectionStatusChanged(false);
            socket->disconnectFromHost();
            tryReconnect();
        }
        noDataTimer->stop();
    });
}

AISSubscriber::~AISSubscriber() {
    // Stop all timers to prevent race conditions during destruction
    shuttingDown = true;

    if (countdownTimer) {
        countdownTimer->stop();
    }

    if (noDataTimer) {
        noDataTimer->stop();
    }

    // Clean up socket
    if (socket) {
        socket->disconnectFromHost();
        socket->deleteLater();
        socket = nullptr;
    }

    qDebug() << "AISSubscriber destroyed safely";
}

void AISSubscriber::connectToHost(const QString &host, quint16 port) {
    QString dHost = SettingsManager::instance().data().moosIp;
    int dPort = 5000;

    // lastHost = host;
    // lastPort = port;

    lastHost = dHost;
    lastPort = dPort;

    if (socket) {
        socket->abort();  // pastikan socket bersih
        socket->deleteLater();
    }

    hasReceivedData = false;
    dataFlag = false;

    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &AISSubscriber::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &AISSubscriber::onDisconnected);
    connect(socket, &QTcpSocket::connected, this, &AISSubscriber::onConnected);

    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &AISSubscriber::onSocketError);

    QString connection = "Connecting to host " + host + ":" + QString::number(port);
    if (mainWindow){
        mainWindow->setReconnectStatusText(connection);
    }

    if (noDataTimer) noDataTimer->stop();
    socket->connectToHost(host, port);
}

void AISSubscriber::onConnected() {
    QString connection = "Connecting to MOOSDB...";
    if (mainWindow){
        mainWindow->setReconnectStatusText(connection);
    }

    // Timeout kalau 10 detik gak ada data (initial)
    QTimer::singleShot(11000, this, [this]() {
        if (!hasReceivedData && socket && socket->state() == QAbstractSocket::ConnectedState) {
            QString qconnection = "MOOSDB not connected, reconnecting...";
            if (mainWindow){
                mainWindow->setReconnectStatusText(qconnection);
            }
            socket->disconnectFromHost();
            tryReconnect();
        }
    });

    // Start runtime no-data watchdog
    if (noDataTimer) noDataTimer->start();
}

void AISSubscriber::disconnectFromHost() {
    if (socket) {
        socket->disconnectFromHost();
        socket->deleteLater();
        socket = nullptr;

        qDebug() << "Disconnected";
        dataFlag = false;

        // Tunda reconnect 100ms supaya socket benar-benar bersih, kecuali saat shutdown
        if (!shuttingDown) {
            QTimer::singleShot(100, this, &AISSubscriber::tryReconnect);
        }
        if (noDataTimer) noDataTimer->stop();
    }
}

void AISSubscriber::setShuttingDown(bool v) {
    shuttingDown = v;
    if (noDataTimer && shuttingDown) {
        noDataTimer->stop();
    }
}

void AISSubscriber::onReadyRead() {
    QByteArray data = socket->readAll();
    data = data.simplified().trimmed().replace("\r", "");

    // CONNECTION STATUS FLAG
    if (!hasReceivedData) {
        hasReceivedData = true;
        dataFlag = true;
        qDebug() << "[AISSubscriber] Data received. Connection status changed";

        reconnectAttempts = 0; // reset backoff karena berhasil konek
        countdownTimer->stop();

        emit connectionStatusChanged(true);

        if (mainWindow) {
            mainWindow->setReconnectStatusText("");
        }

        // TIMER
        emit startDrawTimer();
    }

    // reset runtime no-data watchdog on every data
    if (noDataTimer) noDataTimer->start();

    QList<QByteArray> lines;

    int pos = 0;
    while (pos < data.size()) {
        int next = data.indexOf("}{", pos);
        if (next == -1) {
            lines << data.mid(pos);  // terakhir
            break;
        } else {
            lines << data.mid(pos, next - pos + 1);  // include penutup '}'
            pos = next + 1;  // mulai dari '{' berikutnya
        }
    }

    // iterasi aman pakai indeks karena QList<QByteArray> bisa detach
    for (int i = 0; i < lines.size(); ++i) {
        const QByteArray &line = lines[i];
        if (line.trimmed().isEmpty())
            continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);

        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "Invalid JSON:" << line;
            continue;
        }

        QJsonObject obj = doc.object();

        auto extractDouble = [&](const QString &key, std::function<void(double)> emitter) {
            if (obj.contains(key)) {
                QJsonValue val = obj[key];
                if(!val.isNull()){
                    double d = val.isDouble() ? val.toDouble() : val.toString().toDouble();
                    emitter(d);
                }
            }
        };

        auto extractString = [&](const QString &key, std::function<void(QString)> emitter) {
            if (obj.contains(key)) {
                QJsonValue val = obj[key];
                if(!val.isNull()){
                    QString s = val.toString();
                    emitter(s);
                }
            }
        };

        bool hasLat = false, hasLon = false;
        bool hasAis = false;
        double lat = 0.0, lon = 0.0, hog = 0.0, cog = 0.0, sog = 0.0, hdg = 0.0, dep = 0.0, spd = 0.0, yaw = 0.0, z = 0.0;
        QString ais;

        extractDouble("NAV_LAT", [&](double v){
            if (v != 0.0){
                lat = v;
                hasLat = true;
                emit navLatReceived(v);
            }
        });

        extractDouble("NAV_LONG", [&](double v){
            if (v != 0.0){
                lon = v;
                hasLon = true;
                emit navLongReceived(v);
            }
        });

        extractString("WAIS_NMEA", [&](QString v){
            if (!v.isEmpty()){
                ais = v;
                hasAis = true;
                emit waisNmeaReceived(v);
            }
        });

        extractDouble("NAV_HEADING", [&](double v){
            hdg = v;
            emit navHeadingReceived(v);
        });

        extractDouble("NAV_HEADING_OVER_GROUND", [&](double v){
            hog = v;
            emit navHeadingOGReceived(v);
        });

        extractDouble("NAV_SOG", [&](double v){
            sog = v;
            emit navSOGReceived(v);
        });

        extractDouble("NAV_COG", [&](double v){
            cog = v;
            emit navCourseOGReceived(v);
        });

        extractString("NAV_LAT_DMS", [&](QString v){ emit navLatDmsReceived(v);});
        extractString("NAV_LONG_DMS", [&](QString v){ emit navLongDmsReceived(v);});
        extractString("NAV_LAT_DMM", [&](QString v){ emit navLatDmmReceived(v);});
        extractString("NAV_LONG_DMM", [&](QString v){ emit navLongDmmReceived(v);});

        extractDouble("NAV_SPEED_OVER_GROUND", [=](double v){ emit navSpeedOGReceived(v);});
        extractDouble("NAV_DEPTH", [=](double v){ emit navDepthReceived(v);});
        extractDouble("NAV_SPEED", [=](double v){ emit navSpeedReceived(v);});
        extractDouble("NAV_YAW", [=](double v){ emit navYawReceived(v);});
        extractDouble("NAV_Z", [=](double v){ emit navZReceived(v);});
        extractDouble("NAV_STW", [=](double v){ emit navStwReceived(v);});
        extractDouble("NAV_DRIFT", [=](double v){ emit navDriftReceived(v);});
        extractDouble("NAV_DRAFT", [=](double v){ emit navDraftReceived(v);});
        extractDouble("NAV_DRIFT_ANGLE", [=](double v){ emit navDriftAngleReceived(v);});
        extractDouble("NAV_SET", [=](double v){ emit navSetReceived(v);});
        extractDouble("NAV_ROT", [=](double v){ emit navRotReceived(v);});
        extractDouble("NAV_DEPTH_BELOW_KEEL", [=](double v){ emit navDepthBelowKeelReceived(v);});

        extractString("NAV_DR", [=](QString v){ emit navDeadReckonReceived(v);});

        extractString("MAP_INFO_REQ", [=](QString v){ emit mapInfoReqReceived(v);});

        if (hasLat && hasLon) {
            emit processingData(lat, lon, cog, sog, hdg, spd, dep, yaw, z);
        }
        if (hasAis){
            emit processingAis(ais);
        }

        // ROUTES INFORMATION
        extractDouble("RTE_WP_BRG", [=](double v){ emit rteWpBrgReceived(v); });
        extractString("RTE_XTD", [=](QString v){ emit rteXtdReceived(v); });
        extractDouble("RTE_CRS", [=](double v){ emit rteCrsReceived(v); });
        extractDouble("RTE_CTM", [=](double v){ emit rteCtmReceived(v); });
        extractDouble("RTE_DTG", [=](double v){ emit rteDtgReceived(v); });
        extractDouble("RTE_DTG_M", [=](double v){ emit rteDtgMReceived(v); });
        extractString("RTE_TTG", [=](QString v){ emit rteTtgReceived(v); });
        extractString("RTE_ETA", [=](QString v){ emit rteEtaReceived(v); });
    }
}

void AISSubscriber::onSocketError(QAbstractSocket::SocketError) {
    emit errorOccurred(socket->errorString());
    hasReceivedData = false;
    dataFlag = false;
    emit connectionStatusChanged(false);
    QString connection = "TCP not connected, reconnecting...";
    if (mainWindow){
        mainWindow->setReconnectStatusText(connection);
    }
    if (noDataTimer) noDataTimer->stop();
    tryReconnect();
}

void AISSubscriber::onDisconnected() {
    emit disconnected();
    qDebug() << "[AIS] Disconnected.";

    emit connectionStatusChanged(false);  // ✅ emit jika disconnect
    tryReconnect();
    if (noDataTimer) noDataTimer->stop();
}

void AISSubscriber::tryReconnect()
{
    // Hitung delay exponential
    int delay = qMin(baseDelay * (1 << reconnectAttempts), maxDelay);

    // Tambahkan jitter acak 0–1000 ms
    delay += QRandomGenerator::global()->bounded(1000);

    countdownSeconds = delay / 1000;
    reconnectAttempts++;  // tingkatkan percobaan

    QString status = QString("Will retry in %1")
                         .arg(formatCountdownTime(countdownSeconds));
    qDebug() << status;

    if (mainWindow) {
        mainWindow->setReconnectStatusText(status);
    }

    countdownTimer->start(); // mulai countdown
}

QString AISSubscriber::formatCountdownTime(int totalSeconds) {
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    if (minutes > 0 && seconds > 0) {
        return QString("%1 minute%2 %3 second%4")
        .arg(minutes)
            .arg(minutes == 1 ? "" : "s")
            .arg(seconds)
            .arg(seconds == 1 ? "" : "s");
    } else if (minutes > 0) {
        return QString("%1 minute%2")
        .arg(minutes)
            .arg(minutes == 1 ? "" : "s");
    } else {
        return QString("%1 second%2")
        .arg(seconds)
            .arg(seconds == 1 ? "" : "s");
    }
}

void AISSubscriber::setMainWindow(MainWindow* mw){
    mainWindow = mw;
}

bool AISSubscriber::hasData(){
    return dataFlag;
}

void AISSubscriber::setDialogOpen(bool open) {
    dialogIsOpen = open;
}
