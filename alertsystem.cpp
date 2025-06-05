// alertsystem.cpp - IMPLEMENTASI MINIMAL

#include "alertsystem.h"
#include "ecwidget.h"

AlertSystem::AlertSystem(EcWidget* ecWidget, QObject *parent)
    : QObject(parent)
    , m_ecWidget(ecWidget)
    , m_systemEnabled(true)
    , m_depthMonitoringEnabled(true)
    , m_proximityMonitoringEnabled(true)
    , m_minimumDepth(5.0)
    , m_proximityThreshold(0.5)
    , m_nextAlertId(1)
    , m_currentLat(0.0)
    , m_currentLon(0.0)
    , m_currentDepth(0.0)
{
    qDebug() << "[ALERT] Initializing Alert System";

    // Initialize monitoring timer
    m_monitoringTimer = new QTimer(this);
    connect(m_monitoringTimer, &QTimer::timeout, this, &AlertSystem::checkNavigationAlerts);
    m_monitoringTimer->start(5000); // Check every 5 seconds

    qDebug() << "[ALERT] Alert System initialized successfully";
}

AlertSystem::~AlertSystem()
{
    qDebug() << "[ALERT] Shutting down Alert System";

    if (m_monitoringTimer) {
        m_monitoringTimer->stop();
    }
}

int AlertSystem::triggerAlert(AlertType type, AlertPriority priority,
                              const QString& title, const QString& message,
                              const QString& source,
                              double lat, double lon)
{
    if (!m_systemEnabled) {
        qDebug() << "[ALERT] System disabled, ignoring alert:" << title;
        return -1;
    }

    qDebug() << "[ALERT] Triggering alert:" << title;

    // Create new alert
    AlertData alert;
    alert.id = m_nextAlertId++;
    alert.type = type;
    alert.priority = priority;
    alert.state = STATE_ACTIVE;
    alert.timestamp = QDateTime::currentDateTime();
    alert.title = title;
    alert.message = message;
    alert.source = source;
    alert.latitude = lat;
    alert.longitude = lon;
    alert.requiresAcknowledgment = (priority >= PRIORITY_HIGH);

    // Set color based on priority
    switch (priority) {
    case PRIORITY_LOW: alert.visualColor = QColor(0, 150, 255); break;      // Blue
    case PRIORITY_MEDIUM: alert.visualColor = QColor(255, 165, 0); break;   // Orange
    case PRIORITY_HIGH: alert.visualColor = QColor(255, 69, 0); break;      // Red-Orange
    case PRIORITY_CRITICAL: alert.visualColor = QColor(255, 0, 0); break;   // Red
    }

    // Add to active alerts
    m_activeAlerts[alert.id] = alert;

    // Log the alert
    logAlert(alert);

    // Emit signals
    emit alertTriggered(alert);

    if (priority == PRIORITY_CRITICAL) {
        emit criticalAlert(alert);
    }

    // Play sound
    playSimpleAlert(priority);

    qDebug() << "[ALERT] Alert triggered successfully, ID:" << alert.id;
    return alert.id;
}

bool AlertSystem::acknowledgeAlert(int alertId)
{
    if (!m_activeAlerts.contains(alertId)) {
        qWarning() << "[ALERT] Cannot acknowledge alert - ID not found:" << alertId;
        return false;
    }

    AlertData& alert = m_activeAlerts[alertId];
    alert.state = STATE_ACKNOWLEDGED;

    qDebug() << "[ALERT] Alert acknowledged:" << alertId;
    return true;
}

bool AlertSystem::resolveAlert(int alertId)
{
    if (!m_activeAlerts.contains(alertId)) {
        qWarning() << "[ALERT] Cannot resolve alert - ID not found:" << alertId;
        return false;
    }

    AlertData& alert = m_activeAlerts[alertId];
    alert.state = STATE_RESOLVED;

    // Remove from active alerts
    m_activeAlerts.remove(alertId);

    qDebug() << "[ALERT] Alert resolved:" << alertId;
    return true;
}

void AlertSystem::clearAllAlerts()
{
    qDebug() << "[ALERT] Clearing all active alerts";
    m_activeAlerts.clear();
}

QList<AlertData> AlertSystem::getActiveAlerts() const
{
    QList<AlertData> alerts;
    for (auto it = m_activeAlerts.begin(); it != m_activeAlerts.end(); ++it) {
        alerts.append(it.value());
    }
    return alerts;
}

bool AlertSystem::hasActiveAlerts() const
{
    return !m_activeAlerts.isEmpty();
}

void AlertSystem::updateOwnShipPosition(double lat, double lon, double depth)
{
    m_currentLat = lat;
    m_currentLon = lon;
    m_currentDepth = depth;

    // Trigger immediate checks if position changed significantly
    if (m_depthMonitoringEnabled || m_proximityMonitoringEnabled) {
        checkNavigationAlerts();
    }
}

void AlertSystem::checkNavigationAlerts()
{
    if (!m_systemEnabled) {
        return;
    }

    // Check depth alerts
    if (m_depthMonitoringEnabled && m_currentDepth > 0.0) {
        if (m_currentDepth < m_minimumDepth) {
            // Check if we already have an active shallow depth alert
            bool hasActiveShallowAlert = false;
            for (auto it = m_activeAlerts.begin(); it != m_activeAlerts.end(); ++it) {
                if (it.value().type == ALERT_DEPTH_SHALLOW &&
                    it.value().state == STATE_ACTIVE) {
                    hasActiveShallowAlert = true;
                    break;
                }
            }

            if (!hasActiveShallowAlert) {
                triggerAlert(ALERT_DEPTH_SHALLOW, PRIORITY_CRITICAL,
                             tr("Shallow Water Alert"),
                             tr("Current depth %.1f m is below minimum safe depth %.1f m")
                                 .arg(m_currentDepth).arg(m_minimumDepth),
                             "Depth_Monitor",
                             m_currentLat, m_currentLon);
            }
        }
    }

    // Additional navigation checks can be added here
}

void AlertSystem::playSimpleAlert(AlertPriority priority)
{
    try {
        // Simple audio feedback using QApplication::beep()
        switch (priority) {
        case PRIORITY_CRITICAL:
            // Triple beep for critical alerts
            QApplication::beep();
            QTimer::singleShot(200, []() { QApplication::beep(); });
            QTimer::singleShot(400, []() { QApplication::beep(); });
            break;

        case PRIORITY_HIGH:
            // Double beep for high priority
            QApplication::beep();
            QTimer::singleShot(200, []() { QApplication::beep(); });
            break;

        case PRIORITY_MEDIUM:
            // Single beep for medium priority
            QApplication::beep();
            break;

        case PRIORITY_LOW:
            // Short beep for low priority
            QApplication::beep();
            break;
        }
    } catch (...) {
        // Ignore audio errors
    }
}

void AlertSystem::logAlert(const AlertData& alert)
{
    QString logEntry = QString("[%1] %2 - %3: %4")
    .arg(alert.timestamp.toString("hh:mm:ss"))
        .arg(alert.priority)
        .arg(alert.title)
        .arg(alert.message);

    qDebug() << "[ALERT_LOG]" << logEntry;
}
