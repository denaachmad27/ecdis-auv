#include "AISSubscriber.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <QDebug>
#include <QPointer>
#include <QRandomGenerator>
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
    qDebug() << "[AISSUBSCRIBER] Destructor START";

    // CRITICAL: Stop all timers FIRST to prevent any callbacks during destruction
    shuttingDown = true;

    if (countdownTimer) {
        countdownTimer->stop();
    }

    if (noDataTimer) {
        noDataTimer->stop();
    }

    // Clean up socket - abort first to stop any pending operations
    if (socket) {
        qDebug() << "[AISSUBSCRIBER] Cleaning up socket...";

        // Abort any pending operations
        socket->abort();

        // Disconnect all signals to prevent callbacks
        socket->disconnect(this);

        // Schedule for deletion - Qt will delete it when safe
        socket->deleteLater();
        socket = nullptr;

        qDebug() << "[AISSUBSCRIBER] Socket cleanup complete";
    }

    qDebug() << "[AISSUBSCRIBER] Destructor END";
}

void AISSubscriber::connectToHost(const QString &host, quint16 port) {
    QString dHost = SettingsManager::instance().data().moosIp;
    int dPort = 5000;

    // lastHost = host;
    // lastPort = port;

    lastHost = dHost;
    lastPort = dPort;

    // CRITICAL: Delete old socket SAFELY before creating new one
    if (socket) {
        socket->abort();  // pastikan socket bersih
        socket->disconnect(this);  // Disconnect all signals FIRST
        socket->deleteLater();
        socket = nullptr;  // Nullify BEFORE creating new socket
    }

    hasReceivedData = false;
    dataFlag = false;

    // Create new socket AFTER old one is cleaned up
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
    // CRITICAL: Use QPointer to safely check if socket still exists when lambda executes
    QPointer<QTcpSocket> socketPtr = socket;

    QTimer::singleShot(11000, this, [this, socketPtr]() {
        // Check if we're shutting down before accessing anything
        if (shuttingDown) {
            return;
        }

        // Check if socket still exists (using QPointer)
        if (!socketPtr) {
            return;  // Socket was deleted, don't access it
        }

        if (!hasReceivedData && socketPtr && socketPtr->state() == QAbstractSocket::ConnectedState) {
            QString qconnection = "MOOSDB not connected, reconnecting...";
            if (mainWindow){
                mainWindow->setReconnectStatusText(qconnection);
            }
            socketPtr->disconnectFromHost();
            tryReconnect();
        }
    });

    // Start runtime no-data watchdog
    if (noDataTimer) noDataTimer->start();
}

