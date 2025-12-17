#ifndef TIDEMANAGER_H
#define TIDEMANAGER_H

#include <QObject>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>

#ifdef _WINNT_SOURCE
#ifdef _MSC_VER
#pragma pack(push, 4)  // Match EC2007 struct packing
#include <windows.h>
#pragma pack(pop)
#else
#include <windows.h>
#endif
#else
#ifdef __GNUC__
#pragma pack(push, 4)  // Match EC2007 struct packing for GCC
#endif
#endif

#include "eckernel.h"

// Simple coordinate struct
struct Coordinate {
    double latitude;
    double longitude;

    Coordinate() : latitude(0.0), longitude(0.0) {}
    Coordinate(double lat, double lon) : latitude(lat), longitude(lon) {}
};

struct TidePrediction {
    QDateTime dateTime;
    double height;
    bool isHighTide;
    QString stationName;
    Coordinate stationLocation;
};

struct TideStation {
    QString id;
    QString name;
    Coordinate location;
    QString description;
};

class TideManager : public QObject
{
    Q_OBJECT

public:
    explicit TideManager(QObject *parent = nullptr);
    ~TideManager();

    bool initialize();
    bool loadTideData(const QString &cellPath);
    bool loadTideDataFromJson(const QString &jsonPath);
    void setPredictionLocation(double latitude, double longitude, double searchRadius = 10.0);
    void setPredictionLocationByName(const QString &stationName);
    void setPredictionLocationById(const QString &stationId);

    QList<TidePrediction> getTidePredictions(const QDateTime &startTime,
                                           const QDateTime &endTime,
                                           bool highLowOnly = true) const;

    TidePrediction getCurrentTide() const;
    QList<TidePrediction> getTodaysTides() const;

    QList<TideStation> getAvailableStations() const;
    QList<TideStation> getJsonStations() const;
    QString getLastError() const;
    bool isInitialized() const { return m_initialized; }
    bool isUsingJsonData() const { return m_usingJsonData; }

    // Chart visualization methods
    void drawStationMarkers(void *view, void *dictInfo, const QString &cellPath);
    TideStation getStationAtPosition(EcCoordinate latitude, EcCoordinate longitude, double tolerance = 0.01) const;

signals:
    void tideDataLoaded();
    void predictionsUpdated();
    void errorOccurred(const QString &error);

private:
    void cleanup();
    QString formatError(unsigned long errorCode) const;
    QDateTime predictionDateToQDateTime(const EcPredictionDate &date) const;

    // JSON data methods
    QList<TidePrediction> getTidePredictionsFromJson(const QDateTime &startTime,
                                                    const QDateTime &endTime) const;
    bool loadJsonTideStations();
    void parseJsonTideData(const QJsonObject &jsonData);

    // Web API fallback methods
    QList<TidePrediction> getTidePredictionsFromWeb(const QDateTime &startTime,
                                                   const QDateTime &endTime) const;
    bool loadWebTideStations();
    void fetchTideDataAsync(const QString &stationUrl);

    EcTidesPredictionContext *m_predictionContext;
    EcTidesStationContext *m_stationContext;
    EcCellId m_cellId;
    EcDictInfo *m_dictInfo;

    bool m_initialized;
    bool m_cellLoaded;
    bool m_usingJsonData;
    double m_latitude;
    double m_longitude;
    double m_searchRadius;
    QString m_lastError;
    QString m_currentStationName;
    QString m_currentStationId;

    // JSON data storage
    QList<TideStation> m_jsonStations;
    QJsonObject m_jsonTideData;
};

#ifdef __GNUC__
#pragma pack(pop)  // End of EC2007 struct packing for GCC
#endif

#endif // TIDEMANAGER_H