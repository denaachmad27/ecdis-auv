#include "cpatcpapanel.h"
#include "cpatcpasettingsdialog.h"
#include <QDebug>

CPATCPAPanel::CPATCPAPanel(QWidget *parent)
    : QWidget(parent)
    , ecWidget(nullptr)
    , dangerousCount(0)
    , totalTargets(0)
    , refreshTimer(nullptr)  // Ganti nama
{
    setupUI();

    // Setup refresh timer dengan syntax lama
    refreshTimer = new QTimer();  // Ganti nama
    refreshTimer->setParent(this);
    connect(refreshTimer, SIGNAL(timeout()), this, SLOT(onTimerTimeout()));  // Ganti nama

    // Update setiap 2 detik
    refreshTimer->start(2000);

    // Add tooltips
    refreshButton->setToolTip("Refresh AIS targets data (F5)");
    settingsButton->setToolTip("Open CPA/TCPA settings dialog");
    clearAlarmsButton->setToolTip("Clear all alarm highlights");

    // Add keyboard shortcuts
    refreshButton->setShortcut(QKeySequence("F5"));
    settingsButton->setShortcut(QKeySequence("Ctrl+T"));
}

void CPATCPAPanel::setupUI()
{
    mainLayout = new QVBoxLayout();
    this->setLayout(mainLayout);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // System Status Panel
    setupStatusPanel();

    // Own Ship Info Panel
    ownShipGroup = new QGroupBox("Own Ship");
    QGridLayout* ownShipLayout = new QGridLayout();
    ownShipGroup->setLayout(ownShipLayout);

    ownShipLatLabel = new QLabel("Lat: --");
    ownShipLonLabel = new QLabel("Lon: --");
    ownShipCogLabel = new QLabel("COG: --");
    ownShipSogLabel = new QLabel("SOG: --");

    ownShipLayout->addWidget(new QLabel("Position:"), 0, 0);
    ownShipLayout->addWidget(ownShipLatLabel, 0, 1);
    ownShipLayout->addWidget(ownShipLonLabel, 0, 2);
    ownShipLayout->addWidget(new QLabel("Course/Speed:"), 1, 0);
    ownShipLayout->addWidget(ownShipCogLabel, 1, 1);
    ownShipLayout->addWidget(ownShipSogLabel, 1, 2);

    // Targets Table Panel
    setupTargetsTable();

    // Control Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    refreshButton = new QPushButton("Refresh");
    settingsButton = new QPushButton("Settings");
    clearAlarmsButton = new QPushButton("Clear Alarms");

    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshData()));
    connect(settingsButton, SIGNAL(clicked()), this, SLOT(onSettingsClicked()));
    connect(clearAlarmsButton, SIGNAL(clicked()), this, SLOT(onClearAlarmsClicked()));

    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(settingsButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(clearAlarmsButton);

    // Add all to main layout
    mainLayout->addWidget(statusGroup);
    mainLayout->addWidget(ownShipGroup);
    mainLayout->addWidget(targetsGroup);
    mainLayout->addLayout(buttonLayout);
}

void CPATCPAPanel::setupStatusPanel()
{
    statusGroup = new QGroupBox("CPA/TCPA Status");
    QGridLayout* statusLayout = new QGridLayout();
    statusGroup->setLayout(statusLayout);

    systemStatusLabel = new QLabel("System: <font color='green'>ACTIVE</font>");
    activeTargetsLabel = new QLabel("Targets: 0");
    dangerousTargetsLabel = new QLabel("Dangerous: <font color='red'>0</font>");
    lastUpdateLabel = new QLabel("Last Update: --");

    statusLayout->addWidget(systemStatusLabel, 0, 0);
    statusLayout->addWidget(activeTargetsLabel, 0, 1);
    statusLayout->addWidget(dangerousTargetsLabel, 1, 0);
    statusLayout->addWidget(lastUpdateLabel, 1, 1);
}

