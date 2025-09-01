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
public slots:
    void refreshList();

signals:
    void statusMessage(const QString& message);

private slots:
    void onAddAOI();
    void onCreateByClick();
    void onDeleteAOI();
    void onItemChanged(QTreeWidgetItem* item, int column);

private:
    EcWidget* ecWidget;
    QTreeWidget* tree;
    QPushButton* addBtn;
    QPushButton* createByClickBtn;
    QPushButton* editBtn;
    QPushButton* deleteBtn;
};

#endif // AOIPANEL_H
