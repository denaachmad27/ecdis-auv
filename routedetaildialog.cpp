#include "routedetaildialog.h"
#include "ecwidget.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QStandardPaths>
#include <QtMath>
#include <QSet>
#include <QRegExp>
#include <QCheckBox>
#include <QHBoxLayout>

RouteDetailDialog::RouteDetailDialog(EcWidget* ecWidget, QWidget *parent)
    : QDialog(parent)
    , ecWidget(ecWidget)
    , mainLayout(nullptr)
    , summaryGroup(nullptr)
    , totalRoutesLabel(nullptr)
    , totalWaypointsLabel(nullptr)
    , totalDistanceLabel(nullptr)
    , activeRouteLabel(nullptr)
    , routesGroup(nullptr)
    , routeTree(nullptr)
    , buttonLayout(nullptr)
    , exportAllButton(nullptr)
    , closeButton(nullptr)
    , selectedRouteId(-1)
{
    setupUI();
    connectSignals();
    loadAllRoutesData();
    updateRouteTree();
    updateStatistics();
}

void RouteDetailDialog::setupUI()
{
    setWindowTitle("Routes Detail - All Routes Information");
    setModal(true);
    resize(800, 600);
    
    // Main layout
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Summary section
    summaryGroup = new QGroupBox("Summary Statistics");
    QGridLayout* summaryLayout = new QGridLayout(summaryGroup);
    
    totalRoutesLabel = new QLabel("Total Routes: 0");
    totalWaypointsLabel = new QLabel("Total Waypoints: 0");
    totalDistanceLabel = new QLabel("Total Distance: 0.0 NM");
    activeRouteLabel = new QLabel("Active Route: None");
    
    // Style summary labels with default appearance consistent with RouteFormDialog
    QString labelStyle = "QLabel { font-weight: bold; color: #006600; padding: 5px; }";
    totalRoutesLabel->setStyleSheet(labelStyle);
    totalWaypointsLabel->setStyleSheet(labelStyle);
    totalDistanceLabel->setStyleSheet(labelStyle);
    activeRouteLabel->setStyleSheet(labelStyle);
    
    summaryLayout->addWidget(totalRoutesLabel, 0, 0);
    summaryLayout->addWidget(totalWaypointsLabel, 0, 1);
    summaryLayout->addWidget(totalDistanceLabel, 1, 0);
    summaryLayout->addWidget(activeRouteLabel, 1, 1);
    
    mainLayout->addWidget(summaryGroup);
    
    // Main content area
    // No splitter needed since we're using single tree view
    
    // Routes section with integrated waypoints
    routesGroup = new QGroupBox("All Routes");
    QVBoxLayout* routesLayout = new QVBoxLayout(routesGroup);
    
    // Add instruction label
    QLabel* instructionLabel = new QLabel("Click ▶ to expand routes and view waypoints. Check/uncheck Active column to toggle waypoint status.");
    instructionLabel->setStyleSheet("QLabel { font-style: italic; color: #6c757d; margin: 5px 0px; }");
    routesLayout->addWidget(instructionLabel);
    
    routeTree = new QTreeWidget();
    routeTree->setColumnCount(6);
    QStringList treeHeaders;
    treeHeaders << "Routes & Waypoints" << "Coordinates" << "Distance" << "Status" << "Active" << "Description";
    routeTree->setHeaderLabels(treeHeaders);
    
    // Professional styling for route tree
    routeTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeTree->setSelectionMode(QAbstractItemView::SingleSelection);
    routeTree->setAlternatingRowColors(true);
    routeTree->setRootIsDecorated(true); // Show expand/collapse indicators
    routeTree->setIndentation(20); // Set indentation for child items
    routeTree->setSortingEnabled(false); // Maintain route order
    routeTree->setMinimumHeight(350);
    routeTree->setExpandsOnDoubleClick(true);
    routeTree->setUniformRowHeights(false); // Allow varying row heights for better presentation
    
    // Default styling consistent with RouteFormDialog
    routeTree->setStyleSheet(
        "QTreeWidget {"
        "    gridline-color: #d0d0d0;"
        "    border: 1px solid #c0c0c0;"
        "    selection-background-color: #3daee9;"
        "    selection-color: #ffffff;"
        "    alternate-background-color: #f5f5f5;"
        "}"
        "QTreeWidget::item {"
        "    padding: 8px;"
        "    border: none;"
        "    color: #333;"
        "}"
        "QTreeWidget::item:selected {"
        "    background-color: #3daee9;"
        "    color: #ffffff;"
        "}"
        "QTreeWidget::item:hover {"
        "    background-color: #e3f2fd;"
        "    color: #333;"
        "}"
        "QHeaderView::section {"
        "    background-color: #e8e8e8;"
        "    padding: 8px;"
        "    border: 1px solid #c0c0c0;"
        "    font-weight: bold;"
        "    color: #333;"
        "}"
    );
    
    // Resize columns for optimal display
    routeTree->header()->resizeSection(0, 220); // Routes & Waypoints 
    routeTree->header()->resizeSection(1, 200); // Coordinates - wider for lat,lon display
    routeTree->header()->resizeSection(2, 100); // Distance - for distance values
    routeTree->header()->resizeSection(3, 80);  // Status
    routeTree->header()->resizeSection(4, 60);  // Active - sized for checkbox
    routeTree->header()->setStretchLastSection(true); // Description
    
    routesLayout->addWidget(routeTree);
    mainLayout->addWidget(routesGroup);
    
    // Buttons
    buttonLayout = new QHBoxLayout();
    
    exportAllButton = new QPushButton("Export All Routes");
    closeButton = new QPushButton("Close");
    
    buttonLayout->addWidget(exportAllButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Consistent styling with RouteFormDialog - minimal styling
}

void RouteDetailDialog::connectSignals()
{
    // Tree selection signals
    connect(routeTree, &QTreeWidget::itemSelectionChanged, this, &RouteDetailDialog::onRouteTreeSelectionChanged);
    
    // Tree click signals for toggling active status
    connect(routeTree, &QTreeWidget::itemClicked, this, &RouteDetailDialog::onTreeItemClicked);
    
    // Button signals
    connect(exportAllButton, &QPushButton::clicked, this, &RouteDetailDialog::onExportAllClicked);
    connect(closeButton, &QPushButton::clicked, this, &RouteDetailDialog::onCloseClicked);
}

void RouteDetailDialog::loadAllRoutesData()
{
    allRoutesData.clear();
    
    if (!ecWidget) return;
    
    // Get all waypoints
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    
    // Group waypoints by route ID
    QMap<int, QList<EcWidget::Waypoint>> routeGroups;
    for (const auto& wp : waypoints) {
        if (wp.routeId > 0) { // Only route waypoints, skip single waypoints
            routeGroups[wp.routeId].append(wp);
        }
    }
    
    // Create route data for each route
    for (auto it = routeGroups.begin(); it != routeGroups.end(); ++it) {
        int routeId = it.key();
        AllRouteData routeData;
        
        routeData.routeId = routeId;
        
        // Convert EcWidget::Waypoint to RouteDetailWaypoint
        QList<EcWidget::Waypoint> ecWaypoints = it.value();
        routeData.waypoints.clear();
        for (const auto& ecWp : ecWaypoints) {
            RouteDetailWaypoint wp;
            wp.lat = ecWp.lat;
            wp.lon = ecWp.lon;
            wp.label = ecWp.label;
            wp.remark = ecWp.remark;
            wp.turningRadius = ecWp.turningRadius;
            wp.routeId = ecWp.routeId;
            wp.active = ecWp.active; // Copy active status from EcWidget waypoint
            routeData.waypoints.append(wp);
        }
        
        routeData.waypointCount = routeData.waypoints.size();
        routeData.visible = ecWidget->isRouteVisible(routeId);
        routeData.attachedToShip = ecWidget->isRouteAttachedToShip(routeId);
        
        // Get route name from routeList
        EcWidget::Route route = ecWidget->getRouteById(routeId);
        if (route.routeId != 0) {
            routeData.name = route.name;
            routeData.description = route.description;
        } else {
            routeData.name = QString("Route %1").arg(routeId);
            routeData.description = "";
        }
        
        // Calculate total distance
        routeData.totalDistance = 0.0;
        if (routeData.waypoints.size() >= 2) {
            for (int i = 0; i < routeData.waypoints.size() - 1; ++i) {
                const auto& wp1 = routeData.waypoints[i];
                const auto& wp2 = routeData.waypoints[i + 1];
                
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
                
                routeData.totalDistance += distance * 0.539957; // Convert km to nautical miles
            }
        }
        
        routeData.totalTime = routeData.totalDistance / 10.0; // Assume 10 knots speed
        
        allRoutesData.append(routeData);
    }
}

void RouteDetailDialog::updateRouteTree()
{
    routeTree->clear();
    
    int routeIndex = 0;
    for (const AllRouteData& routeData : allRoutesData) {
        // Create route parent item
        QTreeWidgetItem* routeItem = new QTreeWidgetItem(routeTree);
        
        // Route data in columns (6 columns now)
        routeItem->setText(0, QString("🗺️ %1 (ID: %2)").arg(routeData.name).arg(routeData.routeId)); // Routes & Waypoints
        routeItem->setText(1, QString("%1 waypoints").arg(routeData.waypointCount)); // Coordinates - show waypoint count for route level
        routeItem->setText(2, formatDistance(routeData.totalDistance)); // Distance - total route distance
        
        // Status with visibility info
        QString status = routeData.attachedToShip ? "Active" : "Inactive";
        if (!routeData.visible) status += " (Hidden)";
        routeItem->setText(3, status); // Status
        routeItem->setText(4, "-"); // Active column (N/A for route level)
        routeItem->setText(5, routeData.description); // Description
        
        // Set route item font and colors - default styling
        QFont routeFont = routeItem->font(0);
        routeFont.setBold(true);
        
        for (int col = 0; col < 6; ++col) {
            routeItem->setFont(col, routeFont);
            
            if (routeData.attachedToShip) {
                routeItem->setForeground(col, QBrush(QColor(0, 100, 255))); // Blue for active
            } else {
                routeItem->setForeground(col, QBrush(QColor(80, 80, 80))); // Dark gray for inactive
            }
        }
        
        // Add waypoint children
        for (int i = 0; i < routeData.waypoints.size(); ++i) {
            const RouteDetailWaypoint& waypoint = routeData.waypoints[i];
            int waypointIndex = i; // Define waypointIndex for lambda capture
            
            QTreeWidgetItem* waypointItem = new QTreeWidgetItem(routeItem);
            
            QString label = waypoint.label.isEmpty() ? QString("WP-%1").arg(i + 1) : waypoint.label;
            QString waypointIcon = waypoint.active ? "📍" : "⭕"; // Different icon for inactive
            waypointItem->setText(0, QString("   %1 %2").arg(waypointIcon).arg(label)); // Routes & Waypoints (indented)
            waypointItem->setText(1, QString("%1°, %2°").arg(waypoint.lat, 0, 'f', 6).arg(waypoint.lon, 0, 'f', 6)); // Coordinates
            
            // Calculate distance from previous waypoint for Distance column
            QString distanceText = "-";
            if (i > 0) {
                const RouteDetailWaypoint& prevWaypoint = routeData.waypoints[i-1];
                // Calculate distance using Haversine formula
                double lat1 = qDegreesToRadians(prevWaypoint.lat);
                double lon1 = qDegreesToRadians(prevWaypoint.lon);
                double lat2 = qDegreesToRadians(waypoint.lat);
                double lon2 = qDegreesToRadians(waypoint.lon);
                
                double dlat = lat2 - lat1;
                double dlon = lon2 - lon1;
                
                double a = qSin(dlat/2) * qSin(dlat/2) + qCos(lat1) * qCos(lat2) * qSin(dlon/2) * qSin(dlon/2);
                double c = 2 * qAtan2(qSqrt(a), qSqrt(1-a));
                double distance = 6371.0 * c; // Earth radius in km
                distance *= 0.539957; // Convert km to nautical miles
                
                distanceText = QString("%1 NM").arg(distance, 0, 'f', 2);
            }
            waypointItem->setText(2, distanceText); // Distance from previous waypoint
            waypointItem->setText(3, QString("WP %1").arg(i + 1)); // Status (waypoint index)
            // Create checkbox for Active status instead of text
            QCheckBox* activeCheckBox = new QCheckBox();
            activeCheckBox->setChecked(waypoint.active);
            // Use default checkbox styling for consistency
            
            // Center the checkbox in the cell
            QWidget* checkBoxWidget = new QWidget();
            QHBoxLayout* checkBoxLayout = new QHBoxLayout(checkBoxWidget);
            checkBoxLayout->addWidget(activeCheckBox);
            checkBoxLayout->setAlignment(Qt::AlignCenter);
            checkBoxLayout->setContentsMargins(0, 0, 0, 0);
            
            routeTree->setItemWidget(waypointItem, 4, checkBoxWidget);
            
            // Connect checkbox to toggle function - capture all needed variables
            connect(activeCheckBox, &QCheckBox::toggled, [this, routeIndex, waypointIndex, waypointItem](bool checked) {
                if (routeIndex < allRoutesData.size() && waypointIndex < allRoutesData[routeIndex].waypoints.size()) {
                    RouteDetailWaypoint& waypoint = allRoutesData[routeIndex].waypoints[waypointIndex];
                    waypoint.active = checked;
                    
                    // Update the corresponding waypoint in EcWidget
                    if (ecWidget) {
                        ecWidget->updateWaypointActiveStatus(waypoint.routeId, waypoint.lat, waypoint.lon, waypoint.active);
                    }
                    
                    // Update the visual styling of the waypoint row
                    updateWaypointRowStyling(waypointItem, waypoint);
                }
            });
            waypointItem->setText(5, waypoint.remark.isEmpty() ? "-" : waypoint.remark); // Description
            
            // Set waypoint item styling - simple default styling
            QFont waypointFont = waypointItem->font(0);
            waypointFont.setItalic(true); // Keep italic for waypoints as in original design
            
            // Apply font to all columns first
            for (int col = 0; col < 6; ++col) {
                waypointItem->setFont(col, waypointFont);
            }
            
            // Apply initial styling for the waypoint row (colors and icon)
            updateWaypointRowStyling(waypointItem, waypoint);
        }
        
        // Start collapsed, user can expand to see waypoints
        routeItem->setExpanded(false);
        
        // Increment routeIndex for next route
        routeIndex++;
    }
    
    // Auto-resize columns
    routeTree->resizeColumnToContents(0);
    routeTree->resizeColumnToContents(1);
    routeTree->resizeColumnToContents(2);
    routeTree->resizeColumnToContents(3);
    routeTree->resizeColumnToContents(4);
}


void RouteDetailDialog::updateStatistics()
{
    int totalRoutes = allRoutesData.size();
    int totalWaypoints = 0;
    double totalDistance = 0.0;
    QString activeRouteName = "None";
    
    for (const auto& routeData : allRoutesData) {
        totalWaypoints += routeData.waypointCount;
        totalDistance += routeData.totalDistance;
        
        if (routeData.attachedToShip) {
            activeRouteName = routeData.name;
        }
    }
    
    totalRoutesLabel->setText(QString("📊 Total Routes: %1").arg(totalRoutes));
    totalWaypointsLabel->setText(QString("📍 Total Waypoints: %1").arg(totalWaypoints));
    totalDistanceLabel->setText(QString("📏 Total Distance: %1").arg(formatDistance(totalDistance)));
    activeRouteLabel->setText(QString("⚡ Active Route: %1").arg(activeRouteName));
}

QString RouteDetailDialog::formatDistance(double distanceNM)
{
    return QString("%1 NM").arg(distanceNM, 0, 'f', 1);
}

QString RouteDetailDialog::formatTime(double hours)
{
    int h = static_cast<int>(hours);
    int m = static_cast<int>((hours - h) * 60);
    return QString("%1h %2m").arg(h).arg(m);
}

void RouteDetailDialog::updateWaypointRowStyling(QTreeWidgetItem* waypointItem, const RouteDetailWaypoint& waypoint)
{
    // Update icon based on active status
    QString currentText = waypointItem->text(0);
    QString label = currentText.mid(currentText.indexOf(' ', 3) + 1); // Extract label after icon
    QString waypointIcon = waypoint.active ? "📍" : "⭕";
    waypointItem->setText(0, QString("   %1 %2").arg(waypointIcon).arg(label));
    
    // Simple default coloring - consistent with other dialogs
    QColor waypointColor = waypoint.active ? QColor(120, 120, 120) : QColor(180, 180, 180);
    
    // Apply styling to all columns except the checkbox column (4)
    for (int col = 0; col < 6; ++col) {
        if (col != 4) { // Skip checkbox column
            waypointItem->setForeground(col, QBrush(waypointColor));
        }
    }
}

// Slot implementations
void RouteDetailDialog::onRouteTreeSelectionChanged()
{
    // Optional: Add tree selection handling if needed
    // Currently the tree is mainly for display, no additional action needed on selection
}

void RouteDetailDialog::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    // This method is now mainly for other interactions if needed
    // Checkbox handling is done through direct signal connections
    Q_UNUSED(item);
    Q_UNUSED(column);
}

