#include "autoroutedialog.h"
#include <QApplication>
#include <QPalette>
#include <cmath>

// Helper function to calculate great circle distance
static double calculateGreatCircleDistance(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 3440.065; // Earth radius in nautical miles
    const double toRad = M_PI / 180.0;

    double dLat = (lat2 - lat1) * toRad;
    double dLon = (lon2 - lon1) * toRad;

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(lat1 * toRad) * std::cos(lat2 * toRad) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);

    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return R * c;
}

AutoRouteDialog::AutoRouteDialog(EcCoordinate targetLat, EcCoordinate targetLon,
                               EcCoordinate startLat, EcCoordinate startLon,
                               QWidget* parent)
    : QDialog(parent)
    , targetLat(targetLat)
    , targetLon(targetLon)
    , startLat(startLat)
    , startLon(startLon)
{
    // Calculate straight-line distance
    straightLineDistance = calculateGreatCircleDistance(startLat, startLon, targetLat, targetLon);

    setupUI();
    updateEstimates();
}

void AutoRouteDialog::setupUI()
{
    setWindowTitle("Auto Route Generation");
    setModal(true);
    resize(520, 420); // Adjusted height for safety section
    setStyleSheet(getDialogStyleSheet());

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // ========== ROUTE INFORMATION ==========
    QGroupBox* infoGroup = new QGroupBox("Route Information");
    QFormLayout* infoLayout = new QFormLayout(infoGroup);
    infoLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    startPosLabel = new QLabel(formatCoordinate(startLat, startLon));
    startPosLabel->setMinimumWidth(300);
    startPosLabel->setWordWrap(false);

    targetPosLabel = new QLabel(formatCoordinate(targetLat, targetLon));
    targetPosLabel->setMinimumWidth(300);
    targetPosLabel->setWordWrap(false);

    distanceLabel = new QLabel(formatDistance(straightLineDistance));

    infoLayout->addRow("From:", startPosLabel);
    infoLayout->addRow("To:", targetPosLabel);
    infoLayout->addRow("Direct Distance:", distanceLabel);

    mainLayout->addWidget(infoGroup);

    // ========== OPTIMIZATION STRATEGY (HIDDEN) ==========
    QGroupBox* optimizationGroup = new QGroupBox("Optimization Strategy");
    QVBoxLayout* optimizationLayout = new QVBoxLayout(optimizationGroup);

    shortestDistanceRadio = new QRadioButton("Shortest Distance");
    shortestDistanceRadio->setToolTip("Minimize total route distance (nautical miles)");

    fastestTimeRadio = new QRadioButton("Fastest Time");
    fastestTimeRadio->setToolTip("Minimize travel time considering speed");
    fastestTimeRadio->setChecked(true);

    safestRouteRadio = new QRadioButton("Safest Route");
    safestRouteRadio->setToolTip("Maximize depth clearance and avoid hazards");

    balancedRadio = new QRadioButton("Balanced (Distance + Safety)");
    balancedRadio->setToolTip("Balance between distance, time, and safety");

    optimizationLayout->addWidget(shortestDistanceRadio);
    optimizationLayout->addWidget(fastestTimeRadio);
    optimizationLayout->addWidget(safestRouteRadio);
    optimizationLayout->addWidget(balancedRadio);

    connect(shortestDistanceRadio, &QRadioButton::toggled, this, [this]() { updateEstimates(); });
    connect(fastestTimeRadio, &QRadioButton::toggled, this, [this]() { updateEstimates(); });
    connect(safestRouteRadio, &QRadioButton::toggled, this, [this]() { updateEstimates(); });
    connect(balancedRadio, &QRadioButton::toggled, this, [this]() { updateEstimates(); });

    optimizationGroup->setVisible(false); // Hide this section
    mainLayout->addWidget(optimizationGroup);

    // ========== ROUTE PARAMETERS (HIDDEN) ==========
    QGroupBox* paramsGroup = new QGroupBox("Route Parameters");
    QFormLayout* paramsLayout = new QFormLayout(paramsGroup);

    speedSpinBox = new QDoubleSpinBox();
    speedSpinBox->setRange(1.0, 50.0);
    speedSpinBox->setValue(10.0);
    speedSpinBox->setSuffix(" knots");
    speedSpinBox->setDecimals(1);
    speedSpinBox->setToolTip("Planned speed for the route");
    connect(speedSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() { updateEstimates(); });

    waypointDensitySpinBox = new QSpinBox();
    waypointDensitySpinBox->setRange(1, 20);
    waypointDensitySpinBox->setValue(5);
    waypointDensitySpinBox->setSuffix(" NM");
    waypointDensitySpinBox->setToolTip("Generate one waypoint every X nautical miles");
    connect(waypointDensitySpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this]() { updateEstimates(); });

    paramsLayout->addRow("Planned Speed:", speedSpinBox);
    paramsLayout->addRow("Waypoint Spacing:", waypointDensitySpinBox);

    paramsGroup->setVisible(false); // Hide this section
    mainLayout->addWidget(paramsGroup);

    // ========== SAFETY OPTIONS ==========
    QGroupBox* safetyGroup = new QGroupBox("Safety & Hazard Avoidance");
    QVBoxLayout* safetyLayout = new QVBoxLayout(safetyGroup);

    // Avoid Shallow Water
    QHBoxLayout* shallowLayout = new QHBoxLayout();
    avoidShallowCheckBox = new QCheckBox("Avoid Shallow Water - Min Depth:");
    avoidShallowCheckBox->setChecked(true);
    minDepthSpinBox = new QDoubleSpinBox();
    minDepthSpinBox->setRange(1.0, 100.0);
    minDepthSpinBox->setValue(15.0);
    minDepthSpinBox->setSuffix(" m");
    minDepthSpinBox->setDecimals(1);
    minDepthSpinBox->setToolTip("Minimum safe water depth for route planning");
    shallowLayout->addWidget(avoidShallowCheckBox);
    shallowLayout->addWidget(minDepthSpinBox);
    shallowLayout->addStretch();
    connect(avoidShallowCheckBox, &QCheckBox::stateChanged, this, &AutoRouteDialog::onAvoidShallowChanged);
    safetyLayout->addLayout(shallowLayout);

    // Avoid Hazards
    avoidHazardsCheckBox = new QCheckBox("Avoid Dangerous Objects & Obstructions");
    avoidHazardsCheckBox->setChecked(true);
    avoidHazardsCheckBox->setToolTip("Route will avoid rocks, wrecks, and other dangers");
    safetyLayout->addWidget(avoidHazardsCheckBox);

    // Consider UKC (Hidden but still functional)
    considerUKCCheckBox = new QCheckBox("Under Keel Clearance");
    considerUKCCheckBox->setChecked(false);
    considerUKCCheckBox->setVisible(false);
    minUKCSpinBox = new QDoubleSpinBox();
    minUKCSpinBox->setValue(2.0);
    minUKCSpinBox->setVisible(false);

    // Safety Corridors (Hidden - future feature)
    useSafetyCorridorsCheckBox = new QCheckBox();
    useSafetyCorridorsCheckBox->setChecked(false);
    useSafetyCorridorsCheckBox->setVisible(false);

    mainLayout->addWidget(safetyGroup);

    // ========== ESTIMATES ==========
    QGroupBox* estimatesGroup = new QGroupBox("Estimated Route");
    QFormLayout* estimatesLayout = new QFormLayout(estimatesGroup);

    estimatedDistanceLabel = new QLabel("--");
    estimatedTimeLabel = new QLabel("--");
    estimatedWaypointsLabel = new QLabel("--");

    estimatesLayout->addRow("Distance:", estimatedDistanceLabel);
    estimatesLayout->addRow("Travel Time:", estimatedTimeLabel);
    estimatesLayout->addRow("Waypoints:", estimatedWaypointsLabel);

    mainLayout->addWidget(estimatesGroup);

    // ========== BUTTONS ==========
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    generateButton = new QPushButton("Generate Route");
    generateButton->setDefault(true);
    generateButton->setMinimumHeight(35);
    generateButton->setMinimumWidth(120);
    generateButton->setStyleSheet(
        "QPushButton { "
        "background-color: #14b8a6; "
        "color: white; "
        "border: none; "
        "border-radius: 4px; "
        "padding: 8px 20px; "
        "font-weight: bold; "
        "font-size: 13px; "
        "} "
        "QPushButton:hover { "
        "background-color: #0d9488; "
        "} "
        "QPushButton:pressed { "
        "background-color: #0f766e; "
        "}"
    );
    connect(generateButton, &QPushButton::clicked, this, &AutoRouteDialog::onGenerateClicked);

    cancelButton = new QPushButton("Cancel");
    cancelButton->setMinimumHeight(35);
    cancelButton->setMinimumWidth(100);
    cancelButton->setStyleSheet(
        "QPushButton { "
        "background-color: #6b7280; "
        "color: white; "
        "border: none; "
        "border-radius: 4px; "
        "padding: 8px 20px; "
        "font-size: 13px; "
        "} "
        "QPushButton:hover { "
        "background-color: #4b5563; "
        "} "
        "QPushButton:pressed { "
        "background-color: #374151; "
        "}"
    );
    connect(cancelButton, &QPushButton::clicked, this, &AutoRouteDialog::onCancelClicked);

    buttonLayout->addStretch();
    buttonLayout->addWidget(generateButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->setSpacing(10);

    mainLayout->addLayout(buttonLayout);
}

