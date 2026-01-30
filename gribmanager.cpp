#include "gribmanager.h"
#include <QDateTime>
#include <QDebug>
#include <cmath>

// Define if eccodes is available
// Uncomment this after installing eccodes and updating ecdis.pro
// #define USE_ECCODES

#ifdef USE_ECCODES
#include "eccodes.h"
#endif

GribManager::GribManager(QObject *parent)
    : QObject(parent)
    , m_currentTimeStep(0)
    , m_showHeatmap(true)
    , m_showArrows(true)
    , m_arrowDensity(5)  // Default: show arrows every 5 grid points
    , m_eccodesAvailable(false)
{
#ifdef USE_ECCODES
    m_eccodesAvailable = true;
    qDebug() << "[GRIB] eccodes support is enabled";
#else
    m_eccodesAvailable = false;
    qDebug() << "[GRIB] eccodes support is NOT enabled - using sample data mode";
    qDebug() << "[GRIB] To enable eccodes: install library and add #define USE_ECCODES in gribmanager.cpp";
#endif
}

GribManager::~GribManager()
{
    clear();
}

bool GribManager::loadFromFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        QString error = QString("File not found: %1").arg(filePath);
        qWarning() << "[GRIB]" << error;
        emit loadFailed(error);
        return false;
    }

    // Clear previous data
    clear();

    // Store file info
    m_data.fileName = fileInfo.fileName();
    m_data.filePath = fileInfo.absoluteFilePath();
    m_data.fileSize = fileInfo.size();

    // Try to load with eccodes or create sample data
    bool success = false;

#ifdef USE_ECCODES
    success = parseWithEccodes(filePath);
#else
    // For now, create sample data based on file info for testing UI
    success = createSampleData(filePath);
#endif

    if (success) {
        m_data.updateBounds();
        m_currentTimeStep = 0;
        qDebug() << "[GRIB] File loaded successfully:" << m_data.fileName;
        qDebug() << "[GRIB] Time steps:" << m_data.getTimeStepCount();
        qDebug() << "[GRIB] Bounds:" << m_data.globalMinLat << m_data.globalMaxLat
                 << m_data.globalMinLon << m_data.globalMaxLon;
        emit fileLoaded(m_data.fileName);
        emit timeStepChanged(0);
        return true;
    } else {
        QString error = "Failed to parse GRIB file";
        emit loadFailed(error);
        return false;
    }
}

void GribManager::setCurrentTimeStep(int step)
{
    if (step >= 0 && step < m_data.getTimeStepCount()) {
        m_currentTimeStep = step;
        emit timeStepChanged(step);
    }
}

GribMessage GribManager::getCurrentMessage() const
{
    return m_data.getTimeStep(m_currentTimeStep);
}

QString GribManager::getFileInfo() const
{
    if (!isLoaded()) {
        return tr("No file loaded");
    }

    QString info;
    info += tr("File: %1\n").arg(m_data.fileName);
    info += tr("Size: %1 KB\n").arg(m_data.fileSize / 1024);

    if (!m_data.generatingCenter.isEmpty()) {
        info += tr("Center: %1\n").arg(m_data.generatingCenter);
    }

    if (!m_data.model.isEmpty()) {
        info += tr("Model: %1\n").arg(m_data.model);
    }

    info += tr("Time Steps: %1\n").arg(m_data.getTimeStepCount());
    info += tr("Grid: %1 x %2 points\n").arg(m_data.globalMaxLat > m_data.globalMinLat ?
                                                     QString::number((m_data.globalMaxLat - m_data.globalMinLat) * 10, 'f', 0) : "0")
                                              .arg("180"); // Approximate

    info += tr("Bounds:\n");
    info += tr("  Lat: %1° to %2°\n").arg(m_data.globalMinLat, 0, 'f', 2)
                                        .arg(m_data.globalMaxLat, 0, 'f', 2);
    info += tr("  Lon: %1° to %2°\n").arg(m_data.globalMinLon, 0, 'f', 2)
                                        .arg(m_data.globalMaxLon, 0, 'f', 2);

    // Get wave height range
    double minH, maxH;
    getWaveHeightRange(minH, maxH);
    info += tr("Wave Height: %1m - %2m\n").arg(minH, 0, 'f', 2).arg(maxH, 0, 'f', 2);

    return info;
}

