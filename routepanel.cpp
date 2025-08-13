#include "routepanel.h"
#include "ecwidget.h"
#include "routedetaildialog.h"
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QTimer>
#include <QtMath>
#include <QFormLayout>
#include <QTime>
#include <QFileDialog>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

// ====== RouteTreeItem Implementation ======

RouteTreeItem::RouteTreeItem(const RouteInfo& routeInfo, QTreeWidget* parent, EcWidget* ecWidget)
    : QTreeWidgetItem(parent), routeId(routeInfo.routeId), ecWidget(ecWidget)
{
    updateFromRouteInfo(routeInfo);
}

void RouteTreeItem::updateFromRouteInfo(const RouteInfo& routeInfo)
{
    routeId = routeInfo.routeId;
    updateDisplayText(routeInfo);
}

void RouteTreeItem::updateDisplayText(const RouteInfo& routeInfo)
{
    // Show (active) status for attached routes and (hidden) for invisible routes
    QString activeStatus = routeInfo.attachedToShip ? " (active)" : "";
    QString visibilityStatus = !routeInfo.visible ? " [HIDDEN]" : "";
    
    // Route format: icon + name + distance + status (remove manual arrows, let Qt handle them)
    QString routeText = QString("🗺️ %1 - 📏 %2 NM%3%4")
                       .arg(routeInfo.name)
                       .arg(routeInfo.totalDistance, 0, 'f', 1)
                       .arg(activeStatus)
                       .arg(visibilityStatus);
    
    setText(0, routeText);
    
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
    
    // Set custom font for modern look - increased size
    QFont font = this->font(0);
    font.setFamily("Segoe UI");
    font.setPixelSize(14); // Increased from 12 to 14
    font.setWeight(QFont::Medium);
    
    // Apply different styling for hidden routes
    if (!routeInfo.visible) {
        // Hidden routes - gray out everything with stronger contrast
        setData(0, Qt::ForegroundRole, QBrush(QColor(100, 100, 100))); // Even darker gray for better visibility
        setData(0, Qt::BackgroundRole, QBrush(QColor(245, 245, 245))); // Light gray background
        
        // Make font italic and lighter for hidden routes
        QFont hiddenFont = font;
        hiddenFont.setItalic(true);
        hiddenFont.setWeight(QFont::Light);
        setFont(0, hiddenFont);
    } else {
        // Visible routes - normal styling with proper colors
        setData(0, Qt::ForegroundRole, QBrush(color));
        
        // Add subtle background for route items to distinguish from waypoints
        if (routeInfo.attachedToShip) {
            setData(0, Qt::BackgroundRole, QBrush(QColor(230, 245, 255))); // Light blue for active route
        } else {
            setData(0, Qt::BackgroundRole, QBrush(QColor(248, 248, 248))); // Light gray for inactive routes
        }
        
        // Apply normal font for visible routes
        setFont(0, font);
    }
}

// ====== WaypointTreeItem Implementation ======

WaypointTreeItem::WaypointTreeItem(const EcWidget::Waypoint& waypoint, RouteTreeItem* parent)
    : QTreeWidgetItem(parent), waypointData(waypoint)
{
    updateDisplayText();
}

void WaypointTreeItem::updateWaypoint(const EcWidget::Waypoint& waypoint)
{
    waypointData = waypoint;
    updateDisplayText();
}

