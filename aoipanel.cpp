#include "aoipanel.h"
#include "ecwidget.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QPainter>
#include <QPixmap>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QMenu>
#include <QDialogButtonBox>

AOIPanel::AOIPanel(EcWidget* ecWidget, QWidget* parent)
    : QWidget(parent), ecWidget(ecWidget)
{
    QVBoxLayout* main = new QVBoxLayout(this);

    QGroupBox* group = new QGroupBox("Area Object Management");
    QVBoxLayout* v = new QVBoxLayout(group);

    tree = new QTreeWidget(this);
    tree->setColumnCount(4);
    tree->setHeaderLabels({"Name", "Color", "Show", "Label"});
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(true);
    // Better spacing: Name stretches, Type/Show sized to contents
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tree->setColumnWidth(1, 110);
    tree->setColumnWidth(2, 60);
    tree->setColumnWidth(3, 60);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    v->addWidget(tree);

    // Arrange buttons into 2 columns; add rows as needed (max 3 rows here)
    QGridLayout* btns = new QGridLayout();
    addBtn = new QPushButton("Create by Form");
    createByClickBtn = new QPushButton("Create by Click");
    editBtn = new QPushButton("Edit");
    deleteBtn = new QPushButton("Delete");
    exportBtn = new QPushButton("Export");

    attachBtn = new QPushButton("Attach");
    detachBtn = new QPushButton("Detach");

    attachBtn->setEnabled(true);
    detachBtn->setEnabled(false);

    attachBtn->setVisible(false);
    detachBtn->setVisible(false);

    // Row 1
    btns->addWidget(addBtn,           0, 0);
    btns->addWidget(createByClickBtn, 0, 1);
    // Row 2
    btns->addWidget(exportBtn,        1, 0);
    btns->addWidget(deleteBtn,        1, 1);
    // Row 3
    btns->addWidget(attachBtn,        2, 0);
    btns->addWidget(detachBtn,        2, 1);

    //btns->addWidget(editBtn,          1, 0);

    v->addLayout(btns);


    main->addWidget(group);

    connect(addBtn, &QPushButton::clicked, this, &AOIPanel::onAddAOI);
    connect(deleteBtn, &QPushButton::clicked, this, &AOIPanel::onDeleteAOI);
    connect(createByClickBtn, &QPushButton::clicked, this, &AOIPanel::onCreateByClick);
    connect(tree, &QTreeWidget::itemChanged, this, &AOIPanel::onItemChanged);
    connect(tree, &QTreeWidget::currentItemChanged, this, &AOIPanel::onCurrentItemChanged);
    connect(exportBtn, &QPushButton::clicked, this, &AOIPanel::onExportAOI);

    connect(attachBtn, &QPushButton::clicked, this, &AOIPanel::onAttach);
    connect(detachBtn, &QPushButton::clicked, this, &AOIPanel::onDetach);
    connect(tree, &QTreeWidget::customContextMenuRequested, this, &AOIPanel::onTreeContextMenu);

    // Auto-refresh when AOI list changes in EcWidget (e.g., create-by-click finishes)
    // Use QueuedConnection to avoid re-entrant refresh while handling itemChanged
    if (ecWidget) {
        connect(ecWidget, SIGNAL(aoiListChanged()), this, SLOT(refreshList()), Qt::QueuedConnection);
    }

    connect(editBtn, &QPushButton::clicked, [this]() {
        auto* item = this->tree->currentItem();
        if (!item || !this->ecWidget) return;
        int id = item->data(0, Qt::UserRole).toInt();
        this->ecWidget->startEditAOI(id);
        emit statusMessage("Edit Area Object: drag vertices; right-click handle for Move/Delete; right-click edge to add; click to drop; ESC to finish");
    });

    refreshList();
}