void GribManager::clear()
{
    m_data.clear();
    m_currentTimeStep = 0;
    emit dataCleared();
}

GribWaveData GribManager::getWaveDataAt(double lat, double lon) const
{
    if (!isLoaded()) {
        return GribWaveData();
    }

    GribMessage msg = getCurrentMessage();

    if (!msg.contains(lat, lon)) {
        return GribWaveData();
    }

    // Find nearest grid point
    double i = (lon - msg.minLon) / msg.di;
    double j = (lat - msg.minLat) / msg.dj;

    int i0 = static_cast<int>(std::floor(i));
    int j0 = static_cast<int>(std::floor(j));

    if (i0 >= 0 && i0 < msg.ni - 1 && j0 >= 0 && j0 < msg.nj - 1) {
        // Simple bilinear interpolation could be done here
        // For now, return nearest point
        return msg.getDataPoint(i0, j0);
    }

    return GribWaveData();
}

void GribManager::getWaveHeightRange(double& minH, double& maxH) const
{
    minH = 9999.0;
    maxH = -9999.0;

    GribMessage msg = getCurrentMessage();
    if (msg.dataPoints.isEmpty()) {
        minH = maxH = 0.0;
        return;
    }

    for (const auto& data : msg.dataPoints) {
        if (data.isValid && data.waveHeight > -900) {
            if (data.waveHeight < minH) minH = data.waveHeight;
            if (data.waveHeight > maxH) maxH = data.waveHeight;
        }
    }

    if (minH > 900) minH = 0.0;
    if (maxH < -900) maxH = 0.0;
}

QString GribManager::getParameterName(int parameterId, int parameterCategory)
{
    // GRIB2 parameter table (simplified)
    if (parameterCategory == 0) {  // Temperature
        switch (parameterId) {
            case 0: return "Temperature";
            case 1: return "Maximum temperature";
            case 2: return "Minimum temperature";
        }
    } else if (parameterCategory == 1) {  // Moisture
        switch (parameterId) {
            case 0: return "Relative humidity";
        }
    } else if (parameterCategory == 2) {  // Momentum
        switch (parameterId) {
            case 0: return "Wind speed";
            case 1: return "Wind direction";
        }
    } else if (parameterCategory == 3) {  // Mass
        switch (parameterId) {
            case 0: return "Pressure";
        }
    } else if (parameterCategory == 10) {  // Wave
        switch (parameterId) {
            case 0: return "Significant wave height";  // swh
            case 1: return "Wave direction";
            case 2: return "Wave period";
        }
    }

    return QString("Param_%1_%2").arg(parameterCategory).arg(parameterId);
}

