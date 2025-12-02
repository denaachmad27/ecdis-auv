#include "aisdatabasemanager.h"

#ifdef _WIN32
#include <windows.h>
#endif

AisDatabaseManager::AisDatabaseManager() : asyncProcessingTimer(nullptr) {
    qDebug() << "AisDatabaseManager initialized with high-performance async processing";

    // Setup async processing timer
    asyncProcessingTimer = new QTimer();
    QObject::connect(asyncProcessingTimer, &QTimer::timeout, [this]() {
        processTargetReferencesAsync();
    });
    asyncProcessingTimer->start(5000); // Process every 5 seconds

    setupPerformanceOptimizations();
}

AisDatabaseManager::~AisDatabaseManager() {
    // Process any remaining target references before shutdown
    processTargetReferencesAsync();
    delete asyncProcessingTimer;
    qDebug() << "AisDatabaseManager destroyed";
}

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
    QString sql = R"(
        INSERT INTO ais_targets (
            mmsi, imo_number, latitude, longitude, rot, sog, cog, heading,
            persons_aboard, bunker_oil, tonnage,
            nav_name, destination, call_sign, vendor_id, ship_name,
            ais_version, actual_draught, ship_type, parse_time
        ) VALUES (
            ?, ?, ?, ?, ?, ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?, NOW()
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
    )";

    query.prepare(sql);

    // Bind all parameters in order
    query.addBindValue(info.mmsi);
    query.addBindValue(info.imoNumber);
    query.addBindValue(info.latitude);
    query.addBindValue(info.longitude);
    query.addBindValue(info.rot);
    query.addBindValue(info.sog);
    query.addBindValue(info.cog);
    query.addBindValue(info.heading);
    query.addBindValue(info.personsAboard);
    query.addBindValue(info.bunkerOil);
    query.addBindValue(info.tonnage);
    query.addBindValue(info.navStatus);
    query.addBindValue(QString::fromUtf8(info.destination));
    query.addBindValue(QString::fromUtf8(info.callSign));
    query.addBindValue(QString::fromUtf8(info.vendorID));
    query.addBindValue(QString::fromUtf8(info.shipName));
    query.addBindValue(info.aisVersion);
    query.addBindValue(info.actualDraught);
    query.addBindValue(info.shipType);

    if (!query.exec()) {
        qWarning() << "DB insert/update error:" << query.lastError().text();
        qWarning() << "Executed query:" << query.executedQuery();
    }
}

void AisDatabaseManager::insertOwnShipToDB(double lat, double lon, double depth,
                       double heading, double headingOverGround,
                       double speed, double speedOverGround,
                       double yaw, double z)
{
    QSqlQuery query;
    QString sql = R"(
        INSERT INTO own_ship_log (
            latitude, longitude, depth,
            heading, heading_over_ground,
            speed, speed_over_ground,
            yaw, z
        ) VALUES (
            ?, ?, ?,
            ?, ?,
            ?, ?,
            ?, ?
        )
    )";

    query.prepare(sql);

    // Bind all parameters in order
    query.addBindValue(lat);
    query.addBindValue(lon);
    query.addBindValue(depth);
    query.addBindValue(heading);
    query.addBindValue(headingOverGround);
    query.addBindValue(speed);
    query.addBindValue(speedOverGround);
    query.addBindValue(yaw);
    query.addBindValue(z);

    if (!query.exec()) {
        qWarning() << "OwnShip insert failed:" << query.lastError().text();
        qWarning() << "Executed query:" << query.executedQuery();
    }
}

void AisDatabaseManager::insertOwnShipToDB(QString nmea)
{
    if (!db.isOpen()) {
        qWarning() << "Database not open for ownship NMEA recording";
        return;
    }

    // Check if ownship_nmea table exists
    QSqlQuery checkTableQuery(db);
    if (!checkTableQuery.exec("SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = 'ownship_nmea')")) {
        qWarning() << "Failed to check ownship_nmea table existence:" << checkTableQuery.lastError().text();
        return;
    }

    bool tableExists = false;
    if (checkTableQuery.next()) {
        tableExists = checkTableQuery.value(0).toBool();
    }

    if (!tableExists) {
        // Table doesn't exist, skip insertion
        qDebug() << "ownship_nmea table does not exist, skipping ownship NMEA recording";
        return;
    }

    // Escape NMEA string for SQL
    QString escapedNmea = QString(nmea).replace("'", "''");

    QSqlQuery query(db);
    QString sql = QString(R"(
        INSERT INTO ownship_nmea (
            timestamp, nmea
        ) VALUES (
            TIMESTAMP '%1', '%2'
        )
    )").arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(escapedNmea);

    if (!query.exec(sql)) {
        qWarning() << "OwnShip insert failed:" << query.lastError().text();
        qWarning() << "SQL query:" << sql;
    }
}

