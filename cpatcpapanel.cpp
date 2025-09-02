#include <QDebug>
#include <QHeaderView>

#include "cpatcpapanel.h"
#include "cpatcpasettingsdialog.h"
#include "ais.h"
#include "appconfig.h"

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

    // Update setiap interval detik
    CPATCPASettings& settings = CPATCPASettings::instance();
    //refreshTimer->start(settings.getAlarmUpdateInterval() * 1000);

    refreshTimer->start(60 * 1000);

    if (AppConfig::isDevelopment()){
        // Add tooltips
        refreshButton->setToolTip("Refresh AIS targets data (F5)");
        settingsButton->setToolTip("Open CPA/TCPA settings dialog");
        clearAlarmsButton->setToolTip("Clear all alarm highlights");

        // Add keyboard shortcuts
        refreshButton->setShortcut(QKeySequence("F5"));
        settingsButton->setShortcut(QKeySequence("Ctrl+T"));
    }
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
    // ownShipGroup = new QGroupBox("Own Ship");
    // QGridLayout* ownShipLayout = new QGridLayout();
    // ownShipGroup->setLayout(ownShipLayout);

    ownShipLatLabel = new QLabel("LAT: 0");
    ownShipLonLabel = new QLabel("LON: 0");
    ownShipCogLabel = new QLabel("COG: 0");
    ownShipSogLabel = new QLabel("SOG: 0");

    // ownShipLayout->addWidget(new QLabel("Position:"), 0, 0);
    // ownShipLayout->addWidget(ownShipLatLabel, 0, 1);
    // ownShipLayout->addWidget(ownShipLonLabel, 0, 2);
    // ownShipLayout->addWidget(new QLabel("Course/Speed:"), 1, 0);
    // ownShipLayout->addWidget(ownShipCogLabel, 1, 1);
    // ownShipLayout->addWidget(ownShipSogLabel, 1, 2);

    // Targets Table Panel
    setupTargetsTable();

    // Control Buttons
    if (AppConfig::isDevelopment()){
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
        mainLayout->addLayout(buttonLayout);
    }

    mainLayout->addWidget(statusGroup);
    mainLayout->addWidget(ownShipGroup);
    mainLayout->addWidget(targetsGroup);
}

void CPATCPAPanel::setupStatusPanel()
{
    statusGroup = new QGroupBox("CPA/TCPA Status");
    QGridLayout* statusLayout = new QGridLayout();
    statusGroup->setLayout(statusLayout);

    systemStatusLabel = new QLabel("System: <font color='grey'>INACTIVE</font>");
    activeTargetsLabel = new QLabel("Targets: 0");
    dangerousTargetsLabel = new QLabel("Dangerous: <font color='red'>0</font>");
    lastUpdateLabel = new QLabel("Last Update: --");

    statusLayout->addWidget(systemStatusLabel, 0, 0);
    statusLayout->addWidget(activeTargetsLabel, 0, 1);
    statusLayout->addWidget(dangerousTargetsLabel, 1, 0);
    statusLayout->addWidget(lastUpdateLabel, 1, 1);

    systemStatusLabel->setFixedWidth(190);
    activeTargetsLabel->setFixedWidth(150);
    dangerousTargetsLabel->setFixedWidth(190);
    lastUpdateLabel->setFixedWidth(150);
}

