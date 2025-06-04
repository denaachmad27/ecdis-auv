#include "guardzonepanel.h"
#include "ecwidget.h"
#include "guardzonemanager.h"
#include <QDebug>

// ========== GuardZoneListItem Implementation ==========
GuardZoneListItem::GuardZoneListItem(const GuardZone& guardZone, QListWidget* parent)
    : QListWidgetItem(parent), guardZoneId(guardZone.id)
{
    qDebug() << "Creating GuardZoneListItem for" << guardZone.name << "Active:" << guardZone.active;

    // Set flags dulu sebelum update
    setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);

    // PERBAIKAN: Set checkbox state SEBELUM updateFromGuardZone
    setCheckState(guardZone.active ? Qt::Checked : Qt::Unchecked);

    // Kemudian update display
    updateFromGuardZone(guardZone);

    qDebug() << "GuardZoneListItem created - CheckState:" << (guardZone.active ? "Checked" : "Unchecked");
}

void GuardZoneListItem::updateFromGuardZone(const GuardZone& guardZone)
{
    guardZoneId = guardZone.id;

    // Update display text
    updateDisplayText(guardZone);

    // PERBAIKAN: Jangan set checkbox state di sini, biar yang manggil yang handle
    // Cuma update visual properties lainnya

    // Set item color based on guardzone color
    QColor itemColor = guardZone.color;
    itemColor.setAlpha(100);
    setBackground(QBrush(itemColor));
}

void GuardZoneListItem::updateDisplayText(const GuardZone& guardZone)
{
    QString shapeText = (guardZone.shape == GUARD_ZONE_CIRCLE) ? "Circle" : "Polygon";
    QString statusText = guardZone.active ? "Active" : "Inactive";
    QString attachText = guardZone.attachedToShip ? " [Ship]" : "";

    QString displayText = QString("%1 (%2) - %3%4")
                              .arg(guardZone.name)
                              .arg(shapeText)
                              .arg(statusText)
                              .arg(attachText);

    setText(displayText);

    // ========== PERBAIKAN: PASTIKAN CHECKBOX STATE DI SINI JUGA ==========
    setCheckState(guardZone.active ? Qt::Checked : Qt::Unchecked);
    // ===================================================================

    // Set tooltip with detailed info
    QString tooltip = QString("ID: %1\nName: %2\nShape: %3\nActive: %4\nAttached to Ship: %5\nColor: %6")
                          .arg(guardZone.id)
                          .arg(guardZone.name)
                          .arg(shapeText)
                          .arg(guardZone.active ? "Yes" : "No")
                          .arg(guardZone.attachedToShip ? "Yes" : "No")
                          .arg(guardZone.color.name());

    if (guardZone.shape == GUARD_ZONE_CIRCLE) {
        tooltip += QString("\nRadius: %1 NM").arg(guardZone.radius, 0, 'f', 2);
    } else {
        tooltip += QString("\nVertices: %1").arg(guardZone.latLons.size() / 2);
    }

    setToolTip(tooltip);
}

// ========== GuardZonePanel Implementation ==========
GuardZonePanel::GuardZonePanel(EcWidget* ecWidget, GuardZoneManager* manager, QWidget *parent)
    : QWidget(parent), ecWidget(ecWidget), guardZoneManager(manager)
{
    setupUI();
    setupConnections();
    //refreshGuardZoneList();
}

GuardZonePanel::~GuardZonePanel()
{
}

void GuardZonePanel::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // ========== INFO SECTION ==========
    infoGroup = new QGroupBox("GuardZone Summary", this);
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);

    totalLabel = new QLabel("Total: 0", infoGroup);
    activeLabel = new QLabel("Active: 0", infoGroup);
    inactiveLabel = new QLabel("Inactive: 0", infoGroup);

    infoLayout->addWidget(totalLabel);
    infoLayout->addWidget(activeLabel);
    infoLayout->addWidget(inactiveLabel);

    mainLayout->addWidget(infoGroup);

    // ========== FILTER SECTION ==========
    filterGroup = new QGroupBox("Filters", this);
    QVBoxLayout* filterLayout = new QVBoxLayout(filterGroup);

    // Search box
    searchEdit = new QLineEdit(filterGroup);
    searchEdit->setPlaceholderText("Search by name...");
    filterLayout->addWidget(searchEdit);

    // Visibility filters
    QHBoxLayout* visibilityLayout = new QHBoxLayout();
    showActiveCheck = new QCheckBox("Show Active", filterGroup);
    showActiveCheck->setChecked(true);
    showInactiveCheck = new QCheckBox("Show Inactive", filterGroup);
    showInactiveCheck->setChecked(true);

    visibilityLayout->addWidget(showActiveCheck);
    visibilityLayout->addWidget(showInactiveCheck);
    filterLayout->addLayout(visibilityLayout);

    // Shape filter
    shapeFilterCombo = new QComboBox(filterGroup);
    shapeFilterCombo->addItem("All Shapes");
    shapeFilterCombo->addItem("Circle Only");
    shapeFilterCombo->addItem("Polygon Only");
    filterLayout->addWidget(shapeFilterCombo);

    mainLayout->addWidget(filterGroup);

    // ========== LIST SECTION ==========
    listGroup = new QGroupBox("GuardZone List", this);
    QVBoxLayout* listLayout = new QVBoxLayout(listGroup);

    guardZoneList = new QListWidget(listGroup);
    guardZoneList->setContextMenuPolicy(Qt::CustomContextMenu);
    guardZoneList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listLayout->addWidget(guardZoneList);

    mainLayout->addWidget(listGroup);

    // ========== BULK OPERATIONS SECTION ==========
    bulkGroup = new QGroupBox("Bulk Operations", this);
    QVBoxLayout* bulkLayout = new QVBoxLayout(bulkGroup);

    // Selection buttons
    QHBoxLayout* selectionLayout = new QHBoxLayout();
    selectAllBtn = new QPushButton("Select All", bulkGroup);
    selectNoneBtn = new QPushButton("Select None", bulkGroup);
    selectionLayout->addWidget(selectAllBtn);
    selectionLayout->addWidget(selectNoneBtn);
    bulkLayout->addLayout(selectionLayout);

    // Operation buttons
    QHBoxLayout* operationLayout = new QHBoxLayout();
    toggleActiveBtn = new QPushButton("Toggle Active", bulkGroup);
    changeColorBtn = new QPushButton("Change Color", bulkGroup);
    operationLayout->addWidget(toggleActiveBtn);
    operationLayout->addWidget(changeColorBtn);
    bulkLayout->addLayout(operationLayout);

    // Delete button
    deleteSelectedBtn = new QPushButton("Delete Selected", bulkGroup);
    deleteSelectedBtn->setStyleSheet("QPushButton { background-color: #ff6666; color: white; }");
    bulkLayout->addWidget(deleteSelectedBtn);

    mainLayout->addWidget(bulkGroup);

    // ========== IMPORT/EXPORT SECTION ==========
    importExportGroup = new QGroupBox("Import/Export", this);
    QVBoxLayout* importExportLayout = new QVBoxLayout(importExportGroup);

    QHBoxLayout* exportLayout = new QHBoxLayout();
    exportSelectedBtn = new QPushButton("Export Selected", importExportGroup);
    exportAllBtn = new QPushButton("Export All", importExportGroup);
    exportLayout->addWidget(exportSelectedBtn);
    exportLayout->addWidget(exportAllBtn);
    importExportLayout->addLayout(exportLayout);

    importBtn = new QPushButton("Import GuardZones", importExportGroup);
    importExportLayout->addWidget(importBtn);

    mainLayout->addWidget(importExportGroup);

    // Add stretch to push everything to top
    mainLayout->addStretch();
}

