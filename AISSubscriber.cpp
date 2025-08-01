#include "AISSubscriber.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <QDebug>

AISSubscriber::AISSubscriber(QObject *parent) : QObject(parent) {}

void AISSubscriber::connectToHost(const QString &host, quint16 port) {
    if (socket) return;

    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &AISSubscriber::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &AISSubscriber::onDisconnected);
    connect(socket, &QTcpSocket::connected, this, [this]() {
        emit connectionStatusChanged(true);  // ✅ emit jika connected
    });
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &AISSubscriber::onSocketError);

    socket->connectToHost(host, port);
}

void AISSubscriber::disconnectFromHost() {
    if (socket) {
        socket->disconnectFromHost();
        socket->deleteLater();
        socket = nullptr;

        qDebug() << "Disconnected";
    }
}

void AISSubscriber::onReadyRead() {
    QByteArray data = socket->readAll();
    data = data.simplified().trimmed().replace("\r", "");

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
                double d = val.isDouble() ? val.toDouble() : val.toString().toDouble();
                emitter(d);
            }
        };

        auto extractString = [&](const QString &key, std::function<void(QString)> emitter) {
            if (obj.contains(key)) {
                QJsonValue val = obj[key];
                QString s = val.toString();
                emitter(s);
            }
        };

        bool hasLat = false, hasLon = false;
        bool hasAis = false;
        double lat = 0.0, lon = 0.0, cog = 0.0, sog = 0.0, hdg = 0.0, dep = 0.0, spd = 0.0, yaw = 0.0, z = 0.0;
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
            cog = v;
            emit navHeadingOGReceived(v);
        });

        extractDouble("NAV_SPEED_OVER_GROUND", [&](double v){
            sog = v;
            emit navSpeedOGReceived(v);
        });

        extractDouble("NAV_DEPTH", [=](double v){ emit navDepthReceived(v);});
        extractDouble("NAV_SPEED", [=](double v){ emit navSpeedReceived(v);});
        extractDouble("NAV_YAW", [=](double v){ emit navYawReceived(v);});
        extractDouble("NAV_Z", [=](double v){ emit navZReceived(v);});
        extractDouble("NAV_STW", [=](double v){ emit navStwReceived(v);});
        extractDouble("NAV_DRIFT", [=](double v){ emit navDriftReceived(v);});
        extractDouble("NAV_DRIFT_ANGLE", [=](double v){ emit navDriftAngleReceived(v);});
        extractDouble("NAV_SET", [=](double v){ emit navSetReceived(v);});
        extractDouble("NAV_ROT", [=](double v){ emit navRotReceived(v);});
        extractDouble("NAV_DEPTH_BELOW_KEEL", [=](double v){ emit navDepthBelowKeelReceived(v);});

        extractString("MAP_INFO_REQ", [=](QString v){ emit mapInfoReqReceived(v);});

        if (hasLat && hasLon) { emit processingData(lat, lon, cog, sog, hdg, spd, dep, yaw, z);}
        if (hasAis){ emit processingAis(ais);}

        // ROUTES INFORMATION
        extractString("RTE_WP_BRG", [=](QString v){ emit rteWpBrgReceived(v); });
        extractString("RTE_XTD", [=](QString v){ emit rteXtdReceived(v); });
        extractString("RTE_CRS", [=](QString v){ emit rteCrsReceived(v); });
        extractString("RTE_CTM", [=](QString v){ emit rteCtmReceived(v); });
        extractString("RTE_DTG", [=](QString v){ emit rteDtgReceived(v); });
        extractString("RTE_DTG_M", [=](QString v){ emit rteDtgMReceived(v); });
        extractString("RTE_TTG", [=](QString v){ emit rteTtgReceived(v); });
        extractString("RTE_ETA", [=](QString v){ emit rteEtaReceived(v); });
    }
}


void AISSubscriber::onSocketError(QAbstractSocket::SocketError) {
    emit errorOccurred(socket->errorString());
}

void AISSubscriber::onDisconnected() {
    emit disconnected();
}
