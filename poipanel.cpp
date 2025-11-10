#include "poipanel.h"

#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QDoubleValidator>
#include <QMessageBox>
#include <QtMath>
#include <limits>
#include <cmath>
#include <QHeaderView>

#include "ecwidget.h"

namespace {
constexpr int kColumnName = 0;
constexpr int kColumnCategory = 1;
constexpr int kColumnLatitude = 2;
constexpr int kColumnLongitude = 3;
constexpr int kColumnDepth = 4;
constexpr int kColumnNotes = 5;
}

POIPanel::POIPanel(EcWidget* ecWidget, QWidget* parent)
    : QWidget(parent)
    , ecWidget(ecWidget)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto* headline = new QLabel(tr("Points of Interest help track critical mission spots."));
    headline->setWordWrap(true);
    layout->addWidget(headline);

    tree = new QTreeWidget(this);
    tree->setColumnCount(6);
    QStringList headers;
    headers << tr("POI") << tr("Category") << tr("Latitude") << tr("Longitude") << tr("Depth (m)") << tr("Notes");
    tree->setHeaderLabels(headers);
    tree->setRootIsDecorated(false);
    tree->setAlternatingRowColors(true);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setUniformRowHeights(true);
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tree->header()->setStretchLastSection(true);
    layout->addWidget(tree, 1);

    connect(tree, &QTreeWidget::itemSelectionChanged, this, &POIPanel::onSelectionChanged);
    connect(tree, &QTreeWidget::itemChanged, this, &POIPanel::onItemChanged);
    connect(tree, &QTreeWidget::itemDoubleClicked, this, &POIPanel::onItemDoubleClicked);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(6);

    addBtn = new QPushButton(tr("Add"));
    editBtn = new QPushButton(tr("Edit"));
    deleteBtn = new QPushButton(tr("Remove"));
    focusBtn = new QPushButton(tr("Focus on Map"));

    buttonRow->addWidget(addBtn);
    buttonRow->addWidget(editBtn);
    buttonRow->addWidget(deleteBtn);
    buttonRow->addStretch(1);
    buttonRow->addWidget(focusBtn);

    layout->addLayout(buttonRow);

    connect(addBtn, &QPushButton::clicked, this, &POIPanel::onAddPoi);
    connect(editBtn, &QPushButton::clicked, this, &POIPanel::onEditPoi);
    connect(deleteBtn, &QPushButton::clicked, this, &POIPanel::onDeletePoi);
    connect(focusBtn, &QPushButton::clicked, this, &POIPanel::onFocusPoi);

    setButtonsEnabled(false);

    if (ecWidget) {
        connect(ecWidget, &EcWidget::poiListChanged, this, &POIPanel::refreshList);
        refreshList();
    }
}

void POIPanel::refreshList()
{
    populateTree();
}

void POIPanel::populateTree()
{
    tree->blockSignals(true);
    tree->clear();

    if (!ecWidget) {
        tree->blockSignals(false);
        return;
    }

    const auto pois = ecWidget->poiEntries();
    for (const auto& poi : pois) {
        // Only add POIs with valid IDs
        if (poi.id <= 0) continue;

        auto* item = new QTreeWidgetItem(tree);
        item->setText(kColumnName, poi.label);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setCheckState(kColumnName, (poi.flags & EC_POI_FLAG_ACTIVE) ? Qt::Checked : Qt::Unchecked);
        item->setText(kColumnCategory, categoryToString(poi.category));
        item->setText(kColumnLatitude, formatCoordinate(poi.latitude, true));
        item->setText(kColumnLongitude, formatCoordinate(poi.longitude, false));
        item->setText(kColumnDepth, formatDepth(poi.depth));
        item->setText(kColumnNotes, poi.description);
        item->setData(kColumnName, Qt::UserRole, poi.id);
    }

    for (int col = 0; col < tree->columnCount(); ++col) {
        tree->resizeColumnToContents(col);
    }

    tree->blockSignals(false);
    onSelectionChanged();
}

int POIPanel::currentPoiId() const
{
    auto* item = tree->currentItem();
    if (!item) return -1;
    const int poiId = item->data(kColumnName, Qt::UserRole).toInt();
    return (poiId > 0) ? poiId : -1;
}

