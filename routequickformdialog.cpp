#include "routequickformdialog.h"
#include "ecwidget.h"
#include <QtMath>
#include <QMessageBox>
#include <QIntValidator>

RouteQuickFormDialog::RouteQuickFormDialog(QWidget* parent, EcWidget* ecWidget)
    : QDialog(parent), ecWidget(ecWidget),
      labelEdit(nullptr), unitGroup(nullptr), decDegBtn(nullptr), degMinBtn(nullptr), metersBtn(nullptr),
      latLabel(nullptr), lonLabel(nullptr), latEdit(nullptr), lonEdit(nullptr), decMetersGroup(nullptr),
      degMinGroup(nullptr), latDegEdit(nullptr), latMinEdit(nullptr), latHemCombo(nullptr),
      lonDegEdit(nullptr), lonMinEdit(nullptr), lonHemCombo(nullptr),
      remarkEdit(nullptr), activeCheck(nullptr), addBtn(nullptr), clearBtn(nullptr), createBtn(nullptr), cancelBtn(nullptr),
      table(nullptr), latValidator(nullptr), lonValidator(nullptr)
{
    setWindowTitle(QString("Create Route (Quick) - Route %1").arg(generateNextRouteId(), 3, 10, QChar('0')));
    setModal(true);
    resize(700, 480);
    setupUI();
}

void RouteQuickFormDialog::setupUI()
{
    QVBoxLayout* main = new QVBoxLayout(this);

    // Form
    QFormLayout* form = new QFormLayout();

    // Route name
    routeNameEdit = new QLineEdit();
    routeNameEdit->setText(QString("Route %1").arg(generateNextRouteId(), 3, 10, QChar('0')));
    form->addRow("Route Name:", routeNameEdit);

    labelEdit = new QLineEdit();
    form->addRow("Label:", labelEdit);

    // Unit radio group
    unitGroup = new QGroupBox("Coordinate Units");
    QHBoxLayout* unitLayout = new QHBoxLayout(unitGroup);
    decDegBtn = new QRadioButton("Decimal Degrees");
    degMinBtn = new QRadioButton("Deg-Min");
    metersBtn = new QRadioButton("Meters (N/E)");
    decDegBtn->setChecked(true);
    unitLayout->addWidget(decDegBtn);
    unitLayout->addWidget(degMinBtn);
    unitLayout->addWidget(metersBtn);
    form->addRow(unitGroup);

    // Decimal degrees or meters inputs group
    decMetersGroup = new QGroupBox("Coordinates");
    QFormLayout* decMetersLayout = new QFormLayout(decMetersGroup);
    latLabel = new QLabel("Latitude:");
    lonLabel = new QLabel("Longitude:");
    latEdit = new QLineEdit();
    lonEdit = new QLineEdit();
    latValidator = new QDoubleValidator(-90.0, 90.0, 6, this);
    lonValidator = new QDoubleValidator(-180.0, 180.0, 6, this);
    latEdit->setValidator(latValidator);
    lonEdit->setValidator(lonValidator);
    latEdit->setPlaceholderText("e.g., -7.256940");
    lonEdit->setPlaceholderText("e.g., 112.751940");
    decMetersLayout->addRow(latLabel, latEdit);
    decMetersLayout->addRow(lonLabel, lonEdit);

    // Deg-Min inputs group
    degMinGroup = new QGroupBox("Deg-Min Coordinates");
    QGridLayout* degMinLayout = new QGridLayout(degMinGroup);
    latDegEdit = new QLineEdit(); latDegEdit->setValidator(new QIntValidator(0, 90, this)); latDegEdit->setMaximumWidth(70);
    latMinEdit = new QLineEdit(); latMinEdit->setValidator(new QDoubleValidator(0.0, 60.0, 3, this)); latMinEdit->setMaximumWidth(100);
    latHemCombo = new QComboBox(); latHemCombo->addItems({"N", "S"}); latHemCombo->setMaximumWidth(60);
    lonDegEdit = new QLineEdit(); lonDegEdit->setValidator(new QIntValidator(0, 180, this)); lonDegEdit->setMaximumWidth(70);
    lonMinEdit = new QLineEdit(); lonMinEdit->setValidator(new QDoubleValidator(0.0, 60.0, 3, this)); lonMinEdit->setMaximumWidth(100);
    lonHemCombo = new QComboBox(); lonHemCombo->addItems({"E", "W"}); lonHemCombo->setMaximumWidth(60);

    degMinLayout->addWidget(new QLabel("Lat Deg"), 0, 0);
    degMinLayout->addWidget(latDegEdit, 0, 1);
    degMinLayout->addWidget(new QLabel("Lat Min"), 0, 2);
    degMinLayout->addWidget(latMinEdit, 0, 3);
    degMinLayout->addWidget(new QLabel("N/S"), 0, 4);
    degMinLayout->addWidget(latHemCombo, 0, 5);
    degMinLayout->addWidget(new QLabel("Lon Deg"), 1, 0);
    degMinLayout->addWidget(lonDegEdit, 1, 1);
    degMinLayout->addWidget(new QLabel("Lon Min"), 1, 2);
    degMinLayout->addWidget(lonMinEdit, 1, 3);
    degMinLayout->addWidget(new QLabel("E/W"), 1, 4);
    degMinLayout->addWidget(lonHemCombo, 1, 5);

    form->addRow(decMetersGroup);
    form->addRow(degMinGroup);

    remarkEdit = new QLineEdit();
    form->addRow("Remark:", remarkEdit);

    activeCheck = new QCheckBox();
    activeCheck->setChecked(true);
    form->addRow("Active:", activeCheck);

    main->addLayout(form);

    // Buttons row for input actions
    QHBoxLayout* actions = new QHBoxLayout();
    addBtn = new QPushButton("+ Add Waypoint");
    clearBtn = new QPushButton("Clear Inputs");
    actions->addWidget(addBtn);
    actions->addWidget(clearBtn);
    actions->addStretch();
    main->addLayout(actions);

    // Table for waypoints list
    table = new QTableWidget(0, 6, this);
    table->setHorizontalHeaderLabels({"#", "Label", "Latitude (Deg-Min)", "Longitude (Deg-Min)", "Remark", "Active"});
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->horizontalHeader()->setStretchLastSection(false);
    table->setColumnWidth(0, 40);
    table->setColumnWidth(1, 120);
    table->setColumnWidth(2, 130);
    table->setColumnWidth(3, 130);
    table->setColumnWidth(4, 180);
    table->setColumnWidth(5, 70);
    table->setMinimumHeight(220);
    main->addWidget(table);

    // Dialog buttons
    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->addStretch();
    cancelBtn = new QPushButton("Cancel");
    createBtn = new QPushButton("Create Route");
    createBtn->setEnabled(false);
    bottom->addWidget(cancelBtn);
    bottom->addWidget(createBtn);
    main->addLayout(bottom);

    connect(addBtn, &QPushButton::clicked, this, &RouteQuickFormDialog::onAddWaypoint);
    connect(clearBtn, &QPushButton::clicked, this, &RouteQuickFormDialog::onClearInputs);
    connect(createBtn, &QPushButton::clicked, this, &RouteQuickFormDialog::onCreateRoute);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(decDegBtn, &QRadioButton::toggled, this, &RouteQuickFormDialog::onUnitChanged);
    connect(degMinBtn, &QRadioButton::toggled, this, &RouteQuickFormDialog::onUnitChanged);
    connect(metersBtn, &QRadioButton::toggled, this, &RouteQuickFormDialog::onUnitChanged);
    onUnitChanged();
}

