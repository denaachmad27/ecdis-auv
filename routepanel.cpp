#include "routepanel.h"
#include "ecwidget.h"
#include "routedetaildialog.h"
#include "routedeviationdetector.h"
#include "appconfig.h"
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
#include <QRadioButton>
#include <QComboBox>
#include <QIntValidator>
#include <QColorDialog>

// ====== RouteTreeItem Implementation ======

// Helper: format decimal degrees to Deg-Min representation with hemisphere
static QString formatDegMin(double value, bool isLat)
{
    double absVal = qAbs(value);
    int deg = static_cast<int>(absVal);
    double minutes = (absVal - deg) * 60.0;
    QChar hemi;
    if (isLat) hemi = (value >= 0.0) ? 'N' : 'S';
    else hemi = (value >= 0.0) ? 'E' : 'W';
    return QString("%1° %2' %3")
        .arg(deg, 2, 10, QChar('0'))
        .arg(minutes, 0, 'f', 3)
        .arg(hemi);
}

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
    // Simplified, professional route display without excessive emojis
    QString activeStatus = routeInfo.attachedToShip ? " (Active)" : "";
    QString visibilityStatus = !routeInfo.visible ? " [Hidden]" : "";
    
    // Clean, professional route format
    QString routeText = QString("%1 - %2 NM%3%4")
                       .arg(routeInfo.name)
                       .arg(routeInfo.totalDistance, 0, 'f', 1)
                       .arg(activeStatus)
                       .arg(visibilityStatus);
    
    setText(0, routeText);
    
    // Theme-aware colors
    bool isDark = AppConfig::isDark();
    QColor defaultTextColor = isDark ? QColor(255, 255, 255) : QColor(0, 0, 0);
    QColor activeColor = isDark ? QColor(100, 200, 255) : QColor(0, 100, 200);
    QColor inactiveColor = isDark ? QColor(160, 160, 160) : QColor(120, 120, 120);
    QColor hiddenColor = isDark ? QColor(100, 100, 100) : QColor(150, 150, 150);
    
    // Professional font styling
    QFont font = this->font(0);
    font.setFamily("Segoe UI");
    font.setPixelSize(13); // Slightly smaller for better hierarchy
    font.setWeight(QFont::Medium);
    // Ensure base (visible) state is not italic
    font.setItalic(false);
    
    // Apply styling based on route state
    if (!routeInfo.visible) {
        // Hidden routes - muted appearance
        setData(0, Qt::ForegroundRole, QBrush(hiddenColor));
        QFont hiddenFont = font;
        hiddenFont.setItalic(true);
        hiddenFont.setWeight(QFont::Light);
        setFont(0, hiddenFont);
    } else if (routeInfo.attachedToShip) {
        // Active route - prominent but not overwhelming
        setData(0, Qt::ForegroundRole, QBrush(activeColor));
        QFont activeFont = font;
        activeFont.setItalic(false); // Explicitly clear italic for visible active state
        activeFont.setWeight(QFont::DemiBold);
        setFont(0, activeFont);
    } else {
        // Inactive route - normal appearance
        setData(0, Qt::ForegroundRole, QBrush(defaultTextColor));
        setFont(0, font);
    }
    
    // Remove custom background colors - let the tree widget handle it
    setData(0, Qt::BackgroundRole, QVariant());
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

void WaypointTreeItem::setActiveStatus(bool active)
{
    waypointData.active = active;
    updateDisplayText();
}

void WaypointTreeItem::refreshDisplay()
{
    updateDisplayText();
}

void WaypointTreeItem::updateDisplayText()
{
    // Clean, professional waypoint display without status text (checkbox handles it)
    QString waypointName = waypointData.label.isEmpty() ? QString("WP-%1").arg(waypointData.routeId) : waypointData.label;

    // Coordinates formatted in deg-minute with hemisphere
    QString latDM = formatDegMin(waypointData.lat, true);
    QString lonDM = formatDegMin(waypointData.lon, false);
    QString waypointText = QString("  %1 (%2, %3)")
                          .arg(waypointName)
                          .arg(latDM)
                          .arg(lonDM);
    
    setText(0, waypointText);
    
    // Set checkbox for active/inactive status in column 1
    setCheckState(1, waypointData.active ? Qt::Checked : Qt::Unchecked);
    
    // Theme-aware colors
    bool isDark = AppConfig::isDark();
    QColor defaultTextColor = isDark ? QColor(220, 220, 220) : QColor(60, 60, 60);
    QColor inactiveColor = isDark ? QColor(140, 140, 140) : QColor(150, 150, 150);
    
    // Professional font styling - smaller than route items for hierarchy
    QFont font = this->font(0);
    font.setFamily("Segoe UI");
    font.setPixelSize(11); // Smaller than route items for better hierarchy
    font.setWeight(QFont::Normal);
    
    // Apply styling based on waypoint state
    if (waypointData.active) {
        setData(0, Qt::ForegroundRole, QBrush(defaultTextColor));
        setFont(0, font);
    } else {
        // Inactive waypoints - muted appearance
        setData(0, Qt::ForegroundRole, QBrush(inactiveColor));
        QFont inactiveFont = font;
        inactiveFont.setItalic(true);
        setFont(0, inactiveFont);
    }
    
    // Remove custom background colors - let the tree widget handle it
    setData(0, Qt::BackgroundRole, QVariant());
}

// ====== RoutePanel Implementation ======

RoutePanel::RoutePanel(EcWidget* ecWidget, QWidget *parent)
    : QWidget(parent), ecWidget(ecWidget), selectedRouteId(-1)
{
    setupUI();
    setupConnections();
    
    // Don't refresh here - will be refreshed by MainWindow after data is loaded
}

// Helper function to get theme-aware colors
QString RoutePanel::getThemeAwareStyleSheet()
{
    // Define color scheme based on theme
    QString bgColor, borderColor, textColor;
    QString selectedBgColor, selectedTextColor;
    QString hoverBgColor, hoverTextColor, alternateRowColor;

    if (AppConfig::isDark()) {
        bgColor = "#2b2b2b";
        borderColor = "#555555";
        textColor = "#ffffff";
        selectedBgColor = "#0078d4";
        selectedTextColor = "#ffffff";
        hoverBgColor = "#404040";
        hoverTextColor = "#ffffff";
        alternateRowColor = "#333333";
    }
    else if (AppConfig::isLight()) {
        bgColor = "#ffffff";
        borderColor = "#d0d0d0";
        textColor = "#000000";
        selectedBgColor = "#3daee9";
        selectedTextColor = "#ffffff";
        hoverBgColor = "#e3f2fd";
        hoverTextColor = "#333333";
        alternateRowColor = "#f8f8f8";
    }
    else if (AppConfig::isDim()) {
        bgColor            = "#1e2a38";  // dasar biru keabu-abuan
        borderColor        = "#3a4a5a";  // biru-abu lebih terang untuk border
        textColor          = "#ffffff";  // teks putih
        selectedBgColor    = "#355273";  // biru lembut untuk seleksi
        selectedTextColor  = "#ffffff";
        hoverBgColor       = "#2b3b4c";  // sedikit lebih terang dari background
        hoverTextColor     = "#ffffff";
        alternateRowColor  = "#243447";  // untuk baris selang-seling
    }
    
    // Define checkbox border color based on theme
    QString checkboxBorderColor;
    if (AppConfig::isDark()) {
        checkboxBorderColor = "#ffffff"; // White border for dark mode
    }
    else if (AppConfig::isLight()) {
        checkboxBorderColor = "#000000"; // Black border for light mode
    }
    else if (AppConfig::isDim()) {
        checkboxBorderColor = "#ffffff"; // White border for dim mode
    }
    else {
        checkboxBorderColor = "#000000"; // Default to black
    }
    
    qDebug() << "[CHECKBOX-THEME] Selected checkbox border color:" << checkboxBorderColor
             << "(Dark:" << AppConfig::isDark() << "Light:" << AppConfig::isLight() << "Dim:" << AppConfig::isDim() << ")";
    qDebug() << "[CHECKBOX-THEME] Applied hover fixes:";
    qDebug() << "  - Checkbox checked:hover = #66BB6A (stays green)";
    qDebug() << "  - Checkbox unchecked:hover = rgba(76,175,80,0.3) (preview)";
    qDebug() << "  - Item selected:hover = " << selectedBgColor << " (stays blue highlight)";
    qDebug() << "[SPACING] Applied compact layout:";
    qDebug() << "  - Row padding: 3px↕ 8px↔ (was 6px↕ 8px↔)";
    qDebug() << "  - Row min-height: 22px (was 28px)"; 
    qDebug() << "  - Tree indentation: 16px (was 20px)";
    qDebug() << "  - Checkbox size: 14x14px (was 16x16px)";
    
    // Simplified stylesheet to avoid potential crashes on some platforms/styles
    return QString(
        "QTreeWidget {"
        "    background-color: %1;"
        "    border: 1px solid %2;"
        "    border-radius: 6px;"
        "    padding: 4px;"
        "    color: %3;"
        "    alternate-background-color: %8;"
        "}"
        "QTreeWidget::item {"
        "    padding: 3px 8px;"
        "    border: none;"
        "    min-height: 22px;"
        "    border-radius: 3px;"
        "    color: %3;"
        "}"
        "QTreeWidget::item:selected {"
        "    background-color: %4;"
        "    color: %5;"
        "    border-radius: 3px;"
        "}"
        "QTreeWidget::item:hover {"
        "    background-color: %6;"
        "    color: %7;"
        "    border-radius: 3px;"
        "}"
        "QTreeWidget::item:selected:hover {"
        "    background-color: %4;"
        "    color: %5;"
        "}"
    ).arg(bgColor).arg(borderColor).arg(textColor).arg(selectedBgColor)
     .arg(selectedTextColor).arg(hoverBgColor).arg(hoverTextColor).arg(alternateRowColor)
     .arg(checkboxBorderColor);
}

void RoutePanel::updateThemeAwareStyles()
{
    // Update tree widget styling when theme changes
    QString newStyleSheet = getThemeAwareStyleSheet();
    routeTreeWidget->setStyleSheet(newStyleSheet);
    
    qDebug() << "[THEME] RoutePanel updated theme-aware styles";
    qDebug() << "[THEME] Current theme - Dark:" << AppConfig::isDark() << "Light:" << AppConfig::isLight() << "Dim:" << AppConfig::isDim();
    
    // Force refresh of all items to update their colors
    refreshRouteList();
}

