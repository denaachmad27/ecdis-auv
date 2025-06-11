#ifndef CPATCPASETTINGS_H
#define CPATCPASETTINGS_H

#include <QObject>
#include <QSettings>

class CPATCPASettings : public QObject
{
    Q_OBJECT

public:
    static CPATCPASettings& instance();

    // Getter methods
    double getCPAThreshold() const { return m_cpaThreshold; }
    double getTCPAThreshold() const { return m_tcpaThreshold; }
    bool isCPAAlarmEnabled() const { return m_cpaAlarmEnabled; }
    bool isTCPAAlarmEnabled() const { return m_tcpaAlarmEnabled; }
    bool isVisualAlarmEnabled() const { return m_visualAlarmEnabled; }
    bool isAudioAlarmEnabled() const { return m_audioAlarmEnabled; }
    int getAlarmUpdateInterval() const { return m_updateInterval; }

    // Setter methods
    void setCPAThreshold(double threshold);
    void setTCPAThreshold(double threshold);
    void setCPAAlarmEnabled(bool enabled);
    void setTCPAAlarmEnabled(bool enabled);
    void setVisualAlarmEnabled(bool enabled);
    void setAudioAlarmEnabled(bool enabled);
    void setAlarmUpdateInterval(int interval);

    // Load/Save methods
    void loadSettings();
    void saveSettings();
    void resetToDefaults();

signals:
    void settingsChanged();

private:
    explicit CPATCPASettings(QObject *parent = nullptr);

    // Default values
    static const double DEFAULT_CPA_THRESHOLD;
    static const double DEFAULT_TCPA_THRESHOLD;
    static const bool DEFAULT_CPA_ALARM_ENABLED;
    static const bool DEFAULT_TCPA_ALARM_ENABLED;
    static const bool DEFAULT_VISUAL_ALARM_ENABLED;
    static const bool DEFAULT_AUDIO_ALARM_ENABLED;
    static const int DEFAULT_UPDATE_INTERVAL;

    // Settings values
    double m_cpaThreshold;
    double m_tcpaThreshold;
    bool m_cpaAlarmEnabled;
    bool m_tcpaAlarmEnabled;
    bool m_visualAlarmEnabled;
    bool m_audioAlarmEnabled;
    int m_updateInterval;

    QSettings *m_settings;
};

#endif // CPATCPASETTINGS_H
