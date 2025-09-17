#include "routeformdialog.h"
#include "ecwidget.h"
#include "appconfig.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QGroupBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QGridLayout>
#include <QDoubleSpinBox>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QTextStream>
#include <QStandardPaths>
#include <QtMath>

RouteFormDialog::RouteFormDialog(QWidget *parent, EcWidget *ecWidget)
    : QDialog(parent)
    , ecWidget(ecWidget)
    , tabWidget(nullptr)
    , routeNameEdit(nullptr)
    , routeDescriptionEdit(nullptr)
    , routeIdSpinBox(nullptr)
    , routeStatsLabel(nullptr)
    , waypointTable(nullptr)
    , addWaypointBtn(nullptr)
    , removeWaypointBtn(nullptr)
    , editWaypointBtn(nullptr)
    , moveUpBtn(nullptr)
    , moveDownBtn(nullptr)
    , importBtn(nullptr)
    , exportBtn(nullptr)
    , autoLabelCheckBox(nullptr)
    , defaultTurningRadiusSpinBox(nullptr)
    , routeTypeComboBox(nullptr)
    , routeSpeedEdit(nullptr)
    , reverseRouteCheckBox(nullptr)
    , okButton(nullptr)
    , cancelButton(nullptr)
    , previewButton(nullptr)
    , statusLabel(nullptr)
{
    setupUI();
    connectSignals();
    updateButtonStates();
    
    // Set default values
    int nextId = generateNextRouteId();
    routeIdSpinBox->setValue(nextId);
    routeNameEdit->setText(QString("Route %1").arg(nextId, 3, 10, QChar('0'))); // Route 001, Route 002, etc.
    autoLabelCheckBox->setChecked(true);
    defaultTurningRadiusSpinBox->setValue(0.5);
    routeTypeComboBox->setCurrentText("Navigation");
    routeSpeedEdit->setText("10.0");
    
    // Initialize validators
    latValidator = new QDoubleValidator(-90.0, 90.0, 6, this);
    lonValidator = new QDoubleValidator(-180.0, 180.0, 6, this);
    radiusValidator = new QDoubleValidator(0.1, 10.0, 2, this);
}

