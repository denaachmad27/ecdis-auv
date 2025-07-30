#include "routepanel.h"
#include "ecwidget.h"
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QtMath>

// ====== RouteListItem Implementation ======

RouteListItem::RouteListItem(const RouteInfo& routeInfo, QListWidget* parent)
    : QListWidgetItem(parent), routeId(routeInfo.routeId)
{
    updateFromRouteInfo(routeInfo);
}

void RouteListItem::updateFromRouteInfo(const RouteInfo& routeInfo)
{
    routeId = routeInfo.routeId;
    updateDisplayText(routeInfo);
}

void RouteListItem::updateDisplayText(const RouteInfo& routeInfo)
{
    QString statusIcon = routeInfo.visible ? "👁" : "🚫";
    QString text = QString("%1 %2 (%3 WP, %4 NM)")
                   .arg(statusIcon)
                   .arg(routeInfo.name)
                   .arg(routeInfo.waypointCount)
                   .arg(routeInfo.totalDistance, 0, 'f', 1);
    setText(text);
    
    // Set different color based on route ID
    QColor routeColors[] = {
        QColor(255, 140, 0),   // Orange untuk single waypoints
        QColor(255, 100, 100), // Merah untuk Route 1
        QColor(100, 255, 100), // Hijau untuk Route 2
        QColor(100, 100, 255), // Biru untuk Route 3
        QColor(255, 255, 100), // Kuning untuk Route 4
        QColor(255, 100, 255), // Magenta untuk Route 5
        QColor(100, 255, 255)  // Cyan untuk Route 6
    };
    
    QColor color = routeColors[routeInfo.routeId % 7];
    setForeground(QBrush(color));
}

// ====== RoutePanel Implementation ======

RoutePanel::RoutePanel(EcWidget* ecWidget, QWidget *parent)
    : QWidget(parent), ecWidget(ecWidget), selectedRouteId(-1)
{
    setupUI();
    setupConnections();
    
    // Initial refresh
    refreshRouteList();
}

RoutePanel::~RoutePanel()
{
}

void RoutePanel::setupUI()
{
    setFixedWidth(280);
    setMinimumHeight(400);
    
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);
    
    // Title
    titleLabel = new QLabel("Route Management", this);
    titleLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; color: #2E86AB; padding: 5px; }");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // Route list
    routeListWidget = new QListWidget(this);
    routeListWidget->setMinimumHeight(200);
    routeListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    routeListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    mainLayout->addWidget(routeListWidget);
    
    // Control buttons
    buttonLayout = new QHBoxLayout();
    refreshButton = new QPushButton("Refresh", this);
    clearAllButton = new QPushButton("Clear All", this);
    
    refreshButton->setToolTip("Refresh route list");
    clearAllButton->setToolTip("Clear all routes");
    clearAllButton->setStyleSheet("QPushButton { background-color: #E74C3C; color: white; }");
    
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(clearAllButton);
    mainLayout->addLayout(buttonLayout);
    
    // Route info group
    routeInfoGroup = new QGroupBox("Route Information", this);
    QVBoxLayout* infoLayout = new QVBoxLayout(routeInfoGroup);
    
    routeNameLabel = new QLabel("Name: -", this);
    waypointCountLabel = new QLabel("Waypoints: -", this);
    totalDistanceLabel = new QLabel("Distance: -", this);
    totalTimeLabel = new QLabel("ETA: -", this);
    visibilityCheckBox = new QCheckBox("Visible", this);
    
    infoLayout->addWidget(routeNameLabel);
    infoLayout->addWidget(waypointCountLabel);
    infoLayout->addWidget(totalDistanceLabel);
    infoLayout->addWidget(totalTimeLabel);
    infoLayout->addWidget(visibilityCheckBox);
    
    mainLayout->addWidget(routeInfoGroup);
    
    // Context menu
    contextMenu = new QMenu(this);
    renameAction = contextMenu->addAction("Rename Route");
    toggleVisibilityAction = contextMenu->addAction("Toggle Visibility");
    contextMenu->addSeparator();
    deleteAction = contextMenu->addAction("Delete Route");
    propertiesAction = contextMenu->addAction("Properties");
    
    clearRouteInfoDisplay();
}

