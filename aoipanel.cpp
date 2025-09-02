#include "aoipanel.h"
#include "ecwidget.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

AOIPanel::AOIPanel(EcWidget* ecWidget, QWidget* parent)
    : QWidget(parent), ecWidget(ecWidget)
{
    QVBoxLayout* main = new QVBoxLayout(this);

    QGroupBox* group = new QGroupBox("AOI / ROI Management");
    QVBoxLayout* v = new QVBoxLayout(group);

    tree = new QTreeWidget(this);
    tree->setColumnCount(3);
    tree->setHeaderLabels({"Name", "Type", "Show"});
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(true);
    // Better spacing: Name stretches, Type/Show sized to contents
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree->setColumnWidth(1, 110);
    tree->setColumnWidth(2, 60);
    v->addWidget(tree);

    // Arrange buttons into 2 columns; add rows as needed (max 3 rows here)
    QGridLayout* btns = new QGridLayout();
    addBtn = new QPushButton("Add AOI (Form)");
    createByClickBtn = new QPushButton("Create by Click");
    editBtn = new QPushButton("Edit");
    deleteBtn = new QPushButton("Delete");
    exportBtn = new QPushButton("Export");

    // Row 1
    btns->addWidget(addBtn,           0, 0);
    btns->addWidget(createByClickBtn, 0, 1);
    // Row 2
    btns->addWidget(editBtn,          1, 0);
    btns->addWidget(deleteBtn,        1, 1);
    // Row 3 (optional when space not enough; still 2 columns)
    btns->addWidget(exportBtn,        2, 0, 1, 2); // span 2 columns to keep layout tidy

    v->addLayout(btns);


    main->addWidget(group);

    connect(addBtn, &QPushButton::clicked, this, &AOIPanel::onAddAOI);
    connect(deleteBtn, &QPushButton::clicked, this, &AOIPanel::onDeleteAOI);
    connect(createByClickBtn, &QPushButton::clicked, this, &AOIPanel::onCreateByClick);
    connect(tree, &QTreeWidget::itemChanged, this, &AOIPanel::onItemChanged);
    connect(exportBtn, &QPushButton::clicked, this, &AOIPanel::onExportAOI);

    // Auto-refresh when AOI list changes in EcWidget (e.g., create-by-click finishes)
    if (ecWidget) {
        connect(ecWidget, SIGNAL(aoiListChanged()), this, SLOT(refreshList()));
    }

    connect(editBtn, &QPushButton::clicked, [this]() {
        auto* item = this->tree->currentItem();
        if (!item || !this->ecWidget) return;
        int id = item->data(0, Qt::UserRole).toInt();
        this->ecWidget->startEditAOI(id);
        emit statusMessage("Edit AOI: drag vertices; right-click handle for Move/Delete; right-click edge to add; click to drop; ESC to finish");
    });

    refreshList();
}

void AOIPanel::refreshList()
{
    tree->blockSignals(true);
    tree->clear();
    // Determine theme-based text color for list items
    QColor win = palette().color(QPalette::Window);
    int luma = qRound(0.2126*win.red() + 0.7152*win.green() + 0.0722*win.blue());
    QColor listTextColor = (luma < 128) ? QColor(255,255,255) : QColor(0,0,0);
    if (!ecWidget) return;
    const auto aoiList = ecWidget->getAOIs();
    for (const auto& aoi : aoiList) {
        auto* item = new QTreeWidgetItem(tree);
        item->setText(0, aoi.name);
        item->setText(1, aoiTypeToString(aoi.type));
        // Checkbox for show/hide in column 2
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setCheckState(2, aoi.visible ? Qt::Checked : Qt::Unchecked);
        item->setText(2, "");
        item->setData(0, Qt::UserRole, aoi.id);
        // Colorize by type
        item->setForeground(0, QBrush(listTextColor));
        item->setForeground(1, QBrush(listTextColor));
        item->setForeground(2, QBrush(listTextColor));
    }
    tree->blockSignals(false);
}