void AutoRouteDialog::updateEstimates()
{
    double speed = speedSpinBox->value();
    int spacing = waypointDensitySpinBox->value();

    // Estimate distance (add 10-15% for routing overhead)
    double estimatedDistance = straightLineDistance;

    if (safestRouteRadio->isChecked()) {
        estimatedDistance *= 1.15; // Safest route may be longer
    } else if (balancedRadio->isChecked()) {
        estimatedDistance *= 1.08;
    } else {
        estimatedDistance *= 1.05; // Minimal overhead
    }

    // Estimate time
    double estimatedHours = estimatedDistance / speed;

    // Estimate waypoints
    int estimatedWaypoints = qMax(2, (int)(estimatedDistance / spacing) + 2);

    // Update labels
    estimatedDistanceLabel->setText(formatDistance(estimatedDistance));
    estimatedTimeLabel->setText(formatTime(estimatedHours));
    estimatedWaypointsLabel->setText(QString::number(estimatedWaypoints));
}

void AutoRouteDialog::onOptimizationChanged(int index)
{
    updateEstimates();
}

void AutoRouteDialog::onAvoidShallowChanged(int state)
{
    minDepthSpinBox->setEnabled(state == Qt::Checked);
}

void AutoRouteDialog::onGenerateClicked()
{
    emit routeOptionsConfirmed(getOptions());
    accept();
}