void CPATCPAPanel::setupTargetsTable()
{
    targetsGroup = new QGroupBox("AIS Targets");
    QVBoxLayout* tableLayout = new QVBoxLayout();
    targetsGroup->setLayout(tableLayout);

    targetsTable = new QTableWidget();
    targetsTable->setColumnCount(8);

    QStringList headers;
    headers << "MMSI" << "Distance" << "Bearing" << "CPA" << "TCPA" << "COG" << "SOG" << "Status";
    targetsTable->setHorizontalHeaderLabels(headers);

    // Set column widths - update untuk semua kolom
    targetsTable->setColumnWidth(0, 90);   // MMSI
    targetsTable->setColumnWidth(1, 85);   // Distance
    targetsTable->setColumnWidth(2, 80);   // Bearing
    targetsTable->setColumnWidth(3, 85);   // CPA
    targetsTable->setColumnWidth(4, 85);   // TCPA
    targetsTable->setColumnWidth(5, 60);   // COG
    targetsTable->setColumnWidth(6, 60);   // SOG
    targetsTable->setColumnWidth(7, 100);  // Status

    // Table properties
    targetsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    targetsTable->setAlternatingRowColors(true);
    targetsTable->setSortingEnabled(true);
    targetsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Set minimum height untuk table
    targetsTable->setMinimumHeight(200);

    connect(targetsTable, SIGNAL(itemSelectionChanged()), this, SLOT(onTargetSelected()));

    tableLayout->addWidget(targetsTable);
}

// Ganti nama method
void CPATCPAPanel::onTimerTimeout()
{
    if (ecWidget) {
        refreshData();
    }
}

void CPATCPAPanel::setEcWidget(EcWidget* widget)
{
    ecWidget = widget;
}

void CPATCPAPanel::refreshData()
{
    if (!ecWidget) return;

    // Update targets display
    updateTargetsDisplay();

    // Update last update time
    lastUpdateLabel->setText(QString("Last Update: %1").arg(QTime::currentTime().toString("hh:mm:ss")));
}

void CPATCPAPanel::updateTargetsDisplay()
{
    if (!ecWidget) return;

    // Update AIS targets list
    ecWidget->updateAISTargetsList();
    QList<AISTargetData> targets = ecWidget->getAISTargets();

    // Clear existing rows
    targetsTable->setRowCount(0);
    dangerousCount = 0;
    totalTargets = targets.size();

    int trackingCount = 0;
    double closestCPA = 999.0;
    double shortestTCPA = 999.0;

    // Add targets to table
    for (int i = 0; i < targets.size(); ++i) {
        const AISTargetData& target = targets[i];

        // Calculate CPA/TCPA
        VesselState ownShip;
        ownShip.lat = 29.4037;
        ownShip.lon = -94.7497;
        ownShip.cog = 90.0;
        ownShip.sog = 5.0;

        VesselState targetVessel;
        targetVessel.lat = target.lat;
        targetVessel.lon = target.lon;
        targetVessel.cog = target.cog;
        targetVessel.sog = target.sog;

        CPATCPACalculator calculator;
        CPATCPAResult result = calculator.calculateCPATCPA(ownShip, targetVessel);

        if (result.isValid) {
            trackingCount++;
            if (result.cpa < closestCPA) closestCPA = result.cpa;
            if (result.tcpa > 0 && result.tcpa < shortestTCPA) shortestTCPA = result.tcpa;
        }

        // Add row to table
        targetsTable->insertRow(i);
        updateTargetRow(i, target, result);

        // Count dangerous targets
        CPATCPASettings& settings = CPATCPASettings::instance();
        if (result.isValid &&
            ((settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) ||
             (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()))) {
            dangerousCount++;
        }
    }

    // Update status labels with detailed info
    activeTargetsLabel->setText(QString("Targets: %1 (Tracking: %2)").arg(totalTargets).arg(trackingCount));
    dangerousTargetsLabel->setText(QString("Dangerous: <font color='%2'>%1</font>")
                                       .arg(dangerousCount)
                                       .arg(dangerousCount > 0 ? "red" : "green"));

    // Add closest CPA info if available
    if (closestCPA < 999.0) {
        systemStatusLabel->setText(QString("System: <font color='green'>ACTIVE</font> | Closest CPA: %1").arg(formatDistance(closestCPA)));
    }
}