void CPATCPAPanel::setupTargetsTable()
{
    targetsGroup = new QGroupBox("AIS Targets");
    QVBoxLayout* tableLayout = new QVBoxLayout();
    targetsGroup->setLayout(tableLayout);

    targetsTable = new QTableWidget();
    targetsTable->setColumnCount(4);

    QStringList headers;
    //headers << "MMSI" << "Distance" << "Bearing" << "CPA" << "TCPA" << "COG" << "SOG" << "Status";
    headers << "MMSI" << "CPA" << "TCPA" << "Status";
    targetsTable->setHorizontalHeaderLabels(headers);

    // Set column widths - update untuk semua kolom
    targetsTable->setColumnWidth(0, 65);   // MMSI
    targetsTable->setColumnWidth(1, 60);   // CPA
    targetsTable->setColumnWidth(2, 55);   // TCPA
    targetsTable->setColumnWidth(3, 95);   // Status

    //targetsTable->setColumnWidth(4, 85);   // TCPA
    //targetsTable->setColumnWidth(5, 60);   // COG
    //targetsTable->setColumnWidth(6, 60);   // SOG
    //targetsTable->setColumnWidth(7, 100);  // Status

    // Table properties
    targetsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    targetsTable->setAlternatingRowColors(true);
    targetsTable->setSortingEnabled(true);
    targetsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Set minimum height untuk table
    targetsTable->setMinimumHeight(200);
    targetsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    targetsTable->horizontalHeader()->setStretchLastSection(false);

    connect(targetsTable, SIGNAL(itemSelectionChanged()), this, SLOT(onTargetSelected()));
    connect(targetsTable->horizontalHeader(), &QHeaderView::sortIndicatorChanged,
            this, &CPATCPAPanel::onTableSorted);

    tableLayout->addWidget(targetsTable);
}

