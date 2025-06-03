#include "moosdb.h"

// Global Variables
QTcpServer *serverS = nullptr;
QTcpSocket *clientS = nullptr;
QTcpServer *serverM = nullptr;
QTcpSocket *clientM = nullptr;
QTcpServer *serverP = nullptr;
QTcpSocket *clientP = nullptr;
bool runningS = false;
bool runningP = false;
bool runningM = false;

QTextEdit *nmeaText;
QTextEdit *aisText;
QTextEdit *ownShipText;

QTextEdit *aisTemp;
QTextEdit *ownShipTemp;

//QString moosdb_host = "172.20.97.237";
QString moosdb_host = "172.22.25.17";
int moosdb_port = 5000;

MOOSShip mapShip = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

MOOSShip navShip = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

QString aivdo;

// Thread Subscribe from MOOSDB
QString serverThreadMOOSSubscribe() {
    serverS = new QTcpServer();
    
    if (!serverS->listen(QHostAddress::Any, 8080)) {
        qDebug() << "Server failed to start.";
        return aivdo;
    }
    qDebug() << "Waiting for connection...";

    QObject::connect(serverS, &QTcpServer::newConnection, [=]() {
        clientS = serverS->nextPendingConnection();
        qDebug() << "Client connected!";

        QObject::connect(clientS, &QTcpSocket::readyRead, [=]() {
            QByteArray data = clientS->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
            QJsonObject jsonData = jsonDoc.object();

            // Pastikan JSON memiliki key yang benar
            if (jsonData.contains("NAV_LAT") && jsonData.contains("NAV_LONG")) {
                navShip.lat = jsonData["NAV_LAT"].toDouble();
                navShip.lon = jsonData["NAV_LONG"].toDouble();


                // ================ FORMAT JSON =============== //
                // qDebug().noquote() << "Received JSON:\n" << QJsonDocument(jsonData).toJson(QJsonDocument::Indented);

                // ================ FORMAT GPGGA ================ //
                //qDebug().noquote() << convertToGPGGA(navShip.lat, navShip.lon, 1, 8, 0.9, 15.0);

                // ================ FORMAT AIVDO ================ //


                // Encode AIS !AIVDO
                aivdo = AIVDOEncoder::encodeAIVDO(0, navShip.lat, navShip.lon, 0, 0);
                qDebug().noquote() << aivdo;

            } else {
                qDebug() << "Invalid JSON received.";
            }
        });

        QObject::connect(clientS, &QTcpSocket::disconnected, [=]() {
            qDebug() << "Client disconnected.";
            clientS->deleteLater();
        });
    });

    return aivdo;
}

// Thread Subscribe from MOOSDB -- SSH
QString serverThreadMOOSSubscribeSSH() {
    QTcpSocket* clientS = new QTcpSocket();

    // Hubungkan ke server MOOSDB di Ubuntu (SSH)
    QString sshIP = "172.20.3.72";  // Ganti dengan IP Ubuntu
    quint16 sshPort = 5000;           // Sesuaikan dengan port forwarding SSH

    clientS->connectToHost(sshIP, sshPort);

    if (!clientS->waitForConnected(5000)) {
        qCritical() << "Failed to connect to MOOSDB server.";
        return aivdo;


    }

    qDebug() << "Connected to MOOSDB at" << sshIP << ":" << sshPort;

    QObject::connect(clientS, &QTcpSocket::readyRead, [=]() {
        QByteArray data = clientS->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        QJsonObject jsonData = jsonDoc.object();

        // Pastikan JSON memiliki key yang benar
        if (jsonData.contains("NAV_LAT") && jsonData.contains("NAV_LONG")) {
            QJsonValue latValue = jsonData["NAV_LAT"];
            QJsonValue lonValue = jsonData["NAV_LONG"];

            double lat = 0, lon = 0;

            if(latValue.isDouble() && lonValue.isDouble()){
                lat = jsonData["NAV_LAT"].toDouble();
                lon = jsonData["NAV_LONG"].toDouble();
            }
            else {
                lat = jsonData["NAV_LAT"].toString().toDouble();
                lon = jsonData["NAV_LONG"].toString().toDouble();
            }

            // Encode AIS !AIVDO
            aivdo = AIVDOEncoder::encodeAIVDO(0, lat, lon, 0, 0);
            qDebug().noquote() << "[AIVDO] " << aivdo;

        } else {
            qDebug() << "[ERROR] Invalid JSON received.";
        }
    });

    QObject::connect(clientS, &QTcpSocket::disconnected, [=]() {
        qDebug() << "[INFO] Disconnected from MOOSDB.";
        clientS->deleteLater();
    });

    return aivdo;
}