void GuardZonePanel::setupConnections()
{
    // List operations
    connect(guardZoneList, &QListWidget::itemSelectionChanged, this, &GuardZonePanel::onItemSelectionChanged);
    connect(guardZoneList, &QListWidget::itemDoubleClicked, this, &GuardZonePanel::onItemDoubleClicked);
    connect(guardZoneList, &QListWidget::itemChanged, this, &GuardZonePanel::onItemChanged);
    connect(guardZoneList, &QListWidget::customContextMenuRequested, this, &GuardZonePanel::onItemContextMenu);

    // Filter operations
    connect(searchEdit, &QLineEdit::textChanged, this, &GuardZonePanel::onFilterChanged);
    connect(showActiveCheck, &QCheckBox::toggled, this, &GuardZonePanel::onShowActiveChanged);
    connect(showInactiveCheck, &QCheckBox::toggled, this, &GuardZonePanel::onShowInactiveChanged);
    connect(shapeFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GuardZonePanel::onFilterChanged);

    // Bulk operations
    connect(selectAllBtn, &QPushButton::clicked, this, &GuardZonePanel::onSelectAll);
    connect(selectNoneBtn, &QPushButton::clicked, this, &GuardZonePanel::onSelectNone);
    connect(toggleActiveBtn, &QPushButton::clicked, this, &GuardZonePanel::onToggleSelectedActive);
    connect(deleteSelectedBtn, &QPushButton::clicked, this, &GuardZonePanel::onDeleteSelected);
    connect(changeColorBtn, &QPushButton::clicked, this, &GuardZonePanel::onChangeSelectedColor);

    // Import/Export
    connect(exportSelectedBtn, &QPushButton::clicked, this, &GuardZonePanel::onExportSelected);
    connect(exportAllBtn, &QPushButton::clicked, this, &GuardZonePanel::onExportAll);
    connect(importBtn, &QPushButton::clicked, this, &GuardZonePanel::onImportGuardZones);
}

void GuardZonePanel::refreshGuardZoneList()
{
    if (!ecWidget) {
        qDebug() << "GuardZonePanel::refreshGuardZoneList - ecWidget is null";
        return;
    }

    if (!ecWidget->IsInitialized()) {
        qDebug() << "GuardZonePanel::refreshGuardZoneList - EcWidget not yet initialized, deferring";
        QTimer::singleShot(50, this, &GuardZonePanel::refreshGuardZoneList);
        return;
    }

    // TAMBAHAN: Performance timer
    QElapsedTimer timer;
    timer.start();

    qDebug() << "GuardZonePanel::refreshGuardZoneList called";

    try {
        guardZoneList->blockSignals(true);
        guardZoneList->clear();

        QList<GuardZone>& guardZones = ecWidget->getGuardZones();
        qDebug() << "Found" << guardZones.size() << "guardzones to display";

        int successCount = 0;
        int errorCount = 0;

        for (const GuardZone& gz : guardZones) {
            try {
                // TAMBAHAN: Validate guardzone before creating item
                if (gz.id <= 0) {
                    qDebug() << "[ERROR] Invalid guardzone ID:" << gz.id;
                    errorCount++;
                    continue;
                }

                if (gz.name.isEmpty()) {
                    qDebug() << "[WARNING] GuardZone" << gz.id << "has empty name";
                }

                GuardZoneListItem* item = new GuardZoneListItem(gz, guardZoneList);

                // Triple-check checkbox state
                Qt::CheckState expectedState = gz.active ? Qt::Checked : Qt::Unchecked;
                if (item->checkState() != expectedState) {
                    qDebug() << "Correcting checkbox state for" << gz.name
                             << "from" << item->checkState() << "to" << expectedState;
                    item->setCheckState(expectedState);
                }

                // TAMBAHAN: Validate item after creation
                if (item->getGuardZoneId() != gz.id) {
                    qDebug() << "[ERROR] Item ID mismatch:" << item->getGuardZoneId() << "vs" << gz.id;
                    delete item;
                    errorCount++;
                    continue;
                }

                qDebug() << "Added GuardZone to list:" << gz.name
                         << "ID:" << gz.id
                         << "Active:" << gz.active
                         << "CheckState:" << (gz.active ? "Checked" : "Unchecked");

                guardZoneList->addItem(item);
                successCount++;

            } catch (const std::exception& e) {
                qDebug() << "[ERROR] Exception while creating list item for GuardZone" << gz.id << ":" << e.what();
                errorCount++;
            } catch (...) {
                qDebug() << "[ERROR] Unknown exception while creating list item for GuardZone" << gz.id;
                errorCount++;
            }
        }

        guardZoneList->blockSignals(false);

        updateInfoLabels();
        applyFilters();

        qint64 elapsed = timer.elapsed();
        qDebug() << "GuardZonePanel refresh completed in" << elapsed << "ms"
                 << "- Success:" << successCount << "Errors:" << errorCount
                 << "List widget has" << guardZoneList->count() << "items";

        // TAMBAHAN: Validate final state
        if (successCount > 0) {
            QTimer::singleShot(100, this, &GuardZonePanel::validatePanelState);
        }

    } catch (const std::exception& e) {
        guardZoneList->blockSignals(false);
        qDebug() << "[CRITICAL] Exception in refreshGuardZoneList:" << e.what();
    } catch (...) {
        guardZoneList->blockSignals(false);
        qDebug() << "[CRITICAL] Unknown exception in refreshGuardZoneList";
    }
}

