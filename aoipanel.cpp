#include "aoipanel.h"
#include "ecwidget.h"
#include <QMessageBox>

AOIPanel::AOIPanel(EcWidget* ecWidget, QWidget* parent)
    : QWidget(parent), ecWidget(ecWidget)
{
    QVBoxLayout* main = new QVBoxLayout(this);

    QGroupBox* group = new QGroupBox("AOI / ROI Management");
    QVBoxLayout* v = new QVBoxLayout(group);

    tree = new QTreeWidget(this);
    tree->setColumnCount(3);
    tree->setHeaderLabels({"Name", "Type", "Visible"});
    v->addWidget(tree);

    QGridLayout* btns = new QGridLayout();
    addBtn = new QPushButton("Add AOI (Form)");
    createByClickBtn = new QPushButton("Create by Click");
    editBtn = new QPushButton("Edit");
    deleteBtn = new QPushButton("Delete");
    toggleBtn = new QPushButton("Toggle Vis");

    // Baris 1
    btns->addWidget(addBtn,          0, 0);
    btns->addWidget(createByClickBtn,0, 1);

    // Baris 2
    btns->addWidget(editBtn,         1, 0);
    btns->addWidget(deleteBtn,       1, 1);
    btns->addWidget(toggleBtn,       1, 2);

    v->addLayout(btns);


    main->addWidget(group);

    connect(addBtn, &QPushButton::clicked, this, &AOIPanel::onAddAOI);
    connect(deleteBtn, &QPushButton::clicked, this, &AOIPanel::onDeleteAOI);
    connect(toggleBtn, &QPushButton::clicked, this, &AOIPanel::onToggleVisibility);
    connect(createByClickBtn, &QPushButton::clicked, this, &AOIPanel::onCreateByClick);

    // Auto-refresh when AOI list changes in EcWidget (e.g., create-by-click finishes)
    if (ecWidget) {
        connect(ecWidget, SIGNAL(aoiListChanged()), this, SLOT(refreshList()));
    }

    connect(editBtn, &QPushButton::clicked, [this]() {
        auto* item = this->tree->currentItem();
        if (!item || !this->ecWidget) return;
        int id = item->data(0, Qt::UserRole).toInt();
        this->ecWidget->startEditAOI(id);
        emit statusMessage("Edit AOI: drag vertices; right-click handle to delete; right-click edge to add; ESC to finish");
    });

    refreshList();
}

void AOIPanel::refreshList()
{
    tree->clear();
    if (!ecWidget) return;
    const auto aoiList = ecWidget->getAOIs();
    for (const auto& aoi : aoiList) {
        auto* item = new QTreeWidgetItem(tree);
        item->setText(0, aoi.name);
        item->setText(1, aoiTypeToString(aoi.type));
        item->setText(2, aoi.visible ? "Yes" : "No");
        item->setData(0, Qt::UserRole, aoi.id);
        // Colorize by type
        item->setForeground(0, QBrush(aoi.color));
        item->setForeground(1, QBrush(aoi.color));
    }
    tree->resizeColumnToContents(0);
    tree->resizeColumnToContents(1);
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

void AOIPanel::onToggleVisibility()
{
    if (!ecWidget) return;
    auto* item = this->tree->currentItem();
    if (!item) return;
    int id = item->data(0, Qt::UserRole).toInt();
    ecWidget->toggleAOIVisibility(id);
    refreshList();
    ecWidget->update();
}
