#include "alertpanel.h"
#include "alertsystem.h"
#include "ecwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSplitter>
#include <QDebug>

AlertPanel::AlertPanel(EcWidget* ecWidget, AlertSystem* alertSystem, QWidget *parent)
    : QWidget(parent)
    , m_ecWidget(ecWidget)
    , m_alertSystem(alertSystem)
    , m_lastSelectedAlertId(-1)
    , m_systemEnabled(false)
    , m_autoRefreshEnabled(true)
    , m_refreshInterval(5)
    , m_soundEnabled(true)
    , m_showNotifications(true)
    , m_autoAcknowledgeLow(false)
    , m_maxHistoryCount(100)
    , m_showResolved(false)
{
    qDebug() << "[ALERT_PANEL] Initializing Alert Panel";

    // Validate inputs dengan null check yang lebih safe
    if (!m_ecWidget) {
        qCritical() << "[ALERT_PANEL] EcWidget is null!";
        // Don't return, continue with setup for testing
    }

    if (!m_alertSystem) {
        qWarning() << "[ALERT_PANEL] AlertSystem is null - panel will work in limited mode";
        // Don't return, allow panel to work without AlertSystem for testing
    }

    try {
        // Setup UI - ini harus selalu berhasil
        setupUI();
        setupConnections();
        setupContextMenu();

        // Setup auto refresh timer
        m_autoRefreshTimer = new QTimer(this);
        connect(m_autoRefreshTimer, &QTimer::timeout, this, &AlertPanel::onAutoRefresh);

        if (m_autoRefreshEnabled) {
            m_autoRefreshTimer->start(m_refreshInterval * 1000);
        }

        // Initial update - safe version
        updateSystemStatus();

        // Only refresh if we have AlertSystem
        if (m_alertSystem) {
            refreshAlertList();
        } else {
            // Set dummy status for testing without AlertSystem
            m_systemStatusLabel->setText("Alert System: Not Connected");
            m_systemStatusLabel->setStyleSheet("font-weight: bold; color: red;");
        }

        qDebug() << "[ALERT_PANEL] Alert Panel initialized successfully";

    } catch (const std::exception& e) {
        qCritical() << "[ALERT_PANEL] Exception during initialization:" << e.what();
        throw; // Re-throw to prevent corrupted state
    } catch (...) {
        qCritical() << "[ALERT_PANEL] Unknown exception during initialization";
        throw;
    }
}

AlertPanel::~AlertPanel()
{
    qDebug() << "[ALERT_PANEL] Destroying Alert Panel";

    if (m_autoRefreshTimer) {
        m_autoRefreshTimer->stop();
    }
}

void AlertPanel::setupUI()
{
    setMinimumSize(350, 400);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // System Status Header
    QFrame* statusFrame = new QFrame();
    statusFrame->setFrameStyle(QFrame::StyledPanel);
    statusFrame->setFixedHeight(60);

    QVBoxLayout* statusLayout = new QVBoxLayout(statusFrame);
    statusLayout->setContentsMargins(8, 4, 8, 4);

    m_systemStatusLabel = new QLabel("Alert System: Initializing...");
    m_systemStatusLabel->setStyleSheet("font-weight: bold; color: orange;");

    QHBoxLayout* countsLayout = new QHBoxLayout();
    m_activeAlertCountLabel = new QLabel("Active: 0");
    m_criticalAlertCountLabel = new QLabel("Critical: 0");
    m_criticalAlertCountLabel->setStyleSheet("color: red; font-weight: bold;");

    countsLayout->addWidget(m_activeAlertCountLabel);
    countsLayout->addWidget(m_criticalAlertCountLabel);
    countsLayout->addStretch();

    statusLayout->addWidget(m_systemStatusLabel);
    statusLayout->addLayout(countsLayout);

    mainLayout->addWidget(statusFrame);

    // Tab Widget
    m_tabWidget = new QTabWidget();
    setupActiveAlertsTab();
    setupHistoryTab();
    setupSettingsTab();

    mainLayout->addWidget(m_tabWidget);
}

void AlertPanel::setupActiveAlertsTab()
{
    m_activeAlertsTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_activeAlertsTab);
    layout->setContentsMargins(5, 5, 5, 5);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_acknowledgeButton = new QPushButton("Acknowledge");
    m_acknowledgeButton->setIcon(QIcon(":/icons/acknowledge.png"));
    m_acknowledgeButton->setEnabled(false);
    m_acknowledgeButton->setToolTip("Acknowledge the selected alert");

    m_resolveButton = new QPushButton("Resolve");
    m_resolveButton->setIcon(QIcon(":/icons/resolve.png"));
    m_resolveButton->setEnabled(false);
    m_resolveButton->setToolTip("Resolve the selected alert");

    m_clearAllButton = new QPushButton("Clear All");
    m_clearAllButton->setIcon(QIcon(":/icons/clear.png"));
    m_clearAllButton->setToolTip("Clear all active alerts");

    m_refreshButton = new QPushButton("Refresh");
    m_refreshButton->setIcon(QIcon(":/icons/refresh.png"));
    m_refreshButton->setToolTip("Refresh alert list");

    buttonLayout->addWidget(m_acknowledgeButton);
    buttonLayout->addWidget(m_resolveButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_clearAllButton);
    buttonLayout->addWidget(m_refreshButton);

    layout->addLayout(buttonLayout);

    // Splitter for table and details
    QSplitter* splitter = new QSplitter(Qt::Vertical);

    // Active Alerts Table
    m_activeAlertsTable = new QTableWidget();
    m_activeAlertsTable->setColumnCount(5);
    QStringList headers = {"Time", "Priority", "Type", "Title", "Source"};
    m_activeAlertsTable->setHorizontalHeaderLabels(headers);

    // Configure table
    m_activeAlertsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_activeAlertsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_activeAlertsTable->setAlternatingRowColors(true);
    m_activeAlertsTable->setSortingEnabled(true);

    // PERBAIKAN: Column sizing yang lebih baik
    QHeaderView* header = m_activeAlertsTable->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Fixed);        // Time - fixed
    header->setSectionResizeMode(1, QHeaderView::Fixed);        // Priority - fixed
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents); // Type - content
    header->setSectionResizeMode(3, QHeaderView::Stretch);      // Title - stretch (main content)
    header->setSectionResizeMode(4, QHeaderView::Fixed);        // Source - fixed

    // Set specific column widths
    m_activeAlertsTable->setColumnWidth(0, 70);  // Time
    m_activeAlertsTable->setColumnWidth(1, 80);  // Priority
    m_activeAlertsTable->setColumnWidth(4, 100); // Source

    splitter->addWidget(m_activeAlertsTable);

    // Alert Details Panel - PERBAIKAN
    m_detailsGroup = new QGroupBox("Alert Details");
    QVBoxLayout* detailsLayout = new QVBoxLayout(m_detailsGroup);

    m_alertDetailsText = new QTextEdit();
    m_alertDetailsText->setReadOnly(true);
    m_alertDetailsText->setMaximumHeight(120);
    m_alertDetailsText->setPlainText("Select an alert to view details...");

    // PERBAIKAN: Better styling untuk details
    m_alertDetailsText->setStyleSheet(
        "QTextEdit {"
        "   background-color: #f9f9f9;"
        "   border: 1px solid #ccc;"
        "   font-family: 'Courier New', monospace;"
        "   font-size: 9pt;"
        "}"
        );

    detailsLayout->addWidget(m_alertDetailsText);

    splitter->addWidget(m_detailsGroup);
    splitter->setSizes({250, 120});

    layout->addWidget(splitter);

    m_tabWidget->addTab(m_activeAlertsTab, "Active Alerts");
}

