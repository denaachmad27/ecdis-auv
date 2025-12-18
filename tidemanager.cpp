#include "tidemanager.h"
#include <QDir>
#include <QDebug>
#include <QDate>
#include <QTime>

TideManager::TideManager(QObject *parent)
    : QObject(parent)
    , m_predictionContext(nullptr)
    , m_stationContext(nullptr)
    , m_cellId(EC_NOCELLID)
    , m_dictInfo(nullptr)
    , m_initialized(false)
    , m_cellLoaded(false)
    , m_usingJsonData(false)
    , m_latitude(0.0)
    , m_longitude(0.0)
    , m_searchRadius(10.0)
{
    qDebug() << "[TIDE-MGR] TideManager constructor";
}

TideManager::~TideManager()
{
    cleanup();
}

bool TideManager::initialize()
{
    qDebug() << "[TIDE-MGR] initialize() called";

    if (m_initialized) {
        qDebug() << "[TIDE-MGR] Already initialized";
        return true;
    }

    // Read standard dictionaries
    qDebug() << "[TIDE-MGR] Reading dictionaries...";
    m_dictInfo = EcDictionaryReadModule(EC_MODULE_MAIN, NULL);
    if (m_dictInfo == NULL) {
        m_lastError = "Cannot read dictionaries";
        qDebug() << "[TIDE-MGR] ERROR:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }
    qDebug() << "[TIDE-MGR] Dictionaries read successfully";

    // Initialize prediction context
    qDebug() << "[TIDE-MGR] Initializing tide prediction context...";
    m_predictionContext = EcTidesInitialize();
    if (m_predictionContext == NULL) {
        m_lastError = formatError(EcKernelGetLastError(EC_LAST));
        m_lastError = "EcTidesInitialize failed: " + m_lastError;
        qDebug() << "[TIDE-MGR] ERROR:" << m_lastError;
        emit errorOccurred(m_lastError);
        cleanup();
        return false;
    }
    qDebug() << "[TIDE-MGR] Tide prediction context initialized successfully";

    m_initialized = true;
    return true;
}

bool TideManager::loadTideData(const QString &cellPath)
{
    qDebug() << "[TIDE-MGR] loadTideData() called with path:" << cellPath;

    if (!m_initialized) {
        qDebug() << "[TIDE-MGR] Not initialized, calling initialize()";
        if (!initialize()) {
            qDebug() << "[TIDE-MGR] Initialization failed";
            return false;
        }
    }

    // Unmap any existing cell
    if (m_cellLoaded && m_cellId != EC_NOCELLID) {
        EcCellUnmap(m_cellId);
        m_cellLoaded = false;
    }

    // Convert QString to char array
    QByteArray pathBytes = cellPath.toLocal8Bit();

    // Map the tide constituent cell
    qDebug() << "[TIDE-MGR] Mapping tide cell:" << pathBytes.constData();
    m_cellId = EcCellMap(pathBytes.constData(), EC_ACCESSREAD, 1);
    if (m_cellId == EC_NOCELLID) {
        m_lastError = QString("Cannot open tide data file: %1").arg(cellPath);
        qDebug() << "[TIDE-MGR] ERROR:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    qDebug() << "[TIDE-MGR] Tide cell mapped successfully, cellId:" << m_cellId;
    m_cellLoaded = true;
    qDebug() << "[TIDE-MGR] Emitting tideDataLoaded signal";
    emit tideDataLoaded();
    return true;
}

void TideManager::setPredictionLocation(double latitude, double longitude, double searchRadius)
{
    qDebug() << "[TIDE-MGR] setPredictionLocation called with:" << latitude << longitude << "radius:" << searchRadius;

    m_latitude = latitude;
    m_longitude = longitude;
    m_searchRadius = searchRadius;

    if (!m_initialized || !m_cellLoaded) {
        qDebug() << "[TIDE-MGR] Cannot set location - initialized:" << m_initialized << "cellLoaded:" << m_cellLoaded;
        return;
    }

    // Free existing station context
    if (m_stationContext != NULL) {
        EcTidesStationFree(m_stationContext);
        m_stationContext = NULL;
    }

    // Initialize station context
    char stationName[64];
    EcCoordinate lat = latitude;
    EcCoordinate lon = longitude;

    qDebug() << "[TIDE-MGR] Initializing station context with coordinates:" << lat << lon;
    m_stationContext = EcTidesStationInitialize(m_predictionContext,
                                                m_cellId,
                                                m_dictInfo,
                                                &lat,
                                                &lon,
                                                searchRadius,
                                                stationName,
                                                sizeof(stationName));

    if (m_stationContext != NULL) {
        m_currentStationName = QString::fromLocal8Bit(stationName);
        qDebug() << "[TIDE-MGR] Station context initialized successfully, station:" << m_currentStationName;
        emit predictionsUpdated();
    } else {
        m_lastError = formatError(EcKernelGetLastError(EC_LAST));
        m_lastError = "EcTidesStationInitialize failed: " + m_lastError;
        qDebug() << "[TIDE-MGR] ERROR:" << m_lastError;
        emit errorOccurred(m_lastError);
    }
}

void TideManager::setPredictionLocationByName(const QString &stationName)
{
    if (!m_initialized || !m_cellLoaded) {
        return;
    }

    Bool found = False;
    EcFeature fhz;
    EcFindInfo fi;
    EcCoordinate lat, lon;
    char attrBuf[256];

    // Search for station by name
    QByteArray searchName = stationName.toLower().toLocal8Bit();

    fhz = EcQuerySpotCell(m_cellId, m_dictInfo, "T_HMON", "", '!', &fi, EC_FIRST, &lat, &lon);
    while (ECOK(fhz) && !found) {
        if (EcFeatureQueryAttribute(fhz, m_dictInfo, "OBJNAM", attrBuf, sizeof(attrBuf))) {
            // Convert object name to lower case for comparison
            QString objName = QString::fromLocal8Bit(&attrBuf[EC_LENATRCODE]).toLower();
            if (objName.contains(stationName.toLower())) {
                found = True;
                break;
            }
        }
        fhz = EcQuerySpotCell(m_cellId, m_dictInfo, "T_HMON", "", '!', &fi, EC_NEXT, &lat, &lon);
    }

    if (found) {
        setPredictionLocation(lat, lon, 0.1);
    } else {
        m_lastError = QString("Station '%1' not found").arg(stationName);
        emit errorOccurred(m_lastError);
    }
}

QList<TidePrediction> TideManager::getTidePredictions(const QDateTime &startTime,
                                                      const QDateTime &endTime,
                                                      bool highLowOnly) const
{
    QList<TidePrediction> predictions;

    qDebug() << "[TIDE-MGR] getTidePredictions called";
    qDebug() << "[TIDE-MGR] Start time:" << startTime.toString();
    qDebug() << "[TIDE-MGR] End time:" << endTime.toString();
    qDebug() << "[TIDE-MGR] High/Low only:" << highLowOnly;
    qDebug() << "[TIDE-MGR] Current station:" << m_currentStationName;
    qDebug() << "[TIDE-MGR] Location:" << m_latitude << m_longitude;

    // If using JSON data, delegate to JSON method
    if (m_usingJsonData) {
        qDebug() << "[TIDE-MGR] Using JSON data source";
        return getTidePredictionsFromJson(startTime, endTime);
    }

    if (!m_initialized || !m_cellLoaded || m_stationContext == NULL) {
        qDebug() << "[TIDE-MGR] Not initialized or no cell loaded";
        return predictions;
    }

    // Convert QDateTime to EcPredictionDate
    EcPredictionDate startDate, endDate;
    QDate qStartDate = startTime.date();
    QDate qEndDate = endTime.date();
    QTime qStartTime = startTime.time();
    QTime qEndTime = endTime.time();

    startDate.year = qStartDate.year();
    startDate.month = qStartDate.month();
    startDate.day = qStartDate.day();
    startDate.hour = qStartTime.hour();
    startDate.minute = qStartTime.minute();

    endDate.year = qEndDate.year();
    endDate.month = qEndDate.month();
    endDate.day = qEndDate.day();
    endDate.hour = qEndTime.hour();
    endDate.minute = qEndTime.minute();

    qDebug() << "[TIDE-MGR] Requesting predictions from"
             << startDate.day << startDate.month << startDate.year << startDate.hour << startDate.minute
             << "to"
             << endDate.day << endDate.month << endDate.year << endDate.hour << endDate.minute;

    EcTidesPredictionType pType = highLowOnly ? EC_PREDICT_HILO : EC_PREDICT_EQUI;
    double timeSpace = highLowOnly ? 180.0 : 60.0; // 3 hours for HILO, 1 hour for equidistant - get fewer predictions to test

    qDebug() << "[TIDE-MGR] Prediction type:" << (pType == EC_PREDICT_HILO ? "HILO" : "EQUI") << "timeSpace:" << timeSpace;

    EcTidesPredictionResult *pResult = NULL;
    int nPred = EcTidesPredict(m_stationContext, &startDate, &endDate, pType, timeSpace, &pResult);

    if (nPred > 0 && pResult != NULL) {
        qDebug() << "[TIDE-MGR] EcTidesPredict returned" << nPred << "predictions";

        // Process predictions following the exact pattern from original tidaltest
        // Print results exactly like tidaltest.cxx does
        qDebug() << "[TIDE-MGR] tidal prediction for '" << m_currentStationName << "' at" << m_latitude << m_longitude;
        qDebug() << "[TIDE-MGR] Array pointer address:" << (quintptr)pResult;

        // Try pointer arithmetic instead of array indexing
        EcTidesPredictionResult *currentResult = pResult;
        for (int i = 0; i < nPred; i++) {
            TidePrediction prediction;

            qDebug() << "[TIDE-MGR] Processing prediction" << i << "at address:" << (quintptr)currentResult;

            // Copy the exact printing format from tidaltest.cxx line 169-171
            qDebug() << "[TIDE-MGR] prediction" << i << ": ("
                     << QDate(currentResult->date.year, currentResult->date.month, currentResult->date.day).toString("dd.MM.yyyy")
                     << " at" << QTime(currentResult->date.hour, currentResult->date.minute).toString("hh:mm")
                     << ") height=" << QString("%1").arg(currentResult->height, 0, 'f', 2) << " m";

            // Validate the data before creating QDateTime
            if (currentResult->date.year >= 1900 && currentResult->date.year <= 2100 &&
                currentResult->date.month >= 1 && currentResult->date.month <= 12 &&
                currentResult->date.day >= 1 && currentResult->date.day <= 31 &&
                currentResult->date.hour >= 0 && currentResult->date.hour <= 23 &&
                currentResult->date.minute >= 0 && currentResult->date.minute <= 59) {

                QDate qDate(currentResult->date.year, currentResult->date.month, currentResult->date.day);
                QTime qTime(currentResult->date.hour, currentResult->date.minute);
                prediction.dateTime = QDateTime(qDate, qTime);
                prediction.height = currentResult->height;
                prediction.isHighTide = (i % 2 == 0) && highLowOnly;

                prediction.stationName = m_currentStationName;
                prediction.stationLocation = Coordinate(m_latitude, m_longitude);

                predictions.append(prediction);
                qDebug() << "[TIDE-MGR] Added valid prediction" << i;
            } else {
                qDebug() << "[TIDE-MGR] Skipping invalid prediction data";
            }

            // For now, only use the first prediction since it's valid
            // The stride issue needs more investigation
            if (i == 0) {
                currentResult = currentResult; // Stay at first valid prediction
            } else {
                // Don't advance to corrupted data - break the loop
                break;
            }
        }

        // Release the result array after all processing is done (like original tidaltest)
        EcFree(pResult);
        pResult = NULL;
    }

    return predictions;
}

TidePrediction TideManager::getCurrentTide() const
{
    TidePrediction current;

    QList<TidePrediction> predictions = getTodaysTides();
    if (predictions.isEmpty()) {
        return current;
    }

    QDateTime now = QDateTime::currentDateTime();

    // Find the closest prediction to current time
    int closestIndex = 0;
    qint64 minDiff = qAbs(predictions[0].dateTime.secsTo(now));

    for (int i = 1; i < predictions.size(); i++) {
        qint64 diff = qAbs(predictions[i].dateTime.secsTo(now));
        if (diff < minDiff) {
            minDiff = diff;
            closestIndex = i;
        }
    }

    current = predictions[closestIndex];

    // Interpolate height if not exactly at prediction time
    if (minDiff > 0 && closestIndex > 0 && closestIndex < predictions.size() - 1) {
        TidePrediction prev = predictions[closestIndex - 1];
        TidePrediction next = predictions[closestIndex + 1];

        qint64 totalSecs = next.dateTime.secsTo(prev.dateTime);
        qint64 secsToNow = prev.dateTime.secsTo(now);

        if (totalSecs != 0) {
            double ratio = (double)secsToNow / qAbs(totalSecs);
            current.height = prev.height + (next.height - prev.height) * ratio;
        }
    }

    return current;
}

QList<TidePrediction> TideManager::getTodaysTides() const
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime startOfDay = QDateTime(now.date(), QTime(0, 0));
    QDateTime endOfDay = QDateTime(now.date(), QTime(23, 59));

    return getTidePredictions(startOfDay, endOfDay, true);
}

QList<TideStation> TideManager::getAvailableStations() const
{
    QList<TideStation> stations;

    if (!m_initialized || !m_cellLoaded) {
        return stations;
    }

    EcFeature fhz;
    EcFindInfo fi;
    EcCoordinate lat, lon;
    char attrBuf[256];

    fhz = EcQuerySpotCell(m_cellId, m_dictInfo, "T_HMON", "OBJNAM", '!', &fi, EC_FIRST, &lat, &lon);
    while (ECOK(fhz)) {
        if (EcFeatureQueryAttribute(fhz, m_dictInfo, "OBJNAM", attrBuf, sizeof(attrBuf))) {
            TideStation station;
            station.name = QString::fromLocal8Bit(&attrBuf[EC_LENATRCODE]);
            station.location = Coordinate(lat, lon);
            stations.append(station);
        }
        fhz = EcQuerySpotCell(m_cellId, m_dictInfo, "T_HMON", "OBJNAM", '!', &fi, EC_NEXT, &lat, &lon);
    }

    return stations;
}

QString TideManager::getLastError() const
{
    return m_lastError;
}

void TideManager::cleanup()
{
    if (m_stationContext != NULL) {
        EcTidesStationFree(m_stationContext);
        m_stationContext = NULL;
    }

    if (m_predictionContext != NULL) {
        EcTidesFree(m_predictionContext);
        m_predictionContext = NULL;
    }

    if (m_cellLoaded && m_cellId != EC_NOCELLID) {
        EcCellUnmap(m_cellId);
        m_cellLoaded = false;
    }

    if (m_dictInfo != NULL) {
        EcDictionaryFree(m_dictInfo);
        m_dictInfo = NULL;
    }

    m_initialized = false;
    m_currentStationName.clear();
}

QString TideManager::formatError(unsigned long errorCode) const
{
    // Convert error code to hexadecimal string
    return QString("0x%1").arg(errorCode, 8, 16, QChar('0'));
}

QDateTime TideManager::predictionDateToQDateTime(const EcPredictionDate &date) const
{
    // Fix invalid date values
    int year = date.year;
    int month = date.month;
    int day = date.day;
    int hour = date.hour;
    int minute = date.minute;

    // Validate and fix date components
    if (year < 1900 || year > 2100) {
        qDebug() << "[TIDE-MGR] Invalid year:" << year << ", using current year";
        year = QDate::currentDate().year();
    }
    if (month < 1 || month > 12) {
        qDebug() << "[TIDE-MGR] Invalid month:" << month << ", using current month";
        month = QDate::currentDate().month();
    }
    if (day < 1 || day > 31) {
        qDebug() << "[TIDE-MGR] Invalid day:" << day << ", using current day";
        day = QDate::currentDate().day();
    }
    if (hour < 0 || hour > 23) {
        qDebug() << "[TIDE-MGR] Invalid hour:" << hour << ", using 0";
        hour = 0;
    }
    if (minute < 0 || minute > 59) {
        qDebug() << "[TIDE-MGR] Invalid minute:" << minute << ", using 0";
        minute = 0;
    }

    QDate qDate(year, month, day);
    QTime qTime(hour, minute, 0);
    QDateTime dateTime(qDate, qTime);

    if (!dateTime.isValid()) {
        qDebug() << "[TIDE-MGR] Invalid QDateTime created, using current date/time";
        dateTime = QDateTime::currentDateTime();
    }

    qDebug() << "[TIDE-MGR] Converted date:" << year << month << day << hour << minute
             << "->" << dateTime.toString();

    return dateTime;
}

// JSON data loading methods
bool TideManager::loadTideDataFromJson(const QString &jsonPath)
{
    qDebug() << "[TIDE-MGR] Loading tide data from JSON:" << jsonPath;

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QString("Cannot open JSON file: %1").arg(file.errorString());
        qDebug() << "[TIDE-MGR] ERROR:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        m_lastError = QString("JSON parse error at %1: %2")
                      .arg(parseError.offset)
                      .arg(parseError.errorString());
        qDebug() << "[TIDE-MGR] ERROR:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    parseJsonTideData(doc.object());

    // Set JSON mode
    m_usingJsonData = true;
    m_initialized = true;

    qDebug() << "[TIDE-MGR] JSON tide data loaded successfully";
    emit tideDataLoaded();

    return true;
}

void TideManager::parseJsonTideData(const QJsonObject &jsonData)
{
    qDebug() << "[TIDE-MGR] Parsing JSON tide data";

    m_jsonTideData = jsonData;
    m_jsonStations.clear();

    // Parse tide stations
    if (jsonData.contains("tide_stations")) {
        QJsonArray stations = jsonData["tide_stations"].toArray();
        qDebug() << "[TIDE-MGR] Found" << stations.size() << "tide stations";

        for (int i = 0; i < stations.size(); i++) {
            QJsonObject stationObj = stations[i].toObject();
            TideStation station;

            station.id = stationObj["id"].toString();
            station.name = stationObj["name"].toString();
            station.location.latitude = stationObj["latitude"].toDouble();
            station.location.longitude = stationObj["longitude"].toDouble();
            station.description = stationObj["description"].toString();

            m_jsonStations.append(station);
            qDebug() << "[TIDE-MGR] Added station:" << station.name << "ID:" << station.id;
        }
    }

    qDebug() << "[TIDE-MGR] JSON parsing completed, loaded" << m_jsonStations.size() << "stations";
}

bool TideManager::loadJsonTideStations()
{
    return !m_jsonStations.isEmpty();
}

QList<TidePrediction> TideManager::getTidePredictionsFromJson(const QDateTime &startTime,
                                                             const QDateTime &endTime) const
{
    QList<TidePrediction> predictions;

    if (!m_usingJsonData || m_currentStationId.isEmpty()) {
        qDebug() << "[TIDE-MGR] Cannot get JSON predictions: not using JSON data or no station selected";
        return predictions;
    }

    qDebug() << "[TIDE-MGR] Getting JSON predictions for station:" << m_currentStationId
             << "from" << startTime.toString() << "to" << endTime.toString();

    // Find predictions for current station and date range
    if (m_jsonTideData.contains("tide_predictions")) {
        QJsonArray allPredictions = m_jsonTideData["tide_predictions"].toArray();

        for (int i = 0; i < allPredictions.size(); i++) {
            QJsonObject predictionObj = allPredictions[i].toObject();

            if (predictionObj["station_id"].toString() == m_currentStationId) {
                QString dateStr = predictionObj["date"].toString();
                QDate predDate = QDate::fromString(dateStr, "yyyy-MM-dd");

                // Check if date is within our range
                if (predDate >= startTime.date() && predDate <= endTime.date()) {
                    QJsonArray dailyPredictions = predictionObj["predictions"].toArray();

                    for (int j = 0; j < dailyPredictions.size(); j++) {
                        QJsonObject dailyPred = dailyPredictions[j].toObject();
                        TidePrediction prediction;

                        QString timeStr = dailyPred["time"].toString();
                        QTime predTime = QTime::fromString(timeStr, "hh:mm");
                        QDateTime predDateTime(predDate, predTime);

                        prediction.dateTime = predDateTime;
                        prediction.height = dailyPred["height"].toDouble();
                        prediction.isHighTide = dailyPred["type"].toString() == "high";
                        prediction.stationName = m_currentStationName;

                        // Find station coordinates
                        for (const TideStation &station : m_jsonStations) {
                            if (station.id == m_currentStationId) {
                                prediction.stationLocation = station.location;
                                break;
                            }
                        }

                        predictions.append(prediction);
                        qDebug() << "[TIDE-MGR] Added prediction:"
                                << (prediction.isHighTide ? "HIGH" : "LOW")
                                << prediction.dateTime.toString()
                                << "Height:" << prediction.height << "m";
                    }
                }
            }
        }
    }

    qDebug() << "[TIDE-MGR] JSON predictions loaded:" << predictions.size() << "predictions";
    return predictions;
}

QList<TideStation> TideManager::getJsonStations() const
{
    return m_jsonStations;
}

void TideManager::setPredictionLocationById(const QString &stationId)
{
    qDebug() << "[TIDE-MGR] Setting prediction location by station ID:" << stationId;

    if (!m_usingJsonData) {
        qDebug() << "[TIDE-MGR] Not using JSON data, cannot set location by ID";
        return;
    }

    for (const TideStation &station : m_jsonStations) {
        if (station.id == stationId) {
            m_currentStationId = station.id;
            m_currentStationName = station.name;
            m_latitude = station.location.latitude;
            m_longitude = station.location.longitude;

            qDebug() << "[TIDE-MGR] Set location to station:" << station.name
                    << "at" << m_latitude << m_longitude;
            emit predictionsUpdated();
            return;
        }
    }

    qDebug() << "[TIDE-MGR] Station ID not found:" << stationId;
}

void TideManager::drawStationMarkers(void *view, void *dictInfo, const QString &cellPath)
{
    qDebug() << "[TIDE-MGR] Drawing station markers on chart";

    if (!m_usingJsonData && m_jsonStations.isEmpty()) {
        qDebug() << "[TIDE-MGR] No JSON stations to draw";
        return;
    }

    // Get available stations
    QList<TideStation> stations = getJsonStations();
    if (stations.isEmpty()) {
        qDebug() << "[TIDE-MGR] No stations available for drawing";
        return;
    }

    qDebug() << "[TIDE-MGR] Drawing" << stations.size() << "station markers";

    // For now, we'll create a simple text-based marker display
    // In a full implementation, this would use EC2007 drawing functions
    for (const TideStation &station : stations) {
        qDebug() << "[TIDE-MGR] Station marker:" << station.name
                << "at" << station.location.latitude << station.location.longitude;

        // This would be where EC2007 drawing functions are called:
        // EcDrawSetSymbolSize(view, EC_POINTSYMBOL, 10);
        // EcDrawSymbol(view, x, y, stationSymbolId);
        // EcDrawText(view, station.name, x, y + 10);
    }
}

TideStation TideManager::getStationAtPosition(EcCoordinate latitude, EcCoordinate longitude, double tolerance) const
{
    qDebug() << "[TIDE-MGR] Looking for station at position:" << latitude << longitude;

    if (!m_usingJsonData || m_jsonStations.isEmpty()) {
        return TideStation(); // Return empty station
    }

    for (const TideStation &station : m_jsonStations) {
        double distance = sqrt(pow(station.location.latitude - latitude, 2) + pow(station.location.longitude - longitude, 2));

        if (distance <= tolerance) {
            qDebug() << "[TIDE-MGR] Found station:" << station.name << "distance:" << distance;
            return station;
        }
    }

    qDebug() << "[TIDE-MGR] No station found at position";
    return TideStation(); // Return empty station
}