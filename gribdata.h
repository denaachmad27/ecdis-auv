#ifndef GRIBDATA_H
#define GRIBDATA_H

#include <QDateTime>
#include <QVector>
#include <QString>

/**
 * @brief Structure to hold wave data at a specific grid point
 */
struct GribWaveData {
    double latitude;       // Latitude in degrees
    double longitude;      // Longitude in degrees
    double waveHeight;     // Significant wave height (swh) in meters
    double waveDirection;  // Mean wave direction (mpwd) in degrees (0-360, from north clockwise)
    double wavePeriod;     // Mean wave period (mwp) in seconds
    double windSpeed;      // Wind speed in m/s (optional)
    double windDirection;  // Wind direction in degrees (optional)
    bool isValid;          // Data validity flag

    GribWaveData()
        : latitude(0.0), longitude(0.0), waveHeight(-999.0),
          waveDirection(-999.0), wavePeriod(-999.0),
          windSpeed(-999.0), windDirection(-999.0), isValid(false) {}
};

/**
 * @brief Structure to hold a GRIB message with time step data
 */
struct GribMessage {
    QDateTime referenceTime;    // Analysis/reference time
    QDateTime forecastTime;     // Forecast valid time
    int forecastHour;           // Forecast hour offset from reference
    QVector<GribWaveData> dataPoints;  // Grid data points

    // Grid information
    double minLat;
    double maxLat;
    double minLon;
    double maxLon;
    int ni;                     // Number of points along longitude
    int nj;                     // Number of points along latitude
    double di;                  // Longitude increment
    double dj;                  // Latitude increment

    GribMessage()
        : forecastHour(0), minLat(0.0), maxLat(0.0),
          minLon(0.0), maxLon(0.0), ni(0), nj(0), di(0.0), dj(0.0) {}

    /**
     * @brief Get data point at grid indices
     */
    GribWaveData getDataPoint(int i, int j) const {
        int index = j * ni + i;
        if (index >= 0 && index < dataPoints.size()) {
            return dataPoints[index];
        }
        return GribWaveData();
    }

    /**
     * @brief Check if coordinates are within this message's bounds
     */
    bool contains(double lat, double lon) const {
        return lat >= minLat && lat <= maxLat &&
               lon >= minLon && lon <= maxLon;
    }
};

/**
 * @brief Container for loaded GRIB file data
 */
class GribData
{
public:
    GribData();
    ~GribData();

    void clear();
    bool isEmpty() const { return messages.isEmpty(); }

    // File information
    QString fileName;
    QString filePath;
    qint64 fileSize;

    // Data messages (time steps)
    QVector<GribMessage> messages;

    // Overall bounds
    double globalMinLat;
    double globalMaxLat;
    double globalMinLon;
    double globalMaxLon;

    // Parameter info
    QString parameterName;      // e.g., "Significant wave height"
    QString parameterUnits;     // e.g., "m"
    QString generatingCenter;   // e.g., "ECMWF"
    QString model;              // e.g., "Wave model"

    /**
     * @brief Get total number of time steps
     */
    int getTimeStepCount() const { return messages.size(); }

    /**
     * @brief Get time step at index
     */
    GribMessage getTimeStep(int index) const {
        if (index >= 0 && index < messages.size()) {
            return messages[index];
        }
        return GribMessage();
    }

    /**
     * @brief Get list of forecast times as strings
     */
    QStringList getTimeStepLabels() const;

    /**
     * @brief Update global bounds from all messages
     */
    void updateBounds();
};

#endif // GRIBDATA_H