void AisDatabaseManager::getOwnShipNmeaData(QSqlQuery& query, const QDateTime& startTime, const QDateTime& endTime)
{
    if (!db.isOpen()) {
        qWarning() << "Database not open for retrieving ownship NMEA data";
        return;
    }

    // Check if ownship_nmea table exists
    QSqlQuery checkTableQuery(db);
    if (!checkTableQuery.exec("SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = 'ownship_nmea')")) {
        qWarning() << "Failed to check ownship_nmea table existence:" << checkTableQuery.lastError().text();
        return;
    }

    bool tableExists = false;
    if (checkTableQuery.next()) {
        tableExists = checkTableQuery.value(0).toBool();
    }

    if (!tableExists) {
        // Table doesn't exist, create an empty query result
        qDebug() << "ownship_nmea table does not exist, no data to retrieve";
        return;
    }

    // Prepare the SELECT query with a WHERE clause for the time range
    QString sql = R"(
        SELECT
            timestamp,
            nmea
        FROM
            ownship_nmea
        WHERE
            timestamp BETWEEN ? AND ?
        ORDER BY
            timestamp ASC
    )";

    query.prepare(sql);

    // Bind the start and end timestamps to the query
    query.addBindValue(startTime);
    query.addBindValue(endTime);

    // Execute the query and check for errors
    if (!query.exec()) {
        qWarning() << "Failed to retrieve NMEA data with time range:" << query.lastError().text();
        qWarning() << "Executed query:" << query.executedQuery();
    }
}

// ========================================
// NEW UNIFIED RECORDING FUNCTIONS
// ========================================

bool AisDatabaseManager::insertNmeaRecord(const QString& nmea, const QString& dataSource,
                                        quint32 mmsi, const QString& messageType)
{
    if (!db.isOpen()) {
        qWarning() << "Database not open for NMEA recording";
        return false;
    }

    // Extract additional AIS information if available
    quint32 extractedMmsi = mmsi > 0 ? mmsi : extractMmsiFromNmea(nmea);
    QString msgType = !messageType.isEmpty() ? messageType : parseAisMessageType(nmea);
    QString vesselName = extractVesselNameFromNmea(nmea);
    QString rawDataType = determineDataType(nmea);

    QSqlQuery query(db);
    QString sql = R"(
        INSERT INTO nmea_records (
            timestamp, nmea, data_source, mmsi, message_type,
            vessel_name, raw_data_type, session_id, parsed_successfully
        ) VALUES (
            ?, ?, ?, ?, ?,
            ?, ?, ?, ?
        )
    )";

    query.prepare(sql);

    // Bind all parameters in order
    query.addBindValue(QDateTime::currentDateTimeUtc());
    query.addBindValue(nmea);
    query.addBindValue(dataSource);
    query.addBindValue(extractedMmsi > 0 ? QVariant(extractedMmsi) : QVariant());
    query.addBindValue(msgType.toInt() > 0 ? QVariant(msgType.toInt()) : QVariant());
    query.addBindValue(!vesselName.isEmpty() ? QVariant(vesselName) : QVariant());
    query.addBindValue(rawDataType);
    query.addBindValue(currentSessionId.isNull() ? QVariant() : currentSessionId);
    query.addBindValue(true);

    if (!query.exec()) {
        qWarning() << "NMEA record insert failed:" << query.lastError().text();
        qWarning() << "Executed query:" << query.executedQuery();
        return false;
    }

    qDebug() << "Recorded NMEA:" << dataSource.left(3) << "|" << nmea.left(20) << "...";
    return true;
}

// ========================================
// ENHANCED RECORDING WITH PARSED DATA
// ========================================