void AISSubscriber::disconnectFromHost() {
    if (socket) {
        socket->disconnectFromHost();
        // Don't delete here - let the destructor handle cleanup
        // socket->deleteLater();
        // socket = nullptr;

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
    // CRITICAL: Check if socket still exists and we're not shutting down
    if (!socket || shuttingDown) {
        return;
    }

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

    // === STREAMING JSON PARSER ===
    // Append new data to buffer
    bool wasEmpty = buffer.isEmpty();
    buffer.append(data);

    // Reset/start buffer timer for timeout detection
    if (wasEmpty) {
        bufferTimer.start();
    }

    // Try to find and process complete JSON objects
    while (true) {
        // Check if we should flush the buffer (invalid data)
        if (shouldFlushBuffer(buffer)) {
            qWarning() << "[AISSubscriber] Flushing invalid buffer:" << buffer.left(200);
            buffer.clear();
            bufferTimer.start();
            break;
        }

        // Try to find a complete JSON object
        int jsonEnd = findCompleteJSON(buffer);
        if (jsonEnd <= 0) {
            // No complete JSON found, wait for more data
            break;
        }

        // Extract the complete JSON object
        QByteArray json = buffer.left(jsonEnd);
        buffer.remove(0, jsonEnd);

        // Reset timer since we made progress
        bufferTimer.start();

        // Parse and process the JSON
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(json, &err);

        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "Invalid JSON:" << json;
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
        extractString("NAV_NAME", [&](QString v){ emit navNameReceived(v);});

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

        // NODE_NAME_ALL - Parse and store node names
        extractString("NODE_NAME_ALL", [=](QString v){
            if (!v.isEmpty()) {
                // Parse the comma-separated node names
                nodeNameList.clear();
                QStringList names = v.split(',', Qt::SkipEmptyParts);
                for (const QString &name : names) {
                    nodeNameList.append(name.trimmed().toUpper());
                }
                emit nodeNameAllReceived(v);
            }
        });

        // Dynamic NODE_REPORT_* extraction based on NODE_NAME_ALL content
        if (!nodeNameList.isEmpty()) {
            for (const QString &nodeName : nodeNameList) {
                QString nodeReportKey = "NODE_REPORT_" + nodeName;
                extractString(nodeReportKey, [=](QString v){
                    if (!v.isEmpty()) {
                        // Emit raw data first
                        emit nodeReportReceived(nodeName, v);

                        // Parse the NODE_REPORT data
                        // Format: "NAME=archie,X=177.14,Y=183.33,SPD=2.87,HDG=29.98,DEP=0,LAT=-4.32439555,LON=70.32817697,TYPE=kayak,MODE=MODE:ACTIVE:SURVEYING,ALLSTOP=clear,INDEX=2879,YAW=1.0,TIME=1763534205.16,LENGTH=4"

                        QString name, type, mode;
                        double x = std::numeric_limits<double>::quiet_NaN();
                        double y = std::numeric_limits<double>::quiet_NaN();
                        double spd = std::numeric_limits<double>::quiet_NaN();
                        double hdg = std::numeric_limits<double>::quiet_NaN();
                        double dep = std::numeric_limits<double>::quiet_NaN();
                        double lat = std::numeric_limits<double>::quiet_NaN();
                        double lon = std::numeric_limits<double>::quiet_NaN();
                        double yaw = std::numeric_limits<double>::quiet_NaN();
                        double time = std::numeric_limits<double>::quiet_NaN();
                        int index = 0;

                        // Parse key=value pairs
                        QStringList pairs = v.split(',', Qt::SkipEmptyParts);
                        for (const QString &pair : pairs) {
                            QStringList keyValue = pair.split('=', Qt::SkipEmptyParts);
                            if (keyValue.size() == 2) {
                                QString key = keyValue[0].trimmed();
                                QString value = keyValue[1].trimmed();

                                if (key == "NAME") name = value;
                                else if (key == "X") x = value.toDouble();
                                else if (key == "Y") y = value.toDouble();
                                else if (key == "SPD") spd = value.toDouble();
                                else if (key == "HDG") hdg = value.toDouble();
                                else if (key == "DEP") dep = value.toDouble();
                                else if (key == "LAT") lat = value.toDouble();
                                else if (key == "LON") lon = value.toDouble();
                                else if (key == "TYPE") type = value;
                                else if (key == "MODE") mode = value;
                                else if (key == "INDEX") index = value.toInt();
                                else if (key == "YAW") yaw = value.toDouble();
                                else if (key == "TIME") time = value.toDouble();
                            }
                        }

                        // Emit parsed data
                        emit nodeShipDataReceived(nodeName, name, x, y, spd, hdg, dep, lat, lon, type, mode, index, yaw, time);
                    }
                });
            }
        }

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
    // CRITICAL: Check if socket still exists
    if (!socket) {
        return;
    }

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

// === STREAMING JSON PARSER HELPER FUNCTIONS ===

int AISSubscriber::findCompleteJSON(const QByteArray &data) {
    if (data.isEmpty()) {
        return 0;
    }

    // Find the first opening brace
    int start = -1;
    for (int i = 0; i < data.size(); ++i) {
        if (data[i] == '{') {
            start = i;
            break;
        }
    }

    if (start == -1) {
        return 0;  // No opening brace found
    }

    // Count braces to find matching closing brace
    int braceCount = 0;
    for (int i = start; i < data.size(); ++i) {
        if (data[i] == '{') {
            braceCount++;
        } else if (data[i] == '}') {
            braceCount--;
            if (braceCount == 0) {
                return i + 1;  // Return position after closing brace
            }
        }
    }

    return -1;  // Incomplete JSON
}

bool AISSubscriber::shouldFlushBuffer(const QByteArray &buffer) {
    if (buffer.isEmpty()) {
        return false;
    }

    // Condition 3: Check for invalid patterns (NAV_, NODE_ without opening brace)
    QString str = QString::fromUtf8(buffer).trimmed();
    if (str.startsWith("NAV_") || str.startsWith("NODE_")) {
        return true;  // Invalid data, flush buffer
    }

    // Condition 5: Large buffer (>50KB) - flush to prevent memory issues
    if (buffer.size() > 50000) {
        return true;
    }

    // Condition 6: Timeout (>10 seconds) - data is stale
    if (bufferTimer.hasExpired(10000)) {
        return true;
    }

    // Condition 1 & 2: Valid JSON patterns - don't flush
    if (str.startsWith('{') || str.startsWith('"')) {
        return false;
    }

    // Condition 4: Small unclear buffer - wait for more data
    if (buffer.size() < 1000) {
        return false;
    }

    // Default: flush unclear data
    return true;
}
