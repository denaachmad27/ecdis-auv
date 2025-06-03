#include "SettingsManager.h"
#include <QSettings>

SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

void SettingsManager::load() {
    QSettings settings("config.ini", QSettings::IniFormat);

    m_data.moosIp = settings.value("MOOSDB/ip", "127.0.0.1").toString();
    m_data.moosPort = settings.value("MOOSDB/port", "9000").toString();

    m_data.aisSource = settings.value("AIS/source", "log").toString();
    m_data.aisIp = settings.value("AIS/ip", "").toString();
    m_data.aisLogFile = settings.value("AIS/log_file", "").toString();

    m_data.displayMode = settings.value("Display/mode", "Day").toString();
}

void SettingsManager::save(const SettingsData& data) {
    m_data = data;

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.setValue("MOOSDB/ip", data.moosIp);
    settings.setValue("MOOSDB/port", data.moosPort);
    settings.setValue("AIS/source", data.aisSource);
    settings.setValue("AIS/ip", data.aisIp);
    settings.setValue("AIS/log_file", data.aisLogFile);
    settings.setValue("Display/mode", data.displayMode);
}

const SettingsData& SettingsManager::data() const {
    return m_data;
}
