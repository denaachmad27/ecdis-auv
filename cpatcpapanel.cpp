#include <QDebug>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QPainter>

#include "cpatcpapanel.h"
#include "cpatcpasettingsdialog.h"
#include "ais.h"
#include "appconfig.h"
#include "SettingsManager.h"
#include <QScrollBar>
#include <QSignalBlocker>
#include <QHBoxLayout>
#include <QLabel>

CPATCPAPanel::CPATCPAPanel(QWidget *parent)
    : QWidget(parent)
    , ecWidget(nullptr)
    , dangerousCount(0)
    , totalTargets(0)
    , refreshTimer(nullptr)  // Ganti nama
{   
    setupUI();

    // Setup refresh timer (disabled; we'll sync to EcWidget 1 Hz tick)
    refreshTimer = new QTimer();
    refreshTimer->setParent(this);
    connect(refreshTimer, SIGNAL(timeout()), this, SLOT(onTimerTimeout()));
    // refreshTimer->start(1000);

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
    // Columns: 0 MMSI, 1 CPA, 2 TCPA, 3 Bearing, 4 Range, 5 Age, 6 Ship Name, 7 Status
    targetsTable->setColumnCount(8);

    QStringList headers;
    headers << "MMSI" << "CPA" << "TCPA" << "Bearing" << "Range" << "Age" << "Ship Name" << "Status";
    targetsTable->setHorizontalHeaderLabels(headers);

    // Set column widths - update untuk semua kolom
    targetsTable->setColumnWidth(0, 65);   // MMSI
    targetsTable->setColumnWidth(1, 60);   // CPA
    targetsTable->setColumnWidth(2, 55);   // TCPA
    targetsTable->setColumnWidth(3, 70);   // Bearing
    targetsTable->setColumnWidth(4, 70);   // Range
    targetsTable->setColumnWidth(5, 55);   // Age
    targetsTable->setColumnWidth(6, 140);  // Ship Name
    targetsTable->setColumnWidth(7, 95);   // Status

    //targetsTable->setColumnWidth(4, 85);   // TCPA
    //targetsTable->setColumnWidth(5, 60);   // COG
    //targetsTable->setColumnWidth(6, 60);   // SOG
    //targetsTable->setColumnWidth(7, 100);  // Status

    // Table properties
    targetsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    targetsTable->setAlternatingRowColors(true);
    targetsTable->setSortingEnabled(true);
    targetsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    targetsTable->setWordWrap(false);
    targetsTable->setTextElideMode(Qt::ElideRight);

    // Set minimum height untuk table
    targetsTable->setMinimumHeight(200);
    // Allow user-resizable columns
    targetsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    // We'll manage stretch per mode to auto-expand an appropriate column
    targetsTable->horizontalHeader()->setStretchLastSection(false);

    connect(targetsTable, SIGNAL(itemSelectionChanged()), this, SLOT(onTargetSelected()));
    connect(targetsTable->horizontalHeader(), &QHeaderView::sortIndicatorChanged,
            this, &CPATCPAPanel::onTableSorted);

    // Right-click context menu for follow/unfollow
    targetsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(targetsTable, &QTableWidget::customContextMenuRequested,
            this, &CPATCPAPanel::onTargetsContextMenuRequested);

    tableLayout->addWidget(targetsTable);

    // Custom delegate for Ship Name to elide per-character on the right
    class ShipNameElideDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;
        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            // Ensure single-line, elide-right behavior
            opt.features &= ~QStyleOptionViewItem::WrapText;
            QFontMetrics fm(opt.font);
            opt.text = fm.elidedText(opt.text, Qt::ElideRight, opt.rect.width());
            const QWidget *widget = opt.widget;
            QStyle *style = widget ? widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
        }
    };
    targetsTable->setItemDelegateForColumn(6, new ShipNameElideDelegate(targetsTable));

    // Toggle mode di bawah tabel
    QHBoxLayout* modeLayout = new QHBoxLayout();
    QLabel* modeLabel = new QLabel("Mode:");
    modeCpaTcpAButton = new QRadioButton("CPA/TCPA");
    modeBearingRangeButton = new QRadioButton("Bearing/Range");
    modeBearingRangeButton->setChecked(true);

    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(modeCpaTcpAButton);
    modeLayout->addWidget(modeBearingRangeButton);
    modeLayout->addStretch();
    tableLayout->addLayout(modeLayout);

    connect(modeCpaTcpAButton, &QRadioButton::toggled, this, [this](bool checked){
        if (checked) {
            displayMode = ModeCpaTcpa;
            applyColumnVisibility();
            refreshData();
        }
    });
    connect(modeBearingRangeButton, &QRadioButton::toggled, this, [this](bool checked){
        if (checked) {
            displayMode = ModeBearingRange;
            applyColumnVisibility();
            refreshData();
        }
    });

    applyColumnVisibility();
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
    if (!ecWidget) return;

    // Sync selection when tracking target changes from chart
    connect(ecWidget, &EcWidget::trackTargetChanged, this, [this](const QString& mmsi){
        if (!targetsTable) return;
        if (mmsi.isEmpty()) {
            // Clear selection if tracking cleared from chart
            targetsTable->clearSelection();
            selectedMmsi.clear();
            scrollToTrackedOnNextRefresh = false;
            return;
        }
        selectedMmsi = mmsi;
        scrollToTrackedOnNextRefresh = true; // scroll once on next refresh
    });

    // Sync refresh to EcWidget 1 Hz tick; stop local timer
    connect(ecWidget, SIGNAL(tickPerSecond()), this, SLOT(refreshData()));
    if (refreshTimer) refreshTimer->stop();
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
//             ((settings.isCPAAlarmEnabled() && result.cpa < SettingsManager::instance().data().cpaThreshold) ||
//              (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < SettingsManager::instance().data().tcpaThreshold))) {
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
    isRefreshing = true;
    QSignalBlocker blocker(targetsTable);
    if (!ecWidget || !targetsTable) return;

    // Simpan MMSI dari baris yang sedang terseleksi (gunakan var lokal berbeda agar tidak shadow member)
    QString currentSelectedMmsi;
    QItemSelectionModel *selectionModel = targetsTable->selectionModel();
    if (selectionModel->hasSelection()) {
        int selectedRow = selectionModel->selectedRows().first().row();
        QTableWidgetItem *mmsiItem = targetsTable->item(selectedRow, 0);
        if (mmsiItem)
            currentSelectedMmsi = mmsiItem->text();
    }

    // Simpan posisi scroll agar tidak lompat saat refresh
    int vScroll = targetsTable->verticalScrollBar()->value();
    int hScroll = targetsTable->horizontalScrollBar()->value();

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

    // Ambil daftar bahaya yang dihitung EcWidget pada tick yang sama
    QSet<QString> dangerousSet;
    for (const auto &d : ecWidget->getDangerousAISList()) dangerousSet.insert(d.mmsi);

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

        bool isDangerous = dangerousSet.contains(target.mmsi);
        EcAISTrackingStatus aisTrkStatusManual = isDangerous ? aisIntruder : aisInformationAvailable;
        if (isDangerous) dangerousCount++;

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

        if (ECOK(target.feat) && target._dictInfo) {
            EcAISSetTargetTrackingStatus(target.feat, target._dictInfo, aisTrkStatusManual, NULL );
        }

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
                // Jika diminta scroll (akibat trackTargetChanged), lakukan sekali
                if (scrollToTrackedOnNextRefresh) {
                    targetsTable->setCurrentItem(item);
                    targetsTable->selectRow(row);
                    targetsTable->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                    scrollToTrackedOnNextRefresh = false;
                } else {
                    // Pilih tanpa mengubah current item agar tidak memicu scroll otomatis
                    QItemSelectionModel* sel = targetsTable->selectionModel();
                    if (sel) {
                        QModelIndex idx = targetsTable->model()->index(row, 0);
                        sel->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                    }
                }
                break;
            }
        }
    }

    // Pulihkan posisi scroll agar tidak mengganggu user saat melihat data lain
    if (!scrollToTrackedOnNextRefresh) {
        targetsTable->verticalScrollBar()->setValue(vScroll);
        targetsTable->horizontalScrollBar()->setValue(hScroll);
    }

    // Update label status
    activeTargetsLabel->setText(QString("Targets: %1 (Tracking: %2)").arg(totalTargets).arg(trackingCount));
    dangerousTargetsLabel->setText(QString("Dangerous: <font color='%2'>%1</font>")
                                       .arg(dangerousCount)
                                       .arg(dangerousCount > 0 ? "red" : "green"));

    if (closestCPA < 999.0) {
        systemStatusLabel->setText(QString("System: <font color='green'>ACTIVE</font> | Closest CPA: %1").arg(formatDistance(closestCPA)));
    }

    isRefreshing = false;
}

