#include "aisdatabasemanager.h"
#include "SettingsDialog.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <QDir>
#include <QFile>
#include <QCoreApplication>

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
    // Stop async timer first to prevent race conditions
    if (asyncProcessingTimer) {
        asyncProcessingTimer->stop();
    }

    // Process any remaining target references ONLY if database is still open and not shutting down
    try {
        if (db.isOpen() && !targetReferenceCache.isEmpty()) {
            qDebug() << "Processing final batch of target references during shutdown...";
            processTargetReferencesAsync();
        }
    } catch (...) {
        // Ignore errors during shutdown
        qWarning() << "Error processing target references during shutdown";
    }

    // Clean up timer
    delete asyncProcessingTimer;
    asyncProcessingTimer = nullptr;

    // Close database connection if still open
    try {
        if (db.isOpen()) {
            db.close();
        }
    } catch (...) {
        qWarning() << "Error closing database during shutdown";
    }

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

    // Check if PostgreSQL path exists
    QDir pgDir(pgPath);
    if (!pgDir.exists()) {
        qDebug() << "PostgreSQL path not found:" << pgPath;
        qDebug() << "Trying alternative PostgreSQL paths...";

        // Try alternative PostgreSQL paths
        QStringList altPaths = {
            "C:/Program Files/PostgreSQL/15/bin",
            "C:/Program Files/PostgreSQL/14/bin",
            "C:/Program Files (x86)/PostgreSQL/16/bin",
            "C:/Program Files (x86)/PostgreSQL/15/bin"
        };

        for (const QString& altPath : altPaths) {
            QDir altDir(altPath);
            if (altDir.exists()) {
                pgPath = altPath;
                qDebug() << "Found PostgreSQL at:" << pgPath;
                break;
            }
        }
    }

    // Ensure PostgreSQL DLLs are available
    QStringList requiredDlls = {"libpq.dll", "libiconv-2.dll", "libintl-8.dll"};
    QStringList missingDlls;

    for (const QString& dll : requiredDlls) {
        QString dllPath = pgPath + "/" + dll;
        if (!QFile::exists(dllPath)) {
            missingDlls << dll;
            qDebug() << "Missing PostgreSQL DLL:" << dllPath;
        }
    }

    if (!missingDlls.isEmpty()) {
        qDebug() << "ERROR: Missing PostgreSQL DLLs:" << missingDlls;
        qDebug() << "Attempting to copy missing DLLs to application directory...";

        QString appDir = QCoreApplication::applicationDirPath();
        for (const QString& dll : missingDlls) {
            QString sourcePath = pgPath + "/" + dll;
            QString destPath = appDir + "/" + dll;

            if (QFile::exists(sourcePath)) {
                if (QFile::copy(sourcePath, destPath)) {
                    qDebug() << "Copied" << dll << "to application directory";
                } else {
                    qDebug() << "Failed to copy" << dll;
                }
            }
        }

        // Check again after copying
        bool allFound = true;
        for (const QString& dll : requiredDlls) {
            if (!QFile::exists(appDir + "/" + dll)) {
                allFound = false;
                break;
            }
        }

        if (allFound) {
            qDebug() << "All PostgreSQL DLLs are now available in application directory";
        } else {
            qDebug() << "Still missing some PostgreSQL DLLs after copying attempt";
        }
    } else {
        qDebug() << "All PostgreSQL DLLs found successfully";
    }

    QString newPath = qtPath + ";" + pgPath + ";" + currentPath;
    qputenv("PATH", newPath.toLocal8Bit());
    qDebug() << "Updated PATH for PostgreSQL support";
    qDebug() << "PostgreSQL path:" << pgPath;