void RouteFormDialog::setupUI()
{
    setWindowTitle("Create Route by Form");
    setModal(true);
    resize(800, 600);
    
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Tab widget
    tabWidget = new QTabWidget(this);
    mainLayout->addWidget(tabWidget);
    
    // Setup tabs
    setupRouteInfoTab();
    setupWaypointsTab();
    setupAdvancedTab(); // Still needed for component initialization but tab is hidden
    
    // Status label
    statusLabel = new QLabel(this);
    if (AppConfig::isLight()){
        statusLabel->setStyleSheet("QLabel { color: blue; font-style: italic; }");
    }
    else {
        statusLabel->setStyleSheet("QLabel { color: white; font-style: italic; }");
    }
    mainLayout->addWidget(statusLabel);
    
    // Button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    previewButton = new QPushButton("Preview Route", this);
    cancelButton = new QPushButton("Cancel", this);
    okButton = new QPushButton("Create Route", this);
    
    okButton->setDefault(true);
    okButton->setStyleSheet("QPushButton { font-weight: bold; }");
    
    buttonLayout->addWidget(previewButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    
    mainLayout->addLayout(buttonLayout);
}

void RouteFormDialog::setupRouteInfoTab()
{
    QWidget *routeInfoWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(routeInfoWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // Basic Route Information Group
    QGroupBox *basicInfoGroup = new QGroupBox("Basic Information");
    QFormLayout *basicFormLayout = new QFormLayout(basicInfoGroup);
    basicFormLayout->setVerticalSpacing(8);
    
    // Route ID
    routeIdSpinBox = new QSpinBox();
    routeIdSpinBox->setRange(1, 9999);
    routeIdSpinBox->setEnabled(false); // Auto-generated, read-only
    routeIdSpinBox->setStyleSheet("QSpinBox:disabled { background-color: #f0f0f0; color: #666; }");
    basicFormLayout->addRow("Route ID:", routeIdSpinBox);
    
    // Route Name
    routeNameEdit = new QLineEdit();
    routeNameEdit->setMaxLength(50);
    routeNameEdit->setStyleSheet("QLineEdit { padding: 5px; }");
    basicFormLayout->addRow("Route Name:", routeNameEdit);
    
    mainLayout->addWidget(basicInfoGroup);
    
    // Description Group
    QGroupBox *descGroup = new QGroupBox("Description");
    QVBoxLayout *descLayout = new QVBoxLayout(descGroup);
    
    routeDescriptionEdit = new QTextEdit();
    routeDescriptionEdit->setMaximumHeight(80);
    routeDescriptionEdit->setPlaceholderText("Enter route description or notes...");
    descLayout->addWidget(routeDescriptionEdit);
    
    mainLayout->addWidget(descGroup);
    
    // Statistics Group
    QGroupBox *statsGroup = new QGroupBox("Route Statistics");
    QVBoxLayout *statsLayout = new QVBoxLayout(statsGroup);
    
    routeStatsLabel = new QLabel("Waypoints: 0 | Distance: 0.00 NM");
    if (AppConfig::isLight()){
        routeStatsLabel->setStyleSheet("QLabel { color: #006600; font-weight: bold; font-size: 11pt; padding: 5px; }");
    }
    else {
        routeStatsLabel->setStyleSheet("QLabel { color: #FFFFFF; font-weight: bold; font-size: 11pt; padding: 5px; }");
    }
    routeStatsLabel->setAlignment(Qt::AlignCenter);
    statsLayout->addWidget(routeStatsLabel);
    
    mainLayout->addWidget(statsGroup);
    
    // HIDDEN COMPONENTS - Create but don't display (moved from Advanced tab)
    autoLabelCheckBox = new QCheckBox("Auto-generate waypoint labels");
    autoLabelCheckBox->setVisible(false); // Hidden from UI but available for functionality
    
    defaultTurningRadiusSpinBox = new QDoubleSpinBox();
    defaultTurningRadiusSpinBox->setRange(0.1, 10.0);
    defaultTurningRadiusSpinBox->setSingleStep(0.1);
    defaultTurningRadiusSpinBox->setDecimals(1);
    defaultTurningRadiusSpinBox->setSuffix(" NM");
    defaultTurningRadiusSpinBox->setVisible(false); // Hidden from UI but available for functionality
    
    // Add stretch to push content to top
    mainLayout->addStretch();
    
    tabWidget->addTab(routeInfoWidget, "Route Info");
}

void RouteFormDialog::setupWaypointsTab()
{
    QWidget *waypointsWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(waypointsWidget);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);
    
    // Table header label
    QLabel *tableHeaderLabel = new QLabel("Waypoints List");
    tableHeaderLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 12pt; color: #333; }");
    layout->addWidget(tableHeaderLabel);
    
    // Waypoint table
    waypointTable = new QTableWidget(0, 7);
    QStringList headers;
    headers << "#" << "Label" << "Latitude" << "Longitude" << "Turn Radius" << "Remark" << "Active";
    waypointTable->setHorizontalHeaderLabels(headers);
    
    // HIDE Turn Radius column (index 4)
    waypointTable->setColumnHidden(4, true);
    
    // Add tooltips for headers
    waypointTable->horizontalHeaderItem(0)->setToolTip("Sequential waypoint number");
    waypointTable->horizontalHeaderItem(1)->setToolTip("Waypoint identifier/name");
    waypointTable->horizontalHeaderItem(2)->setToolTip("Latitude coordinate in decimal degrees");
    waypointTable->horizontalHeaderItem(3)->setToolTip("Longitude coordinate in decimal degrees");
    waypointTable->horizontalHeaderItem(4)->setToolTip("Turning radius in nautical miles");
    waypointTable->horizontalHeaderItem(5)->setToolTip("Additional notes or remarks");
    waypointTable->horizontalHeaderItem(6)->setToolTip("Enable/disable this waypoint");
    
    // Improved table styling
    waypointTable->setStyleSheet(
        "QTableWidget {"
        "    gridline-color: #d0d0d0;"
        "    border: 1px solid #c0c0c0;"
        "    selection-background-color: #3daee9;"
        "    selection-color: #ffffff;"
        "    alternate-background-color: #f5f5f5;"
        "}"
        "QTableWidget::item {"
        "    padding: 8px;"
        "    border: none;"
        "    color: #333;"
        "}"
        "QTableWidget::item:selected {"
        "    background-color: #3daee9;"
        "    color: #ffffff;"
        "}"
        "QTableWidget::item:hover {"
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
    
    // Set column widths with better proportions
    waypointTable->setColumnWidth(0, 40);  // #
    waypointTable->setColumnWidth(1, 90);  // Label  
    waypointTable->setColumnWidth(2, 110); // Latitude
    waypointTable->setColumnWidth(3, 110); // Longitude
    waypointTable->setColumnWidth(4, 100); // Turn Radius
    waypointTable->setColumnWidth(5, 200); // Remark
    waypointTable->setColumnWidth(6, 70);  // Active
    
    // Table behavior settings
    waypointTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    waypointTable->setSelectionMode(QAbstractItemView::SingleSelection);
    waypointTable->setAlternatingRowColors(true);
    waypointTable->setSortingEnabled(false);
    waypointTable->verticalHeader()->setVisible(false);
    waypointTable->horizontalHeader()->setStretchLastSection(false);
    waypointTable->setShowGrid(true);
    
    // Set minimum height for table
    waypointTable->setMinimumHeight(200);
    
    layout->addWidget(waypointTable);
    
    // Button groups with better organization
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    
    // Waypoint management buttons
    QGroupBox *waypointGroup = new QGroupBox("Waypoint Management");
    QHBoxLayout *waypointButtonLayout = new QHBoxLayout(waypointGroup);
    
    addWaypointBtn = new QPushButton("+ Add");
    editWaypointBtn = new QPushButton("✎ Edit");
    removeWaypointBtn = new QPushButton("✕ Remove");
    
    // Use standard button styling like Cancel button
    addWaypointBtn->setStyleSheet("QPushButton { padding: 6px 12px; }");
    editWaypointBtn->setStyleSheet("QPushButton { padding: 6px 12px; }");
    removeWaypointBtn->setStyleSheet("QPushButton { padding: 6px 12px; }");
    
    waypointButtonLayout->addWidget(addWaypointBtn);
    waypointButtonLayout->addWidget(editWaypointBtn);
    waypointButtonLayout->addWidget(removeWaypointBtn);
    
    // Order management buttons
    QGroupBox *orderGroup = new QGroupBox("Order");
    QHBoxLayout *orderButtonLayout = new QHBoxLayout(orderGroup);
    
    moveUpBtn = new QPushButton("↑ Up");
    moveDownBtn = new QPushButton("↓ Down");
    
    moveUpBtn->setStyleSheet("QPushButton { padding: 6px 12px; }");
    moveDownBtn->setStyleSheet("QPushButton { padding: 6px 12px; }");
    
    orderButtonLayout->addWidget(moveUpBtn);
    orderButtonLayout->addWidget(moveDownBtn);
    
    // Import/Export buttons
    QGroupBox *importExportGroup = new QGroupBox("Import/Export");
    QHBoxLayout *importExportLayout = new QHBoxLayout(importExportGroup);
    
    importBtn = new QPushButton("📁 Import CSV");
    exportBtn = new QPushButton("💾 Export CSV");
    
    importBtn->setStyleSheet("QPushButton { padding: 6px 12px; }");
    exportBtn->setStyleSheet("QPushButton { padding: 6px 12px; }");
    
    importExportLayout->addWidget(importBtn);
    importExportLayout->addWidget(exportBtn);
    
    // Add groups to main button layout
    buttonLayout->addWidget(waypointGroup);
    buttonLayout->addWidget(orderGroup);
    buttonLayout->addStretch();
    buttonLayout->addWidget(importExportGroup);
    
    layout->addLayout(buttonLayout);
    
    tabWidget->addTab(waypointsWidget, "Waypoints");
}

void RouteFormDialog::setupAdvancedTab()
{
    QWidget *advancedWidget = new QWidget();
    QFormLayout *formLayout = new QFormLayout(advancedWidget);
    
    // DISABLED: Advanced tab functionality removed
    // Components have been moved to setupRouteInfoTab() (hidden) or disabled
    // autoLabelCheckBox and defaultTurningRadiusSpinBox now created in setupRouteInfoTab()
    
    // Create remaining components but don't add them to UI (to prevent crashes)
    routeTypeComboBox = new QComboBox();
    routeTypeComboBox->addItems({"Navigation", "Survey", "Emergency", "Training", "Test"});
    routeTypeComboBox->setVisible(false); // Hidden
    
    routeSpeedEdit = new QLineEdit();
    routeSpeedEdit->setValidator(new QDoubleValidator(0.1, 50.0, 1, this));
    routeSpeedEdit->setVisible(false); // Hidden
    
    reverseRouteCheckBox = new QCheckBox("Create reverse route automatically");
    reverseRouteCheckBox->setVisible(false); // Hidden
    
    // Don't add the tab to UI since Advanced tab is disabled
}

void RouteFormDialog::connectSignals()
{
    // Waypoint table signals
    connect(addWaypointBtn, &QPushButton::clicked, this, &RouteFormDialog::onAddWaypoint);
    connect(editWaypointBtn, &QPushButton::clicked, this, &RouteFormDialog::onEditWaypoint);
    connect(removeWaypointBtn, &QPushButton::clicked, this, &RouteFormDialog::onRemoveWaypoint);
    connect(moveUpBtn, &QPushButton::clicked, this, &RouteFormDialog::onMoveWaypointUp);
    connect(moveDownBtn, &QPushButton::clicked, this, &RouteFormDialog::onMoveWaypointDown);
    connect(importBtn, &QPushButton::clicked, this, &RouteFormDialog::onImportFromCSV);
    connect(exportBtn, &QPushButton::clicked, this, &RouteFormDialog::onExportToCSV);
    
    connect(waypointTable, &QTableWidget::itemSelectionChanged, this, &RouteFormDialog::onWaypointSelectionChanged);
    connect(waypointTable, &QTableWidget::itemDoubleClicked, this, &RouteFormDialog::onEditWaypoint);
    
    // Dialog buttons
    connect(okButton, &QPushButton::clicked, this, &RouteFormDialog::validateAndAccept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(previewButton, &QPushButton::clicked, this, &RouteFormDialog::onPreviewRoute);
    
    // Route info changes
    connect(routeNameEdit, &QLineEdit::textChanged, this, [this]() {
        statusLabel->setText("Route information updated");
    });
}

void RouteFormDialog::onAddWaypoint()
{
    showWaypointInputDialog();
}

void RouteFormDialog::onEditWaypoint()
{
    int currentRow = waypointTable->currentRow();
    if (currentRow >= 0) {
        showWaypointInputDialog(currentRow);
    }
}

void RouteFormDialog::onRemoveWaypoint()
{
    int currentRow = waypointTable->currentRow();
    if (currentRow >= 0) {
        QString removedLabel = waypoints[currentRow].label;
        waypoints.removeAt(currentRow);
        updateWaypointList();
        updateButtonStates();
        statusLabel->setText(QString("✗ Waypoint %1 (%2) removed - %3 waypoints remaining").arg(currentRow + 1).arg(removedLabel).arg(waypoints.size()));
    }
}

void RouteFormDialog::onMoveWaypointUp()
{
    int currentRow = waypointTable->currentRow();
    if (currentRow > 0) {
        QString waypointLabel = waypoints[currentRow].label;
        waypoints.swapItemsAt(currentRow - 1, currentRow);
        updateWaypointList();
        waypointTable->selectRow(currentRow - 1);
        statusLabel->setText(QString("↑ Waypoint %1 (%2) moved up to position %3").arg(currentRow + 1).arg(waypointLabel).arg(currentRow));
    }
}

void RouteFormDialog::onMoveWaypointDown()
{
    int currentRow = waypointTable->currentRow();
    if (currentRow >= 0 && currentRow < waypoints.size() - 1) {
        QString waypointLabel = waypoints[currentRow].label;
        waypoints.swapItemsAt(currentRow, currentRow + 1);
        updateWaypointList();
        waypointTable->selectRow(currentRow + 1);
        statusLabel->setText(QString("↓ Waypoint %1 (%2) moved down to position %3").arg(currentRow + 1).arg(waypointLabel).arg(currentRow + 2));
    }
}

void RouteFormDialog::showWaypointInputDialog(int editIndex)
{
    QDialog dialog(this);
    dialog.setWindowTitle(editIndex >= 0 ? "Edit Waypoint" : "Add Waypoint");
    dialog.setModal(true);

    QFormLayout *layout = new QFormLayout(&dialog);

    QLineEdit *labelEdit = new QLineEdit();
    QLineEdit *latEdit = new QLineEdit();
    QLineEdit *lonEdit = new QLineEdit();
    QDoubleSpinBox *radiusSpin = new QDoubleSpinBox();
    QLineEdit *remarkEdit = new QLineEdit();
    QCheckBox *activeCheck = new QCheckBox();

    QLabel *latLabel = new QLabel("Latitude:");
    QLabel *lonLabel = new QLabel("Longitude:");

    enum UnitOption { UnitDecimal = 0, UnitDegMin = 1, UnitDegMinSec = 2, UnitMeters = 3 };
    QGroupBox *unitGroup = new QGroupBox("Coordinate Units");
    QGridLayout *unitLayout = new QGridLayout(unitGroup);
    unitLayout->setHorizontalSpacing(12);
    unitLayout->setVerticalSpacing(4);
    unitLayout->setContentsMargins(4, 6, 4, 6);

    QButtonGroup *unitButtons = new QButtonGroup(&dialog);
    QRadioButton *decDegBtn = new QRadioButton("Decimal Degrees");
    QRadioButton *degMinBtn = new QRadioButton("Deg-Min");
    QRadioButton *degMinSecBtn = new QRadioButton("Deg-Min-Sec");
    QRadioButton *metersBtn = new QRadioButton("Meters (N/E)");
    unitButtons->addButton(decDegBtn, UnitDecimal);
    unitButtons->addButton(degMinBtn, UnitDegMin);
    unitButtons->addButton(degMinSecBtn, UnitDegMinSec);
    unitButtons->addButton(metersBtn, UnitMeters);
    decDegBtn->setChecked(true);
    unitLayout->addWidget(decDegBtn, 0, 0);
    unitLayout->addWidget(degMinBtn, 0, 1);
    unitLayout->addWidget(degMinSecBtn, 1, 0);
    unitLayout->addWidget(metersBtn, 1, 1);

    latEdit->setValidator(latValidator);
    lonEdit->setValidator(lonValidator);
    latEdit->setPlaceholderText("e.g., -7.256940");
    lonEdit->setPlaceholderText("e.g., 112.751940");

    radiusSpin->setRange(0.1, 10.0);
    radiusSpin->setSingleStep(0.1);
    radiusSpin->setDecimals(1);
    radiusSpin->setSuffix(" NM");
    radiusSpin->setValue(0.5);
    radiusSpin->setVisible(false);

    activeCheck->setChecked(true);

    if (editIndex >= 0 && editIndex < waypoints.size()) {
        const RouteWaypointData &wp = waypoints[editIndex];
        labelEdit->setText(wp.label);
        latEdit->setText(QString::number(wp.lat, 'f', 6));
        lonEdit->setText(QString::number(wp.lon, 'f', 6));
        radiusSpin->setValue(wp.turningRadius);
        remarkEdit->setText(wp.remark);
        activeCheck->setChecked(wp.active);
    } else if (autoLabelCheckBox->isChecked()) {
        labelEdit->setText(QString("WP%1").arg(waypoints.size() + 1));
    }

    auto updateUnitUI = [&]() {
        switch (unitButtons->checkedId()) {
        case UnitDecimal:
            latLabel->setText("Latitude:");
            lonLabel->setText("Longitude:");
            latEdit->setValidator(latValidator);
            lonEdit->setValidator(lonValidator);
            latEdit->setPlaceholderText("e.g., -7.256940");
            lonEdit->setPlaceholderText("e.g., 112.751940");
            break;
        case UnitDegMin:
            latLabel->setText("Latitude (Deg-Min):");
            lonLabel->setText("Longitude (Deg-Min):");
            latEdit->setValidator(nullptr);
            lonEdit->setValidator(nullptr);
            latEdit->setPlaceholderText("e.g., 7 15.300 S or S 7 15.300");
            lonEdit->setPlaceholderText("e.g., 112 45.120 E or E 112 45.120");
            break;
        case UnitDegMinSec:
            latLabel->setText("Latitude (Deg-Min-Sec):");
            lonLabel->setText("Longitude (Deg-Min-Sec):");
            latEdit->setValidator(nullptr);
            lonEdit->setValidator(nullptr);
            latEdit->setPlaceholderText("e.g., 7 15 30.5 S or S 7 15 30.5");
            lonEdit->setPlaceholderText("e.g., 112 45 18.2 E or E 112 45 18.2");
            break;
        case UnitMeters:
        default:
            latLabel->setText("North (m):");
            lonLabel->setText("East (m):");
            latEdit->setValidator(new QDoubleValidator(-1e7, 1e7, 3, &dialog));
            lonEdit->setValidator(new QDoubleValidator(-1e7, 1e7, 3, &dialog));
            latEdit->setPlaceholderText("e.g., 0 (north offset)");
            lonEdit->setPlaceholderText("e.g., 0 (east offset)");
            break;
        }
    };

    auto connectUnit = [&](QRadioButton *btn) {
        QObject::connect(btn, &QRadioButton::toggled, &dialog, [&](bool checked) {
            if (checked) updateUnitUI();
        });
    };
    connectUnit(decDegBtn);
    connectUnit(degMinBtn);
    connectUnit(degMinSecBtn);
    connectUnit(metersBtn);
    updateUnitUI();

    layout->addRow("Label:", labelEdit);
    layout->addRow(unitGroup);
    layout->addRow(latLabel, latEdit);
    layout->addRow(lonLabel, lonEdit);
    layout->addRow("Remark:", remarkEdit);
    layout->addRow("Active:", activeCheck);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton("OK");
    QPushButton *cancelBtn = new QPushButton("Cancel");
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    layout->addRow(buttonLayout);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    auto parseCoordinate = [&](const QString& text, bool isLat, bool requireSeconds, double& out) -> bool {
        QString raw = text.trimmed().toUpper();
        if (raw.isEmpty()) return false;
        int sign = 1;
        if (raw.contains("S") || raw.contains("W")) sign = -1;
        if (raw.startsWith('-')) {
            sign = -1;
            raw.remove(0, 1);
        } else if (raw.startsWith('+')) {
            raw.remove(0, 1);
        }
        QString t = raw;
        t.replace('°', " ");
        t.replace("'", " ");
        t.replace('"', ' ');
        t.replace(",", " ");
        t.replace(":", " ");
        t.replace("-", " ");
        t = t.simplified();
        t.remove("N");
        t.remove("S");
        t.remove("E");
        t.remove("W");
        t = t.simplified();
        if (t.isEmpty()) return false;
        QStringList parts = t.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) return false;
        bool okDeg=false;
        int deg = parts[0].toInt(&okDeg);
        double minutes = 0.0;
        bool okMin = true;
        if (parts.size() >= 2) {
            minutes = parts[1].toDouble(&okMin);
        } else if (requireSeconds) {
            return false;
        }
        double seconds = 0.0;
        bool okSec = true;
        if (parts.size() >= 3) {
            seconds = parts[2].toDouble(&okSec);
        } else if (requireSeconds) {
            return false;
        }
        if (!okDeg || !okMin || !okSec) return false;
        if (minutes < 0.0 || minutes >= 60.0) return false;
        if (seconds < 0.0 || seconds >= 60.0) return false;
        if (isLat && (deg < 0 || deg > 90)) return false;
        if (!isLat && (deg < 0 || deg > 180)) return false;
        double decimal = deg + minutes / 60.0 + seconds / 3600.0;
        out = sign * decimal;
        return true;
    };

    auto metersToLatLon = [&](double north, double east, double& outLat, double& outLon) {
        EcCoordinate oLat=0.0, oLon=0.0;
        if (ecWidget) ecWidget->GetCenter(oLat, oLon);
        double metersPerDegLat = 111320.0;
        double metersPerDegLon = metersPerDegLat * qCos(qDegreesToRadians(static_cast<double>(oLat)));
        outLat = static_cast<double>(oLat) + (north / metersPerDegLat);
        if (metersPerDegLon == 0.0) metersPerDegLon = 1.0;
        outLon = static_cast<double>(oLon) + (east / metersPerDegLon);
    };

    if (dialog.exec() == QDialog::Accepted) {
        RouteWaypointData wp;
        wp.label = labelEdit->text().trimmed();
        wp.turningRadius = radiusSpin->value();
        wp.remark = remarkEdit->text().trimmed();
        wp.active = activeCheck->isChecked();

        int unit = unitButtons->checkedId();
        bool okCoords = false;

        if (unit == UnitDecimal) {
            bool okLat=false, okLon=false;
            double lat = latEdit->text().toDouble(&okLat);
            double lon = lonEdit->text().toDouble(&okLon);
            okCoords = okLat && okLon;
            wp.lat = lat;
            wp.lon = lon;
        } else if (unit == UnitDegMin) {
            double lat=0.0, lon=0.0;
            bool okLat = parseCoordinate(latEdit->text(), true, false, lat);
            bool okLon = parseCoordinate(lonEdit->text(), false, false, lon);
            okCoords = okLat && okLon;
            wp.lat = lat;
            wp.lon = lon;
        } else if (unit == UnitDegMinSec) {
            double lat=0.0, lon=0.0;
            bool okLat = parseCoordinate(latEdit->text(), true, true, lat);
            bool okLon = parseCoordinate(lonEdit->text(), false, true, lon);
            okCoords = okLat && okLon;
            wp.lat = lat;
            wp.lon = lon;
        } else { // Meters
            bool okNorth=false, okEast=false;
            double north = latEdit->text().toDouble(&okNorth);
            double east = lonEdit->text().toDouble(&okEast);
            if (okNorth && okEast) {
                double lat=0.0, lon=0.0;
                metersToLatLon(north, east, lat, lon);
                wp.lat = lat;
                wp.lon = lon;
                okCoords = true;
            }
        }

        if (!okCoords || wp.lat < -90.0 || wp.lat > 90.0 || wp.lon < -180.0 || wp.lon > 180.0) {
            QMessageBox::warning(this, "Invalid Coordinates",
                                 "Please enter valid coordinates.\n"
                                 "- Decimal Degrees: -90..90 (lat), -180..180 (lon)\n"
                                 "- Deg-Min: D M.MMM H (e.g., 7 15.300 S)\n"
                                 "- Deg-Min-Sec: D M S.SSS H (e.g., 7 15 18.5 S)\n"
                                 "- Meters: North/East offsets relative to map center");
            return;
        }

        if (editIndex >= 0) {
            qDebug() << "[WAYPOINT-EDIT] Waypoint" << editIndex + 1 << "coordinates changed from"
                     << waypoints[editIndex].lat << "," << waypoints[editIndex].lon
                     << "to" << wp.lat << "," << wp.lon;
            waypoints[editIndex] = wp;
            statusLabel->setText(QString("✓ Waypoint %1 (%2) updated successfully").arg(editIndex + 1).arg(wp.label));
        } else {
            qDebug() << "[WAYPOINT-ADD] New waypoint added at" << wp.lat << "," << wp.lon;
            waypoints.append(wp);
            statusLabel->setText(QString("✓ Waypoint %1 (%2) added successfully - Total: %3 waypoints").arg(waypoints.size()).arg(wp.label).arg(waypoints.size()));
        }

        updateWaypointList();
        updateButtonStates();
    }
}


void RouteFormDialog::updateWaypointList()
{
    waypointTable->setRowCount(waypoints.size());
    
    for (int i = 0; i < waypoints.size(); ++i) {
        const RouteWaypointData &wp = waypoints[i];
        
        // Column 0: Sequential number
        QTableWidgetItem *numberItem = new QTableWidgetItem(QString::number(i + 1));
        numberItem->setTextAlignment(Qt::AlignCenter);
        numberItem->setFlags(numberItem->flags() & ~Qt::ItemIsEditable); // Read-only
        numberItem->setBackground(QBrush(QColor(245, 245, 245))); // Light gray background
        waypointTable->setItem(i, 0, numberItem);
        
        // Column 1: Label
        QTableWidgetItem *labelItem = new QTableWidgetItem(wp.label);
        labelItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        labelItem->setFont(QFont("Arial", 9, QFont::Bold));
        waypointTable->setItem(i, 1, labelItem);
        
        // Column 2: Latitude with proper formatting
        QTableWidgetItem *latItem = new QTableWidgetItem(QString::number(wp.lat, 'f', 6) + "°");
        latItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        latItem->setFont(QFont("Courier", 9)); // Monospace for coordinates
        waypointTable->setItem(i, 2, latItem);
        
        // Column 3: Longitude with proper formatting
        QTableWidgetItem *lonItem = new QTableWidgetItem(QString::number(wp.lon, 'f', 6) + "°");
        lonItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lonItem->setFont(QFont("Courier", 9)); // Monospace for coordinates
        waypointTable->setItem(i, 3, lonItem);
        
        // Column 4: Turn Radius with unit
        QTableWidgetItem *radiusItem = new QTableWidgetItem(QString::number(wp.turningRadius, 'f', 1) + " NM");
        radiusItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        waypointTable->setItem(i, 4, radiusItem);
        
        // Column 5: Remark
        QTableWidgetItem *remarkItem = new QTableWidgetItem(wp.remark.isEmpty() ? "-" : wp.remark);
        remarkItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        if (wp.remark.isEmpty()) {
            remarkItem->setForeground(QBrush(QColor(150, 150, 150))); // Gray for empty
        }
        waypointTable->setItem(i, 5, remarkItem);
        
        // Column 6: Active status with centered checkbox
        QWidget *checkBoxWidget = new QWidget();
        QCheckBox *activeCheckBox = new QCheckBox();
        QHBoxLayout *checkBoxLayout = new QHBoxLayout(checkBoxWidget);
        checkBoxLayout->addWidget(activeCheckBox);
        checkBoxLayout->setAlignment(Qt::AlignCenter);
        checkBoxLayout->setContentsMargins(0, 0, 0, 0);
        activeCheckBox->setChecked(wp.active);
        waypointTable->setCellWidget(i, 6, checkBoxWidget);
        
        // Set row height for better appearance
        waypointTable->setRowHeight(i, 35);
    }
    
    // Update statistics
    double totalDistance = calculateRouteDistance();
    qDebug() << "[ROUTE-FORM] Route distance calculated:" << totalDistance << "NM, formatted:" << formatDistance(totalDistance);
    routeStatsLabel->setText(QString("Waypoints: %1 | Distance: %2")
        .arg(waypoints.size())
        .arg(formatDistance(totalDistance)));
}

void RouteFormDialog::updateButtonStates()
{
    bool hasSelection = waypointTable->currentRow() >= 0;
    bool hasWaypoints = waypoints.size() > 0;
    
    editWaypointBtn->setEnabled(hasSelection);
    removeWaypointBtn->setEnabled(hasSelection);
    moveUpBtn->setEnabled(hasSelection && waypointTable->currentRow() > 0);
    moveDownBtn->setEnabled(hasSelection && waypointTable->currentRow() < waypoints.size() - 1);
    exportBtn->setEnabled(hasWaypoints);
    previewButton->setEnabled(hasWaypoints);
    okButton->setEnabled(hasWaypoints && !routeNameEdit->text().trimmed().isEmpty());
}

void RouteFormDialog::onWaypointSelectionChanged()
{
    updateButtonStates();
}

void RouteFormDialog::onImportFromCSV()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "Import Waypoints from CSV", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "CSV Files (*.csv)");
    
    if (!fileName.isEmpty()) {
        // Implementation for CSV import would go here
        statusLabel->setText("CSV import functionality not yet implemented");
    }
}