void AOIPanel::refreshList()
{
    QSignalBlocker guard(tree); // RAII: block and restore signals safely
    tree->clear();
    // Determine theme-based text color for list items
    QColor win = palette().color(QPalette::Window);
    int luma = qRound(0.2126*win.red() + 0.7152*win.green() + 0.0722*win.blue());
    QColor listTextColor = (luma < 128) ? QColor(255,255,255) : QColor(0,0,0);
    if (!ecWidget) return;
    const auto aoiList = ecWidget->getAOIs();
    for (const auto& aoi : aoiList) {
        auto* item = new QTreeWidgetItem(tree);
        QString name = aoi.name;
        if (ecWidget && ecWidget->isAOIAttachedToShip(aoi.id)) {
            name += " (Active)";
        }
        item->setText(0, name);

        // Column 1: Display color box with filled color
        // Create a colored square icon
        QPixmap colorPixmap(16, 16);
        colorPixmap.fill(aoi.color);
        QPainter painter(&colorPixmap);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(0, 0, 15, 15);
        item->setIcon(1, QIcon(colorPixmap));
        item->setText(1, "");  // No text, just the icon

        // Checkbox for show/hide in column 2 and label in column 3
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setCheckState(2, aoi.visible ? Qt::Checked : Qt::Unchecked);
        item->setText(2, "");
        item->setCheckState(3, aoi.showLabel ? Qt::Checked : Qt::Unchecked);
        item->setText(3, "");
        item->setData(0, Qt::UserRole, aoi.id);
        // Colorize by type
        item->setForeground(0, QBrush(listTextColor));
        item->setForeground(1, QBrush(listTextColor));
        item->setForeground(2, QBrush(listTextColor));
        item->setForeground(3, QBrush(listTextColor));
    }
    updateAttachButtons();
}

void AOIPanel::onAddAOI()
{
    if (!ecWidget) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Add Area Object");
    QFormLayout* form = new QFormLayout(&dlg);
    QLineEdit* nameEdit = new QLineEdit();
    QComboBox* typeCombo = new QComboBox();

    // Add color choices with color icon
    typeCombo->addItem("🔴 Red", "Red");
    typeCombo->addItem("🔵 Blue", "Blue");
    typeCombo->addItem("🟢 Green", "Green");
    typeCombo->addItem("🟡 Yellow", "Yellow");
    typeCombo->addItem("🟠 Orange", "Orange");

    QTextEdit* coordsEdit = new QTextEdit();
    coordsEdit->setPlaceholderText("Enter vertices (one per line):\nlat,lon\nlat,lon\n...");

    form->addRow("Name:", nameEdit);
    form->addRow("Color:", typeCombo);
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
    aoi.type = AOIType::AOI;  // Default type
    // Get color from combo selection
    AOIColorChoice colorChoice = aoiColorChoiceFromString(typeCombo->currentData().toString());
    aoi.color = aoiColorFromChoice(colorChoice);
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
    emit statusMessage(QString("Area Object '%1' created (%2 points)").arg(aoi.name).arg(aoi.vertices.size()));
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
                                                   tr("Export Areas to JSON"),
                                                   QDir::homePath() + "/Areas_" + suggested + ".json",
                                                   tr("JSON Files (*.json);;All Files (*.*)"));
    if (filename.isEmpty()) return;
    if (!filename.endsWith(".json", Qt::CaseInsensitive)) filename += ".json";
    bool ok = ecWidget->exportAOIsToFile(filename);
    if (ok) emit statusMessage(tr("Areas exported to %1").arg(QFileInfo(filename).fileName()));
}

void AOIPanel::onCreateByClick()
{
    if (!ecWidget) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Create by Click");
    QFormLayout* form = new QFormLayout(&dlg);
    QLineEdit* nameEdit = new QLineEdit();
    // Prefill default AOI name
    nameEdit->setText(QString("Area %1").arg(ecWidget->getNextAoiId()));
    QComboBox* typeCombo = new QComboBox();

    // Add color choices with color icon
    typeCombo->addItem("🔴 Red", "Red");
    typeCombo->addItem("🔵 Blue", "Blue");
    typeCombo->addItem("🟢 Green", "Green");
    typeCombo->addItem("🟡 Yellow", "Yellow");
    typeCombo->addItem("🟠 Orange", "Orange");

    form->addRow("Name:", nameEdit);
    form->addRow("Color:", typeCombo);
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
    if (name.isEmpty()) name = "Area";
    AOIColorChoice colorChoice = aoiColorChoiceFromString(typeCombo->currentData().toString());
    ecWidget->startAOICreationWithColor(name, colorChoice);
    emit statusMessage("Area mode: Left-click to add points, Right-click to finish (min 3 points)");
}

void AOIPanel::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (!item || !ecWidget) return;
    int id = item->data(0, Qt::UserRole).toInt();
    const auto list = ecWidget->getAOIs();
    if (column == 2) {
        bool visible = (item->checkState(2) == Qt::Checked);
        bool currentVisible = visible;
        for (const auto& a : list) { if (a.id == id) { currentVisible = a.visible; break; } }
        if (currentVisible != visible) ecWidget->setAOIVisibility(id, visible);
        ecWidget->update();
    } else if (column == 3) {
        bool showLabel = (item->checkState(3) == Qt::Checked);
        bool currentShow = showLabel;
        for (const auto& a : list) { if (a.id == id) { currentShow = a.showLabel; break; } }
        if (currentShow != showLabel) ecWidget->setAOILabelVisibility(id, showLabel);
        ecWidget->update();
    } else {
        return;
    }
    updateAttachButtons();
}

