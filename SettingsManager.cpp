#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "appconfig.h"
#include <QSettings>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>

SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

void SettingsManager::load() {
    // Resolve config path with robust APPDATA fallback
    QString baseAppData;
    const char* appDataEnv = EcKernelGetEnv("APPDATA");
    if (appDataEnv && *appDataEnv) {
        baseAppData = QString::fromLocal8Bit(appDataEnv);
    } else {
        baseAppData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (baseAppData.isEmpty()) {
            baseAppData = QCoreApplication::applicationDirPath();
        }
    }
    QDir baseDir(baseAppData);
    QString dirPath = baseDir.filePath("SevenCs/EC2007/DENC");
    QDir().mkpath(dirPath);
    QString configPath = QDir(dirPath).filePath("config.ini");
    QSettings settings(configPath, QSettings::IniFormat);

    // MOOSDB
    m_data.moosIp = settings.value("MOOSDB/ip", "127.0.0.1").toString();
    m_data.moosPort = settings.value("MOOSDB/port", "9000").toString();

    // AIS
    if (AppConfig::isDevelopment()){
        m_data.aisSource = settings.value("AIS/source", "log").toString();
        m_data.aisIp = settings.value("AIS/ip", "").toString();
        m_data.aisLogFile = settings.value("AIS/log_file", "").toString();
    }

    SettingsDialog *dialogObj = new SettingsDialog();

    // DISPLAY
    m_data.displayMode = settings.value("Display/mode", "Day").toString();
    m_data.themeMode = dialogObj->theme(settings.value("Display/theme", "Dark").toString());

    // DISPLAY
    m_data.chartMode = settings.value("Display/move", "Drag").toString();

    // CHART MANAGER - ISDT Expiration
    m_data.isdtExpirationDays = settings.value("ChartManager/isdt_expiration_days", 7).toInt();
    
    // GUARDZON
    m_data.defaultShipTypeFilter = settings.value("GuardZone/default_ship_type", 0).toInt();
    m_data.defaultAlertDirection = settings.value("GuardZone/default_alert_direction", 0).toInt();

    // OWN SHIP

    m_data.orientationMode = dialogObj->orientation(settings.value("OwnShip/orientation", "NorthUp").toString());
    m_data.centeringMode = dialogObj->centering(settings.value("OwnShip/centering", "AutoRecenter").toString());
    m_data.courseUpHeading = settings.value("OwnShip/course_heading", 0).toInt();

    m_data.trailMode = settings.value("OwnShip/mode", 2).toInt();
    m_data.trailMinute = settings.value("OwnShip/interval", 1).toInt();
    m_data.trailDistance = settings.value("OwnShip/distance", 0.01).toDouble();

    m_data.latViewMode = settings.value("OwnShip/lat_view", "NAV_LAT").toString();
    m_data.longViewMode = settings.value("OwnShip/long_view", "NAV_LONG").toString();

    if(AppConfig::isDevelopment()){
        // NAVIGATION SAFETY
        m_data.shipDraftMeters = settings.value("OwnShip/ship_draft", 2.5).toDouble();
        m_data.ukcDangerMeters = settings.value("OwnShip/ukc_danger", 0.5).toDouble();
        m_data.ukcWarningMeters = settings.value("OwnShip/ukc_warning", 2.0).toDouble();
    }

    // CPA/TCPA
    m_data.cpaThreshold = settings.value("CPA-TCPA/cpa_threshold", 0.2).toDouble();
    m_data.tcpaThreshold = settings.value("CPA-TCPA/tcpa_threshold", 1).toDouble();

    // Collision Risk
    m_data.enableCollisionRisk = settings.value("CollisionRisk/enabled", false).toBool();
    m_data.criticalRiskDistance = settings.value("CollisionRisk/critical_distance_nm", 0.1).toDouble();
    m_data.highRiskDistance = settings.value("CollisionRisk/high_distance_nm", 0.25).toDouble();
    m_data.criticalRiskTime = settings.value("CollisionRisk/critical_time_min", 2.0).toDouble();

    // Ship Dimensions
    m_data.shipLength = settings.value("ShipDimensions/length", 170.0).toDouble();
    m_data.shipBeam = settings.value("ShipDimensions/beam", 13.0).toDouble();
    m_data.shipHeight = settings.value("ShipDimensions/height", 25.0).toDouble();
    m_data.primaryGpsIndex = settings.value("ShipDimensions/primaryGpsIndex", 0).toInt();

    // GPS Positions
    m_data.gpsPositions.clear();
    int gpsCount = settings.beginReadArray("GPSPositions");
    for (int i = 0; i < gpsCount; ++i) {
        settings.setArrayIndex(i);
        GpsPosition pos;
        pos.name = settings.value("name", QString("GPS %1").arg(i + 1)).toString();
        pos.offsetX = settings.value("offsetX", 0.0).toDouble();
        pos.offsetY = settings.value("offsetY", 0.0).toDouble();
        m_data.gpsPositions.append(pos);
    }
    settings.endArray();

    // NOTE: removed duplicate ShipDimensions and GPSPositions read to avoid
    // mismatched beginReadArray/endArray and inconsistent state
}