void RouteQuickFormDialog::onUnitChanged()
{
    if (decDegBtn->isChecked()) {
        latLabel->setText("Latitude:");
        lonLabel->setText("Longitude:");
        latEdit->setValidator(latValidator);
        lonEdit->setValidator(lonValidator);
        latEdit->setPlaceholderText("e.g., -7.256940");
        lonEdit->setPlaceholderText("e.g., 112.751940");
        decMetersGroup->setVisible(true);
        degMinGroup->setVisible(false);
    } else if (degMinBtn->isChecked()) {
        decMetersGroup->setVisible(false);
        degMinGroup->setVisible(true);
    } else { // Meters
        latLabel->setText("North (m):");
        lonLabel->setText("East (m):");
        latEdit->setValidator(new QDoubleValidator(-1e7, 1e7, 3, this));
        lonEdit->setValidator(new QDoubleValidator(-1e7, 1e7, 3, this));
        latEdit->setPlaceholderText("0");
        lonEdit->setPlaceholderText("0");
        decMetersGroup->setVisible(true);
        degMinGroup->setVisible(false);
    }
}

void RouteQuickFormDialog::onAddWaypoint()
{
    RouteWaypointData wp;
    wp.label = labelEdit->text().trimmed();
    wp.remark = remarkEdit->text().trimmed();
    wp.active = activeCheck->isChecked();

    double lat = 0.0, lon = 0.0;
    bool okCoords = false;
    if (decDegBtn->isChecked()) {
        bool okLat=false, okLon=false;
        lat = latEdit->text().toDouble(&okLat);
        lon = lonEdit->text().toDouble(&okLon);
        okCoords = okLat && okLon;
    } else if (metersBtn->isChecked()) {
        bool okN=false, okE=false;
        double north = latEdit->text().toDouble(&okN);
        double east = lonEdit->text().toDouble(&okE);
        if (okN && okE) {
            metersToLatLon(north, east, lat, lon);
            okCoords = true;
        }
    } else { // Deg-Min
        bool okDegLat = !latDegEdit->text().isEmpty();
        bool okMinLat = !latMinEdit->text().isEmpty();
        bool okDegLon = !lonDegEdit->text().isEmpty();
        bool okMinLon = !lonMinEdit->text().isEmpty();
        int dlat = latDegEdit->text().toInt();
        double mlat = latMinEdit->text().toDouble();
        int dlon = lonDegEdit->text().toInt();
        double mlon = lonMinEdit->text().toDouble();
        int signLat = (latHemCombo->currentText() == "S") ? -1 : 1;
        int signLon = (lonHemCombo->currentText() == "W") ? -1 : 1;
        double outLat = 0.0, outLon = 0.0;
        bool ok1 = okDegLat && okMinLat && parseDegMinNumeric(dlat, mlat, true, signLat, outLat);
        bool ok2 = okDegLon && okMinLon && parseDegMinNumeric(dlon, mlon, false, signLon, outLon);
        if (ok1 && ok2) { lat = outLat; lon = outLon; okCoords = true; }
    }
    if (!okCoords) {
        QMessageBox::warning(this, "Invalid Coordinates", "Please enter valid coordinates based on selected unit.");
        return;
    }
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        QMessageBox::warning(this, "Invalid Coordinates",
                             "Latitude must be within [-90, 90] and longitude within [-180, 180].");
        return;
    }

    wp.lat = lat;
    wp.lon = lon;

    waypoints.append(wp);
    updateTable();

    // Enable create when >= 2 points
    createBtn->setEnabled(waypoints.size() >= 2);

    // Prepare next input
    labelEdit->clear();
    latEdit->clear();
    lonEdit->clear();
    remarkEdit->clear();
    activeCheck->setChecked(true);
    labelEdit->setFocus();
}