void RouteFormDialog::onExportToCSV()
{
    if (waypoints.isEmpty()) {
        QMessageBox::information(this, "No Data", "No waypoints to export.");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Waypoints to CSV",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + 
        "/" + routeNameEdit->text() + "_waypoints.csv",
        "CSV Files (*.csv)");
    
    if (!fileName.isEmpty()) {
        // Implementation for CSV export would go here
        statusLabel->setText("CSV export functionality not yet implemented");
    }
}

void RouteFormDialog::onValidateRoute()
{
    if (validateRouteData()) {
        statusLabel->setText("Route validation passed");
    }
}

void RouteFormDialog::onPreviewRoute()
{
    if (waypoints.isEmpty()) {
        QMessageBox::information(this, "No Waypoints", "Please add waypoints to preview the route.");
        return;
    }
    
    statusLabel->setText("Route preview functionality not yet implemented");
}

void RouteFormDialog::validateAndAccept()
{
    if (validateRouteData()) {
        accept();
    }
}

bool RouteFormDialog::validateRouteData()
{
    if (routeNameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please enter a route name.");
        tabWidget->setCurrentIndex(0);
        routeNameEdit->setFocus();
        return false;
    }
    
    if (waypoints.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please add at least one waypoint.");
        tabWidget->setCurrentIndex(1);
        return false;
    }
    
    if (waypoints.size() < 2) {
        QMessageBox::warning(this, "Validation Error", "A route must have at least 2 waypoints.");
        tabWidget->setCurrentIndex(1);
        return false;
    }
    
    return true;
}