void SettingsManager::save(const SettingsData& data) {
    m_data = data;

    // Resolve config path with robust APPDATA fallback
    QString baseAppData;
    const char* appDataEnv = EcKernelGetEnv("APPDATA");
    if (appDataEnv && *appDataEnv) {
        baseAppData = QString::fromLocal8Bit(appDataEnv);
    } else {
        baseAppData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (baseAppData.isEmpty()) {
            baseAppData = QCoreApplication::applicationDirPath();
        }
    }
    QDir baseDir(baseAppData);
    QString dirPath = baseDir.filePath("SevenCs/EC2007/DENC");
    QDir().mkpath(dirPath);
    QString configPath = QDir(dirPath).filePath("config.ini");
    QSettings settings(configPath, QSettings::IniFormat);

    // MOOSDB
    settings.setValue("MOOSDB/ip", data.moosIp);
    settings.setValue("MOOSDB/port", data.moosPort);

    // AIS
    if (AppConfig::isDevelopment()){
        settings.setValue("AIS/source", data.aisSource);
        settings.setValue("AIS/ip", data.aisIp);
        settings.setValue("AIS/log_file", data.aisLogFile);
    }

    // DISPLAY
    settings.setValue("Display/mode", data.displayMode);
    settings.setValue("Display/theme", static_cast<int>(data.themeMode));

    // CHART
    settings.setValue("Display/move", data.chartMode);

    // CHART MANAGER - ISDT Expiration
    settings.setValue("ChartManager/isdt_expiration_days", data.isdtExpirationDays);

    // GUARDZONE
    settings.setValue("GuardZone/default_ship_type", data.defaultShipTypeFilter);
    settings.setValue("GuardZone/default_alert_direction", data.defaultAlertDirection);

    // OWN SHIP
    settings.setValue("OwnShip/orientation", data.orientationMode);
    settings.setValue("OwnShip/centering", data.centeringMode);

    // if (data.orientationMode == EcWidget::CourseUp) {
    //     settings.setValue("OwnShip/course_heading", data.courseUpHeading);
    // }

    settings.setValue("OwnShip/course_heading", data.courseUpHeading);
    settings.setValue("OwnShip/mode", data.trailMode);
    settings.setValue("OwnShip/interval", data.trailMinute);
    settings.setValue("OwnShip/distance", data.trailDistance);

    settings.setValue("OwnShip/lat_view", data.latViewMode);
    settings.setValue("OwnShip/long_view", data.longViewMode);

    if(AppConfig::isDevelopment()){
        // NAVIGATION SAFETY
        settings.setValue("OwnShip/ship_draft", data.shipDraftMeters);
        settings.setValue("OwnShip/ukc_danger", data.ukcDangerMeters);
        settings.setValue("OwnShip/ukc_warning", data.ukcWarningMeters);
    }

    // CPA/TCPA
    settings.setValue("CPA-TCPA/cpa_threshold", data.cpaThreshold);
    settings.setValue("CPA-TCPA/tcpa_threshold", data.tcpaThreshold);

    // Collision Risk
    settings.setValue("CollisionRisk/enabled", data.enableCollisionRisk);
    settings.setValue("CollisionRisk/critical_distance_nm", data.criticalRiskDistance);
    settings.setValue("CollisionRisk/high_distance_nm", data.highRiskDistance);
    settings.setValue("CollisionRisk/critical_time_min", data.criticalRiskTime);

    // Ship Dimensions
    settings.setValue("ShipDimensions/length", data.shipLength);
    settings.setValue("ShipDimensions/beam", data.shipBeam);
    settings.setValue("ShipDimensions/height", data.shipHeight);
    settings.setValue("ShipDimensions/primaryGpsIndex", data.primaryGpsIndex);

    // GPS Positions
    settings.beginWriteArray("GPSPositions");
    for (int i = 0; i < data.gpsPositions.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("name", data.gpsPositions.at(i).name);
        settings.setValue("offsetX", data.gpsPositions.at(i).offsetX);
        settings.setValue("offsetY", data.gpsPositions.at(i).offsetY);
    }
    settings.endArray();
}

const SettingsData& SettingsManager::data() const {
    return m_data;
}