void AOIPanel::setAttachDetachButton(bool connection){
    attachBtn->setVisible(connection);
    detachBtn->setVisible(connection);
}

void AOIPanel::publishToMOOSDB(){
    auto* item = this->tree->currentItem();
    if (!item) return;
    int id = item->data(0, Qt::UserRole).toInt();

    QList<AOI> selectedData = getAOIById(id);
    QStringList coordPairs;

    for (const AOI& aoi : selectedData) {
        for (const QPointF &pt : aoi.vertices){
            coordPairs << QString::number(pt.x(), 'f', 6) + ", " + QString::number(pt.y(), 'f', 6);
        }
    }

    QString result = "pts={" + coordPairs.join(": ") + "}";
    ecWidget->publishToMOOS("AREA_NAV", result);
}

void AOIPanel::onAttach(){
    if (!ecWidget) return;
    auto* item = this->tree->currentItem();
    if (!item) return;
    int id = item->data(0, Qt::UserRole).toInt();

    if (id > 0 && ecWidget) {
        // Attach this route to ship (detaches others)
        //ecWidget->attachRouteToShip(selectedRouteId);
        ecWidget->attachAOIToShip(id);
        publishToMOOSDB();

        // Update button states
        updateAttachButtons();

        // Use a timer to refresh the list after attachment is complete
        QTimer::singleShot(100, [this]() {
            refreshList();
        });
    }
}

void AOIPanel::onDetach(){
    if (ecWidget) {
        // Preserve visibility before detaching
        //bool currentVisibility = ecWidget->isRouteVisible(selectedRouteId);

        // Detach this route from ship (this will make all routes blue again)
        //ecWidget->attachRouteToShip(-1); // Detach all routes
        ecWidget->attachAOIToShip(-1);
        ecWidget->publishToMOOS("AREA_NAV", "");

        //ecWidget->publishToMOOS("OWNSHIP_OOB", "");
        //ecWidget->cachedOwnshipOutsideAoiCopy = false;

        // Ensure visibility is maintained
        //ecWidget->setRouteVisibility(selectedRouteId, currentVisibility);

        // Update button states
        updateAttachButtons();

        // Use a timer to refresh the list after detachment is complete
        QTimer::singleShot(100, [this]() {
            refreshList();
        });
    }
}

QList<AOI> AOIPanel::getAOIById(int aoiId)
{
    if (!ecWidget) return {};

    QList<AOI> aois = ecWidget->getAOIs();
    QList<AOI> aoiSelect;

    for (const auto& aoi : aois) {
        if (aoi.id == aoiId) {
            aoiSelect.append(aoi);
        }
    }

    return aoiSelect;
}

void AOIPanel::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* /*previous*/)
{
    Q_UNUSED(current);
    updateAttachButtons();
}

void AOIPanel::updateAttachButtons()
{
    if (!ecWidget) {
        attachBtn->setEnabled(false);
        detachBtn->setEnabled(false);
        editBtn->setEnabled(false);
        deleteBtn->setEnabled(false);
        addBtn->setEnabled(false);
        createByClickBtn->setEnabled(false);
        exportBtn->setEnabled(false);
        return;
    }

    auto* item = tree->currentItem();
    bool hasSelection = (item != nullptr);

    if (!hasSelection) {
        attachBtn->setEnabled(false);
        detachBtn->setEnabled(false);
        editBtn->setEnabled(false);
        deleteBtn->setEnabled(false);
        // Allow create buttons even when no selection
        addBtn->setEnabled(true);
        createByClickBtn->setEnabled(true);
        exportBtn->setEnabled(tree->topLevelItemCount() > 0);
        return;
    }

    int id = item->data(0, Qt::UserRole).toInt();
    bool isAttached = ecWidget->isAOIAttachedToShip(id);

    // Attach/Detach buttons
    attachBtn->setEnabled(!isAttached);
    detachBtn->setEnabled(isAttached);

    // Disable modification buttons if AOI is attached
    editBtn->setEnabled(!isAttached);
    deleteBtn->setEnabled(!isAttached);

    // Create buttons should be enabled if no AOI is attached
    bool anyAttached = false;
    const auto aoiList = ecWidget->getAOIs();
    for (const auto& aoi : aoiList) {
        if (ecWidget->isAOIAttachedToShip(aoi.id)) {
            anyAttached = true;
            break;
        }
    }
    addBtn->setEnabled(!anyAttached);
    createByClickBtn->setEnabled(!anyAttached);

    // Export button always enabled if there are AOIs
    exportBtn->setEnabled(tree->topLevelItemCount() > 0);
}

