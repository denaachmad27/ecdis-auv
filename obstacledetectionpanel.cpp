#include "obstacledetectionpanel.h"
#include <QSplitter>
#include <QFileDialog>
#include <QTextStream>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <algorithm>

ObstacleDetectionPanel::ObstacleDetectionPanel(EcWidget* ecWidget, GuardZoneManager* gzManager, QWidget *parent)
    : QWidget(parent)
    , ecWidget(ecWidget)
    , guardZoneManager(gzManager)
    , totalObstacles(0)
    , activeObstacles(0)
    , dangerousObstacles(0)
    , warningObstacles(0)
    , noteObstacles(0)
{
    setupUI();
    
    // Setup timer untuk update otomatis
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &ObstacleDetectionPanel::updateObstacleData);
    updateTimer->start(10000); // Update setiap 10 detik
    
    qDebug() << "ObstacleDetectionPanel initialized successfully";
}

ObstacleDetectionPanel::~ObstacleDetectionPanel()
{
    if (updateTimer) {
        updateTimer->stop();
    }
}

void ObstacleDetectionPanel::addPickReportObstacle(const PickReportObstacle& obstacle)
{
    // Check for duplicates using object name, type, and position
    QString obstacleKey = QString("%1_%2_%3_%4")
                         .arg(obstacle.objectName)
                         .arg(obstacle.objectType)
                         .arg(obstacle.lat, 0, 'f', 4)
                         .arg(obstacle.lon, 0, 'f', 4);
    
    // Check if this obstacle already exists
    for (const PickReportObstacle& existing : detectedObstacles) {
        QString existingKey = QString("%1_%2_%3_%4")
                             .arg(existing.objectName)
                             .arg(existing.objectType)
                             .arg(existing.lat, 0, 'f', 4)
                             .arg(existing.lon, 0, 'f', 4);
        
        if (existingKey == obstacleKey) {
            qDebug() << "Duplicate obstacle detected, skipping:" << obstacle.objectName;
            return; // Skip duplicate
        }
    }
    
    detectedObstacles.append(obstacle);
    addObstacleToList(obstacle);
    updateStatistics();
    qDebug() << "Added pick report obstacle:" << obstacle.objectName;
}

void ObstacleDetectionPanel::clearAllObstacles()
{
    obstacleList->clear();
    detectedObstacles.clear();
    updateStatistics();
    qDebug() << "Cleared all obstacles";
}