void RoutePanel::setupConnections()
{
    // List widget connections
    connect(routeListWidget, &QListWidget::itemSelectionChanged, 
            this, &RoutePanel::onRouteItemSelectionChanged);
    connect(routeListWidget, &QListWidget::itemDoubleClicked, 
            this, &RoutePanel::onRouteItemDoubleClicked);
    connect(routeListWidget, &QListWidget::customContextMenuRequested, 
            this, &RoutePanel::onShowContextMenu);
    
    // Button connections
    connect(refreshButton, &QPushButton::clicked, this, &RoutePanel::onRefreshClicked);
    connect(clearAllButton, &QPushButton::clicked, this, &RoutePanel::onClearAllClicked);
    
    // Checkbox connection
    connect(visibilityCheckBox, &QCheckBox::toggled, [this](bool checked) {
        if (selectedRouteId > 0) {
            emit routeVisibilityChanged(selectedRouteId, checked);
        }
    });
    
    // Context menu connections
    connect(renameAction, &QAction::triggered, this, &RoutePanel::onRenameRoute);
    connect(toggleVisibilityAction, &QAction::triggered, this, &RoutePanel::onToggleRouteVisibility);
    connect(deleteAction, &QAction::triggered, this, &RoutePanel::onDeleteRoute);
    connect(propertiesAction, &QAction::triggered, this, &RoutePanel::onRouteProperties);
}

void RoutePanel::refreshRouteList()
{
    if (!ecWidget) return;
    
    routeListWidget->clear();
    
    // Get all waypoints from EcWidget
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    
    qDebug() << "[ROUTE-PANEL] Refreshing route list, total waypoints:" << waypoints.size();
    
    // Group waypoints by route ID
    QMap<int, QList<EcWidget::Waypoint>> routeGroups;
    for (const auto& wp : waypoints) {
        qDebug() << "[ROUTE-PANEL] Waypoint:" << wp.label << "routeId:" << wp.routeId;
        if (wp.routeId > 0) { // Only route waypoints, skip single waypoints
            routeGroups[wp.routeId].append(wp);
        }
    }
    
    qDebug() << "[ROUTE-PANEL] Found" << routeGroups.size() << "routes";
    
    // Create route info items
    for (auto it = routeGroups.begin(); it != routeGroups.end(); ++it) {
        int routeId = it.key();
        RouteInfo info = calculateRouteInfo(routeId);
        
        RouteListItem* item = new RouteListItem(info, routeListWidget);
        routeListWidget->addItem(item);
    }
    
    // Update title with count
    int routeCount = routeGroups.size();
    titleLabel->setText(QString("Route Management (%1)").arg(routeCount));
}

RouteInfo RoutePanel::calculateRouteInfo(int routeId)
{
    RouteInfo info;
    info.routeId = routeId;
    info.name = QString("Route %1").arg(routeId);
    info.visible = true; // For now, assume all routes are visible
    
    if (!ecWidget) return info;
    
    // Get waypoints for this route
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    QList<EcWidget::Waypoint> routeWaypoints;
    
    for (const auto& wp : waypoints) {
        if (wp.routeId == routeId) {
            routeWaypoints.append(wp);
        }
    }
    
    info.waypointCount = routeWaypoints.size();
    
    // Calculate total distance and time
    if (routeWaypoints.size() >= 2) {
        double totalDistance = 0.0;
        
        for (int i = 0; i < routeWaypoints.size() - 1; ++i) {
            const auto& wp1 = routeWaypoints[i];
            const auto& wp2 = routeWaypoints[i + 1];
            
            // Calculate distance using Haversine formula
            double lat1 = qDegreesToRadians(wp1.lat);
            double lon1 = qDegreesToRadians(wp1.lon);
            double lat2 = qDegreesToRadians(wp2.lat);
            double lon2 = qDegreesToRadians(wp2.lon);
            
            double dlat = lat2 - lat1;
            double dlon = lon2 - lon1;
            
            double a = qSin(dlat/2) * qSin(dlat/2) + qCos(lat1) * qCos(lat2) * qSin(dlon/2) * qSin(dlon/2);
            double c = 2 * qAtan2(qSqrt(a), qSqrt(1-a));
            double distance = 6371.0 * c; // Earth radius in km
            
            totalDistance += distance * 0.539957; // Convert km to nautical miles
        }
        
        info.totalDistance = totalDistance;
        info.totalTime = totalDistance / 10.0; // Assume 10 knots speed
    }
    
    return info;
}

QString RoutePanel::formatDistance(double distanceNM)
{
    return QString("%1 NM").arg(distanceNM, 0, 'f', 1);
}

QString RoutePanel::formatTime(double hours)
{
    int h = static_cast<int>(hours);
    int m = static_cast<int>((hours - h) * 60);
    return QString("%1h %2m").arg(h).arg(m);
}

void RoutePanel::updateRouteInfoDisplay(const RouteInfo& info)
{
    routeNameLabel->setText(QString("Name: %1").arg(info.name));
    waypointCountLabel->setText(QString("Waypoints: %1").arg(info.waypointCount));
    totalDistanceLabel->setText(QString("Distance: %1").arg(formatDistance(info.totalDistance)));
    totalTimeLabel->setText(QString("ETA: %1").arg(formatTime(info.totalTime)));
    visibilityCheckBox->setChecked(info.visible);
    
    routeInfoGroup->setEnabled(true);
}