void POIPanel::onSelectionChanged()
{
    const bool hasSelection = currentPoiId() != -1;
    setButtonsEnabled(hasSelection);
}

void POIPanel::setButtonsEnabled(bool enabled)
{
    editBtn->setEnabled(enabled);
    deleteBtn->setEnabled(enabled);
    focusBtn->setEnabled(enabled);
}

QString POIPanel::formatCoordinate(double value, bool isLatitude) const
{
    if (!qIsFinite(value)) return QStringLiteral("--");

    const QChar hemi = isLatitude ? (value < 0 ? 'S' : 'N')
                                  : (value < 0 ? 'W' : 'E');
    const double absVal = std::fabs(value);
    return QString::number(absVal, 'f', 6) + QLatin1Char(' ') + hemi;
}

QString POIPanel::formatDepth(double value) const
{
    if (!qIsFinite(value)) return QStringLiteral("-");
    return QString::number(value, 'f', 1);
}

QString POIPanel::categoryToString(EcPoiCategory category) const
{
    switch (category) {
    case EC_POI_GENERIC: return tr("Generic");
    case EC_POI_CHECKPOINT: return tr("Checkpoint");
    case EC_POI_HAZARD: return tr("Hazard");
    case EC_POI_SURVEY_TARGET: return tr("Survey Target");
    default: return tr("Unknown");
    }
}

EcPoiCategory POIPanel::categoryFromIndex(int index) const
{
    switch (index) {
    case 0: return EC_POI_GENERIC;
    case 1: return EC_POI_CHECKPOINT;
    case 2: return EC_POI_HAZARD;
    case 3: return EC_POI_SURVEY_TARGET;
    default: return EC_POI_GENERIC;
    }
}

bool POIPanel::showPoiDialog(PoiEntry& inOutPoi, bool isEdit)
{
    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? tr("Edit Point of Interest") : tr("Add Point of Interest"));

    auto* formLayout = new QFormLayout(&dialog);

    auto* nameEdit = new QLineEdit(inOutPoi.label, &dialog);
    formLayout->addRow(tr("Name"), nameEdit);

    auto* categoryCombo = new QComboBox(&dialog);
    categoryCombo->addItems({tr("Generic"), tr("Checkpoint"), tr("Hazard"), tr("Survey Target")});
    categoryCombo->setCurrentIndex(static_cast<int>(inOutPoi.category));
    formLayout->addRow(tr("Category"), categoryCombo);

    auto* latEdit = new QLineEdit(&dialog);
    latEdit->setText(qIsFinite(inOutPoi.latitude) ? QString::number(inOutPoi.latitude, 'f', 6) : QString());
    latEdit->setValidator(new QDoubleValidator(-90.0, 90.0, 6, latEdit));
    formLayout->addRow(tr("Latitude"), latEdit);

    auto* lonEdit = new QLineEdit(&dialog);
    lonEdit->setText(qIsFinite(inOutPoi.longitude) ? QString::number(inOutPoi.longitude, 'f', 6) : QString());
    lonEdit->setValidator(new QDoubleValidator(-180.0, 180.0, 6, lonEdit));
    formLayout->addRow(tr("Longitude"), lonEdit);

    auto* depthEdit = new QLineEdit(&dialog);
    depthEdit->setText(qIsFinite(inOutPoi.depth) ? QString::number(inOutPoi.depth, 'f', 1) : QString());
    depthEdit->setValidator(new QDoubleValidator(-11000.0, 11000.0, 2, depthEdit));
    formLayout->addRow(tr("Depth (m)"), depthEdit);

    auto* notesEdit = new QTextEdit(&dialog);
    notesEdit->setPlainText(inOutPoi.description);
    notesEdit->setMinimumHeight(60);
    formLayout->addRow(tr("Notes"), notesEdit);

    if (!isEdit && ecWidget) {
        EcCoordinate latCenter = 0.0;
        EcCoordinate lonCenter = 0.0;
        ecWidget->GetCenter(latCenter, lonCenter);
        if (!qIsFinite(inOutPoi.latitude)) {
            latEdit->setText(QString::number(latCenter, 'f', 6));
        }
        if (!qIsFinite(inOutPoi.longitude)) {
            lonEdit->setText(QString::number(lonCenter, 'f', 6));
        }
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    formLayout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Please provide a name for the POI."));
        return false;
    }

    bool okLat = false;
    bool okLon = false;
    const double latitude = latEdit->text().toDouble(&okLat);
    const double longitude = lonEdit->text().toDouble(&okLon);

    if (!okLat || !okLon) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Latitude or longitude is not valid."));
        return false;
    }

    double depth = std::numeric_limits<double>::quiet_NaN();
    if (!depthEdit->text().trimmed().isEmpty()) {
        bool okDepth = false;
        depth = depthEdit->text().toDouble(&okDepth);
        if (!okDepth) {
            QMessageBox::warning(this, tr("Invalid Input"), tr("Depth value is not valid."));
            return false;
        }
    }

    inOutPoi.label = name;
    inOutPoi.category = categoryFromIndex(categoryCombo->currentIndex());
    inOutPoi.latitude = latitude;
    inOutPoi.longitude = longitude;
    inOutPoi.depth = depth;
    inOutPoi.description = notesEdit->toPlainText().trimmed();
    inOutPoi.updatedAt = QDateTime::currentDateTimeUtc();

    return true;
}