void RouteDetailDialog::onExportAllClicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Export All Routes",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/all_routes.csv",
        "CSV Files (*.csv);;All Files (*)"
    );
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Error", "Could not open file for writing.");
        return;
    }
    
    QTextStream stream(&file);
    
    // Write CSV header
    stream << "Route ID,Route Name,Waypoint Index,Waypoint Label,Latitude,Longitude,Turning Radius,Remark,Route Distance,Route Status\n";
    
    // Write data
    for (const auto& routeData : allRoutesData) {
        for (int i = 0; i < routeData.waypoints.size(); ++i) {
            const auto& waypoint = routeData.waypoints[i];
            QString label = waypoint.label.isEmpty() ? QString("WP-%1").arg(i + 1) : waypoint.label;
            QString status = routeData.attachedToShip ? "Active" : "Inactive";
            
            stream << routeData.routeId << ","
                   << "\"" << routeData.name << "\","
                   << (i + 1) << ","
                   << "\"" << label << "\","
                   << waypoint.lat << ","
                   << waypoint.lon << ","
                   << waypoint.turningRadius << ","
                   << "\"" << waypoint.remark << "\","
                   << routeData.totalDistance << ","
                   << status << "\n";
        }
    }
    
    file.close();
    
    QMessageBox::information(this, "Export Complete", 
                           QString("All routes exported successfully to:\n%1").arg(fileName));
}

void RouteDetailDialog::onCloseClicked()
{
    accept();
}