void AlertPanel::setupHistoryTab()
{
    m_historyTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_historyTab);
    layout->setContentsMargins(5, 5, 5, 5);

    // Filter controls
    QHBoxLayout* filterLayout = new QHBoxLayout();

    filterLayout->addWidget(new QLabel("Priority:"));
    m_priorityFilterCombo = new QComboBox();
    m_priorityFilterCombo->addItems({"All", "Critical", "High", "Medium", "Low"});
    filterLayout->addWidget(m_priorityFilterCombo);

    m_showResolvedCheckBox = new QCheckBox("Show Resolved");
    m_showResolvedCheckBox->setChecked(m_showResolved);
    filterLayout->addWidget(m_showResolvedCheckBox);

    filterLayout->addStretch();

    m_totalAlertCountLabel = new QLabel("Total: 0");
    filterLayout->addWidget(m_totalAlertCountLabel);

    m_clearHistoryButton = new QPushButton("Clear History");
    m_clearHistoryButton->setIcon(QIcon(":/icons/clear.png"));
    filterLayout->addWidget(m_clearHistoryButton);

    layout->addLayout(filterLayout);

    // History Table
    m_historyTable = new QTableWidget();
    m_historyTable->setColumnCount(6);
    QStringList historyHeaders = {"Time", "Priority", "Type", "Title", "Status", "Source"};
    m_historyTable->setHorizontalHeaderLabels(historyHeaders);

    // Configure history table
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->setSortingEnabled(true);

    // Auto-resize columns
    QHeaderView* historyHeader = m_historyTable->horizontalHeader();
    historyHeader->setStretchLastSection(true);
    historyHeader->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Time
    historyHeader->setSectionResizeMode(1, QHeaderView::ResizeToContents); // Priority
    historyHeader->setSectionResizeMode(2, QHeaderView::ResizeToContents); // Type
    historyHeader->setSectionResizeMode(3, QHeaderView::Stretch);          // Title
    historyHeader->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Status
    historyHeader->setSectionResizeMode(5, QHeaderView::ResizeToContents); // Source

    layout->addWidget(m_historyTable);

    m_tabWidget->addTab(m_historyTab, "History");
}

void AlertPanel::setupSettingsTab()
{
    m_settingsTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_settingsTab);
    layout->setContentsMargins(10, 10, 10, 10);

    // Auto Refresh Settings
    QGroupBox* refreshGroup = new QGroupBox("Auto Refresh");
    QVBoxLayout* refreshLayout = new QVBoxLayout(refreshGroup);

    m_autoRefreshCheckBox = new QCheckBox("Enable auto refresh");
    m_autoRefreshCheckBox->setChecked(m_autoRefreshEnabled);
    refreshLayout->addWidget(m_autoRefreshCheckBox);

    QHBoxLayout* intervalLayout = new QHBoxLayout();
    intervalLayout->addWidget(new QLabel("Refresh interval:"));
    m_refreshIntervalSpinBox = new QSpinBox();
    m_refreshIntervalSpinBox->setRange(1, 60);
    m_refreshIntervalSpinBox->setValue(m_refreshInterval);
    m_refreshIntervalSpinBox->setSuffix(" seconds");
    intervalLayout->addWidget(m_refreshIntervalSpinBox);
    intervalLayout->addStretch();
    refreshLayout->addLayout(intervalLayout);

    layout->addWidget(refreshGroup);

    // Notification Settings
    QGroupBox* notificationGroup = new QGroupBox("Notifications");
    QVBoxLayout* notificationLayout = new QVBoxLayout(notificationGroup);

    m_soundEnabledCheckBox = new QCheckBox("Enable alert sounds");
    m_soundEnabledCheckBox->setChecked(m_soundEnabled);
    notificationLayout->addWidget(m_soundEnabledCheckBox);

    m_showNotificationsCheckBox = new QCheckBox("Show popup notifications");
    m_showNotificationsCheckBox->setChecked(m_showNotifications);
    notificationLayout->addWidget(m_showNotificationsCheckBox);

    layout->addWidget(notificationGroup);

    // Auto Actions
    QGroupBox* autoGroup = new QGroupBox("Auto Actions");
    QVBoxLayout* autoLayout = new QVBoxLayout(autoGroup);

    m_autoAcknowledgeLowCheckBox = new QCheckBox("Auto-acknowledge low priority alerts");
    m_autoAcknowledgeLowCheckBox->setChecked(m_autoAcknowledgeLow);
    autoLayout->addWidget(m_autoAcknowledgeLowCheckBox);

    layout->addWidget(autoGroup);

    // History Settings
    QGroupBox* historyGroup = new QGroupBox("History");
    QVBoxLayout* historyLayout = new QVBoxLayout(historyGroup);

    QHBoxLayout* historyCountLayout = new QHBoxLayout();
    historyCountLayout->addWidget(new QLabel("Maximum history entries:"));
    m_maxHistoryCountSpinBox = new QSpinBox();
    m_maxHistoryCountSpinBox->setRange(10, 1000);
    m_maxHistoryCountSpinBox->setValue(m_maxHistoryCount);
    historyCountLayout->addWidget(m_maxHistoryCountSpinBox);
    historyCountLayout->addStretch();
    historyLayout->addLayout(historyCountLayout);

    layout->addWidget(historyGroup);

    layout->addStretch();

    m_tabWidget->addTab(m_settingsTab, "Settings");
}

// PERBAIKAN untuk AlertPanel::setupConnections() di alertpanel.cpp
// Ganti method setupConnections() dengan kode berikut:

void AlertPanel::setupConnections()
{
    qDebug() << "[ALERT_PANEL] Setting up connections...";

    // Active Alerts Tab
    connect(m_acknowledgeButton, &QPushButton::clicked, this, &AlertPanel::onAcknowledgeAlert);
    connect(m_resolveButton, &QPushButton::clicked, this, &AlertPanel::onResolveAlert);
    connect(m_clearAllButton, &QPushButton::clicked, this, &AlertPanel::onClearAllAlerts);
    connect(m_refreshButton, &QPushButton::clicked, this, &AlertPanel::onRefreshAlerts);

    connect(m_activeAlertsTable, &QTableWidget::itemClicked, this, &AlertPanel::onAlertItemClicked);
    connect(m_activeAlertsTable, &QTableWidget::itemDoubleClicked, this, &AlertPanel::onAlertItemDoubleClicked);

    // History Tab
    connect(m_priorityFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AlertPanel::onFilterChanged);
    connect(m_showResolvedCheckBox, &QCheckBox::toggled, this, &AlertPanel::onShowResolvedChanged);
    connect(m_clearHistoryButton, &QPushButton::clicked, [this]() {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Clear History",
            "Are you sure you want to clear all alert history?",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            m_alertHistory.clear();
            updateAlertList();
            updateAlertCounts();
        }
    });

    // Settings Tab
    connect(m_autoRefreshCheckBox, &QCheckBox::toggled, this, &AlertPanel::onAutoRefreshChanged);
    connect(m_refreshIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AlertPanel::onSettingsChanged);
    connect(m_soundEnabledCheckBox, &QCheckBox::toggled, this, &AlertPanel::onSoundEnabledChanged);
    connect(m_showNotificationsCheckBox, &QCheckBox::toggled, this, &AlertPanel::onSettingsChanged);
    connect(m_autoAcknowledgeLowCheckBox, &QCheckBox::toggled, this, &AlertPanel::onSettingsChanged);
    connect(m_maxHistoryCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AlertPanel::onSettingsChanged);

    // Tab changes
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &AlertPanel::onTabChanged);

    // === PERBAIKAN UTAMA: KONEKSI ALERT SYSTEM ===
    if (m_alertSystem) {
        qDebug() << "[ALERT_PANEL] AlertSystem available - setting up direct connections";

        // GUNAKAN DIRECT CONNECTION untuk memastikan signal sampai
        bool conn1 = connect(m_alertSystem, &AlertSystem::alertTriggered,
                             this, &AlertPanel::onAlertTriggered,
                             Qt::DirectConnection);  // UBAH ke DirectConnection

        bool conn2 = connect(m_alertSystem, &AlertSystem::systemStatusChanged,
                             this, &AlertPanel::onAlertSystemStatusChanged,
                             Qt::DirectConnection);  // UBAH ke DirectConnection

        qDebug() << "[ALERT_PANEL] Direct AlertSystem connections:"
                 << "alertTriggered:" << conn1
                 << "statusChanged:" << conn2;

        if (!conn1 || !conn2) {
            qCritical() << "[ALERT_PANEL] FAILED to connect to AlertSystem signals!";
        }

        // TAMBAHAN: Set system enabled dan refresh
        m_systemEnabled = true;
        updateSystemStatus();

        // Force refresh setelah setup
        QTimer::singleShot(100, this, &AlertPanel::refreshAlertList);

    } else {
        qDebug() << "[ALERT_PANEL] AlertSystem is null - setup delayed connections";

        // Setup delayed connection check
        QTimer::singleShot(1000, [this]() {
            if (m_ecWidget) {
                AlertSystem* delayedSystem = m_ecWidget->getAlertSystem();
                if (delayedSystem && delayedSystem != m_alertSystem) {
                    qDebug() << "[ALERT_PANEL] Found delayed AlertSystem - reconnecting";
                    setAlertSystem(delayedSystem);
                }
            }
        });
    }

    // === KONEKSI KE ECWIDGET (SEBAGAI BACKUP) ===
    if (m_ecWidget) {
        qDebug() << "[ALERT_PANEL] Setting up EcWidget backup connections";

        bool conn3 = connect(m_ecWidget, &EcWidget::alertTriggered,
                             this, &AlertPanel::onAlertTriggered,
                             Qt::DirectConnection);

        bool conn4 = connect(m_ecWidget, &EcWidget::alertSystemStatusChanged,
                             this, &AlertPanel::onAlertSystemStatusChanged,
                             Qt::DirectConnection);

        qDebug() << "[ALERT_PANEL] EcWidget backup connections:"
                 << "alertTriggered:" << conn3
                 << "statusChanged:" << conn4;
    }

    qDebug() << "[ALERT_PANEL] All connections setup completed";
}

