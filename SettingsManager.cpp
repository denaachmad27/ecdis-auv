#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "appconfig.h"
#include <QSettings>
#include <QCoreApplication>

SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

void SettingsManager::load() {
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
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

    // DISPLAY
    m_data.displayMode = settings.value("Display/mode", "Day").toString();
    
    // GUARDZON
    m_data.defaultShipTypeFilter = settings.value("GuardZone/default_ship_type", 0).toInt();
    m_data.defaultAlertDirection = settings.value("GuardZone/default_alert_direction", 0).toInt();

    // OWN SHIP
    SettingsDialog *dialogObj = new SettingsDialog();

    m_data.orientationMode = dialogObj->orientation(settings.value("OwnShip/orientation", "NorthUp").toString());
    m_data.centeringMode = dialogObj->centering(settings.value("OwnShip/centering", "AutoRecenter").toString());
    m_data.courseUpHeading = settings.value("OwnShip/course_heading", 0).toInt();

    m_data.trailMode = settings.value("OwnShip/mode", 2).toInt();
    m_data.trailMinute = settings.value("OwnShip/interval", 1).toInt();
    m_data.trailDistance = settings.value("OwnShip/distance", 0.01).toDouble();

    if(AppConfig::isDevelopment()){
        // NAVIGATION SAFETY
        m_data.shipDraftMeters = settings.value("OwnShip/ship_draft", 2.5).toDouble();
        m_data.ukcDangerMeters = settings.value("OwnShip/ukc_danger", 0.5).toDouble();
        m_data.ukcWarningMeters = settings.value("OwnShip/ukc_warning", 2.0).toDouble();
    }
}

void SettingsManager::save(const SettingsData& data) {
    m_data = data;

    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
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

    // GUARDZONE
    settings.setValue("GuardZone/default_ship_type", data.defaultShipTypeFilter);
    settings.setValue("GuardZone/default_alert_direction", data.defaultAlertDirection);

    // OWN SHIP
    settings.setValue("OwnShip/orientation", data.orientationMode);
    settings.setValue("OwnShip/centering", data.centeringMode);

    if (data.orientationMode == EcWidget::CourseUp) {
        settings.setValue("OwnShip/course_heading", data.courseUpHeading);
    }

    settings.setValue("OwnShip/mode", data.trailMode);
    settings.setValue("OwnShip/interval", data.trailMinute);
    settings.setValue("OwnShip/distance", data.trailDistance);

    if(AppConfig::isDevelopment()){
        // NAVIGATION SAFETY
        settings.setValue("OwnShip/ship_draft", data.shipDraftMeters);
        settings.setValue("OwnShip/ukc_danger", data.ukcDangerMeters);
        settings.setValue("OwnShip/ukc_warning", data.ukcWarningMeters);
    }
}

const SettingsData& SettingsManager::data() const {
    return m_data;
}
