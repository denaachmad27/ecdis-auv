#include "aisdatabasemanager.h"

#ifdef _WIN32
#include <windows.h>
#endif

AisDatabaseManager& AisDatabaseManager::instance() {
    static AisDatabaseManager instance;
    return instance;
}

bool AisDatabaseManager::connect(const QString& host, int port, const QString& dbName,
                                 const QString& user, const QString& password) {

#ifdef _WIN32
    // Set PATH untuk Qt dan PostgreSQL binaries (Windows only)
    QString currentPath = qgetenv("PATH");
    QString qtPath = "C:/Qt/5.15.0/mingw81_64/bin";
    QString pgPath = "C:/Program Files/PostgreSQL/16/bin";

    if (!currentPath.contains(qtPath, Qt::CaseInsensitive)) {
        qputenv("PATH", (qtPath + ";" + pgPath + ";" + currentPath).toLocal8Bit());
        qDebug() << "Updated PATH for PostgreSQL support";
    }
#endif

    qDebug() << "Available SQL drivers:" << QSqlDatabase::drivers();

    if (!QSqlDatabase::drivers().contains("QPSQL")) {
        qDebug() << "ERROR: QPSQL driver not found!";
        return false;
    }

    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName(host);
    db.setPort(port);
    db.setDatabaseName(dbName);
    db.setUserName(user);
    db.setPassword(password);

    // Tambahkan connection options untuk debugging
    db.setConnectOptions("connect_timeout=5");

    bool connected = db.open();
    if (!connected) {
        qDebug() << "=== DATABASE CONNECTION FAILED ===";
        qDebug() << "Error:" << db.lastError().text();
        qDebug() << "Error Type:" << db.lastError().type();
        qDebug() << "Error Number:" << db.lastError().number();
        qDebug() << "Database:" << db.databaseName();
        qDebug() << "Host:" << db.hostName();
        qDebug() << "Port:" << db.port();
        qDebug() << "User:" << db.userName();
        qDebug() << "Driver:" << db.driverName();
        qDebug() << "Connection Options:" << db.connectOptions();
        qDebug() << "=================================";
    } else {
        qDebug() << "=== DATABASE CONNECTED SUCCESSFULLY ===";
        qDebug() << "Database:" << db.databaseName();
        qDebug() << "Host:" << db.hostName();
        qDebug() << "=================================";
    }
    return connected;
}

void AisDatabaseManager::insertOrUpdateAisTarget(const EcAISTargetInfo& info) {
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO ais_targets (
            mmsi, imo_number, latitude, longitude, rot, sog, cog, heading,
            persons_aboard, bunker_oil, tonnage,
            nav_name, destination, call_sign, vendor_id, ship_name,
            ais_version, actual_draught, ship_type, parse_time
        ) VALUES (
            :mmsi, :imo_number, :latitude, :longitude, :rot, :sog, :cog, :heading,
            :persons_aboard, :bunker_oil, :tonnage,
            :nav_name, :destination, :call_sign, :vendor_id, :ship_name,
            :ais_version, :actual_draught, :ship_type, NOW()
        )
        ON CONFLICT (mmsi) DO UPDATE SET
            imo_number = EXCLUDED.imo_number,
            latitude = EXCLUDED.latitude,
            longitude = EXCLUDED.longitude,
            rot = EXCLUDED.rot,
            sog = EXCLUDED.sog,
            cog = EXCLUDED.cog,
            heading = EXCLUDED.heading,
            persons_aboard = EXCLUDED.persons_aboard,
            bunker_oil = EXCLUDED.bunker_oil,
            tonnage = EXCLUDED.tonnage,
            nav_name = EXCLUDED.nav_name,
            destination = EXCLUDED.destination,
            call_sign = EXCLUDED.call_sign,
            vendor_id = EXCLUDED.vendor_id,
            ship_name = EXCLUDED.ship_name,
            ais_version = EXCLUDED.ais_version,
            actual_draught = EXCLUDED.actual_draught,
            ship_type = EXCLUDED.ship_type,
            parse_time = NOW()
    )");

    query.bindValue(":mmsi", info.mmsi);
    query.bindValue(":imo_number", info.imoNumber);
    query.bindValue(":latitude", info.latitude);
    query.bindValue(":longitude", info.longitude);
    query.bindValue(":rot", info.rot);
    query.bindValue(":sog", info.sog);
    query.bindValue(":cog", info.cog);
    query.bindValue(":heading", info.heading);
    query.bindValue(":persons_aboard", info.personsAboard);
    query.bindValue(":bunker_oil", info.bunkerOil);
    query.bindValue(":tonnage", info.tonnage);
    query.bindValue(":nav_name", info.navStatus);
    query.bindValue(":destination", QString::fromUtf8(info.destination));
    query.bindValue(":call_sign", QString::fromUtf8(info.callSign));
    query.bindValue(":vendor_id", QString::fromUtf8(info.vendorID));
    query.bindValue(":ship_name", QString::fromUtf8(info.shipName));
    query.bindValue(":ais_version", info.aisVersion);
    query.bindValue(":actual_draught", info.actualDraught);
    query.bindValue(":ship_type", info.shipType);

    if (!query.exec()) {
        qWarning() << "DB insert/update error:" << query.lastError().text();
    }
}

void AisDatabaseManager::insertOwnShipToDB(double lat, double lon, double depth,
                       double heading, double headingOverGround,
                       double speed, double speedOverGround,
                       double yaw, double z)
{
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO own_ship_log (
            latitude, longitude, depth,
            heading, heading_over_ground,
            speed, speed_over_ground,
            yaw, z
        ) VALUES (
            :lat, :lon, :depth,
            :heading, :hog,
            :speed, :sog,
            :yaw, :z
        )
    )");

    query.bindValue(":lat", lat);
    query.bindValue(":lon", lon);
    query.bindValue(":depth", depth);
    query.bindValue(":heading", heading);
    query.bindValue(":hog", headingOverGround);
    query.bindValue(":speed", speed);
    query.bindValue(":sog", speedOverGround);
    query.bindValue(":yaw", yaw);
    query.bindValue(":z", z);

    if (!query.exec()) {
        qWarning() << "OwnShip insert failed:" << query.lastError().text();
    }
}

void AisDatabaseManager::insertOwnShipToDB(QString nmea)
{
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO ownship_nmea (
            timestamp, nmea
        ) VALUES (
            :ts, :nmea
        )
    )");

    query.bindValue(":ts", QDateTime::currentDateTimeUtc());
    query.bindValue(":nmea", nmea);

    if (!query.exec()) {
        qWarning() << "OwnShip insert failed:" << query.lastError().text();
    }
}

void AisDatabaseManager::getOwnShipNmeaData(QSqlQuery& query, const QDateTime& startTime, const QDateTime& endTime)
{
    // Prepare the SELECT query with a WHERE clause for the time range
    query.prepare(R"(
        SELECT
            timestamp,
            nmea
        FROM
            ownship_nmea
        WHERE
            timestamp BETWEEN :startTime AND :endTime
        ORDER BY
            timestamp ASC
    )");

    // Bind the start and end timestamps to the query
    query.bindValue(":startTime", startTime);
    query.bindValue(":endTime", endTime);

    // Execute the query and check for errors
    if (!query.exec()) {
        qWarning() << "Failed to retrieve NMEA data with time range:" << query.lastError().text();
    }
}