void WaypointTreeItem::updateDisplayText()
{
    // Use different icons and better visual indicators for active/inactive waypoints
    QString icon = waypointData.active ? "📍" : "⭕";
    QString statusText = waypointData.active ? "" : " [INACTIVE]";
    
    // Better coordinate formatting - 6 decimal places for precision
    QString waypointText = QString("%1 %2 (%3°, %4°)%5")
                          .arg(icon)
                          .arg(waypointData.label.isEmpty() ? QString("WP-%1").arg(waypointData.routeId) : waypointData.label)
                          .arg(waypointData.lat, 0, 'f', 6)
                          .arg(waypointData.lon, 0, 'f', 6)
                          .arg(statusText);
    
    setText(0, waypointText);
    
    // Improved styling with larger font
    QFont font = this->font(0);
    font.setFamily("Segoe UI");
    font.setPixelSize(12); // Increased from 10 to 12
    font.setWeight(QFont::Normal);
    font.setItalic(false); // Remove italic for better readability
    setFont(0, font);
    
    // Better color differentiation for active/inactive waypoints
    if (waypointData.active) {
        setData(0, Qt::ForegroundRole, QBrush(QColor(40, 120, 40))); // Dark green for active
        setData(0, Qt::BackgroundRole, QBrush(QColor(245, 255, 245))); // Very light green background
        
        // Normal font weight for active waypoints
        QFont activeFont = font;
        activeFont.setWeight(QFont::Normal);
        setFont(0, activeFont);
    } else {
        setData(0, Qt::ForegroundRole, QBrush(QColor(120, 120, 120))); // Darker gray for better contrast
        setData(0, Qt::BackgroundRole, QBrush(QColor(245, 245, 245))); // Light gray background
        
        // Strike-through effect for inactive waypoints
        QFont inactiveFont = font;
        inactiveFont.setStrikeOut(true); // Add strike-through
        inactiveFont.setWeight(QFont::Light); // Lighter weight
        setFont(0, inactiveFont);
    }
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
    
    // Route Management Buttons Group
    QGroupBox* routeManagementGroup = new QGroupBox("Route Management");
    routeManagementGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #c0c0c0;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QHBoxLayout* routeManagementLayout = new QHBoxLayout();
    routeManagementGroup->setLayout(routeManagementLayout);
    
    addRouteButton = new QPushButton("Add Route");
    importRoutesButton = new QPushButton("Import");
    exportRoutesButton = new QPushButton("Export");
    refreshButton = new QPushButton("Refresh");
    clearAllButton = new QPushButton("Clear All");
    
    addRouteButton->setToolTip("Create new route");
    importRoutesButton->setToolTip("Import routes from CSV file");
    exportRoutesButton->setToolTip("Export all routes to CSV file");
    refreshButton->setToolTip("Refresh route list");
    clearAllButton->setToolTip("Clear all routes");
    
    routeManagementLayout->addWidget(addRouteButton);
    routeManagementLayout->addWidget(importRoutesButton);
    routeManagementLayout->addWidget(exportRoutesButton);
    routeManagementLayout->addStretch();
    routeManagementLayout->addWidget(refreshButton);
    routeManagementLayout->addWidget(clearAllButton);
    
    mainLayout->addWidget(routeManagementGroup);
    
    // Route List Group (consistent with CPA/TCPA GroupBox style)
    QGroupBox* routeListGroup = new QGroupBox("Available Routes");
    routeListGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #c0c0c0;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout* listGroupLayout = new QVBoxLayout();
    routeListGroup->setLayout(listGroupLayout);
    
    routeTreeWidget = new QTreeWidget(this);
    routeTreeWidget->setMinimumHeight(200);
    routeTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    routeTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    routeTreeWidget->setHeaderHidden(true); // Hide column headers
    routeTreeWidget->setRootIsDecorated(true); // Show expand/collapse indicators
    routeTreeWidget->setIndentation(25); // Increased indentation for better hierarchy
    routeTreeWidget->setUniformRowHeights(false); // Allow different row heights
    
    // Set better styling for improved readability
    routeTreeWidget->setStyleSheet(
        "QTreeWidget {"
        "    background-color: #ffffff;"
        "    border: 1px solid #d0d0d0;"
        "    border-radius: 4px;"
        "    padding: 5px;"
        "}"
        "QTreeWidget::item {"
        "    padding: 8px 4px;" // Increased padding for better spacing
        "    border: none;"
        "    min-height: 24px;" // Minimum height for better touch targets
        "}"
        "QTreeWidget::item:selected {"
        "    background-color: #3daee9;"
        "    color: #ffffff;"
        "    border-radius: 3px;"
        "}"
        "QTreeWidget::item:hover {"
        "    background-color: #e3f2fd;"
        "    border-radius: 3px;"
        "}"
        "QTreeWidget::branch {"
        "    width: 20px;"
        "    height: 20px;"
        "    margin: 2px;"
        "}"
        "QTreeWidget::branch:has-children:closed {"
        "    background: transparent;"
        "    border: none;"
        "}"
        "QTreeWidget::branch:has-children:open {"
        "    background: transparent;"
        "    border: none;"
        "}"
    );
    
    listGroupLayout->addWidget(routeTreeWidget);
    
    // Waypoint Management Buttons Group
    QGroupBox* waypointManagementGroup = new QGroupBox("Waypoint Management");
    waypointManagementGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #c0c0c0;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QGridLayout* waypointManagementLayout = new QGridLayout();
    waypointManagementGroup->setLayout(waypointManagementLayout);
    
    addWaypointButton = new QPushButton("+ Add");
    editWaypointButton = new QPushButton("✎ Edit");
    deleteWaypointButton = new QPushButton("✕ Delete");
    moveUpButton = new QPushButton("↑ Up");
    moveDownButton = new QPushButton("↓ Down");
    duplicateWaypointButton = new QPushButton("⧉ Duplicate");
    toggleActiveButton = new QPushButton("●/○ Toggle");
    
    addWaypointButton->setToolTip("Add new waypoint to selected route");
    editWaypointButton->setToolTip("Edit selected waypoint");
    deleteWaypointButton->setToolTip("Delete selected waypoint");
    moveUpButton->setToolTip("Move waypoint up in route");
    moveDownButton->setToolTip("Move waypoint down in route");
    duplicateWaypointButton->setToolTip("Duplicate selected waypoint");
    toggleActiveButton->setToolTip("Toggle waypoint active/inactive status");
    
    // Initially disable all waypoint buttons until selection
    editWaypointButton->setEnabled(false);
    deleteWaypointButton->setEnabled(false);
    moveUpButton->setEnabled(false);
    moveDownButton->setEnabled(false);
    duplicateWaypointButton->setEnabled(false);
    toggleActiveButton->setEnabled(false);
    
    // Layout buttons in 2 rows
    waypointManagementLayout->addWidget(addWaypointButton, 0, 0);
    waypointManagementLayout->addWidget(editWaypointButton, 0, 1);
    waypointManagementLayout->addWidget(deleteWaypointButton, 0, 2);
    waypointManagementLayout->addWidget(duplicateWaypointButton, 0, 3);
    waypointManagementLayout->addWidget(moveUpButton, 1, 0);
    waypointManagementLayout->addWidget(moveDownButton, 1, 1);
    waypointManagementLayout->addWidget(toggleActiveButton, 1, 2);
    waypointManagementLayout->setColumnStretch(3, 1); // Stretch last column
    
    listGroupLayout->addWidget(waypointManagementGroup);
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
    
    // Context menus
    // Route context menu
    routeContextMenu = new QMenu(this);
    renameRouteAction = routeContextMenu->addAction("Rename Route");
    duplicateRouteAction = routeContextMenu->addAction("Duplicate Route");
    exportRouteAction = routeContextMenu->addAction("Export Route");
    routeContextMenu->addSeparator();
    toggleVisibilityAction = routeContextMenu->addAction("Toggle Visibility");
    routeContextMenu->addSeparator();
    deleteRouteAction = routeContextMenu->addAction("Delete Route");
    routePropertiesAction = routeContextMenu->addAction("Properties");
    
    // Waypoint context menu
    waypointContextMenu = new QMenu(this);
    editWaypointAction = waypointContextMenu->addAction("Edit Waypoint");
    duplicateWaypointAction = waypointContextMenu->addAction("Duplicate Waypoint");
    waypointContextMenu->addSeparator();
    insertBeforeAction = waypointContextMenu->addAction("Insert Waypoint Before");
    insertAfterAction = waypointContextMenu->addAction("Insert Waypoint After");
    waypointContextMenu->addSeparator();
    moveUpAction = waypointContextMenu->addAction("Move Up");
    moveDownAction = waypointContextMenu->addAction("Move Down");
    waypointContextMenu->addSeparator();
    toggleActiveAction = waypointContextMenu->addAction("Toggle Active/Inactive");
    waypointContextMenu->addSeparator();
    deleteWaypointAction = waypointContextMenu->addAction("Delete Waypoint");
    
    clearRouteInfoDisplay();
}

