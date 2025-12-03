#include "aistargetpanel.h"
#include "ais.h"
#include <QSplitter>
#include <QFileDialog>
#include <QTextStream>
#include <QApplication>
#include <QClipboard>
#include <QDebug>

AISTargetPanel::AISTargetPanel(EcWidget* ecWidget, GuardZoneManager* gzManager, QWidget *parent)
    : QWidget(parent)
    , ecWidget(ecWidget)
    , guardZoneManager(gzManager)
    , totalTargets(0)
    , activeTargets(0)
    , enteredTargets(0)
    , exitedTargets(0)
{
    setupUI();
    
    // Setup timer untuk update otomatis
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &AISTargetPanel::updateTargetData);
    updateTimer->start(5000); // Update setiap 5 detik
    
    // Connect signals dari GuardZoneManager
    if (guardZoneManager) {
        connect(guardZoneManager, &GuardZoneManager::guardZoneAlert, 
                this, &AISTargetPanel::onGuardZoneAlert);
    }
    
    qDebug() << "AISTargetPanel initialized successfully";
}

AISTargetPanel::~AISTargetPanel()
{
    if (updateTimer) {
        updateTimer->stop();
    }
}


void AISTargetPanel::setupUI()
{
    setWindowTitle("AIS Target Manager");
    resize(800, 600);
    
    mainLayout = new QVBoxLayout(this);
    
    // ========== FILTER CONTROLS ==========
    filterGroup = new QGroupBox("Filters", this);
    filterLayout = new QHBoxLayout(filterGroup);
    
    // GuardZone Filter
    guardZoneLabel = new QLabel("GuardZone:");
    guardZoneFilter = new QComboBox();
    guardZoneFilter->addItem("All GuardZones", -1);
    
    // Event Type Filter
    eventTypeLabel = new QLabel("Event:");
    eventTypeFilter = new QComboBox();
    eventTypeFilter->addItem("All Events", "ALL");
    eventTypeFilter->addItem("Entered", "ENTERED");
    eventTypeFilter->addItem("Exited", "EXITED");
    
    // Status Filter
    statusLabel = new QLabel("Status:");
    statusFilter = new QComboBox();
    statusFilter->addItem("All Status", "ALL");
    statusFilter->addItem("Active", "ACTIVE");
    statusFilter->addItem("Cleared", "CLEARED");
    
    // Search Filter
    searchFilter = new QLineEdit();
    searchFilter->setPlaceholderText("Search by MMSI or Ship Name...");
    
    filterLayout->addWidget(guardZoneLabel);
    filterLayout->addWidget(guardZoneFilter);
    filterLayout->addWidget(eventTypeLabel);
    filterLayout->addWidget(eventTypeFilter);
    filterLayout->addWidget(statusLabel);
    filterLayout->addWidget(statusFilter);
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
    
    // ========== TARGET LIST ==========
    targetList = new QTreeWidget(this);
    targetList->setHeaderLabels({
        "MMSI", "Ship Name", "Ship Type", "GuardZone", 
        "Event", "Timestamp", "Position", "SOG", "COG", "Status"
    });
    
    // Setup columns
    targetList->setColumnWidth(0, 80);   // MMSI
    targetList->setColumnWidth(1, 120);  // Ship Name
    targetList->setColumnWidth(2, 100);  // Ship Type
    targetList->setColumnWidth(3, 100);  // GuardZone
    targetList->setColumnWidth(4, 80);   // Event
    targetList->setColumnWidth(5, 150);  // Timestamp
    targetList->setColumnWidth(6, 150);  // Position
    targetList->setColumnWidth(7, 60);   // SOG
    targetList->setColumnWidth(8, 60);   // COG
    targetList->setColumnWidth(9, 80);   // Status
    
    targetList->setAlternatingRowColors(true);
    targetList->setSortingEnabled(true);
    targetList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    targetList->setContextMenuPolicy(Qt::CustomContextMenu);
    
    mainLayout->addWidget(targetList);
    
    // ========== STATISTICS ==========
    statisticsLabel = new QLabel("Statistics: 0 total targets");
    mainLayout->addWidget(statisticsLabel);
    
    // ========== CONTEXT MENU ==========
    contextMenu = new QMenu(this);
    
    clearTargetAction = contextMenu->addAction("Clear Selected Target");
    clearTargetAction->setIcon(QIcon(":/images/delete.png"));
    
    clearGuardZoneAction = contextMenu->addAction("Clear All Targets for GuardZone");
    clearGuardZoneAction->setIcon(QIcon(":/images/clear.png"));
    
    contextMenu->addSeparator();
    
    centerMapAction = contextMenu->addAction("Center Map on Target");
    centerMapAction->setIcon(QIcon(":/images/center.png"));
    
    exportAction = contextMenu->addAction("Export Selected");
    exportAction->setIcon(QIcon(":/images/export.png"));
    
    // ========== SIGNAL CONNECTIONS ==========
    connect(guardZoneFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AISTargetPanel::onFilterChanged);
    connect(eventTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AISTargetPanel::onFilterChanged);
    connect(statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AISTargetPanel::onFilterChanged);
    connect(searchFilter, &QLineEdit::textChanged, this, &AISTargetPanel::onFilterChanged);
    
    connect(targetList, &QTreeWidget::itemSelectionChanged, 
            this, &AISTargetPanel::onTargetSelectionChanged);
    connect(targetList, &QTreeWidget::customContextMenuRequested, 
            this, &AISTargetPanel::showContextMenu);
    
    connect(refreshButton, &QPushButton::clicked, this, &AISTargetPanel::refreshTargetList);
    connect(clearAllButton, &QPushButton::clicked, this, &AISTargetPanel::clearAllTargets);
    connect(exportButton, &QPushButton::clicked, this, &AISTargetPanel::exportTargetList);
    
    connect(clearTargetAction, &QAction::triggered, this, &AISTargetPanel::clearSelectedTarget);
    connect(clearGuardZoneAction, &QAction::triggered, this, &AISTargetPanel::clearTargetsForGuardZone);
    connect(centerMapAction, &QAction::triggered, this, &AISTargetPanel::centerMapOnTarget);
    connect(exportAction, &QAction::triggered, this, &AISTargetPanel::exportTargetList);
    
    // Populate initial data
    populateGuardZoneFilter();
}

