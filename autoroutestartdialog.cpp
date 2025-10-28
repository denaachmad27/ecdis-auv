#include "autoroutestartdialog.h"
#include <QApplication>
#include <QPalette>
#include <QDebug>
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

AutoRouteStartDialog::AutoRouteStartDialog(EcCoordinate ownShipLat, EcCoordinate ownShipLon,
                                           EcCoordinate targetLat, EcCoordinate targetLon,
                                           QWidget* parent)
    : QDialog(parent)
    , m_ownShipLat(ownShipLat)
    , m_ownShipLon(ownShipLon)
    , m_targetLat(targetLat)
    , m_targetLon(targetLon)
    , m_manualLat(0.0)
    , m_manualLon(0.0)
    , m_manualPositionSet(false)
    , m_confirmed(false)
{
    qDebug() << "[AutoRouteStartDialog] Constructor called";
    setupUI();
    updateLabels();
    qDebug() << "[AutoRouteStartDialog] Dialog setup complete";
}

void AutoRouteStartDialog::setupUI()
{
    setWindowTitle("Select Auto Route Starting Point");
    setModal(true);
    resize(500, 400);
    setStyleSheet(getDialogStyleSheet());

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    // ========== TARGET INFO ==========
    QGroupBox* targetGroup = new QGroupBox("Target Destination");
    QVBoxLayout* targetLayout = new QVBoxLayout(targetGroup);

    m_targetPosLabel = new QLabel();
    m_targetPosLabel->setStyleSheet("font-weight: bold;");
    targetLayout->addWidget(m_targetPosLabel);

    mainLayout->addWidget(targetGroup);

    // ========== START POINT SELECTION ==========
    QGroupBox* startGroup = new QGroupBox("Select Starting Point");
    QVBoxLayout* startLayout = new QVBoxLayout(startGroup);

    // Option 1: From Own Ship
    m_ownShipRadio = new QRadioButton("Start from Own Ship Position");
    m_ownShipRadio->setChecked(true);
    m_ownShipRadio->setToolTip("Route will start from your current vessel position");
    connect(m_ownShipRadio, &QRadioButton::toggled, this, &AutoRouteStartDialog::onOwnShipRadioToggled);
    startLayout->addWidget(m_ownShipRadio);

    m_ownShipPosLabel = new QLabel();
    m_ownShipPosLabel->setStyleSheet("margin-left: 25px; color: #666;");
    startLayout->addWidget(m_ownShipPosLabel);

    startLayout->addSpacing(10);

    // Option 2: Manual Selection
    m_manualRadio = new QRadioButton("Select Start Position Manually");
    m_manualRadio->setToolTip("Choose a custom starting point on the chart");
    connect(m_manualRadio, &QRadioButton::toggled, this, &AutoRouteStartDialog::onManualRadioToggled);
    startLayout->addWidget(m_manualRadio);

    // Manual selection controls
    QHBoxLayout* manualControlLayout = new QHBoxLayout();
    manualControlLayout->setContentsMargins(25, 5, 0, 5);

    m_selectOnMapButton = new QPushButton("Click on Map to Select");
    m_selectOnMapButton->setEnabled(false);
    m_selectOnMapButton->setMinimumHeight(30);
    m_selectOnMapButton->setMinimumWidth(180);
    m_selectOnMapButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_selectOnMapButton->setToolTip("Click this button, then click on the chart to select starting position");
    connect(m_selectOnMapButton, &QPushButton::clicked, this, &AutoRouteStartDialog::onSelectOnMapClicked);
    manualControlLayout->addWidget(m_selectOnMapButton);
    manualControlLayout->addStretch();

    startLayout->addLayout(manualControlLayout);

    m_manualPosLabel = new QLabel("Not selected");
    m_manualPosLabel->setStyleSheet("margin-left: 25px; color: #999; font-style: italic;");
    startLayout->addWidget(m_manualPosLabel);

    mainLayout->addWidget(startGroup);

    // ========== DISTANCE INFO ==========
    QGroupBox* infoGroup = new QGroupBox("Route Information");
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);

    m_distanceLabel = new QLabel();
    infoLayout->addWidget(m_distanceLabel);

    mainLayout->addWidget(infoGroup);

    // ========== INSTRUCTION ==========
    m_instructionLabel = new QLabel();
    m_instructionLabel->setWordWrap(true);
    m_instructionLabel->setStyleSheet("color: #0066cc; font-style: italic; padding: 10px; background-color: rgba(0, 102, 204, 0.1); border-radius: 5px;");
    mainLayout->addWidget(m_instructionLabel);

    mainLayout->addStretch();

    // ========== BUTTONS ==========
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_continueButton = new QPushButton("Continue to Route Options");
    m_continueButton->setDefault(true);
    m_continueButton->setMinimumHeight(35);
    m_continueButton->setToolTip("Proceed to configure auto route parameters");
    connect(m_continueButton, &QPushButton::clicked, this, &AutoRouteStartDialog::onContinueClicked);

    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setMinimumHeight(35);
    connect(m_cancelButton, &QPushButton::clicked, this, &AutoRouteStartDialog::onCancelClicked);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_continueButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void AutoRouteStartDialog::updateLabels()
{
    // Target position
    m_targetPosLabel->setText(QString("Target: %1").arg(formatCoordinate(m_targetLat, m_targetLon)));

    // Own ship position
    m_ownShipPosLabel->setText(formatCoordinate(m_ownShipLat, m_ownShipLon));

    // Manual position
    if (m_manualPositionSet) {
        m_manualPosLabel->setText(formatCoordinate(m_manualLat, m_manualLon));
        m_manualPosLabel->setStyleSheet("margin-left: 25px; color: #00aa00; font-weight: bold;");
    } else {
        m_manualPosLabel->setText("Not selected");
        m_manualPosLabel->setStyleSheet("margin-left: 25px; color: #999; font-style: italic;");
    }

    // Distance calculation
    double startLat = m_ownShipRadio->isChecked() ? m_ownShipLat : m_manualLat;
    double startLon = m_ownShipRadio->isChecked() ? m_ownShipLon : m_manualLon;

    if (m_ownShipRadio->isChecked() || m_manualPositionSet) {
        double distance = calculateGreatCircleDistance(startLat, startLon, m_targetLat, m_targetLon);
        m_distanceLabel->setText(QString("Direct distance: %1").arg(formatDistance(distance)));
    } else {
        m_distanceLabel->setText("Distance: Not available (select start position first)");
    }

    // Instruction label
    if (m_ownShipRadio->isChecked()) {
        m_instructionLabel->setText("Route will be generated from your current position to the selected target.");
    } else if (m_manualPositionSet) {
        m_instructionLabel->setText("Route will be generated from the selected start position to the target.");
    } else {
        m_instructionLabel->setText("Click \"Click on Map to Select\" button, then click on the chart to choose your starting position. A shadow marker will appear to help you visualize the location.");
    }
}