void GuardZonePanel::selectGuardZone(int guardZoneId)
{
    for (int i = 0; i < guardZoneList->count(); ++i) {
        GuardZoneListItem* item = dynamic_cast<GuardZoneListItem*>(guardZoneList->item(i));
        if (item && item->getGuardZoneId() == guardZoneId) {
            guardZoneList->setCurrentItem(item);
            guardZoneList->scrollToItem(item);
            break;
        }
    }
}

void GuardZonePanel::updateInfoLabels()
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    int total = guardZones.size();
    int active = 0;
    int inactive = 0;
    int circles = 0;
    int polygons = 0;
    int attachedToShip = 0;

    for (const GuardZone& gz : guardZones) {
        if (gz.active) {
            active++;
        } else {
            inactive++;
        }

        if (gz.shape == GUARD_ZONE_CIRCLE) {
            circles++;
        } else if (gz.shape == GUARD_ZONE_POLYGON) {
            polygons++;
        }

        if (gz.attachedToShip) {
            attachedToShip++;
        }
    }

    totalLabel->setText(QString("Total: %1 (◯%2 ▲%3)").arg(total).arg(circles).arg(polygons));
    activeLabel->setText(QString("Active: %1").arg(active));
    inactiveLabel->setText(QString("Inactive: %1 %2")
                               .arg(inactive)
                               .arg(attachedToShip > 0 ? QString("🚢%1").arg(attachedToShip) : ""));

    // TAMBAHAN: Color coding untuk status
    if (active > 0) {
        activeLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    } else {
        activeLabel->setStyleSheet("QLabel { color: gray; }");
    }

    if (inactive > 0) {
        inactiveLabel->setStyleSheet("QLabel { color: orange; }");
    } else {
        inactiveLabel->setStyleSheet("QLabel { color: gray; }");
    }

    qDebug() << "Info labels updated - Total:" << total << "Active:" << active
             << "Inactive:" << inactive << "Shapes(C/P):" << circles << "/" << polygons;
}

void GuardZonePanel::applyFilters()
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (int i = 0; i < guardZoneList->count(); ++i) {
        GuardZoneListItem* item = dynamic_cast<GuardZoneListItem*>(guardZoneList->item(i));
        if (item) {
            // Find corresponding guardzone
            bool found = false;
            for (const GuardZone& gz : guardZones) {
                if (gz.id == item->getGuardZoneId()) {
                    bool visible = passesFilter(gz);
                    item->setHidden(!visible);
                    found = true;
                    break;
                }
            }
            if (!found) {
                item->setHidden(true);
            }
        }
    }
}

bool GuardZonePanel::passesFilter(const GuardZone& guardZone)
{
    // Search filter
    QString searchText = searchEdit->text().trimmed().toLower();
    if (!searchText.isEmpty()) {
        if (!guardZone.name.toLower().contains(searchText)) {
            return false;
        }
    }

    // Visibility filter
    if (guardZone.active && !showActiveCheck->isChecked()) {
        return false;
    }
    if (!guardZone.active && !showInactiveCheck->isChecked()) {
        return false;
    }

    // Shape filter
    int shapeFilter = shapeFilterCombo->currentIndex();
    if (shapeFilter == 1 && guardZone.shape != GUARD_ZONE_CIRCLE) { // Circle only
        return false;
    }
    if (shapeFilter == 2 && guardZone.shape != GUARD_ZONE_POLYGON) { // Polygon only
        return false;
    }

    return true;
}

QList<int> GuardZonePanel::getSelectedGuardZoneIds()
{
    QList<int> selectedIds;
    QList<QListWidgetItem*> selectedItems = guardZoneList->selectedItems();

    for (QListWidgetItem* item : selectedItems) {
        GuardZoneListItem* gzItem = dynamic_cast<GuardZoneListItem*>(item);
        if (gzItem) {
            selectedIds.append(gzItem->getGuardZoneId());
        }
    }

    return selectedIds;
}

// ========== SLOT IMPLEMENTATIONS ==========

void GuardZonePanel::onGuardZoneModified()
{
    // PERBAIKAN: Jangan refresh saat dalam edit mode untuk avoid conflict
    if (ecWidget && ecWidget->getGuardZoneManager() &&
        ecWidget->getGuardZoneManager()->isEditingGuardZone()) {
        qDebug() << "Skipping refresh during edit mode";
        return;
    }

    // PERBAIKAN: Update selective tanpa full refresh
    updateExistingItems();
}

void GuardZonePanel::onGuardZoneCreated()
{
    refreshGuardZoneList();
}

void GuardZonePanel::onGuardZoneDeleted()
{
    refreshGuardZoneList();
}

void GuardZonePanel::onItemSelectionChanged()
{
    QList<int> selectedIds = getSelectedGuardZoneIds();

    // Update bulk operation button states
    bool hasSelection = !selectedIds.isEmpty();
    toggleActiveBtn->setEnabled(hasSelection);
    deleteSelectedBtn->setEnabled(hasSelection);
    changeColorBtn->setEnabled(hasSelection);
    exportSelectedBtn->setEnabled(hasSelection);

    // Emit signal for first selected item
    if (!selectedIds.isEmpty()) {
        emit guardZoneSelected(selectedIds.first());
    }
}