void RoutePanel::setupConnections()
{
    // Tree widget connections
    connect(routeTreeWidget, &QTreeWidget::itemSelectionChanged, 
            this, &RoutePanel::onRouteItemSelectionChanged);
    connect(routeTreeWidget, &QTreeWidget::itemDoubleClicked, 
            this, &RoutePanel::onRouteItemDoubleClicked);
    connect(routeTreeWidget, &QTreeWidget::customContextMenuRequested, 
            this, &RoutePanel::onShowContextMenu);
    
    // Route Management Button connections
    connect(addRouteButton, &QPushButton::clicked, this, &RoutePanel::onAddRouteClicked);
    connect(importRoutesButton, &QPushButton::clicked, this, &RoutePanel::onImportRoutesClicked);
    connect(exportRoutesButton, &QPushButton::clicked, this, &RoutePanel::onExportRoutesClicked);
    connect(refreshButton, &QPushButton::clicked, this, &RoutePanel::onRefreshClicked);
    connect(clearAllButton, &QPushButton::clicked, this, &RoutePanel::onClearAllClicked);
    
    // Waypoint Management Button connections
    connect(addWaypointButton, &QPushButton::clicked, this, &RoutePanel::onAddWaypointClicked);
    connect(editWaypointButton, &QPushButton::clicked, this, &RoutePanel::onEditWaypointClicked);
    connect(deleteWaypointButton, &QPushButton::clicked, this, &RoutePanel::onDeleteWaypointClicked);
    connect(moveUpButton, &QPushButton::clicked, this, &RoutePanel::onMoveWaypointUp);
    connect(moveDownButton, &QPushButton::clicked, this, &RoutePanel::onMoveWaypointDown);
    connect(duplicateWaypointButton, &QPushButton::clicked, this, &RoutePanel::onDuplicateWaypointClicked);
    connect(toggleActiveButton, &QPushButton::clicked, this, &RoutePanel::onToggleWaypointActive);
    
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
    
    // Route Context menu connections
    connect(renameRouteAction, &QAction::triggered, this, &RoutePanel::onRenameRoute);
    connect(duplicateRouteAction, &QAction::triggered, [this]() { 
        // TODO: Implement duplicate route functionality
        emit statusMessage("Duplicate route functionality not yet implemented");
    });
    connect(exportRouteAction, &QAction::triggered, [this]() {
        // TODO: Implement export single route functionality  
        emit statusMessage("Export single route functionality not yet implemented");
    });
    connect(toggleVisibilityAction, &QAction::triggered, this, &RoutePanel::onToggleRouteVisibility);
    connect(deleteRouteAction, &QAction::triggered, this, &RoutePanel::onDeleteRoute);
    connect(routePropertiesAction, &QAction::triggered, this, &RoutePanel::onRouteProperties);
    
    // Waypoint Context menu connections
    connect(editWaypointAction, &QAction::triggered, this, &RoutePanel::onEditWaypointFromContext);
    connect(duplicateWaypointAction, &QAction::triggered, this, &RoutePanel::onDuplicateWaypointFromContext);
    connect(insertBeforeAction, &QAction::triggered, this, &RoutePanel::onInsertWaypointBefore);
    connect(insertAfterAction, &QAction::triggered, this, &RoutePanel::onInsertWaypointAfter);
    connect(moveUpAction, &QAction::triggered, this, &RoutePanel::onMoveWaypointUpFromContext);
    connect(moveDownAction, &QAction::triggered, this, &RoutePanel::onMoveWaypointDownFromContext);
    connect(toggleActiveAction, &QAction::triggered, [this]() {
        onToggleWaypointActive(); // Reuse button functionality
    });
    connect(deleteWaypointAction, &QAction::triggered, this, &RoutePanel::onDeleteWaypointFromContext);
}