void CPATCPAPanel::updateTargetRow(int row, const AISTargetData& target, const CPATCPAResult& result)
{
    CPATCPASettings& settings = CPATCPASettings::instance();
    bool isDangerous = false;

    // if (result.isValid) {
    //     if (settings.isCPAAlarmEnabled() && result.cpa < SettingsManager::instance().data().cpaThreshold) isDangerous = true;
    //     if (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < SettingsManager::instance().data().tcpaThreshold) isDangerous = true;
    // }

    if (result.isValid && result.currentRange < 0.5) {
        if (settings.isCPAAlarmEnabled() && result.cpa < SettingsManager::instance().data().cpaThreshold) isDangerous = true;
        if (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < SettingsManager::instance().data().tcpaThreshold) isDangerous = true;
    }

    auto formatBearing = [](double deg){ return QString("%1°").arg(deg, 0, 'f', 1); };
    auto formatAge = [](const QDateTime& last){
        if (!last.isValid()) return QString("--:--");
        qint64 secs = last.secsTo(QDateTime::currentDateTime());
        if (secs < 0) secs = 0;
        qint64 m = secs / 60;
        qint64 s = secs % 60;
        return QString("%1:%2")
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    };

    QStringList values;
    QString shipName = QString(target.rawInfo.shipName).trimmed();
    if (shipName.isEmpty()) shipName = target.mmsi;
    QString statusText;
    if (isDangerous) {
        statusText = "⚠ DANGEROUS";
    } else {
        switch (result.status) {
            case CPATCPAResult::Valid: statusText = "Tracking"; break;
            case CPATCPAResult::StationaryRelative: statusText = "Stationary"; break;
            case CPATCPAResult::Diverging: statusText = "Diverging"; break;
            case CPATCPAResult::OutOfRange: statusText = "Out of Range"; break;
            case CPATCPAResult::InvalidMotionData:
            default: statusText = "No Data"; break;
        }
    }

    values << target.mmsi
           << formatDistance(result.cpa)
           << formatTime(result.tcpa)
           << formatBearing(result.relativeBearing)
           << formatDistance(result.currentRange)
           << formatAge(target.lastUpdate)
           << shipName
           << statusText;

    for (int col = 0; col < values.size(); ++col) {
        QTableWidgetItem* item = new QTableWidgetItem(values[col]);
        if (col == 6) {
            // Tooltip with full ship name (not elided)
            item->setToolTip(shipName);
        }

        // Warna merah untuk dangerous
        if (isDangerous) {
            item->setTextColor(QColor(139, 0, 0));
            item->setBackgroundColor(QColor(255, 230, 230));
        }

        // Highlight khusus CPA
        if (col == 1 && result.isValid && settings.isCPAAlarmEnabled() && result.cpa < SettingsManager::instance().data().cpaThreshold) {
            item->setBackgroundColor(QColor(255, 150, 150));
            item->setFont(QFont("Arial", 8, QFont::Bold));
        }

        // Highlight khusus TCPA
        if (col == 2 && result.isValid && settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < SettingsManager::instance().data().tcpaThreshold) {
            item->setBackgroundColor(QColor(255, 150, 150));
            item->setFont(QFont("Arial", 8, QFont::Bold));
        }

        // Highlight status
        if (col == 7) {
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

void CPATCPAPanel::applyColumnVisibility()
{
    if (!targetsTable) return;

    // Columns: 0 MMSI, 1 CPA, 2 TCPA, 3 Bearing, 4 Range, 5 Age, 6 Ship Name, 7 Status
    const bool isCpaMode = (displayMode == ModeCpaTcpa);
    // CPA/TCPA mode shows MMSI, CPA, TCPA, Status
    targetsTable->setColumnHidden(0, !isCpaMode); // MMSI visible only in CPA mode
    targetsTable->setColumnHidden(1, !isCpaMode);
    targetsTable->setColumnHidden(2, !isCpaMode);
    targetsTable->setColumnHidden(7, !isCpaMode);
    // Hide Bearing/Range/Age/Ship Name in CPA mode
    targetsTable->setColumnHidden(3, isCpaMode);
    targetsTable->setColumnHidden(4, isCpaMode);
    targetsTable->setColumnHidden(5, isCpaMode);
    targetsTable->setColumnHidden(6, isCpaMode);

    // Bearing & Range mode shows Ship Name, Bearing, Range, Age (no MMSI)
    if (!isCpaMode) {
        targetsTable->setColumnHidden(0, true);   // hide MMSI in this mode
        targetsTable->setColumnHidden(3, false);
        targetsTable->setColumnHidden(4, false);
        targetsTable->setColumnHidden(5, false);
        targetsTable->setColumnHidden(6, false);
        // Hide CPA/TCPA/Status
        targetsTable->setColumnHidden(1, true);
        targetsTable->setColumnHidden(2, true);
        targetsTable->setColumnHidden(7, true);
    }

    // Apply visual order per mode
    if (isCpaMode) {
        // MMSI, CPA, TCPA, Status
        reorderColumns({0,1,2,7});
        // Stretch Status to fill space
        QHeaderView* header = targetsTable->horizontalHeader();
        header->setSectionResizeMode(QHeaderView::Interactive);
        header->setSectionResizeMode(7, QHeaderView::Stretch);
        header->setSectionResizeMode(0, QHeaderView::Interactive);
        header->setSectionResizeMode(1, QHeaderView::Interactive);
        header->setSectionResizeMode(2, QHeaderView::Interactive);
    } else {
        // Ship Name, Bearing, Range, Age (MMSI hidden)
        reorderColumns({6,3,4,5});
        // Stretch Ship Name to fill space
        QHeaderView* header = targetsTable->horizontalHeader();
        header->setSectionResizeMode(QHeaderView::Interactive);
        header->setSectionResizeMode(6, QHeaderView::Stretch);
        header->setSectionResizeMode(3, QHeaderView::Interactive);
        header->setSectionResizeMode(4, QHeaderView::Interactive);
        header->setSectionResizeMode(5, QHeaderView::Interactive);
    }
}

void CPATCPAPanel::reorderColumns(const QList<int>& logicalOrder)
{
    if (!targetsTable) return;
    QHeaderView* header = targetsTable->horizontalHeader();
    for (int visualIndex = 0; visualIndex < logicalOrder.size(); ++visualIndex) {
        int logical = logicalOrder[visualIndex];
        int currentVisual = header->visualIndex(logical);
        if (currentVisual != visualIndex) {
            header->moveSection(currentVisual, visualIndex);
        }
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
    if (isRefreshing) return;

    int row = targetsTable->currentRow();
    if (row >= 0) {
        QTableWidgetItem* mmsiItem = targetsTable->item(row, 0);
        selectedMmsi = mmsiItem ? mmsiItem->text() : QString();
        // Do not auto-follow on selection; follow via context menu
    } else {
        // No selection (clicked empty area) -> stop tracking on chart
        selectedMmsi.clear();
        // Do not auto-unfollow on deselect; keep current follow state until user chooses
    }
}

void CPATCPAPanel::onTargetsContextMenuRequested(const QPoint& pos)
{
    if (!ecWidget) return;
    QModelIndex idx = targetsTable->indexAt(pos);
    if (!idx.isValid()) return;

    int row = idx.row();
    QTableWidgetItem* mmsiItem = targetsTable->item(row, 0);
    if (!mmsiItem) return;
    QString mmsi = mmsiItem->text();
    if (mmsi.isEmpty()) return;

    QMenu menu(this);
    bool isTracking = ecWidget->isTrackTarget() && (ecWidget->getTrackMMSI() == mmsi);
    QAction* followAction = nullptr;
    if (isTracking) {
        followAction = menu.addAction(tr("Unfollow"));
    } else {
        followAction = menu.addAction(tr("Follow"));
    }

    QAction* chosen = menu.exec(targetsTable->viewport()->mapToGlobal(pos));
    if (chosen == followAction) {
        if (isTracking) {
            ecWidget->TrackTarget("");
        } else {
            // Set track target and optionally seed last known position
            QMap<unsigned int, AISTargetData>& targets = Ais::instance()->getTargetMap();
            unsigned int mmsiInt = mmsi.toUInt();
            if (targets.contains(mmsiInt)) {
                const AISTargetData& td = targets[mmsiInt];
                AISTargetData track; track.mmsi = mmsi; track.lat = td.lat; track.lon = td.lon;
                ecWidget->setAISTrack(track);
            }
            ecWidget->TrackTarget(mmsi);
        }
    }
}

void CPATCPAPanel::onSettingsClicked()
{
    CPATCPASettingsDialog* dialog = new CPATCPASettingsDialog(this);

    // Load current settings
    CPATCPASettings& settings = CPATCPASettings::instance();
    dialog->setCPAThreshold(SettingsManager::instance().data().cpaThreshold);
    dialog->setTCPAThreshold(SettingsManager::instance().data().tcpaThreshold);
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

