#ifndef AISDATABASEMANAGER_H
#define AISDATABASEMANAGER_H

#include <QtSql>
#include <QDebug>
// SevenCs Kernel EC2007
#ifdef _WIN32
#include <windows.h>
#pragma pack(push, 4)
#include <eckernel.h>
#include <ecs63.h>
#pragma pack (pop)
#else
#include <stdio.h>
#include <X11/Xlib.h>
#include <eckernel.h>
#endif

class AisDatabaseManager {
public:
    static AisDatabaseManager& instance();
    bool connect(const QString& host, int port, const QString& dbName,
                 const QString& user, const QString& password);
    void insertOrUpdateAisTarget(const EcAISTargetInfo& info);
    void insertOwnShipToDB(double lat, double lon, double depth,
                       double heading, double headingOverGround,
                       double speed, double speedOverGround,
                           double yaw, double z);

    void insertOwnShipToDB(QString nmea);
    void getOwnShipNmeaData(QSqlQuery& query, const QDateTime& startTime, const QDateTime& endTime);

private:
    AisDatabaseManager() {}
    QSqlDatabase db;
};

#endif // AISDATABASEMANAGER_H