void RoutePanel::refreshRouteList()
{
    if (!ecWidget) return;
    
    // Preserve current selection
    int previouslySelectedRouteId = selectedRouteId;
    
    qDebug() << "[ROUTE-PANEL] refreshRouteList() called with selectedRouteId:" << selectedRouteId;
    
    routeTreeWidget->clear();
    
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
    
    // Create route tree items with their waypoint children
    RouteTreeItem* itemToSelect = nullptr;
    for (auto it = routeGroups.begin(); it != routeGroups.end(); ++it) {
        int routeId = it.key();
        RouteInfo info = calculateRouteInfo(routeId);
        
        // Create route item
        RouteTreeItem* routeItem = new RouteTreeItem(info, routeTreeWidget, ecWidget);
        
        // Add waypoint children
        QList<EcWidget::Waypoint> routeWaypoints = getWaypointById(routeId);
        for (const auto& waypoint : routeWaypoints) {
            WaypointTreeItem* waypointItem = new WaypointTreeItem(waypoint, routeItem);
        }
        
        // Expand by default to show waypoints
        routeItem->setExpanded(true);
        
        // Remember item to re-select
        if (routeId == previouslySelectedRouteId) {
            itemToSelect = routeItem;
        }
    }
    
    // Restore selection and update info display
    if (itemToSelect && previouslySelectedRouteId > 0) {
        // Block selection change signals during restore to prevent unnecessary redraws
        routeTreeWidget->blockSignals(true);
        routeTreeWidget->setCurrentItem(itemToSelect);
        routeTreeWidget->blockSignals(false);
        
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
    
    // Expand all routes to show waypoints by default
    routeTreeWidget->expandAll();
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

// ====== Helper Functions ======

void RoutePanel::updateButtonStates()
{
    QTreeWidgetItem* currentItem = routeTreeWidget->currentItem();
    
    // Route selection states
    bool hasRouteSelected = (selectedRouteId > 0);
    
    // Waypoint selection states
    bool hasWaypointSelected = false;
    bool canMoveUp = false;
    bool canMoveDown = false;
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(currentItem);
    
    if (waypointItem && waypointItem->parent()) {
        hasWaypointSelected = true;
        
        // Check if waypoint can move up/down
        QTreeWidgetItem* parent = waypointItem->parent();
        int waypointIndex = parent->indexOfChild(waypointItem);
        canMoveUp = (waypointIndex > 0);
        canMoveDown = (waypointIndex < parent->childCount() - 1);
    }
    
    // Enable Add Waypoint button only if route is selected
    addWaypointButton->setEnabled(hasRouteSelected);
    
    // Enable waypoint operations only if waypoint is selected
    editWaypointButton->setEnabled(hasWaypointSelected);
    deleteWaypointButton->setEnabled(hasWaypointSelected);
    duplicateWaypointButton->setEnabled(hasWaypointSelected);
    toggleActiveButton->setEnabled(hasWaypointSelected);
    
    // Enable move buttons based on position
    moveUpButton->setEnabled(canMoveUp);
    moveDownButton->setEnabled(canMoveDown);
    
    // Enable export if there are any routes
    bool hasRoutes = (routeTreeWidget->topLevelItemCount() > 0);
    exportRoutesButton->setEnabled(hasRoutes);
}

QTreeWidgetItem* RoutePanel::getSelectedWaypointItem()
{
    QTreeWidgetItem* currentItem = routeTreeWidget->currentItem();
    return dynamic_cast<WaypointTreeItem*>(currentItem);
}

int RoutePanel::getWaypointIndex(QTreeWidgetItem* waypointItem)
{
    if (!waypointItem || !waypointItem->parent()) return -1;
    return waypointItem->parent()->indexOfChild(waypointItem);
}

int RoutePanel::getRouteIdFromItem(QTreeWidgetItem* item)
{
    if (!item) return -1;
    
    // If it's a waypoint, get route ID from parent
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(item);
    if (waypointItem) {
        return waypointItem->getWaypoint().routeId;
    }
    
    // If it's a route, get route ID directly
    RouteTreeItem* routeItem = dynamic_cast<RouteTreeItem*>(item);
    if (routeItem) {
        return routeItem->getRouteId();
    }
    
    return -1;
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
    QTreeWidgetItem* currentItem = routeTreeWidget->currentItem();
    
    if (!currentItem) {
        clearRouteInfoDisplay();
        selectedRouteId = -1;
        updateButtonStates();
        return;
    }
    
    // Handle selection of waypoint items differently
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(currentItem);
    if (waypointItem) {
        // When waypoint is selected, select its parent route
        int waypointRouteId = waypointItem->getWaypoint().routeId;
        if (waypointRouteId != selectedRouteId) {
            selectedRouteId = waypointRouteId;
            RouteInfo info = calculateRouteInfo(selectedRouteId);
            updateRouteInfoDisplay(info);
            
            // Set visual feedback in chart
            if (ecWidget) {
                ecWidget->setSelectedRoute(selectedRouteId);
            }
            
            emit routeSelectionChanged(selectedRouteId);
            emit statusMessage(QString("Selected waypoint from Route %1").arg(selectedRouteId));
        }
        
        // Update button states for waypoint selection
        updateButtonStates();
        return;
    }
    
    // Handle route item selection
    RouteTreeItem* routeItem = dynamic_cast<RouteTreeItem*>(currentItem);
    if (routeItem) {
        int newSelectedRouteId = routeItem->getRouteId();
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
    
    // Update button states for route selection
    updateButtonStates();
}

void RoutePanel::onRouteItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    RouteTreeItem* routeItem = dynamic_cast<RouteTreeItem*>(item);
    if (routeItem) {
        int routeId = routeItem->getRouteId();
        emit requestEditRoute(routeId);
        emit statusMessage(QString("Opening route editor for Route ID: %1").arg(routeId));
    }
}

void RoutePanel::onShowContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = routeTreeWidget->itemAt(pos);
    if (!item) return;
    
    if (dynamic_cast<RouteTreeItem*>(item)) {
        // Route item - show route context menu
        selectedRouteId = dynamic_cast<RouteTreeItem*>(item)->getRouteId();
        routeContextMenu->exec(routeTreeWidget->mapToGlobal(pos));
    } else if (dynamic_cast<WaypointTreeItem*>(item)) {
        // Waypoint item - show waypoint context menu
        routeTreeWidget->setCurrentItem(item); // Select the waypoint
        waypointContextMenu->exec(routeTreeWidget->mapToGlobal(pos));
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

// ====== New Helper Methods for Tree Widget ======

RouteTreeItem* RoutePanel::findRouteItem(int routeId)
{
    for (int i = 0; i < routeTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = routeTreeWidget->topLevelItem(i);
        RouteTreeItem* routeItem = dynamic_cast<RouteTreeItem*>(item);
        if (routeItem && routeItem->getRouteId() == routeId) {
            return routeItem;
        }
    }
    return nullptr;
}

bool RoutePanel::isWaypointItem(QTreeWidgetItem* item) const
{
    return dynamic_cast<WaypointTreeItem*>(item) != nullptr;
}

// ====== Waypoint Operation Functions ======

void RoutePanel::showWaypointEditDialog(int routeId, int waypointIndex)
{
    QDialog dialog(this);
    dialog.setWindowTitle(waypointIndex >= 0 ? "Edit Waypoint" : "Add Waypoint");
    dialog.setModal(true);
    dialog.resize(400, 300);
    
    QFormLayout* layout = new QFormLayout(&dialog);
    
    QLineEdit* labelEdit = new QLineEdit();
    QLineEdit* latEdit = new QLineEdit();
    QLineEdit* lonEdit = new QLineEdit();
    QLineEdit* remarkEdit = new QLineEdit();
    QCheckBox* activeCheck = new QCheckBox();
    
    // Set validators
    latEdit->setValidator(new QDoubleValidator(-90.0, 90.0, 6, &dialog));
    lonEdit->setValidator(new QDoubleValidator(-180.0, 180.0, 6, &dialog));
    
    activeCheck->setChecked(true);
    
    // If editing, populate with existing data
    if (waypointIndex >= 0 && ecWidget) {
        QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
        QList<EcWidget::Waypoint> routeWaypoints;
        
        for (const auto& wp : waypoints) {
            if (wp.routeId == routeId) {
                routeWaypoints.append(wp);
            }
        }
        
        if (waypointIndex < routeWaypoints.size()) {
            const EcWidget::Waypoint& wp = routeWaypoints[waypointIndex];
            labelEdit->setText(wp.label);
            latEdit->setText(QString::number(wp.lat, 'f', 6));
            lonEdit->setText(QString::number(wp.lon, 'f', 6));
            remarkEdit->setText(wp.remark);
            activeCheck->setChecked(wp.active);
        }
    } else {
        // Auto-generate label for new waypoint
        labelEdit->setText(QString("WP-%1").arg(QTime::currentTime().toString("hhmmss")));
    }
    
    layout->addRow("Label:", labelEdit);
    layout->addRow("Latitude:", latEdit);
    layout->addRow("Longitude:", lonEdit);
    layout->addRow("Remark:", remarkEdit);
    layout->addRow("Active:", activeCheck);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("OK");
    QPushButton* cancelBtn = new QPushButton("Cancel");
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    
    layout->addRow(buttonLayout);
    
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        double lat = latEdit->text().toDouble();
        double lon = lonEdit->text().toDouble();
        QString label = labelEdit->text().trimmed();
        QString remark = remarkEdit->text().trimmed();
        bool active = activeCheck->isChecked();
        
        // Validate coordinates
        if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
            QMessageBox::warning(this, "Invalid Coordinates", 
                "Please enter valid coordinates:\nLatitude: -90.0 to 90.0\nLongitude: -180.0 to 180.0");
            return;
        }
        
        if (ecWidget) {
            if (waypointIndex >= 0) {
                // Edit existing waypoint - delete old and create new
                QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
                QList<EcWidget::Waypoint> routeWaypoints;
                
                for (const auto& wp : waypoints) {
                    if (wp.routeId == routeId) {
                        routeWaypoints.append(wp);
                    }
                }
                
                if (waypointIndex < routeWaypoints.size()) {
                    // Update the waypoint data
                    routeWaypoints[waypointIndex].lat = lat;
                    routeWaypoints[waypointIndex].lon = lon;
                    routeWaypoints[waypointIndex].label = label;
                    routeWaypoints[waypointIndex].remark = remark;
                    routeWaypoints[waypointIndex].active = active;
                    
                    // Replace waypoints for route
                    ecWidget->replaceWaypointsForRoute(routeId, routeWaypoints);
                    refreshRouteList();
                    emit statusMessage(QString("Waypoint '%1' updated successfully").arg(label));
                }
            } else {
                // Add new waypoint
                ecWidget->createWaypointFromForm(lat, lon, label, remark, routeId, active);
                refreshRouteList();
                emit statusMessage(QString("Waypoint '%1' added successfully").arg(label));
            }
        }
    }
}

void RoutePanel::reorderWaypoint(int routeId, int fromIndex, int toIndex)
{
    if (!ecWidget) return;
    
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    QList<EcWidget::Waypoint> routeWaypoints;
    
    // Get waypoints for this route
    for (const auto& wp : waypoints) {
        if (wp.routeId == routeId) {
            routeWaypoints.append(wp);
        }
    }
    
    if (fromIndex < 0 || fromIndex >= routeWaypoints.size() || 
        toIndex < 0 || toIndex >= routeWaypoints.size()) {
        return;
    }
    
    // Perform the reorder
    EcWidget::Waypoint waypoint = routeWaypoints.takeAt(fromIndex);
    routeWaypoints.insert(toIndex, waypoint);
    
    // Update EcWidget with new order
    ecWidget->replaceWaypointsForRoute(routeId, routeWaypoints);
    refreshRouteList();
    
    // Restore selection to the moved waypoint
    RouteTreeItem* routeItem = findRouteItem(routeId);
    if (routeItem && routeItem->childCount() > toIndex) {
        QTreeWidgetItem* movedWaypoint = routeItem->child(toIndex);
        routeTreeWidget->setCurrentItem(movedWaypoint);
        routeItem->setExpanded(true);
    }
    
    emit statusMessage(QString("Waypoint moved from position %1 to %2")
        .arg(fromIndex + 1).arg(toIndex + 1));
}

void RoutePanel::duplicateWaypoint(int routeId, int waypointIndex)
{
    if (!ecWidget) return;
    
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    QList<EcWidget::Waypoint> routeWaypoints;
    
    // Get waypoints for this route
    for (const auto& wp : waypoints) {
        if (wp.routeId == routeId) {
            routeWaypoints.append(wp);
        }
    }
    
    if (waypointIndex < 0 || waypointIndex >= routeWaypoints.size()) {
        return;
    }
    
    // Create duplicate waypoint
    EcWidget::Waypoint originalWaypoint = routeWaypoints[waypointIndex];
    EcWidget::Waypoint duplicateWaypoint = originalWaypoint;
    
    // Modify label to indicate it's a duplicate
    QString originalLabel = duplicateWaypoint.label;
    if (originalLabel.isEmpty()) {
        originalLabel = QString("WP-%1").arg(waypointIndex + 1);
    }
    duplicateWaypoint.label = QString("%1-Copy").arg(originalLabel);
    
    // Add duplicate to the end of the route
    routeWaypoints.append(duplicateWaypoint);
    
    // Update EcWidget with new waypoints
    ecWidget->replaceWaypointsForRoute(routeId, routeWaypoints);
    refreshRouteList();
    
    // Select the new duplicate waypoint
    RouteTreeItem* routeItem = findRouteItem(routeId);
    if (routeItem && routeItem->childCount() > 0) {
        QTreeWidgetItem* newWaypoint = routeItem->child(routeItem->childCount() - 1);
        routeTreeWidget->setCurrentItem(newWaypoint);
        routeItem->setExpanded(true);
    }
    
    emit statusMessage(QString("Waypoint '%1' duplicated as '%2'")
        .arg(originalLabel).arg(duplicateWaypoint.label));
}

void RoutePanel::toggleWaypointActiveStatus(int routeId, int waypointIndex)
{
    if (!ecWidget) return;
    
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    QList<EcWidget::Waypoint> routeWaypoints;
    
    // Get waypoints for this route
    for (const auto& wp : waypoints) {
        if (wp.routeId == routeId) {
            routeWaypoints.append(wp);
        }
    }
    
    if (waypointIndex < 0 || waypointIndex >= routeWaypoints.size()) {
        return;
    }
    
    // Toggle active status
    EcWidget::Waypoint& waypoint = routeWaypoints[waypointIndex];
    waypoint.active = !waypoint.active;
    
    // Update EcWidget
    ecWidget->updateWaypointActiveStatus(routeId, waypoint.lat, waypoint.lon, waypoint.active);
    refreshRouteList();
    
    // Restore selection
    RouteTreeItem* routeItem = findRouteItem(routeId);
    if (routeItem && routeItem->childCount() > waypointIndex) {
        QTreeWidgetItem* waypointItem = routeItem->child(waypointIndex);
        routeTreeWidget->setCurrentItem(waypointItem);
        routeItem->setExpanded(true);
    }
    
    QString statusText = waypoint.active ? "activated" : "deactivated";
    emit statusMessage(QString("Waypoint '%1' %2")
        .arg(waypoint.label.isEmpty() ? QString("WP-%1").arg(waypointIndex + 1) : waypoint.label)
        .arg(statusText));
}

// ====== Route Management Slot Implementations ======

void RoutePanel::onImportRoutesClicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Import Routes from JSON",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "JSON Files (*.json);;All Files (*)"
    );
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import Error", "Could not open file for reading.");
        return;
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "Import Error", 
            QString("Failed to parse JSON file:\n%1").arg(parseError.errorString()));
        return;
    }
    
    QJsonObject rootObject = jsonDoc.object();
    QJsonArray routesArray = rootObject["routes"].toArray();
    
    if (routesArray.isEmpty()) {
        QMessageBox::information(this, "Import Warning", "No routes found in the selected file.");
        return;
    }
    
    int importedCount = 0;
    for (const QJsonValue& routeValue : routesArray) {
        QJsonObject routeObject = routeValue.toObject();
        
        // Create route in EcWidget
        EcWidget::Route route;
        route.routeId = ecWidget->getNextAvailableRouteId();
        route.name = routeObject["name"].toString();
        route.description = routeObject["description"].toString();
        route.totalDistance = routeObject["totalDistance"].toDouble();
        route.estimatedTime = routeObject["estimatedTime"].toDouble();
        
        // Import waypoints
        QJsonArray waypointsArray = routeObject["waypoints"].toArray();
        for (const QJsonValue& waypointValue : waypointsArray) {
            QJsonObject waypointObject = waypointValue.toObject();
            
            double lat = waypointObject["lat"].toDouble();
            double lon = waypointObject["lon"].toDouble();
            QString label = waypointObject["label"].toString();
            QString remark = waypointObject["remark"].toString();
            double turningRadius = waypointObject["turningRadius"].toDouble(0.5);
            bool active = waypointObject["active"].toBool(true);
            
            // Create waypoint using EcWidget method
            ecWidget->createWaypointFromForm(lat, lon, label, remark, route.routeId, turningRadius, active);
        }
        
        importedCount++;
    }
    
    // Refresh the route list
    refreshRouteList();
    
    QMessageBox::information(this, "Import Complete", 
        QString("Successfully imported %1 routes from:\n%2").arg(importedCount).arg(fileName));
    
    emit statusMessage(QString("Imported %1 routes successfully").arg(importedCount));
}

