#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "appconfig.h"
#include "mainwindow.h"

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
#include <QCheckBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QDir>
#include <QCoreApplication>
#include <QTimer>
#include <QTableWidget>
#include <QHeaderView>

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
    moosIpLineEdit->setDisabled(false);
    moosPortLineEdit = new QLineEdit;
    moosLayout->addRow("MOOSDB IP:", moosIpLineEdit);



    //moosLayout->addRow("MOOSDB Port:", moosPortLineEdit);

    moosTab->setLayout(moosLayout);


    // --- Own Ship Tab ---
    QWidget *ownShipTab = new QWidget;
    QFormLayout *ownShipLayout = new QFormLayout;




    // --- Ship Dimensions Tab ---
    QWidget *shipDimensionsTab = new QWidget;
    QVBoxLayout *shipDimensionsLayout = new QVBoxLayout(shipDimensionsTab);

    // Ship Dimensions Group
    QGroupBox *dimensionsGroup = new QGroupBox(tr("Vessel Dimensions"));
    QFormLayout *dimensionsForm = new QFormLayout(dimensionsGroup);
    shipLengthSpin = new QDoubleSpinBox;
    shipLengthSpin->setRange(1.0, 2000.0); shipLengthSpin->setSuffix(" m");
    shipBeamSpin = new QDoubleSpinBox;
    shipBeamSpin->setRange(1.0, 200.0); shipBeamSpin->setSuffix(" m");
    shipHeightSpin = new QDoubleSpinBox;
    shipHeightSpin->setRange(1.0, 200.0); shipHeightSpin->setSuffix(" m");
    dimensionsForm->addRow(tr("Overall Length (meters):"), shipLengthSpin);
    dimensionsForm->addRow(tr("Beam (meters):"), shipBeamSpin);
    dimensionsForm->addRow(tr("Overall Height (meters):"), shipHeightSpin);

    // GPS Configuration Group
    QGroupBox *gpsGroup = new QGroupBox(tr("GPS Antenna Positions"));
    QVBoxLayout *gpsLayout = new QVBoxLayout(gpsGroup);
    gpsTableWidget = new QTableWidget;
    gpsTableWidget->setColumnCount(3);
    gpsTableWidget->setHorizontalHeaderLabels({"Name", "Offset X (Centerline)", "Offset Y (Bow)"});
    gpsTableWidget->horizontalHeader()->setStretchLastSection(true);
    QHBoxLayout *gpsButtons = new QHBoxLayout;
    QPushButton *addGpsButton = new QPushButton(tr("Add GPS"));
    QPushButton *removeGpsButton = new QPushButton(tr("Remove Selected"));
    gpsButtons->addStretch();
    gpsButtons->addWidget(addGpsButton);
    gpsButtons->addWidget(removeGpsButton);
    
    QFormLayout* primaryGpsForm = new QFormLayout;
    primaryGpsCombo = new QComboBox;
    primaryGpsForm->addRow(tr("Primary Reference (CCRP):"), primaryGpsCombo);

    gpsLayout->addLayout(primaryGpsForm);
    gpsLayout->addWidget(gpsTableWidget);
    gpsLayout->addLayout(gpsButtons);

    shipDimensionsLayout->addWidget(dimensionsGroup);
    shipDimensionsLayout->addWidget(gpsGroup);

    centeringCombo = new QComboBox;
    centeringCombo->addItem("Auto Recenter", "AutoRecenter");
    centeringCombo->addItem("Centered", "Centered");
    centeringCombo->addItem("Look Ahead", "LookAhead");
    centeringCombo->addItem("Manual Offset", "Manual");
    ownShipLayout->addRow("Default Centering:", centeringCombo);

    orientationCombo = new QComboBox;
    orientationCombo->addItem("North Up", "NorthUp");
    orientationCombo->addItem("Head Up", "HeadUp");
    orientationCombo->addItem("Course Up", "CourseUp");
    ownShipLayout->addRow("Default Orientation:", orientationCombo);

    // SEPARATOR
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    if (AppConfig::isLight()){
        line->setStyleSheet("color: gray;");
    }
    else {
        line->setStyleSheet("color: #444444;");
    }

    ownShipLayout->addRow(line);

    // NON DEFAULT DATA
    headingLabel = new QLabel("Course-Up Heading:");
    headingSpin = new QSpinBox;
    headingSpin->setRange(0, 359);
    headingSpin->setSuffix("°");
    // headingLabel->setVisible(false);
    // headingSpin->setVisible(false);
    ownShipLayout->addRow(headingLabel, headingSpin);

    // connect(orientationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
    //     bool isCourseUp = (orientationCombo->currentData().toString() == "CourseUp");
    //     headingLabel->setVisible(isCourseUp);
    //     headingSpin->setVisible(isCourseUp);
    // });

    if (AppConfig::isDevelopment()){
        // --- Navigation Safety Tab ---
        QWidget *safetyTab = new QWidget;
        QFormLayout *safetyLayout = new QFormLayout;

        // Ship Draft (meters)
        shipDraftSpin = new QDoubleSpinBox;
        shipDraftSpin->setRange(0.00, 30.00);
        shipDraftSpin->setDecimals(2);
        shipDraftSpin->setSingleStep(0.10);
        shipDraftSpin->setSuffix(" m");
        safetyLayout->addRow("Ship Draft:", shipDraftSpin);

        // UKC thresholds (meters)
        ukcDangerSpin = new QDoubleSpinBox;
        ukcDangerSpin->setRange(0.00, 10.00);
        ukcDangerSpin->setDecimals(2);
        ukcDangerSpin->setSingleStep(0.10);
        ukcDangerSpin->setSuffix(" m");
        safetyLayout->addRow("UKC Danger Margin:", ukcDangerSpin);

        ukcWarningSpin = new QDoubleSpinBox;
        ukcWarningSpin->setRange(0.00, 20.00);
        ukcWarningSpin->setDecimals(2);
        ukcWarningSpin->setSingleStep(0.10);
        ukcWarningSpin->setSuffix(" m");
        safetyLayout->addRow("UKC Warning Margin:", ukcWarningSpin);

        // Info notice for auto-adjust
        ukcNoticeLabel = new QLabel(tr("UKC Warning disesuaikan agar tidak lebih kecil dari Danger"));
        QFont f = ukcNoticeLabel->font();
        f.setItalic(true);
        ukcNoticeLabel->setFont(f);
        ukcNoticeLabel->setStyleSheet("color: #888;");
        ukcNoticeLabel->setVisible(false);
        safetyLayout->addRow("", ukcNoticeLabel);

        ukcNoticeTimer = new QTimer(this);
        ukcNoticeTimer->setSingleShot(true);
        connect(ukcNoticeTimer, &QTimer::timeout, this, [=]() {
            ukcNoticeLabel->setVisible(false);
        });

        safetyTab->setLayout(safetyLayout);

        // Validation: ensure Warning ≥ Danger at all times
        auto enforceUkcOrder = [this]() {
            double danger = ukcDangerSpin->value();
            double warning = ukcWarningSpin->value();
            if (warning < danger) {
                ukcWarningSpin->setValue(danger);
                // Show notice for 2 seconds
                ukcNoticeLabel->setVisible(true);
                ukcNoticeTimer->start(2000);
            }
            // Keep warning minimum tied to danger
            ukcWarningSpin->setMinimum(danger);
        };
        // Connect value changes to enforcement
        connect(ukcDangerSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double){ enforceUkcOrder(); });
        connect(ukcWarningSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double){ enforceUkcOrder(); });

        tabWidget->addTab(safetyTab, "Safety");
    }

    trailCombo = new QComboBox;
    trailCombo->addItem("Every Update", 0);
    trailCombo->addItem("Fixed Interval", 1);
    trailCombo->addItem("Fixed Distance", 2);
    ownShipLayout->addRow("Track Line Mode:", trailCombo);

    trailSpin = new QSpinBox;
    trailLabel = new QLabel("Interval:");
    trailSpin->setRange(1, 300);
    trailSpin->setSuffix(" minute(s)");
    trailLabel->setVisible(false);
    trailSpin->setVisible(false);
    ownShipLayout->addRow(trailLabel, trailSpin);

    connect(trailCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
        bool isMode = (trailCombo->currentData() == 1);
        trailLabel->setVisible(isMode);
        trailSpin->setVisible(isMode);
    });

    trailSpinDistance = new QDoubleSpinBox;
    trailSpinDistance->setRange(0.01, 10.0);
    trailSpinDistance->setSuffix(" NM");
    trailSpinDistance->setDecimals(2);
    trailSpinDistance->setSingleStep(0.01);
    trailSpinDistance->setSuffix(" NM");
    trailSpinDistance->setVisible(false);

    trailLabelDistance = new QLabel("Distance:");
    trailLabelDistance->setVisible(false);

    ownShipLayout->addRow(trailLabelDistance, trailSpinDistance);

    connect(trailCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
        bool isMode = (trailCombo->currentData() == 2);
        trailLabelDistance->setVisible(isMode);
        trailSpinDistance->setVisible(isMode);
    });

    ownShipTab->setLayout(ownShipLayout);

    if (AppConfig::isDevelopment()){
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

        tabWidget->addTab(aisTab, "AIS");
    }

    // Display tab
    QWidget *displayTab = new QWidget;
    QFormLayout *displayLayout = new QFormLayout;
    displayModeCombo = new QComboBox;
    displayModeCombo->addItems({"Day", "Dusk", "Night"});
    displayLayout->addRow("Default Chart Theme:", displayModeCombo);
    displayTab->setLayout(displayLayout);

    themeModeCombo = new QComboBox;
    themeModeCombo->addItems({"Light", "Dim", "Dark"});
    displayLayout->addRow("Default UI Theme:", themeModeCombo);
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

    // --- Alert Tab ---
    QWidget *alertTab = new QWidget;
    QVBoxLayout *alertLayout = new QVBoxLayout;
    
    // Visual Alert Group
    QGroupBox *visualAlertGroup = new QGroupBox(tr("Visual Alert Settings"));
    QVBoxLayout *visualAlertLayout = new QVBoxLayout;
    
    visualFlashingCheckBox = new QCheckBox(tr("Enable Flashing Alert"));
    visualFlashingCheckBox->setChecked(true); // Default enabled
    visualAlertLayout->addWidget(visualFlashingCheckBox);
    visualAlertGroup->setLayout(visualAlertLayout);
    
    // Sound Alert Group
    QGroupBox *soundAlertGroup = new QGroupBox(tr("Sound Alert Settings"));
    QFormLayout *soundAlertLayout = new QFormLayout;
    
    soundAlarmEnabledCheckBox = new QCheckBox(tr("Enable Sound Alarm"));
    soundAlarmEnabledCheckBox->setChecked(true); // Default enabled
    soundAlertLayout->addRow("Sound:", soundAlarmEnabledCheckBox);
    
    soundAlarmCombo = new QComboBox;
    // Populate with available sound files
    QDir soundDir("alarm_sound");
    QStringList soundFiles = soundDir.entryList(QStringList() << "*.wav" << "*.mp3", QDir::Files);
    if (soundFiles.isEmpty()) {
        soundFiles << "critical-alarm.wav" << "vintage-alarm.wav" << "clasic-alarm.wav" << "street-public.wav";
    }
    soundAlarmCombo->addItems(soundFiles);
    soundAlertLayout->addRow("Alarm Sound:", soundAlarmCombo);
    
    soundVolumeSlider = new QSlider(Qt::Horizontal);
    soundVolumeSlider->setRange(0, 100);
    soundVolumeSlider->setValue(80); // Default 80%
    soundVolumeLabel = new QLabel("80%");
    
    QHBoxLayout *volumeLayout = new QHBoxLayout;
    volumeLayout->addWidget(soundVolumeSlider);
    volumeLayout->addWidget(soundVolumeLabel);
    
    QWidget *volumeWidget = new QWidget;
    volumeWidget->setLayout(volumeLayout);
    soundAlertLayout->addRow("Volume:", volumeWidget);
    
    // Connect volume slider to label update
    connect(soundVolumeSlider, &QSlider::valueChanged, this, [=](int value) {
        soundVolumeLabel->setText(QString("%1%").arg(value));
    });
    
    // Connect sound enabled checkbox to disable/enable sound controls
    connect(soundAlarmEnabledCheckBox, &QCheckBox::toggled, this, [=](bool enabled) {
        soundAlarmCombo->setEnabled(enabled);
        soundVolumeSlider->setEnabled(enabled);
        soundVolumeLabel->setEnabled(enabled);
    });
    
    soundAlertGroup->setLayout(soundAlertLayout);
    
    alertLayout->addWidget(visualAlertGroup);
    alertLayout->addWidget(soundAlertGroup);
    alertLayout->addStretch();
    alertTab->setLayout(alertLayout);

    // CPA/TCPA
    QWidget *cpatcpaTab = new QWidget;
    QFormLayout *cpatcpaLayout = new QFormLayout;

    cpaSpin = new QDoubleSpinBox;
    cpaSpin->setRange(0.1, 5.0);
    cpaSpin->setSuffix(" NM");
    cpaSpin->setDecimals(1);
    cpaSpin->setSingleStep(0.1);
    cpaSpin->setSuffix(" NM");

    cpatcpaLayout->addRow("CPA Threshold:", cpaSpin);
    cpatcpaTab->setLayout(cpatcpaLayout);


    tcpaSpin = new QDoubleSpinBox;
    tcpaSpin->setRange(1, 20);
    tcpaSpin->setSuffix(" NM");
    tcpaSpin->setDecimals(0);
    tcpaSpin->setSingleStep(1);
    tcpaSpin->setSuffix(" min");

    cpatcpaLayout->addRow("TCPA Threshold:", tcpaSpin);
    cpatcpaTab->setLayout(cpatcpaLayout);


    tabWidget->addTab(moosTab, "MOOSDB");
    tabWidget->addTab(ownShipTab, "Own Ship");
    tabWidget->addTab(shipDimensionsTab, "Ship Dimensions");
    tabWidget->addTab(displayTab, "Display");
    tabWidget->addTab(cpatcpaTab, "CPA/TCPA");

    if (AppConfig::isDevelopment()){
        tabWidget->addTab(guardzoneTab, "GuardZone");
        tabWidget->addTab(alertTab, "Alert");
    }

    mainLayout->addWidget(tabWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    // GPS UI Connections
    connect(addGpsButton, &QPushButton::clicked, this, &SettingsDialog::onAddGpsRow);
    connect(removeGpsButton, &QPushButton::clicked, this, &SettingsDialog::onRemoveGpsRow);
    connect(gpsTableWidget, &QTableWidget::itemChanged, this, &SettingsDialog::updatePrimaryGpsCombo);
}