// TAMBAHAN: Method untuk test koneksi
void AlertPanel::testConnections()
{
    qDebug() << "[ALERT_PANEL] Testing connections...";

    if (m_alertSystem) {
        qDebug() << "[ALERT_PANEL] AlertSystem available at:" << m_alertSystem;

        // Test dengan membuat alert langsung
        QTimer::singleShot(500, [this]() {
            if (m_alertSystem) {
                int testId = m_alertSystem->triggerAlert(
                    ALERT_USER_DEFINED,
                    PRIORITY_LOW,
                    "Connection Test",
                    "Testing AlertPanel connection",
                    "Test_System",
                    0.0, 0.0
                    );
                qDebug() << "[ALERT_PANEL] Test alert created with ID:" << testId;
            }
        });
    } else {
        qDebug() << "[ALERT_PANEL] AlertSystem is NULL - cannot test";
    }
}

// PERBAIKAN untuk onAlertTriggered - pastikan method ini robust
void AlertPanel::onAlertTriggered(const AlertData& alert)
{
    qDebug() << "[ALERT_PANEL] *** onAlertTriggered called ***";
    qDebug() << "[ALERT_PANEL] Alert ID:" << alert.id;
    qDebug() << "[ALERT_PANEL] Alert Title:" << alert.title;
    qDebug() << "[ALERT_PANEL] Alert Priority:" << alert.priority;
    qDebug() << "[ALERT_PANEL] Alert State:" << alert.state;

    try {
        // PERBAIKAN: Pastikan alert ditambahkan ke history
        addAlert(alert);

        // PERBAIKAN: Force UI update dengan error handling
        QTimer::singleShot(10, [this]() {
            try {
                updateAlertCounts();
                updateAlertList();

                // Force table refresh
                if (m_activeAlertsTable) {
                    m_activeAlertsTable->viewport()->update();
                }

                qDebug() << "[ALERT_PANEL] UI refresh completed";
            } catch (const std::exception& e) {
                qCritical() << "[ALERT_PANEL] Error in UI refresh:" << e.what();
            }
        });

        // Auto acknowledge untuk low priority jika enabled
        if (m_autoAcknowledgeLow && alert.priority == PRIORITY_LOW) {
            QTimer::singleShot(200, [this, alert]() {
                if (m_alertSystem) {
                    qDebug() << "[ALERT_PANEL] Auto-acknowledging low priority alert:" << alert.id;
                    m_alertSystem->acknowledgeAlert(alert.id);
                    QTimer::singleShot(100, this, &AlertPanel::refreshAlertList);
                }
            });
        }

        qDebug() << "[ALERT_PANEL] onAlertTriggered completed successfully";

    } catch (const std::exception& e) {
        qCritical() << "[ALERT_PANEL] Exception in onAlertTriggered:" << e.what();
    } catch (...) {
        qCritical() << "[ALERT_PANEL] Unknown exception in onAlertTriggered";
    }
}

void AlertPanel::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    m_acknowledgeAction = m_contextMenu->addAction("Acknowledge Alert");
    m_acknowledgeAction->setIcon(QIcon(":/icons/acknowledge.png"));
    connect(m_acknowledgeAction, &QAction::triggered, this, &AlertPanel::onAcknowledgeAlert);

    m_resolveAction = m_contextMenu->addAction("Resolve Alert");
    m_resolveAction->setIcon(QIcon(":/icons/resolve.png"));
    connect(m_resolveAction, &QAction::triggered, this, &AlertPanel::onResolveAlert);

    m_contextMenu->addSeparator();

    m_showDetailsAction = m_contextMenu->addAction("Show Details");
    m_showDetailsAction->setIcon(QIcon(":/icons/details.png"));
    connect(m_showDetailsAction, &QAction::triggered, this, &AlertPanel::onShowAlertDetails);

    m_copyToClipboardAction = m_contextMenu->addAction("Copy to Clipboard");
    m_copyToClipboardAction->setIcon(QIcon(":/icons/copy.png"));
    connect(m_copyToClipboardAction, &QAction::triggered, this, [this]() {
        int currentRow = m_activeAlertsTable->currentRow();
        if (currentRow >= 0) {
            QStringList rowData;
            for (int col = 0; col < m_activeAlertsTable->columnCount(); ++col) {
                QTableWidgetItem* item = m_activeAlertsTable->item(currentRow, col);
                rowData << (item ? item->text() : "");
            }
            QApplication::clipboard()->setText(rowData.join("\t"));
        }
    });

    // Enable context menu untuk table
    m_activeAlertsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_activeAlertsTable, &QTableWidget::customContextMenuRequested,
            this, &AlertPanel::onShowContextMenu);
}

void AlertPanel::onAlertSystemStatusChanged(bool enabled)
{
    m_systemEnabled = enabled;
    updateSystemStatus();

    if (enabled) {
        refreshAlertList();
    }
}

void AlertPanel::refreshAlertList()
{
    if (!m_alertSystem) {
        qWarning() << "[ALERT_PANEL] Cannot refresh - AlertSystem is null";
        return;
    }

    try {
        updateAlertList();
        updateAlertCounts();
    } catch (const std::exception& e) {
        qCritical() << "[ALERT_PANEL] Error refreshing alert list:" << e.what();
        logError(QString("Failed to refresh alert list: %1").arg(e.what()));
    } catch (...) {
        qCritical() << "[ALERT_PANEL] Unknown error refreshing alert list";
        logError("Unknown error occurred while refreshing alert list");
    }
}

void AlertPanel::addAlert(const AlertData& alert)
{
    qDebug() << "[ALERT_PANEL] addAlert called - ID:" << alert.id << "Title:" << alert.title << "State:" << alert.state;

    // PERBAIKAN: Pastikan alert memiliki state yang benar
    AlertData processedAlert = alert;

    // Jika state tidak di-set, default ke ACTIVE
    if (processedAlert.state == 0) {
        processedAlert.state = STATE_ACTIVE;
        qDebug() << "[ALERT_PANEL] Fixed alert state to ACTIVE";
    }

    // Check untuk duplikat berdasarkan ID
    bool isDuplicate = false;
    for (int i = 0; i < m_alertHistory.size(); ++i) {
        if (m_alertHistory[i].id == processedAlert.id) {
            // Update existing alert
            m_alertHistory[i] = processedAlert;
            isDuplicate = true;
            qDebug() << "[ALERT_PANEL] Updated existing alert ID:" << processedAlert.id;
            break;
        }
    }

    // Add new alert jika bukan duplikat
    if (!isDuplicate) {
        m_alertHistory.prepend(processedAlert);
        qDebug() << "[ALERT_PANEL] Added new alert to history, total size:" << m_alertHistory.size();
    }

    // Limit history size
    while (m_alertHistory.size() > m_maxHistoryCount) {
        m_alertHistory.removeLast();
    }

    // PERBAIKAN: Force immediate UI update
    qDebug() << "[ALERT_PANEL] Forcing immediate UI refresh...";
    updateAlertCounts();
    updateAlertList();

    // Force table repaint
    if (m_activeAlertsTable) {
        m_activeAlertsTable->viewport()->update();
    }

    qDebug() << "[ALERT_PANEL] addAlert completed";
}

void AlertPanel::updateAlert(const AlertData& alert)
{
    // Update in history
    for (int i = 0; i < m_alertHistory.size(); ++i) {
        if (m_alertHistory[i].id == alert.id) {
            m_alertHistory[i] = alert;
            break;
        }
    }

    refreshAlertList();
}

void AlertPanel::removeAlert(int alertId)
{
    // Remove from history
    for (int i = 0; i < m_alertHistory.size(); ++i) {
        if (m_alertHistory[i].id == alertId) {
            m_alertHistory.removeAt(i);
            break;
        }
    }

    refreshAlertList();
}

// Slot implementations akan dilanjutkan di bagian berikutnya...

void AlertPanel::onAcknowledgeAlert()
{
    acknowledgeSelectedAlert();
}

void AlertPanel::onResolveAlert()
{
    resolveSelectedAlert();
}

void AlertPanel::onClearAllAlerts()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Clear All Alerts",
        "Are you sure you want to clear all active alerts?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        clearAllAlerts();
    }
}