double RouteFormDialog::calculateRouteDistance()
{
    if (waypoints.size() < 2) return 0.0;
    
    double totalDistance = 0.0;
    
    for (int i = 1; i < waypoints.size(); ++i) {
        const RouteWaypointData &wp1 = waypoints[i-1];
        const RouteWaypointData &wp2 = waypoints[i];
        
        // Simple great circle distance calculation
        double lat1 = qDegreesToRadians(wp1.lat);
        double lon1 = qDegreesToRadians(wp1.lon);
        double lat2 = qDegreesToRadians(wp2.lat);
        double lon2 = qDegreesToRadians(wp2.lon);
        
        double deltaLat = lat2 - lat1;
        double deltaLon = lon2 - lon1;
        
        double a = qSin(deltaLat/2) * qSin(deltaLat/2) +
                   qCos(lat1) * qCos(lat2) *
                   qSin(deltaLon/2) * qSin(deltaLon/2);
        double c = 2 * qAtan2(qSqrt(a), qSqrt(1-a));
        
        // Earth radius in nautical miles
        double distance = 3440.065 * c;
        totalDistance += distance;
    }
    
    return totalDistance;
}

QString RouteFormDialog::formatDistance(double distanceNM)
{
    if (distanceNM < 1.0) {
        return QString("%1 m").arg(qRound(distanceNM * 1852));
    } else {
        return QString("%1 NM").arg(distanceNM, 0, 'f', 2);
    }
}