// Thread Subscribe from MOOSDB -- MAP INFO ONLY
MOOSShip serverThreadMOOSSubscribeMapInfo() {
    serverS = new QTcpServer();
    QEventLoop loop;  // Event loop untuk menunggu data valid

    if (!serverS->listen(QHostAddress::Any, 8081)) {
        qDebug() << "Server failed to start.";
        return mapShip;  // Return langsung jika server gagal start
    }

    qDebug() << "Waiting for connection...";

    QObject::connect(serverS, &QTcpServer::newConnection, [&]() {
        clientS = serverS->nextPendingConnection();
        qDebug() << "Client connected!";

        QObject::connect(clientS, &QTcpSocket::readyRead, [&]() {
            QByteArray data = clientS->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
            QJsonObject jsonData = jsonDoc.object();

            if (jsonData.contains("MAP_LAT") && jsonData.contains("MAP_LONG")) {
                mapShip.lat = jsonData["MAP_LAT"].toDouble();
                mapShip.lon = jsonData["MAP_LONG"].toDouble();

                qDebug().noquote() << "Received JSON:\n" << QJsonDocument(jsonData).toJson(QJsonDocument::Indented);

                // Tutup koneksi & server setelah data valid diterima
                clientS->disconnectFromHost();
                clientS->close();
                clientS->deleteLater();

                serverS->close();
                serverS->deleteLater();

                loop.quit();  // Data valid, keluar dari event loop
            } else {
                qDebug() << "Invalid JSON received. Waiting for valid data...";
            }
        });

        QObject::connect(clientS, &QTcpSocket::disconnected, [&]() {
            qDebug() << "Client disconnected.";
            clientS->deleteLater();
        });
    });

    loop.exec();  // Tunggu sampai data valid diterima, lalu keluar

    return mapShip;
}

QString convertToGPGGA(double lat, double lon, int fixQuality, int satellites, double hdop, double altitude) {
    int lat_deg = static_cast<int>(lat);
    double lat_min = (lat - lat_deg) * 60;
    int lon_deg = static_cast<int>(lon);
    double lon_min = (lon - lon_deg) * 60;

    char ns = (lat >= 0) ? 'N' : 'S';
    char ew = (lon >= 0) ? 'E' : 'W';

    std::ostringstream oss;
    oss << "$GPGGA,123519,"
        << std::setw(2) << std::setfill('0') << lat_deg
        << std::fixed << std::setprecision(4) << lat_min << "," << ns << ","
        << std::setw(3) << std::setfill('0') << lon_deg
        << std::fixed << std::setprecision(4) << lon_min << "," << ew << ","
        << fixQuality << "," << satellites << "," << std::fixed << std::setprecision(1) << hdop << ","
        << altitude << ",M,,M,,";

    return QString::fromStdString(oss.str());
}

QString encodeAISPayload(const std::string& binaryString) {
    const char lookupTable[] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^- !\"#$%&'()*+,-./0123456789:;<=>?";
    QString encoded;
    for (size_t i = 0; i < binaryString.length(); i += 6) {
        std::bitset<6> bits(binaryString.substr(i, 6));
        encoded.append(lookupTable[bits.to_ulong()]);
    }
    return encoded;
}

void startServerMOOSSubscribe() {
    QThread *thread = QThread::create([=]() { serverThreadMOOSSubscribeSSH(); });
    thread->start();
}