void GuardZonePanel::onItemDoubleClicked(QListWidgetItem* item)
{
    GuardZoneListItem* gzItem = dynamic_cast<GuardZoneListItem*>(item);
    if (gzItem) {
        emit guardZoneEditRequested(gzItem->getGuardZoneId());
    }
}

void GuardZonePanel::onItemChanged(QListWidgetItem* item)
{
    GuardZoneListItem* gzItem = dynamic_cast<GuardZoneListItem*>(item);
    if (!gzItem || !ecWidget) {
        return;
    }

    // TAMBAHAN: Cegah recursive update saat sedang refresh
    static bool updating = false;
    if (updating) {
        qDebug() << "Preventing recursive update in onItemChanged";
        return;
    }

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == gzItem->getGuardZoneId()) {
            bool newActive = (item->checkState() == Qt::Checked);

            if (gz.active != newActive) {
                qDebug() << "Changing GuardZone" << gz.id << "active from" << gz.active << "to" << newActive;

                // Set flag untuk mencegah recursive call
                updating = true;

                gz.active = newActive;

                // Update text secara manual tanpa mengubah checkbox state
                QString shapeText = (gz.shape == GUARD_ZONE_CIRCLE) ? "Circle" : "Polygon";
                QString statusText = gz.active ? "Active" : "Inactive";
                QString attachText = gz.attachedToShip ? " [Ship]" : "";
                QString displayText = QString("%1 (%2) - %3%4")
                                          .arg(gz.name)
                                          .arg(shapeText)
                                          .arg(statusText)
                                          .arg(attachText);
                gzItem->setText(displayText);

                // Save dan update
                ecWidget->saveGuardZones();
                ecWidget->update();

                // Update info labels tanpa full refresh
                updateInfoLabels();

                emit guardZoneVisibilityChanged(gz.id, newActive);

                // Reset flag
                updating = false;
            }
            break;
        }
    }
}

void GuardZonePanel::onItemContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = guardZoneList->itemAt(pos);
    showContextMenu(pos, item);
}

void GuardZonePanel::showContextMenu(const QPoint& pos, QListWidgetItem* item)
{
    QMenu contextMenu(this);

    if (item) {
        GuardZoneListItem* gzItem = dynamic_cast<GuardZoneListItem*>(item);
        if (gzItem) {
            // Single item context menu
            QAction* editAction = contextMenu.addAction(tr("Edit GuardZone"));
            QAction* centerAction = contextMenu.addAction(tr("Center on Map"));

            contextMenu.addSeparator();

            QAction* toggleAction = contextMenu.addAction(tr("Toggle Active"));
            QAction* colorAction = contextMenu.addAction(tr("Change Color"));
            QAction* renameAction = contextMenu.addAction(tr("Rename"));

            contextMenu.addSeparator();

            QAction* propertiesAction = contextMenu.addAction(tr("Properties"));
            QAction* deleteAction = contextMenu.addAction(tr("Delete"));
            deleteAction->setIcon(QIcon(":/images/delete.png"));

            QAction* selectedAction = contextMenu.exec(guardZoneList->mapToGlobal(pos));

            if (selectedAction == editAction) {
                emit guardZoneEditRequested(gzItem->getGuardZoneId());
            }
            else if (selectedAction == centerAction) {
                centerOnGuardZone(gzItem->getGuardZoneId());
            }
            else if (selectedAction == toggleAction) {
                toggleGuardZoneActive(gzItem->getGuardZoneId());
            }
            else if (selectedAction == colorAction) {
                changeGuardZoneColor(gzItem->getGuardZoneId());
            }
            else if (selectedAction == renameAction) {
                renameGuardZone(gzItem->getGuardZoneId());
            }
            else if (selectedAction == propertiesAction) {
                showGuardZoneProperties(gzItem->getGuardZoneId());
            }
            else if (selectedAction == deleteAction) {
                deleteGuardZone(gzItem->getGuardZoneId());
            }
        }
    } else {
        // Empty area context menu
        QAction* refreshAction = contextMenu.addAction(tr("Refresh List"));
        QAction* importAction = contextMenu.addAction(tr("Import GuardZones"));

        QAction* selectedAction = contextMenu.exec(guardZoneList->mapToGlobal(pos));

        if (selectedAction == refreshAction) {
            refreshGuardZoneList();
        }
        else if (selectedAction == importAction) {
            onImportGuardZones();
        }
    }
}

void GuardZonePanel::onFilterChanged()
{
    applyFilters();
}

void GuardZonePanel::onShowActiveChanged(bool show)
{
    applyFilters();
}

void GuardZonePanel::onShowInactiveChanged(bool show)
{
    applyFilters();
}

void GuardZonePanel::onShapeFilterChanged()
{
    applyFilters();
}

void GuardZonePanel::onSelectAll()
{
    guardZoneList->selectAll();
}

void GuardZonePanel::onSelectNone()
{
    guardZoneList->clearSelection();
}

void GuardZonePanel::onToggleSelectedActive()
{
    QList<int> selectedIds = getSelectedGuardZoneIds();
    if (selectedIds.isEmpty()) return;

    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (int id : selectedIds) {
        for (GuardZone& gz : guardZones) {
            if (gz.id == id) {
                gz.active = !gz.active;
                break;
            }
        }
    }

    ecWidget->saveGuardZones();
    ecWidget->update();
    refreshGuardZoneList();
}