void CPATCPAPanel::onTableSorted(int column, Qt::SortOrder order)
{
    Q_UNUSED(column)
    Q_UNUSED(order)

    // Sorting sudah dilakukan otomatis oleh QTableWidget
    // Sekarang pulihkan seleksi berdasarkan MMSI
    if (!selectedMmsi.isEmpty()) {
        for (int row = 0; row < targetsTable->rowCount(); ++row) {
            QTableWidgetItem *item = targetsTable->item(row, 0); // Kolom MMSI
            if (item && item->text() == selectedMmsi) {
                targetsTable->setCurrentItem(item);
                targetsTable->selectRow(row);
                break;
            }
        }
    }
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

/*
// void CPATCPAPanel::updateTargetsDisplay()
// {
//     if (!ecWidget || !targetsTable) return;

//     // Ambil data AIS target dari sistem
//     ecWidget->updateAISTargetsList();
//     QMap<unsigned int, AISTargetData> targets = Ais::instance()->getTargetMap();

//     //QList<AISTargetData> targets = ecWidget->getAISTargets();

//     // Variabel status
//     dangerousCount = 0;
//     totalTargets = targets.size();
//     int trackingCount = 0;
//     double closestCPA = 999.0;
//     double shortestTCPA = 999.0;

//     // Iterasi setiap target
//     for (const auto &target : targets) {

//         //if (target.mmsi != "367193790") continue;

//         VesselState ownShip;
//         ownShip.lat = navShip.lat;
//         ownShip.lon = navShip.lon;
//         ownShip.sog = navShip.speed_og;
//         ownShip.cog = navShip.heading_og;

//         VesselState targetVessel;
//         targetVessel.lat = target.lat;
//         targetVessel.lon = target.lon;
//         targetVessel.cog = target.cog;
//         targetVessel.sog = target.sog;

//         // Calculate CPA/TCPA
//         CPATCPACalculator calculator;
//         CPATCPAResult result = calculator.calculateCPATCPA(ownShip, targetVessel);

//         // Hitung tracking dan nilai CPA terdekat
//         if (result.isValid) {
//             trackingCount++;
//             if (result.cpa < closestCPA) closestCPA = result.cpa;
//             if (result.tcpa > 0 && result.tcpa < shortestTCPA) shortestTCPA = result.tcpa;
//         }

//         // Insert row at the end
//         int row = targetsTable->rowCount();
//         targetsTable->insertRow(row);
//         updateTargetRow(row, target, result);

//         // Count dangerous targets
//         CPATCPASettings& settings = CPATCPASettings::instance();
//         if (result.isValid &&
//             ((settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) ||
//              (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()))) {
//             dangerousCount++;
//         }
//     }

//     // Update status labels with detailed info
//     activeTargetsLabel->setText(QString("Targets: %1 (Tracking: %2)").arg(totalTargets).arg(trackingCount));
//     dangerousTargetsLabel->setText(QString("Dangerous: <font color='%2'>%1</font>")
//                                        .arg(dangerousCount)
//                                        .arg(dangerousCount > 0 ? "red" : "green"));

//     // Add closest CPA info if available
//     if (closestCPA < 999.0) {
//         systemStatusLabel->setText(QString("System: <font color='green'>ACTIVE</font> | Closest CPA: %1").arg(formatDistance(closestCPA)));
//     }
// }
*/

void CPATCPAPanel::updateTargetsDisplay()
{
    ecWidget->clearDangerousAISList();

    if (!ecWidget || !targetsTable) return;

    // Simpan MMSI dari baris yang sedang terseleksi
    QString selectedMmsi;
    QItemSelectionModel *selectionModel = targetsTable->selectionModel();
    if (selectionModel->hasSelection()) {
        int selectedRow = selectionModel->selectedRows().first().row();
        QTableWidgetItem *mmsiItem = targetsTable->item(selectedRow, 0);
        if (mmsiItem)
            selectedMmsi = mmsiItem->text();
    }

    // Bersihkan tabel
    targetsTable->setSortingEnabled(false);
    targetsTable->setRowCount(0);

    // Ambil data AIS target dari sistem
    //ecWidget->updateAISTargetsList();
    QMap<unsigned int, AISTargetData> targets = Ais::instance()->getTargetMap();

    // Variabel status
    dangerousCount = 0;
    totalTargets = targets.size();
    int trackingCount = 0;
    double closestCPA = 999.0;
    double shortestTCPA = 999.0;
    double targetCPA = 999.0;

    struct TargetWithResult {
        AISTargetData target;
        CPATCPAResult result;
        bool isDangerous;
    };

    AISTargetData closestAIS;
    AISTargetData dangerousAIS;

    QList<TargetWithResult> sortedList;
    for (const auto &target : targets) {
        //if (target.mmsi != "367159080" && target.mmsi != "366973590" && target.mmsi != "366996240") continue;

        VesselState ownShip;
        ownShip.lat = Ais::instance()->getOwnShipVar().lat;
        ownShip.lon = Ais::instance()->getOwnShipVar().lon;
        ownShip.sog = Ais::instance()->getOwnShipVar().sog;
        ownShip.cog = Ais::instance()->getOwnShipVar().cog;

        VesselState targetVessel;
        targetVessel.lat = target.lat;
        targetVessel.lon = target.lon;
        targetVessel.sog = target.sog;
        targetVessel.cog = target.cog;

        // Hitung CPA/TCPA
        CPATCPACalculator calculator;
        CPATCPAResult result = calculator.calculateCPATCPA(ownShip, targetVessel);

        bool isDangerous = false;
        CPATCPASettings& settings = CPATCPASettings::instance();
        EcAISTrackingStatus aisTrkStatusManual;

        if (result.isValid && result.currentRange < 0.5 &&
            ((settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) ||
             (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()))) {
            isDangerous = true;
            dangerousCount++;
            aisTrkStatusManual = aisIntruder;
        }
        else {
            aisTrkStatusManual = aisInformationAvailable;
        }

        if (isDangerous){
            dangerousAIS.mmsi = target.mmsi;
            dangerousAIS.lat = target.lat;
            dangerousAIS.lon = target.lon;

            ecWidget->addDangerousAISTarget(dangerousAIS);
        }

        if (result.isValid) {
            trackingCount++;
            if (result.cpa < closestCPA) {
                closestCPA = result.cpa;
            }
            if (result.tcpa > 0 && result.tcpa < shortestTCPA) shortestTCPA = result.tcpa;
        }

        sortedList.append({target, result, isDangerous});

        // Set the tracking status of the ais target feature
        EcAISSetTargetTrackingStatus(target.feat, target._dictInfo, aisTrkStatusManual, NULL );

        //ecWidget->drawShipGuardianSquare(target.lat, target.lon);
    }

    // Urutkan: yang dangerous di atas
    // std::sort(sortedList.begin(), sortedList.end(), [](const TargetWithResult &a, const TargetWithResult &b) {
    //     return a.isDangerous > b.isDangerous;
    // });

    // Tambahkan ke tabel
    for (const auto &entry : sortedList) {
        int row = targetsTable->rowCount();
        targetsTable->insertRow(row);
        updateTargetRow(row, entry.target, entry.result);
    }

    // Pulihkan seleksi MMSI jika ada
    if (!selectedMmsi.isEmpty()) {
        for (int row = 0; row < targetsTable->rowCount(); ++row) {
            QTableWidgetItem *item = targetsTable->item(row, 0); // Kolom MMSI
            if (item && item->text() == selectedMmsi) {
                targetsTable->setCurrentItem(item);
                targetsTable->selectRow(row);
                break;
            }
        }
    }

    // Update label status
    activeTargetsLabel->setText(QString("Targets: %1 (Tracking: %2)").arg(totalTargets).arg(trackingCount));
    dangerousTargetsLabel->setText(QString("Dangerous: <font color='%2'>%1</font>")
                                       .arg(dangerousCount)
                                       .arg(dangerousCount > 0 ? "red" : "green"));

    if (closestCPA < 999.0) {
        systemStatusLabel->setText(QString("System: <font color='green'>ACTIVE</font> | Closest CPA: %1").arg(formatDistance(closestCPA)));
    }
}

void CPATCPAPanel::updateTargetRow(int row, const AISTargetData& target, const CPATCPAResult& result)
{
    CPATCPASettings& settings = CPATCPASettings::instance();
    bool isDangerous = false;

    // if (result.isValid) {
    //     if (settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) isDangerous = true;
    //     if (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()) isDangerous = true;
    // }

    if (result.isValid && result.currentRange < 0.5) {
        if (settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) isDangerous = true;
        if (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()) isDangerous = true;
    }

    QStringList values;
    values << target.mmsi
           << formatDistance(result.cpa)
           << formatTime(result.tcpa)
           << (isDangerous ? "⚠ DANGEROUS" : (result.isValid ? "Tracking" : "No Data"));

    for (int col = 0; col < values.size(); ++col) {
        QTableWidgetItem* item = new QTableWidgetItem(values[col]);

        // Warna merah untuk dangerous
        if (isDangerous) {
            item->setTextColor(QColor(139, 0, 0));
            item->setBackgroundColor(QColor(255, 230, 230));
        }

        // Highlight khusus CPA
        if (col == 1 && result.isValid && settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) {
            item->setBackgroundColor(QColor(255, 150, 150));
            item->setFont(QFont("Arial", 8, QFont::Bold));
        }

        // Highlight khusus TCPA
        if (col == 2 && result.isValid && settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()) {
            item->setBackgroundColor(QColor(255, 150, 150));
            item->setFont(QFont("Arial", 8, QFont::Bold));
        }

        // Highlight status
        if (col == 3) {
            if (isDangerous) {
                item->setBackgroundColor(QColor(255, 100, 100));
                item->setTextColor(Qt::white);
                item->setFont(QFont("Arial", 8, QFont::Bold));
            } else if (result.isValid) {
                item->setBackgroundColor(QColor(200, 255, 200));
                item->setTextColor(QColor(0, 100, 0));
            }
        }

        targetsTable->setItem(row, col, item);
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

void CPATCPAPanel::updateOwnShipInfo(double lat, double lon, double sog, double cog)
{
    ownShipLatLabel->setText(QString("LAT: <b>%1°</b>").arg(lat, 0, 'f', 4));
    ownShipLonLabel->setText(QString("LON: <b>%1°</b>").arg(lon, 0, 'f', 4));
    ownShipSogLabel->setText(QString("COG: <b>%1°</b>").arg(cog, 0, 'f', 1));
    ownShipCogLabel->setText(QString("SOG: <b>%1kt</b>").arg(sog, 0, 'f', 1));
}

void CPATCPAPanel::onTargetSelected()
{
    // int row = targetsTable->currentRow();
    // if (row >= 0 && targetsTable->item(row, 0)) {
    //     QString mmsi = targetsTable->item(row, 0)->text();
    //     //qDebug() << "Selected target MMSI:" << mmsi;
    // }

    QList<QTableWidgetItem*> selectedItems = targetsTable->selectedItems();
    if (!selectedItems.isEmpty()) {
        selectedMmsi = selectedItems.first()->text();  // Kolom MMSI harus kolom 0
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