RoutePanel::~RoutePanel()
{
}

void RoutePanel::setupUI()
{
    // Set minimum width to accommodate lat/lon coordinates properly
    setMinimumWidth(320);
    
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
    QGridLayout* routeManagementLayout = new QGridLayout();
    routeManagementGroup->setLayout(routeManagementLayout);
    
    addRouteButton = new QPushButton("Add Route");
    saveRouteButton = new QPushButton("Save");
    loadRouteButton = new QPushButton("Load");
    importRoutesButton = new QPushButton("Import");
    exportRoutesButton = new QPushButton("Export");
    refreshButton = new QPushButton("Refresh");
    clearAllButton = new QPushButton("Clear All");
    
    addRouteButton->setToolTip("Create new route");
    saveRouteButton->setToolTip("Save selected route to library");
    loadRouteButton->setToolTip("Load a route from library");
    importRoutesButton->setToolTip("Import routes from CSV file");
    exportRoutesButton->setToolTip("Export all routes to JSON file");
    refreshButton->setToolTip("Refresh route list");
    clearAllButton->setToolTip("Clear all routes");
    
    // Layout buttons in 3 columns, 2 rows
    // Row 1: Add, Save, Load
    routeManagementLayout->addWidget(addRouteButton, 0, 0);
    routeManagementLayout->addWidget(saveRouteButton, 0, 1);
    routeManagementLayout->addWidget(loadRouteButton, 0, 2);
    // Row 2: Clear All only (keep UI clean). Import/Export/Refresh hidden.
    routeManagementLayout->addWidget(clearAllButton, 1, 0);
    // Keep placeholders off-panel
    importRoutesButton->setVisible(false);
    exportRoutesButton->setVisible(false);
    refreshButton->setVisible(false);

    // Ensure export is available in all modes
    // Import/Export moved to main menu; keep hidden here
    
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
    routeTreeWidget->setMinimumHeight(100);
    routeTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    routeTreeWidget->setSelectionBehavior(QAbstractItemView::SelectRows); // Select entire rows
    routeTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Setup columns: [Route/Waypoint Name] [Active Checkbox]
    routeTreeWidget->setColumnCount(2);
    QStringList headers;
    headers << "Route / Waypoint" << "Active";
    routeTreeWidget->setHeaderLabels(headers);
    routeTreeWidget->setHeaderHidden(false); // Show headers for clarity
    
    // Set column widths
    routeTreeWidget->setColumnWidth(0, 300); // Main content column
    routeTreeWidget->setColumnWidth(1, 60);  // Checkbox column
    routeTreeWidget->header()->setStretchLastSection(false);
    routeTreeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    routeTreeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    
    routeTreeWidget->setRootIsDecorated(true); // Show expand/collapse indicators
    routeTreeWidget->setIndentation(16); // Further reduced indentation for more compact layout
    routeTreeWidget->setUniformRowHeights(false); // Allow different row heights
    routeTreeWidget->setAlternatingRowColors(true); // Enable alternating row colors
    routeTreeWidget->setAllColumnsShowFocus(true); // Ensure full row selection highlighting
    
    // Apply theme-aware professional styling
    routeTreeWidget->setStyleSheet(getThemeAwareStyleSheet());
    
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
    addWaypointButton->setEnabled(false); // Enable only when a route is selected
    editWaypointButton->setEnabled(false);
    deleteWaypointButton->setEnabled(false);
    moveUpButton->setEnabled(false);
    moveDownButton->setEnabled(false);
    duplicateWaypointButton->setEnabled(false);
    toggleActiveButton->setEnabled(false);
    
    // Hide toggle button since it's replaced by checkbox in Available routes
    toggleActiveButton->setVisible(false);
    
    // Layout buttons in 3 columns, 2 rows
    waypointManagementLayout->addWidget(addWaypointButton, 0, 0);
    waypointManagementLayout->addWidget(editWaypointButton, 0, 1);
    waypointManagementLayout->addWidget(deleteWaypointButton, 0, 2);
    waypointManagementLayout->addWidget(moveUpButton, 1, 0);
    waypointManagementLayout->addWidget(moveDownButton, 1, 1);
    waypointManagementLayout->addWidget(duplicateWaypointButton, 1, 2);
    
    listGroupLayout->addWidget(waypointManagementGroup);
    mainLayout->addWidget(routeListGroup);
    
    // Route Details Group (consistent with other GroupBox styles)
    routeInfoGroup = new QGroupBox("Route Details");
    routeInfoGroup->setStyleSheet(
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
    QVBoxLayout* infoGroupLayout = new QVBoxLayout();
    routeInfoGroup->setLayout(infoGroupLayout);
    
    // Route Info Display Grid
    QGridLayout* infoLayout = new QGridLayout();
    
    // Labels in grid layout like CPA/TCPA
    routeNameLabel = new QLabel("-");
    waypointCountLabel = new QLabel("-");
    totalDistanceLabel = new QLabel("-");
    totalTimeLabel = new QLabel("--:--:--");
    timeToGoLabel = new QLabel("-- -- ---- --:--:--");
    
    infoLayout->addWidget(new QLabel("Route:"), 0, 0);
    infoLayout->addWidget(routeNameLabel, 0, 1);
    infoLayout->addWidget(new QLabel("Points:"), 1, 0);
    infoLayout->addWidget(waypointCountLabel, 1, 1);
    infoLayout->addWidget(new QLabel("Distance:"), 2, 0);
    infoLayout->addWidget(totalDistanceLabel, 2, 1);

    // TTG information
    infoLayout->addWidget(new QLabel("Time To Go:"), 3, 0);
    infoLayout->addWidget(timeToGoLabel, 3, 1);

    // ETA information
    infoLayout->addWidget(new QLabel("ETA:"), 4, 0);
    infoLayout->addWidget(totalTimeLabel, 4, 1);
    
    // Add info layout to group
    infoGroupLayout->addLayout(infoLayout);
    
    // Route Actions Group (nested within Route Details)
    QGroupBox* actionsGroup = new QGroupBox("Route Actions");
    actionsGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: normal;"
        "    border: 1px solid #a0a0a0;"
        "    border-radius: 3px;"
        "    margin-top: 5px;"
        "    padding-top: 5px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 3px 0 3px;"
        "}"
    );
    QGridLayout* actionsLayout = new QGridLayout();
    actionsGroup->setLayout(actionsLayout);
    
    // Visibility checkbox and ship attachment buttons
    visibilityCheckBox = new QCheckBox("Show on Chart");
    addToShipButton = new QPushButton("Attach");
    detachFromShipButton = new QPushButton("Detach");
    routeDeviationCheckBox = new QCheckBox("Track Deviation Alert");

    addToShipButton->setToolTip("Attach this route to ship navigation (only one route can be attached)");
    detachFromShipButton->setToolTip("Remove this route from ship navigation");
    routeDeviationCheckBox->setToolTip("Enable route deviation detection (shows alert when off track)");

    // Awal: addToShip aktif, detachFromShip pasif
    addToShipButton->setEnabled(true);
    detachFromShipButton->setEnabled(false);

    addToShipButton->setVisible(false);
    detachFromShipButton->setVisible(false);

    // Route deviation checkbox - only visible in development mode
    routeDeviationCheckBox->setVisible(AppConfig::isDevelopment());
    routeDeviationCheckBox->setChecked(true);  // Enabled by default when visible

    // Layout actions in rows
    actionsLayout->addWidget(visibilityCheckBox, 0, 0);
    actionsLayout->addWidget(routeDeviationCheckBox, 0, 1);  // Row 0, column 1
    actionsLayout->addWidget(addToShipButton, 1, 0);  // Row 1, column 0
    actionsLayout->addWidget(detachFromShipButton, 1, 1);  // Row 1, column 1
    
    // Add actions group to info group
    infoGroupLayout->addWidget(actionsGroup);
    
    mainLayout->addWidget(routeInfoGroup);

    // Button states are now managed by updateRouteInfoDisplay based on actual attachment status
    
    // Context menus
    // Route context menu
    routeContextMenu = new QMenu(this);
    renameRouteAction = routeContextMenu->addAction("Rename Route");
    duplicateRouteAction = routeContextMenu->addAction("Duplicate Route");
    exportRouteAction = routeContextMenu->addAction("Export Route");
    changeColorAction = routeContextMenu->addAction("Change Color...");
    reverseRouteAction = routeContextMenu->addAction("Reverse Route");
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
    // Double-click disabled - not needed currently
    // connect(routeTreeWidget, &QTreeWidget::itemDoubleClicked, 
    //         this, &RoutePanel::onRouteItemDoubleClicked);
    connect(routeTreeWidget, &QTreeWidget::customContextMenuRequested, 
            this, &RoutePanel::onShowContextMenu);
    connect(routeTreeWidget, &QTreeWidget::itemChanged,
            this, &RoutePanel::onTreeItemChanged);
    
    // Route Management Button connections
    connect(addRouteButton, &QPushButton::clicked, this, &RoutePanel::onAddRouteClicked);
    // Import/Export moved to main menu; keep handlers accessible but buttons hidden
    connect(importRoutesButton, &QPushButton::clicked, this, &RoutePanel::onImportRoutesClicked);
    connect(exportRoutesButton, &QPushButton::clicked, this, &RoutePanel::onExportRoutesClicked);
    // Refresh button removed to simplify UI
    // connect(refreshButton, &QPushButton::clicked, this, &RoutePanel::onRefreshClicked);
    connect(clearAllButton, &QPushButton::clicked, this, &RoutePanel::onClearAllClicked);
    connect(saveRouteButton, &QPushButton::clicked, this, &RoutePanel::onSaveRouteClicked);
    connect(loadRouteButton, &QPushButton::clicked, this, &RoutePanel::onLoadRouteClicked);
    
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
            // Persist visibility change to routes.json
            ecWidget->saveRoutes();
            ecWidget->Draw(); // Use Draw() like route selection fix
            emit routeVisibilityChanged(selectedRouteId, checked);
            emit statusMessage(QString("Route %1 %2").arg(selectedRouteId).arg(checked ? "shown" : "hidden"));
            // Update only this route item's title/styling to reflect [Hidden]/italic
            refreshRouteItem(selectedRouteId);
            // Also keep the info panel in sync
            updateRouteInfo(selectedRouteId);
        }
    });
    
    // Route deviation checkbox connection
    connect(routeDeviationCheckBox, &QCheckBox::toggled, [this](bool checked) {
        if (ecWidget && ecWidget->getRouteDeviationDetector()) {
            ecWidget->getRouteDeviationDetector()->setAutoCheckEnabled(checked);
            qDebug() << "[ROUTE-PANEL] Route deviation detection" << (checked ? "enabled" : "disabled");
            emit statusMessage(QString("Track Deviation Alert %1").arg(checked ? "enabled" : "disabled"));
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
            ecWidget->publishToMOOS("WAYPT_NAV", "");
            ecWidget->setOwnShipTrail(false);

            // Clear deviation alert when detaching
            if (ecWidget->getRouteDeviationDetector()) {
                ecWidget->getRouteDeviationDetector()->clearDeviation();
            }

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
    // Match delete confirmation with waypoint delete (Yes/No)
    connect(deleteRouteAction, &QAction::triggered, this, [this]() {
        if (selectedRouteId <= 0 || !ecWidget) return;
        int ret = QMessageBox::question(
            this,
            "Delete Route",
            QString("Are you sure you want to delete Route %1?").arg(selectedRouteId),
            QMessageBox::Yes | QMessageBox::No
        );
        if (ret != QMessageBox::Yes) return;
        bool success = ecWidget->deleteRoute(selectedRouteId);
        if (success) {
            refreshRouteList();
            clearRouteInfoDisplay();
            selectedRouteId = -1;
            emit statusMessage("Route deleted successfully");
        } else {
            QMessageBox::critical(this, "Delete Error",
                                  QString("Failed to delete Route %1").arg(selectedRouteId));
        }
    });
    connect(routePropertiesAction, &QAction::triggered, this, &RoutePanel::onRouteProperties);
    connect(changeColorAction, &QAction::triggered, this, &RoutePanel::onChangeRouteColor);
    connect(reverseRouteAction, &QAction::triggered, this, &RoutePanel::onReverseRoute);
    
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

    connect(ecWidget, &EcWidget::updateEta, [this](){
        bool attached = ecWidget->isRouteAttachedToShip(selectedRouteId);

        if (attached){
            if (!activeRoute.rteEta.isEmpty()){etaText = activeRoute.rteEta;}
            if (!activeRoute.rteTtg.isEmpty()){ttgText = activeRoute.rteTtg;}

            timeToGoLabel->setText(QString("⌛ %1").arg(ttgText));
            totalTimeLabel->setText(QString("📆 %1").arg(etaText));
        }
        else {
            timeToGoLabel->setText(QString("⌛ --:--:--"));
            totalTimeLabel->setText(QString("📆 --- -- ---- --:--:--"));
        }
    });
}

// Public wrappers for main menu actions (avoid private slot access issue)
void RoutePanel::handleImportRoutesFromMenu()
{
    onImportRoutesClicked();
}

void RoutePanel::handleExportRoutesFromMenu()
{
    onExportRoutesClicked();
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
        
        qDebug() << "[ROUTE-PANEL] Route" << routeId << "created with" << routeWaypoints.size() << "children";
        
        // Remember item to re-select
        if (routeId == previouslySelectedRouteId) {
            itemToSelect = routeItem;
            // Expand selected route to show its waypoints
            routeItem->setExpanded(true);
        } else {
            // Start collapsed to show expand/collapse indicators
            routeItem->setExpanded(false);
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
    } else {
        // No valid item to restore; clear selection state so buttons reflect no selection
        selectedRouteId = -1;
    }
    
    // Update title with count
    int routeCount = routeGroups.size();
    titleLabel->setText(QString("Routes (%1)").arg(routeCount));
    
    // Note: Routes are now collapsed by default to show expand/collapse indicators
    // Only selected route will be expanded automatically
    
    // Ensure button states reflect current selection/availability
    if (selectedRouteId <= 0) {
        clearRouteInfoDisplay();
    }
    updateButtonStates();
}

void RoutePanel::refreshRouteEta()
{
    if (!ecWidget) return;

    // Preserve current selection
    int previouslySelectedRouteId = selectedRouteId;

    // Restore selection and update info display
    if (previouslySelectedRouteId > 0) {
        selectedRouteId = previouslySelectedRouteId;
        RouteInfo info = calculateRouteInfo(selectedRouteId);
        updateRouteInfoDisplay(info);
    }
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
        if (ecWidget->getSpeedAverage() != 0){
            info.totalTime = totalDistance / ecWidget->getSpeedAverage();
        }
        else {
            info.totalTime = 0;
        }
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
    ecWidget->publishToMOOS("WAYPT_NAV", result);
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
    // Try to show ETA if available from EcWidget activeRoute or compute fallback
    ttgText = "--:--:--";
    etaText = "--- -- ---- --:--:--";

    // Prefer ETA/TTG from global activeRoute if available (updated by AIS/MOOS)
    // if (!activeRoute.rteEta.isEmpty()) {
    //     etaText = activeRoute.rteEta;
    // } else if (!activeRoute.rteTtg.isEmpty()) {
    //     etaText = QString("TTG %1").arg(activeRoute.rteTtg);
    // } else if (info.totalTime > 0.0) {
    //     etaText = formatTime(info.totalTime);
    // }

    timeToGoLabel->setText(QString("⌛ %1").arg(ttgText));
    totalTimeLabel->setText(QString("📆 %1").arg(etaText));
    
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

    // Check if selected route is attached to ship
    bool isRouteAttached = false;
    if (hasRouteSelected && ecWidget) {
        isRouteAttached = ecWidget->isRouteAttachedToShip(selectedRouteId);
    }

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

    // Enable Add Waypoint button only if route is selected AND not attached
    addWaypointButton->setEnabled(hasRouteSelected && !isRouteAttached);

    // Enable waypoint operations only if waypoint is selected AND route is not attached
    editWaypointButton->setEnabled(hasWaypointSelected && !isRouteAttached);
    deleteWaypointButton->setEnabled(hasWaypointSelected && !isRouteAttached);
    duplicateWaypointButton->setEnabled(hasWaypointSelected && !isRouteAttached);
    toggleActiveButton->setEnabled(hasWaypointSelected && !isRouteAttached);

    // Enable move buttons based on position AND route is not attached
    moveUpButton->setEnabled(canMoveUp && !isRouteAttached);
    moveDownButton->setEnabled(canMoveDown && !isRouteAttached);

    // Enable export if there are any routes
    bool hasRoutes = (routeTreeWidget->topLevelItemCount() > 0);
    exportRoutesButton->setEnabled(hasRoutes);

    // Disable route modification buttons if route is attached
    clearAllButton->setEnabled(!isRouteAttached);
    saveRouteButton->setEnabled(hasRouteSelected && !isRouteAttached);
    loadRouteButton->setEnabled(!isRouteAttached);
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
    
    // CRITICAL FIX: Skip refresh during waypoint reordering to prevent selection issues
    if (isReorderingWaypoints) {
        qDebug() << "[ROUTE-PANEL] *** SKIPPING REFRESH - isReorderingWaypoints = true ***";
        return;
    }
    
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
    
    qDebug() << "[SELECTION-CHANGED] onRouteItemSelectionChanged called - currentItem:" 
             << (currentItem ? "EXISTS" : "NULL");
    
    if (currentItem) {
        WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(currentItem);
        RouteTreeItem* routeItem = dynamic_cast<RouteTreeItem*>(currentItem);
        qDebug() << "[SELECTION-CHANGED] Item type - waypoint:" << (waypointItem ? "YES" : "NO")
                 << "route:" << (routeItem ? "YES" : "NO");
    }
    
    if (!currentItem) {
        clearRouteInfoDisplay();
        selectedRouteId = -1;
        // Keep waypoint highlight even when selection is cleared
        updateButtonStates();
        return;
    }
    
    // Handle selection of waypoint items differently
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(currentItem);
    if (waypointItem) {
        // When waypoint is selected, select its parent route
        int waypointRouteId = waypointItem->getWaypoint().routeId;
        qDebug() << "[WAYPOINT-SELECTION] Waypoint selected:" << waypointItem->getWaypoint().label
                 << "routeId:" << waypointRouteId << "current selectedRouteId:" << selectedRouteId;
        
        if (waypointRouteId != selectedRouteId) {
            qDebug() << "[WAYPOINT-SELECTION] Route change detected, updating selectedRouteId";
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
        
        // HIGHLIGHT WAYPOINT: Find waypoint index and highlight it on map
        if (ecWidget) {
            QTreeWidgetItem* parent = waypointItem->parent();
            if (parent) {
                int waypointIndex = parent->indexOfChild(waypointItem);
                qDebug() << "[WAYPOINT-HIGHLIGHT] About to highlight waypoint" << waypointItem->getWaypoint().label 
                         << "route:" << waypointRouteId << "index:" << waypointIndex;
                ecWidget->highlightWaypoint(waypointRouteId, waypointIndex);
                qDebug() << "[WAYPOINT-HIGHLIGHT] ✅ Successfully highlighted waypoint" << waypointItem->getWaypoint().label;
            }
        }
        
        // Update button states for waypoint selection
        updateButtonStates();
        return;
    }
    
    // Handle route item selection
    RouteTreeItem* routeItem = dynamic_cast<RouteTreeItem*>(currentItem);
    if (routeItem) {
        int newSelectedRouteId = routeItem->getRouteId();
        qDebug() << "[ROUTE-SELECTION] Route item selected, changing selectedRouteId from" << selectedRouteId << "to" << newSelectedRouteId;
        qDebug() << "[ROUTE-SELECTION] *** THIS WILL CLEAR WAYPOINT HIGHLIGHT ***";
        selectedRouteId = newSelectedRouteId;
        RouteInfo info = calculateRouteInfo(selectedRouteId);
        updateRouteInfoDisplay(info);
        
        // Set visual feedback in chart - SYNC with EcWidget's selectedRouteId
        if (ecWidget) {
            qDebug() << "[ROUTE-SELECTION] ⚠️ ROUTE SELECTED - This will CLEAR waypoint highlight!";
            qDebug() << "[SELECTED-ROUTE] Syncing EcWidget selectedRouteId to" << selectedRouteId;
            ecWidget->setSelectedRoute(selectedRouteId);
            // Clear waypoint highlight when route is selected (not waypoint)
            qDebug() << "[ROUTE-SELECTION] ❌ CLEARING WAYPOINT HIGHLIGHT due to route selection";
            ecWidget->clearWaypointHighlight(); // Clear highlight when route is selected
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

void RoutePanel::onTreeItemChanged(QTreeWidgetItem* item, int column)
{
    // Only handle checkbox changes in column 1 (Active column)
    if (column != 1) return;
    
    // Only handle waypoint items (routes don't have checkboxes)
    WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(item);
    if (!waypointItem) return;
    
    // Get checkbox state
    bool isChecked = (item->checkState(1) == Qt::Checked);
    
    // Get waypoint data
    EcWidget::Waypoint waypoint = waypointItem->getWaypoint();
    
    // Skip if state hasn't actually changed
    if (waypoint.active == isChecked) return;
    
    qDebug() << "[CHECKBOX] Waypoint" << waypoint.label << "active state changed from" 
             << waypoint.active << "to" << isChecked;
    
    // CRITICAL FIX: Prevent refresh during direct checkbox toggle to avoid collapse
    isReorderingWaypoints = true;
    qDebug() << "[CHECKBOX-FLAG] Set isReorderingWaypoints = true to prevent refresh during checkbox toggle";
    
    // Update waypoint in EcWidget
    if (ecWidget) {
        ecWidget->updateWaypointActiveStatus(waypoint.routeId, waypoint.lat, waypoint.lon, isChecked);
        
        // Update local waypoint data using public method
        waypointItem->setActiveStatus(isChecked);
        
        // Update route info if needed (distance might change based on active waypoints)
        if (selectedRouteId == waypoint.routeId) {
            RouteInfo info = calculateRouteInfo(selectedRouteId);
            updateRouteInfoDisplay(info);
        }
        
        emit statusMessage(QString("Waypoint '%1' %2").arg(waypoint.label).arg(isChecked ? "activated" : "deactivated"));
    }
    
    // CRITICAL FIX: Reset flag after operations
    isReorderingWaypoints = false;
    qDebug() << "[CHECKBOX-FLAG] Set isReorderingWaypoints = false after checkbox toggle";
}

void RoutePanel::onShowContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = routeTreeWidget->itemAt(pos);
    if (!item) return;

    if (dynamic_cast<RouteTreeItem*>(item)) {
        // Route item - show route context menu
        selectedRouteId = dynamic_cast<RouteTreeItem*>(item)->getRouteId();

        // Check if route is attached to ship
        bool isAttached = ecWidget ? ecWidget->isRouteAttachedToShip(selectedRouteId) : false;

        // Disable modification actions for attached routes
        renameRouteAction->setEnabled(!isAttached);
        deleteRouteAction->setEnabled(!isAttached);
        duplicateRouteAction->setEnabled(!isAttached);
        reverseRouteAction->setEnabled(!isAttached);
        changeColorAction->setEnabled(!isAttached);
        exportRouteAction->setEnabled(!isAttached);

        // Allow visibility and properties for all routes
        toggleVisibilityAction->setEnabled(true);
        routePropertiesAction->setEnabled(true);

        routeContextMenu->exec(routeTreeWidget->mapToGlobal(pos));
    } else if (dynamic_cast<WaypointTreeItem*>(item)) {
        // Waypoint item - show waypoint context menu
        routeTreeWidget->setCurrentItem(item); // Select the waypoint

        // Get the route ID from the waypoint's parent route
        WaypointTreeItem* waypointItem = dynamic_cast<WaypointTreeItem*>(item);
        int routeId = waypointItem->getWaypoint().routeId;

        // Check if route is attached to ship
        bool isAttached = ecWidget ? ecWidget->isRouteAttachedToShip(routeId) : false;

        // Disable all waypoint modification actions for attached routes
        editWaypointAction->setEnabled(!isAttached);
        duplicateWaypointAction->setEnabled(!isAttached);
        deleteWaypointAction->setEnabled(!isAttached);
        toggleActiveAction->setEnabled(!isAttached);
        insertBeforeAction->setEnabled(!isAttached);
        insertAfterAction->setEnabled(!isAttached);
        moveUpAction->setEnabled(!isAttached);
        moveDownAction->setEnabled(!isAttached);

        waypointContextMenu->exec(routeTreeWidget->mapToGlobal(pos));
    }
}

void RoutePanel::onChangeRouteColor()
{
    if (selectedRouteId <= 0 || !ecWidget) return;
    QColor initial = ecWidget->getBaseRouteColor(selectedRouteId);
    QColor chosen = QColorDialog::getColor(initial, this, "Select Route Color");
    if (!chosen.isValid()) return;
    ecWidget->setRouteCustomColor(selectedRouteId, chosen);
    refreshRouteList();
    emit statusMessage(QString("Route %1 color updated").arg(selectedRouteId));
}

void RoutePanel::onReverseRoute()
{
    if (selectedRouteId <= 0 || !ecWidget) {
        qDebug() << "[ROUTE-REVERSE] Invalid route ID or ecWidget";
        return;
    }

    // Confirmation dialog
    EcWidget::Route route = ecWidget->getRouteById(selectedRouteId);
    QString routeName = route.name.isEmpty() ? QString::number(selectedRouteId) : route.name;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Reverse Route",
        QString("Are you sure you want to reverse route '%1'?\n\n"
                "This will reverse the order of all waypoints in the route.").arg(routeName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    // Perform reverse
    bool success = ecWidget->reverseRoute(selectedRouteId);

    if (success) {
        refreshRouteList();
        updateRouteInfo(selectedRouteId);
        emit statusMessage(QString("Route '%1' reversed successfully").arg(routeName));
    } else {
        QMessageBox::warning(this, "Reverse Failed",
                           QString("Failed to reverse route '%1'").arg(routeName));
    }
}

void RoutePanel::onAddRouteClicked()
{
    // Offer choice between creating by click or by form
    QMessageBox modeBox(this);
    modeBox.setWindowTitle("Add Route");
    modeBox.setText("How do you want to create the route?");
    QPushButton* clickBtn = modeBox.addButton("By Click", QMessageBox::AcceptRole);
    QPushButton* formBtn = modeBox.addButton("By Form", QMessageBox::ActionRole);
    QPushButton* cancelBtn = modeBox.addButton(QMessageBox::Cancel);
    modeBox.setDefaultButton(clickBtn);
    modeBox.exec();

    if (modeBox.clickedButton() == cancelBtn) return;

    if (modeBox.clickedButton() == clickBtn) {
        // Start create-by-click mode (existing behavior)
        emit requestCreateRoute();
        return;
    }

    // Create by form: open the simplified quick create dialog
    if (ecWidget) {
        ecWidget->showCreateRouteQuickDialog();
    }
}

void RoutePanel::onSaveRouteClicked()
{
    if (selectedRouteId <= 0 || !ecWidget) {
        QMessageBox::information(this, "No Route Selected", "Select a route to save to library.");
        return;
    }
    // Show Windows-like confirmation dialog with Save, Save As, Cancel
    EcWidget::Route r = ecWidget->getRouteById(selectedRouteId);
    QString routeName = r.name.trimmed().isEmpty() ? QString("Route %1").arg(selectedRouteId) : r.name;

    QDialog dlg(this);
    dlg.setWindowTitle("Save Route");
    QVBoxLayout* vlay = new QVBoxLayout(&dlg);
    vlay->setContentsMargins(16,16,16,16);
    vlay->setSpacing(12);
    QLabel* text = new QLabel(QString("Do you want to save %1?").arg(routeName), &dlg);
    vlay->addWidget(text);
    QHBoxLayout* btns = new QHBoxLayout();
    btns->addStretch();
    QPushButton* saveBtn = new QPushButton("Save", &dlg);
    QPushButton* saveAsBtn = new QPushButton("Save As", &dlg);
    QPushButton* cancelBtn = new QPushButton("Cancel", &dlg);
    btns->setSpacing(10);
    btns->addWidget(saveBtn);
    btns->addWidget(saveAsBtn);
    btns->addWidget(cancelBtn);
    vlay->addLayout(btns);

    QObject::connect(saveBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    // Save As handled by lambda
    bool saveAsRequested = false;
    QObject::connect(saveAsBtn, &QPushButton::clicked, [&](){ saveAsRequested = true; dlg.accept(); });

    if (dlg.exec() != QDialog::Accepted) return;

    if (!saveAsRequested) {
        if (ecWidget->saveRouteToLibrary(selectedRouteId)) {
            emit statusMessage(QString("Route %1 saved to library").arg(selectedRouteId));
            QMessageBox::information(this, "Saved", "Route has been saved to library.");
        } else {
            QMessageBox::warning(this, "Save Failed", "Could not save route to library.");
        }
        return;
    }

    // Save As flow
    bool ok=false;
    QString newName = QInputDialog::getText(this, "Save Route As", "Route name:", QLineEdit::Normal, routeName, &ok);
    if (!ok || newName.trimmed().isEmpty()) return;
    if (ecWidget->saveRouteToLibraryAs(selectedRouteId, newName)) {
        // Keep names consistent across UI: rename actual route too
        ecWidget->renameRoute(selectedRouteId, newName);
        refreshRouteList();
        emit statusMessage(QString("Route saved as '%1' to library").arg(newName));
        QMessageBox::information(this, "Saved", "Route has been saved to library with a new name.");
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not save route to library.");
    }
}

void RoutePanel::onLoadRouteClicked()
{
    if (!ecWidget) return;
    QString libPath = ecWidget->getRouteLibraryFilePath();
    QFile f(libPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, "No Saved Routes", "No routes found in library.");
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QJsonArray arr = doc.object().value("saved_routes").toArray();
    if (arr.isEmpty()) {
        QMessageBox::information(this, "No Saved Routes", "No routes found in library.");
        return;
    }

    // Build selection dialog with checkboxes and details
    QDialog dlg(this);
    dlg.setWindowTitle("Load Route(s)");
    QVBoxLayout* vlay = new QVBoxLayout(&dlg);
    vlay->setContentsMargins(16,16,16,16);
    vlay->setSpacing(10);
    QLabel* info = new QLabel("Select one or more routes to load.", &dlg);
    vlay->addWidget(info);

    QTreeWidget* list = new QTreeWidget(&dlg);
    list->setColumnCount(3);
    list->setHeaderLabels({"Route Name", "Waypoints", "Distance (NM)"});
    list->setRootIsDecorated(false);
    list->setAlternatingRowColors(true);
    list->setUniformRowHeights(false);
    list->setStyleSheet("QTreeWidget::item{padding:6px 10px;} QTreeWidget{min-width:420px;}");
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Populate with metadata
    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        QString name = o.value("name").toString();
        QJsonArray wps = o.value("waypoints").toArray();
        int count = wps.size();
        double dist_m = 0.0;
        // Compute total distance
        if (count >= 2) {
            for (int i = 0; i < count - 1; ++i) {
                QJsonObject w1 = wps[i].toObject();
                QJsonObject w2 = wps[i+1].toObject();
                double lat1 = w1.value("lat").toDouble();
                double lon1 = w1.value("lon").toDouble();
                double lat2 = w2.value("lat").toDouble();
                double lon2 = w2.value("lon").toDouble();
                dist_m += ecWidget->haversine(lat1, lon1, lat2, lon2); // meters
            }
        }
        double dist_nm = dist_m / 1852.0; // convert meters to nautical miles
        auto* it = new QTreeWidgetItem(list);
        it->setText(0, name);
        it->setText(1, QString::number(count));
        it->setText(2, QString::number(dist_nm, 'f', 2));
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        it->setCheckState(0, Qt::Unchecked);
    }
    list->resizeColumnToContents(0);
    vlay->addWidget(list);

    // Simplified, predictable behavior:
    // - Click checkbox (col 0) toggles only that row
    // - Click other columns checks that row (does not uncheck others)
    QObject::connect(list, &QTreeWidget::itemClicked, &dlg, [list](QTreeWidgetItem* item, int column){
        if (!item) return;
        if (column == 0) {
            item->setCheckState(0, item->checkState(0) == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        } else {
            item->setCheckState(0, Qt::Checked);
        }
    });

    QHBoxLayout* btns = new QHBoxLayout();
    btns->addStretch();
    QPushButton* loadBtn = new QPushButton("Load Selected", &dlg);
    QPushButton* deleteBtn = new QPushButton("Delete Selected", &dlg);
    QPushButton* cancelBtn = new QPushButton("Cancel", &dlg);
    btns->setSpacing(10);
    btns->addWidget(loadBtn);
    btns->addWidget(deleteBtn);
    btns->addWidget(cancelBtn);
    vlay->addLayout(btns);
    QObject::connect(loadBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    // Delete selected from library
    QObject::connect(deleteBtn, &QPushButton::clicked, &dlg, [&, list, libPath]() {
        // Gather checked names
        QStringList toDelete;
        for (int i=0;i<list->topLevelItemCount();++i) {
            auto* it = list->topLevelItem(i);
            if (it->checkState(0) == Qt::Checked) toDelete << it->text(0);
        }
        if (toDelete.isEmpty()) {
            QMessageBox::information(&dlg, "No Selection", "Please check at least one route to delete.");
            return;
        }
        if (QMessageBox::question(&dlg, "Confirm Delete", QString("Delete %1 selected route(s) from library?").arg(toDelete.size())) != QMessageBox::Yes) {
            return;
        }
        // Load JSON, remove entries, save back
        QFile f(libPath);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(&dlg, "Delete Failed", "Could not open route library.");
            return;
        }
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        f.close();
        QJsonArray saved = d.object().value("saved_routes").toArray();
        QJsonArray newSaved;
        for (const auto &vv : saved) {
            QJsonObject o = vv.toObject();
            if (!toDelete.contains(o.value("name").toString())) newSaved.append(o);
        }
        QJsonObject root; root["saved_routes"] = newSaved;
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(&dlg, "Delete Failed", "Could not write route library.");
            return;
        }
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
        // Remove rows from UI
        for (int i=list->topLevelItemCount()-1;i>=0;--i) {
            auto* it = list->topLevelItem(i);
            if (it->checkState(0) == Qt::Checked) delete list->takeTopLevelItem(i);
        }
        QMessageBox::information(&dlg, "Deleted", "Selected routes deleted from library.");
    });

    if (dlg.exec() != QDialog::Accepted) return;

    // Load all checked routes
    int loaded = 0;
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        QTreeWidgetItem* it = list->topLevelItem(i);
        if (it->checkState(0) == Qt::Checked) {
            QString name = it->text(0);
            if (ecWidget->loadRouteFromLibrary(name)) {
                loaded++;
            }
        }
    }
    if (loaded > 0) {
        refreshRouteList();
        emit statusMessage(QString("Loaded %1 route(s) from library").arg(loaded));
    } else {
        QMessageBox::information(this, "No Selection", "No routes were selected to load.");
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
    
    // Create modern input dialog
    QInputDialog dialog(this);
    dialog.setWindowTitle("✏️ Rename Route");
    dialog.setLabelText(QString("Enter new name for %1:").arg(info.name));
    dialog.setTextValue(info.name);
    dialog.setInputMode(QInputDialog::TextInput);
    
    // Theme-aware styling
    dialog.setStyleSheet(getDialogStyleSheet());
    
    if (dialog.exec() == QDialog::Accepted) {
        QString newName = dialog.textValue().trimmed();
        if (!newName.isEmpty() && newName != info.name) {
            if (ecWidget && ecWidget->renameRoute(selectedRouteId, newName)) {
                // Update tree item and info panel without full rebuild
                refreshRouteItem(selectedRouteId);
                updateRouteInfo(selectedRouteId);
                emit statusMessage(QString("✅ Route renamed to '%1'").arg(newName));
            } else {
                QMessageBox::warning(this, "Rename Failed", "Could not rename the route.");
            }
        }
    }
}

void RoutePanel::onToggleRouteVisibility()
{
    if (selectedRouteId > 0 && ecWidget) {
        bool currentVisibility = ecWidget->isRouteVisible(selectedRouteId);
        ecWidget->setRouteVisibility(selectedRouteId, !currentVisibility);
        // Persist visibility change to routes.json
        ecWidget->saveRoutes();
        visibilityCheckBox->setChecked(!currentVisibility);
        ecWidget->update(); // Use lighter update instead of forceRedraw
        emit routeVisibilityChanged(selectedRouteId, !currentVisibility);
        // Reflect title/state changes immediately in the tree
        refreshRouteItem(selectedRouteId);
        updateRouteInfo(selectedRouteId);
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

void RoutePanel::refreshRouteItem(int routeId)
{
    // Update a single route item's display (text, color, italic) without full list rebuild
    RouteTreeItem* routeItem = findRouteItem(routeId);
    if (routeItem) {
        RouteInfo info = calculateRouteInfo(routeId);
        routeItem->updateFromRouteInfo(info);
    } else {
        // Fallback: if not found, rebuild list
        refreshRouteList();
    }
}

QString RoutePanel::getDialogStyleSheet() const
{
    QString bg, text, label, inputBg, inputText, border, primary, primaryHover;

    if (AppConfig::isDark()) {
        bg = "#2b2b2b"; text = "#ffffff"; label = "#e0e0e0";
        inputBg = "#3a3a3a"; inputText = "#ffffff"; border = "#5a5a5a";
        primary = "#0078d4"; primaryHover = "#1a86db";
    } else if (AppConfig::isDim()) {
        bg = "#1e2a38"; text = "#ffffff"; label = "#e8eef5";
        inputBg = "#243447"; inputText = "#ffffff"; border = "#3a4a5a";
        primary = "#355273"; primaryHover = "#3f5f85";
    } else { // Light
        bg = "#ffffff"; text = "#000000"; label = "#495057";
        inputBg = "#ffffff"; inputText = "#000000"; border = "#e9ecef";
        primary = "#007bff"; primaryHover = "#0056b3";
    }

    return QString(
        "QInputDialog { background-color: %1; font-family: 'Segoe UI'; color: %2; }"
        "QLabel { color: %3; font-size: 13px; font-weight: 500; }"
        "QLineEdit { border: 2px solid %6; border-radius: 4px; padding: 8px 12px; font-size: 13px; background-color: %4; color: %5; }"
        "QLineEdit:focus { border-color: %7; outline: none; }"
        "QPushButton { background-color: %7; color: #ffffff; border: none; border-radius: 4px; padding: 8px 16px; font-weight: 500; min-width: 80px; }"
        "QPushButton:hover { background-color: %8; }"
    ).arg(bg).arg(text).arg(label).arg(inputBg).arg(inputText).arg(border).arg(primary).arg(primaryHover);
}

// ====== Waypoint Operation Functions ======

void RoutePanel::showWaypointEditDialog(int routeId, int waypointIndex)
{
    QDialog dialog(this);
    dialog.setWindowTitle(waypointIndex >= 0 ? "Edit Waypoint" : "Add Waypoint");
    dialog.setModal(true);
    
    QFormLayout* layout = new QFormLayout(&dialog);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->setSizeConstraint(QLayout::SetFixedSize); // Make dialog auto-fit content
    
    QLineEdit* labelEdit = new QLineEdit();
    QLineEdit* latEdit = new QLineEdit();
    QLineEdit* lonEdit = new QLineEdit();
    QLineEdit* remarkEdit = new QLineEdit();
    QCheckBox* activeCheck = new QCheckBox();

    // Unit selection for coordinate input
    QGroupBox* unitGroup = new QGroupBox("Coordinate Units");
    QGridLayout* unitLayout = new QGridLayout(unitGroup);
    unitLayout->setContentsMargins(0, 0, 0, 0);
    unitLayout->setHorizontalSpacing(12);
    unitLayout->setVerticalSpacing(4);
    QRadioButton* decDegBtn = new QRadioButton("Decimal Degrees");
    QRadioButton* degMinBtn = new QRadioButton("Deg-Min");
    QRadioButton* degMinSecBtn = new QRadioButton("Deg-Min-Sec");
    QRadioButton* metersBtn = new QRadioButton("Meters (N/E)");
    decDegBtn->setChecked(true);
    unitLayout->addWidget(decDegBtn, 0, 0);
    unitLayout->addWidget(degMinBtn, 0, 1);
    unitLayout->addWidget(degMinSecBtn, 1, 0);
    unitLayout->addWidget(metersBtn, 1, 1);
    
    // Validators will be set based on unit selection
    auto setValidatorsForUnit = [&]() {
        if (decDegBtn->isChecked()) {
            latEdit->setValidator(new QDoubleValidator(-90.0, 90.0, 6, &dialog));
            lonEdit->setValidator(new QDoubleValidator(-180.0, 180.0, 6, &dialog));
        } else if (metersBtn->isChecked()) {
            latEdit->setValidator(new QDoubleValidator(-10000000.0, 10000000.0, 3, &dialog));
            lonEdit->setValidator(new QDoubleValidator(-10000000.0, 10000000.0, 3, &dialog));
        } else {
            // Deg-Min: allow free text (we parse manually)
            latEdit->setValidator(nullptr);
            lonEdit->setValidator(nullptr);
        }
    };
    setValidatorsForUnit();
    
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
    
    // Dynamic relabeling for meters/dec mode
    QLabel* latLabel = new QLabel("Latitude:");
    QLabel* lonLabel = new QLabel("Longitude:");
    // Removed examples helper for Deg-Min per user request
    
    // Build groups for different unit modes
    QGroupBox* decMetersGroup = new QGroupBox("Coordinates");
    QFormLayout* decMetersLayout = new QFormLayout(decMetersGroup);
    decMetersLayout->setContentsMargins(0, 0, 0, 0);
    decMetersLayout->setSpacing(4);
    decMetersGroup->setFlat(true);
    decMetersLayout->addRow(latLabel, latEdit);
    decMetersLayout->addRow(lonLabel, lonEdit);

    QGroupBox* degMinGroup = new QGroupBox("Deg-Min Coordinates");
    QGridLayout* degMinLayout = new QGridLayout(degMinGroup);
    degMinLayout->setContentsMargins(0, 0, 0, 0);
    degMinLayout->setHorizontalSpacing(6);
    degMinLayout->setVerticalSpacing(4);
    degMinGroup->setFlat(true);
    QLineEdit* latDegEdit = new QLineEdit(); latDegEdit->setValidator(new QIntValidator(0, 90, &dialog)); latDegEdit->setMaximumWidth(70);
    QLineEdit* latMinEdit = new QLineEdit(); latMinEdit->setValidator(new QDoubleValidator(0.0, 60.0, 3, &dialog)); latMinEdit->setMaximumWidth(100);
    QComboBox* latHem = new QComboBox(); latHem->addItems({"N", "S"}); latHem->setMaximumWidth(60);
    QLineEdit* lonDegEdit = new QLineEdit(); lonDegEdit->setValidator(new QIntValidator(0, 180, &dialog)); lonDegEdit->setMaximumWidth(70);
    QLineEdit* lonMinEdit = new QLineEdit(); lonMinEdit->setValidator(new QDoubleValidator(0.0, 60.0, 3, &dialog)); lonMinEdit->setMaximumWidth(100);
    QComboBox* lonHem = new QComboBox(); lonHem->addItems({"E", "W"}); lonHem->setMaximumWidth(60);
    degMinLayout->addWidget(new QLabel("Lat Deg"), 0, 0);
    degMinLayout->addWidget(latDegEdit, 0, 1);
    degMinLayout->addWidget(new QLabel("Lat Min"), 0, 2);
    degMinLayout->addWidget(latMinEdit, 0, 3);
    degMinLayout->addWidget(new QLabel("N/S"), 0, 4);
    degMinLayout->addWidget(latHem, 0, 5);
    degMinLayout->addWidget(new QLabel("Lon Deg"), 1, 0);
    degMinLayout->addWidget(lonDegEdit, 1, 1);
    degMinLayout->addWidget(new QLabel("Lon Min"), 1, 2);
    degMinLayout->addWidget(lonMinEdit, 1, 3);
    degMinLayout->addWidget(new QLabel("E/W"), 1, 4);
    degMinLayout->addWidget(lonHem, 1, 5);

    QGroupBox* degMinSecGroup = new QGroupBox("Deg-Min-Sec Coordinates");
    QGridLayout* degMinSecLayout = new QGridLayout(degMinSecGroup);
    degMinSecLayout->setContentsMargins(0, 0, 0, 0);
    degMinSecLayout->setHorizontalSpacing(6);
    degMinSecLayout->setVerticalSpacing(4);
    degMinSecGroup->setFlat(true);
    QLineEdit* latDegDmsEdit = new QLineEdit(); latDegDmsEdit->setValidator(new QIntValidator(0, 90, &dialog)); latDegDmsEdit->setMaximumWidth(70);
    QLineEdit* latMinDmsEdit = new QLineEdit(); latMinDmsEdit->setValidator(new QIntValidator(0, 59, &dialog)); latMinDmsEdit->setMaximumWidth(70);
    QLineEdit* latSecDmsEdit = new QLineEdit(); latSecDmsEdit->setValidator(new QDoubleValidator(0.0, 60.0, 3, &dialog)); latSecDmsEdit->setMaximumWidth(90);
    QComboBox* latHemDms = new QComboBox(); latHemDms->addItems({"N", "S"}); latHemDms->setMaximumWidth(60);
    QLineEdit* lonDegDmsEdit = new QLineEdit(); lonDegDmsEdit->setValidator(new QIntValidator(0, 180, &dialog)); lonDegDmsEdit->setMaximumWidth(70);
    QLineEdit* lonMinDmsEdit = new QLineEdit(); lonMinDmsEdit->setValidator(new QIntValidator(0, 59, &dialog)); lonMinDmsEdit->setMaximumWidth(70);
    QLineEdit* lonSecDmsEdit = new QLineEdit(); lonSecDmsEdit->setValidator(new QDoubleValidator(0.0, 60.0, 3, &dialog)); lonSecDmsEdit->setMaximumWidth(90);
    QComboBox* lonHemDms = new QComboBox(); lonHemDms->addItems({"E", "W"}); lonHemDms->setMaximumWidth(60);
    degMinSecLayout->addWidget(new QLabel("Lat Deg"), 0, 0);
    degMinSecLayout->addWidget(latDegDmsEdit, 0, 1);
    degMinSecLayout->addWidget(new QLabel("Lat Min"), 0, 2);
    degMinSecLayout->addWidget(latMinDmsEdit, 0, 3);
    degMinSecLayout->addWidget(new QLabel("Lat Sec"), 0, 4);
    degMinSecLayout->addWidget(latSecDmsEdit, 0, 5);
    degMinSecLayout->addWidget(new QLabel("N/S"), 0, 6);
    degMinSecLayout->addWidget(latHemDms, 0, 7);
    degMinSecLayout->addWidget(new QLabel("Lon Deg"), 1, 0);
    degMinSecLayout->addWidget(lonDegDmsEdit, 1, 1);
    degMinSecLayout->addWidget(new QLabel("Lon Min"), 1, 2);
    degMinSecLayout->addWidget(lonMinDmsEdit, 1, 3);
    degMinSecLayout->addWidget(new QLabel("Lon Sec"), 1, 4);
    degMinSecLayout->addWidget(lonSecDmsEdit, 1, 5);
    degMinSecLayout->addWidget(new QLabel("E/W"), 1, 6);
    degMinSecLayout->addWidget(lonHemDms, 1, 7);

    // Pre-fill deg-min widgets from decimal degree fields (if available)
    {
        bool ok1=false, ok2=false;
        double alat = latEdit->text().toDouble(&ok1);
        double alon = lonEdit->text().toDouble(&ok2);
        if (!ok1) alat = 0.0;
        if (!ok2) alon = 0.0;
        int dlat = static_cast<int>(qFloor(qAbs(alat)));
        double mlat = (qAbs(alat) - dlat) * 60.0;
        latDegEdit->setText(QString::number(dlat));
        latMinEdit->setText(QString::number(mlat, 'f', 3));
        latHem->setCurrentText(alat >= 0 ? "N" : "S");
        int latMinWhole = static_cast<int>(qFloor(mlat));
        double latSeconds = (mlat - latMinWhole) * 60.0;
        latDegDmsEdit->setText(QString::number(dlat));
        latMinDmsEdit->setText(QString::number(latMinWhole));
        latSecDmsEdit->setText(QString::number(latSeconds, 'f', 2));
        latHemDms->setCurrentText(alat >= 0 ? "N" : "S");
        int dlon = static_cast<int>(qFloor(qAbs(alon)));
        double mlon = (qAbs(alon) - dlon) * 60.0;
        lonDegEdit->setText(QString::number(dlon));
        lonMinEdit->setText(QString::number(mlon, 'f', 3));
        lonHem->setCurrentText(alon >= 0 ? "E" : "W");
        int lonMinWhole = static_cast<int>(qFloor(mlon));
        double lonSeconds = (mlon - lonMinWhole) * 60.0;
        lonDegDmsEdit->setText(QString::number(dlon));
        lonMinDmsEdit->setText(QString::number(lonMinWhole));
        lonSecDmsEdit->setText(QString::number(lonSeconds, 'f', 2));
        lonHemDms->setCurrentText(alon >= 0 ? "E" : "W");
    }

    // Toggle visibility when switching units
    auto updateCoordinateLabels = [&]() {
        if (metersBtn->isChecked()) {
            latLabel->setText("North (m):");
            lonLabel->setText("East (m):");
            latEdit->setPlaceholderText("e.g., 25.0");
            lonEdit->setPlaceholderText("e.g., 50.0");

            decMetersGroup->setVisible(true);
            degMinGroup->setVisible(false);
            degMinSecGroup->setVisible(false);
        } else if (degMinBtn->isChecked()) {
            decMetersGroup->setVisible(false);
            degMinGroup->setVisible(true);
            degMinSecGroup->setVisible(false);
        } else if (degMinSecBtn->isChecked()) {
            decMetersGroup->setVisible(false);
            degMinGroup->setVisible(false);
            degMinSecGroup->setVisible(true);
        } else {
            latLabel->setText("Latitude:");
            lonLabel->setText("Longitude:");
            latEdit->setPlaceholderText("e.g., -7.508333");
            lonEdit->setPlaceholderText("e.g., 112.754167");

            decMetersGroup->setVisible(true);
            degMinGroup->setVisible(false);
            degMinSecGroup->setVisible(false);
        }
        setValidatorsForUnit();
        dialog.adjustSize();
    };
    QObject::connect(decDegBtn, &QRadioButton::toggled, &dialog, updateCoordinateLabels);
    QObject::connect(degMinBtn, &QRadioButton::toggled, &dialog, updateCoordinateLabels);
    QObject::connect(degMinSecBtn, &QRadioButton::toggled, &dialog, updateCoordinateLabels);
    QObject::connect(metersBtn, &QRadioButton::toggled, &dialog, updateCoordinateLabels);
    updateCoordinateLabels();
    layout->addRow("Label:", labelEdit);
    layout->addRow(unitGroup);
    layout->addRow(decMetersGroup);
    layout->addRow(degMinGroup);
    layout->addRow(degMinSecGroup);
    // No examples row for Deg-Min per request
    layout->addRow("Remark:", remarkEdit);
    layout->addRow("Active:", activeCheck);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);
    QPushButton* okBtn = new QPushButton("OK");
    QPushButton* cancelBtn = new QPushButton("Cancel");
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    
    layout->addRow(buttonLayout);
    
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    // Helper for deg-min to decimal
    auto toDecimalFromDegMin = [&](QLineEdit* degEdit, QLineEdit* minEdit, QComboBox* hem, bool isLat, bool& ok) -> double {
        ok = false;
        bool okd=false, okm=false;
        int deg = degEdit->text().toInt(&okd);
        double minu = minEdit->text().toDouble(&okm);
        if (!okd || !okm) return 0.0;
        if (isLat && (deg < 0 || deg > 90)) return 0.0;
        if (!isLat && (deg < 0 || deg > 180)) return 0.0;
        if (minu < 0.0 || minu >= 60.0) return 0.0;
        double val = deg + (minu / 60.0);
        QString h = hem->currentText().toUpper();
        if ((isLat && h == "S") || (!isLat && h == "W")) val = -val;
        ok = true;
        return val;
    };

    auto toDecimalFromDegMinSec = [&](QLineEdit* degEdit, QLineEdit* minEdit, QLineEdit* secEdit, QComboBox* hem, bool isLat, bool& ok) -> double {
        ok = false;
        bool okd=false, okm=false, oks=false;
        int deg = degEdit->text().toInt(&okd);
        int minu = minEdit->text().toInt(&okm);
        double secs = secEdit->text().toDouble(&oks);
        if (!okd || !okm || !oks) return 0.0;
        if (isLat && (deg < 0 || deg > 90)) return 0.0;
        if (!isLat && (deg < 0 || deg > 180)) return 0.0;
        if (minu < 0 || minu >= 60) return 0.0;
        if (secs < 0.0 || secs >= 60.0) return 0.0;
        double val = deg + (static_cast<double>(minu) / 60.0) + (secs / 3600.0);
        QString h = hem->currentText().toUpper();
        if ((isLat && h == "S") || (!isLat && h == "W")) val = -val;
        ok = true;
        return val;
    };

    dialog.adjustSize();
    if (dialog.exec() == QDialog::Accepted) {
        bool okLat = true, okLon = true;
        double lat = 0.0, lon = 0.0;
        if (decDegBtn->isChecked()) {
            lat = latEdit->text().toDouble(&okLat);
            lon = lonEdit->text().toDouble(&okLon);
        } else if (degMinBtn->isChecked()) {
            lat = toDecimalFromDegMin(latDegEdit, latMinEdit, latHem, true, okLat);
            lon = toDecimalFromDegMin(lonDegEdit, lonMinEdit, lonHem, false, okLon);
        } else if (degMinSecBtn->isChecked()) {
            lat = toDecimalFromDegMinSec(latDegDmsEdit, latMinDmsEdit, latSecDmsEdit, latHemDms, true, okLat);
            lon = toDecimalFromDegMinSec(lonDegDmsEdit, lonMinDmsEdit, lonSecDmsEdit, lonHemDms, false, okLon);
        } else if (metersBtn->isChecked()) {
            double north = latEdit->text().toDouble(&okLat);
            double east = lonEdit->text().toDouble(&okLon);
            // Convert meters offset from last waypoint in this route
            if (okLat && okLon && ecWidget) {
                QList<EcWidget::Waypoint> wps = ecWidget->getWaypoints();
                EcWidget::Waypoint lastWp; bool found=false;
                for (int i = wps.size()-1; i >= 0; --i) {
                    if (wps[i].routeId == routeId) { lastWp = wps[i]; found=true; break; }
                }
                if (found) {
                    double lat0 = qDegreesToRadians(lastWp.lat);
                    double nm_per_meter = 1.0 / 1852.0;
                    double dlat_deg = (north * nm_per_meter) / 60.0;
                    double dlon_deg = (east * nm_per_meter) / (60.0 * qCos(lat0));
                    lat = lastWp.lat + dlat_deg;
                    lon = lastWp.lon + dlon_deg;
                } else {
                    okLat = okLon = false;
                }
            } else {
                okLat = okLon = false;
            }
        }
        QString label = labelEdit->text().trimmed();
        QString remark = remarkEdit->text().trimmed();
        bool active = activeCheck->isChecked();
        
        // Validate
        if (!okLat || !okLon || lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
            QMessageBox::warning(this, "Invalid Coordinates", 
                "Please enter valid coordinates or offsets.\nLatitude: -90.0 to 90.0\nLongitude: -180.0 to 180.0");
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
                    
                    // Replace waypoints for route and force persist
                    ecWidget->replaceWaypointsForRoute(routeId, routeWaypoints);
                    ecWidget->saveRoutes();
                    refreshRouteList();
                    if (selectedRouteId == routeId) {
                        updateRouteInfo(selectedRouteId);
                    }
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
    
    // CRITICAL FIX: Prevent onWaypointAdded from triggering refreshRouteList during reorder
    isReorderingWaypoints = true;
    qDebug() << "[REORDER-FLAG] Set isReorderingWaypoints = true";
    
    // ANTI-FLICKER: Preserve current highlight information to maintain it during rebuild
    bool hadHighlight = false;
    if (ecWidget) {
        // Check if we currently have a highlighted waypoint - we'll restore it manually
        hadHighlight = true; // Assume we have highlight since we're in reorder operation
        qDebug() << "[ANTI-FLICKER] Preserving highlight state during reorder operation";
    }
    
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
        // CRITICAL FIX: Reset flag on early return
        isReorderingWaypoints = false;
        return;
    }
    
    // Perform the reorder
    EcWidget::Waypoint waypoint = routeWaypoints.takeAt(fromIndex);
    routeWaypoints.insert(toIndex, waypoint);
    
    // Update EcWidget with new order
    ecWidget->replaceWaypointsForRoute(routeId, routeWaypoints);
    
    // CRITICAL FIX: Update tree manually without full refresh to preserve waypoint selection
    RouteTreeItem* routeItem = findRouteItem(routeId);
    if (routeItem) {
        // MAINTAIN HIGHLIGHT during rebuild by keeping EcWidget highlight active
        if (ecWidget) {
            // Keep the highlight on the map during tree rebuild to prevent flicker
            qDebug() << "[ANTI-FLICKER] Maintaining EcWidget highlight during tree rebuild";
        }
        
        // BLOCK SIGNALS during tree update to prevent interference
        routeTreeWidget->blockSignals(true);
        
        // Clear and rebuild waypoint children for this route
        qDeleteAll(routeItem->takeChildren());
        
        // Add waypoint children in new order
        for (const auto& waypoint : routeWaypoints) {
            WaypointTreeItem* waypointItem = new WaypointTreeItem(waypoint, routeItem);
        }
        
        routeItem->setExpanded(true);
        
        // Select the moved waypoint IMMEDIATELY before unblocking signals
        if (routeItem->childCount() > toIndex) {
            QTreeWidgetItem* movedWaypoint = routeItem->child(toIndex);
            routeTreeWidget->setCurrentItem(movedWaypoint);
            qDebug() << "[ANTI-FLICKER] Set selection immediately before unblocking signals";
        }
        
        // UNBLOCK SIGNALS after setting selection - this should trigger onRouteItemSelectionChanged
        routeTreeWidget->blockSignals(false);
        
        // IMMEDIATE highlight restoration to prevent flicker
        if (routeItem->childCount() > toIndex && hadHighlight && ecWidget) {
            // Directly set the highlight on EcWidget to prevent any gap
            ecWidget->highlightWaypoint(routeId, toIndex);
            qDebug() << "[ANTI-FLICKER] Immediately restored EcWidget highlight to prevent flicker";
            
            // Also trigger the selection event for UI consistency
            onRouteItemSelectionChanged();
            qDebug() << "[ANTI-FLICKER] Called onRouteItemSelectionChanged for UI consistency";
        }
    } else {
        // Fallback: If route item not found, do full refresh
        refreshRouteList();
    }
    
    emit statusMessage(QString("Waypoint moved from position %1 to %2")
        .arg(fromIndex + 1).arg(toIndex + 1));
    
    // CRITICAL FIX: Reset reordering flag
    isReorderingWaypoints = false;
    qDebug() << "[REORDER-FLAG] Set isReorderingWaypoints = false";
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
    
    // CRITICAL FIX: Update tree manually without full refresh to preserve waypoint selection
    RouteTreeItem* routeItem = findRouteItem(routeId);
    if (routeItem) {
        // Clear and rebuild waypoint children for this route
        qDeleteAll(routeItem->takeChildren());
        
        // Add waypoint children in new order
        for (const auto& waypoint : routeWaypoints) {
            WaypointTreeItem* waypointItem = new WaypointTreeItem(waypoint, routeItem);
        }
        
        // Select the new duplicate waypoint (should be the last one)
        int newWaypointIndex = routeItem->childCount() - 1;
        if (newWaypointIndex >= 0) {
            routeItem->setExpanded(true);
            
            // CRITICAL FIX: Use QTimer to delay selection to avoid race conditions
            QTimer::singleShot(0, this, [this, routeItem, newWaypointIndex, routeId]() {
                int finalIndex = routeItem->childCount() - 1;
                if (finalIndex >= 0) {
                    QTreeWidgetItem* newWaypoint = routeItem->child(finalIndex);
                    routeTreeWidget->setCurrentItem(newWaypoint);
                    qDebug() << "[DUPLICATE-WAYPOINT] Delayed selection set to duplicated waypoint - route:" << routeId << "index:" << finalIndex;
                }
            });
        }
    } else {
        // Fallback: If route item not found, do full refresh
        refreshRouteList();
    }
    
    emit statusMessage(QString("Waypoint '%1' duplicated as '%2'")
        .arg(originalLabel).arg(duplicateWaypoint.label));
}

void RoutePanel::toggleWaypointActiveStatus(int routeId, int waypointIndex)
{
    if (!ecWidget) return;
    
    // CRITICAL FIX: Prevent onWaypointAdded from triggering refreshRouteList during toggle
    isReorderingWaypoints = true;
    qDebug() << "[TOGGLE-FLAG] Set isReorderingWaypoints = true to prevent refresh interference";
    
    QList<EcWidget::Waypoint> waypoints = ecWidget->getWaypoints();
    QList<EcWidget::Waypoint> routeWaypoints;
    
    // Get waypoints for this route
    for (const auto& wp : waypoints) {
        if (wp.routeId == routeId) {
            routeWaypoints.append(wp);
        }
    }
    
    if (waypointIndex < 0 || waypointIndex >= routeWaypoints.size()) {
        // CRITICAL FIX: Reset flag on early return
        isReorderingWaypoints = false;
        return;
    }
    
    // Toggle active status
    EcWidget::Waypoint& waypoint = routeWaypoints[waypointIndex];
    waypoint.active = !waypoint.active;
    
    // Update EcWidget
    ecWidget->updateWaypointActiveStatus(routeId, waypoint.lat, waypoint.lon, waypoint.active);
    
    // CRITICAL FIX: Update tree manually without full refresh to preserve waypoint selection
    RouteTreeItem* routeItem = findRouteItem(routeId);
    if (routeItem) {
        // Get updated waypoints after active status change
        QList<EcWidget::Waypoint> updatedWaypoints = ecWidget->getWaypoints();
        QList<EcWidget::Waypoint> updatedRouteWaypoints;
        
        for (const auto& wp : updatedWaypoints) {
            if (wp.routeId == routeId) {
                updatedRouteWaypoints.append(wp);
            }
        }
        
        // ANTI-FLICKER: Maintain highlight during rebuild
        bool hadHighlight = true; // Assume we have highlight since we're toggling
        if (ecWidget) {
            qDebug() << "[ANTI-FLICKER] Maintaining EcWidget highlight during toggle tree rebuild";
        }
        
        // BLOCK SIGNALS during tree update
        routeTreeWidget->blockSignals(true);
        
        // Clear and rebuild waypoint children for this route
        qDeleteAll(routeItem->takeChildren());
        
        // Add waypoint children with updated active status
        for (const auto& waypoint : updatedRouteWaypoints) {
            WaypointTreeItem* waypointItem = new WaypointTreeItem(waypoint, routeItem);
        }
        
        routeItem->setExpanded(true);
        
        // Set selection IMMEDIATELY before unblocking signals
        if (routeItem->childCount() > waypointIndex) {
            QTreeWidgetItem* waypointItem = routeItem->child(waypointIndex);
            routeTreeWidget->setCurrentItem(waypointItem);
            qDebug() << "[ANTI-FLICKER] Set selection immediately before unblocking signals - toggle";
        }
        
        // UNBLOCK SIGNALS
        routeTreeWidget->blockSignals(false);
        
        // IMMEDIATE highlight restoration
        if (routeItem->childCount() > waypointIndex && hadHighlight && ecWidget) {
            ecWidget->highlightWaypoint(routeId, waypointIndex);
            qDebug() << "[ANTI-FLICKER] Immediately restored EcWidget highlight after toggle";
            
            // Also trigger selection event for UI consistency
            onRouteItemSelectionChanged();
            qDebug() << "[ANTI-FLICKER] Called onRouteItemSelectionChanged for consistency";
        }
    } else {
        // Fallback: If route item not found, do full refresh
        refreshRouteList();
    }
    
    QString statusText = waypoint.active ? "activated" : "deactivated";
    emit statusMessage(QString("Waypoint '%1' %2")
        .arg(waypoint.label.isEmpty() ? QString("WP-%1").arg(waypointIndex + 1) : waypoint.label)
        .arg(statusText));
    
    // CRITICAL FIX: Reset flag
    isReorderingWaypoints = false;
    qDebug() << "[TOGGLE-FLAG] Set isReorderingWaypoints = false";
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

        // Apply imported route name after waypoints created to override default
        if (!route.name.trimmed().isEmpty()) {
            ecWidget->renameRoute(route.routeId, route.name.trimmed());
        }
        // Persist now that name is applied
        ecWidget->saveRoutes();
        
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

    // First choice: Create New or From Existing
    QStringList addModes;
    addModes << "Create New" << "From Existing";
    bool ok = false;
    QString selMode = QInputDialog::getItem(this, "Add Waypoint", "Select mode:", addModes, 0, false, &ok);
    if (!ok || selMode.isEmpty()) return;

    if (selMode == "Create New") {
        // Ask user how to add the new waypoint (existing behavior)
        QMessageBox modeBox(this);
        modeBox.setWindowTitle("Add Waypoint");
        modeBox.setText("How do you want to add the waypoint?");
        QPushButton* mouseBtn = modeBox.addButton("By Mouse", QMessageBox::AcceptRole);
        QPushButton* formBtn = modeBox.addButton("By Form", QMessageBox::ActionRole);
        QPushButton* cancelBtn = modeBox.addButton(QMessageBox::Cancel);
        modeBox.setDefaultButton(mouseBtn);
        modeBox.exec();

        if (modeBox.clickedButton() == cancelBtn) return;

        if (modeBox.clickedButton() == mouseBtn) {
            // Start append-by-mouse mode on the selected route
            if (ecWidget) {
                ecWidget->startAppendWaypointMode(selectedRouteId);
                emit statusMessage(QString("Add waypoint by mouse on Route %1").arg(selectedRouteId));
            }
            return;
        }

        // Otherwise, open the form dialog
        showWaypointEditDialog(selectedRouteId);
        return;
    }

    // From Existing: show a list of all existing waypoints from all routes
    if (!ecWidget) return;

    QList<EcWidget::Waypoint> allWps = ecWidget->getWaypoints();
    QVector<EcWidget::Waypoint> routeWps;
    routeWps.reserve(allWps.size());
    for (const auto& wp : allWps) {
        if (wp.routeId > 0) {
            routeWps.append(wp);
        }
    }

    if (routeWps.isEmpty()) {
        QMessageBox::information(this, "No Waypoints", "No existing waypoints found across routes.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Select Existing Waypoint");
    QVBoxLayout* vlay = new QVBoxLayout(&dlg);
    QLabel* info = new QLabel("Choose a waypoint from any route to add to the selected route.", &dlg);
    info->setWordWrap(true);
    vlay->addWidget(info);

    QTreeWidget* list = new QTreeWidget(&dlg);
    list->setColumnCount(4);
    QStringList headers; headers << "Label" << "Route" << "Latitude" << "Longitude";
    list->setHeaderLabels(headers);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setUniformRowHeights(true);
    list->setRootIsDecorated(false);
    list->setAlternatingRowColors(true);

    // Fill items
    for (int i = 0; i < routeWps.size(); ++i) {
        const auto& wp = routeWps[i];
        auto* it = new QTreeWidgetItem(list);
        it->setText(0, wp.label);
        // Resolve route name
        QString routeName = QString("Route %1").arg(wp.routeId);
        EcWidget::Route r = ecWidget->getRouteById(wp.routeId);
        if (!r.name.trimmed().isEmpty()) routeName = r.name;
        it->setText(1, routeName);
        // Show coordinates in Deg-Min format
        it->setText(2, formatDegMin(wp.lat, true));
        it->setText(3, formatDegMin(wp.lon, false));
        it->setData(0, Qt::UserRole, i); // store index
    }
    list->resizeColumnToContents(0);
    list->resizeColumnToContents(1);
    vlay->addWidget(list);

    QHBoxLayout* btns = new QHBoxLayout();
    btns->addStretch();
    QPushButton* okBtn = new QPushButton("Add to Route", &dlg);
    QPushButton* cancelBtn = new QPushButton("Cancel", &dlg);
    btns->addWidget(okBtn);
    btns->addWidget(cancelBtn);
    vlay->addLayout(btns);

    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    QTreeWidgetItem* sel = list->currentItem();
    if (!sel) {
        QMessageBox::information(this, "No Selection", "Please select a waypoint.");
        return;
    }
    bool okIdx = false;
    int idx = sel->data(0, Qt::UserRole).toInt(&okIdx);
    if (!okIdx || idx < 0 || idx >= routeWps.size()) return;

    const auto& src = routeWps[idx];
    // Create a new waypoint in the selected route using the chosen waypoint's data
    ecWidget->createWaypointFromForm(src.lat, src.lon, src.label, src.remark, selectedRouteId, src.turningRadius, src.active);

    // Refresh and notify
    refreshRouteList();
    emit statusMessage(QString("Added existing waypoint '%1' to Route %2").arg(src.label).arg(selectedRouteId));
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
            // Remove by route-local index to avoid mismatch caused by duplicate labels/coords
            int indexInRoute = getWaypointIndex(waypointItem);
            if (indexInRoute >= 0) {
                ecWidget->deleteRouteWaypointAt(waypoint.routeId, indexInRoute);
            }
            
            // CRITICAL FIX: Clear highlight when waypoint is deleted
            if (ecWidget) {
                ecWidget->clearWaypointHighlight();
                qDebug() << "[DELETE-WAYPOINT] Cleared highlight after waypoint deletion";
            }
            
            refreshRouteList();
            // Ensure info panel shows updated counts/distances if this route is still selected
            if (selectedRouteId == waypoint.routeId) {
                updateRouteInfo(selectedRouteId);
            }
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

void RoutePanel::mousePressEvent(QMouseEvent* event)
{
    // Check if click is on empty area (not on tree widget items)
    QPoint clickPos = event->pos();
    QPoint treeClickPos = routeTreeWidget->mapFromParent(clickPos);
    
    // If click is within tree widget bounds but not on any item
    if (routeTreeWidget->geometry().contains(clickPos)) {
        QTreeWidgetItem* clickedItem = routeTreeWidget->itemAt(treeClickPos);
        if (!clickedItem) {
            // Clear selection when clicking on empty tree area
            routeTreeWidget->clearSelection();
            // This will trigger onRouteItemSelectionChanged() which clears highlight
        }
    } else {
        // Click is outside tree widget - clear selection
        routeTreeWidget->clearSelection();
    }
    
    // Call parent implementation
    QWidget::mousePressEvent(event);
}

void RoutePanel::setAttachDetachButton(bool connection){
    addToShipButton->setVisible(connection);
    detachFromShipButton->setVisible(connection);
    routeDeviationCheckBox->setVisible(connection);  // Show when MOOSDB connected
}