void GuardZonePanel::onDeleteSelected()
{
    QList<int> selectedIds = getSelectedGuardZoneIds();
    if (selectedIds.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Delete GuardZones"),
        tr("Are you sure you want to delete %1 selected GuardZone(s)?").arg(selectedIds.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (!ecWidget) return;

        QList<GuardZone>& guardZones = ecWidget->getGuardZones();

        // Remove in reverse order to maintain indices
        for (int i = guardZones.size() - 1; i >= 0; --i) {
            if (selectedIds.contains(guardZones[i].id)) {
                guardZones.removeAt(i);
            }
        }

        ecWidget->saveGuardZones();
        ecWidget->update();
        refreshGuardZoneList();
    }
}

void GuardZonePanel::onChangeSelectedColor()
{
    QList<int> selectedIds = getSelectedGuardZoneIds();
    if (selectedIds.isEmpty()) return;

    QColor newColor = QColorDialog::getColor(Qt::red, this, tr("Choose Color for Selected GuardZones"));

    if (newColor.isValid()) {
        if (!ecWidget) return;

        QList<GuardZone>& guardZones = ecWidget->getGuardZones();

        for (int id : selectedIds) {
            for (GuardZone& gz : guardZones) {
                if (gz.id == id) {
                    gz.color = newColor;
                    break;
                }
            }
        }

        ecWidget->saveGuardZones();
        ecWidget->update();
        refreshGuardZoneList();
    }
}

// ========== HELPER METHODS IMPLEMENTATION ==========

void GuardZonePanel::centerOnGuardZone(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (const GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            if (gz.shape == GUARD_ZONE_CIRCLE) {
                // Center on circle center
                ecWidget->SetCenter(gz.centerLat, gz.centerLon);
                // Set appropriate scale for the circle
                double radiusPixels = ecWidget->calculatePixelsFromNauticalMiles(gz.radius);
                int appropriateScale = static_cast<int>(gz.radius * 10000); // Rough calculation
                if (appropriateScale < 1000) appropriateScale = 1000;
                if (appropriateScale > 500000) appropriateScale = 500000;
                ecWidget->SetScale(appropriateScale);
            }
            else if (gz.shape == GUARD_ZONE_POLYGON && gz.latLons.size() >= 6) {
                // Calculate center of polygon
                double centerLat = 0, centerLon = 0;
                int numPoints = gz.latLons.size() / 2;

                for (int i = 0; i < gz.latLons.size(); i += 2) {
                    centerLat += gz.latLons[i];
                    centerLon += gz.latLons[i + 1];
                }

                centerLat /= numPoints;
                centerLon /= numPoints;

                ecWidget->SetCenter(centerLat, centerLon);

                // Calculate appropriate scale based on polygon size
                double maxDist = 0;
                for (int i = 0; i < gz.latLons.size(); i += 2) {
                    double dist, bearing;
                    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                           centerLat, centerLon,
                                                           gz.latLons[i], gz.latLons[i + 1],
                                                           &dist, &bearing);
                    if (dist > maxDist) maxDist = dist;
                }

                int appropriateScale = static_cast<int>(maxDist * 15000); // Rough calculation
                if (appropriateScale < 1000) appropriateScale = 1000;
                if (appropriateScale > 500000) appropriateScale = 500000;
                ecWidget->SetScale(appropriateScale);
            }

            ecWidget->Draw();
            emit guardZoneSelected(guardZoneId);
            break;
        }
    }
}

void GuardZonePanel::toggleGuardZoneActive(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            gz.active = !gz.active;
            ecWidget->saveGuardZones();
            ecWidget->update();
            refreshGuardZoneList();
            emit guardZoneVisibilityChanged(gz.id, gz.active);
            break;
        }
    }
}

void GuardZonePanel::changeGuardZoneColor(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            QColor newColor = QColorDialog::getColor(gz.color, this,
                                                     tr("Choose Color for %1").arg(gz.name));

            if (newColor.isValid() && newColor != gz.color) {
                gz.color = newColor;
                ecWidget->saveGuardZones();
                ecWidget->update();
                refreshGuardZoneList();
            }
            break;
        }
    }
}

void GuardZonePanel::renameGuardZone(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            bool ok;
            QString newName = QInputDialog::getText(this, tr("Rename GuardZone"),
                                                    tr("Enter new name for GuardZone:"),
                                                    QLineEdit::Normal, gz.name, &ok);
            if (ok && !newName.isEmpty()) {
                gz.name = newName;
                ecWidget->saveGuardZones();
                ecWidget->update();
                refreshGuardZoneList();
            }
            break;
        }
    }
}

void GuardZonePanel::showGuardZoneProperties(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (const GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            QString properties;
            properties += tr("GuardZone Properties\n");
            properties += tr("═══════════════════\n");
            properties += tr("ID: %1\n").arg(gz.id);
            properties += tr("Name: %1\n").arg(gz.name);
            properties += tr("Shape: %1\n").arg(gz.shape == GUARD_ZONE_CIRCLE ? tr("Circle") : tr("Polygon"));
            properties += tr("Active: %1\n").arg(gz.active ? tr("Yes") : tr("No"));
            properties += tr("Attached to Ship: %1\n").arg(gz.attachedToShip ? tr("Yes") : tr("No"));
            properties += tr("Color: %1\n").arg(gz.color.name());

            if (gz.shape == GUARD_ZONE_CIRCLE) {
                properties += tr("Center: %1°, %2°\n").arg(gz.centerLat, 0, 'f', 6).arg(gz.centerLon, 0, 'f', 6);
                properties += tr("Radius: %1 NM\n").arg(gz.radius, 0, 'f', 2);

                // Calculate area
                double areaKm2 = M_PI * pow(gz.radius * 1.852, 2); // Convert NM to km
                properties += tr("Area: %1 km²").arg(areaKm2, 0, 'f', 2);
            } else {
                properties += tr("Points: %1\n").arg(gz.latLons.size() / 2);

                // Calculate approximate area for polygon (simplified)
                if (gz.latLons.size() >= 6) {
                    // Find bounding box
                    double minLat = gz.latLons[0], maxLat = gz.latLons[0];
                    double minLon = gz.latLons[1], maxLon = gz.latLons[1];

                    for (int i = 2; i < gz.latLons.size(); i += 2) {
                        if (gz.latLons[i] < minLat) minLat = gz.latLons[i];
                        if (gz.latLons[i] > maxLat) maxLat = gz.latLons[i];
                        if (gz.latLons[i+1] < minLon) minLon = gz.latLons[i+1];
                        if (gz.latLons[i+1] > maxLon) maxLon = gz.latLons[i+1];
                    }

                    double latRange = maxLat - minLat;
                    double lonRange = maxLon - minLon;
                    double approxAreaKm2 = latRange * lonRange * 111.32 * 111.32; // Very rough approximation

                    properties += tr("Bounding Box: %1° × %2°\n").arg(latRange, 0, 'f', 4).arg(lonRange, 0, 'f', 4);
                    properties += tr("Approx. Area: %1 km²").arg(approxAreaKm2, 0, 'f', 2);
                }
            }

            QMessageBox::information(this, tr("GuardZone Properties"), properties);
            break;
        }
    }
}