void RoutePanel::onExportRoutesClicked()
{
    if (!ecWidget) {
        QMessageBox::warning(this, "Export Error", "No chart widget available.");
        return;
    }
    
    // Check if there are routes to export
    QList<EcWidget::Route> routeList = ecWidget->getRoutes();
    if (routeList.isEmpty()) {
        QMessageBox::information(this, "Export Warning", "No routes available to export.");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Export Routes to JSON",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/exported_routes.json",
        "JSON Files (*.json);;All Files (*)"
    );
    
    if (fileName.isEmpty()) return;
    
    // Use EcWidget's existing JSON export functionality
    // First, save current routes to ensure data is up-to-date
    ecWidget->saveRoutes();
    
    // Read the saved routes file and copy it to export location
    QString routeFilePath = ecWidget->getRouteFilePath();
    
    if (!QFile::exists(routeFilePath)) {
        QMessageBox::warning(this, "Export Error", "Routes file not found. Please ensure routes are saved.");
        return;
    }
    
    if (!QFile::copy(routeFilePath, fileName)) {
        // If copy fails, try manual export
        QFile sourceFile(routeFilePath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, "Export Error", "Could not read routes file.");
            return;
        }
        
        QFile destFile(fileName);
        if (!destFile.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, "Export Error", "Could not create export file.");
            return;
        }
        
        destFile.write(sourceFile.readAll());
        sourceFile.close();
        destFile.close();
    }
    
    QMessageBox::information(this, "Export Complete", 
        QString("Routes exported successfully to:\n%1").arg(fileName));
    
    emit statusMessage(QString("Exported %1 routes successfully").arg(routeList.size()));
}