void AISTargetPanel::populateGuardZoneFilter()
{
    if (!ecWidget) return;
    
    guardZoneFilter->clear();
    guardZoneFilter->addItem("All GuardZones", -1);
    
    const QList<GuardZone>& guardZones = ecWidget->getGuardZones();
    for (const GuardZone& gz : guardZones) {
        guardZoneFilter->addItem(QString("%1 (ID: %2)").arg(gz.name).arg(gz.id), gz.id);
    }
}

void AISTargetPanel::onGuardZoneAlert(int guardZoneId, int mmsi, const QString& message)
{
    qDebug() << "AISTargetPanel received alert - GuardZone:" << guardZoneId << "MMSI:" << mmsi << "Message:" << message;
    qDebug() << "AISTargetPanel: Processing alert and adding to list...";
    
    // Parse alert message untuk menentukan event type
    QString eventType = "UNKNOWN";
    if (message.contains("entered", Qt::CaseInsensitive)) {
        eventType = "ENTERED";
    } else if (message.contains("exited", Qt::CaseInsensitive)) {
        eventType = "EXITED";
    }
    
    // Cari info guardzone
    QString guardZoneName = QString("GuardZone_%1").arg(guardZoneId);
    if (ecWidget) {
        const QList<GuardZone>& guardZones = ecWidget->getGuardZones();
        for (const GuardZone& gz : guardZones) {
            if (gz.id == guardZoneId) {
                guardZoneName = gz.name;
                break;
            }
        }
    }
    
    // Cari data AIS target
    QString shipName = QString::number(mmsi);
    QString shipType = "Unknown";
    double lat = 0.0, lon = 0.0, sog = 0.0, cog = 0.0;
    
    if (Ais::instance()) {
        QMap<unsigned int, EcAISTargetInfo>& targetInfoMap = Ais::instance()->getTargetInfoMap();
        if (targetInfoMap.contains(mmsi)) {
            EcAISTargetInfo& targetInfo = targetInfoMap[mmsi];
            if (!QString(targetInfo.shipName).isEmpty()) {
                shipName = QString(targetInfo.shipName);
            }
            
            if (guardZoneManager) {
                ShipTypeFilter shipTypeEnum = guardZoneManager->getShipTypeFromAIS(targetInfo.shipType);
                shipType = guardZoneManager->getShipTypeDisplayName(shipTypeEnum);
            }
            
            // Convert koordinat dari AIS format
            lat = ((double)targetInfo.latitude / 10000.0) / 60.0;
            lon = ((double)targetInfo.longitude / 10000.0) / 60.0;
            sog = targetInfo.sog / 10.0;  // AIS SOG dalam 0.1 knot units
            cog = targetInfo.cog / 10.0;  // AIS COG dalam 0.1 degree units
        }
    }
    
    // Buat detection record
    AISTargetDetection detection;
    detection.mmsi = mmsi;
    detection.shipName = shipName;
    detection.shipType = shipType;
    detection.guardZoneId = guardZoneId;
    detection.guardZoneName = guardZoneName;
    detection.eventType = eventType;
    detection.timestamp = QDateTime::currentDateTime();
    detection.lat = lat;
    detection.lon = lon;
    detection.sog = sog;
    detection.cog = cog;
    detection.status = "ACTIVE";
    
    // Tambahkan ke list
    detectedTargets.append(detection);
    addTargetToList(detection);
    
    // Update statistics
    updateStatistics();
    
    qDebug() << "Added target detection:" << shipName << "(" << mmsi << ")" << eventType << "GuardZone" << guardZoneName;
}

