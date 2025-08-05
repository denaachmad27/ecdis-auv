#include "cpatcpasettings.h"
#include <QDebug>

// Default values
const double CPATCPASettings::DEFAULT_CPA_THRESHOLD = 0.2;
const double CPATCPASettings::DEFAULT_TCPA_THRESHOLD = 1.0;
const bool CPATCPASettings::DEFAULT_CPA_ALARM_ENABLED = true;
const bool CPATCPASettings::DEFAULT_TCPA_ALARM_ENABLED = true;
const bool CPATCPASettings::DEFAULT_VISUAL_ALARM_ENABLED = true;
const bool CPATCPASettings::DEFAULT_AUDIO_ALARM_ENABLED = false;
const int CPATCPASettings::DEFAULT_UPDATE_INTERVAL = 500;

CPATCPASettings::CPATCPASettings(QObject *parent)
    : QObject(parent)
{
    m_settings = new QSettings("Solusi247", "ECDIS", this);
    resetToDefaults();
    loadSettings();
}

CPATCPASettings& CPATCPASettings::instance()
{
    static CPATCPASettings instance;
    return instance;
}

void CPATCPASettings::setCPAThreshold(double threshold)
{
    if (m_cpaThreshold != threshold) {
        m_cpaThreshold = threshold;
        emit settingsChanged();
    }
}

void CPATCPASettings::setTCPAThreshold(double threshold)
{
    if (m_tcpaThreshold != threshold) {
        m_tcpaThreshold = threshold;
        emit settingsChanged();
    }
}

void CPATCPASettings::setCPAAlarmEnabled(bool enabled)
{
    if (m_cpaAlarmEnabled != enabled) {
        m_cpaAlarmEnabled = enabled;
        emit settingsChanged();
    }
}

void CPATCPASettings::setTCPAAlarmEnabled(bool enabled)
{
    if (m_tcpaAlarmEnabled != enabled) {
        m_tcpaAlarmEnabled = enabled;
        emit settingsChanged();
    }
}

void CPATCPASettings::setVisualAlarmEnabled(bool enabled)
{
    if (m_visualAlarmEnabled != enabled) {
        m_visualAlarmEnabled = enabled;
        emit settingsChanged();
    }
}

void CPATCPASettings::setAudioAlarmEnabled(bool enabled)
{
    if (m_audioAlarmEnabled != enabled) {
        m_audioAlarmEnabled = enabled;
        emit settingsChanged();
    }
}

void CPATCPASettings::setAlarmUpdateInterval(int interval)
{
    if (m_updateInterval != interval) {
        m_updateInterval = interval;
        emit settingsChanged();
    }
}

void CPATCPASettings::loadSettings()
{
    qDebug() << "Loading CPA/TCPA settings...";

    m_settings->beginGroup("CPATCPA");

    m_cpaThreshold = m_settings->value("CPAThreshold", DEFAULT_CPA_THRESHOLD).toDouble();
    m_tcpaThreshold = m_settings->value("TCPAThreshold", DEFAULT_TCPA_THRESHOLD).toDouble();
    m_cpaAlarmEnabled = m_settings->value("CPAAlarmEnabled", DEFAULT_CPA_ALARM_ENABLED).toBool();
    m_tcpaAlarmEnabled = m_settings->value("TCPAAlarmEnabled", DEFAULT_TCPA_ALARM_ENABLED).toBool();
    m_visualAlarmEnabled = m_settings->value("VisualAlarmEnabled", DEFAULT_VISUAL_ALARM_ENABLED).toBool();
    m_audioAlarmEnabled = m_settings->value("AudioAlarmEnabled", DEFAULT_AUDIO_ALARM_ENABLED).toBool();
    m_updateInterval = m_settings->value("UpdateInterval", DEFAULT_UPDATE_INTERVAL).toInt();

    m_settings->endGroup();

    qDebug() << "Settings loaded - CPA:" << m_cpaThreshold << "TCPA:" << m_tcpaThreshold;
}

void CPATCPASettings::saveSettings()
{
    qDebug() << "Saving CPA/TCPA settings...";

    m_settings->beginGroup("CPATCPA");

    m_settings->setValue("CPAThreshold", m_cpaThreshold);
    m_settings->setValue("TCPAThreshold", m_tcpaThreshold);
    m_settings->setValue("CPAAlarmEnabled", m_cpaAlarmEnabled);
    m_settings->setValue("TCPAAlarmEnabled", m_tcpaAlarmEnabled);
    m_settings->setValue("VisualAlarmEnabled", m_visualAlarmEnabled);
    m_settings->setValue("AudioAlarmEnabled", m_audioAlarmEnabled);
    m_settings->setValue("UpdateInterval", m_updateInterval);

    m_settings->endGroup();
    m_settings->sync();

    qDebug() << "Settings saved successfully";
}

void CPATCPASettings::resetToDefaults()
{
    m_cpaThreshold = DEFAULT_CPA_THRESHOLD;
    m_tcpaThreshold = DEFAULT_TCPA_THRESHOLD;
    m_cpaAlarmEnabled = DEFAULT_CPA_ALARM_ENABLED;
    m_tcpaAlarmEnabled = DEFAULT_TCPA_ALARM_ENABLED;
    m_visualAlarmEnabled = DEFAULT_VISUAL_ALARM_ENABLED;
    m_audioAlarmEnabled = DEFAULT_AUDIO_ALARM_ENABLED;
    m_updateInterval = DEFAULT_UPDATE_INTERVAL;

    emit settingsChanged();
}