void AOIPanel::onAddAOI()
{
    if (!ecWidget) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Add AOI / ROI");
    QFormLayout* form = new QFormLayout(&dlg);
    QLineEdit* nameEdit = new QLineEdit();
    QComboBox* typeCombo = new QComboBox();

    typeCombo->addItem("Area of Interest (AOI)", "AOI");
    typeCombo->addItem("Restricted Operations Zone (ROZ)", "ROZ");
    typeCombo->addItem("Missile Engagement Zone (MEZ)", "MEZ");
    typeCombo->addItem("Weapons Engagement Zone (WEZ)", "WEZ");
    typeCombo->addItem("Patrol Area", "Patrol Area");
    typeCombo->addItem("Sector of Action (SOA)", "SOA");
    typeCombo->addItem("Area of Responsibility (AOR)", "AOR");
    typeCombo->addItem("Joint Operations Area (JOA)", "JOA");

    QTextEdit* coordsEdit = new QTextEdit();
    coordsEdit->setPlaceholderText("Enter vertices (one per line):\nlat,lon\nlat,lon\n...");

    form->addRow("Name:", nameEdit);
    form->addRow("Type:", typeCombo);
    form->addRow("Vertices:", coordsEdit);

    QHBoxLayout* btns = new QHBoxLayout();
    QPushButton* ok = new QPushButton("Create");
    QPushButton* cancel = new QPushButton("Cancel");
    btns->addStretch();
    btns->addWidget(ok);
    btns->addWidget(cancel);
    form->addRow(btns);

    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    AOI aoi;
    aoi.id = ecWidget->getNextAoiId();
    aoi.name = nameEdit->text().trimmed();
    aoi.type = aoiTypeFromString(typeCombo->currentData().toString());
    aoi.color = aoiDefaultColor(aoi.type);
    aoi.visible = true;

    // Parse coordinates
    QStringList lines = coordsEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const auto parts = line.split(',', Qt::SkipEmptyParts);
        if (parts.size() != 2) continue;
        bool ok1=false, ok2=false;
        double lat = parts[0].trimmed().toDouble(&ok1);
        double lon = parts[1].trimmed().toDouble(&ok2);
        if (ok1 && ok2) {
            aoi.vertices.append(QPointF(lat, lon));
        }
    }

    if (aoi.vertices.size() < 3) {
        QMessageBox::warning(this, "Invalid Polygon", "Please input at least 3 vertices (lat,lon per line).");
        return;
    }

    ecWidget->addAOI(aoi);
    refreshList();
    ecWidget->update();
    emit statusMessage(QString("AOI '%1' created (%2 points)").arg(aoi.name).arg(aoi.vertices.size()));
}

void AOIPanel::onDeleteAOI()
{
    if (!ecWidget) return;
    auto* item = this->tree->currentItem();
    if (!item) return;
    int id = item->data(0, Qt::UserRole).toInt();
    ecWidget->removeAOI(id);
    refreshList();
    ecWidget->update();
}

void AOIPanel::onExportAOI()
{
    if (!ecWidget) return;
    QString suggested = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename = QFileDialog::getSaveFileName(this,
                                                   tr("Export AOIs to JSON"),
                                                   QDir::homePath() + "/AOIs_" + suggested + ".json",
                                                   tr("JSON Files (*.json);;All Files (*.*)"));
    if (filename.isEmpty()) return;
    if (!filename.endsWith(".json", Qt::CaseInsensitive)) filename += ".json";
    bool ok = ecWidget->exportAOIsToFile(filename);
    if (ok) emit statusMessage(tr("AOIs exported to %1").arg(QFileInfo(filename).fileName()));
}

void AOIPanel::onCreateByClick()
{
    if (!ecWidget) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Create AOI by Click");
    QFormLayout* form = new QFormLayout(&dlg);
    QLineEdit* nameEdit = new QLineEdit();
    // Prefill default AOI name
    nameEdit->setText(QString("AOI %1").arg(ecWidget->getNextAoiId()));
    QComboBox* typeCombo = new QComboBox();

    typeCombo->addItem("Area of Interest (AOI)", "AOI");
    typeCombo->addItem("Restricted Operations Zone (ROZ)", "ROZ");
    typeCombo->addItem("Missile Engagement Zone (MEZ)", "MEZ");
    typeCombo->addItem("Weapons Engagement Zone (WEZ)", "WEZ");
    typeCombo->addItem("Patrol Area", "Patrol Area");
    typeCombo->addItem("Sector of Action (SOA)", "SOA");
    typeCombo->addItem("Area of Responsibility (AOR)", "AOR");
    typeCombo->addItem("Joint Operations Area (JOA)", "JOA");

    form->addRow("Name:", nameEdit);
    form->addRow("Type:", typeCombo);
    QHBoxLayout* btns = new QHBoxLayout();
    QPushButton* ok = new QPushButton("Start");
    QPushButton* cancel = new QPushButton("Cancel");
    btns->addStretch();
    btns->addWidget(ok);
    btns->addWidget(cancel);
    form->addRow(btns);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) name = "AOI";
    AOIType type = aoiTypeFromString(typeCombo->currentData().toString());
    ecWidget->startAOICreation(name, type);
    emit statusMessage("AOI mode: Left-click to add points, Right-click to finish (min 3 points)");
}

void AOIPanel::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (!item || !ecWidget) return;
    if (column != 2) return; // Only handle checkbox column
    int id = item->data(0, Qt::UserRole).toInt();
    bool visible = (item->checkState(2) == Qt::Checked);
    // Prefer explicit setter if available; fallback to toggle if necessary
    // This relies on EcWidget::setAOIVisibility being present (added below).
    ecWidget->setAOIVisibility(id, visible);
    ecWidget->update();
}