void AISTargetPanel::addTargetToList(const AISTargetDetection& detection)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(targetList);
    
    item->setText(0, QString::number(detection.mmsi));
    item->setText(1, detection.shipName);
    item->setText(2, detection.shipType);
    item->setText(3, detection.guardZoneName);
    item->setText(4, formatEventType(detection.eventType));
    item->setText(5, formatTimestamp(detection.timestamp));
    item->setText(6, QString("%1, %2").arg(detection.lat, 0, 'f', 6).arg(detection.lon, 0, 'f', 6));
    item->setText(7, QString("%1 kts").arg(detection.sog, 0, 'f', 1));
    item->setText(8, QString("%1°").arg(detection.cog, 0, 'f', 1));
    item->setText(9, detection.status);
    
    // Set colors based on event type
    if (detection.eventType == "ENTERED") {
        item->setBackground(4, QColor(144, 238, 144)); // Light green
    } else if (detection.eventType == "EXITED") {
        item->setBackground(4, QColor(255, 182, 193)); // Light pink
    }
    
    // Set status color
    if (detection.status == "ACTIVE") {
        item->setBackground(9, QColor(173, 216, 230)); // Light blue
    } else {
        item->setBackground(9, QColor(211, 211, 211)); // Light gray
    }
    
    // Store detection data in item
    QVariant detectionVariant;
    detectionVariant.setValue(detection);
    item->setData(0, Qt::UserRole, detectionVariant);
    
    targetList->addTopLevelItem(item);
    applyFilters();
}


QString AISTargetPanel::formatEventType(const QString& eventType)
{
    if (eventType == "ENTERED") return "→ IN";
    if (eventType == "EXITED") return "← OUT";
    return eventType;
}

QString AISTargetPanel::formatTimestamp(const QDateTime& timestamp)
{
    return timestamp.toString("yyyy-MM-dd hh:mm:ss");
}

void AISTargetPanel::onFilterChanged()
{
    applyFilters();
}