void stopServerMOOSSubscribe() {
    runningS = false;
    if (clientS) clientS->close();
    if (serverS) serverS->close();

    qDebug() << "Server disconnected";
    //listWidget->addItem("Server disconnected.");
}

// Thread Publish to MOOSDB
void serverThreadMOOSPublish() {
    runningP = true;
    int counter = 0;

    while (runningP) {
        QTcpSocket socket;
        socket.connectToHost(moosdb_host, moosdb_port);
        if (!socket.waitForConnected(3000)) {
            qDebug() << "Connection failed";
            break;
        }

        // mapShip.lat = ...
        QJsonObject jsonData {
            {"NAV_LAT", mapShip.lat},
            {"NAV_LON", mapShip.lon},
            {"NAV_X", mapShip.x},
            {"NAV_Y", mapShip.y},
            {"NAV_HEADING", mapShip.heading},
            {"NAV_HEADING_OVER_GROUND", mapShip.heading_og},
            {"NAV_SPEED", mapShip.speed},
            {"NAV_SPEED_OVER_GROUND", mapShip.speed_og},
            {"NAV_YAW", mapShip.yaw},
            {"NAV_DEPTH", mapShip.depth},
            {"NAV_Z", mapShip.lon},
            {"MAP_INFO", "JSON"}
        };

        QJsonDocument jsonDoc(jsonData);
        QByteArray jsonString = jsonDoc.toJson();

        qDebug().noquote() << "Send: \n" << QJsonDocument(jsonData).toJson(QJsonDocument::Indented);
        //listWidget->addItem(QString::fromUtf8(jsonString));

        socket.write(jsonString);
        socket.waitForBytesWritten();
        socket.waitForReadyRead();

        counter++;
        QThread::sleep(1);
    }
}

void serverThreadMOOSPublishPlus() {
    runningP = true;
    int counter = 0;

    while (runningP) {
        QTcpSocket socket;
        socket.connectToHost(moosdb_host, moosdb_port);
        if (!socket.waitForConnected(5001)) {
            qDebug() << "Connection failed";
            break;
        }

        // 1️⃣ Buat data NAV_* yang berubah setiap iterasi
        QString newInfo = "Data: " + QString::number(counter);

        // 2️⃣ Buat JSON untuk `MAP_INFO`
        QJsonObject jsonMapInfo {
            {"areasDetected", QJsonObject{
              {"depthArea", QJsonObject{
                    {"areaNumber", 1},
                    {"depthRangeValue1", 0},
                    {"depthRangeValue2", 2},
                    {"depthUnit", "meter"},
                    {"sourceDate", "20080501"},
                    {"sourceIndication", "ID, ID, graph, Peta 96 Edisi IX"}
                }},
              {"restrictedArea", QJsonObject{
                     {"areaNumber", 2},
                     {"category", "minefield"},
                     {"information", "FORMER MINED AREA"},
                     {"scaleMinimum", 119999},
                     {"scaleUnit", "1: scale"},
                     {"sourceDate", "20080501"},
                     {"sourceIndication", "ID, ID, graph, Peta 96 Edisi IX"},
                     {"textualDescription", "IDB50001.TXT"},
                     {"informationNationalLanguage", "BEKAS DAERAH RANJAU"}
                 }},
              {"cautionAreas", QJsonArray{
                   QJsonObject{
                       {"areaNumber", 3},
                       {"information", "AIDS TO NAVIGATION"},
                       {"scaleMinimum", 179999},
                       {"scaleUnit", "1: scale"},
                       {"sourceDate", "20050120"},
                       {"sourceIndication", "GB, GB, graph, BA 975"},
                       {"textualDescription", "IDA10001.TXT"},
                       {"informationNationalLanguage", "PERINGATAN"}
                   },
                   QJsonObject{
                       {"areaNumber", 4},
                       {"information", "CHANGEABLE COASTLINE"},
                       {"scaleMinimum", 179999},
                       {"scaleUnit", "1: scale"},
                       {"sourceDate", "20050120"},
                       {"sourceIndication", "GB, GB, graph, BA 975"},
                       {"textualDescription", "IDM10002.TXT"},
                       {"informationNationalLanguage", "PERUBAHAN GARIS PANTAI"}
                   }
               }}
          }}
        };

        // 3️⃣ Gabungkan NAV_* dan MAP_INFO ke dalam satu JSON utama
        QJsonObject jsonData {
            // {"NAV_LAT", mapShip.lat},
            // {"NAV_LON", mapShip.lon},
            // {"NAV_X", mapShip.x},
            // {"NAV_Y", mapShip.y},
            // {"NAV_HEADING", mapShip.heading},
            // {"NAV_HEADING_OVER_GROUND", mapShip.heading_og},
            // {"NAV_SPEED", mapShip.speed},
            // {"NAV_SPEED_OVER_GROUND", mapShip.speed_og},
            // {"NAV_YAW", mapShip.yaw},
            // {"NAV_DEPTH", mapShip.depth},
            // {"NAV_Z", mapShip.lon},
            {"MAP_INFO", jsonMapInfo}
        };

        // 4️⃣ Ubah JSON ke QByteArray
        QJsonDocument jsonDoc(jsonData);
        QByteArray jsonString = jsonDoc.toJson(QJsonDocument::Compact);

        // 5️⃣ Kirim data JSON
        socket.write(jsonString);
        socket.waitForBytesWritten();
        socket.waitForReadyRead();

        qDebug().noquote() << "Sent Data:\n" << QJsonDocument(jsonData).toJson(QJsonDocument::Indented);

        counter++;
        QThread::sleep(5);
    }
}