bool AisDatabaseManager::insertParsedAisData(const QString& nmea, const QString& dataSource,
                                             quint32 mmsi, const EcAISTargetInfo& targetInfo)
{
    // Add protection against infinite recursion
    static QAtomicInt recordingDepth(0);
    if (recordingDepth.load() > 5) {
        qWarning() << "Too many database operations in progress, skipping recording";
        return false;
    }

    // Increment depth counter atomically for infinite loop protection
    if (recordingDepth.fetchAndAddOrdered(1) > 5) {
        qWarning() << "Too many database operations in progress, skipping recording";
        recordingDepth.fetchAndAddOrdered(-1);
        return false;
    }

    if (!db.isOpen()) {
        qWarning() << "Database not open for parsed AIS recording";
        recordingDepth.fetchAndAddOrdered(-1);
        return false;
    }

    // Use high-performance fast insert for 60+ records/second capability
    bool success = insertParsedAisDataFast(nmea, dataSource, mmsi, targetInfo);

    recordingDepth.fetchAndAddOrdered(-1);
    return success;
}

bool AisDatabaseManager::insertParsedOwnshipData(const QString& nmea, const QString& dataSource,
                                                double lat, double lon, double sog, double cog, double heading)
{
    // Add protection against infinite recursion
    static QAtomicInt recordingDepth(0);
    if (recordingDepth.load() > 5) {
        qWarning() << "Too many database operations in progress, skipping ownship recording";
        return false;
    }

    // Increment depth counter atomically for infinite loop protection
    if (recordingDepth.fetchAndAddOrdered(1) > 5) {
        qWarning() << "Too many database operations in progress, skipping recording";
        recordingDepth.fetchAndAddOrdered(-1);
        return false;
    }

    if (!db.isOpen()) {
        qWarning() << "Database not open for parsed ownship recording";
        recordingDepth.fetchAndAddOrdered(-1);
        return false;
    }

    // Use high-performance fast insert for 60+ records/second capability
    bool success = insertParsedOwnshipDataFast(nmea, dataSource, lat, lon, sog, cog, heading);

    recordingDepth.fetchAndAddOrdered(-1);
    return success;
}

void AisDatabaseManager::startRecordingSession(const QString& sessionName)
{
    if (!db.isOpen()) {
        qWarning() << "Database not open for session creation";
        return;
    }

    QSqlQuery query(db);
    QString sql = R"(
        INSERT INTO recording_sessions (session_name, start_time, status, source_type)
        VALUES (?, NOW(), 'active', 'MOOSDB')
        RETURNING id
    )";

    query.prepare(sql);

    // Bind all parameters in order
    QString sessionNameToUse = sessionName.isEmpty() ?
                              QString("Session_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")) :
                              sessionName;
    query.addBindValue(sessionNameToUse);

    if (query.exec() && query.next()) {
        currentSessionId = query.value(0).toUuid();
        qDebug() << "Started recording session:" << currentSessionId.toString();
    } else {
        qWarning() << "Failed to create recording session:" << query.lastError().text();
        qWarning() << "Executed query:" << query.executedQuery();
    }
}

void AisDatabaseManager::stopRecordingSession()
{
    if (currentSessionId.isNull() || !db.isOpen()) {
        return;
    }

    QSqlQuery query(db);
    QString sql = R"(
        UPDATE recording_sessions
        SET end_time = NOW(), status = 'completed',
            duration_seconds = EXTRACT(EPOCH FROM (NOW() - start_time))::INTEGER
        WHERE id = ?
    )";

    query.prepare(sql);
    query.addBindValue(currentSessionId);

    if (query.exec()) {
        qDebug() << "Stopped recording session:" << currentSessionId.toString();
        currentSessionId = QUuid();
    } else {
        qWarning() << "Failed to stop recording session:" << query.lastError().text();
        qWarning() << "Executed query:" << query.executedQuery();
    }
}