void AISTargetPanel::applyFilters()
{
    int selectedGuardZoneId = guardZoneFilter->currentData().toInt();
    QString selectedEventType = eventTypeFilter->currentData().toString();
    QString selectedStatus = statusFilter->currentData().toString();
    QString searchText = searchFilter->text().toLower();
    
    for (int i = 0; i < targetList->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = targetList->topLevelItem(i);
        bool visible = true;
        
        AISTargetDetection detection = item->data(0, Qt::UserRole).value<AISTargetDetection>();
        
        // Filter by GuardZone
        if (selectedGuardZoneId != -1 && detection.guardZoneId != selectedGuardZoneId) {
            visible = false;
        }
        
        // Filter by Event Type
        if (selectedEventType != "ALL" && detection.eventType != selectedEventType) {
            visible = false;
        }
        
        // Filter by Status
        if (selectedStatus != "ALL" && detection.status != selectedStatus) {
            visible = false;
        }
        
        // Filter by Search Text
        if (!searchText.isEmpty()) {
            QString mmsiText = QString::number(detection.mmsi);
            QString shipNameText = detection.shipName.toLower();
            
            if (!mmsiText.contains(searchText) && !shipNameText.contains(searchText)) {
                visible = false;
            }
        }
        
        item->setHidden(!visible);
    }
}

void AISTargetPanel::updateStatistics()
{
    totalTargets = detectedTargets.size();
    activeTargets = 0;
    enteredTargets = 0;
    exitedTargets = 0;
    
    for (const AISTargetDetection& detection : detectedTargets) {
        if (detection.status == "ACTIVE") activeTargets++;
        if (detection.eventType == "ENTERED") enteredTargets++;
        if (detection.eventType == "EXITED") exitedTargets++;
    }
    
    statisticsLabel->setText(QString("Statistics: %1 total | %2 active | %3 entered | %4 exited")
                           .arg(totalTargets)
                           .arg(activeTargets)
                           .arg(enteredTargets)
                           .arg(exitedTargets));
}

void AISTargetPanel::refreshTargetList()
{
    populateGuardZoneFilter();
    applyFilters();
    updateStatistics();
    qDebug() << "AISTargetPanel refreshed";
}

void AISTargetPanel::clearAllTargets()
{
    int ret = QMessageBox::question(this, "Clear All Targets",
                                   "Are you sure you want to clear all target detections?",
                                   QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        detectedTargets.clear();
        targetList->clear();
        updateStatistics();
        qDebug() << "All target detections cleared";
    }
}

void AISTargetPanel::clearAllTargetsForce()
{
    // Clear without confirmation dialog
    detectedTargets.clear();
    targetList->clear();
    updateStatistics();
    qDebug() << "All target detections force cleared";
}

void AISTargetPanel::onTargetSelectionChanged()
{
    // Enable/disable context menu actions based on selection
    bool hasSelection = !targetList->selectedItems().isEmpty();
    
    clearTargetAction->setEnabled(hasSelection);
    centerMapAction->setEnabled(hasSelection);
    exportAction->setEnabled(hasSelection);
}

void AISTargetPanel::showContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = targetList->itemAt(pos);
    if (item) {
        contextMenu->exec(targetList->mapToGlobal(pos));
    }
}

void AISTargetPanel::clearSelectedTarget()
{
    QList<QTreeWidgetItem*> selectedItems = targetList->selectedItems();
    
    for (QTreeWidgetItem* item : selectedItems) {
        AISTargetDetection detection = item->data(0, Qt::UserRole).value<AISTargetDetection>();
        
        // Remove from data list
        for (int i = 0; i < detectedTargets.size(); ++i) {
            if (detectedTargets[i].mmsi == detection.mmsi && 
                detectedTargets[i].guardZoneId == detection.guardZoneId &&
                detectedTargets[i].timestamp == detection.timestamp) {
                detectedTargets.removeAt(i);
                break;
            }
        }
        
        // Remove from tree
        delete item;
    }
    
    updateStatistics();
}