void SettingsDialog::loadSettings() {
    //QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QString configPath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC" + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    // MOOSDB
    moosIpLineEdit->setText(settings.value("MOOSDB/ip", "127.0.0.1").toString());
    moosPortLineEdit->setText(settings.value("MOOSDB/port", "9000").toString());

    // AIS
    if (AppConfig::isDevelopment()){
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
    }

    // Display
    QString displayMode = settings.value("Display/mode", "Day").toString();
    int displayIndex = displayModeCombo->findText(displayMode);
    if (displayIndex >= 0) {
        displayModeCombo->setCurrentIndex(displayIndex);
    } else {
        displayModeCombo->setCurrentIndex(0); // fallback default
    }

    QString themeMode = settings.value("Display/theme", "Dark").toString();
    int themeIndex = themeModeCombo->findText(themeMode);
    if (themeIndex >= 0) {
        themeModeCombo->setCurrentIndex(themeIndex);
    } else {
        themeModeCombo->setCurrentIndex(0); // fallback default
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
    QString cent = settings.value("OwnShip/centering", "AutoRecenter").toString();
    int heading = settings.value("OwnShip/course_heading", 0).toInt();
    int trailMode = settings.value("OwnShip/mode", 2).toInt();
    int trailMinute = settings.value("OwnShip/interval", 1).toInt();
    double trailDistance = settings.value("OwnShip/distance", 0.01).toDouble();

    int oriIndex = orientationCombo->findData(ori);
    if (oriIndex >= 0) orientationCombo->setCurrentIndex(oriIndex);
    else orientationCombo->setCurrentIndex(0);

    int centIndex = centeringCombo->findData(cent);
    if (centIndex >= 0) centeringCombo->setCurrentIndex(centIndex);
    else centeringCombo->setCurrentIndex(0);

    int modeIndex = trailCombo->findData(trailMode);
    if (modeIndex >= 0) trailCombo->setCurrentIndex(modeIndex);
    else trailCombo->setCurrentIndex(0);

    headingSpin->setValue(heading);
    // bool isCourseUp = (ori == "CourseUp");
    // headingLabel->setVisible(isCourseUp);
    // headingSpin->setVisible(isCourseUp);

    trailSpin->setValue(trailMinute);
    bool isTime = (trailMode == 1);
    trailLabel->setVisible(isTime);
    trailSpin->setVisible(isTime);

    trailSpinDistance->setValue(trailDistance);
    bool isDistance = (trailMode == 2);
    trailLabelDistance->setVisible(isDistance);
    trailSpinDistance->setVisible(isDistance);

    // Alert Settings
    visualFlashingCheckBox->setChecked(settings.value("Alert/visual_flashing", true).toBool());
    soundAlarmEnabledCheckBox->setChecked(settings.value("Alert/sound_enabled", true).toBool());
    
    QString soundFile = settings.value("Alert/sound_file", "critical-alarm.wav").toString();
    int soundIndex = soundAlarmCombo->findText(soundFile);
    if (soundIndex >= 0) {
        soundAlarmCombo->setCurrentIndex(soundIndex);
    } else {
        soundAlarmCombo->setCurrentIndex(0);
    }
    
    int volume = settings.value("Alert/sound_volume", 80).toInt();
    soundVolumeSlider->setValue(volume);
    soundVolumeLabel->setText(QString("%1%").arg(volume));
    
    // Update sound controls state based on enabled checkbox
    bool soundEnabled = soundAlarmEnabledCheckBox->isChecked();
    soundAlarmCombo->setEnabled(soundEnabled);
    soundVolumeSlider->setEnabled(soundEnabled);
    soundVolumeLabel->setEnabled(soundEnabled);

    if(AppConfig::isDevelopment()){
        // Navigation safety
        shipDraftSpin->setValue(settings.value("OwnShip/ship_draft", 2.5).toDouble());
        ukcDangerSpin->setValue(settings.value("OwnShip/ukc_danger", 0.5).toDouble());
        ukcWarningSpin->setValue(settings.value("OwnShip/ukc_warning", 2.0).toDouble());
        // Enforce relation after loading
        {
            double danger = ukcDangerSpin->value();
            if (ukcWarningSpin->value() < danger) {
                ukcWarningSpin->setValue(danger);
            }
            ukcWarningSpin->setMinimum(danger);
        }
    }

    // CPA/TCPA
    cpaSpin->setValue(settings.value("CPA-TCPA/cpa_threshold", 0.2).toDouble());
    tcpaSpin->setValue(settings.value("CPA-TCPA/tcpa_threshold", 1).toDouble());

    // Ship Dimensions
    shipLengthSpin->setValue(settings.value("ShipDimensions/length", 170.0).toDouble());
    shipBeamSpin->setValue(settings.value("ShipDimensions/beam", 13.0).toDouble());
    shipHeightSpin->setValue(settings.value("ShipDimensions/height", 25.0).toDouble());

    // GPS Positions
    gpsTableWidget->setRowCount(0); // Clear table before loading
    int gpsCount = settings.beginReadArray("GPSPositions");
    for (int i = 0; i < gpsCount; ++i) {
        settings.setArrayIndex(i);
        onAddGpsRow(); // Add a new row to the table
        QTableWidgetItem *nameItem = gpsTableWidget->item(i, 0);
        QTableWidgetItem *xItem = gpsTableWidget->item(i, 1);
        QTableWidgetItem *yItem = gpsTableWidget->item(i, 2);

        if(nameItem) nameItem->setText(settings.value("name").toString());
        if(xItem) xItem->setText(settings.value("offsetX").toString());
        if(yItem) yItem->setText(settings.value("offsetY").toString());
    }
    settings.endArray();

    updatePrimaryGpsCombo();
    int primaryIndex = settings.value("ShipDimensions/primaryGpsIndex", 0).toInt();
    if (primaryIndex < primaryGpsCombo->count()) {
        primaryGpsCombo->setCurrentIndex(primaryIndex);
    }


}

void SettingsDialog::saveSettings() {
    //QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QString configPath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC" + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    // MOOSDB
    settings.setValue("MOOSDB/ip", moosIpLineEdit->text());
    settings.setValue("MOOSDB/port", moosPortLineEdit->text());

    // AIS
    if (AppConfig::isDevelopment()){
        settings.setValue("AIS/source", aisSourceCombo->currentText());
        settings.setValue("AIS/ip", ipAisLineEdit->text());
        settings.setValue("AIS/log_file", logFileLineEdit->text());
    }

    // Display
    settings.setValue("Display/mode", displayModeCombo->currentText());
    settings.setValue("Display/theme", themeModeCombo->currentText());

    // GuardZone
    settings.setValue("GuardZone/default_ship_type", shipTypeButtonGroup->checkedId());
    settings.setValue("GuardZone/default_alert_direction", alertDirectionButtonGroup->checkedId());

    // Own Ship
    settings.setValue("OwnShip/orientation", orientationCombo->currentData().toString());
    settings.setValue("OwnShip/centering", centeringCombo->currentData().toString());

    settings.setValue("OwnShip/course_heading", headingSpin->value());
    // if (orientationCombo->currentData().toString() == "CourseUp") {
    //     settings.setValue("OwnShip/course_heading", headingSpin->value());
    // } else {
    //     settings.remove("OwnShip/course_heading"); // bersihkan jika tidak relevan
    // }

    settings.setValue("OwnShip/mode", trailCombo->currentData().toString());
    settings.setValue("OwnShip/interval", trailSpin->value());
    settings.setValue("OwnShip/distance", trailSpinDistance->value());

    if(AppConfig::isDevelopment()){
        // Navigation safety
        settings.setValue("OwnShip/ship_draft", shipDraftSpin->value());
        settings.setValue("OwnShip/ukc_danger", ukcDangerSpin->value());
        settings.setValue("OwnShip/ukc_warning", ukcWarningSpin->value());
    }

    // Alert Settings
    settings.setValue("Alert/visual_flashing", visualFlashingCheckBox->isChecked());
    settings.setValue("Alert/sound_enabled", soundAlarmEnabledCheckBox->isChecked());
    settings.setValue("Alert/sound_file", soundAlarmCombo->currentText());
    settings.setValue("Alert/sound_volume", soundVolumeSlider->value());

    // CPA/TCPA
    settings.setValue("CPA-TCPA/cpa_threshold", cpaSpin->value());
    settings.setValue("CPA-TCPA/tcpa_threshold", tcpaSpin->value());
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

    //QString configPath = QCoreApplication::applicationDirPath() + filePath;
    QString configPath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC" + filePath;
    QSettings settings(configPath, QSettings::IniFormat);


    // MOOSDB
    data.moosIp = settings.value("MOOSDB/ip", "127.0.0.1").toString();
    //data.moosPort = settings.value("MOOSDB/port", "9000").toString();

    // AIS
    if (AppConfig::isDevelopment()){
        data.aisSource = settings.value("AIS/source", "log").toString();
        data.aisIp = settings.value("AIS/ip", "").toString();
        data.aisLogFile = settings.value("AIS/log_file", "").toString();
    }

    // Display
    data.displayMode = settings.value("Display/mode", "Day").toString();
    data.themeMode = theme(settings.value("Display/theme", "Dark").toString());

    // Guardzone
    data.defaultShipTypeFilter = settings.value("GuardZone/default_ship_type", 0).toInt();
    data.defaultAlertDirection = settings.value("GuardZone/default_alert_direction", 0).toInt();

    // Own Ship
    data.orientationMode = orientation(settings.value("OwnShip/orientation", "NorthUp").toString());
    data.centeringMode = centering(settings.value("OwnShip/centering", "AutoRecenter").toString());
    data.courseUpHeading = settings.value("OwnShip/course_heading", 0).toInt();
    data.trailMode = settings.value("OwnShip/mode", 2).toInt();
    data.trailMinute = settings.value("OwnShip/interval", 1).toInt();
    data.trailDistance = settings.value("OwnShip/distance", 0.01).toDouble();

    if(AppConfig::isDevelopment()){
        data.shipDraftMeters = settings.value("OwnShip/ship_draft", 2.5).toDouble();
        data.ukcDangerMeters = settings.value("OwnShip/ukc_danger", 0.5).toDouble();
        data.ukcWarningMeters = settings.value("OwnShip/ukc_warning", 2.0).toDouble();
    }

    // Alert Settings
    data.visualFlashingEnabled = settings.value("Alert/visual_flashing", true).toBool();
    data.soundAlarmEnabled = settings.value("Alert/sound_enabled", true).toBool();
    data.soundAlarmFile = settings.value("Alert/sound_file", "critical-alarm.wav").toString();
    data.soundAlarmVolume = settings.value("Alert/sound_volume", 80).toInt();

    // CPA/TCPA
    data.cpaThreshold = settings.value("CPA-TCPA/cpa_threshold", 0.2).toDouble();
    data.tcpaThreshold = settings.value("CPA-TCPA/tcpa_threshold", 1).toDouble();

    return data;
}

void SettingsDialog::accept() {
    SettingsData data;

    // MOOSDB
    data.moosIp = moosIpLineEdit->text();
    //data.moosPort = moosPortLineEdit->text();

    // AIS
    if (AppConfig::isDevelopment()){
        data.aisSource = aisSourceCombo->currentText();
        data.aisIp = ipAisLineEdit->text();
        data.aisLogFile = logFileLineEdit->text();
    }

    // Display
    data.displayMode = displayModeCombo->currentText();
    data.themeMode = theme(themeModeCombo->currentData().toString());

    // Guardzone
    data.defaultShipTypeFilter = shipTypeButtonGroup->checkedId();
    data.defaultAlertDirection = alertDirectionButtonGroup->checkedId();

    // Own Ship settings
    data.orientationMode = orientation(orientationCombo->currentData().toString());
    data.centeringMode = centering(centeringCombo->currentData().toString());
    //data.courseUpHeading = (data.orientationMode == EcWidget::CourseUp) ? headingSpin->value() : -1;
    data.courseUpHeading = headingSpin->value();
    data.trailMode = trailCombo->currentData().toInt();
    data.trailMinute = trailSpin->value();
    data.trailDistance = trailSpinDistance->value();

    if(AppConfig::isDevelopment()){
        data.shipDraftMeters = shipDraftSpin->value();
        // Final validation: force Warning ≥ Danger
        double dangerVal = ukcDangerSpin->value();
        double warningVal = ukcWarningSpin->value();
        if (warningVal < dangerVal) {
            warningVal = dangerVal;
        }
        data.ukcDangerMeters = dangerVal;
        data.ukcWarningMeters = warningVal;
    }

    // Alert settings
    data.visualFlashingEnabled = visualFlashingCheckBox->isChecked();
    data.soundAlarmEnabled = soundAlarmEnabledCheckBox->isChecked();
    data.soundAlarmFile = soundAlarmCombo->currentText();
    data.soundAlarmVolume = soundVolumeSlider->value();

    // CPA/TCPA
    data.cpaThreshold = cpaSpin->value();
    data.tcpaThreshold = tcpaSpin->value();

    // Ship Dimensions
    data.shipLength = shipLengthSpin->value();
    data.shipBeam = shipBeamSpin->value();
    data.shipHeight = shipHeightSpin->value();

    // GPS Positions
    data.gpsPositions.clear();
    for (int i = 0; i < gpsTableWidget->rowCount(); ++i) {
        GpsPosition pos;
        pos.name = gpsTableWidget->item(i, 0) ? gpsTableWidget->item(i, 0)->text() : "";
        pos.offsetX = gpsTableWidget->item(i, 1) ? gpsTableWidget->item(i, 1)->text().toDouble() : 0.0;
        pos.offsetY = gpsTableWidget->item(i, 2) ? gpsTableWidget->item(i, 2)->text().toDouble() : 0.0;
        data.gpsPositions.append(pos);
    }
    data.primaryGpsIndex = primaryGpsCombo->currentIndex();

    // Ship Dimensions
    data.shipLength = shipLengthSpin->value();
    data.shipBeam = shipBeamSpin->value();
    data.shipHeight = shipHeightSpin->value();

    // GPS Positions
    data.gpsPositions.clear();
    for (int i = 0; i < gpsTableWidget->rowCount(); ++i) {
        GpsPosition pos;
        pos.name = gpsTableWidget->item(i, 0) ? gpsTableWidget->item(i, 0)->text() : "";
        pos.offsetX = gpsTableWidget->item(i, 1) ? gpsTableWidget->item(i, 1)->text().toDouble() : 0.0;
        pos.offsetY = gpsTableWidget->item(i, 2) ? gpsTableWidget->item(i, 2)->text().toDouble() : 0.0;
        data.gpsPositions.append(pos);
    }
    data.primaryGpsIndex = primaryGpsCombo->currentIndex();

    SettingsManager::instance().save(data);

    // Find the EcWidget instance and apply the new dimensions
    if (parentWidget()) {
        MainWindow* mainWindow = qobject_cast<MainWindow*>(parentWidget());
        if (mainWindow) {
            EcWidget* ecWidget = mainWindow->findChild<EcWidget*>();
            if (ecWidget) {
                ecWidget->applyShipDimensions();
            }
        }
    }

    QDialog::accept();

    // EMIT CLOSED
    emit dialogClosed();
}

void SettingsDialog::reject() {
    QDialog::reject();
    emit dialogClosed();
    qDebug() << "SettingsDialog rejected";
}

EcWidget::DisplayOrientationMode SettingsDialog::orientation(const QString &str) {
    if (str == "HeadUp") return EcWidget::HeadUp;
    if (str == "CourseUp") return EcWidget::CourseUp;
    return EcWidget::NorthUp;
}

EcWidget::OSCenteringMode SettingsDialog::centering(const QString &str) {
    if (str == "Centered") return EcWidget::Centered;
    if (str == "LookAhead") return EcWidget::LookAhead;
    if (str == "AutoRecenter") return EcWidget::AutoRecenter;
    return EcWidget::Manual;
}

AppConfig::AppTheme SettingsDialog::theme(const QString &str){
    if (str == "Dark") return AppConfig::AppTheme::Dark;
    if (str == "Dim") return AppConfig::AppTheme::Dim;
    return AppConfig::AppTheme::Light;
}

void SettingsDialog::onConnectionStatusChanged(const bool &connection){
    if (moosIpLineEdit){
        moosIpLineEdit->setDisabled(connection);
    }
}

void SettingsDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    emit dialogOpened();
    qDebug() << "SettingsDialog opened";
}

