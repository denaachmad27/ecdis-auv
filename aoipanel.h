#ifndef AOIPANEL_H
#define AOIPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialog>
#include <QFormLayout>

#include "aoi.h"

class EcWidget;

class AOIPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AOIPanel(EcWidget* ecWidget, QWidget* parent = nullptr);

    void setAttachDetachButton(bool connection);

public slots:
    void refreshList();

signals:
    void statusMessage(const QString& message);

  private slots:
      void onAddAOI();
      void onCreateByClick();
      void onDeleteAOI();
      void onExportAOI();
      void onItemChanged(QTreeWidgetItem* item, int column);
      void onAttach();
      void onDetach();
      void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

private:
    EcWidget* ecWidget;
    QTreeWidget* tree;
    QPushButton* addBtn;
      QPushButton* createByClickBtn;
      QPushButton* editBtn;
      QPushButton* deleteBtn;
      QPushButton* exportBtn;

    QPushButton* attachBtn;
    QPushButton* detachBtn;

    QList<AOI> getAOIById(int aoiId);
    void publishToMOOSDB();
    void updateAttachButtons();
};

#endif // AOIPANEL_H