void AlertPanel::onRefreshAlerts()
{
    qDebug() << "[ALERT_PANEL] Manual refresh requested";

    try {
        // Show loading state
        m_refreshButton->setText("Refreshing...");
        m_refreshButton->setEnabled(false);

        // Force refresh
        refreshAlertList();

        // Update counts
        updateAlertCounts();

        // Reset button
        m_refreshButton->setText("Refresh");
        m_refreshButton->setEnabled(true);

        // Show feedback dengan QMessageBox instead of statusBar
        int activeCount = 0;
        if (m_alertSystem) {
            activeCount = m_alertSystem->getActiveAlerts().size();
        } else {
            for (const AlertData& alert : m_alertHistory) {
                if (alert.state == STATE_ACTIVE) {
                    activeCount++;
                }
            }
        }

        // GANTI statusBar dengan QMessageBox temporary
        QString message = QString("Refreshed successfully!\nActive alerts: %1").arg(activeCount);

        // Show temporary message box (auto-close after 2 seconds)
        QMessageBox* msgBox = new QMessageBox(QMessageBox::Information,
                                              "Refresh Complete",
                                              message,
                                              QMessageBox::NoButton,
                                              this);
        msgBox->setModal(false);
        msgBox->show();

        // Auto close after 2 seconds
        QTimer::singleShot(2000, msgBox, &QMessageBox::close);
        QTimer::singleShot(2100, msgBox, &QMessageBox::deleteLater);

        qDebug() << "[ALERT_PANEL] Manual refresh completed";

    } catch (const std::exception& e) {
        qCritical() << "[ALERT_PANEL] Error during refresh:" << e.what();

        m_refreshButton->setText("Refresh");
        m_refreshButton->setEnabled(true);

        QMessageBox::critical(this, "Refresh Error",
                              QString("Failed to refresh alerts: %1").arg(e.what()));
    }
}

void AlertPanel::onAlertItemClicked()
{
    int currentRow = m_activeAlertsTable->currentRow();
    bool hasSelection = (currentRow >= 0);

    qDebug() << "[ALERT_PANEL] Alert item clicked, row:" << currentRow;

    // Enable/disable buttons
    m_acknowledgeButton->setEnabled(hasSelection);
    m_resolveButton->setEnabled(hasSelection);

    if (hasSelection) {
        // Get alert ID from table
        QTableWidgetItem* timeItem = m_activeAlertsTable->item(currentRow, 0);
        if (timeItem) {
            int alertId = timeItem->data(Qt::UserRole).toInt();
            qDebug() << "[ALERT_PANEL] Selected alert ID:" << alertId;

            // PERBAIKAN: Cari di history dulu (lebih reliable)
            AlertData* selectedAlert = nullptr;

            for (const AlertData& alert : m_alertHistory) {
                if (alert.id == alertId) {
                    selectedAlert = const_cast<AlertData*>(&alert);
                    qDebug() << "[ALERT_PANEL] Found alert in history - State:" << alert.state;
                    break;
                }
            }

            // Fallback: cari di AlertSystem
            if (!selectedAlert && m_alertSystem) {
                QList<AlertData> activeAlerts = m_alertSystem->getActiveAlerts();
                for (const AlertData& alert : activeAlerts) {
                    if (alert.id == alertId) {
                        selectedAlert = const_cast<AlertData*>(&alert);
                        qDebug() << "[ALERT_PANEL] Found alert in AlertSystem - State:" << alert.state;
                        break;
                    }
                }
            }

            if (selectedAlert) {
                updateAlertDetails(*selectedAlert);
                m_lastSelectedAlertId = alertId;

                // PERBAIKAN: Enable/disable buttons berdasarkan state
                if (selectedAlert->state == STATE_ACTIVE) {
                    m_acknowledgeButton->setEnabled(true);
                    m_resolveButton->setEnabled(true);
                } else if (selectedAlert->state == STATE_ACKNOWLEDGED) {
                    m_acknowledgeButton->setEnabled(false);  // Sudah acknowledged
                    m_resolveButton->setEnabled(true);       // Bisa resolve
                }

                emit alertSelected(alertId);
            } else {
                qWarning() << "[ALERT_PANEL] Could not find alert data for ID:" << alertId;
                debugAlertStates();  // Debug jika tidak ketemu
            }
        }
    } else {
        // Clear details jika tidak ada selection
        m_alertDetailsText->setPlainText("Select an alert to view details...");
        m_lastSelectedAlertId = -1;
    }
}

void AlertPanel::onAlertItemDoubleClicked()
{
    onShowAlertDetails();
}

void AlertPanel::onShowAlertDetails()
{
    if (m_lastSelectedAlertId != -1) {
        emit alertDetailsRequested(m_lastSelectedAlertId);
    }
}

void AlertPanel::onTabChanged(int index)
{
    // Refresh data when switching tabs
    if (index == 1) { // History tab
        updateAlertList();
    }
}

void AlertPanel::onFilterChanged()
{
    updateAlertList();
}

void AlertPanel::onPriorityFilterChanged()
{
    updateAlertList();
}

void AlertPanel::onShowResolvedChanged(bool show)
{
    m_showResolved = show;
    updateAlertList();
}

void AlertPanel::onSettingsChanged()
{
    // Update settings from UI
    m_refreshInterval = m_refreshIntervalSpinBox->value();
    m_showNotifications = m_showNotificationsCheckBox->isChecked();
    m_autoAcknowledgeLow = m_autoAcknowledgeLowCheckBox->isChecked();
    m_maxHistoryCount = m_maxHistoryCountSpinBox->value();

    // Restart timer if interval changed
    if (m_autoRefreshEnabled && m_autoRefreshTimer) {
        m_autoRefreshTimer->start(m_refreshInterval * 1000);
    }

    emit settingsChanged();
}

void AlertPanel::onAutoRefreshChanged(bool enabled)
{
    m_autoRefreshEnabled = enabled;

    if (m_autoRefreshTimer) {
        if (enabled) {
            m_autoRefreshTimer->start(m_refreshInterval * 1000);
        } else {
            m_autoRefreshTimer->stop();
        }
    }

    m_refreshIntervalSpinBox->setEnabled(enabled);
}

void AlertPanel::onSoundEnabledChanged(bool enabled)
{
    m_soundEnabled = enabled;
}

void AlertPanel::onShowContextMenu(const QPoint& position)
{
    QTableWidgetItem* item = m_activeAlertsTable->itemAt(position);
    if (item) {
        m_contextMenu->exec(m_activeAlertsTable->mapToGlobal(position));
    }
}

void AlertPanel::onAutoRefresh()
{
    if (m_systemEnabled && isVisible()) {
        refreshAlertList();
    }
}

void AlertPanel::updateAlertCounts()
{
    int activeCount = 0;
    int acknowledgedCount = 0;
    int criticalCount = 0;
    int totalCount = m_alertHistory.size();

    if (m_alertSystem) {
        // Hitung dari AlertSystem
        QList<AlertData> allAlerts = m_alertSystem->getActiveAlerts();

        for (const AlertData& alert : allAlerts) {
            if (alert.state == STATE_ACTIVE) {
                activeCount++;
                if (alert.priority == PRIORITY_CRITICAL) {
                    criticalCount++;
                }
            } else if (alert.state == STATE_ACKNOWLEDGED) {
                acknowledgedCount++;
                if (alert.priority == PRIORITY_CRITICAL) {
                    criticalCount++;
                }
            }
        }

    } else {
        // Hitung dari history
        for (const AlertData& alert : m_alertHistory) {
            if (alert.state == STATE_ACTIVE) {
                activeCount++;
                if (alert.priority == PRIORITY_CRITICAL) {
                    criticalCount++;
                }
            } else if (alert.state == STATE_ACKNOWLEDGED) {
                acknowledgedCount++;
                if (alert.priority == PRIORITY_CRITICAL) {
                    criticalCount++;
                }
            }
        }
    }

    // PERBAIKAN: Display format yang lebih informatif
    if (acknowledgedCount > 0) {
        m_activeAlertCountLabel->setText(QString("Active: %1 (%2 ack)").arg(activeCount).arg(acknowledgedCount));
    } else {
        m_activeAlertCountLabel->setText(QString("Active: %1").arg(activeCount));
    }

    m_criticalAlertCountLabel->setText(QString("Critical: %1").arg(criticalCount));
    m_totalAlertCountLabel->setText(QString("Total: %1").arg(totalCount));

    // Update critical count color
    if (criticalCount > 0) {
        m_criticalAlertCountLabel->setStyleSheet("color: red; font-weight: bold;");
    } else {
        m_criticalAlertCountLabel->setStyleSheet("color: green; font-weight: bold;");
    }

    qDebug() << "[ALERT_PANEL] Counts - Active:" << activeCount << "Acknowledged:" << acknowledgedCount << "Critical:" << criticalCount;
}