void GuardZonePanel::deleteGuardZone(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (int i = 0; i < guardZones.size(); i++) {
        if (guardZones[i].id == guardZoneId) {
            QString name = guardZones[i].name;

            QMessageBox::StandardButton reply = QMessageBox::question(
                this, tr("Delete GuardZone"),
                tr("Are you sure you want to delete GuardZone '%1'?").arg(name),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                guardZones.removeAt(i);

                // Update GuardZone system
                if (guardZones.isEmpty()) {
                    ecWidget->enableGuardZone(false);
                } else {
                    ecWidget->enableGuardZone(true);
                }

                ecWidget->saveGuardZones();
                ecWidget->update();
                refreshGuardZoneList();
            }
            return;
        }
    }
}

// ========== IMPORT/EXPORT IMPLEMENTATION ==========

void GuardZonePanel::onExportSelected()
{
    QList<int> selectedIds = getSelectedGuardZoneIds();
    if (selectedIds.isEmpty()) {
        QMessageBox::warning(this, tr("Export GuardZones"), tr("No GuardZones selected for export."));
        return;
    }

    QString filename = QFileDialog::getSaveFileName(this, tr("Export Selected GuardZones"),
                                                    QDir::homePath() + "/selected_guardzones.json",
                                                    tr("JSON Files (*.json);;All Files (*)"));
    if (!filename.isEmpty()) {
        if (!filename.endsWith(".json", Qt::CaseInsensitive)) {
            filename += ".json";
        }

        exportGuardZones(filename, selectedIds);
    }
}

void GuardZonePanel::onExportAll()
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();
    if (guardZones.isEmpty()) {
        QMessageBox::warning(this, tr("Export GuardZones"), tr("No GuardZones to export."));
        return;
    }

    QString filename = QFileDialog::getSaveFileName(this, tr("Export All GuardZones"),
                                                    QDir::homePath() + "/all_guardzones.json",
                                                    tr("JSON Files (*.json);;All Files (*)"));
    if (!filename.isEmpty()) {
        if (!filename.endsWith(".json", Qt::CaseInsensitive)) {
            filename += ".json";
        }

        // Export all - pass empty list to indicate all
        exportGuardZones(filename, QList<int>());
    }
}

void GuardZonePanel::onImportGuardZones()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Import GuardZones"),
                                                    QDir::homePath(),
                                                    tr("JSON Files (*.json);;All Files (*)"));
    if (!filename.isEmpty()) {
        importGuardZones(filename);
    }
}

bool GuardZonePanel::exportGuardZones(const QString& filename, const QList<int>& selectedIds)
{
    if (!ecWidget) return false;

    try {
        QList<GuardZone>& guardZones = ecWidget->getGuardZones();

        QJsonArray guardZoneArray;
        int exportCount = 0;
        int skipCount = 0;
        QStringList errors;

        for (const GuardZone& gz : guardZones) {
            // If selectedIds is empty, export all; otherwise check if ID is selected
            if (!selectedIds.isEmpty() && !selectedIds.contains(gz.id)) {
                continue;
            }

            // TAMBAHAN: Validate before export
            bool isValid = true;
            QString validationError;

            if (gz.id <= 0) {
                validationError = "Invalid ID";
                isValid = false;
            } else if (gz.shape == GUARD_ZONE_CIRCLE) {
                if (gz.centerLat < -90 || gz.centerLat > 90 ||
                    gz.centerLon < -180 || gz.centerLon > 180 ||
                    gz.radius <= 0 || gz.radius > 100) {
                    validationError = "Invalid circle parameters";
                    isValid = false;
                }
            } else if (gz.shape == GUARD_ZONE_POLYGON) {
                if (gz.latLons.size() < 6 || gz.latLons.size() % 2 != 0) {
                    validationError = "Invalid polygon data";
                    isValid = false;
                }
            }

            if (!isValid) {
                errors << QString("GuardZone %1 (%2): %3").arg(gz.id).arg(gz.name).arg(validationError);
                skipCount++;
                continue;
            }

            QJsonObject gzObject;
            gzObject["id"] = gz.id;
            gzObject["name"] = gz.name;
            gzObject["shape"] = static_cast<int>(gz.shape);
            gzObject["active"] = gz.active;
            gzObject["attachedToShip"] = gz.attachedToShip;
            gzObject["color"] = gz.color.name();
            gzObject["exported_on"] = QDateTime::currentDateTime().toString(Qt::ISODate);

            if (gz.shape == GUARD_ZONE_CIRCLE) {
                gzObject["centerLat"] = gz.centerLat;
                gzObject["centerLon"] = gz.centerLon;
                gzObject["radius"] = gz.radius;
            } else if (gz.shape == GUARD_ZONE_POLYGON) {
                QJsonArray latLonArray;
                for (double coord : gz.latLons) {
                    latLonArray.append(coord);
                }
                gzObject["latLons"] = latLonArray;
            }

            guardZoneArray.append(gzObject);
            exportCount++;
        }

        QJsonObject rootObject;
        rootObject["version"] = "1.1";
        rootObject["exported_on"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        rootObject["exported_by"] = "ECDIS GuardZone Manager";
        rootObject["export_type"] = selectedIds.isEmpty() ? "all" : "selected";
        rootObject["guardzone_count"] = exportCount;
        rootObject["skipped_count"] = skipCount;
        rootObject["guardzones"] = guardZoneArray;

        if (!errors.isEmpty()) {
            rootObject["export_errors"] = QJsonArray::fromStringList(errors);
        }

        QJsonDocument jsonDoc(rootObject);

        // TAMBAHAN: Atomic write
        QString tempFilename = filename + ".tmp";
        QFile tempFile(tempFilename);

        if (tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QByteArray data = jsonDoc.toJson(QJsonDocument::Indented);
            qint64 written = tempFile.write(data);
            tempFile.close();

            if (written == data.size()) {
                // Atomic rename
                if (QFile::exists(filename)) {
                    QFile::remove(filename);
                }

                if (QFile::rename(tempFilename, filename)) {
                    QString message = tr("Successfully exported %1 GuardZone(s) to:\n%2")
                    .arg(exportCount)
                        .arg(QFileInfo(filename).fileName());

                    if (skipCount > 0) {
                        message += tr("\n\n%1 GuardZone(s) were skipped due to validation errors.").arg(skipCount);
                    }

                    QMessageBox::information(this, tr("Export Successful"), message);
                    return true;
                } else {
                    QFile::remove(tempFilename);
                    throw std::runtime_error("Failed to rename temporary file");
                }
            } else {
                QFile::remove(tempFilename);
                throw std::runtime_error("Incomplete file write");
            }
        } else {
            throw std::runtime_error(tempFile.errorString().toStdString());
        }

    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Export Failed"),
                              tr("Failed to export GuardZones:\n%1").arg(e.what()));
        return false;
    } catch (...) {
        QMessageBox::critical(this, tr("Export Failed"),
                              tr("An unknown error occurred during export."));
        return false;
    }
}