void POIPanel::onAddPoi()
{
    if (!ecWidget) return;

    PoiEntry newPoi;
    newPoi.flags = EC_POI_FLAG_ACTIVE | EC_POI_FLAG_PERSISTENT;

    if (!showPoiDialog(newPoi, false)) {
        return;
    }

    const int poiId = ecWidget->addPoi(newPoi);
    if (poiId < 0) {
        QMessageBox::warning(this, tr("Add POI"), tr("Failed to create POI."));
        return;
    }

    emit statusMessage(tr("POI \"%1\" added").arg(newPoi.label));
}

void POIPanel::onEditPoi()
{
    if (!ecWidget) return;
    const int poiId = currentPoiId();
    if (poiId < 0) return;

    PoiEntry poi = ecWidget->poiEntry(poiId);
    if (poi.id < 0) return;

    if (!showPoiDialog(poi, true)) {
        return;
    }

    if (!ecWidget->updatePoi(poiId, poi)) {
        QMessageBox::warning(this, tr("Edit POI"), tr("Failed to update POI."));
        return;
    }

    emit statusMessage(tr("POI \"%1\" updated").arg(poi.label));
}

void POIPanel::onDeletePoi()
{
    if (!ecWidget) return;
    const int poiId = currentPoiId();
    if (poiId < 0) return;

    auto response = QMessageBox::question(this,
                                          tr("Remove POI"),
                                          tr("Remove selected POI?"),
                                          QMessageBox::Yes | QMessageBox::No);
    if (response != QMessageBox::Yes) {
        return;
    }

    if (!ecWidget->removePoi(poiId)) {
        QMessageBox::warning(this, tr("Remove POI"), tr("Failed to remove POI."));
        return;
    }

    emit statusMessage(tr("POI removed"));
}

void POIPanel::onFocusPoi()
{
    if (!ecWidget) return;
    const int poiId = currentPoiId();
    if (poiId < 0) return;

    if (!ecWidget->focusPoi(poiId)) {
        QMessageBox::warning(this, tr("Focus POI"), tr("Unable to center map on POI."));
        return;
    }

    const PoiEntry poi = ecWidget->poiEntry(poiId);
    if (poi.id >= 0) {
        emit statusMessage(tr("Centered on %1").arg(poi.label));
    }
}

void POIPanel::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (!item || !ecWidget) return;
    if (column != kColumnName) return;

    const int poiId = item->data(kColumnName, Qt::UserRole).toInt();

    // Validate POI ID before processing
    if (poiId <= 0) return;

    const bool active = item->checkState(kColumnName) == Qt::Checked;

    // Check if POI still exists before trying to modify it
    PoiEntry existingPoi = ecWidget->poiEntry(poiId);
    if (existingPoi.id != poiId) return;

    ecWidget->setPoiActive(poiId, active);
}

void POIPanel::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    onEditPoi();
}