void AutoRouteDialog::onCancelClicked()
{
    reject();
}

AutoRouteOptions AutoRouteDialog::getOptions() const
{
    AutoRouteOptions options;

    // Optimization strategy
    if (shortestDistanceRadio->isChecked()) {
        options.optimization = RouteOptimization::SHORTEST_DISTANCE;
    } else if (fastestTimeRadio->isChecked()) {
        options.optimization = RouteOptimization::FASTEST_TIME;
    } else if (safestRouteRadio->isChecked()) {
        options.optimization = RouteOptimization::SAFEST_ROUTE;
    } else if (balancedRadio->isChecked()) {
        options.optimization = RouteOptimization::BALANCED;
    }

    // Parameters
    options.plannedSpeed = speedSpinBox->value();
    options.waypointDensity = waypointDensitySpinBox->value();

    // Safety options
    options.avoidShallowWater = avoidShallowCheckBox->isChecked();
    options.minDepth = minDepthSpinBox->value();
    options.considerUKC = considerUKCCheckBox->isChecked();
    options.minUKC = minUKCSpinBox->value();
    options.avoidHazards = avoidHazardsCheckBox->isChecked();
    options.stayInSafetyCorridors = useSafetyCorridorsCheckBox->isChecked();

    return options;
}

QString AutoRouteDialog::formatCoordinate(double lat, double lon) const
{
    auto formatDegMin = [](double value, bool isLat) -> QString {
        double absVal = std::abs(value);
        int deg = static_cast<int>(std::floor(absVal));
        double minutes = (absVal - deg) * 60.0;
        QChar hemi;
        if (isLat) hemi = (value >= 0.0) ? 'N' : 'S';
        else hemi = (value >= 0.0) ? 'E' : 'W';
        return QString("%1° %2' %3")
            .arg(deg, 2, 10, QChar('0'))
            .arg(minutes, 6, 'f', 3, QChar('0'))
            .arg(hemi);
    };

    return QString("%1, %2").arg(formatDegMin(lat, true)).arg(formatDegMin(lon, false));
}

QString AutoRouteDialog::formatDistance(double distanceNM) const
{
    return QString("%1 NM").arg(distanceNM, 0, 'f', 2);
}

QString AutoRouteDialog::formatTime(double hours) const
{
    int h = static_cast<int>(hours);
    int m = static_cast<int>((hours - h) * 60);
    return QString("%1h %2m").arg(h).arg(m, 2, 10, QChar('0'));
}

QString AutoRouteDialog::getDialogStyleSheet() const
{
    bool isDark = (QApplication::palette().color(QPalette::Window).lightness() < 128);

    if (isDark) {
        return R"(
            QDialog {
                background-color: #2b2b2b;
                color: #e0e0e0;
            }
            QGroupBox {
                font-weight: bold;
                border: 1px solid #555;
                border-radius: 5px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
            QLabel {
                color: #e0e0e0;
            }
        )";
    } else {
        return R"(
            QGroupBox {
                font-weight: bold;
                border: 1px solid #ccc;
                border-radius: 5px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
        )";
    }
}
