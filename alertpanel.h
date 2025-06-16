#ifndef ALERTPANEL_H
#define ALERTPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QTimer>
#include <QScrollArea>
#include <QFrame>
#include <QTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QMessageBox>
#include <QStatusBar>
#include <QMainWindow>

// Forward declarations
class AlertSystem;
struct AlertData;
class EcWidget;

class AlertPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AlertPanel(EcWidget* ecWidget, AlertSystem* alertSystem, QWidget *parent = nullptr);
    ~AlertPanel();

    // Public interface methods
    void refreshAlertList();
    void addAlert(const AlertData& alert);
    void updateAlert(const AlertData& alert);
    void removeAlert(int alertId);

    // Statistics and info
    int getActiveAlertCount() const;
    int getTotalAlertCount() const;
    void showAlertStatistics();

    // Panel state management
    bool validatePanelState();
    void recoverFromError();

    void onAlertTriggered(const AlertData& alert);
    void onAlertSystemStatusChanged(bool enabled);

    void updateAlertCounts();
    void setAlertSystem(AlertSystem* alertSystem);
    void testConnections();

public slots:
    void forceRefreshActiveAlerts();
    void debugAlertStates();
    void forceFullRefresh();

private slots:
    // Alert management
    void onAcknowledgeAlert();
    void onResolveAlert();
    void onClearAllAlerts();
    void onRefreshAlerts();

    // UI interaction
    void onAlertItemClicked();
    void onAlertItemDoubleClicked();
    void onShowAlertDetails();
    void onTabChanged(int index);

    // Filtering and sorting
    void onFilterChanged();
    void onPriorityFilterChanged();
    void onShowResolvedChanged(bool show);

    // Settings
    void onSettingsChanged();
    void onAutoRefreshChanged(bool enabled);
    void onSoundEnabledChanged(bool enabled);

    // Context menu
    void onShowContextMenu(const QPoint& position);

    // Auto refresh
    void onAutoRefresh();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    // Setup methods
    void setupUI();
    void setupActiveAlertsTab();
    void setupHistoryTab();
    void setupSettingsTab();
    void setupConnections();
    void setupContextMenu();

    // UI update methods

    void updateAlertList();
    void updateHistoryTable();  // TAMBAHAN INI
    void updateAlertDetails(const AlertData& alert);
    void updateSystemStatus();

    // Alert management methods
    AlertData* findAlert(int alertId);
    void acknowledgeSelectedAlert();
    void resolveSelectedAlert();
    void clearAllAlerts();

    // Utility methods
    void applyAlertItemStyle(QListWidgetItem* item, const AlertData& alert);
    QString formatAlertTime(const QDateTime& timestamp);
    QString formatAlertPriority(int priority);
    QString formatAlertType(int type);
    QColor getAlertPriorityColor(int priority);
    QIcon getAlertPriorityIcon(int priority);

    // Validation and error handling
    bool validateAlertSystem();
    void logError(const QString& error);
    void showError(const QString& title, const QString& message);

    // UI Components
    QTabWidget* m_tabWidget;

    // Active Alerts Tab
    QWidget* m_activeAlertsTab;
    QTableWidget* m_activeAlertsTable;
    QLabel* m_activeAlertCountLabel;
    QLabel* m_criticalAlertCountLabel;
    QLabel* m_systemStatusLabel;
    QPushButton* m_acknowledgeButton;
    QPushButton* m_resolveButton;
    QPushButton* m_clearAllButton;
    QPushButton* m_refreshButton;

    // Alert Details Panel
    QGroupBox* m_detailsGroup;
    QTextEdit* m_alertDetailsText;

    // History Tab
    QWidget* m_historyTab;
    QTableWidget* m_historyTable;
    QLabel* m_totalAlertCountLabel;
    QCheckBox* m_showResolvedCheckBox;
    QComboBox* m_priorityFilterCombo;
    QPushButton* m_clearHistoryButton;

    // Settings Tab
    QWidget* m_settingsTab;
    QCheckBox* m_autoRefreshCheckBox;
    QSpinBox* m_refreshIntervalSpinBox;
    QCheckBox* m_soundEnabledCheckBox;
    QCheckBox* m_showNotificationsCheckBox;
    QCheckBox* m_autoAcknowledgeLowCheckBox;
    QSpinBox* m_maxHistoryCountSpinBox;

    // Context Menu
    QMenu* m_contextMenu;
    QAction* m_acknowledgeAction;
    QAction* m_resolveAction;
    QAction* m_showDetailsAction;
    QAction* m_copyToClipboardAction;

    // Data and state
    EcWidget* m_ecWidget;
    AlertSystem* m_alertSystem;
    QList<AlertData> m_alertHistory;
    QTimer* m_autoRefreshTimer;

    // Settings
    bool m_autoRefreshEnabled;
    int m_refreshInterval;  // seconds
    bool m_soundEnabled;
    bool m_showNotifications;
    bool m_autoAcknowledgeLow;
    int m_maxHistoryCount;
    bool m_showResolved;

    // State tracking
    int m_lastSelectedAlertId;
    bool m_systemEnabled;
    QString m_lastError;

    void addAlertToTable(int row, const AlertData& alert);

    QStatusBar* statusBar();

signals:
    // Alert actions
    void alertAcknowledged(int alertId);
    void alertResolved(int alertId);
    void alertSelected(int alertId);
    void alertDetailsRequested(int alertId);

    // System events
    void systemStatusChanged(bool enabled);
    void settingsChanged();
    void errorOccurred(const QString& error);

    // Statistics
    void statisticsRequested();
};

#endif // ALERTPANEL_H