void CPATCPAPanel::updateTargetRow(int row, const AISTargetData& target, const CPATCPAResult& result)
{
    CPATCPASettings& settings = CPATCPASettings::instance();
    bool isDangerous = false;

    // Check if dangerous
    if (result.isValid) {
        if (settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) {
            isDangerous = true;
        }
        if (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()) {
            isDangerous = true;
        }
    }

    // Set row background untuk dangerous targets
    if (isDangerous) {
        for (int col = 0; col < 8; col++) {
            QTableWidgetItem* item = new QTableWidgetItem();
            item->setBackgroundColor(QColor(255, 230, 230)); // Light red background
            targetsTable->setItem(row, col, item);
        }
    }

    // MMSI
    QTableWidgetItem* mmsiItem = targetsTable->item(row, 0);
    mmsiItem->setText(target.mmsi);
    if (isDangerous) mmsiItem->setTextColor(QColor(139, 0, 0));

    // Distance
    QTableWidgetItem* distItem = targetsTable->item(row, 1);
    distItem->setText(formatDistance(result.currentRange));
    if (isDangerous) distItem->setTextColor(QColor(139, 0, 0));

    // Bearing
    QTableWidgetItem* bearingItem = targetsTable->item(row, 2);
    bearingItem->setText(QString("%1°").arg(result.relativeBearing, 0, 'f', 0));
    if (isDangerous) bearingItem->setTextColor(QColor(139, 0, 0));

    // CPA - dengan highlight khusus jika berbahaya
    QTableWidgetItem* cpaItem = targetsTable->item(row, 3);
    cpaItem->setText(formatDistance(result.cpa));
    if (result.isValid && settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) {
        cpaItem->setBackgroundColor(QColor(255, 150, 150)); // Darker red for CPA
        cpaItem->setTextColor(QColor(139, 0, 0));
        cpaItem->setFont(QFont("Arial", 8, QFont::Bold));
    }

    // TCPA - dengan highlight khusus jika berbahaya
    QTableWidgetItem* tcpaItem = targetsTable->item(row, 4);
    tcpaItem->setText(formatTime(result.tcpa));
    if (result.isValid && settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()) {
        tcpaItem->setBackgroundColor(QColor(255, 150, 150)); // Darker red for TCPA
        tcpaItem->setTextColor(QColor(139, 0, 0));
        tcpaItem->setFont(QFont("Arial", 8, QFont::Bold));
    }

    // COG
    QTableWidgetItem* cogItem = targetsTable->item(row, 5);
    cogItem->setText(QString("%1°").arg(target.cog, 0, 'f', 0));
    if (isDangerous) cogItem->setTextColor(QColor(139, 0, 0));

    // SOG
    QTableWidgetItem* sogItem = targetsTable->item(row, 6);
    sogItem->setText(QString("%1kt").arg(target.sog, 0, 'f', 1));
    if (isDangerous) sogItem->setTextColor(QColor(139, 0, 0));

    // Status dengan icon
    QString status = result.isValid ? "Tracking" : "No Data";
    if (isDangerous) {
        status = "⚠ DANGEROUS";
    }

    QTableWidgetItem* statusItem = targetsTable->item(row, 7);
    statusItem->setText(status);
    if (isDangerous) {
        statusItem->setBackgroundColor(QColor(255, 100, 100)); // Strong red
        statusItem->setTextColor(QColor(255, 255, 255)); // White text
        statusItem->setFont(QFont("Arial", 8, QFont::Bold));
    } else if (result.isValid) {
        statusItem->setBackgroundColor(QColor(200, 255, 200));
        statusItem->setTextColor(QColor(0, 100, 0));
    }
}