// Getters
QString RouteFormDialog::getRouteName() const
{
    return routeNameEdit->text().trimmed();
}

QString RouteFormDialog::getRouteDescription() const
{
    return routeDescriptionEdit->toPlainText().trimmed();
}

int RouteFormDialog::getRouteId() const
{
    return routeIdSpinBox->value();
}

QList<RouteWaypointData> RouteFormDialog::getWaypoints() const
{
    return waypoints;
}

// Setters
void RouteFormDialog::setRouteName(const QString& name)
{
    routeNameEdit->setText(name);
}

void RouteFormDialog::setRouteDescription(const QString& description)
{
    routeDescriptionEdit->setPlainText(description);
}

void RouteFormDialog::setRouteId(int routeId)
{
    routeIdSpinBox->setValue(routeId);
}

int RouteFormDialog::generateNextRouteId()
{
    if (!ecWidget) {
        return 1; // Default if no EcWidget available
    }
    
    // Get existing routes
    QList<EcWidget::Route> existingRoutes = ecWidget->getRoutes();
    
    if (existingRoutes.isEmpty()) {
        return 1; // First route
    }
    
    // Find highest route ID and add 1
    int maxId = 0;
    for (const auto& route : existingRoutes) {
        if (route.routeId > maxId) {
            maxId = route.routeId;
        }
    }
    
    return maxId + 1;
}