void serverThreadMOOSPublishMapInfo(QJsonObject jsonDoc) {
    while (true) {
        if (jsonDoc.isEmpty()) {
            QThread::sleep(5);
            continue;
        }

        QTcpSocket socket;
        socket.connectToHost(moosdb_host, moosdb_port);
        if (!socket.waitForConnected(3000)) {
            qDebug() << "Connection failed";
            return;
        }

        // Buat data JSON utama
        QJsonObject jsonData {
            {"NAV_LAT", mapShip.lat},
            {"NAV_LON", mapShip.lon},
            {"NAV_X", mapShip.x},
            {"NAV_Y", mapShip.y},
            {"NAV_HEADING", mapShip.heading},
            {"NAV_HEADING_OVER_GROUND", mapShip.heading_og},
            {"NAV_SPEED", mapShip.speed},
            {"NAV_SPEED_OVER_GROUND", mapShip.speed_og},
            {"NAV_YAW", mapShip.yaw},
            {"NAV_DEPTH", mapShip.depth},
            {"NAV_Z", mapShip.lon},
            {"MAP_INFO", jsonDoc}
        };

        // Ubah JSON ke QByteArray
        QJsonDocument jsonDoc(jsonData);
        QByteArray jsonString = jsonDoc.toJson(QJsonDocument::Compact);

        // Kirim data JSON
        socket.write(jsonString);
        if (socket.waitForBytesWritten()) {
            qDebug().noquote() << "Sent Data:\n" << QJsonDocument(jsonData).toJson(QJsonDocument::Indented);
        }

        socket.disconnectFromHost();
        if (socket.state() != QAbstractSocket::UnconnectedState) {
            socket.waitForDisconnected();
        }
        break; // Keluar dari loop setelah mengirim data
    }
}

void startServerMOOSPublish() {
    QThread *thread = QThread::create([=]() { serverThreadMOOSPublishPlus(); });
    thread->start();
}

void startServerMOOSPublishMapInfo(QJsonObject jsonDoc) {
    QThread *thread = QThread::create([=]() { serverThreadMOOSPublishMapInfo(jsonDoc); });
    thread->start();
}

void stopServerMOOSPublish() {
    runningP = false;
    if (clientP) clientP->close();
    if (serverP) serverP->close();

    qDebug() << "Server disconnected";
    //listWidget->addItem("Server disconnected.");
}

void AddToListBox(QListWidget *listWidget, const QString &text) {
    if (listWidget) {
        listWidget->addItem(text);
        listWidget->scrollToBottom();
    }
}