void ObstacleDetectionPanel::setupUI()
{
    setWindowTitle("Obstacle Detection Panel");
    resize(900, 600);
    
    mainLayout = new QVBoxLayout(this);
    
    // ========== FILTER CONTROLS ==========
    filterGroup = new QGroupBox("Filters", this);
    filterLayout = new QHBoxLayout(filterGroup);
    
    // Event Type Filter
    eventTypeLabel = new QLabel("Event:");
    eventTypeFilter = new QComboBox();
    eventTypeFilter->addItem("All Events", "ALL");
    eventTypeFilter->addItem("Detected", "DETECTED");
    eventTypeFilter->addItem("Cleared", "CLEARED");
    
    // Danger Level Filter (old dropdown - keep for compatibility)
    dangerLevelLabel = new QLabel("Danger Level:");
    dangerLevelFilter = new QComboBox();
    dangerLevelFilter->addItem("All Levels", "ALL");
    dangerLevelFilter->addItem("Dangerous", "DANGEROUS");
    dangerLevelFilter->addItem("Warning", "WARNING");
    dangerLevelFilter->addItem("Note", "NOTE");
    
    // Danger Level Checkboxes (new filter method)
    dangerCheckboxLabel = new QLabel("Show:");
    dangerousCheckbox = new QCheckBox("Dangerous");
    dangerousCheckbox->setChecked(true);
    dangerousCheckbox->setStyleSheet("QCheckBox { color: red; font-weight: bold; }");
    
    warningCheckbox = new QCheckBox("Warning");
    warningCheckbox->setChecked(true);
    warningCheckbox->setStyleSheet("QCheckBox { color: orange; font-weight: bold; }");
    
    noteCheckbox = new QCheckBox("Note");
    noteCheckbox->setChecked(true);
    noteCheckbox->setStyleSheet("QCheckBox { color: blue; font-weight: bold; }");
    
    // Search Filter
    searchFilter = new QLineEdit();
    searchFilter->setPlaceholderText("Search by Object Type or Name...");
    
    filterLayout->addWidget(eventTypeLabel);
    filterLayout->addWidget(eventTypeFilter);
    filterLayout->addWidget(dangerLevelLabel);
    filterLayout->addWidget(dangerLevelFilter);
    
    filterLayout->addWidget(new QLabel("|")); // Separator
    filterLayout->addWidget(dangerCheckboxLabel);
    filterLayout->addWidget(dangerousCheckbox);
    filterLayout->addWidget(warningCheckbox);
    filterLayout->addWidget(noteCheckbox);
    
    filterLayout->addWidget(new QLabel("Search:"));
    filterLayout->addWidget(searchFilter);
    filterLayout->addStretch();
    
    mainLayout->addWidget(filterGroup);
    
    // ========== CONTROL BUTTONS ==========
    controlLayout = new QHBoxLayout();
    
    refreshButton = new QPushButton("Refresh");
    refreshButton->setIcon(QIcon(":/images/refresh.png"));
    
    clearAllButton = new QPushButton("Clear All");
    clearAllButton->setIcon(QIcon(":/images/clear.png"));
    
    exportButton = new QPushButton("Export");
    exportButton->setIcon(QIcon(":/images/export.png"));
    
    controlLayout->addWidget(refreshButton);
    controlLayout->addWidget(clearAllButton);
    controlLayout->addWidget(exportButton);
    controlLayout->addStretch();
    
    mainLayout->addLayout(controlLayout);
    
    // ========== OBSTACLE LIST ==========
    obstacleList = new CustomObstacleTreeWidget(this);
    obstacleList->setHeaderLabels({
        "Danger Level", "Object Name", "Information", "Timestamp", "Position"
    });
    
    // Setup columns
    obstacleList->setColumnWidth(0, 120);  // Danger Level
    obstacleList->setColumnWidth(1, 150);  // Object Name
    obstacleList->setColumnWidth(2, 250);  // Information
    obstacleList->setColumnWidth(3, 150);  // Timestamp
    obstacleList->setColumnWidth(4, 200);  // Position
    
    obstacleList->setAlternatingRowColors(true);
    obstacleList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    obstacleList->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Set custom delegate for danger level column to hide prefix
    obstacleList->setItemDelegateForColumn(0, new DangerLevelDelegate(this));
    
    // Enable sorting and set default sort by danger level
    obstacleList->setSortingEnabled(true);
    obstacleList->sortByColumn(0, Qt::AscendingOrder); // A_DANGEROUS comes first
    
    mainLayout->addWidget(obstacleList);
    
    // ========== STATISTICS ==========
    statisticsLabel = new QLabel("Statistics: 0 total obstacles");
    mainLayout->addWidget(statisticsLabel);
    
    // ========== CONTEXT MENU ==========
    contextMenu = new QMenu(this);
    
    clearObstacleAction = contextMenu->addAction("Clear Selected Obstacle");
    clearObstacleAction->setIcon(QIcon(":/images/delete.png"));
    
    clearGuardZoneAction = contextMenu->addAction("Clear All Obstacles for GuardZone");
    clearGuardZoneAction->setIcon(QIcon(":/images/clear.png"));
    
    contextMenu->addSeparator();
    
    centerMapAction = contextMenu->addAction("Center Map on Obstacle");
    centerMapAction->setIcon(QIcon(":/images/center.png"));
    
    exportAction = contextMenu->addAction("Export Obstacle Data");
    exportAction->setIcon(QIcon(":/images/export.png"));
    
    // ========== SIGNAL CONNECTIONS ==========
    connect(eventTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ObstacleDetectionPanel::onFilterChanged);
    connect(dangerLevelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ObstacleDetectionPanel::onFilterChanged);
    connect(searchFilter, &QLineEdit::textChanged, this, &ObstacleDetectionPanel::onFilterChanged);
    
    // Connect checkbox filters
    connect(dangerousCheckbox, &QCheckBox::toggled, this, &ObstacleDetectionPanel::onFilterChanged);
    connect(warningCheckbox, &QCheckBox::toggled, this, &ObstacleDetectionPanel::onFilterChanged);
    connect(noteCheckbox, &QCheckBox::toggled, this, &ObstacleDetectionPanel::onFilterChanged);
    
    connect(obstacleList, &QTreeWidget::itemSelectionChanged, this, &ObstacleDetectionPanel::onObstacleSelectionChanged);
    connect(obstacleList, &QTreeWidget::customContextMenuRequested, this, &ObstacleDetectionPanel::showContextMenu);
    
    connect(refreshButton, &QPushButton::clicked, this, &ObstacleDetectionPanel::refreshObstacleList);
    connect(clearAllButton, &QPushButton::clicked, this, &ObstacleDetectionPanel::clearAllObstacles);
    connect(exportButton, &QPushButton::clicked, this, &ObstacleDetectionPanel::exportObstacleList);
    
    connect(clearObstacleAction, &QAction::triggered, this, &ObstacleDetectionPanel::clearSelectedObstacle);
    connect(clearGuardZoneAction, &QAction::triggered, this, &ObstacleDetectionPanel::clearObstaclesForGuardZone);
    connect(centerMapAction, &QAction::triggered, this, &ObstacleDetectionPanel::centerMapOnObstacle);
    connect(exportAction, &QAction::triggered, this, &ObstacleDetectionPanel::exportObstacleList);
}