bool GuardZonePanel::importGuardZones(const QString& filename)
{
    if (!ecWidget) return false;

    QFile file(filename);
    if (!file.exists()) {
        QMessageBox::warning(this, tr("Import GuardZones"),
                             tr("File does not exist: %1").arg(filename));
        return false;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray jsonData = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            QMessageBox::critical(this, tr("Import Failed"),
                                  tr("JSON parse error at position %1:\n%2")
                                      .arg(parseError.offset)
                                      .arg(parseError.errorString()));
            return false;
        }

        if (!jsonDoc.isObject()) {
            QMessageBox::critical(this, tr("Import Failed"),
                                  tr("Invalid JSON file format"));
            return false;
        }

        QJsonObject rootObject = jsonDoc.object();
        if (!rootObject.contains("guardzones") || !rootObject["guardzones"].isArray()) {
            QMessageBox::critical(this, tr("Import Failed"),
                                  tr("Invalid GuardZone file format"));
            return false;
        }

        QJsonArray guardZoneArray = rootObject["guardzones"].toArray();

        // Ask user about merge strategy
        QList<GuardZone>& existingGuardZones = ecWidget->getGuardZones();
        if (!existingGuardZones.isEmpty()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Import GuardZones"));
            msgBox.setText(tr("You already have %1 GuardZone(s).").arg(existingGuardZones.size()));
            msgBox.setInformativeText(tr("How do you want to import the new GuardZones?"));

            QPushButton *replaceButton = msgBox.addButton(tr("Replace All"), QMessageBox::DestructiveRole);
            QPushButton *mergeButton = msgBox.addButton(tr("Merge (Add)"), QMessageBox::AcceptRole);
            QPushButton *cancelButton = msgBox.addButton(QMessageBox::Cancel);
            msgBox.setDefaultButton(mergeButton);

            msgBox.exec();

            if (msgBox.clickedButton() == cancelButton) {
                return false;
            }

            if (msgBox.clickedButton() == replaceButton) {
                existingGuardZones.clear();
            }
        }

        int importedCount = 0;
        int skippedCount = 0;
        int nextId = ecWidget->getNextGuardZoneId();

        for (const QJsonValue &value : guardZoneArray) {
            if (!value.isObject()) {
                skippedCount++;
                continue;
            }

            QJsonObject gzObject = value.toObject();

            if (!gzObject.contains("shape")) {
                skippedCount++;
                continue;
            }

            GuardZone gz;
            gz.id = nextId++; // Assign new ID to avoid conflicts
            gz.name = gzObject.contains("name") ? gzObject["name"].toString() : QString("Imported_GuardZone_%1").arg(gz.id);
            gz.shape = static_cast<::GuardZoneShape>(gzObject["shape"].toInt());
            gz.active = gzObject.contains("active") ? gzObject["active"].toBool() : true;
            gz.attachedToShip = gzObject.contains("attachedToShip") ? gzObject["attachedToShip"].toBool() : false;
            gz.color = gzObject.contains("color") ? QColor(gzObject["color"].toString()) : Qt::red;

            if (gz.shape == GUARD_ZONE_CIRCLE) {
                if (gzObject.contains("centerLat") && gzObject.contains("centerLon") && gzObject.contains("radius")) {
                    gz.centerLat = gzObject["centerLat"].toDouble();
                    gz.centerLon = gzObject["centerLon"].toDouble();
                    gz.radius = gzObject["radius"].toDouble();

                    // Validate circle data
                    if (gz.centerLat >= -90 && gz.centerLat <= 90 &&
                        gz.centerLon >= -180 && gz.centerLon <= 180 &&
                        gz.radius > 0 && gz.radius <= 100) {
                        existingGuardZones.append(gz);
                        importedCount++;
                    } else {
                        skippedCount++;
                    }
                } else {
                    skippedCount++;
                }
            } else if (gz.shape == GUARD_ZONE_POLYGON) {
                if (gzObject.contains("latLons") && gzObject["latLons"].isArray()) {
                    QJsonArray latLonArray = gzObject["latLons"].toArray();
                    gz.latLons.clear();

                    for (const QJsonValue &coord : latLonArray) {
                        gz.latLons.append(coord.toDouble());
                    }

                    // Validate polygon data (minimum 3 points)
                    if (gz.latLons.size() >= 6) {
                        bool validCoords = true;
                        for (int i = 0; i < gz.latLons.size(); i += 2) {
                            if (gz.latLons[i] < -90 || gz.latLons[i] > 90 ||
                                gz.latLons[i+1] < -180 || gz.latLons[i+1] > 180) {
                                validCoords = false;
                                break;
                            }
                        }

                        if (validCoords) {
                            existingGuardZones.append(gz);
                            importedCount++;
                        } else {
                            skippedCount++;
                        }
                    } else {
                        skippedCount++;
                    }
                } else {
                    skippedCount++;
                }
            } else {
                skippedCount++;
            }
        }

        if (importedCount > 0) {
            // Enable GuardZone system
            ecWidget->enableGuardZone(true);

            // Save and refresh
            ecWidget->saveGuardZones();
            ecWidget->update();
            refreshGuardZoneList();

            QString message = tr("Successfully imported %1 GuardZone(s)").arg(importedCount);
            if (skippedCount > 0) {
                message += tr("\n%1 GuardZone(s) were skipped due to invalid data").arg(skippedCount);
            }

            QMessageBox::information(this, tr("Import Successful"), message);
            return true;
        } else {
            if (skippedCount > 0) {
                QMessageBox::warning(this, tr("Import Failed"),
                                     tr("All %1 GuardZone(s) in the file were invalid").arg(skippedCount));
            } else {
                QMessageBox::warning(this, tr("Import Failed"),
                                     tr("No GuardZones found in the file"));
            }
            return false;
        }
    } else {
        QMessageBox::critical(this, tr("Import Failed"),
                              tr("Failed to open file: %1").arg(file.errorString()));
        return false;
    }
}