void RoutePanel::clearRouteInfoDisplay()
{  
    routeNameLabel->setText("Name: -");
    waypointCountLabel->setText("Waypoints: -");
    totalDistanceLabel->setText("Distance: -");
    totalTimeLabel->setText("ETA: -");
    visibilityCheckBox->setChecked(false);
    
    routeInfoGroup->setEnabled(false);
}

// ====== Slots Implementation ======

void RoutePanel::onRouteCreated()
{
    refreshRouteList();
}

void RoutePanel::onRouteModified()
{
    refreshRouteList();
}

void RoutePanel::onRouteDeleted()
{
    refreshRouteList();
    clearRouteInfoDisplay();
    selectedRouteId = -1;
}

// Manual update functions for specific changes
void RoutePanel::onWaypointAdded()
{
    qDebug() << "[ROUTE-PANEL] onWaypointAdded() called";
    refreshRouteList();
}

void RoutePanel::onWaypointRemoved()
{
    refreshRouteList();
}

void RoutePanel::onWaypointMoved()
{
    refreshRouteList();
}

void RoutePanel::onRouteItemSelectionChanged()
{
    QList<QListWidgetItem*> selectedItems = routeListWidget->selectedItems();
    
    if (selectedItems.isEmpty()) {
        clearRouteInfoDisplay();
        selectedRouteId = -1;
        return;
    }
    
    RouteListItem* item = dynamic_cast<RouteListItem*>(selectedItems.first());
    if (item) {
        selectedRouteId = item->getRouteId();
        RouteInfo info = calculateRouteInfo(selectedRouteId);
        updateRouteInfoDisplay(info);
        
        emit routeSelectionChanged(selectedRouteId);
    }
}

void RoutePanel::onRouteItemDoubleClicked(QListWidgetItem* item)
{
    RouteListItem* routeItem = dynamic_cast<RouteListItem*>(item);
    if (routeItem) {
        onToggleRouteVisibility();
    }
}

void RoutePanel::onShowContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = routeListWidget->itemAt(pos);
    if (item) {
        contextMenu->exec(routeListWidget->mapToGlobal(pos));
    }
}

void RoutePanel::onRefreshClicked()
{
    refreshRouteList();
    emit statusMessage("Route list refreshed");
}

void RoutePanel::onClearAllClicked()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Clear All Routes", 
        "Are you sure you want to delete all routes?",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes && ecWidget) {
        ecWidget->clearWaypoints(); // This will clear all waypoints including routes
        refreshRouteList();
        emit statusMessage("All routes cleared");
    }
}

void RoutePanel::onRenameRoute()
{
    if (selectedRouteId <= 0) return;
    
    RouteInfo info = calculateRouteInfo(selectedRouteId);
    bool ok;
    QString newName = QInputDialog::getText(
        this, "Rename Route",
        "Enter new route name:", QLineEdit::Normal,
        info.name, &ok
    );
    
    if (ok && !newName.isEmpty()) {
        // Note: For now we just refresh the display
        // In future, you might want to store route names in a separate structure
        refreshRouteList();
        emit statusMessage(QString("Route %1 renamed to %2").arg(selectedRouteId).arg(newName));
    }
}

void RoutePanel::onToggleRouteVisibility()
{
    if (selectedRouteId > 0) {
        bool currentVisibility = visibilityCheckBox->isChecked();
        visibilityCheckBox->setChecked(!currentVisibility);
        emit routeVisibilityChanged(selectedRouteId, !currentVisibility);
    }
}

void RoutePanel::onDeleteRoute()
{
    if (selectedRouteId <= 0) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Delete Route", 
        QString("Are you sure you want to delete Route %1?").arg(selectedRouteId),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes && ecWidget) {
        // Remove all waypoints with this routeId
        // Note: This is a simplified implementation
        // You might want to add a proper deleteRoute method to EcWidget
        emit statusMessage(QString("Route %1 deletion requested").arg(selectedRouteId));
    }
}

void RoutePanel::onRouteProperties()
{
    if (selectedRouteId <= 0) return;
    
    RouteInfo info = calculateRouteInfo(selectedRouteId);
    
    QString properties = QString(
        "Route %1 Properties:\n\n"
        "Name: %2\n"
        "Waypoints: %3\n"
        "Total Distance: %4\n"
        "Estimated Time: %5\n"
        "Visible: %6"
    ).arg(info.routeId)
     .arg(info.name)
     .arg(info.waypointCount)
     .arg(formatDistance(info.totalDistance))
     .arg(formatTime(info.totalTime))
     .arg(info.visible ? "Yes" : "No");
    
    QMessageBox::information(this, "Route Properties", properties);
}

void RoutePanel::updateRouteInfo(int routeId)
{
    if (routeId == selectedRouteId) {
        RouteInfo info = calculateRouteInfo(routeId);
        updateRouteInfoDisplay(info);
    }
}