void ObstacleDetectionPanel::addObstacleToList(const PickReportObstacle& obstacle)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(obstacleList);
    
    // New column order: Danger Level, Object Name, Information, Timestamp, Position
    item->setText(0, obstacle.dangerLevel);                                                    // Danger Level
    item->setText(1, obstacle.objectName);                                                     // Object Name
    item->setText(2, obstacle.information);                                                    // Information
    item->setText(3, formatTimestamp(obstacle.timestamp));                                     // Timestamp
    item->setText(4, QString("%1, %2").arg(obstacle.lat, 0, 'f', 6).arg(obstacle.lon, 0, 'f', 6)); // Position
    
    // Set colors and text based on danger level (now column 0)
    if (obstacle.dangerLevel == "DANGEROUS") {
        item->setBackground(0, QColor(255, 99, 71)); // Tomato red
        item->setForeground(0, QColor(255, 255, 255)); // White text
        item->setText(0, "A_DANGEROUS"); // Use A prefix for sorting (A comes first)
        item->setData(0, Qt::UserRole, "DANGEROUS"); // Store original value
    } else if (obstacle.dangerLevel == "WARNING") {
        item->setBackground(0, QColor(255, 215, 0)); // Gold yellow
        item->setForeground(0, QColor(0, 0, 0)); // Black text
        item->setText(0, "B_WARNING"); // Use B prefix for sorting
        item->setData(0, Qt::UserRole, "WARNING"); // Store original value
    } else { // NOTE
        item->setBackground(0, QColor(173, 216, 230)); // Light blue
        item->setForeground(0, QColor(0, 0, 0)); // Black text
        item->setText(0, "C_NOTE"); // Use C prefix for sorting (C comes last)
        item->setData(0, Qt::UserRole, "NOTE"); // Store original value
    }
    
    // Store obstacle data in item
    QVariant obstacleVariant;
    obstacleVariant.setValue(obstacle);
    item->setData(1, Qt::UserRole, obstacleVariant); // Store in Object Name column
    
    obstacleList->addTopLevelItem(item);
    
    // Apply sorting after adding new item
    obstacleList->sortByColumn(0, Qt::AscendingOrder);
    
    applyFilters();
    
    qDebug() << "ObstacleDetectionPanel: Added obstacle to tree widget. Total items now:" << obstacleList->topLevelItemCount();
}

void ObstacleDetectionPanel::onPickReportObstacleDetected(int guardZoneId, const QString& details)
{
    qDebug() << "Obstacle Detection Panel: New obstacle detected - Details:" << details;
    
    // Parse the details string: "PICK_REPORT|objectType|objectName|featureClass|lat|lon|dangerLevel|information"
    QStringList parts = details.split("|");
    if (parts.size() >= 7 && parts[0] == "PICK_REPORT") {
        PickReportObstacle obstacle;
        obstacle.objectType = parts[1];
        obstacle.objectName = parts[2];
        obstacle.featureClass = parts[3];
        obstacle.lat = parts[4].toDouble();
        obstacle.lon = parts[5].toDouble();
        obstacle.dangerLevel = parts[6];
        obstacle.information = parts.size() > 7 ? parts[7] : "";
        obstacle.attributes = "";
        
        // Set GuardZone info
        obstacle.guardZoneId = guardZoneId;
        obstacle.guardZoneName = guardZoneId == 0 ? "Ship Guardian" : QString("GuardZone %1").arg(guardZoneId);
        obstacle.eventType = "DETECTED";
        obstacle.timestamp = QDateTime::currentDateTime();
        obstacle.status = "ACTIVE";
        
        // Add to the list
        addPickReportObstacle(obstacle);
        
        qDebug() << "Added pick report obstacle:" << obstacle.objectName << "Level:" << obstacle.dangerLevel;
    } else {
        qDebug() << "Invalid pick report obstacle data format:" << details;
    }
}