void AlertPanel::updateAlertList()
{
    qDebug() << "[ALERT_PANEL] updateAlertList() called";

    if (!m_activeAlertsTable) {
        qWarning() << "[ALERT_PANEL] Active alerts table is null!";
        return;
    }

    // Clear table first
    m_activeAlertsTable->setRowCount(0);

    // PERBAIKAN: Active tab harus menampilkan ACTIVE + ACKNOWLEDGED
    QList<AlertData> displayAlerts;

    // Method 1: Dari AlertSystem (jika tersedia)
    if (m_alertSystem) {
        QList<AlertData> systemAlerts = m_alertSystem->getActiveAlerts();
        for (const AlertData& alert : systemAlerts) {
            // Tampilkan alert yang ACTIVE atau ACKNOWLEDGED
            if (alert.state == STATE_ACTIVE || alert.state == STATE_ACKNOWLEDGED) {
                displayAlerts.append(alert);
            }
        }
        qDebug() << "[ALERT_PANEL] AlertSystem provides" << systemAlerts.size() << "total alerts," << displayAlerts.size() << "for display";
    }

    // Method 2: Dari History (filter yang STATE_ACTIVE + STATE_ACKNOWLEDGED)
    QList<AlertData> historyDisplayAlerts;
    for (const AlertData& alert : m_alertHistory) {
        if (alert.state == STATE_ACTIVE || alert.state == STATE_ACKNOWLEDGED) {
            historyDisplayAlerts.append(alert);
        }
    }
    qDebug() << "[ALERT_PANEL] History provides" << historyDisplayAlerts.size() << "display alerts";

    // PERBAIKAN: Gunakan history jika AlertSystem kosong
    if (displayAlerts.isEmpty() && !historyDisplayAlerts.isEmpty()) {
        displayAlerts = historyDisplayAlerts;
        qDebug() << "[ALERT_PANEL] Using history display alerts as fallback";
    }

    // PERBAIKAN: Merge alerts dari kedua sumber (hapus duplikat)
    if (!displayAlerts.isEmpty() && !historyDisplayAlerts.isEmpty()) {
        for (const AlertData& historyAlert : historyDisplayAlerts) {
            bool found = false;
            for (const AlertData& systemAlert : displayAlerts) {
                if (systemAlert.id == historyAlert.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                displayAlerts.append(historyAlert);
            }
        }
    }

    qDebug() << "[ALERT_PANEL] Final display alerts count:" << displayAlerts.size();

    // Populate Active Alerts Table
    m_activeAlertsTable->setRowCount(displayAlerts.size());

    for (int i = 0; i < displayAlerts.size(); ++i) {
        const AlertData& alert = displayAlerts[i];
        qDebug() << "[ALERT_PANEL] Adding alert to table - ID:" << alert.id << "Title:" << alert.title << "State:" << alert.state;
        addAlertToTable(i, alert);
    }

    qDebug() << "[ALERT_PANEL] Active alerts table updated with" << displayAlerts.size() << "rows";

    // Update history table
    updateHistoryTable();

    // Force repaint
    m_activeAlertsTable->viewport()->update();
    update();
}

// Helper method untuk add alert ke table
void AlertPanel::addAlertToTable(int row, const AlertData& alert)
{
    if (!m_activeAlertsTable || row < 0) {
        qWarning() << "[ALERT_PANEL] Invalid table or row for addAlertToTable";
        return;
    }

    try {
        // Pastikan table memiliki row yang cukup
        if (row >= m_activeAlertsTable->rowCount()) {
            m_activeAlertsTable->setRowCount(row + 1);
        }

        // Time - format yang lebih readable
        QTableWidgetItem* timeItem = new QTableWidgetItem(alert.timestamp.toString("hh:mm:ss"));
        timeItem->setData(Qt::UserRole, alert.id);
        timeItem->setTextAlignment(Qt::AlignCenter);
        m_activeAlertsTable->setItem(row, 0, timeItem);

        // Priority dengan color dan emoji
        QString priorityText = formatAlertPriority(alert.priority);
        QString priorityEmoji;
        QColor priorityColor;

        switch (alert.priority) {
        case PRIORITY_CRITICAL:
            priorityEmoji = "🚨 ";
            priorityColor = QColor(255, 100, 100);
            break;
        case PRIORITY_HIGH:
            priorityEmoji = "🔴 ";
            priorityColor = QColor(255, 150, 100);
            break;
        case PRIORITY_MEDIUM:
            priorityEmoji = "🟡 ";
            priorityColor = QColor(255, 255, 150);
            break;
        case PRIORITY_LOW:
            priorityEmoji = "🟢 ";
            priorityColor = QColor(150, 255, 150);
            break;
        default:
            priorityEmoji = "⚪ ";
            priorityColor = QColor(200, 200, 200);
            break;
        }

        // PERBAIKAN: Dimmed color untuk ACKNOWLEDGED alerts
        if (alert.state == STATE_ACKNOWLEDGED) {
            priorityColor = priorityColor.lighter(150); // Make it lighter
        }

        QTableWidgetItem* priorityItem = new QTableWidgetItem(priorityEmoji + priorityText);
        priorityItem->setBackground(priorityColor);
        priorityItem->setTextAlignment(Qt::AlignCenter);
        m_activeAlertsTable->setItem(row, 1, priorityItem);

        // Type dengan icon
        QString typeText = formatAlertType(alert.type);
        QString typeIcon;
        switch (alert.type) {
        case ALERT_GUARDZONE_PROXIMITY: typeIcon = "🛡️ "; break;
        case ALERT_DEPTH_SHALLOW: typeIcon = "💧 "; break;
        case ALERT_DEPTH_DEEP: typeIcon = "🌊 "; break;
        case ALERT_COLLISION_RISK: typeIcon = "⚠️ "; break;
        case ALERT_NAVIGATION_WARNING: typeIcon = "🧭 "; break;
        case ALERT_SYSTEM_ERR: typeIcon = "⚙️ "; break;
        case ALERT_USER_DEFINED: typeIcon = "👤 "; break;
        default: typeIcon = "📋 "; break;
        }

        QTableWidgetItem* typeItem = new QTableWidgetItem(typeIcon + typeText);
        m_activeAlertsTable->setItem(row, 2, typeItem);

        // Title dengan tooltip dan status indicator
        QString titleText = alert.title;

        // PERBAIKAN: Tambah status indicator di title
        if (alert.state == STATE_ACKNOWLEDGED) {
            titleText = "✅ " + titleText + " (Acknowledged)";
        }

        QTableWidgetItem* titleItem = new QTableWidgetItem(titleText);
        titleItem->setToolTip(QString("Message: %1\nTime: %2\nStatus: %3")
                                  .arg(alert.message)
                                  .arg(alert.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
                                  .arg(alert.state == STATE_ACKNOWLEDGED ? "Acknowledged" : "Active"));
        m_activeAlertsTable->setItem(row, 3, titleItem);

        // Source
        QTableWidgetItem* sourceItem = new QTableWidgetItem(alert.source);
        sourceItem->setTextAlignment(Qt::AlignCenter);
        m_activeAlertsTable->setItem(row, 4, sourceItem);

        // PERBAIKAN: Row styling berdasarkan STATE
        QColor rowColor;
        QFont rowFont;

        if (alert.state == STATE_ACKNOWLEDGED) {
            // Acknowledged alerts - dimmed appearance
            rowColor = QColor(240, 240, 240);  // Light gray background
            rowFont.setItalic(true);           // Italic text

            // Apply dimmed styling to all columns
            for (int col = 0; col < m_activeAlertsTable->columnCount(); ++col) {
                QTableWidgetItem* item = m_activeAlertsTable->item(row, col);
                if (item) {
                    item->setBackground(rowColor);
                    item->setFont(rowFont);

                    // Also dim the text color
                    QColor textColor = item->foreground().color();
                    textColor = textColor.lighter(120);
                    item->setForeground(textColor);
                }
            }

        } else {
            // Active alerts - normal appearance with priority coloring
            if (alert.priority == PRIORITY_CRITICAL) {
                rowColor = QColor(255, 240, 240);
            } else if (alert.priority == PRIORITY_HIGH) {
                rowColor = QColor(255, 248, 240);
            }

            if (rowColor.isValid()) {
                for (int col = 0; col < m_activeAlertsTable->columnCount(); ++col) {
                    QTableWidgetItem* item = m_activeAlertsTable->item(row, col);
                    if (item && col != 1) { // Skip priority column (sudah ada background)
                        item->setBackground(rowColor);
                    }
                }
            }
        }

        qDebug() << "[ALERT_PANEL] Successfully added alert to table row" << row << "with state" << alert.state;

    } catch (const std::exception& e) {
        qWarning() << "[ALERT_PANEL] Error adding alert to table:" << e.what();
    } catch (...) {
        qWarning() << "[ALERT_PANEL] Unknown error adding alert to table";
    }
}

void AlertPanel::updateHistoryTable()
{
    // Apply filters
    QList<AlertData> filteredHistory;
    QString priorityFilter = m_priorityFilterCombo->currentText();

    for (const AlertData& alert : m_alertHistory) {
        // Priority filter
        if (priorityFilter != "All") {
            QString alertPriorityStr = formatAlertPriority(alert.priority);
            if (alertPriorityStr != priorityFilter) {
                continue;
            }
        }

        // Resolved filter
        if (!m_showResolved && alert.state == STATE_RESOLVED) {
            continue;
        }

        filteredHistory.append(alert);
    }

    // Populate history table
    m_historyTable->setRowCount(filteredHistory.size());

    for (int i = 0; i < filteredHistory.size(); ++i) {
        const AlertData& alert = filteredHistory[i];

        // Time
        m_historyTable->setItem(i, 0, new QTableWidgetItem(formatAlertTime(alert.timestamp)));

        // Priority
        QTableWidgetItem* priorityItem = new QTableWidgetItem(formatAlertPriority(alert.priority));
        priorityItem->setBackground(getAlertPriorityColor(alert.priority));
        priorityItem->setIcon(getAlertPriorityIcon(alert.priority));
        m_historyTable->setItem(i, 1, priorityItem);

        // Type
        m_historyTable->setItem(i, 2, new QTableWidgetItem(formatAlertType(alert.type)));

        // Title
        QTableWidgetItem* titleItem = new QTableWidgetItem(alert.title);
        titleItem->setToolTip(alert.message);
        m_historyTable->setItem(i, 3, titleItem);

        // Status
        QString status;
        switch (alert.state) {
        case STATE_ACTIVE: status = "Active"; break;
        case STATE_ACKNOWLEDGED: status = "Acknowledged"; break;
        case STATE_RESOLVED: status = "Resolved"; break;
        case STATE_SILENCED: status = "Silenced"; break;
        default: status = "Unknown"; break;
        }
        m_historyTable->setItem(i, 4, new QTableWidgetItem(status));

        // Source
        m_historyTable->setItem(i, 5, new QTableWidgetItem(alert.source));
    }
}

void AlertPanel::updateAlertDetails(const AlertData& alert)
{
    QString details;

    // Header dengan separator
    details += QString("═══ ALERT DETAILS ═══\n\n");

    // Basic info
    details += QString("🆔 Alert ID: %1\n").arg(alert.id);
    details += QString("📋 Title: %1\n").arg(alert.title);
    details += QString("📝 Message: %1\n\n").arg(alert.message);

    // Priority dengan emoji
    QString priorityText = formatAlertPriority(alert.priority);
    QString priorityEmoji;
    switch (alert.priority) {
    case PRIORITY_CRITICAL: priorityEmoji = "🚨"; break;
    case PRIORITY_HIGH: priorityEmoji = "🔴"; break;
    case PRIORITY_MEDIUM: priorityEmoji = "🟡"; break;
    case PRIORITY_LOW: priorityEmoji = "🟢"; break;
    default: priorityEmoji = "⚪"; break;
    }
    details += QString("⚠️  Priority: %1 %2\n").arg(priorityEmoji, priorityText);

    // Type dan Source
    details += QString("🏷️  Type: %1\n").arg(formatAlertType(alert.type));
    details += QString("🏢 Source: %1\n").arg(alert.source);

    // Timestamp
    details += QString("🕒 Time: %1\n").arg(alert.timestamp.toString("yyyy-MM-dd hh:mm:ss"));

    // Location jika ada
    if (alert.latitude != 0.0 || alert.longitude != 0.0) {
        details += QString("📍 Position: %1°, %2°\n")
                       .arg(alert.latitude, 0, 'f', 6)
                       .arg(alert.longitude, 0, 'f', 6);
    }

    // Status dengan action hints
    QString statusText;
    QString statusEmoji;
    QString actionHint;

    switch (alert.state) {
    case STATE_ACTIVE:
        statusText = "Active";
        statusEmoji = "🔴";
        actionHint = "Click 'Acknowledge' to confirm you've seen this alert";
        break;
    case STATE_ACKNOWLEDGED:
        statusText = "Acknowledged";
        statusEmoji = "🟡";
        actionHint = "Click 'Resolve' when the issue is fixed";
        break;
    case STATE_RESOLVED:
        statusText = "Resolved";
        statusEmoji = "🟢";
        actionHint = "Alert has been resolved";
        break;
    case STATE_SILENCED:
        statusText = "Silenced";
        statusEmoji = "🔇";
        actionHint = "Alert has been silenced";
        break;
    default:
        statusText = "Unknown";
        statusEmoji = "❓";
        actionHint = "";
        break;
    }
    details += QString("📊 Status: %1 %2\n").arg(statusEmoji, statusText);

    if (!actionHint.isEmpty()) {
        details += QString("💡 Action: %1\n").arg(actionHint);
    }

    if (alert.requiresAcknowledgment) {
        details += QString("✅ Requires Acknowledgment: Yes\n");
    }

    m_alertDetailsText->setPlainText(details);
}

void AlertPanel::updateSystemStatus()
{
    if (!m_alertSystem) {
        m_systemStatusLabel->setText("Alert System: Not Available");
        m_systemStatusLabel->setStyleSheet("font-weight: bold; color: red;");
        return;
    }

    if (m_systemEnabled) {
        m_systemStatusLabel->setText("Alert System: Active");
        m_systemStatusLabel->setStyleSheet("font-weight: bold; color: green;");
    } else {
        m_systemStatusLabel->setText("Alert System: Inactive");
        m_systemStatusLabel->setStyleSheet("font-weight: bold; color: orange;");
    }
}

AlertData* AlertPanel::findAlert(int alertId)
{
    if (!m_alertSystem) {
        return nullptr;
    }

    QList<AlertData> activeAlerts = m_alertSystem->getActiveAlerts();
    for (AlertData& alert : activeAlerts) {
        if (alert.id == alertId) {
            return &alert;
        }
    }

    return nullptr;
}

void AlertPanel::acknowledgeSelectedAlert()
{
    qDebug() << "[ALERT_PANEL] acknowledgeSelectedAlert() called";

    if (m_lastSelectedAlertId == -1) {
        qWarning() << "[ALERT_PANEL] No alert selected for acknowledgment";
        QMessageBox::warning(this, "No Selection", "Please select an alert to acknowledge.");
        return;
    }

    qDebug() << "[ALERT_PANEL] Acknowledging alert ID:" << m_lastSelectedAlertId;

    bool success = false;
    AlertData* alertToUpdate = nullptr;

    // PERBAIKAN: Update history DULU, baru AlertSystem
    for (int i = 0; i < m_alertHistory.size(); ++i) {
        if (m_alertHistory[i].id == m_lastSelectedAlertId) {
            alertToUpdate = &m_alertHistory[i];
            alertToUpdate->state = STATE_ACKNOWLEDGED;
            success = true;
            qDebug() << "[ALERT_PANEL] Updated alert state in history to ACKNOWLEDGED";
            break;
        }
    }

    // Method 2: Acknowledge via AlertSystem (secondary)
    if (m_alertSystem) {
        bool systemSuccess = m_alertSystem->acknowledgeAlert(m_lastSelectedAlertId);
        qDebug() << "[ALERT_PANEL] AlertSystem acknowledge result:" << systemSuccess;
        if (!success) success = systemSuccess;
    }

    if (success) {
        // PERBAIKAN: Force immediate refresh
        qDebug() << "[ALERT_PANEL] Forcing immediate UI refresh after acknowledge";

        // Debug before refresh
        debugAlertStates();

        // Update UI immediately dengan force
        updateAlertCounts();
        updateAlertList();

        // Force table repaint
        m_activeAlertsTable->viewport()->repaint();

        // Debug after refresh
        debugAlertStates();

        // Clear selection dan disable buttons
        m_activeAlertsTable->clearSelection();
        m_acknowledgeButton->setEnabled(false);
        m_resolveButton->setEnabled(false);
        m_alertDetailsText->setPlainText("Select an alert to view details...");

        int acknowledgedId = m_lastSelectedAlertId;
        m_lastSelectedAlertId = -1;

        // Emit signal
        emit alertAcknowledged(acknowledgedId);

        // Show user feedback
        QString message = QString("Alert #%1 has been acknowledged").arg(acknowledgedId);
        QMessageBox::information(this, "Alert Acknowledged", message);

        qDebug() << "[ALERT_PANEL] Alert acknowledged successfully";

    } else {
        QString errorMsg = QString("Failed to acknowledge alert #%1").arg(m_lastSelectedAlertId);
        qWarning() << "[ALERT_PANEL]" << errorMsg;
        QMessageBox::critical(this, "Acknowledge Failed", errorMsg);
    }
}

void AlertPanel::resolveSelectedAlert()
{
    qDebug() << "[ALERT_PANEL] resolveSelectedAlert() called";

    if (m_lastSelectedAlertId == -1) {
        qWarning() << "[ALERT_PANEL] No alert selected for resolution";
        QMessageBox::warning(this, "No Selection", "Please select an alert to resolve.");
        return;
    }

    // Konfirmasi dari user
    AlertData* alertData = nullptr;
    for (const AlertData& alert : m_alertHistory) {
        if (alert.id == m_lastSelectedAlertId) {
            alertData = const_cast<AlertData*>(&alert);
            break;
        }
    }

    QString confirmMsg;
    if (alertData) {
        confirmMsg = QString("Are you sure you want to resolve this alert?\n\n"
                             "ID: %1\n"
                             "Title: %2\n"
                             "Current State: %3\n"
                             "Priority: %4")
                         .arg(alertData->id)
                         .arg(alertData->title)
                         .arg(alertData->state == STATE_ACKNOWLEDGED ? "Acknowledged" : "Active")
                         .arg(formatAlertPriority(alertData->priority));
    } else {
        confirmMsg = QString("Are you sure you want to resolve alert #%1?").arg(m_lastSelectedAlertId);
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Resolve Alert", confirmMsg,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        qDebug() << "[ALERT_PANEL] User cancelled alert resolution";
        return;
    }

    qDebug() << "[ALERT_PANEL] Resolving alert ID:" << m_lastSelectedAlertId;

    bool success = false;

    // PERBAIKAN: Update history DULU, baru AlertSystem
    for (int i = 0; i < m_alertHistory.size(); ++i) {
        if (m_alertHistory[i].id == m_lastSelectedAlertId) {
            m_alertHistory[i].state = STATE_RESOLVED;
            success = true;
            qDebug() << "[ALERT_PANEL] Updated alert state to RESOLVED in history";
            break;
        }
    }

    // Method 2: Resolve via AlertSystem (secondary)
    if (m_alertSystem) {
        bool systemSuccess = m_alertSystem->resolveAlert(m_lastSelectedAlertId);
        qDebug() << "[ALERT_PANEL] AlertSystem resolve result:" << systemSuccess;
        if (!success) success = systemSuccess;
    }

    if (success) {
        // PERBAIKAN: Force immediate refresh
        qDebug() << "[ALERT_PANEL] Forcing immediate UI refresh after resolve";

        // Debug before refresh
        debugAlertStates();

        // Update UI immediately
        updateAlertCounts();
        updateAlertList();

        // Force table repaint
        m_activeAlertsTable->viewport()->repaint();

        // Debug after refresh
        debugAlertStates();

        // Clear selection dan disable buttons
        m_activeAlertsTable->clearSelection();
        m_acknowledgeButton->setEnabled(false);
        m_resolveButton->setEnabled(false);
        m_alertDetailsText->setPlainText("Select an alert to view details...");

        int resolvedId = m_lastSelectedAlertId;
        m_lastSelectedAlertId = -1;

        // Emit signal
        emit alertResolved(resolvedId);

        // Show user feedback
        QString message = QString("Alert #%1 has been resolved and removed from active alerts").arg(resolvedId);
        QMessageBox::information(this, "Alert Resolved", message);

        qDebug() << "[ALERT_PANEL] Alert resolved successfully";

    } else {
        QString errorMsg = QString("Failed to resolve alert #%1").arg(m_lastSelectedAlertId);
        qWarning() << "[ALERT_PANEL]" << errorMsg;
        QMessageBox::critical(this, "Resolve Failed", errorMsg);
    }
}

void AlertPanel::clearAllAlerts()
{
    qDebug() << "[ALERT_PANEL] clearAllAlerts() called";

    // Hitung jumlah active alerts
    int activeCount = 0;
    if (m_alertSystem) {
        activeCount = m_alertSystem->getActiveAlerts().size();
    } else {
        for (const AlertData& alert : m_alertHistory) {
            if (alert.state == STATE_ACTIVE) {
                activeCount++;
            }
        }
    }

    if (activeCount == 0) {
        QMessageBox::information(this, "No Active Alerts", "There are no active alerts to clear.");
        return;
    }

    // Konfirmasi dari user
    QString confirmMsg = QString("Are you sure you want to clear ALL %1 active alerts?\n\n"
                                 "This will resolve all active alerts immediately.\n"
                                 "This action cannot be undone.").arg(activeCount);

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Clear All Alerts", confirmMsg,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        qDebug() << "[ALERT_PANEL] User cancelled clear all alerts";
        return;
    }

    bool success = false;

    // Method 1: Clear via AlertSystem
    if (m_alertSystem) {
        m_alertSystem->clearAllAlerts();
        success = true;
        qDebug() << "[ALERT_PANEL] Cleared all alerts via AlertSystem";
    }

    // Method 2: Update history (backup)
    for (int i = 0; i < m_alertHistory.size(); ++i) {
        if (m_alertHistory[i].state == STATE_ACTIVE) {
            m_alertHistory[i].state = STATE_RESOLVED;
        }
    }
    success = true;

    if (success) {
        // Update UI
        refreshAlertList();

        // Clear selection
        m_activeAlertsTable->clearSelection();
        m_acknowledgeButton->setEnabled(false);
        m_resolveButton->setEnabled(false);
        m_alertDetailsText->setPlainText("All alerts have been cleared.");
        m_lastSelectedAlertId = -1;

        // Show feedback
        QString message = QString("Successfully cleared %1 active alerts").arg(activeCount);
        QMessageBox::information(this, "Alerts Cleared", message);

        qDebug() << "[ALERT_PANEL] All alerts cleared successfully";
    }
}

// Utility methods
QString AlertPanel::formatAlertTime(const QDateTime& timestamp)
{
    return timestamp.toString("hh:mm:ss");
}

QString AlertPanel::formatAlertPriority(int priority)
{
    switch (priority) {
    case PRIORITY_LOW: return "Low";
    case PRIORITY_MEDIUM: return "Medium";
    case PRIORITY_HIGH: return "High";
    case PRIORITY_CRITICAL: return "Critical";
    default: return "Unknown";
    }
}

QString AlertPanel::formatAlertType(int type)
{
    switch (type) {
    case ALERT_GUARDZONE_PROXIMITY: return "GuardZone";
    case ALERT_DEPTH_SHALLOW: return "Shallow";
    case ALERT_DEPTH_DEEP: return "Deep";
    case ALERT_COLLISION_RISK: return "Collision";
    case ALERT_NAVIGATION_WARNING: return "Navigation";
    case ALERT_SYSTEM_ERR: return "System";
    case ALERT_USER_DEFINED: return "User";
    default: return "Unknown";
    }
}

QColor AlertPanel::getAlertPriorityColor(int priority)
{
    switch (priority) {
    case PRIORITY_LOW: return QColor(0, 150, 255, 100);      // Light blue
    case PRIORITY_MEDIUM: return QColor(255, 165, 0, 100);   // Light orange
    case PRIORITY_HIGH: return QColor(255, 69, 0, 100);      // Light red-orange
    case PRIORITY_CRITICAL: return QColor(255, 0, 0, 100);   // Light red
    default: return QColor(128, 128, 128, 100);              // Light gray
    }
}

QIcon AlertPanel::getAlertPriorityIcon(int priority)
{
    // Return appropriate icons based on priority
    // For now, return default icon
    return QIcon();
}

// Public interface methods
int AlertPanel::getActiveAlertCount() const
{
    if (!m_alertSystem) {
        return 0;
    }
    return m_alertSystem->getActiveAlerts().size();
}

int AlertPanel::getTotalAlertCount() const
{
    return m_alertHistory.size();
}

void AlertPanel::showAlertStatistics()
{
    QString stats;
    stats += QString("=== Alert Panel Statistics ===\n\n");

    if (m_alertSystem) {
        QList<AlertData> activeAlerts = m_alertSystem->getActiveAlerts();
        stats += QString("Active Alerts: %1\n").arg(activeAlerts.size());

        int criticalCount = 0, highCount = 0, mediumCount = 0, lowCount = 0;
        for (const AlertData& alert : activeAlerts) {
            switch (alert.priority) {
            case PRIORITY_CRITICAL: criticalCount++; break;
            case PRIORITY_HIGH: highCount++; break;
            case PRIORITY_MEDIUM: mediumCount++; break;
            case PRIORITY_LOW: lowCount++; break;
            }
        }

        stats += QString("  Critical: %1\n").arg(criticalCount);
        stats += QString("  High: %1\n").arg(highCount);
        stats += QString("  Medium: %1\n").arg(mediumCount);
        stats += QString("  Low: %1\n\n").arg(lowCount);
    }

    stats += QString("Total History: %1\n").arg(m_alertHistory.size());
    stats += QString("Max History: %1\n\n").arg(m_maxHistoryCount);

    stats += QString("Settings:\n");
    stats += QString("  Auto Refresh: %1\n").arg(m_autoRefreshEnabled ? "Yes" : "No");
    stats += QString("  Refresh Interval: %1s\n").arg(m_refreshInterval);
    stats += QString("  Sound Enabled: %1\n").arg(m_soundEnabled ? "Yes" : "No");
    stats += QString("  Auto Acknowledge Low: %1\n").arg(m_autoAcknowledgeLow ? "Yes" : "No");

    QMessageBox::information(this, "Alert Panel Statistics", stats);
}

bool AlertPanel::validatePanelState()
{
    bool isValid = true;
    QStringList issues;

    // Check alert system
    if (!m_alertSystem) {
        issues << "Alert System is null";
        isValid = false;
    }

    // Check UI components
    if (!m_activeAlertsTable) {
        issues << "Active alerts table is null";
        isValid = false;
    }

    if (!m_historyTable) {
        issues << "History table is null";
        isValid = false;
    }

    // Check timers
    if (!m_autoRefreshTimer) {
        issues << "Auto refresh timer is null";
        isValid = false;
    }

    if (!isValid) {
        qWarning() << "[ALERT_PANEL] Validation failed:" << issues;
    }

    return isValid;
}

void AlertPanel::recoverFromError()
{
    qDebug() << "[ALERT_PANEL] Attempting to recover from error";

    try {
        // Reset UI state
        m_lastSelectedAlertId = -1;

        if (m_activeAlertsTable) {
            m_activeAlertsTable->clearSelection();
            m_activeAlertsTable->setRowCount(0);
        }

        if (m_historyTable) {
            m_historyTable->clearSelection();
            m_historyTable->setRowCount(0);
        }

        if (m_alertDetailsText) {
            m_alertDetailsText->setPlainText("Select an alert to view details...");
        }

        // Disable buttons
        if (m_acknowledgeButton) m_acknowledgeButton->setEnabled(false);
        if (m_resolveButton) m_resolveButton->setEnabled(false);

        // Reset counters
        updateAlertCounts();
        updateSystemStatus();

        qDebug() << "[ALERT_PANEL] Recovery completed";

    } catch (const std::exception& e) {
        qCritical() << "[ALERT_PANEL] Recovery failed:" << e.what();
    } catch (...) {
        qCritical() << "[ALERT_PANEL] Recovery failed with unknown error";
    }
}

void AlertPanel::logError(const QString& error)
{
    m_lastError = error;
    qWarning() << "[ALERT_PANEL] Error:" << error;
    emit errorOccurred(error);
}

void AlertPanel::showError(const QString& title, const QString& message)
{
    QMessageBox::warning(this, title, message);
    logError(QString("%1: %2").arg(title, message));
}

bool AlertPanel::validateAlertSystem()
{
    if (!m_alertSystem) {
        logError("Alert System is not initialized");
        return false;
    }

    return true;
}

void AlertPanel::contextMenuEvent(QContextMenuEvent* event)
{
    // Handle context menu for the widget itself
    QWidget::contextMenuEvent(event);
}

void AlertPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // Refresh when panel becomes visible
    if (m_systemEnabled) {
        refreshAlertList();
    }
}