void GuardZonePanel::updateExistingItems()
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    // Block signals untuk prevent loop
    guardZoneList->blockSignals(true);

    // Update existing items
    for (int i = 0; i < guardZoneList->count(); ++i) {
        GuardZoneListItem* item = dynamic_cast<GuardZoneListItem*>(guardZoneList->item(i));
        if (item) {
            // Find corresponding guardzone
            for (const GuardZone& gz : guardZones) {
                if (gz.id == item->getGuardZoneId()) {
                    // Update item dari guardzone data
                    item->updateFromGuardZone(gz);

                    // PASTIKAN checkbox state benar
                    item->setCheckState(gz.active ? Qt::Checked : Qt::Unchecked);
                    break;
                }
            }
        }
    }

    // Re-enable signals
    guardZoneList->blockSignals(false);

    updateInfoLabels();
}

void GuardZonePanel::validatePanelState()
{
    if (!ecWidget) return;

    qDebug() << "[VALIDATION] Starting panel state validation";

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();
    bool stateChanged = false;
    int checkedItems = 0;
    int uncheckedItems = 0;
    int activeGuardZones = 0;
    int inactiveGuardZones = 0;
    QStringList mismatches;

    // Count actual guardzone states
    for (const GuardZone& gz : guardZones) {
        if (gz.active) {
            activeGuardZones++;
        } else {
            inactiveGuardZones++;
        }
    }

    // Validate panel items
    for (int i = 0; i < guardZoneList->count(); ++i) {
        GuardZoneListItem* item = dynamic_cast<GuardZoneListItem*>(guardZoneList->item(i));
        if (!item) {
            qDebug() << "[ERROR] Invalid item at index" << i;
            continue;
        }

        Qt::CheckState currentState = item->checkState();
        if (currentState == Qt::Checked) {
            checkedItems++;
        } else {
            uncheckedItems++;
        }

        // Find corresponding guardzone
        bool found = false;
        for (const GuardZone& gz : guardZones) {
            if (gz.id == item->getGuardZoneId()) {
                Qt::CheckState expectedState = gz.active ? Qt::Checked : Qt::Unchecked;

                if (currentState != expectedState) {
                    QString mismatch = QString("GuardZone %1 (%2): Expected %3, Got %4")
                    .arg(gz.id)
                        .arg(gz.name)
                        .arg(expectedState == Qt::Checked ? "Checked" : "Unchecked")
                        .arg(currentState == Qt::Checked ? "Checked" : "Unchecked");
                    mismatches << mismatch;

                    guardZoneList->blockSignals(true);
                    item->setCheckState(expectedState);
                    guardZoneList->blockSignals(false);

                    stateChanged = true;
                }
                found = true;
                break;
            }
        }

        if (!found) {
            qDebug() << "[ERROR] Panel item with ID" << item->getGuardZoneId() << "has no corresponding guardzone";
        }
    }

    // Log validation results
    qDebug() << "[VALIDATION] Panel State Summary:";
    qDebug() << "[VALIDATION]   GuardZones - Active:" << activeGuardZones << "Inactive:" << inactiveGuardZones;
    qDebug() << "[VALIDATION]   Panel Items - Checked:" << checkedItems << "Unchecked:" << uncheckedItems;

    if (mismatches.isEmpty()) {
        qDebug() << "[VALIDATION] ✓ All panel items match guardzone states";
    } else {
        qDebug() << "[VALIDATION] ✗ Found" << mismatches.size() << "mismatches:";
        for (const QString& mismatch : mismatches) {
            qDebug() << "[VALIDATION]   -" << mismatch;
        }
    }

    if (stateChanged) {
        qDebug() << "[VALIDATION] Panel state corrected - updating info labels";
        updateInfoLabels();
    }

    // TAMBAHAN: Emit validation completed signal
    emit validationCompleted(mismatches.isEmpty());
}

void GuardZonePanel::recoverFromError()
{
    qDebug() << "[RECOVERY] Attempting to recover panel state";

    try {
        // Clear everything and start fresh
        guardZoneList->blockSignals(true);
        guardZoneList->clear();
        guardZoneList->blockSignals(false);

        // Retry refresh after brief delay
        QTimer::singleShot(200, this, &GuardZonePanel::refreshGuardZoneList);

        // Show user feedback
        QMessageBox::information(this, tr("Panel Recovery"),
                                 tr("GuardZone panel encountered an error and has been refreshed."));

    } catch (...) {
        qDebug() << "[RECOVERY] Recovery failed - panel may be in unstable state";
        QMessageBox::warning(this, tr("Panel Error"),
                             tr("GuardZone panel encountered a serious error. Please restart the application."));
    }
}

int GuardZonePanel::getGuardZoneListCount() const
{
    return guardZoneList ? guardZoneList->count() : 0;
}