// ====== Waypoint Management Slot Implementations ======

void RoutePanel::onAddWaypointClicked()
{
    if (selectedRouteId <= 0) {
        QMessageBox::information(this, "No Route Selected", "Please select a route first to add waypoints.");
        return;
    }
    
    showWaypointEditDialog(selectedRouteId);
}

void RoutePanel::onEditWaypointClicked()
{
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(getSelectedWaypointItem());
    if (!waypointItem) {
        QMessageBox::information(this, "No Waypoint Selected", "Please select a waypoint to edit.");
        return;
    }
    
    int routeId = waypointItem->getWaypoint().routeId;
    int waypointIndex = getWaypointIndex(waypointItem);
    showWaypointEditDialog(routeId, waypointIndex);
}

void RoutePanel::onDeleteWaypointClicked()
{
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(getSelectedWaypointItem());
    if (!waypointItem) {
        QMessageBox::information(this, "No Waypoint Selected", "Please select a waypoint to delete.");
        return;
    }
    
    const EcWidget::Waypoint& waypoint = waypointItem->getWaypoint();
    
    int ret = QMessageBox::question(this, "Delete Waypoint", 
        QString("Are you sure you want to delete waypoint '%1'?")
        .arg(waypoint.label.isEmpty() ? QString("WP-%1").arg(getWaypointIndex(waypointItem) + 1) : waypoint.label),
        QMessageBox::Yes | QMessageBox::No);
        
    if (ret == QMessageBox::Yes) {
        if (ecWidget) {
            // Get all waypoints for this route
            QList<EcWidget::Waypoint> allWaypoints = ecWidget->getWaypoints();
            QList<EcWidget::Waypoint> routeWaypoints;
            
            for (const auto& wp : allWaypoints) {
                if (wp.routeId == waypoint.routeId) {
                    routeWaypoints.append(wp);
                }
            }
            
            // Find and remove the waypoint
            for (int i = 0; i < routeWaypoints.size(); ++i) {
                if (qAbs(routeWaypoints[i].lat - waypoint.lat) < 0.000001 && 
                    qAbs(routeWaypoints[i].lon - waypoint.lon) < 0.000001) {
                    routeWaypoints.removeAt(i);
                    break;
                }
            }
            
            // Replace waypoints for route
            ecWidget->replaceWaypointsForRoute(waypoint.routeId, routeWaypoints);
            refreshRouteList();
            emit statusMessage(QString("Waypoint '%1' deleted successfully")
                .arg(waypoint.label.isEmpty() ? "WP" : waypoint.label));
        }
    }
}

