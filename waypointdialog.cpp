#include "waypointdialog.h"
#include <QMessageBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QSpinBox>

WaypointDialog::WaypointDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Add Waypoint");
    setModal(true);
    resize(400, 350);
    
    setupUI();
    connectSignals();
}

void WaypointDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Create form layout
    QFormLayout *formLayout = new QFormLayout;
    
    // Coordinate inputs
    QGroupBox *coordGroup = new QGroupBox("Coordinates");
    QFormLayout *coordLayout = new QFormLayout(coordGroup);
    
    // Latitude input
    latitudeEdit = new QLineEdit;
    latitudeEdit->setPlaceholderText("e.g., -7.25694 (Decimal degrees)");
    latValidator = new QDoubleValidator(-90.0, 90.0, 8, this);
    latValidator->setNotation(QDoubleValidator::StandardNotation);
    latitudeEdit->setValidator(latValidator);
    coordLayout->addRow("Latitude:", latitudeEdit);
    
    // Longitude input
    longitudeEdit = new QLineEdit;
    longitudeEdit->setPlaceholderText("e.g., 112.75194 (Decimal degrees)");
    lonValidator = new QDoubleValidator(-180.0, 180.0, 8, this);
    lonValidator->setNotation(QDoubleValidator::StandardNotation);
    longitudeEdit->setValidator(lonValidator);
    coordLayout->addRow("Longitude:", longitudeEdit);
    
    mainLayout->addWidget(coordGroup);
    
    // Waypoint details
    QGroupBox *detailsGroup = new QGroupBox("Waypoint Details");
    QFormLayout *detailsLayout = new QFormLayout(detailsGroup);
    
    // Label input
    labelEdit = new QLineEdit;
    labelEdit->setPlaceholderText("Auto-generated if empty");
    detailsLayout->addRow("Label:", labelEdit);
    
    // Route selection
    routeComboBox = new QComboBox;
    routeComboBox->addItem("Single Waypoint", 0);
    routeComboBox->addItem("Route 1", 1);
    routeComboBox->addItem("Route 2", 2);
    routeComboBox->addItem("Route 3", 3);
    routeComboBox->addItem("Route 4", 4);
    routeComboBox->addItem("Route 5", 5);
    detailsLayout->addRow("Route:", routeComboBox);
    
    // Turning radius
    turningRadiusEdit = new QLineEdit;
    turningRadiusEdit->setText("0.5");
    turningRadiusEdit->setPlaceholderText("0.5");
    radiusValidator = new QDoubleValidator(0.0, 10.0, 2, this);
    turningRadiusEdit->setValidator(radiusValidator);
    detailsLayout->addRow("Turning Radius (NM):", turningRadiusEdit);
    
    // Remark input
    remarkEdit = new QTextEdit;
    remarkEdit->setMaximumHeight(60);
    remarkEdit->setPlaceholderText("Optional remarks...");
    detailsLayout->addRow("Remarks:", remarkEdit);
    
    mainLayout->addWidget(detailsGroup);
    
    // Status label
    statusLabel = new QLabel;
    statusLabel->setStyleSheet("QLabel { color: red; }");
    statusLabel->hide();
    mainLayout->addWidget(statusLabel);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    
    cancelButton = new QPushButton("Cancel");
    okButton = new QPushButton("Add Waypoint");
    okButton->setDefault(true);
    okButton->setEnabled(false); // Disabled until valid coordinates entered
    
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    
    mainLayout->addLayout(buttonLayout);
}

void WaypointDialog::connectSignals()
{
    connect(okButton, &QPushButton::clicked, this, &WaypointDialog::validateAndAccept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    connect(latitudeEdit, &QLineEdit::textChanged, this, &WaypointDialog::onCoordinateChanged);
    connect(longitudeEdit, &QLineEdit::textChanged, this, &WaypointDialog::onCoordinateChanged);
}

void WaypointDialog::onCoordinateChanged()
{
    bool isValid = validateCoordinates();
    okButton->setEnabled(isValid);
    
    if (isValid) {
        statusLabel->hide();
    }
}

bool WaypointDialog::validateCoordinates()
{
    bool latOk = false, lonOk = false;
    
    double lat = latitudeEdit->text().toDouble(&latOk);
    double lon = longitudeEdit->text().toDouble(&lonOk);
    
    if (!latOk || !lonOk) {
        statusLabel->setText("Please enter valid decimal coordinates");
        statusLabel->show();
        return false;
    }
    
    if (lat < -90.0 || lat > 90.0) {
        statusLabel->setText("Latitude must be between -90 and 90 degrees");
        statusLabel->show();
        return false;
    }
    
    if (lon < -180.0 || lon > 180.0) {
        statusLabel->setText("Longitude must be between -180 and 180 degrees");
        statusLabel->show();
        return false;
    }
    
    return true;
}

void WaypointDialog::validateAndAccept()
{
    if (validateCoordinates()) {
        accept();
    }
}

// Getters
double WaypointDialog::getLatitude() const
{
    return latitudeEdit->text().toDouble();
}

double WaypointDialog::getLongitude() const
{
    return longitudeEdit->text().toDouble();
}

QString WaypointDialog::getLabel() const
{
    return labelEdit->text().trimmed();
}

QString WaypointDialog::getRemark() const
{
    return remarkEdit->toPlainText().trimmed();
}

int WaypointDialog::getRouteId() const
{
    return routeComboBox->currentData().toInt();
}

double WaypointDialog::getTurningRadius() const
{
    bool ok;
    double radius = turningRadiusEdit->text().toDouble(&ok);
    return ok ? radius : 0.5; // Default to 0.5 if invalid
}

// Setters
void WaypointDialog::setLatitude(double lat)
{
    latitudeEdit->setText(QString::number(lat, 'f', 6));
}

void WaypointDialog::setLongitude(double lon)
{
    longitudeEdit->setText(QString::number(lon, 'f', 6));
}

void WaypointDialog::setLabel(const QString& label)
{
    labelEdit->setText(label);
}

void WaypointDialog::setRemark(const QString& remark)
{
    remarkEdit->setPlainText(remark);
}

void WaypointDialog::setRouteId(int routeId)
{
    int index = routeComboBox->findData(routeId);
    if (index >= 0) {
        routeComboBox->setCurrentIndex(index);
    }
}

void WaypointDialog::setTurningRadius(double radius)
{
    turningRadiusEdit->setText(QString::number(radius, 'f', 2));
}