void AISTargetPanel::clearTargetsForGuardZone()
{
    QTreeWidgetItem* currentItem = targetList->currentItem();
    if (!currentItem) return;
    
    AISTargetDetection detection = currentItem->data(0, Qt::UserRole).value<AISTargetDetection>();
    
    int ret = QMessageBox::question(this, "Clear GuardZone Targets", 
                                   QString("Clear all targets for GuardZone '%1'?").arg(detection.guardZoneName),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // Remove from data list
        for (int i = detectedTargets.size() - 1; i >= 0; --i) {
            if (detectedTargets[i].guardZoneId == detection.guardZoneId) {
                detectedTargets.removeAt(i);
            }
        }
        
        // Remove from tree
        for (int i = targetList->topLevelItemCount() - 1; i >= 0; --i) {
            QTreeWidgetItem* item = targetList->topLevelItem(i);
            AISTargetDetection itemDetection = item->data(0, Qt::UserRole).value<AISTargetDetection>();
            if (itemDetection.guardZoneId == detection.guardZoneId) {
                delete item;
            }
        }
        
        updateStatistics();
    }
}

void AISTargetPanel::centerMapOnTarget()
{
    QTreeWidgetItem* currentItem = targetList->currentItem();
    if (!currentItem || !ecWidget) return;
    
    AISTargetDetection detection = currentItem->data(0, Qt::UserRole).value<AISTargetDetection>();
    
    if (detection.lat != 0.0 && detection.lon != 0.0) {
        // Center map pada posisi target
        ecWidget->SetCenter(detection.lat, detection.lon);
        qDebug() << "Centered map on target" << detection.mmsi << "at" << detection.lat << "," << detection.lon;
    }
}

void AISTargetPanel::exportTargetList()
{
    QString fileName = QFileDialog::getSaveFileName(this, 
                                                   "Export AIS Target List", 
                                                   QString("ais_targets_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
                                                   "CSV Files (*.csv)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Error", "Could not open file for writing.");
        return;
    }
    
    QTextStream out(&file);
    
    // Header
    out << "MMSI,Ship Name,Ship Type,GuardZone,Event,Timestamp,Latitude,Longitude,SOG,COG,Status\n";
    
    // Data
    QList<QTreeWidgetItem*> itemsToExport;
    if (targetList->selectedItems().isEmpty()) {
        // Export all visible items
        for (int i = 0; i < targetList->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = targetList->topLevelItem(i);
            if (!item->isHidden()) {
                itemsToExport.append(item);
            }
        }
    } else {
        // Export selected items
        itemsToExport = targetList->selectedItems();
    }
    
    for (QTreeWidgetItem* item : itemsToExport) {
        AISTargetDetection detection = item->data(0, Qt::UserRole).value<AISTargetDetection>();
        
        out << detection.mmsi << ","
            << "\"" << detection.shipName << "\","
            << "\"" << detection.shipType << "\","
            << "\"" << detection.guardZoneName << "\","
            << detection.eventType << ","
            << detection.timestamp.toString("yyyy-MM-dd hh:mm:ss") << ","
            << QString::number(detection.lat, 'f', 6) << ","
            << QString::number(detection.lon, 'f', 6) << ","
            << QString::number(detection.sog, 'f', 1) << ","
            << QString::number(detection.cog, 'f', 1) << ","
            << detection.status << "\n";
    }
    
    file.close();
    
    QMessageBox::information(this, "Export Complete", 
                            QString("Exported %1 records to %2").arg(itemsToExport.size()).arg(fileName));
}

void AISTargetPanel::updateTargetData()
{
    // Update existing targets with latest AIS data if available
    // This could be expanded to update positions, speeds, etc.
}

void AISTargetPanel::onTargetEntered(int guardZoneId, int mmsi, const QString& details)
{
    // Alternative slot for more specific entered events
    qDebug() << "Target entered - GuardZone:" << guardZoneId << "MMSI:" << mmsi << "Details:" << details;
}

void AISTargetPanel::onTargetExited(int guardZoneId, int mmsi, const QString& details)
{
    // Alternative slot for more specific exited events
    qDebug() << "Target exited - GuardZone:" << guardZoneId << "MMSI:" << mmsi << "Details:" << details;
}