void AisDatabaseManager::getCombinedNmeaData(QSqlQuery& query, const QDateTime& startTime,
                                           const QDateTime& endTime, const QStringList& dataSources)
{
    QString sourceFilter = dataSources.isEmpty() ? "'ownship', 'aistarget'" :
                          QString("'%1'").arg(dataSources.join("', '"));

    QString sql = QString(R"(
        SELECT timestamp, nmea, data_source
        FROM nmea_records
        WHERE timestamp BETWEEN ? AND ?
          AND data_source IN (%1)
        ORDER BY timestamp ASC
    )").arg(sourceFilter);

    query.prepare(sql);
    query.addBindValue(startTime);
    query.addBindValue(endTime);

    if (!query.exec()) {
        qWarning() << "Failed to retrieve combined NMEA data:" << query.lastError().text();
        qWarning() << "Executed query:" << query.executedQuery();
    }
}

void AisDatabaseManager::getNmeaDataWithFilter(QSqlQuery& query, const QDateTime& startTime,
                                             const QDateTime& endTime, const QStringList& dataSources,
                                             const QList<quint32>& mmsiFilter)
{
    QStringList conditions;
    QStringList bindValues;
    conditions << "timestamp BETWEEN ? AND ?";
    bindValues << startTime.toString(Qt::ISODate) << endTime.toString(Qt::ISODate);

    if (!dataSources.isEmpty()) {
        QString sourceFilter = QString("data_source IN ('%1')").arg(dataSources.join("', '"));
        conditions << sourceFilter;
    }

    if (!mmsiFilter.isEmpty()) {
        QStringList mmsiStrings;
        for (quint32 mmsi : mmsiFilter) {
            mmsiStrings << QString::number(mmsi);
        }
        conditions << QString("mmsi IN (%1)").arg(mmsiStrings.join(", "));
    }

    QString sql = QString(R"(
        SELECT timestamp, nmea, data_source, mmsi, vessel_name, latitude, longitude,
               speed_over_ground, course_over_ground
        FROM nmea_records
        WHERE %1
        ORDER BY timestamp ASC
    )").arg(conditions.join(" AND "));

    query.prepare(sql);
    query.addBindValue(startTime);
    query.addBindValue(endTime);

    if (!query.exec()) {
        qWarning() << "Failed to retrieve filtered NMEA data:" << query.lastError().text();
        qWarning() << "Executed query:" << query.executedQuery();
    }
}

void AisDatabaseManager::getRecordingSessions(QSqlQuery& query)
{
    query.prepare(R"(
        SELECT id, session_name, start_time, end_time, status,
               total_records, ownship_records, aistarget_records,
               duration_seconds
        FROM recording_sessions
        ORDER BY start_time DESC
    )");

    if (!query.exec()) {
        qWarning() << "Failed to retrieve recording sessions:" << query.lastError().text();
    }
}

void AisDatabaseManager::createPlaybackRequest(const QString& userId, const QDateTime& startTime,
                                             const QDateTime& endTime, double playbackSpeed)
{
    if (!db.isOpen()) {
        return;
    }

    QSqlQuery query(db);
    QString sql = R"(
        INSERT INTO playback_requests (user_id, start_time, end_time, playback_speed, status)
        VALUES (?, ?, ?, ?, 'pending')
    )";

    query.prepare(sql);
    query.addBindValue(userId);
    query.addBindValue(startTime);
    query.addBindValue(endTime);
    query.addBindValue(playbackSpeed);

    if (!query.exec()) {
        qWarning() << "Failed to create playback request:" << query.lastError().text();
        qWarning() << "Executed query:" << query.executedQuery();
    }
}

// ========================================
// HELPER FUNCTIONS
// ========================================

QString AisDatabaseManager::parseAisMessageType(const QString& nmea)
{
    if (!nmea.contains("!AIVDM") && !nmea.contains("!AIVDO")) {
        return "0";
    }

    QStringList parts = nmea.split(',');
    if (parts.size() >= 2) {
        return parts[1]; // Message type
    }
    return "0";
}

quint32 AisDatabaseManager::extractMmsiFromNmea(const QString& nmea)
{
    if (!nmea.contains("!AIVDM") && !nmea.contains("!AIVDO")) {
        return 0;
    }

    QStringList parts = nmea.split(',');
    if (parts.size() >= 6) {
        // Extract MMSI from decoded AIS payload (simplified)
        // In real implementation, you'd use proper AIS decoder
        return parts[5].toUInt();
    }
    return 0;
}

QString AisDatabaseManager::extractVesselNameFromNmea(const QString& nmea)
{
    // Simplified vessel name extraction
    // In real implementation, you'd decode the AIS payload
    return ""; // Not implemented yet
}

