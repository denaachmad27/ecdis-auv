#ifndef AISDATABASEMANAGER_H
#define AISDATABASEMANAGER_H

#include <QtSql>
#include <QDebug>
#include <QAtomicInt>
#include <QTimer>
#include <QMutex>
#include <QDateTime>
#include <QObject>
#include <QQueue>
#include <QHash>
#include "AIVDOEncoder.h"
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
#include <ecs63.h>
#endif

class AisDatabaseManager {
public:
    static AisDatabaseManager& instance();
    bool connect(const QString& host, int port, const QString& dbName,
                 const QString& user, const QString& password);

    // Connect using settings from config file
    bool connectFromSettings();

    // Check if database is connected
    bool isConnected() const;

    // Legacy functions (keep for backward compatibility)
    void insertOrUpdateAisTarget(const EcAISTargetInfo& info);
    void insertOwnShipToDB(double lat, double lon, double depth,
                       double heading, double headingOverGround,
                       double speed, double speedOverGround,
                           double yaw, double z);
    void insertOwnShipToDB(QString nmea);
    void getOwnShipNmeaData(QSqlQuery& query, const QDateTime& startTime, const QDateTime& endTime);

    // New unified recording functions
    bool insertNmeaRecord(const QString& nmea, const QString& dataSource,
                          quint32 mmsi = 0, const QString& messageType = "");

    // Enhanced recording with parsed AIS data (with throttling)
    bool insertParsedAisData(const QString& nmea, const QString& dataSource,
                           quint32 mmsi, const EcAISTargetInfo& targetInfo);
    bool insertParsedOwnshipData(const QString& nmea, const QString& dataSource,
                               double lat, double lon, double sog, double cog, double heading);

    void startRecordingSession(const QString& sessionName = "");
    void stopRecordingSession();
    QUuid getCurrentSessionId() const { return currentSessionId; }

    // New unified playback functions
    void getCombinedNmeaData(QSqlQuery& query, const QDateTime& startTime, const QDateTime& endTime,
                           const QStringList& dataSources = {"ownship", "aistarget"});
    void getNmeaDataWithFilter(QSqlQuery& query, const QDateTime& startTime, const QDateTime& endTime,
                              const QStringList& dataSources = {},
                              const QList<quint32>& mmsiFilter = {});

    // Session management
    void getRecordingSessions(QSqlQuery& query);
    void createPlaybackRequest(const QString& userId, const QDateTime& startTime,
                              const QDateTime& endTime, double playbackSpeed = 1.0);

    // NMEA Playback Control - Get targets for specific date
    struct TargetData {
        quint32 mmsi;
        QString vesselName;
        QString callSign;
        quint64 imo;
        int shipType;
        double latitude;
        double longitude;
        double sog;
        double cog;
        double heading;
        QDateTime timestamp;
        QString nmea;

        TargetData() : mmsi(0), imo(0), shipType(0), latitude(0), longitude(0),
                      sog(0), cog(0), heading(0) {}
    };

    QList<TargetData> getTargetsForDate(const QDateTime& date);
    QList<TargetData> getTargetsForDateRev(const QDateTime& date);

    QStringList encodeTargetsToNMEA(const QList<TargetData>& targets);

    ~AisDatabaseManager();

private:
    AisDatabaseManager();
    QSqlDatabase db;
    QUuid currentSessionId;

    // Simple throttling
    static QDateTime lastAisRecordTime;
    static QDateTime lastOwnshipRecordTime;
    static const int RECORDING_THROTTLE_MS = 2000; // 2 seconds minimum interval

    // High-performance async processing
    struct TargetReferenceRecord {
        quint32 mmsi;
        QString vesselName;
        QString callSign;
        quint64 imo;
        int shipType;
        QDateTime timestamp;
        int sourceCount;

        TargetReferenceRecord() : mmsi(0), imo(0), shipType(0), sourceCount(1) {}
    };

    QHash<quint32, TargetReferenceRecord> targetReferenceCache;
    QMutex cacheMutex;
    QTimer* asyncProcessingTimer;

    // Performance setup
    void setupPerformanceOptimizations();
    void processTargetReferencesAsync();

    // Fast insert functions (no trigger overhead)
    bool insertParsedAisDataFast(const QString& nmea, const QString& dataSource,
                                quint32 mmsi, const EcAISTargetInfo& targetInfo);
    bool insertParsedOwnshipDataFast(const QString& nmea, const QString& dataSource,
                                    double lat, double lon, double sog, double cog, double heading);

    // Helper functions
    QString parseAisMessageType(const QString& nmea);
    quint32 extractMmsiFromNmea(const QString& nmea);
    QString extractVesselNameFromNmea(const QString& nmea);
    QString determineDataType(const QString& nmea);
};

#endif // AISDATABASEMANAGER_H
