#include "SettingsDialog.h"
#include "SettingsManager.h"

#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QSizePolicy>
#include <QTabWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGroupBox>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI() {
    setWindowTitle("Settings Manager");
    resize(400, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QTabWidget *tabWidget = new QTabWidget(this);

    // MOOSDB tab
    QWidget *moosTab = new QWidget;
    QFormLayout *moosLayout = new QFormLayout;
    moosIpLineEdit = new QLineEdit;
    moosPortLineEdit = new QLineEdit;
    moosLayout->addRow("MOOSDB IP:", moosIpLineEdit);
    //moosLayout->addRow("MOOSDB Port:", moosPortLineEdit);
    moosTab->setLayout(moosLayout);


    // --- Own Ship Tab ---
    QWidget *ownShipTab = new QWidget;
    QFormLayout *ownShipLayout = new QFormLayout;

    centeringCombo = new QComboBox;
    centeringCombo->addItem("Centered", "Centered");
    centeringCombo->addItem("Look Ahead", "LookAhead");
    centeringCombo->addItem("Manual Offset", "Manual");
    ownShipLayout->addRow("Centering Mode:", centeringCombo);

    orientationCombo = new QComboBox;
    orientationCombo->addItem("North Up", "NorthUp");
    orientationCombo->addItem("Head Up", "HeadUp");
    orientationCombo->addItem("Course Up", "CourseUp");
    ownShipLayout->addRow("Orientation Mode:", orientationCombo);

    headingLabel = new QLabel("Course-Up Heading:");
    headingSpin = new QSpinBox;
    headingSpin->setRange(0, 359);
    headingSpin->setSuffix("°");
    headingLabel->setVisible(false);
    headingSpin->setVisible(false);
    ownShipLayout->addRow(headingLabel, headingSpin);

    connect(orientationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
        bool isCourseUp = (orientationCombo->currentData().toString() == "CourseUp");
        headingLabel->setVisible(isCourseUp);
        headingSpin->setVisible(isCourseUp);
    });

    ownShipTab->setLayout(ownShipLayout);

    // --- AIS Tab ---
    QWidget *aisTab = new QWidget;
    QFormLayout *aisLayout = new QFormLayout;

    // AIS Source
    aisSourceCombo = new QComboBox;
    aisSourceCombo->addItems({"log", "moosdb", "ip"});
    aisLayout->addRow("AIS Source:", aisSourceCombo);

    // AIS IP
    ipLabel = new QLabel("AIS IP:");
    ipAisLineEdit = new QLineEdit;
    aisLayout->addRow(ipLabel, ipAisLineEdit);

    // AIS Log File
    logFileLabel = new QLabel("AIS Log File:");
    logFileLineEdit = new QLineEdit;
    logFileLineEdit->setPlaceholderText("Pilih file log...");
    QPushButton *logFileButton = new QPushButton("Browse...");
    QHBoxLayout *logInputLayout = new QHBoxLayout;
    logInputLayout->addWidget(logFileLineEdit);
    logInputLayout->addWidget(logFileButton);
    logInputLayout->setContentsMargins(0, 0, 0, 0); // biar rapat

    QWidget *logInputWidget = new QWidget;
    logInputWidget->setLayout(logInputLayout);
    aisLayout->addRow(logFileLabel, logInputWidget);

    aisTab->setLayout(aisLayout);

    // Browse button action
    connect(logFileButton, &QPushButton::clicked, this, [=]() {
        QString file = QFileDialog::getOpenFileName(this, "Select AIS Log File");
        if (!file.isEmpty()) {
            logFileLineEdit->setText(file);
        }
    });

    // Update visibility
    connect(aisSourceCombo, &QComboBox::currentTextChanged, this, &SettingsDialog::updateAisWidgetsVisibility);

    // Display tab
    QWidget *displayTab = new QWidget;
    QFormLayout *displayLayout = new QFormLayout;
    displayModeCombo = new QComboBox;
    displayModeCombo->addItems({"Day", "Dusk", "Night"});
    displayLayout->addRow("Display Mode:", displayModeCombo);
    displayTab->setLayout(displayLayout);

    // GuardZone tab
    QWidget *guardzoneTab = new QWidget;
    QVBoxLayout *guardzoneLayout = new QVBoxLayout;
    
    // Ship Type Filter Group
    QGroupBox *shipTypeGroup = new QGroupBox(tr("Ship Type Filter"));
    QVBoxLayout *shipTypeLayout = new QVBoxLayout;
    
    shipTypeButtonGroup = new QButtonGroup(this);
    
    QRadioButton *shipAllRadio = new QRadioButton(tr("All Ships"));
    QRadioButton *shipCargoRadio = new QRadioButton(tr("Cargo Ships"));
    QRadioButton *shipTankerRadio = new QRadioButton(tr("Tanker Ships"));
    QRadioButton *shipPassengerRadio = new QRadioButton(tr("Passenger Ships"));
    QRadioButton *shipFishingRadio = new QRadioButton(tr("Fishing Vessels"));
    QRadioButton *shipMilitaryRadio = new QRadioButton(tr("Military Vessels"));
    QRadioButton *shipPleasureRadio = new QRadioButton(tr("Pleasure Craft"));
    QRadioButton *shipOtherRadio = new QRadioButton(tr("Other Vessels"));
    
    shipTypeButtonGroup->addButton(shipAllRadio, 0);
    shipTypeButtonGroup->addButton(shipCargoRadio, 1);
    shipTypeButtonGroup->addButton(shipTankerRadio, 2);
    shipTypeButtonGroup->addButton(shipPassengerRadio, 3);
    shipTypeButtonGroup->addButton(shipFishingRadio, 4);
    shipTypeButtonGroup->addButton(shipMilitaryRadio, 5);
    shipTypeButtonGroup->addButton(shipPleasureRadio, 6);
    shipTypeButtonGroup->addButton(shipOtherRadio, 7);
    
    shipTypeLayout->addWidget(shipAllRadio);
    shipTypeLayout->addWidget(shipCargoRadio);
    shipTypeLayout->addWidget(shipTankerRadio);
    shipTypeLayout->addWidget(shipPassengerRadio);
    shipTypeLayout->addWidget(shipFishingRadio);
    shipTypeLayout->addWidget(shipMilitaryRadio);
    shipTypeLayout->addWidget(shipPleasureRadio);
    shipTypeLayout->addWidget(shipOtherRadio);
    shipTypeGroup->setLayout(shipTypeLayout);
    
    // Alert Direction Group
    QGroupBox *alertDirectionGroup = new QGroupBox(tr("Alert Direction"));
    QVBoxLayout *alertDirectionLayout = new QVBoxLayout;
    
    alertDirectionButtonGroup = new QButtonGroup(this);
    
    QRadioButton *alertBothRadio = new QRadioButton(tr("Alert In & Out"));
    QRadioButton *alertInRadio = new QRadioButton(tr("Alert In Only"));
    QRadioButton *alertOutRadio = new QRadioButton(tr("Alert Out Only"));
    
    alertDirectionButtonGroup->addButton(alertBothRadio, 0);
    alertDirectionButtonGroup->addButton(alertInRadio, 1);
    alertDirectionButtonGroup->addButton(alertOutRadio, 2);
    
    alertDirectionLayout->addWidget(alertBothRadio);
    alertDirectionLayout->addWidget(alertInRadio);
    alertDirectionLayout->addWidget(alertOutRadio);
    alertDirectionGroup->setLayout(alertDirectionLayout);
    
    guardzoneLayout->addWidget(shipTypeGroup);
    guardzoneLayout->addWidget(alertDirectionGroup);
    guardzoneLayout->addStretch();
    guardzoneTab->setLayout(guardzoneLayout);

    tabWidget->addTab(moosTab, "MOOSDB");
    tabWidget->addTab(ownShipTab, "Own Ship");
    tabWidget->addTab(aisTab, "AIS");
    tabWidget->addTab(displayTab, "Display");
    tabWidget->addTab(guardzoneTab, "GuardZone");

    mainLayout->addWidget(tabWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

void SettingsDialog::loadSettings() {
    QSettings settings("config.ini", QSettings::IniFormat);

    // MOOSDB
    moosIpLineEdit->setText(settings.value("MOOSDB/ip", "127.0.0.1").toString());
    moosPortLineEdit->setText(settings.value("MOOSDB/port", "9000").toString());

    // AIS
    QString aisSource = settings.value("AIS/source", "log").toString();
    int aisIndex = aisSourceCombo->findText(aisSource);
    if (aisIndex >= 0) {
        aisSourceCombo->setCurrentIndex(aisIndex);
    } else {
        aisSourceCombo->setCurrentIndex(0); // default to first if not found
    }

    ipAisLineEdit->setText(settings.value("AIS/ip", "").toString());
    logFileLineEdit->setText(settings.value("AIS/log_file", "").toString());

    // Perbarui visibilitas sesuai source
    updateAisWidgetsVisibility(aisSource);

    // Display
    QString displayMode = settings.value("Display/mode", "Day").toString();
    int displayIndex = displayModeCombo->findText(displayMode);
    if (displayIndex >= 0) {
        displayModeCombo->setCurrentIndex(displayIndex);
    } else {
        displayModeCombo->setCurrentIndex(0); // fallback default
    }

    // GuardZone
    int defaultShipType = settings.value("GuardZone/default_ship_type", 0).toInt();
    if (shipTypeButtonGroup->button(defaultShipType)) {
        shipTypeButtonGroup->button(defaultShipType)->setChecked(true);
    } else {
        shipTypeButtonGroup->button(0)->setChecked(true); // fallback to "All Ships"
    }

    int defaultAlertDirection = settings.value("GuardZone/default_alert_direction", 0).toInt();
    if (alertDirectionButtonGroup->button(defaultAlertDirection)) {
        alertDirectionButtonGroup->button(defaultAlertDirection)->setChecked(true);
    } else {
        alertDirectionButtonGroup->button(0)->setChecked(true); // fallback to "Alert In & Out"
    }

    // Own Ship
    QString ori = settings.value("OwnShip/orientation", "NorthUp").toString();
    QString cent = settings.value("OwnShip/centering", "Centered").toString();
    int heading = settings.value("OwnShip/course_heading", 0).toInt();

    int oriIndex = orientationCombo->findData(ori);
    if (oriIndex >= 0) orientationCombo->setCurrentIndex(oriIndex);
    else orientationCombo->setCurrentIndex(0);

    int centIndex = centeringCombo->findData(cent);
    if (centIndex >= 0) centeringCombo->setCurrentIndex(centIndex);
    else centeringCombo->setCurrentIndex(0);

    headingSpin->setValue(heading);
    bool isCourseUp = (ori == "CourseUp");
    headingLabel->setVisible(isCourseUp);
    headingSpin->setVisible(isCourseUp);
}

void SettingsDialog::saveSettings() {
    QSettings settings("config.ini", QSettings::IniFormat);

    // MOOSDB
    settings.setValue("MOOSDB/ip", moosIpLineEdit->text());
    settings.setValue("MOOSDB/port", moosPortLineEdit->text());

    // AIS
    settings.setValue("AIS/source", aisSourceCombo->currentText());
    settings.setValue("AIS/ip", ipAisLineEdit->text());
    settings.setValue("AIS/log_file", logFileLineEdit->text());

    // Display
    settings.setValue("Display/mode", displayModeCombo->currentText());

    // GuardZone
    settings.setValue("GuardZone/default_ship_type", shipTypeButtonGroup->checkedId());
    settings.setValue("GuardZone/default_alert_direction", alertDirectionButtonGroup->checkedId());

    // Own Ship
    settings.setValue("OwnShip/orientation", orientationCombo->currentData().toString());
    settings.setValue("OwnShip/centering", centeringCombo->currentData().toString());

    if (orientationCombo->currentData().toString() == "CourseUp") {
        settings.setValue("OwnShip/course_heading", headingSpin->value());
    } else {
        settings.remove("OwnShip/course_heading"); // bersihkan jika tidak relevan
    }
}

void SettingsDialog::updateAisWidgetsVisibility(const QString &text) {
    bool isIp = (text == "ip");
    bool isLog = (text == "log");

    ipLabel->setVisible(isIp);
    ipAisLineEdit->setVisible(isIp);

    logFileLabel->setVisible(isLog);
    logFileLineEdit->parentWidget()->setVisible(isLog); // parentWidget karena ada layout dalam widget
}

// Fungsi static atau anggota kelas
SettingsData SettingsDialog::loadSettingsFromFile(const QString &filePath) {
    SettingsData data;

    QSettings settings(filePath, QSettings::IniFormat);

    // MOOSDB
    data.moosIp = settings.value("MOOSDB/ip", "127.0.0.1").toString();
    //data.moosPort = settings.value("MOOSDB/port", "9000").toString();

    // AIS
    data.aisSource = settings.value("AIS/source", "log").toString();
    data.aisIp = settings.value("AIS/ip", "").toString();
    data.aisLogFile = settings.value("AIS/log_file", "").toString();

    // Guardzone
    data.displayMode = settings.value("Display/mode", "Day").toString();
    data.defaultShipTypeFilter = settings.value("GuardZone/default_ship_type", 0).toInt();
    data.defaultAlertDirection = settings.value("GuardZone/default_alert_direction", 0).toInt();

    // Own Ship
    data.orientationMode = orientation(settings.value("OwnShip/orientation", "NorthUp").toString());
    data.centeringMode = centering(settings.value("OwnShip/centering", "Centered").toString());
    data.courseUpHeading = settings.value("OwnShip/course_heading", 0).toInt();

    return data;
}

void SettingsDialog::accept() {
    SettingsData data;

    // MOOSDB
    data.moosIp = moosIpLineEdit->text();
    //data.moosPort = moosPortLineEdit->text();

    // AIS
    data.aisSource = aisSourceCombo->currentText();
    data.aisIp = ipAisLineEdit->text();
    data.aisLogFile = logFileLineEdit->text();

    // Display
    data.displayMode = displayModeCombo->currentText();

    // Guardzone
    data.defaultShipTypeFilter = shipTypeButtonGroup->checkedId();
    data.defaultAlertDirection = alertDirectionButtonGroup->checkedId();

    // Own Ship settings
    data.orientationMode = orientation(orientationCombo->currentData().toString());
    data.centeringMode = centering(centeringCombo->currentData().toString());
    data.courseUpHeading = (data.orientationMode == EcWidget::CourseUp) ? headingSpin->value() : -1;

    SettingsManager::instance().save(data);

    QDialog::accept();
}

EcWidget::DisplayOrientationMode SettingsDialog::orientation(const QString &str) {
    if (str == "HeadUp") return EcWidget::HeadUp;
    if (str == "CourseUp") return EcWidget::CourseUp;
    return EcWidget::NorthUp;
}

EcWidget::OSCenteringMode SettingsDialog::centering(const QString &str) {
    if (str == "Centered") return EcWidget::Centered;
    if (str == "LookAhead") return EcWidget::LookAhead;
    return EcWidget::Manual;
}
