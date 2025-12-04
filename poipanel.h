#ifndef POIPANEL_H
#define POIPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

#include "poi.h"

class EcWidget;

class POIPanel : public QWidget
{
    Q_OBJECT
public:
    explicit POIPanel(EcWidget* ecWidget, QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& message);

public slots:
    void refreshList();
    void editPoiById(int poiId, bool* handled = nullptr);

private slots:
    void onAddPoi();
    void onEditPoi();
    void onDeletePoi();
    void onFocusPoi();
    void onSelectionChanged();
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    EcWidget* ecWidget = nullptr;
    QTreeWidget* tree = nullptr;
    QPushButton* addBtn = nullptr;
    QPushButton* editBtn = nullptr;
    QPushButton* deleteBtn = nullptr;
    QPushButton* focusBtn = nullptr;
    bool populating = false;

    int currentPoiId() const;
    void populateTree();
    QString formatCoordinate(double value, bool isLatitude) const;
    QString formatDepth(double value) const;
    QString categoryToString(EcPoiCategory category) const;
    EcPoiCategory categoryFromIndex(int index) const;
    bool showPoiDialog(PoiEntry& inOutPoi, bool isEdit);
    void setButtonsEnabled(bool enabled);
};

#endif // POIPANEL_H