void RouteFormDialog::loadRouteData(int routeId)
{
    if (!ecWidget) return;
    
    // Get route data from EcWidget
    EcWidget::Route route = ecWidget->getRouteById(routeId);
    
    if (route.routeId == 0) {
        QMessageBox::warning(this, "Route Not Found", 
            QString("Route with ID %1 not found").arg(routeId));
        return;
    }
    
    // Load route information
    routeIdSpinBox->setValue(route.routeId);
    routeNameEdit->setText(route.name);
    routeDescriptionEdit->setPlainText(route.description);
    
    // Clear existing waypoints and load route waypoints
    waypoints.clear();
    for (const auto& routeWp : route.waypoints) {
        RouteWaypointData wp;
        wp.lat = routeWp.lat;
        wp.lon = routeWp.lon;
        wp.label = routeWp.label;
        wp.remark = routeWp.remark;
        wp.turningRadius = routeWp.turningRadius;
        wp.active = routeWp.active;
        waypoints.append(wp);
    }
    
    // Update UI
    updateWaypointList();
    updateButtonStates();
    
    // Change dialog title to indicate edit mode
    setWindowTitle(QString("Edit Route: %1").arg(route.name));
    
    // Change OK button text
    okButton->setText("Update Route");
    
    statusLabel->setText(QString("Loaded route '%1' with %2 waypoints for editing")
        .arg(route.name)
        .arg(waypoints.size()));
}