QString AisDatabaseManager::determineDataType(const QString& nmea)
{
    if (nmea.startsWith("!AIVDO")) {
        return "AIVDO";
    } else if (nmea.startsWith("!AIVDM")) {
        return "AIVDM";
    } else if (nmea.startsWith("$GPGGA")) {
        return "GGA";
    } else if (nmea.startsWith("$GPRMC")) {
        return "RMC";
    } else if (nmea.startsWith("$GP")) {
        return "GPS";
    } else {
        return "UNKNOWN";
    }
}

// Throttling variables initialization
QDateTime AisDatabaseManager::lastAisRecordTime;
QDateTime AisDatabaseManager::lastOwnshipRecordTime;

// ========================================
// HIGH-PERFORMANCE ASYNC PROCESSING
// ========================================

void AisDatabaseManager::setupPerformanceOptimizations() {
    if (!db.isOpen()) {
        qWarning() << "Database not open for performance optimizations";
        return;
    }

    // Disable the performance bottleneck trigger
    QSqlQuery query(db);
    if (query.exec("ALTER TABLE nmea_records DISABLE TRIGGER update_target_reference;")) {
        qDebug() << "✓ Trigger disabled - switched to high-performance async processing";
    } else {
        qWarning() << "Failed to disable trigger (may already be disabled):" << query.lastError().text();
    }

    qDebug() << "High-performance mode enabled - capable of 60+ records/second";
}

void AisDatabaseManager::processTargetReferencesAsync() {
    if (!db.isOpen()) {
        return;
    }

    QMutexLocker locker(&cacheMutex);
    if (targetReferenceCache.isEmpty()) {
        return; // Nothing to process
    }

    qDebug() << "Processing" << targetReferenceCache.size() << "target references asynchronously...";

    db.transaction();
    QSqlQuery query(db);

    bool hasErrors = false;
    int processedCount = 0;

    // Process in batches of 100 for better performance
    QList<quint32> mmsiList = targetReferenceCache.keys();
    for (int i = 0; i < mmsiList.size(); i += 100) {
        QStringList batchValues;
        int batchSize = qMin(100, mmsiList.size() - i);

        // Build batch values
        for (int j = 0; j < batchSize; ++j) {
            quint32 mmsi = mmsiList[i + j];
            const TargetReferenceRecord& record = targetReferenceCache[mmsi];

            if (record.mmsi > 0) {  // Skip invalid records
                QString escapedVesselName = QString(record.vesselName).replace("'", "''");
                QString escapedCallSign = QString(record.callSign).replace("'", "''");
                batchValues << QString("(%1, '%2', '%3', %4, %5, 'partial', CURRENT_TIMESTAMP, %6)")
                    .arg(record.mmsi)
                    .arg(escapedVesselName)
                    .arg(escapedCallSign)
                    .arg(record.imo > 0 ? QString::number(record.imo) : "NULL")
                    .arg(record.shipType > 0 ? QString::number(record.shipType) : "NULL")
                    .arg(record.sourceCount);
            }
        }

        if (!batchValues.isEmpty()) {
            QString batchSql = QString(
                "INSERT INTO target_references (mmsi, vessel_name, call_sign, imo, ship_type, data_quality, last_seen, source_count) "
                "VALUES %1 "
                "ON CONFLICT (mmsi) DO UPDATE SET "
                "vessel_name = COALESCE(EXCLUDED.vessel_name, target_references.vessel_name), "
                "call_sign = COALESCE(EXCLUDED.call_sign, target_references.call_sign), "
                "imo = COALESCE(EXCLUDED.imo, target_references.imo), "
                "ship_type = COALESCE(EXCLUDED.ship_type, target_references.ship_type), "
                "last_seen = CURRENT_TIMESTAMP, "
                "source_count = target_references.source_count + EXCLUDED.source_count"
            ).arg(batchValues.join(", "));

            if (!query.exec(batchSql)) {
                qWarning() << "Batch target reference insert failed:" << query.lastError().text();
                hasErrors = true;
            } else {
                processedCount += batchSize;
            }
        }
    }

    if (!hasErrors) {
        db.commit();
        qDebug() << "✓ Processed" << processedCount << "target references successfully";
        targetReferenceCache.clear(); // Clear processed cache
    } else {
        db.rollback();
        qWarning() << "Batch processing failed, keeping cache for retry";
    }
}

