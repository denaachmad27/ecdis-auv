#include "routepanel.h"
#include "ecwidget.h"
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QTimer>
#include <QtMath>

// ====== RouteListItem Implementation ======

RouteListItem::RouteListItem(const RouteInfo& routeInfo, QListWidget* parent, EcWidget* ecWidget)
    : QListWidgetItem(parent), routeId(routeInfo.routeId), ecWidget(ecWidget)
{
    updateFromRouteInfo(routeInfo);
}

void RouteListItem::updateFromRouteInfo(const RouteInfo& routeInfo)
{
    routeId = routeInfo.routeId;
    updateDisplayText(routeInfo, ecWidget);
}

void RouteListItem::updateDisplayText(const RouteInfo& routeInfo, EcWidget* ecWidget)
{
    // Show (active) status for attached routes
    QString activeStatus = routeInfo.attachedToShip ? " (active)" : "";
    
    // Single line format: name + distance + active status
    QString combinedText = QString("🗺️ %1 - 📏 %2 NM%3")
                          .arg(routeInfo.name)
                          .arg(routeInfo.totalDistance, 0, 'f', 1)
                          .arg(activeStatus);
    
    setText(combinedText);
    
    // Use colors based on attachment status to match getRouteColor() logic
    QColor color;
    if (routeInfo.routeId == 0) {
        color = QColor(255, 140, 0); // Orange for single waypoints
    } else {
        // Check if any route is attached to ship
        bool hasAttachedRoute = ecWidget ? ecWidget->hasAttachedRoute() : false;
        
        if (!hasAttachedRoute) {
            color = QColor(0, 100, 255); // Blue for all routes when none attached
        } else if (routeInfo.attachedToShip) {
            color = QColor(0, 100, 255); // Blue for attached route
        } else {
            color = QColor(128, 128, 128); // Gray for non-attached routes
        }
    }
    
    // Create modern gradient-like effect with the route color
    setData(Qt::ForegroundRole, QBrush(color));
    
    // Set custom font for modern look
    QFont font = this->font();
    font.setFamily("Segoe UI");
    font.setPixelSize(12);
    font.setWeight(QFont::Medium);
    setFont(font);
}

// ====== RoutePanel Implementation ======

RoutePanel::RoutePanel(EcWidget* ecWidget, QWidget *parent)
    : QWidget(parent), ecWidget(ecWidget), selectedRouteId(-1)
{
    setupUI();
    setupConnections();
    
    // Don't refresh here - will be refreshed by MainWindow after data is loaded
}

RoutePanel::~RoutePanel()
{
}

