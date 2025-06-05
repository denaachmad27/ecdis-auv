// alertsystem.h - VERSI MINIMAL YANG BEKERJA

#ifndef ALERTSYSTEM_H
#define ALERTSYSTEM_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QColor>
#include <QString>
#include <QMap>
#include <QList>
#include <QDebug>
#include <QApplication>

// Forward declarations
class EcWidget;

// Simple enums - GANTI NAMA UNTUK HINDARI KONFLIK WINDOWS
enum AlertType {
    ALERT_GUARDZONE_PROXIMITY = 1,
    ALERT_DEPTH_SHALLOW = 2,
    ALERT_DEPTH_DEEP = 3,
    ALERT_COLLISION_RISK = 4,
    ALERT_NAVIGATION_WARNING = 5,
    ALERT_SYSTEM_ERR = 6,        // GANTI DARI ALERT_SYSTEM_ERROR
    ALERT_USER_DEFINED = 7
};

enum AlertPriority {
    PRIORITY_LOW = 1,
    PRIORITY_MEDIUM = 2,
    PRIORITY_HIGH = 3,
    PRIORITY_CRITICAL = 4
};

enum AlertState {
    STATE_ACTIVE = 1,
    STATE_ACKNOWLEDGED = 2,
    STATE_RESOLVED = 3,
    STATE_SILENCED = 4
};

// Simple struct
struct AlertData {
    int id;
    AlertType type;
    AlertPriority priority;
    AlertState state;
    QDateTime timestamp;
    QString title;
    QString message;
    QString source;
    double latitude;
    double longitude;
    bool requiresAcknowledgment;
    QColor visualColor;
};

class AlertSystem : public QObject
{
    Q_OBJECT

public:
    explicit AlertSystem(EcWidget* ecWidget, QObject *parent = nullptr);
    ~AlertSystem();

    // Basic alert management
    int triggerAlert(AlertType type, AlertPriority priority,
                     const QString& title, const QString& message,
                     const QString& source = "",
                     double lat = 0.0, double lon = 0.0);

    bool acknowledgeAlert(int alertId);
    bool resolveAlert(int alertId);
    void clearAllAlerts();

    QList<AlertData> getActiveAlerts() const;
    bool hasActiveAlerts() const;

    // Configuration
    void updateOwnShipPosition(double lat, double lon, double depth = 0.0);
    void setSystemEnabled(bool enabled) { m_systemEnabled = enabled; }
    void setDepthMonitoringEnabled(bool enabled) { m_depthMonitoringEnabled = enabled; }
    void setProximityMonitoringEnabled(bool enabled) { m_proximityMonitoringEnabled = enabled; }
    void setMinimumDepth(double meters) { m_minimumDepth = meters; }
    void setProximityThreshold(double nauticalMiles) { m_proximityThreshold = nauticalMiles; }

signals:
    void alertTriggered(const AlertData& alert);
    void criticalAlert(const AlertData& alert);
    void systemStatusChanged(bool enabled);

private slots:
    void checkNavigationAlerts();

private:
    EcWidget* m_ecWidget;
    bool m_systemEnabled;
    bool m_depthMonitoringEnabled;
    bool m_proximityMonitoringEnabled;
    double m_minimumDepth;
    double m_proximityThreshold;

    QMap<int, AlertData> m_activeAlerts;
    int m_nextAlertId;
    QTimer* m_monitoringTimer;

    double m_currentLat;
    double m_currentLon;
    double m_currentDepth;

    void playSimpleAlert(AlertPriority priority);
    void logAlert(const AlertData& alert);
};

#endif // ALERTSYSTEM_H