QString CPATCPAPanel::formatDistance(double nauticalMiles)
{
    if (nauticalMiles < 0.1) {
        return QString("%1 m").arg(nauticalMiles * 1852, 0, 'f', 0);
    } else {
        return QString("%1 NM").arg(nauticalMiles, 0, 'f', 2);
    }
}

QString CPATCPAPanel::formatTime(double minutes)
{
    if (minutes < 0) {
        return "N/A";
    } else if (minutes < 60) {
        return QString("%1 min").arg(minutes, 0, 'f', 1);
    } else {
        int hours = minutes / 60;
        int mins = minutes - (hours * 60);
        return QString("%1h %2m").arg(hours).arg(mins);
    }
}

void CPATCPAPanel::updateOwnShipInfo(double lat, double lon, double cog, double sog)
{
    ownShipLatLabel->setText(QString("Lat: %1°").arg(lat, 0, 'f', 4));
    ownShipLonLabel->setText(QString("Lon: %1°").arg(lon, 0, 'f', 4));
    ownShipCogLabel->setText(QString("COG: %1°").arg(cog, 0, 'f', 1));
    ownShipSogLabel->setText(QString("SOG: %1kt").arg(sog, 0, 'f', 1));
}

void CPATCPAPanel::onTargetSelected()
{
    int row = targetsTable->currentRow();
    if (row >= 0 && targetsTable->item(row, 0)) {
        QString mmsi = targetsTable->item(row, 0)->text();
        qDebug() << "Selected target MMSI:" << mmsi;
    }
}

void CPATCPAPanel::onSettingsClicked()
{
    CPATCPASettingsDialog* dialog = new CPATCPASettingsDialog(this);

    // Load current settings
    CPATCPASettings& settings = CPATCPASettings::instance();
    dialog->setCPAThreshold(settings.getCPAThreshold());
    dialog->setTCPAThreshold(settings.getTCPAThreshold());
    dialog->setCPAAlarmEnabled(settings.isCPAAlarmEnabled());
    dialog->setTCPAAlarmEnabled(settings.isTCPAAlarmEnabled());
    dialog->setVisualAlarmEnabled(settings.isVisualAlarmEnabled());
    dialog->setAudioAlarmEnabled(settings.isAudioAlarmEnabled());
    dialog->setAlarmUpdateInterval(settings.getAlarmUpdateInterval());

    if (dialog->exec() == QDialog::Accepted) {
        // Save new settings
        settings.setCPAThreshold(dialog->getCPAThreshold());
        settings.setTCPAThreshold(dialog->getTCPAThreshold());
        settings.setCPAAlarmEnabled(dialog->isCPAAlarmEnabled());
        settings.setTCPAAlarmEnabled(dialog->isTCPAAlarmEnabled());
        settings.setVisualAlarmEnabled(dialog->isVisualAlarmEnabled());
        settings.setAudioAlarmEnabled(dialog->isAudioAlarmEnabled());
        settings.setAlarmUpdateInterval(dialog->getAlarmUpdateInterval());
        settings.saveSettings();

        // Refresh display
        refreshData();
    }

    delete dialog;
}

void CPATCPAPanel::onClearAlarmsClicked()
{
    dangerousCount = 0;
    dangerousTargetsLabel->setText("Dangerous: <font color='green'>0</font>");

    // Clear highlighting in table
    for (int row = 0; row < targetsTable->rowCount(); ++row) {
        for (int col = 0; col < targetsTable->columnCount(); ++col) {
            QTableWidgetItem* item = targetsTable->item(row, col);
            if (item) {
                item->setBackgroundColor(QColor(255, 255, 255));
                item->setTextColor(QColor(0, 0, 0));
            }
        }
    }
}