void SettingsDialog::closeEvent(QCloseEvent *event) {
    QDialog::closeEvent(event);
    emit dialogClosed();
    qDebug() << "SettingsDialog closed";
}

void SettingsDialog::onAddGpsRow()
{
    int row = gpsTableWidget->rowCount();
    gpsTableWidget->insertRow(row);

    // Create and set a default name item
    QTableWidgetItem *nameItem = new QTableWidgetItem(QString("GPS %1").arg(row + 1));
    gpsTableWidget->setItem(row, 0, nameItem);

    // Create and set default offset items
    QTableWidgetItem *offsetXItem = new QTableWidgetItem("0.0");
    offsetXItem->setTextAlignment(Qt::AlignCenter);
    gpsTableWidget->setItem(row, 1, offsetXItem);

    QTableWidgetItem *offsetYItem = new QTableWidgetItem("0.0");
    offsetYItem->setTextAlignment(Qt::AlignCenter);
    gpsTableWidget->setItem(row, 2, offsetYItem);

    updatePrimaryGpsCombo();
}

void SettingsDialog::onRemoveGpsRow()
{
    int currentRow = gpsTableWidget->currentRow();
    if (currentRow >= 0) {
        gpsTableWidget->removeRow(currentRow);
        updatePrimaryGpsCombo();
    }
}

void SettingsDialog::updatePrimaryGpsCombo()
{
    QString currentSelection = primaryGpsCombo->currentText();
    primaryGpsCombo->clear();
    for (int i = 0; i < gpsTableWidget->rowCount(); ++i) {
        QTableWidgetItem *item = gpsTableWidget->item(i, 0);
        if (item) {
            primaryGpsCombo->addItem(item->text(), i);
        }
    }

    int index = primaryGpsCombo->findText(currentSelection);
    if (index != -1) {
        primaryGpsCombo->setCurrentIndex(index);
    }
}