void AlertPanel::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);

    // Could pause auto refresh when hidden to save resources
    // (implementation depends on requirements)
}

void AlertPanel::setAlertSystem(AlertSystem* alertSystem)
{
    qDebug() << "[ALERT_PANEL] setAlertSystem called with:" << alertSystem;

    // Disconnect old connections if any
    if (m_alertSystem) {
        disconnect(m_alertSystem, nullptr, this, nullptr);
        qDebug() << "[ALERT_PANEL] Disconnected from old AlertSystem";
    }

    // Set new AlertSystem
    m_alertSystem = alertSystem;

    // Setup new connections
    if (m_alertSystem) {
        qDebug() << "[ALERT_PANEL] Setting up connections to new AlertSystem...";

        bool conn1 = connect(m_alertSystem, &AlertSystem::alertTriggered,
                             this, &AlertPanel::onAlertTriggered, Qt::QueuedConnection);
        bool conn2 = connect(m_alertSystem, &AlertSystem::systemStatusChanged,
                             this, &AlertPanel::onAlertSystemStatusChanged, Qt::QueuedConnection);

        qDebug() << "[ALERT_PANEL] New connections established:" << conn1 << conn2;

        m_systemEnabled = true;
        updateSystemStatus();
        refreshAlertList();

        qDebug() << "[ALERT_PANEL] AlertSystem reconnected successfully";
    } else {
        m_systemEnabled = false;
        updateSystemStatus();
        qDebug() << "[ALERT_PANEL] AlertSystem set to null - panel in limited mode";
    }
}