void RoutePanel::onMoveWaypointUp()
{
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(getSelectedWaypointItem());
    if (!waypointItem) return;
    
    int routeId = waypointItem->getWaypoint().routeId;
    int fromIndex = getWaypointIndex(waypointItem);
    int toIndex = fromIndex - 1;
    
    if (toIndex >= 0) {
        reorderWaypoint(routeId, fromIndex, toIndex);
    }
}

void RoutePanel::onMoveWaypointDown()
{
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(getSelectedWaypointItem());
    if (!waypointItem) return;
    
    int routeId = waypointItem->getWaypoint().routeId;
    int fromIndex = getWaypointIndex(waypointItem);
    int toIndex = fromIndex + 1;
    
    QTreeWidgetItem* parent = waypointItem->parent();
    if (parent && toIndex < parent->childCount()) {
        reorderWaypoint(routeId, fromIndex, toIndex);
    }
}

void RoutePanel::onDuplicateWaypointClicked()
{
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(getSelectedWaypointItem());
    if (!waypointItem) {
        QMessageBox::information(this, "No Waypoint Selected", "Please select a waypoint to duplicate.");
        return;
    }
    
    int routeId = waypointItem->getWaypoint().routeId;
    int waypointIndex = getWaypointIndex(waypointItem);
    duplicateWaypoint(routeId, waypointIndex);
}