#ifdef USE_ECCODES
bool GribManager::parseWithEccodes(const QString& filePath)
{
    FILE* file = fopen(filePath.toUtf8().constData(), "rb");
    if (!file) {
        qWarning() << "[GRIB] Cannot open file:" << filePath;
        return false;
    }

    int err = 0;
    grib_handle* h = nullptr;

    // Loop through all messages in the file
    int msgCount = 0;
    while ((h = codes_handle_new_from_file(0, file, PRODUCT_GRIB, &err)) != nullptr || err != GRIB_SUCCESS) {
        if (err == GRIB_END_OF_FILE) {
            break;
        }

        if (err != GRIB_SUCCESS) {
            qWarning() << "[GRIB] Error reading message:" << err;
            break;
        }

        GribMessage msg;
        msg.forecastHour = 0;

        // Get basic info
        char value[1024];
        size_t len = sizeof(value);

        // Get generating center
        if (codes_get_long(h, "centre", &err) == 0) {
            long centre;
            codes_get_long(h, "centre", &centre);
            // ECMWF = 98, etc.
            switch (centre) {
                case 98: m_data.generatingCenter = "ECMWF"; break;
                case 7: m_data.generatingCenter = "NCEP"; break;
                case 74: m_data.generatingCenter = "Met Office"; break;
                default: m_data.generatingCenter = QString("Center_%1").arg(centre);
            }
        }

        // Get data date/time
        long dataDate, dataTime, forecastTime;
        if (codes_get_long(h, "dataDate", &dataDate) == 0) {
            int year = dataDate / 10000;
            int month = (dataDate / 100) % 100;
            int day = dataDate % 100;
            msg.referenceTime = QDateTime(QDate(year, month, day), QTime(0, 0));
        }

        if (codes_get_long(h, "dataTime", &dataTime) == 0) {
            int hour = dataTime / 100;
            int minute = dataTime % 100;
            msg.referenceTime.setTime(QTime(hour, minute));
        }

        if (codes_get_long(h, "forecastTime", &forecastTime) == 0) {
            msg.forecastHour = static_cast<int>(forecastTime);
            msg.forecastTime = msg.referenceTime.addSecs(forecastTime * 3600);
        } else {
            msg.forecastTime = msg.referenceTime;
        }

        // Get grid info
        long ni = 0, nj = 0;
        codes_get_long(h, "Ni", &ni);
        codes_get_long(h, "Nj", &nj);
        msg.ni = static_cast<int>(ni);
        msg.nj = static_cast<int>(nj);

        // Get grid increments
        double di = 0.0, dj = 0.0;
        codes_get_double(h, "iDirectionIncrement", &di);
        codes_get_double(h, "jDirectionIncrement", &dj);
        msg.di = di;
        msg.dj = dj;

        // Get area extents
        double latitudeOfFirstGridPoint, longitudeOfFirstGridPoint;
        double latitudeOfLastGridPoint, longitudeOfLastGridPoint;
        codes_get_double(h, "latitudeOfFirstGridPointInDegrees", &latitudeOfFirstGridPoint);
        codes_get_double(h, "longitudeOfFirstGridPointInDegrees", &longitudeOfFirstGridPoint);
        codes_get_double(h, "latitudeOfLastGridPointInDegrees", &latitudeOfLastGridPoint);
        codes_get_double(h, "longitudeOfLastGridPointInDegrees", &longitudeOfLastGridPoint);

        msg.minLat = qMin(latitudeOfFirstGridPoint, latitudeOfLastGridPoint);
        msg.maxLat = qMax(latitudeOfFirstGridPoint, latitudeOfLastGridPoint);
        msg.minLon = qMin(longitudeOfFirstGridPoint, longitudeOfLastGridPoint);
        msg.maxLon = qMax(longitudeOfFirstGridPoint, longitudeOfLastGridPoint);

        // Get parameter info
        long parameterCategory, parameterNumber;
        if (codes_get_long(h, "parameterCategory", &parameterCategory) == 0 &&
            codes_get_long(h, "parameterNumber", &parameterNumber) == 0) {
            QString paramName = getParameterName(parameterNumber, parameterCategory);
            if (m_data.parameterName.isEmpty()) {
                m_data.parameterName = paramName;
            }
        }

        // Get units
        len = sizeof(value);
        if (codes_get_string(h, "units", value, &len) == 0) {
            m_data.parameterUnits = QString::fromUtf8(value);
        }

        // Get data values
        size_t values_len = 0;
        codes_get_size(h, "values", &values_len);
        QVector<double> values(values_len);
        codes_get_double_array(h, "values", values.data(), &values_len);

        // Calculate grid positions and populate data points
        msg.dataPoints.reserve(values_len);

        // Determine grid scanning mode
        long scanningMode = 0;
        codes_get_long(h, "scanningMode", &scanningMode);

        int dataIndex = 0;
        for (int j = 0; j < nj; ++j) {
            for (int i = 0; i < ni; ++i) {
                GribWaveData data;
                data.waveHeight = values[dataIndex++];

                // Calculate lat/lon based on grid position
                if (scanningMode == 0 || scanningMode == 64) {
                    // West to East, depending on j direction
                    data.latitude = latitudeOfFirstGridPoint + j * dj;
                    data.longitude = longitudeOfFirstGridPoint + i * di;
                } else {
                    data.latitude = latitudeOfFirstGridPoint - j * dj;
                    data.longitude = longitudeOfFirstGridPoint + i * di;
                }

                data.isValid = (data.waveHeight > -900);
                data.waveDirection = 270.0;  // Default westerly
                data.wavePeriod = 8.0;

                msg.dataPoints.append(data);
            }
        }

        m_data.messages.append(msg);
        codes_handle_delete(h);
        msgCount++;
    }

    fclose(file);

    // Set model info from file name pattern if available
    if (m_data.fileName.contains("ECMWF")) {
        m_data.model = "ECMWF Wave Model";
        m_data.generatingCenter = "ECMWF";
    }

    if (m_data.parameterName.isEmpty()) {
        m_data.parameterName = "Significant wave height";
    }
    if (m_data.parameterUnits.isEmpty()) {
        m_data.parameterUnits = "m";
    }

    qDebug() << "[GRIB] Loaded" << msgCount << "messages from file";
    return msgCount > 0;
}
#endif