void AOIPanel::onTreeContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = tree->itemAt(pos);
    if (!item || !ecWidget) return;

    // Check if AOI is attached - disable editing if attached
    int id = item->data(0, Qt::UserRole).toInt();
    bool isAttached = ecWidget->isAOIAttachedToShip(id);

    QMenu contextMenu(this);
    QAction* editAction = contextMenu.addAction(tr("Edit"));
    QAction* deleteAction = contextMenu.addAction(tr("Delete"));

    editAction->setEnabled(!isAttached);
    deleteAction->setEnabled(!isAttached);

    QAction* selectedAction = contextMenu.exec(tree->mapToGlobal(pos));

    if (selectedAction == editAction) {
        onEditAOI();
    } else if (selectedAction == deleteAction) {
        onDeleteAOI();
    }
}

void AOIPanel::onEditAOI()
{
    QTreeWidgetItem* item = tree->currentItem();
    if (!item || !ecWidget) return;

    int aoiId = item->data(0, Qt::UserRole).toInt();
    const auto aoiList = ecWidget->getAOIs();

    // Find the AOI by ID
    AOI editAOI;
    bool found = false;
    for (const auto& aoi : aoiList) {
        if (aoi.id == aoiId) {
            editAOI = aoi;
            found = true;
            break;
        }
    }
    if (!found) return;

    // Create edit dialog
    QDialog dlg(this);
    dlg.setWindowTitle("Edit Area Object");
    QFormLayout* form = new QFormLayout(&dlg);

    // Name field
    QLineEdit* nameEdit = new QLineEdit(editAOI.name);

    // Color dropdown
    QComboBox* colorCombo = new QComboBox();
    colorCombo->addItem("🔴 Red", "Red");
    colorCombo->addItem("🔵 Blue", "Blue");
    colorCombo->addItem("🟢 Green", "Green");
    colorCombo->addItem("🟡 Yellow", "Yellow");
    colorCombo->addItem("🟠 Orange", "Orange");

    // Set current color - find matching color choice
    AOIColorChoice currentColor = AOIColorChoice::Yellow; // default
    if (editAOI.color == QColor(255, 0, 0)) currentColor = AOIColorChoice::Red;
    else if (editAOI.color == QColor(0, 120, 255)) currentColor = AOIColorChoice::Blue;
    else if (editAOI.color == QColor(0, 200, 0)) currentColor = AOIColorChoice::Green;
    else if (editAOI.color == QColor(255, 220, 0)) currentColor = AOIColorChoice::Yellow;
    else if (editAOI.color == QColor(255, 140, 0)) currentColor = AOIColorChoice::Orange;

    QString currentColorStr = aoiColorChoiceToString(currentColor);
    for (int i = 0; i < colorCombo->count(); ++i) {
        if (colorCombo->itemData(i).toString() == currentColorStr) {
            colorCombo->setCurrentIndex(i);
            break;
        }
    }

    // Visibility checkbox
    QCheckBox* visibleCheck = new QCheckBox();
    visibleCheck->setChecked(editAOI.visible);

    // Show Label checkbox
    QCheckBox* showLabelCheck = new QCheckBox();
    showLabelCheck->setChecked(editAOI.showLabel);

    // Add widgets to form
    form->addRow("Name:", nameEdit);
    form->addRow("Color:", colorCombo);
    form->addRow("Visible:", visibleCheck);
    form->addRow("Show Label:", showLabelCheck);

    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg);
    form->addRow(buttons);

    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        // Update AOI with new values
        editAOI.name = nameEdit->text().trimmed();
        AOIColorChoice colorChoice = aoiColorChoiceFromString(colorCombo->currentData().toString());
        editAOI.color = aoiColorFromChoice(colorChoice);
        editAOI.visible = visibleCheck->isChecked();
        editAOI.showLabel = showLabelCheck->isChecked();

        // Update in EcWidget
        ecWidget->updateAOI(editAOI);

        // Refresh list
        refreshList();
        emit statusMessage(QString("Area Object '%1' updated").arg(editAOI.name));
    }
}
