#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "appconfig.h"
#include "mainwindow.h"
#include "qsqlerror.h"
#include "routesafetyfeature.h"
#include "aisdatabasemanager.h"

#ifdef _WIN32
#include <windows.h>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#endif

#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
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
#include <QSqlDatabase>
#include <QProgressDialog>
#include <QApplication>
#include <QtConcurrent>
#include <QFutureWatcher>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent), isDatabaseConnected(false), loadingDialog(nullptr), connectionWatcher(nullptr) {
    setupUI();
    loadSettings();

    // Connect button action
    connect(dbConnectButton, &QPushButton::clicked, this, &SettingsDialog::onDbConnectClicked);

    // NOTE: Removed initial database connection check to speed up dialog opening
    // Connection status will be set from main window when dialog opens
    isDatabaseConnected = false;
}

void SettingsDialog::setDatabaseConnectionStatus(bool connected)
{
    isDatabaseConnected = connected;

    if (connected) {
        dbStatusLabel->setText("Connected");
        dbStatusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    } else {
        dbStatusLabel->setText("Disconnected");
        dbStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    }
}

void SettingsDialog::setupUI() {
    setWindowTitle("Settings Manager");
    resize(400, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QTabWidget *tabWidget = new QTabWidget(this);

    // Make all QGroupBox titles bold without affecting child widget fonts
    this->setStyleSheet("QGroupBox::title { font-weight: bold; }");

    // =========== CONNECTION TAB =========== //
    QWidget *connectionTab = new QWidget;
    QVBoxLayout *connectionLayout = new QVBoxLayout(connectionTab);

    // MOOSDB Section
    QGroupBox *moosdbGroup = new QGroupBox(tr("MOOSDB"));
    QFormLayout *moosdbForm = new QFormLayout(moosdbGroup);
    moosIpLineEdit = new QLineEdit;
    moosIpLineEdit->setDisabled(false);
    moosPortLineEdit = new QLineEdit;
    moosdbForm->addRow("MOOSDB IP:", moosIpLineEdit);
    //moosdbForm->addRow("MOOSDB Port:", moosPortLineEdit);

    // Database Section
    QGroupBox *databaseGroup = new QGroupBox(tr("Database"));
    QVBoxLayout *databaseLayout = new QVBoxLayout(databaseGroup);

    QFormLayout *databaseForm = new QFormLayout;
    dbHostLineEdit = new QLineEdit;
    dbPortLineEdit = new QLineEdit;
    dbNameLineEdit = new QLineEdit;
    dbUserLineEdit = new QLineEdit;
    dbPasswordLineEdit = new QLineEdit;
    dbPasswordLineEdit->setEchoMode(QLineEdit::Password);

    databaseForm->addRow("Host:", dbHostLineEdit);
    databaseForm->addRow("Port:", dbPortLineEdit);
    databaseForm->addRow("DB Name:", dbNameLineEdit);
    databaseForm->addRow("User:", dbUserLineEdit);
    databaseForm->addRow("Password:", dbPasswordLineEdit);

    databaseLayout->addLayout(databaseForm);

    // Database status and connect button
    QHBoxLayout *dbStatusLayout = new QHBoxLayout;

    dbStatusLabel = new QLabel("Disconnected");
    dbStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    dbConnectButton = new QPushButton("Connect");
    dbConnectButton->setMaximumWidth(100);

    dbStatusLayout->addWidget(dbStatusLabel);
    dbStatusLayout->addStretch();
    dbStatusLayout->addWidget(dbConnectButton);

    databaseLayout->addLayout(dbStatusLayout);

    // Align Connection tab fields so inputs start at the same x as
    // the "MOOSDB IP:" input by normalizing label widths
    {
        int refWidth = 0;
        if (QLayoutItem* moosdbLabelItem = moosdbForm->itemAt(0, QFormLayout::LabelRole)) {
            if (QWidget* moosdbLabel = moosdbLabelItem->widget()) refWidth = moosdbLabel->sizeHint().width();
        }
        if (refWidth == 0) {
            QFontMetrics fm(moosdbGroup->font());
            refWidth = fm.horizontalAdvance(tr("MOOSDB IP:"));
        }

        auto applyLabelWidth = [&](QFormLayout* form) {
            if (!form) return;
            for (int i = 0; i < form->rowCount(); ++i) {
                if (QLayoutItem* li = form->itemAt(i, QFormLayout::LabelRole)) {
                    if (QWidget* w = li->widget()) w->setMinimumWidth(refWidth);
                }
            }
        };

        applyLabelWidth(moosdbForm);
        applyLabelWidth(databaseForm);
    }

    connectionLayout->addWidget(moosdbGroup);
    connectionLayout->addWidget(databaseGroup);
    connectionLayout->addStretch();

    // =========== OWNSHIP TAB =========== //
    QWidget *ownShipTab = new QWidget;
    QVBoxLayout *ownShipLayout = new QVBoxLayout(ownShipTab);

    // Chart Display
    QGroupBox *chartDisplayGroup = new QGroupBox(tr("Navigation Mode"));
    QFormLayout *chartDisplayForm = new QFormLayout(chartDisplayGroup);

    centeringCombo = new QComboBox;
    centeringCombo->addItem("Auto Recenter", "AutoRecenter");
    centeringCombo->addItem("Centered", "Centered");
    centeringCombo->addItem("Look Ahead", "LookAhead");
    centeringCombo->addItem("Manual Offset", "Manual");
    chartDisplayForm->addRow("Default Centering:", centeringCombo);

    orientationCombo = new QComboBox;
    orientationCombo->addItem("North Up", "NorthUp");
    orientationCombo->addItem("Head Up", "HeadUp");
    orientationCombo->addItem("Course Up", "CourseUp");
    chartDisplayForm->addRow("Default Orientation:", orientationCombo);

    headingLabel = new QLabel("Course-Up Heading:");
    headingSpin = new QSpinBox;
    headingSpin->setRange(0, 359);
    headingSpin->setSuffix("°");
    // headingLabel->setVisible(false);
    // headingSpin->setVisible(false);
    chartDisplayForm->addRow(headingLabel, headingSpin);

    /*
    connect(orientationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
        bool isCourseUp = (orientationCombo->currentData().toString() == "CourseUp");
        headingLabel->setVisible(isCourseUp);
        headingSpin->setVisible(isCourseUp);
    });
    */

    // Track Line Group
    QGroupBox *trackLineGroup = new QGroupBox(tr("Track Line"));
    QFormLayout *trackLineForm = new QFormLayout(trackLineGroup);

    trailCombo = new QComboBox;
    trailCombo->addItem("Every Update", 0);
    trailCombo->addItem("Fixed Interval", 1);
    trailCombo->addItem("Fixed Distance", 2);
    trackLineForm->addRow("Track Line Mode:", trailCombo);

    trailSpin = new QSpinBox;
    trailLabel = new QLabel("Interval:");
    trailSpin->setRange(1, 300);
    trailSpin->setSuffix(" minute(s)");
    trailLabel->setVisible(false);
    trailSpin->setVisible(false);
    trackLineForm->addRow(trailLabel, trailSpin);

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

    trackLineForm->addRow(trailLabelDistance, trailSpinDistance);

    connect(trailCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
        bool isMode = (trailCombo->currentData() == 2);
        trailLabelDistance->setVisible(isMode);
        trailSpinDistance->setVisible(isMode);
    });

    // Data View
    QGroupBox *dataViewGroup = new QGroupBox(tr("Data View Format"));
    QFormLayout *dataViewForm = new QFormLayout(dataViewGroup);

    latViewCombo = new QComboBox;
    latViewCombo->addItem("NAV_LAT", "NAV_LAT");
    latViewCombo->addItem("NAV_LAT_DMS", "NAV_LAT_DMS");
    latViewCombo->addItem("NAV_LAT_DMM", "NAV_LAT_DMM");
    dataViewForm->addRow("Latitude:", latViewCombo);

    longViewCombo = new QComboBox;
    longViewCombo->addItem("NAV_LONG", "NAV_LONG");
    longViewCombo->addItem("NAV_LONG_DMS", "NAV_LONG_DMS");
    longViewCombo->addItem("NAV_LONG_DMM", "NAV_LONG_DMM");
    dataViewForm->addRow("Longitude:", longViewCombo);

    //

    //ownShipTab->setLayout(ownShipLayout);
    // Navigation Mode moved to Display tab; Track Line to Ownship (Ship Dimensions); Data View to Display
    ownShipLayout->setSpacing(10);

    // =========== SHIP DIMENSION TAB =========== //
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

    // Turning Prediction header (title left, checkbox right)
    QWidget *turningHeaderWidget = new QWidget;
    QHBoxLayout *turningHeaderLayout = new QHBoxLayout(turningHeaderWidget);
    turningHeaderLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *turningHeaderLabel = new QLabel(tr("Turning Radius Prediction"));
    QFont turningHeaderFont = turningHeaderLabel->font();
    turningHeaderFont.setBold(true);
    turningHeaderLabel->setFont(turningHeaderFont);
    turningHeaderLabel->setStyleSheet("font-weight: bold;");
    showTurningPredictionCheckBox = new QCheckBox(tr("Enable"));
    turningHeaderLayout->addWidget(turningHeaderLabel);
    turningHeaderLayout->addStretch();
    turningHeaderLayout->addWidget(showTurningPredictionCheckBox);

    // Group content without title
    turningPredictionGroup = new QGroupBox("");
    turningPredictionGroup->setObjectName("turningPredictionContent");
    turningPredictionGroup->setStyleSheet("QGroupBox#turningPredictionContent { padding-top: 15px; margin-top: 0px; }");
    QFormLayout *turningPredictionForm = new QFormLayout(turningPredictionGroup);
    int tpLeft, tpTop, tpRight, tpBottom;
    turningPredictionForm->getContentsMargins(&tpLeft, &tpTop, &tpRight, &tpBottom);
    turningPredictionForm->setContentsMargins(tpLeft, 0, tpRight, tpBottom);

    predictionTimeLabel = new QLabel(tr("Prediction Time:"));
    predictionTimeSpin = new QSpinBox;
    predictionTimeSpin->setRange(1, 10);
    predictionTimeSpin->setValue(3);
    predictionTimeSpin->setSuffix(" min");
    predictionTimeSpin->setToolTip(tr("How many minutes ahead to predict the ship's path"));
    turningPredictionForm->addRow(predictionTimeLabel, predictionTimeSpin);

    predictionDensityLabel = new QLabel(tr("Ship Outline Density:"));
    predictionDensityCombo = new QComboBox;
    predictionDensityCombo->addItem("Low (every 20s)", 1);
    predictionDensityCombo->addItem("Medium (every 10s)", 2);
    predictionDensityCombo->addItem("High (every 5s)", 3);
    predictionDensityCombo->setCurrentIndex(1); // Default: Medium
    predictionDensityCombo->setToolTip(tr("How often to draw ship outlines along the prediction path"));
    turningPredictionForm->addRow(predictionDensityLabel, predictionDensityCombo);

    // Connect checkbox to enable/disable controls
    connect(showTurningPredictionCheckBox, &QCheckBox::toggled, this, [=](bool enabled) {
        predictionTimeLabel->setEnabled(enabled);
        predictionTimeSpin->setEnabled(enabled);
        predictionDensityLabel->setEnabled(enabled);
        predictionDensityCombo->setEnabled(enabled);
    });

    // Initialize enabled state based on checkbox
    predictionTimeLabel->setEnabled(showTurningPredictionCheckBox->isChecked());
    predictionTimeSpin->setEnabled(showTurningPredictionCheckBox->isChecked());
    predictionDensityLabel->setEnabled(showTurningPredictionCheckBox->isChecked());
    predictionDensityCombo->setEnabled(showTurningPredictionCheckBox->isChecked());

    // Navigation Safety group removed as requested

    // GPS Configuration Group
    QGroupBox *gpsGroup = new QGroupBox(tr("GPS Antenna Positions"));
    QVBoxLayout *gpsLayout = new QVBoxLayout(gpsGroup);
    gpsTableWidget = new QTableWidget;
    gpsTableWidget->setColumnCount(3);
    gpsTableWidget->setHorizontalHeaderLabels({"Name", "Offset X (Centerline)", "Offset Y (Bow)"});
    // Ensure "Offset X (Centerline)" header is fully visible when dialog opens
    QHeaderView *gpsHeaderView = gpsTableWidget->horizontalHeader();
    gpsHeaderView->setSectionResizeMode(1, QHeaderView::ResizeToContents);
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

    // Align Ownship fields so inputs start at the same x as the
    // "Primary Reference (CCRP)" input by giving all forms' labels
    // the same minimum width as the Primary Reference label.
    {
        int refWidth = 0;
        if (QLayoutItem* primaryLabelItem = primaryGpsForm->itemAt(0, QFormLayout::LabelRole)) {
            if (QWidget* primaryLabel = primaryLabelItem->widget()) refWidth = primaryLabel->sizeHint().width();
        }
        if (refWidth == 0) {
            QFontMetrics fm(dimensionsGroup->font());
            refWidth = fm.horizontalAdvance(tr("Primary Reference (CCRP):"));
        }

        auto applyLabelWidth = [&](QFormLayout* form) {
            if (!form) return;
            for (int i = 0; i < form->rowCount(); ++i) {
                if (QLayoutItem* li = form->itemAt(i, QFormLayout::LabelRole)) {
                    if (QWidget* w = li->widget()) w->setMinimumWidth(refWidth);
                }
            }
        };

        applyLabelWidth(dimensionsForm);
        applyLabelWidth(turningPredictionForm);
        applyLabelWidth(trackLineForm);
        applyLabelWidth(primaryGpsForm);
    }

    // Ownship tab section order: Vessel Dimensions, Track Line, Turning Prediction, GPS
    shipDimensionsLayout->addWidget(dimensionsGroup);
    shipDimensionsLayout->addWidget(trackLineGroup);
    // Container to tightly couple header and group with minimal spacing
    QWidget *turningContainer = new QWidget;
    QVBoxLayout *turningContainerLayout = new QVBoxLayout(turningContainer);
    turningContainerLayout->setContentsMargins(0, 0, 0, 0);
    turningContainerLayout->setSpacing(0); // reduce gap between title and group border
    turningContainerLayout->addWidget(turningHeaderWidget);
    turningContainerLayout->addWidget(turningPredictionGroup);
    shipDimensionsLayout->addWidget(turningContainer);

    // Collision Risk header (title left, enable checkbox right)
    QWidget *collisionHeaderWidget = new QWidget;
    QHBoxLayout *collisionHeaderLayout = new QHBoxLayout(collisionHeaderWidget);
    collisionHeaderLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *collisionHeaderLabel = new QLabel(tr("Collision Risk Indication"));
    QFont collisionHeaderFont = collisionHeaderLabel->font();
    collisionHeaderFont.setBold(true);
    collisionHeaderLabel->setFont(collisionHeaderFont);
    collisionHeaderLabel->setStyleSheet("font-weight: bold;");

    enableCollisionRiskCheckBox = new QCheckBox(tr("Enable"));
    enableCollisionRiskCheckBox->setChecked(false);
    enableCollisionRiskCheckBox->setToolTip(tr("Display collision risk warnings during navigation"));
    collisionHeaderLayout->addWidget(collisionHeaderLabel);
    collisionHeaderLayout->addStretch();
    if (AppConfig::isDevelopment()){
        collisionHeaderLayout->addWidget(enableCollisionRiskCheckBox);
    }

    // Collision Risk content
    QGroupBox *collisionRiskGroup = new QGroupBox("");
    QFormLayout *collisionRiskForm = new QFormLayout(collisionRiskGroup);
    int crLeft, crTop, crRight, crBottom;
    collisionRiskForm->getContentsMargins(&crLeft, &crTop, &crRight, &crBottom);
    collisionRiskForm->setContentsMargins(crLeft, 0, crRight, crBottom);

    showRiskSymbolsCheckBox = new QCheckBox(tr("Show Risk Warning Symbols"));
    showRiskSymbolsCheckBox->setChecked(true);

    enableAudioAlertsCheckBox = new QCheckBox(tr("Enable Audio Alerts"));
    enableAudioAlertsCheckBox->setChecked(false);

    enablePulsingWarningsCheckBox = new QCheckBox(tr("Enable Pulsing Warnings"));
    enablePulsingWarningsCheckBox->setChecked(true);

    criticalRiskDistanceSpin = new QDoubleSpinBox;
    criticalRiskDistanceSpin->setRange(0.01, 1.0);
    criticalRiskDistanceSpin->setDecimals(2);
    criticalRiskDistanceSpin->setSingleStep(0.01);
    criticalRiskDistanceSpin->setValue(0.1);
    criticalRiskDistanceSpin->setSuffix(" NM");
    if (AppConfig::isDevelopment()){
        collisionRiskForm->addRow(tr("Critical Distance:"), criticalRiskDistanceSpin);
    }

    highRiskDistanceSpin = new QDoubleSpinBox;
    highRiskDistanceSpin->setRange(0.1, 2.0);
    highRiskDistanceSpin->setDecimals(2);
    highRiskDistanceSpin->setSingleStep(0.05);
    highRiskDistanceSpin->setValue(0.25);
    highRiskDistanceSpin->setSuffix(" NM");
    if (AppConfig::isDevelopment()){
        collisionRiskForm->addRow(tr("High Risk Distance:"), highRiskDistanceSpin);
    }

    criticalTimeSpin = new QDoubleSpinBox;
    criticalTimeSpin->setRange(0.5, 10.0);
    criticalTimeSpin->setDecimals(1);
    criticalTimeSpin->setSingleStep(0.5);
    criticalTimeSpin->setValue(2.0);
    criticalTimeSpin->setSuffix(" min");
    if (AppConfig::isDevelopment()){
        collisionRiskForm->addRow(tr("Critical Time:"), criticalTimeSpin);
    }

    // CPA/TCPA Threshold fields (merged into Collision Risk Indication)
    cpaSpin = new QDoubleSpinBox;
    cpaSpin->setRange(0.1, 5.0);
    cpaSpin->setDecimals(1);
    cpaSpin->setSingleStep(0.1);
    cpaSpin->setSuffix(" NM");
    collisionRiskForm->addRow(tr("CPA Threshold:"), cpaSpin);

    tcpaSpin = new QDoubleSpinBox;
    tcpaSpin->setRange(1, 20);
    tcpaSpin->setDecimals(0);
    tcpaSpin->setSingleStep(1);
    tcpaSpin->setSuffix(" min");
    collisionRiskForm->addRow(tr("TCPA Threshold:"), tcpaSpin);

    // Connect collision risk signals
    connect(enableCollisionRiskCheckBox, &QCheckBox::toggled, this, [=](bool enabled) {
        showRiskSymbolsCheckBox->setEnabled(enabled);
        enableAudioAlertsCheckBox->setEnabled(enabled);
        enablePulsingWarningsCheckBox->setEnabled(enabled);
        criticalRiskDistanceSpin->setEnabled(enabled);
        highRiskDistanceSpin->setEnabled(enabled);
        criticalTimeSpin->setEnabled(enabled);
        cpaSpin->setEnabled(enabled);
        tcpaSpin->setEnabled(enabled);
    });

    // Place remaining checkboxes at the bottom (three rows)
    QWidget *riskBottomWidget = new QWidget;
    QVBoxLayout *riskBottomLayout = new QVBoxLayout(riskBottomWidget);
    if (AppConfig::isDevelopment()){
        riskBottomLayout->setContentsMargins(0, 0, 0, 0);
        riskBottomLayout->setSpacing(4);
        riskBottomLayout->addWidget(showRiskSymbolsCheckBox);
        riskBottomLayout->addWidget(enableAudioAlertsCheckBox);
        riskBottomLayout->addWidget(enablePulsingWarningsCheckBox);
        collisionRiskForm->addRow("", riskBottomWidget);
    }

    // Place groups into appropriate tabs
    // CollisionRisk moved to AIS Target tab below
    shipDimensionsLayout->addWidget(gpsGroup);

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

    // ================ DISPLAY TAB ======================
    QWidget *displayTab = new QWidget;
    QFormLayout *displayLayout = new QFormLayout;

    // Theme group
    QGroupBox *themeGroup = new QGroupBox(tr("Theme"));
    QFormLayout *themeForm = new QFormLayout(themeGroup);
    displayModeCombo = new QComboBox;
    displayModeCombo->addItems({"Day", "Dusk", "Night"});
    themeForm->addRow(tr("Default Chart Theme:"), displayModeCombo);

    themeModeCombo = new QComboBox;
    themeModeCombo->addItems({"Light", "Dim", "Dark"});
    themeForm->addRow(tr("Default UI Theme:"), themeModeCombo);

    // Chart Manager group
    QGroupBox *chartManagerGroup = new QGroupBox(tr("Chart Manager"));
    QFormLayout *chartManagerForm = new QFormLayout(chartManagerGroup);

    isdtExpirationDaysSpin = new QSpinBox;
    isdtExpirationDaysSpin->setRange(1, 365);
    isdtExpirationDaysSpin->setValue(7);
    isdtExpirationDaysSpin->setSuffix(" days");
    isdtExpirationDaysSpin->setToolTip(tr("Number of days before ISDT (Issue Date) is considered expired"));
    chartManagerForm->addRow(tr("ISDT Expiration:"), isdtExpirationDaysSpin);

    // Merge Navigation Mode from Own Ship with Chart Move Mode
    chartCombo = new QComboBox;
    chartCombo->addItem("Drag", "Drag");
    chartCombo->addItem("Pan", "Pan");
    chartDisplayForm->addRow(tr("Chart Move Mode:"), chartCombo);

    // Display tab section order: Navigation Mode, Data View Format, Theme, Chart Manager
    displayLayout->addRow(chartDisplayGroup);
    // Add Data View Format group to Display tab
    displayLayout->addRow(dataViewGroup);
    displayLayout->addRow(themeGroup);
    displayLayout->addRow(chartManagerGroup);

    // Align Display tab fields so inputs start at the same x as
    // the "Default Chart Theme:" input by normalizing label widths
    {
        int refWidth = 0;
        if (QLayoutItem* lblItem = themeForm->itemAt(0, QFormLayout::LabelRole)) {
            if (QWidget* lbl = lblItem->widget()) refWidth = lbl->sizeHint().width();
        }
        if (refWidth == 0) {
            QFontMetrics fm(themeGroup->font());
            refWidth = fm.horizontalAdvance(tr("Default Chart Theme:"));
        }

        auto applyLabelWidth = [&](QFormLayout* form) {
            if (!form) return;
            for (int i = 0; i < form->rowCount(); ++i) {
                if (QLayoutItem* li = form->itemAt(i, QFormLayout::LabelRole)) {
                    if (QWidget* w = li->widget()) w->setMinimumWidth(refWidth);
                }
            }
        };

        applyLabelWidth(themeForm);
        applyLabelWidth(chartDisplayForm);
        applyLabelWidth(dataViewForm);
        applyLabelWidth(chartManagerForm);
    }
    displayTab->setLayout(displayLayout);

    // ================= SHIP DIMENSION TAB ====================== //
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

    // AIS Target
    QWidget *cpatcpaTab = new QWidget;
    QFormLayout *cpatcpaLayout = new QFormLayout;

    // Add Collision Risk header + group into AIS Target tab with tight spacing
    QWidget *collisionContainer = new QWidget;
    QVBoxLayout *collisionContainerLayout = new QVBoxLayout(collisionContainer);
    collisionContainerLayout->setContentsMargins(0, 0, 0, 0);
    collisionContainerLayout->setSpacing(0);
    collisionContainerLayout->addWidget(collisionHeaderWidget);
    collisionContainerLayout->addWidget(collisionRiskGroup);
    cpatcpaLayout->addRow(collisionContainer);

    // Align AIS Target tab fields so inputs start at the same x
    // by normalizing label widths in collisionRiskForm
    {
        int refWidth = 0;
        // Find the "High Risk Distance:" label for reference width
        for (int i = 0; i < collisionRiskForm->rowCount(); ++i) {
            if (QLayoutItem* li = collisionRiskForm->itemAt(i, QFormLayout::LabelRole)) {
                if (QLabel* lbl = qobject_cast<QLabel*>(li->widget())) {
                    if (lbl->text() == tr("High Risk Distance:")) {
                        refWidth = lbl->sizeHint().width();
                        break;
                    }
                }
            }
        }
        if (refWidth == 0) {
            QFontMetrics fm(collisionRiskGroup->font());
            refWidth = fm.horizontalAdvance(tr("High Risk Distance:"));
        }

        // Apply label width to all labels in collisionRiskForm for alignment
        for (int i = 0; i < collisionRiskForm->rowCount(); ++i) {
            if (QLayoutItem* li = collisionRiskForm->itemAt(i, QFormLayout::LabelRole)) {
                if (QWidget* w = li->widget()) w->setMinimumWidth(refWidth);
            }
        }
    }
    cpatcpaTab->setLayout(cpatcpaLayout);



    // ================================================= //

    // Reorder tabs: Own Ship, Ship Dimension, AIS Target, Display, Connection
    // Remove Own Ship tab; rename Ship Dimensions tab to Ownship
    tabWidget->addTab(shipDimensionsTab, "Ownship");
    tabWidget->addTab(cpatcpaTab, "AIS Target");
    tabWidget->addTab(displayTab, "Display");
    tabWidget->addTab(connectionTab, "Connection");

    if (AppConfig::isDevelopment()){
        tabWidget->addTab(guardzoneTab, "GuardZone");
        tabWidget->addTab(alertTab, "Alert");
    }

    mainLayout->addWidget(tabWidget);

    // Ensure all group titles render bold regardless of platform style
    const auto groups = this->findChildren<QGroupBox*>();
    for (QGroupBox* gb : groups) {
        if (!gb) continue;
        gb->setStyleSheet("QGroupBox { font-weight: bold; } QGroupBox * { font-weight: normal; }");
    }


    // ================ BUTTON CONNECT ================= //
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

    // Database
    dbHostLineEdit->setText(settings.value("Database/host", "localhost").toString());
    dbPortLineEdit->setText(settings.value("Database/port", "5432").toString());
    dbNameLineEdit->setText(settings.value("Database/name", "ecdis_ais").toString());
    dbUserLineEdit->setText(settings.value("Database/user", "postgres").toString());
    dbPasswordLineEdit->setText(settings.value("Database/password", "").toString());

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

    // Chart Manager - ISDT Expiration
    isdtExpirationDaysSpin->setValue(settings.value("ChartManager/isdt_expiration_days", 7).toInt());

    QString themeMode = settings.value("Display/theme", "Dark").toString();
    int themeIndex = themeModeCombo->findText(themeMode);
    if (themeIndex >= 0) {
        themeModeCombo->setCurrentIndex(themeIndex);
    } else {
        themeModeCombo->setCurrentIndex(0); // fallback default
    }

    QString chartMode = settings.value("Display/move", "Drag").toString();
    int chartIndex = chartCombo->findText(chartMode);
    if (chartIndex >= 0) {
        chartCombo->setCurrentIndex(chartIndex);
    } else {
        chartCombo->setCurrentIndex(0); // fallback default
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

    QString latView = settings.value("OwnShip/lat_view", "NAV_LAT").toString();
    QString longView = settings.value("OwnShip/long_view", "NAV_LONG").toString();

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

    int latIndex = latViewCombo->findData(latView);
    if (latIndex >= 0) latViewCombo->setCurrentIndex(latIndex);
    else latViewCombo->setCurrentIndex(0);

    int longIndex = longViewCombo->findData(longView);
    if (longIndex >= 0) longViewCombo->setCurrentIndex(longIndex);
    else longViewCombo->setCurrentIndex(0);

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

    // Collision Risk (load settings)
    enableCollisionRiskCheckBox->setChecked(settings.value("CollisionRisk/enabled", false).toBool());
    showRiskSymbolsCheckBox->setChecked(settings.value("CollisionRisk/show_symbols", true).toBool());
    enableAudioAlertsCheckBox->setChecked(settings.value("CollisionRisk/audio_alerts", false).toBool());
    enablePulsingWarningsCheckBox->setChecked(settings.value("CollisionRisk/pulsing_warnings", true).toBool());
    criticalRiskDistanceSpin->setValue(settings.value("CollisionRisk/critical_distance_nm", 0.1).toDouble());
    highRiskDistanceSpin->setValue(settings.value("CollisionRisk/high_distance_nm", 0.25).toDouble());
    criticalTimeSpin->setValue(settings.value("CollisionRisk/critical_time_min", 2.0).toDouble());
    // Apply initial enabled state
    {
        bool enabled = enableCollisionRiskCheckBox->isChecked();
        showRiskSymbolsCheckBox->setEnabled(enabled);
        enableAudioAlertsCheckBox->setEnabled(enabled);
        enablePulsingWarningsCheckBox->setEnabled(enabled);
        criticalRiskDistanceSpin->setEnabled(enabled);
        highRiskDistanceSpin->setEnabled(enabled);
        criticalTimeSpin->setEnabled(enabled);
    }

    // Ship Dimensions
    shipLengthSpin->setValue(settings.value("ShipDimensions/length", 170.0).toDouble());
    shipBeamSpin->setValue(settings.value("ShipDimensions/beam", 13.0).toDouble());
    shipHeightSpin->setValue(settings.value("ShipDimensions/height", 25.0).toDouble());

    // Turning Prediction
    showTurningPredictionCheckBox->setChecked(settings.value("TurningPrediction/enabled", false).toBool());
    predictionTimeSpin->setValue(settings.value("TurningPrediction/timeMinutes", 3).toInt());

    int densityValue = settings.value("TurningPrediction/density", 2).toInt();
    int densityIndex = predictionDensityCombo->findData(densityValue);
    if (densityIndex >= 0) {
        predictionDensityCombo->setCurrentIndex(densityIndex);
    } else {
        predictionDensityCombo->setCurrentIndex(1); // Default: Medium
    }

    // Update enabled state of prediction time controls
    bool predictionEnabled = showTurningPredictionCheckBox->isChecked();
    predictionTimeLabel->setEnabled(predictionEnabled);
    predictionTimeSpin->setEnabled(predictionEnabled);
    predictionDensityLabel->setEnabled(predictionEnabled);
    predictionDensityCombo->setEnabled(predictionEnabled);

    // Navigation Safety Variables removed
    // (kept backward compatibility in settings file but UI control removed)
    // If needed in future, guard usage with null checks.

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

    // Database
    settings.setValue("Database/host", dbHostLineEdit->text());
    settings.setValue("Database/port", dbPortLineEdit->text());
    settings.setValue("Database/name", dbNameLineEdit->text());
    settings.setValue("Database/user", dbUserLineEdit->text());
    settings.setValue("Database/password", dbPasswordLineEdit->text());

    // AIS
    if (AppConfig::isDevelopment()){
        settings.setValue("AIS/source", aisSourceCombo->currentText());
        settings.setValue("AIS/ip", ipAisLineEdit->text());
        settings.setValue("AIS/log_file", logFileLineEdit->text());
    }

    // Display
    settings.setValue("Display/mode", displayModeCombo->currentText());
    settings.setValue("Display/theme", themeModeCombo->currentText());

    // Chart Manager - ISDT Expiration
    settings.setValue("ChartManager/isdt_expiration_days", isdtExpirationDaysSpin->value());

    // Chart
    settings.setValue("Display/move", chartCombo->currentText());

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

    settings.setValue("OwnShip/lat_view", latViewCombo->currentData().toString());
    settings.setValue("OwnShip/long_view", longViewCombo->currentData().toString());

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

    // Collision Risk (save settings)
    settings.setValue("CollisionRisk/enabled", enableCollisionRiskCheckBox->isChecked());
    settings.setValue("CollisionRisk/show_symbols", showRiskSymbolsCheckBox->isChecked());
    settings.setValue("CollisionRisk/audio_alerts", enableAudioAlertsCheckBox->isChecked());
    settings.setValue("CollisionRisk/pulsing_warnings", enablePulsingWarningsCheckBox->isChecked());
    settings.setValue("CollisionRisk/critical_distance_nm", criticalRiskDistanceSpin->value());
    settings.setValue("CollisionRisk/high_distance_nm", highRiskDistanceSpin->value());
    settings.setValue("CollisionRisk/critical_time_min", criticalTimeSpin->value());

    // Turning Prediction
    settings.setValue("TurningPrediction/enabled", showTurningPredictionCheckBox->isChecked());
    settings.setValue("TurningPrediction/timeMinutes", predictionTimeSpin->value());
    settings.setValue("TurningPrediction/density", predictionDensityCombo->currentData().toInt());
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

    // Chart
    data.chartMode = settings.value("Display/move", "Drag").toString();

    // Chart Manager - ISDT Expiration
    data.isdtExpirationDays = settings.value("ChartManager/isdt_expiration_days", 7).toInt();

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

    data.latViewMode = settings.value("OwnShip/lat_view", "NAV_LAT").toString();
    data.longViewMode = settings.value("OwnShip/long_view", "NAV_LONG").toString();

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

    // Chart Manager - ISDT Expiration
    data.isdtExpirationDays = isdtExpirationDaysSpin->value();

    // Chart
    data.chartMode = chartCombo->currentText();

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

    data.latViewMode = latViewCombo->currentData().toString();
    data.longViewMode = longViewCombo->currentData().toString();

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

    // Turning Prediction
    data.showTurningPrediction = showTurningPredictionCheckBox->isChecked();
    data.predictionTimeMinutes = predictionTimeSpin->value();
    data.predictionDensity = predictionDensityCombo->currentData().toInt();

    // Collision Risk
    data.enableCollisionRisk = enableCollisionRiskCheckBox->isChecked();
    data.criticalRiskDistance = criticalRiskDistanceSpin->value();
    data.highRiskDistance = highRiskDistanceSpin->value();
    data.criticalRiskTime = criticalTimeSpin->value();
    data.showRiskSymbols = showRiskSymbolsCheckBox->isChecked();
    data.enableAudioAlerts = enableAudioAlertsCheckBox->isChecked();
    data.enablePulsingWarnings = enablePulsingWarningsCheckBox->isChecked();

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

    // Save Navigation Safety Variables to settings file
    QString configPath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC" + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    // Navigation Safety Variables removed; skip persisting UI controls

    SettingsManager::instance().save(data);

    // Find the EcWidget instance and apply the new dimensions
    /*
    if (parentWidget()) {
        MainWindow* mainWindow = qobject_cast<MainWindow*>(parentWidget());
        if (mainWindow) {
            EcWidget* ecWidget = mainWindow->findChild<EcWidget*>();
            if (ecWidget) {
                ecWidget->defaultSettingsStartUp();
                ecWidget->applyShipDimensions();
            }
        }
    }
    */

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

void SettingsDialog::onNavDepthChanged(double value)
{
    RouteSafetyFeature::setNavDepth(value);
    qDebug() << "[SETTINGS] NAV_DEPTH changed to:" << value;
}

void SettingsDialog::onNavDraftChanged(double value)
{
    RouteSafetyFeature::setNavDraft(value);
    qDebug() << "[SETTINGS] NAV_DRAFT changed to:" << value;
}

void SettingsDialog::onNavDraftBelowKeelChanged(double value)
{
    RouteSafetyFeature::setNavDraftBelowKeel(value);
    qDebug() << "[SETTINGS] NAV_DRAFT_BELOW_KEEL changed to:" << value;
}

SettingsDialog::DatabaseSettings SettingsDialog::getDatabaseSettings()
{
    DatabaseSettings settings;

    QString configPath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC" + "/config.ini";
    QSettings configSettings(configPath, QSettings::IniFormat);

    settings.host = configSettings.value("Database/host", "localhost").toString();
    settings.port = configSettings.value("Database/port", "5432").toString();
    settings.dbName = configSettings.value("Database/name", "ecdis_ais").toString();
    settings.user = configSettings.value("Database/user", "postgres").toString();
    settings.password = configSettings.value("Database/password", "").toString();

    return settings;
}

void SettingsDialog::onDbConnectClicked()
{
    // Save current settings first
    QString configPath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC" + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    settings.setValue("Database/host", dbHostLineEdit->text());
    settings.setValue("Database/port", dbPortLineEdit->text());
    settings.setValue("Database/name", dbNameLineEdit->text());
    settings.setValue("Database/user", dbUserLineEdit->text());
    settings.setValue("Database/password", dbPasswordLineEdit->text());

    // Create loading dialog
    loadingDialog = new QProgressDialog("Connecting to database...", "Cancel", 0, 0, this);
    loadingDialog->setWindowModality(Qt::WindowModal);
    loadingDialog->setCancelButton(nullptr); // Remove cancel button to prevent interruption
    loadingDialog->setRange(0, 0); // Indeterminate progress
    loadingDialog->setMinimumDuration(0); // Show immediately
    loadingDialog->show();

    // Create future watcher for async connection
    if (connectionWatcher) {
        connectionWatcher->deleteLater();
    }
    connectionWatcher = new QFutureWatcher<bool>(this);

    // Connect the finished signal
    connect(connectionWatcher, &QFutureWatcher<bool>::finished, this, &SettingsDialog::onDatabaseConnectionFinished);

    // Start the database connection in a separate thread
    QFuture<bool> future = QtConcurrent::run(this, &SettingsDialog::checkDatabaseConnectionAsync);
    connectionWatcher->setFuture(future);
}

bool SettingsDialog::checkDatabaseConnectionAsync()
{
#ifdef _WIN32
    // Ensure PostgreSQL is in PATH
    QString currentPath = qgetenv("PATH");
    QString pgPath = "C:/Program Files/PostgreSQL/16/bin";

    // Try alternative paths if primary doesn't exist
    QDir pgDir(pgPath);
    if (!pgDir.exists()) {
        QStringList altPaths = {
            "C:/Program Files/PostgreSQL/15/bin",
            "C:/Program Files/PostgreSQL/14/bin",
            "C:/Program Files (x86)/PostgreSQL/16/bin",
            "C:/Program Files (x86)/PostgreSQL/15/bin"
        };

        for (const QString& altPath : altPaths) {
            QDir altDir(altPath);
            if (altDir.exists()) {
                pgPath = altPath;
                break;
            }
        }
    }

    QString newPath = pgPath + ";" + currentPath;
    qputenv("PATH", newPath.toLocal8Bit());
#endif

    // Create temporary database connection to test
    QSqlDatabase testDb = QSqlDatabase::addDatabase("QPSQL", "test_connection");

    testDb.setHostName(dbHostLineEdit->text());
    testDb.setPort(dbPortLineEdit->text().toInt());
    testDb.setDatabaseName(dbNameLineEdit->text());
    testDb.setUserName(dbUserLineEdit->text());
    testDb.setPassword(dbPasswordLineEdit->text());

    bool connected = testDb.open();

    // Close test connection
    if (testDb.isOpen()) {
        testDb.close();
    }
    QSqlDatabase::removeDatabase("test_connection");

    return connected;
}

void SettingsDialog::checkDatabaseConnection()
{
    // This method can still be used for synchronous connection (startup)
    bool connected = checkDatabaseConnectionAsync();

    if (connected) {
        dbStatusLabel->setText("Connected");
        dbStatusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        isDatabaseConnected = true;
        qDebug() << "Database connection test: SUCCESS";
    } else {
        dbStatusLabel->setText("Disconnected");
        dbStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        isDatabaseConnected = false;
        qWarning() << "Database connection test FAILED";
    }

    // Emit signal to notify about database connection status change
    emit databaseConnectionStatusChanged(isDatabaseConnected);
}

void SettingsDialog::onDatabaseConnectionFinished()
{
    // Get the result from the future
    bool connected = connectionWatcher->result();

    // Update UI based on connection result
    if (connected) {
        dbStatusLabel->setText("Connected");
        dbStatusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        isDatabaseConnected = true;

        // Update AisDatabaseManager instance with new connection parameters
        // This ensures that recording and playback functions use the updated database connection
        bool dbManagerConnected = AisDatabaseManager::instance().connect(
            dbHostLineEdit->text(),
            dbPortLineEdit->text().toInt(),
            dbNameLineEdit->text(),
            dbUserLineEdit->text(),
            dbPasswordLineEdit->text()
        );

        if (dbManagerConnected) {
            qDebug() << "Database connection test: SUCCESS - AisDatabaseManager updated";
        } else {
            qWarning() << "Database connection test: SUCCESS but AisDatabaseManager update FAILED";
            isDatabaseConnected = false;
            dbStatusLabel->setText("Disconnected");
            dbStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        }
    } else {
        dbStatusLabel->setText("Disconnected");
        dbStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        isDatabaseConnected = false;

        // Explicitly disconnect AisDatabaseManager to ensure consistency
        AisDatabaseManager::instance().disconnect();
        qWarning() << "Database connection test FAILED - AisDatabaseManager disconnected";
    }

    // Emit signal to notify about database connection status change
    emit databaseConnectionStatusChanged(isDatabaseConnected);

    // Hide and delete loading dialog
    if (loadingDialog) {
        loadingDialog->hide();
        loadingDialog->deleteLater();
        loadingDialog = nullptr;
    }

    // Clean up connection watcher
    connectionWatcher->deleteLater();
    connectionWatcher = nullptr;
}

SettingsDialog::~SettingsDialog()
{
    // Clean up loading dialog if it exists
    if (loadingDialog) {
        loadingDialog->hide();
        loadingDialog->deleteLater();
        loadingDialog = nullptr;
    }

    // Clean up connection watcher if it exists
    if (connectionWatcher) {
        connectionWatcher->deleteLater();
        connectionWatcher = nullptr;
    }
}