#endif

    qDebug() << "Available SQL drivers:" << QSqlDatabase::drivers();

    if (!QSqlDatabase::drivers().contains("QPSQL")) {
        qDebug() << "ERROR: QPSQL driver not found!";
        qDebug() << "This usually means PostgreSQL libraries are not found.";
        qDebug() << "Make sure libpq.dll and related PostgreSQL DLLs are in PATH";
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

bool AisDatabaseManager::connectFromSettings()
{
    SettingsDialog::DatabaseSettings dbSettings = SettingsDialog::getDatabaseSettings();

    bool portOk;
    int port = dbSettings.port.toInt(&portOk);
    if (!portOk) {
        port = 5432; // default PostgreSQL port
    }

    qDebug() << "Connecting to database using settings:";
    qDebug() << "Host:" << dbSettings.host;
    qDebug() << "Port:" << port;
    qDebug() << "Database:" << dbSettings.dbName;
    qDebug() << "User:" << dbSettings.user;

    return connect(dbSettings.host, port, dbSettings.dbName, dbSettings.user, dbSettings.password);
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
    // Use our AIVDOEncoder to decode vessel name from NMEA
    AisDecoded decoded = AIVDOEncoder::decodeNMEALine(nmea);

    QString vesselName = "";

    // Check if this is Type 5 message (Static and Voyage Related Data)
    if (decoded.type == 5) {
        vesselName = decoded.name;
        // FIX: Replace @ with space for better display
        vesselName.replace("@", " ");
        vesselName = vesselName.trimmed();
    }

    return vesselName;
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
                "vessel_name = CASE "
                "WHEN EXCLUDED.vessel_name IS NOT NULL AND EXCLUDED.vessel_name != '' "
                "THEN EXCLUDED.vessel_name "
                "ELSE target_references.vessel_name "
                "END, "
                "call_sign = CASE "
                "WHEN EXCLUDED.call_sign IS NOT NULL AND EXCLUDED.call_sign != '' "
                "THEN EXCLUDED.call_sign "
                "ELSE target_references.call_sign "
                "END, "
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

    // Extract additional data from targetInfo
    QString callSign = QString::fromUtf8(targetInfo.callSign).replace("'", "''");
    QString imo = targetInfo.imoNumber > 0 ? QString::number(targetInfo.imoNumber) : "NULL";
    QString shipType = targetInfo.shipType > 0 ? QString::number(targetInfo.shipType) : "NULL";
    QString sog = (targetInfo.sog > 0 && targetInfo.sog < 1023) ? QString::number(targetInfo.sog/10) : "NULL";
    QString cog = (targetInfo.cog > 0 && targetInfo.cog < 3600) ? QString::number(targetInfo.cog/10) : "NULL";
    QString trueHeading = (targetInfo.heading > 0 && targetInfo.heading <= 360) ? QString::number(targetInfo.heading) : "NULL";

    // Extract message type from NMEA
    QString messageType = "NULL";
    QStringList parts = nmea.split(',');
    if (parts.length() >= 6 && !parts[1].isEmpty()) {
        messageType = QString("'%1'").arg(parts[1]); // Message type is field 2 (index 1)
    }

    // Handle call sign - empty string should be NULL, not quoted
    QString callSignSql = callSign.isEmpty() ? "NULL" : QString("'%1'").arg(callSign);

    // Fast insert without trigger overhead - using received_at for correct timezone
    QSqlQuery query(db);
    QString sql = QString(
        "INSERT INTO nmea_records (nmea, data_source, mmsi, latitude, longitude, "
        "vessel_name, raw_data_type, parsed_successfully, received_at, "
        "message_type, call_sign, imo, ship_type, speed_over_ground, course_over_ground, true_heading, "
        "destination, draught) "
        "VALUES ('%1', '%2', %3, %4, %5, '%6', '%7', true, TIMESTAMP '%8', "
        "%9, %10, %11, %12, %13, %14, %15, NULL, NULL)"
    ).arg(QString(nmea).replace("'", "''"))
     .arg(dataSource)
     .arg(mmsi)
     .arg(latitude != 0 ? QString::number(latitude, 'f', 6) : "NULL")
     .arg(longitude != 0 ? QString::number(longitude, 'f', 6) : "NULL")
     .arg(QString::fromUtf8(targetInfo.shipName).replace("'", "''"))
     .arg(determineDataType(nmea))
     .arg(currentTime.toString("yyyy-MM-dd hh:mm:ss.zzz"))
     .arg(messageType)
     .arg(callSignSql)
     .arg(imo)
     .arg(shipType)
     .arg(sog)
     .arg(cog)
     .arg(trueHeading);

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
        // Normal successful inserts are silent for clean logging
    } else {
        qWarning() << "Fast AIS insert failed for MMSI:" << mmsi;
        qWarning() << "SQL Error:" << query.lastError().text();
        // Only show partial SQL query to avoid log spam
        qWarning() << "SQL Query (first 100 chars):" << sql.left(100) << "...";
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

// NMEA Playback Control - Get targets for specific date
QList<AisDatabaseManager::TargetData> AisDatabaseManager::getTargetsForDate(const QDateTime& date) {
    QList<TargetData> targets;

    if (!db.isOpen()) {
        qCritical() << "Database is not open";
        return targets;
    }

    QString dateStr = date.toString("yyyy-MM-dd");
    qCritical() << "Fetching targets for date:" << dateStr;

    // (a) Get static data from target_references
    QHash<quint32, TargetData> staticDataMap;
    QSqlQuery staticQuery;
    QString staticSql = QString(
        "SELECT DISTINCT mmsi, vessel_name, call_sign, imo, ship_type "
        "FROM target_references "
        "WHERE DATE(created_at) = DATE('%1') OR DATE(last_seen) = DATE('%1')"
    ).arg(dateStr);

    if (staticQuery.exec(staticSql)) {
        while (staticQuery.next()) {
            TargetData data;
            data.mmsi = staticQuery.value("mmsi").toUInt();
            data.vesselName = staticQuery.value("vessel_name").toString();
            data.vesselName.replace("@", " "); // Fix @ symbols in vessel names
            data.callSign = staticQuery.value("call_sign").toString();
            data.callSign.replace("@", " "); // Fix @ symbols in call signs
            data.imo = staticQuery.value("imo").toULongLong();
            data.shipType = staticQuery.value("ship_type").toInt();

            
            staticDataMap[data.mmsi] = data;
        }
        qCritical() << "Found" << staticDataMap.size() << "static targets from target_references";
    } else {
        qCritical() << "Error fetching static data:" << staticQuery.lastError().text();
    }

    // (b) Get all unique MMSIs with their navigation data directly from nmea_records
    QHash<quint32, TargetData> nmeaDataMap;

    QSqlQuery nmeaQuery;
    QString nmeaSql = QString(
        "SELECT mmsi, latitude, longitude, speed_over_ground, course_over_ground, true_heading "
        "FROM nmea_records "
        "WHERE DATE(timestamp) = DATE('%1') AND mmsi IS NOT NULL AND mmsi != 0 "
        "GROUP BY mmsi, latitude, longitude, speed_over_ground, course_over_ground, true_heading"
    ).arg(dateStr);

    if (nmeaQuery.exec(nmeaSql)) {
        while (nmeaQuery.next()) {
            TargetData data;
            data.mmsi = nmeaQuery.value("mmsi").toUInt();
            data.latitude = nmeaQuery.value("latitude").toDouble();
            data.longitude = nmeaQuery.value("longitude").toDouble();
            data.sog = nmeaQuery.value("speed_over_ground").toDouble();
            data.cog = nmeaQuery.value("course_over_ground").toDouble();
            data.heading = nmeaQuery.value("true_heading").toDouble();

            // Store the data - this will include all navigation data for each MMSI
            nmeaDataMap[data.mmsi] = data;
        }
        qCritical() << "Found" << nmeaDataMap.size() << "targets with navigation data from nmea_records";
    } else {
        qCritical() << "Error fetching NMEA data:" << nmeaQuery.lastError().text();
    }

    // (c) Combine static data from target_references with navigation data from nmea_records
    QSet<quint32> allTargetMmsis = QSet<quint32>::fromList(staticDataMap.keys()) + QSet<quint32>::fromList(nmeaDataMap.keys());

    for (quint32 mmsi : allTargetMmsis) {
        TargetData combined;

        // Start with static data from target_references if available
        if (staticDataMap.contains(mmsi)) {
            combined = staticDataMap[mmsi];
        } else {
            combined.mmsi = mmsi;
        }

        // Merge with navigation data from nmea_records if available
        if (nmeaDataMap.contains(mmsi)) {
            const TargetData& nmea = nmeaDataMap[mmsi];
            if (nmea.latitude != 0) combined.latitude = nmea.latitude;
            if (nmea.longitude != 0) combined.longitude = nmea.longitude;
            if (nmea.sog != 0) combined.sog = nmea.sog;
            if (nmea.cog != 0) combined.cog = nmea.cog;
            if (nmea.heading != 0) combined.heading = nmea.heading;
        }

        targets.append(combined);
    }

    return targets;
}

QList<AisDatabaseManager::TargetData> AisDatabaseManager::getTargetsForDateRev(const QDateTime& date) {
    QList<TargetData> targets;

    if (!db.isOpen()) {
        qCritical() << "Database is not open";
        return targets;
    }

    QString dateStr = date.toString("yyyy-MM-dd");
    qCritical() << "Fetching targets for date:" << dateStr;

    // (a) Get static data from target_references
    QHash<quint32, TargetData> staticDataMap;
    QSqlQuery staticQuery;
    QString staticSql = QString(
                            "SELECT DISTINCT mmsi, vessel_name, call_sign, imo, ship_type "
                            "FROM target_references "
                            "WHERE DATE(created_at) = DATE('%1') OR DATE(last_seen) = DATE('%1')"
                            ).arg(dateStr);

    if (staticQuery.exec(staticSql)) {
        while (staticQuery.next()) {
            TargetData data;
            data.mmsi = staticQuery.value("mmsi").toUInt();
            data.vesselName = staticQuery.value("vessel_name").toString();
            data.vesselName.replace("@", " "); // Fix @ symbols in vessel names
            data.callSign = staticQuery.value("call_sign").toString();
            data.callSign.replace("@", " "); // Fix @ symbols in call signs
            data.imo = staticQuery.value("imo").toULongLong();
            data.shipType = staticQuery.value("ship_type").toInt();


            staticDataMap[data.mmsi] = data;
        }
        qCritical() << "Found" << staticDataMap.size() << "static targets from target_references";
    } else {
        qCritical() << "Error fetching static data:" << staticQuery.lastError().text();
    }

    // (b) Get all unique MMSIs with their raw NMEA strings from nmea_records
    QHash<quint32, TargetData> nmeaDataMap;

    QSqlQuery nmeaQuery;
    QString nmeaSql = QString(
                          "SELECT DISTINCT ON (mmsi) mmsi, nmea "
                          "FROM nmea_records "
                          "WHERE DATE(timestamp) = DATE('%1') AND mmsi IS NOT NULL AND mmsi != 0 "
                          "AND (message_type = '1' OR message_type = '2' OR message_type = '3' OR message_type = '18' OR message_type = '19') "
                          "ORDER BY mmsi, timestamp DESC"
                          ).arg(dateStr);

    if (nmeaQuery.exec(nmeaSql)) {
        while (nmeaQuery.next()) {
            TargetData data;
            data.mmsi = nmeaQuery.value("mmsi").toUInt();
            data.nmea = nmeaQuery.value("nmea").toString();

            // Store the raw NMEA string
            nmeaDataMap[data.mmsi] = data;
        }
        qCritical() << "Found" << nmeaDataMap.size() << "targets with raw NMEA from nmea_records";
    } else {
        qCritical() << "Error fetching NMEA data:" << nmeaQuery.lastError().text();
    }

    // (c) Combine static data from target_references with navigation data from nmea_records
    QSet<quint32> allTargetMmsis = QSet<quint32>::fromList(staticDataMap.keys()) + QSet<quint32>::fromList(nmeaDataMap.keys());

    for (quint32 mmsi : allTargetMmsis) {
        TargetData combined;

        // Start with static data from target_references if available
        if (staticDataMap.contains(mmsi)) {
            combined = staticDataMap[mmsi];
        } else {
            combined.mmsi = mmsi;
        }

        // Add raw NMEA from nmea_records if available
        if (nmeaDataMap.contains(mmsi)) {
            const TargetData& nmea = nmeaDataMap[mmsi];
            combined.nmea = nmea.nmea;
        }

        targets.append(combined);
    }

    return targets;
}


// Encode targets to NMEA 0183
QStringList AisDatabaseManager::encodeTargetsToNMEA(const QList<TargetData>& targets) {
    AIVDOEncoder encoder;
    QStringList nmeaList;

    // Start encoding targets to NMEA 0183

    for (const TargetData& target : targets) {
        // 1. NAV - Use raw NMEA from database (if available)
        if (!target.nmea.isEmpty()) {
            nmeaList.append(target.nmea);
            // Raw NMEA added from database
        } else {
            // No raw NMEA available in database for this MMSI
        }

        // 2. VES - Type 5 - Vessel Name and Call Sign (always if vessel name available)
        if (!target.vesselName.isEmpty()) {
            // Trim whitespace from database values before encoding
            QString cleanCallSign = target.callSign.trimmed();
            QString cleanVesselName = target.vesselName.trimmed();

            QStringList nmeaType5List = encoder.encodeVesselNameType5(target.mmsi, cleanCallSign, cleanVesselName);

            if (!nmeaType5List.isEmpty()) {
                // Add all Type 5 fragments (usually 2 strings)
                for (const QString& nmeaType5 : nmeaType5List) {
                    nmeaList.append(nmeaType5);
                }
                // Type 5 encoding successful
            }
        }
        // No vessel name available for Type 5 encoding
    }

    return nmeaList;
}