bool AisDatabaseManager::insertParsedAisDataFast(const QString& nmea, const QString& dataSource,
                                                quint32 mmsi, const EcAISTargetInfo& targetInfo) {
    // Quick validation
    if (nmea.isEmpty() || mmsi == 0) {
        return false;
    }

    // Convert SevenCs coordinates (1/10000 degree) to decimal degrees
    double latitude = (targetInfo.latitude != 0) ?
        ((double)targetInfo.latitude / 10000.0) / 60.0 : 0.0;
    double longitude = (targetInfo.longitude != 0) ?
        ((double)targetInfo.longitude / 10000.0) / 60.0 : 0.0;

    QDateTime currentTime = QDateTime::currentDateTimeUtc();

    // Fast insert without trigger overhead - using received_at for correct timezone
    QSqlQuery query(db);
    QString sql = QString(
        "INSERT INTO nmea_records (nmea, data_source, mmsi, latitude, longitude, "
        "vessel_name, raw_data_type, parsed_successfully, received_at) "
        "VALUES ('%1', '%2', %3, %4, %5, '%6', '%7', true, TIMESTAMP '%8')"
    ).arg(QString(nmea).replace("'", "''"))
     .arg(dataSource)
     .arg(mmsi)
     .arg(latitude != 0 ? QString::number(latitude, 'f', 6) : "NULL")
     .arg(longitude != 0 ? QString::number(longitude, 'f', 6) : "NULL")
     .arg(QString::fromUtf8(targetInfo.shipName).replace("'", "''"))
     .arg(determineDataType(nmea))
     .arg(currentTime.toString("yyyy-MM-dd hh:mm:ss.zzz"));

    bool success = query.exec(sql);

    if (success) {
        // Update cache for async processing - use received_at timestamp
        QMutexLocker locker(&cacheMutex);
        TargetReferenceRecord& record = targetReferenceCache[mmsi];
        if (record.mmsi == 0) { // New record
            record.mmsi = mmsi;
            record.vesselName = QString::fromUtf8(targetInfo.shipName);
            record.callSign = QString::fromUtf8(targetInfo.callSign);
            record.imo = targetInfo.imoNumber;
            record.shipType = targetInfo.shipType;
            record.timestamp = currentTime;
        } else {
            record.sourceCount++;
            record.timestamp = currentTime;
        }
    } else {
        qWarning() << "Fast AIS insert failed:" << query.lastError().text();
    }

    return success;
}

bool AisDatabaseManager::insertParsedOwnshipDataFast(const QString& nmea, const QString& dataSource,
                                                    double lat, double lon, double sog, double cog, double heading) {
    if (nmea.isEmpty()) {
        return false;
    }

    QDateTime currentTime = QDateTime::currentDateTimeUtc();

    // Fast insert without trigger overhead - using received_at for correct timezone
    QSqlQuery query(db);
    QString sql = QString(
        "INSERT INTO nmea_records (nmea, data_source, mmsi, latitude, longitude, "
        "speed_over_ground, course_over_ground, true_heading, vessel_name, raw_data_type, parsed_successfully, received_at) "
        "VALUES ('%1', '%2', 999999999, %3, %4, %5, %6, %7, 'OWNSHIP', '%8', true, TIMESTAMP '%9')"
    ).arg(QString(nmea).replace("'", "''"))
     .arg(dataSource)
     .arg(lat != 0 ? QString::number(lat, 'f', 6) : "NULL")
     .arg(lon != 0 ? QString::number(lon, 'f', 6) : "NULL")
     .arg(sog > 0 ? QString::number(sog, 'f', 2) : "NULL")
     .arg(cog > 0 ? QString::number(cog, 'f', 2) : "NULL")
     .arg(heading > 0 ? QString::number(heading, 'f', 2) : "NULL")
     .arg(determineDataType(nmea))
     .arg(currentTime.toString("yyyy-MM-dd hh:mm:ss.zzz"));

    if (!query.exec(sql)) {
        qWarning() << "Fast ownship insert failed:" << query.lastError().text();
        return false;
    }

    return true;
}