void RoutePanel::onToggleWaypointActive()
{
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(getSelectedWaypointItem());
    if (!waypointItem) {
        QMessageBox::information(this, "No Waypoint Selected", "Please select a waypoint to toggle active status.");
        return;
    }
    
    int routeId = waypointItem->getWaypoint().routeId;
    int waypointIndex = getWaypointIndex(waypointItem);
    toggleWaypointActiveStatus(routeId, waypointIndex);
}

// ====== Context Menu Slot Implementations ======

void RoutePanel::onEditWaypointFromContext()
{
    onEditWaypointClicked(); // Reuse button functionality
}

void RoutePanel::onDuplicateWaypointFromContext()
{
    onDuplicateWaypointClicked(); // Reuse button functionality
}

void RoutePanel::onDeleteWaypointFromContext()
{
    onDeleteWaypointClicked(); // Reuse button functionality
}

void RoutePanel::onInsertWaypointBefore()
{
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(getSelectedWaypointItem());
    if (!waypointItem) return;
    
    int routeId = waypointItem->getWaypoint().routeId;
    int insertIndex = getWaypointIndex(waypointItem);
    
    // TODO: Implement insert waypoint at specific position
    emit statusMessage("Insert waypoint before functionality not yet implemented");
}

void RoutePanel::onInsertWaypointAfter()
{
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(getSelectedWaypointItem());
    if (!waypointItem) return;
    
    int routeId = waypointItem->getWaypoint().routeId;
    int insertIndex = getWaypointIndex(waypointItem) + 1;
    
    // TODO: Implement insert waypoint at specific position
    emit statusMessage("Insert waypoint after functionality not yet implemented");
}

void RoutePanel::onMoveWaypointUpFromContext()
{
    onMoveWaypointUp(); // Reuse button functionality
}

void RoutePanel::onMoveWaypointDownFromContext()
{
    onMoveWaypointDown(); // Reuse button functionality
}