void RoutePanel::setupUI()
{
    // Consistent with CPA/TCPA Panel layout and styling
    mainLayout = new QVBoxLayout();
    this->setLayout(mainLayout);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Title (simple like CPA/TCPA)
    titleLabel = new QLabel("Routes");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; }");
    mainLayout->addWidget(titleLabel);
    
    // Route List Group (consistent with CPA/TCPA GroupBox style)
    QGroupBox* routeListGroup = new QGroupBox("Available Routes");
    QVBoxLayout* listGroupLayout = new QVBoxLayout();
    routeListGroup->setLayout(listGroupLayout);
    
    routeListWidget = new QListWidget(this);
    routeListWidget->setMinimumHeight(200);
    routeListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    routeListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    listGroupLayout->addWidget(routeListWidget);
    
    // Add route button in list group
    addRouteButton = new QPushButton("Add New Route");
    addRouteButton->setToolTip("Create new route");
    listGroupLayout->addWidget(addRouteButton);
    
    mainLayout->addWidget(routeListGroup);
    
    // Route Details Group (similar to Own Ship group in CPA/TCPA)
    routeInfoGroup = new QGroupBox("Route Details");
    QGridLayout* infoLayout = new QGridLayout();
    routeInfoGroup->setLayout(infoLayout);
    
    // Labels in grid layout like CPA/TCPA
    routeNameLabel = new QLabel("Name: -");
    waypointCountLabel = new QLabel("Waypoints: -");
    totalDistanceLabel = new QLabel("Distance: -");
    totalTimeLabel = new QLabel("ETA: -");
    
    infoLayout->addWidget(new QLabel("Route:"), 0, 0);
    infoLayout->addWidget(routeNameLabel, 0, 1);
    infoLayout->addWidget(new QLabel("Points:"), 1, 0);
    infoLayout->addWidget(waypointCountLabel, 1, 1);
    infoLayout->addWidget(new QLabel("Distance:"), 2, 0);
    infoLayout->addWidget(totalDistanceLabel, 2, 1);
    infoLayout->addWidget(new QLabel("ETA:"), 3, 0);
    infoLayout->addWidget(totalTimeLabel, 3, 1);
    
    // Visibility checkbox and ship attachment buttons
    visibilityCheckBox = new QCheckBox("Show on Chart");
    addToShipButton = new QPushButton("Attach to Ship");
    detachFromShipButton = new QPushButton("Detach from Ship");

    addToShipButton->setToolTip("Attach this route to ship navigation (only one route can be attached)");
    detachFromShipButton->setToolTip("Remove this route from ship navigation");

    // Awal: addToShip aktif, detachFromShip pasif
    addToShipButton->setEnabled(true);
    detachFromShipButton->setEnabled(false);
    
    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->addWidget(visibilityCheckBox);
    actionLayout->addWidget(addToShipButton);
    actionLayout->addWidget(detachFromShipButton);
    actionLayout->addStretch();
    
    infoLayout->addLayout(actionLayout, 4, 0, 1, 2);
    mainLayout->addWidget(routeInfoGroup);

    // Button states are now managed by updateRouteInfoDisplay based on actual attachment status
    
    // Control Buttons (consistent with CPA/TCPA button layout)
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    refreshButton = new QPushButton("Refresh");
    clearAllButton = new QPushButton("Clear All");
    
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(clearAllButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Context menu (simple)
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
    connect(addRouteButton, &QPushButton::clicked, this, &RoutePanel::onAddRouteClicked);
    connect(refreshButton, &QPushButton::clicked, this, &RoutePanel::onRefreshClicked);
    connect(clearAllButton, &QPushButton::clicked, this, &RoutePanel::onClearAllClicked);
    
    // Checkbox connections
    connect(visibilityCheckBox, &QCheckBox::toggled, [this](bool checked) {
        if (selectedRouteId > 0 && ecWidget) {
            qDebug() << "[ROUTE-PANEL] Visibility checkbox toggled for route" << selectedRouteId << "to" << checked;
            ecWidget->setRouteVisibility(selectedRouteId, checked);
            ecWidget->Draw(); // Use Draw() like route selection fix
            emit routeVisibilityChanged(selectedRouteId, checked);
            emit statusMessage(QString("Route %1 %2").arg(selectedRouteId).arg(checked ? "shown" : "hidden"));
        }
    });
    
    // Add to ship button connection
    connect(addToShipButton, &QPushButton::clicked, [this]() {
        if (selectedRouteId > 0 && ecWidget) {
            // Attach this route to ship (detaches others)
            ecWidget->attachRouteToShip(selectedRouteId);
            publishToMOOSDB();
            ecWidget->clearOwnShipTrail();
            ecWidget->setOwnShipTrail(true);
            
            // Update button states
            addToShipButton->setEnabled(false);
            detachFromShipButton->setEnabled(true);
            
            // Don't refresh route list immediately, let forceRedraw handle the update
            // refreshRouteList(); // This might be causing the visibility issue
            
            // Use a timer to refresh the list after attachment is complete
            QTimer::singleShot(100, [this]() {
                refreshRouteList();
            });
            
            emit statusMessage(QString("Route %1 attached to ship").arg(selectedRouteId));
        }
    });

    // Detach from ship button
    connect(detachFromShipButton, &QPushButton::clicked, [this]() {
        if (selectedRouteId > 0 && ecWidget) {
            // Preserve visibility before detaching
            bool currentVisibility = ecWidget->isRouteVisible(selectedRouteId);
            
            // Detach this route from ship (this will make all routes blue again)
            ecWidget->attachRouteToShip(-1); // Detach all routes
            ecWidget->publishToMOOSDB("WAYPT_NAV", "");
            ecWidget->setOwnShipTrail(false);
            
            // Ensure visibility is maintained
            ecWidget->setRouteVisibility(selectedRouteId, currentVisibility);
            
            // Update button states
            addToShipButton->setEnabled(true);
            detachFromShipButton->setEnabled(false);
            
            // Use a timer to refresh the list after detachment is complete
            QTimer::singleShot(100, [this]() {
                refreshRouteList();
            });
            
            emit statusMessage(QString("Route %1 detached from ship").arg(selectedRouteId));
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
    
    // Preserve current selection
    int previouslySelectedRouteId = selectedRouteId;
    
    qDebug() << "[ROUTE-PANEL] refreshRouteList() called with selectedRouteId:" << selectedRouteId;
    
    routeListWidget->clear();
    
    // Get all waypoints from EcWidget
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    
    qDebug() << "[ROUTE-PANEL] Refreshing route list, total waypoints:" << waypoints.size();
    
    // Debug: Print all waypoints with their routeIds
    for (const auto& wp : waypoints) {
        qDebug() << "[ROUTE-PANEL] Waypoint:" << wp.label << "routeId:" << wp.routeId << "lat:" << wp.lat << "lon:" << wp.lon;
    }
    
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
    RouteListItem* itemToSelect = nullptr;
    for (auto it = routeGroups.begin(); it != routeGroups.end(); ++it) {
        int routeId = it.key();
        RouteInfo info = calculateRouteInfo(routeId);
        
        RouteListItem* item = new RouteListItem(info, routeListWidget, ecWidget);
        routeListWidget->addItem(item);
        
        // Remember item to re-select
        if (routeId == previouslySelectedRouteId) {
            itemToSelect = item;
        }
    }
    
    // Restore selection and update info display
    if (itemToSelect && previouslySelectedRouteId > 0) {
        // Block selection change signals during restore to prevent unnecessary redraws
        routeListWidget->blockSignals(true);
        routeListWidget->setCurrentItem(itemToSelect);
        routeListWidget->blockSignals(false);
        
        qDebug() << "[SELECTED-ROUTE] RoutePanel restoring selectedRouteId to" << previouslySelectedRouteId;
        selectedRouteId = previouslySelectedRouteId;
        RouteInfo info = calculateRouteInfo(selectedRouteId);
        updateRouteInfoDisplay(info);
        
        // CRITICAL: Sync with EcWidget's selectedRouteId during restore
        if (ecWidget) {
            int ecWidgetSelectedRoute = ecWidget->getSelectedRoute();
            if (ecWidgetSelectedRoute != selectedRouteId) {
                qDebug() << "[SELECTED-ROUTE] SYNC: EcWidget selectedRouteId differs (" << ecWidgetSelectedRoute << "vs" << selectedRouteId << "), syncing...";
                ecWidget->setSelectedRoute(selectedRouteId);
            } else {
                qDebug() << "[SELECTED-ROUTE] SYNC: EcWidget already has correct selectedRouteId (" << selectedRouteId << "), skipping sync";
            }
        }
        
        qDebug() << "[ROUTE-PANEL] Restored selection for route" << selectedRouteId << "name:" << info.name << "visibility:" << info.visible << "waypoints:" << info.waypointCount;
    }
    
    // Update title with count
    int routeCount = routeGroups.size();
    titleLabel->setText(QString("Routes (%1)").arg(routeCount));
}

RouteInfo RoutePanel::calculateRouteInfo(int routeId)
{
    RouteInfo info;
    info.routeId = routeId;
    info.visible = ecWidget ? ecWidget->isRouteVisible(routeId) : true;
    info.attachedToShip = ecWidget ? ecWidget->isRouteAttachedToShip(routeId) : false;
    qDebug() << "[ROUTE-PANEL] *** calculateRouteInfo for route" << routeId << "visibility:" << info.visible << "attachedToShip:" << info.attachedToShip;
    qDebug() << "[ROUTE-PANEL] *** selectedRouteId:" << selectedRouteId << "calling from calculateRouteInfo";
    
    if (!ecWidget) {
        info.name = QString("Route %1").arg(routeId);
        return info;
    }
    
    // Get actual route name from routeList
    EcWidget::Route routeData = ecWidget->getRouteById(routeId);
    if (routeData.routeId != 0) {
        info.name = routeData.name;
        qDebug() << "[ROUTE-PANEL] Found route name:" << info.name << "for route" << routeId;
    } else {
        info.name = QString("Route %1").arg(routeId);
        qDebug() << "[ROUTE-PANEL] Route" << routeId << "not found in routeList, using default name";
    }
    
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

QList<EcWidget::Waypoint> RoutePanel::getWaypointById(int routeId)
{
    if (!ecWidget) return {};

    // Get waypoints for this route
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    QList<EcWidget::Waypoint> routeWaypoints;

    for (const auto& wp : waypoints) {
        if (wp.routeId == routeId) {
            routeWaypoints.append(wp);
        }
    }

    return routeWaypoints;
}

void RoutePanel::publishToMOOSDB(){
    QList<EcWidget::Waypoint> selectedData = getWaypointById(selectedRouteId);

    QStringList coordPairs;
    for (const EcWidget::Waypoint& wp : selectedData) {
        coordPairs << QString::number(wp.lat, 'f', 6) + ", " + QString::number(wp.lon, 'f', 6);
    }

    QString result = "pts={" + coordPairs.join(": ") + "}";
    ecWidget->publishToMOOSDB("WAYPT_NAV", result);
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
    routeNameLabel->setText(QString("📍 %1").arg(info.name));
    waypointCountLabel->setText(QString("📌 %1 waypoints").arg(info.waypointCount));
    totalDistanceLabel->setText(QString("📏 %1").arg(formatDistance(info.totalDistance)));
    totalTimeLabel->setText("⏱️ -"); // ETA will be processed later
    
    qDebug() << "[ROUTE-PANEL] *** SETTING CHECKBOX TO:" << info.visible << "for route" << info.routeId;
    qDebug() << "[ROUTE-PANEL] *** selectedRouteId:" << selectedRouteId << "checkbox about to be set";
    
    // Block signals while updating checkbox to prevent unnecessary events
    visibilityCheckBox->blockSignals(true);
    visibilityCheckBox->setChecked(info.visible);
    visibilityCheckBox->blockSignals(false);
    
    qDebug() << "[ROUTE-PANEL] *** CHECKBOX SET COMPLETED - checkbox checked:" << visibilityCheckBox->isChecked();
    
    // Update button states based on attachment status
    if (info.attachedToShip) {
        addToShipButton->setEnabled(false);
        detachFromShipButton->setEnabled(true);
    } else {
        addToShipButton->setEnabled(true);
        detachFromShipButton->setEnabled(false);
    }
    
    qDebug() << "[ROUTE-PANEL] *** Updated info display for route" << info.routeId << "visibility:" << info.visible << "attachedToShip:" << info.attachedToShip;
    
    routeInfoGroup->setEnabled(true);
    
    // Add subtle animation effect
    routeInfoGroup->setStyleSheet(routeInfoGroup->styleSheet() + 
        "QGroupBox { border-color: #007bff; }");
}

void RoutePanel::clearRouteInfoDisplay()
{  
    routeNameLabel->setText("📍 No route selected");
    waypointCountLabel->setText("📌 -");
    totalDistanceLabel->setText("📏 -");
    totalTimeLabel->setText("⏱️ -");
    
    // CRITICAL FIX: Block signals to prevent unwanted toggle events
    visibilityCheckBox->blockSignals(true);
    visibilityCheckBox->setChecked(false);
    visibilityCheckBox->blockSignals(false);
    
    addToShipButton->setEnabled(false);
    detachFromShipButton->setEnabled(false);
    
    routeInfoGroup->setEnabled(false);
    
    // Clear visual feedback in chart
    if (ecWidget) {
        ecWidget->setSelectedRoute(-1);
        // Note: setSelectedRoute already calls forceRedraw() internally
    }
    
    // Reset border color
    routeInfoGroup->setStyleSheet(routeInfoGroup->styleSheet().replace("#007bff", "#e9ecef"));
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
    
    // Only refresh if there are actual routes (avoid refresh during route creation)
    if (!ecWidget) return;
    
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    QMap<int, QList<EcWidget::Waypoint>> routeGroups;
    for (const auto& wp : waypoints) {
        if (wp.routeId > 0) {
            routeGroups[wp.routeId].append(wp);
        }
    }
    
    // Only refresh if we have actual complete routes
    bool hasCompleteRoutes = false;
    for (auto it = routeGroups.begin(); it != routeGroups.end(); ++it) {
        if (it.value().size() >= 2) { // Route needs at least 2 waypoints
            hasCompleteRoutes = true;
            break;
        }
    }
    
    if (hasCompleteRoutes) {
        // Optimize delay based on selection state - shorter delay if route is selected
        int refreshDelay = (selectedRouteId > 0) ? 100 : 200;
        qDebug() << "[ROUTE-PANEL] Scheduling refresh with delay:" << refreshDelay << "ms (selectedRouteId:" << selectedRouteId << ")";
        
        // Use a timer to delay refresh and avoid excessive redraws during route creation
        QTimer::singleShot(refreshDelay, [this]() {
            refreshRouteList();
        });
    }
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
        int newSelectedRouteId = item->getRouteId();
        qDebug() << "[SELECTED-ROUTE] RoutePanel changing selectedRouteId from" << selectedRouteId << "to" << newSelectedRouteId;
        selectedRouteId = newSelectedRouteId;
        RouteInfo info = calculateRouteInfo(selectedRouteId);
        updateRouteInfoDisplay(info);
        
        // Set visual feedback in chart - SYNC with EcWidget's selectedRouteId
        if (ecWidget) {
            qDebug() << "[SELECTED-ROUTE] Syncing EcWidget selectedRouteId to" << selectedRouteId;
            ecWidget->setSelectedRoute(selectedRouteId);
            // Note: setSelectedRoute already calls forceRedraw() internally, no need for additional redraw
        }
        
        emit routeSelectionChanged(selectedRouteId);
        
        // Emit status message for route selection
        emit statusMessage(QString("Selected Route %1").arg(selectedRouteId));
    }
}

void RoutePanel::onRouteItemDoubleClicked(QListWidgetItem* item)
{
    RouteListItem* routeItem = dynamic_cast<RouteListItem*>(item);
    if (routeItem) {
        int routeId = routeItem->getRouteId();
        emit requestEditRoute(routeId);
        emit statusMessage(QString("Opening route editor for Route ID: %1").arg(routeId));
    }
}

void RoutePanel::onShowContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = routeListWidget->itemAt(pos);
    if (item) {
        contextMenu->exec(routeListWidget->mapToGlobal(pos));
    }
}

void RoutePanel::onAddRouteClicked()
{
    // Emit signal to main window to start route creation
    emit requestCreateRoute();
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
    
    // Create modern input dialog
    QInputDialog dialog(this);
    dialog.setWindowTitle("✏️ Rename Route");
    dialog.setLabelText(QString("Enter new name for %1:").arg(info.name));
    dialog.setTextValue(info.name);
    dialog.setInputMode(QInputDialog::TextInput);
    
    // Modern styling
    dialog.setStyleSheet(
        "QInputDialog {"
        "    background-color: white;"
        "    font-family: 'Segoe UI';"
        "}"
        "QLabel {"
        "    color: #495057;"
        "    font-size: 13px;"
        "    font-weight: 500;"
        "}"
        "QLineEdit {"
        "    border: 2px solid #e9ecef;"
        "    border-radius: 4px;"
        "    padding: 8px 12px;"
        "    font-size: 13px;"
        "    background-color: white;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #007bff;"
        "    outline: none;"
        "}"
        "QPushButton {"
        "    background-color: #007bff;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    padding: 8px 16px;"
        "    font-weight: 500;"
        "    min-width: 80px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0056b3;"
        "}"
    );
    
    if (dialog.exec() == QDialog::Accepted) {
        QString newName = dialog.textValue().trimmed();
        if (!newName.isEmpty() && newName != info.name) {
            // TODO: Implement actual rename functionality in EcWidget
            // For now, just refresh and show message
            refreshRouteList();
            emit statusMessage(QString("✅ Route renamed to '%1'").arg(newName));
        }
    }
}

void RoutePanel::onToggleRouteVisibility()
{
    if (selectedRouteId > 0 && ecWidget) {
        bool currentVisibility = ecWidget->isRouteVisible(selectedRouteId);
        ecWidget->setRouteVisibility(selectedRouteId, !currentVisibility);
        visibilityCheckBox->setChecked(!currentVisibility);
        ecWidget->update(); // Use lighter update instead of forceRedraw
        emit routeVisibilityChanged(selectedRouteId, !currentVisibility);
    }
}

void RoutePanel::onDeleteRoute()
{
    if (selectedRouteId <= 0) return;
    
    // Modern styled message box
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("🗑️ Delete Route");
    msgBox.setText(QString("Are you sure you want to delete Route %1?").arg(selectedRouteId));
    msgBox.setInformativeText("This action cannot be undone.");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setIcon(QMessageBox::Warning);
    
    // Modern button styling
    msgBox.setStyleSheet(
        "QMessageBox {"
        "    background-color: white;"
        "    font-family: 'Segoe UI';"
        "}"
        "QPushButton {"
        "    background-color: #6c757d;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    padding: 8px 16px;"
        "    font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "    background-color: #545b62;"
        "}"
    );
    
    if (msgBox.exec() == QMessageBox::Yes && ecWidget) {
        // Call the actual delete function
        bool success = ecWidget->deleteRoute(selectedRouteId);
        
        if (success) {
            refreshRouteList();
            clearRouteInfoDisplay();
            selectedRouteId = -1;
            emit statusMessage(QString("✅ Route %1 deleted successfully").arg(selectedRouteId));
        } else {
            QMessageBox::critical(this, "❌ Delete Error", 
                QString("Failed to delete Route %1").arg(selectedRouteId));
        }
    }
}

void RoutePanel::onRouteProperties()
{
    if (selectedRouteId <= 0) return;
    
    RouteInfo info = calculateRouteInfo(selectedRouteId);
    
    // Create modern properties dialog
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("ℹ️ Route Properties");
    
    QString properties = QString(
        "🗺️ <b>%1</b><br><br>"
        "📍 <b>Route ID:</b> %2<br>"
        "📌 <b>Waypoints:</b> %3<br>"
        "📏 <b>Total Distance:</b> %4<br>"
        "⏱️ <b>Estimated Time:</b> %5<br>"
        "👁️ <b>Visibility:</b> %6<br>"
        "⚡ <b>Status:</b> %7"
    ).arg(info.name)
     .arg(info.routeId)
     .arg(info.waypointCount)
     .arg(formatDistance(info.totalDistance))
     .arg(formatTime(info.totalTime))
     .arg(info.visible ? "Visible" : "Hidden")
     .arg(info.attachedToShip ? "Attached to Ship" : "Not Attached");
    
    msgBox.setText(properties);
    msgBox.setIcon(QMessageBox::Information);
    
    // Modern styling
    msgBox.setStyleSheet(
        "QMessageBox {"
        "    background-color: white;"
        "    font-family: 'Segoe UI';"
        "    font-size: 13px;"
        "}"
        "QMessageBox QLabel {"
        "    color: #495057;"
        "}"
        "QPushButton {"
        "    background-color: #007bff;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    padding: 8px 16px;"
        "    font-weight: 500;"
        "    min-width: 80px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0056b3;"
        "}"
    );
    
    msgBox.exec();
}

void RoutePanel::updateRouteInfo(int routeId)
{
    if (routeId == selectedRouteId) {
        RouteInfo info = calculateRouteInfo(routeId);
        updateRouteInfoDisplay(info);
    }
}