bool GribManager::createSampleData(const QString& filePath)
{
    qDebug() << "[GRIB] Creating sample data for UI testing";

    // Parse file info for bounds from filename pattern
    // Example: ECMWF_Wave_10k_5d_24h_24N_-25S_161E_89W_20260107_0045.grb
    QFileInfo fileInfo(filePath);

    // Set default bounds for Indonesia region
    m_data.globalMinLat = -25.0;
    m_data.globalMaxLat = 24.0;
    m_data.globalMinLon = 89.0;
    m_data.globalMaxLon = 161.0;
    m_data.generatingCenter = "ECMWF (Sample)";
    m_data.model = "ECMWF Wave Model (Sample Data)";
    m_data.parameterName = "Significant wave height";
    m_data.parameterUnits = "m";

    // Create 5 time steps (24-hour forecast, 6-hour steps)
    QDateTime baseTime = QDateTime::currentDateTimeUtc();
    baseTime.setTime(QTime(0, 0));

    // Grid resolution
    double latStep = 0.5;  // ~50km
    double lonStep = 0.5;

    int ni = static_cast<int>((m_data.globalMaxLon - m_data.globalMinLon) / lonStep) + 1;
    int nj = static_cast<int>((m_data.globalMaxLat - m_data.globalMinLat) / latStep) + 1;

    for (int step = 0; step < 5; ++step) {
        GribMessage msg;
        msg.referenceTime = baseTime;
        msg.forecastHour = step * 6;
        msg.forecastTime = baseTime.addSecs(step * 6 * 3600);
        msg.minLat = m_data.globalMinLat;
        msg.maxLat = m_data.globalMaxLat;
        msg.minLon = m_data.globalMinLon;
        msg.maxLon = m_data.globalMaxLon;
        msg.ni = ni;
        msg.nj = nj;
        msg.di = lonStep;
        msg.dj = latStep;

        // Generate synthetic wave data
        msg.dataPoints.reserve(ni * nj);

        for (int j = 0; j < nj; ++j) {
            for (int i = 0; i < ni; ++i) {
                GribWaveData data;
                data.latitude = m_data.globalMinLat + j * latStep;
                data.longitude = m_data.globalMinLon + i * lonStep;

                // Create some synthetic wave patterns
                // Wave height varies with latitude (higher in southern hemisphere)
                double latFactor = 1.0 - (data.latitude - m_data.globalMinLat) /
                                             (m_data.globalMaxLat - m_data.globalMinLat);
                double timeFactor = 1.0 + 0.2 * std::sin(step * M_PI / 2.5);

                // Add some spatial variation
                double spatialVar = std::sin(data.longitude * M_PI / 180.0 * 3) *
                                    std::cos(data.latitude * M_PI / 180.0 * 2);

                data.waveHeight = 1.0 + 2.0 * latFactor * timeFactor + 0.5 * spatialVar;
                data.waveHeight = qMax(0.1, data.waveHeight);  // Minimum 0.1m

                // Wave direction (from southwest in Indonesia region)
                data.waveDirection = 225.0 + 30.0 * spatialVar + step * 5;
                if (data.waveDirection >= 360.0) data.waveDirection -= 360.0;

                // Wave period related to height
                data.wavePeriod = 6.0 + 2.0 * data.waveHeight;

                data.isValid = true;
                msg.dataPoints.append(data);
            }
        }

        m_data.messages.append(msg);
    }

    return true;
}
