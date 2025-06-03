#ifndef MOOSDB_H
#define MOOSDB_H

#include <QWidget>
#include <QString>
#include <QListWidget>
#include <QJsonArray>

#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTextEdit>

#include <QString>
#include <QDebug>
#include <bitset>
#include <sstream>

#include "json.hpp" // Untuk mengubah ke format JSON
#include "aivdoencoder.h"

// SevenCs Kernel EC2007
#ifdef _WIN32
#include <windows.h>
#else
#include <stdio.h>
#include <X11/Xlib.h>
#endif

#include <eckernel.h>

// Defines
#define PIXMAP_X 1280
#define PIXMAP_Y 720
#define DEFAULT_LATITUDE -7.18551
#define DEFAULT_LONGITUDE 112.78012

// Struktur untuk menyimpan data kapal
struct MOOSShip {
    double lat;           // Latitude
    double lon;          // Longitude
    double x;           // Pixel X
    double y;          // Pixel Y
    double heading;      // Heading dalam derajat
    double heading_og;   // Heading over ground
    double speed;        // Kecepatan dalam knot
    double speed_og;   // Kecepatan over ground
    double yaw;        // Sudut yaw kapal
    double depth;        // Kedalaman
    double z;        // Vertikal kapal
};

/******* Global Variable ********/
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

extern QString moosdb_host;
extern int moosdb_port;

extern MOOSShip mapShip;
extern MOOSShip navShip;

extern QString aivdo;

extern QTextEdit *nmeaText;
extern QTextEdit *aisText;
extern QTextEdit *ownShipText;

extern QTextEdit *aisTemp;
extern QTextEdit *ownShipTemp;


// Global Variables
extern QTcpServer *serverS;
extern QTcpSocket *clientS;
extern QTcpServer *serverM;
extern QTcpSocket *clientM;
extern QTcpServer *serverP;
extern QTcpSocket *clientP;
extern bool runningS;
extern bool runningP;
extern bool runningM;


// MOOS Server Subscribe
void startServerMOOSSubscribe();
void stopServerMOOSSubscribe();
QString serverThreadMOOSSubscribe();
QString serverThreadMOOSSubscribeSSH();
MOOSShip serverThreadMOOSSubscribeMapInfo();

// MOOS Server Publish
void startServerMOOSPublish();
void startServerMOOSPublishMapInfo(QJsonObject jsonDoc);
void stopServerMOOSPublish();
void serverThreadMOOSPublish();
void serverThreadMOOSPublishMapInfo(QJsonObject jsonDoc);

// Convert
QString convertToGPGGA(double lat, double lon, int fixQuality, int satellites, double hdop, double altitude);
QString convertToAIVDO(int msgType, int repeat, int mmsi, int status, double rot, double sog, bool positionAccuracy, double lon, double lat, double cog, double trueHeading, int timestamp);
QString encodeAISPayload(const std::string& binaryString);

void AddToListBox(QListWidget*, const QString&);

#endif // MOOSDB_H