QString ObstacleDetectionPanel::formatEventType(const QString& eventType)
{
    if (eventType == "DETECTED") return "🟢 Detected";
    if (eventType == "CLEARED") return "🔴 Cleared";
    return eventType;
}

QString ObstacleDetectionPanel::formatTimestamp(const QDateTime& timestamp)
{
    return timestamp.toString("yyyy-MM-dd hh:mm:ss");
}

void ObstacleDetectionPanel::updateStatistics()
{
    totalObstacles = detectedObstacles.size();
    activeObstacles = 0;
    dangerousObstacles = 0;
    warningObstacles = 0;
    noteObstacles = 0;
    
    for (const PickReportObstacle& obstacle : detectedObstacles) {
        if (obstacle.status == "ACTIVE") {
            activeObstacles++;
        }
        
        if (obstacle.dangerLevel == "DANGEROUS") {
            dangerousObstacles++;
        } else if (obstacle.dangerLevel == "WARNING") {
            warningObstacles++;
        } else {
            noteObstacles++;
        }
    }
    
    QString stats = QString("Statistics: %1 total obstacles (%2 active) - 🔴%3 dangerous, 🟡%4 warnings, 🔵%5 notes")
                   .arg(totalObstacles)
                   .arg(activeObstacles)
                   .arg(dangerousObstacles)
                   .arg(warningObstacles)
                   .arg(noteObstacles);
    
    statisticsLabel->setText(stats);
}

// Stub implementations for required slots
void ObstacleDetectionPanel::refreshObstacleList() { updateStatistics(); }
void ObstacleDetectionPanel::updateObstacleData() { /* Auto refresh logic */ }
void ObstacleDetectionPanel::onFilterChanged() { applyFilters(); }
void ObstacleDetectionPanel::onObstacleSelectionChanged() { /* Selection logic */ }
void ObstacleDetectionPanel::showContextMenu(const QPoint& pos) { contextMenu->exec(obstacleList->mapToGlobal(pos)); }
void ObstacleDetectionPanel::clearSelectedObstacle() { /* Clear selected */ }
void ObstacleDetectionPanel::clearObstaclesForGuardZone() { /* Clear by guardzone */ }
void ObstacleDetectionPanel::exportObstacleList() { /* Export logic */ }
void ObstacleDetectionPanel::centerMapOnObstacle() { /* Center map */ }
void ObstacleDetectionPanel::updateObstacleInList(const QString&, const QString&) { /* Update logic */ }
QTreeWidgetItem* ObstacleDetectionPanel::findObstacleItem(const QString&) { return nullptr; }

void ObstacleDetectionPanel::applyFilters()
{
    QString searchText = searchFilter->text().toLower();
    bool showDangerous = dangerousCheckbox->isChecked();
    bool showWarning = warningCheckbox->isChecked();
    bool showNote = noteCheckbox->isChecked();
    
    for (int i = 0; i < obstacleList->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = obstacleList->topLevelItem(i);
        bool shouldShow = true;
        
        // Get danger level from UserRole (original value without prefix)
        QString dangerLevel = item->data(0, Qt::UserRole).toString();
        QString objectName = item->text(1).toLower();
        QString information = item->text(2).toLower();
        
        // Apply danger level filter
        if (dangerLevel == "DANGEROUS" && !showDangerous) {
            shouldShow = false;
        } else if (dangerLevel == "WARNING" && !showWarning) {
            shouldShow = false;
        } else if (dangerLevel == "NOTE" && !showNote) {
            shouldShow = false;
        }
        
        // Apply search filter
        if (shouldShow && !searchText.isEmpty()) {
            if (!objectName.contains(searchText) && !information.contains(searchText)) {
                shouldShow = false;
            }
        }
        
        item->setHidden(!shouldShow);
    }
    
    qDebug() << "Applied filters: DANGEROUS=" << showDangerous 
             << "WARNING=" << showWarning << "NOTE=" << showNote 
             << "Search='" << searchText << "'";
}