void RouteQuickFormDialog::onClearInputs()
{
    labelEdit->clear();
    latEdit->clear();
    lonEdit->clear();
    remarkEdit->clear();
    activeCheck->setChecked(true);
    labelEdit->setFocus();
}

void RouteQuickFormDialog::onCreateRoute()
{
    if (waypoints.size() < 2) {
        QMessageBox::information(this, "Add Waypoints", "Please add at least two waypoints.");
        return;
    }
    accept();
}

void RouteQuickFormDialog::updateTable()
{
    table->setRowCount(waypoints.size());
    for (int i = 0; i < waypoints.size(); ++i) {
        const auto& wp = waypoints[i];
        auto* num = new QTableWidgetItem(QString::number(i + 1));
        num->setTextAlignment(Qt::AlignCenter);
        num->setFlags(num->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 0, num);

        table->setItem(i, 1, new QTableWidgetItem(wp.label));
        table->setItem(i, 2, new QTableWidgetItem(formatDegMin(wp.lat, true)));
        table->setItem(i, 3, new QTableWidgetItem(formatDegMin(wp.lon, false)));
        table->setItem(i, 4, new QTableWidgetItem(wp.remark));
        table->setItem(i, 5, new QTableWidgetItem(wp.active ? "Yes" : "No"));
        table->setRowHeight(i, 28);
    }
}

bool RouteQuickFormDialog::parseDegMinNumeric(int deg, double minutes, bool isLat, int sign, double& out)
{
    if (minutes < 0.0 || minutes >= 60.0) return false;
    if (isLat && (deg < 0 || deg > 90)) return false;
    if (!isLat && (deg < 0 || deg > 180)) return false;
    out = sign * (deg + minutes / 60.0);
    return true;
}

QString RouteQuickFormDialog::formatDegMin(double value, bool isLat) const
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

void RouteQuickFormDialog::metersToLatLon(double north, double east, double& outLat, double& outLon)
{
    EcCoordinate oLat=0.0, oLon=0.0;
    if (ecWidget) ecWidget->GetCenter(oLat, oLon);
    double metersPerDegLat = 111320.0;
    double metersPerDegLon = metersPerDegLat * qCos(qDegreesToRadians(static_cast<double>(oLat)));
    outLat = static_cast<double>(oLat) + (north / metersPerDegLat);
    if (metersPerDegLon == 0.0) metersPerDegLon = 1.0;
    outLon = static_cast<double>(oLon) + (east / metersPerDegLon);
}

int RouteQuickFormDialog::generateNextRouteId() const
{
    if (!ecWidget) return 1;
    auto routes = ecWidget->getRoutes();
    int maxId = 0;
    for (const auto& r : routes) maxId = qMax(maxId, r.routeId);
    return maxId + 1;
}