void AlertPanel::forceRefreshActiveAlerts()
{
    qDebug() << "[ALERT_PANEL] Force refresh active alerts called";

    // Debug info
    qDebug() << "[ALERT_PANEL] Alert history size:" << m_alertHistory.size();

    int activeCount = 0;
    for (const AlertData& alert : m_alertHistory) {
        if (alert.state == STATE_ACTIVE) {
            activeCount++;
            qDebug() << "[ALERT_PANEL] Active alert:" << alert.id << alert.title;
        }
    }

    qDebug() << "[ALERT_PANEL] Active alerts in history:" << activeCount;

    if (m_alertSystem) {
        QList<AlertData> systemAlerts = m_alertSystem->getActiveAlerts();
        qDebug() << "[ALERT_PANEL] Active alerts in system:" << systemAlerts.size();

        for (const AlertData& alert : systemAlerts) {
            qDebug() << "[ALERT_PANEL] System alert:" << alert.id << alert.title;
        }
    }

    // Force refresh
    updateAlertCounts();
    updateAlertList();

    // Force table repaint
    if (m_activeAlertsTable) {
        m_activeAlertsTable->clearSelection();
        m_activeAlertsTable->viewport()->update();
        m_activeAlertsTable->repaint();
    }
}

void AlertPanel::debugAlertStates()
{
    qDebug() << "=== ALERT STATES DEBUG ===";

    // Debug AlertSystem
    if (m_alertSystem) {
        QList<AlertData> systemAlerts = m_alertSystem->getActiveAlerts();
        qDebug() << "AlertSystem has" << systemAlerts.size() << "alerts:";
        for (const AlertData& alert : systemAlerts) {
            qDebug() << "  ID:" << alert.id << "Title:" << alert.title << "State:" << alert.state;
        }
    } else {
        qDebug() << "AlertSystem is NULL";
    }

    // Debug History
    qDebug() << "History has" << m_alertHistory.size() << "alerts:";
    for (const AlertData& alert : m_alertHistory) {
        qDebug() << "  ID:" << alert.id << "Title:" << alert.title << "State:" << alert.state;
    }

    // Debug Table
    qDebug() << "Table has" << m_activeAlertsTable->rowCount() << "rows:";
    for (int i = 0; i < m_activeAlertsTable->rowCount(); ++i) {
        QTableWidgetItem* timeItem = m_activeAlertsTable->item(i, 0);
        if (timeItem) {
            int alertId = timeItem->data(Qt::UserRole).toInt();
            QString title = m_activeAlertsTable->item(i, 3) ? m_activeAlertsTable->item(i, 3)->text() : "No title";
            qDebug() << "  Row" << i << "ID:" << alertId << "Title:" << title;
        }
    }
    qDebug() << "========================";
}

void AlertPanel::forceFullRefresh()
{
    qDebug() << "[ALERT_PANEL] Force full refresh called";

    // Debug current state
    debugAlertStates();

    // Clear table completely
    m_activeAlertsTable->setRowCount(0);
    m_activeAlertsTable->clearContents();

    // Force refresh
    updateAlertCounts();
    updateAlertList();

    // Force repaint
    m_activeAlertsTable->viewport()->repaint();
    update();

    qDebug() << "[ALERT_PANEL] Force full refresh completed";
}