void AutoRouteStartDialog::onOwnShipRadioToggled(bool checked)
{
    qDebug() << "[AutoRouteStartDialog] OwnShip radio toggled:" << checked;
    if (checked) {
        m_selectOnMapButton->setEnabled(false);
        updateLabels();
    }
}

void AutoRouteStartDialog::onManualRadioToggled(bool checked)
{
    qDebug() << "[AutoRouteStartDialog] Manual radio toggled:" << checked << "- Button will be enabled:" << checked;
    if (checked) {
        m_selectOnMapButton->setEnabled(true);
        qDebug() << "[AutoRouteStartDialog] Select on map button is now enabled";
        updateLabels();
    }
}

void AutoRouteStartDialog::onSelectOnMapClicked()
{
    qDebug() << "[AutoRouteStartDialog] Select on map clicked - emitting signal";

    // Hide dialog temporarily and emit signal for map selection
    hide();
    emit manualSelectionRequested();

    qDebug() << "[AutoRouteStartDialog] Dialog hidden, signal emitted";
}

void AutoRouteStartDialog::onContinueClicked()
{
    if (m_manualRadio->isChecked() && !m_manualPositionSet) {
        m_instructionLabel->setText("⚠ Please select a starting position on the map first!");
        m_instructionLabel->setStyleSheet("color: #cc0000; font-style: italic; font-weight: bold; padding: 10px; background-color: rgba(204, 0, 0, 0.1); border-radius: 5px;");
        return;
    }

    m_confirmed = true;
    accept();
}

void AutoRouteStartDialog::onCancelClicked()
{
    m_confirmed = false;
    reject();
}

AutoRouteStartDialog::StartMode AutoRouteStartDialog::getStartMode() const
{
    return m_ownShipRadio->isChecked() ? FROM_OWNSHIP : MANUAL_SELECTION;
}

void AutoRouteStartDialog::getManualStartPosition(EcCoordinate& lat, EcCoordinate& lon) const
{
    lat = m_manualLat;
    lon = m_manualLon;
}

void AutoRouteStartDialog::setManualStartPosition(EcCoordinate lat, EcCoordinate lon)
{
    qDebug() << "[AutoRouteStartDialog] setManualStartPosition called:" << lat << lon;

    m_manualLat = lat;
    m_manualLon = lon;
    m_manualPositionSet = true;

    // Show dialog again and update labels
    qDebug() << "[AutoRouteStartDialog] Showing dialog again";
    show();
    updateLabels();

    emit startPositionSelected(lat, lon);

    qDebug() << "[AutoRouteStartDialog] Position set and dialog shown";
}

QString AutoRouteStartDialog::formatCoordinate(double lat, double lon) const
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

QString AutoRouteStartDialog::formatDistance(double distanceNM) const
{
    return QString("%1 NM").arg(distanceNM, 0, 'f', 2);
}

QString AutoRouteStartDialog::getDialogStyleSheet() const
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
            QPushButton {
                background-color: #3a3a3a;
                color: #e0e0e0;
                border: 1px solid #555;
                border-radius: 4px;
                padding: 5px 15px;
            }
            QPushButton:hover {
                background-color: #4a4a4a;
            }
            QPushButton:pressed {
                background-color: #2a2a2a;
            }
            QPushButton:default {
                background-color: #0d7377;
                font-weight: bold;
            }
            QPushButton:default:hover {
                background-color: #0e8a8f;
            }
            QPushButton:disabled {
                background-color: #2a2a2a;
                color: #666;
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
            QPushButton {
                border: 1px solid #ccc;
                border-radius: 4px;
                padding: 5px 15px;
            }
            QPushButton:default {
                background-color: #0d7377;
                color: white;
                font-weight: bold;
            }
            QPushButton:default:hover {
                background-color: #0e8a8f;
            }
        )";
    }
}
