#ifndef OBSTACLEDETECTIONPANEL_H
#define OBSTACLEDETECTIONPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QDateTime>
#include <QTimer>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QCheckBox>
#include <QStyledItemDelegate>
#include <QPainter>
#include <algorithm>

#include "ecwidget.h"
#include "guardzonemanager.h"
#include "guardzone.h"

class EcWidget;
class GuardZoneManager;

// Struktur untuk menyimpan data pick report obstacles
struct PickReportObstacle {
    QString objectType;       // Tipe object (WRECKS, OBSTNS, DEPARE, dll)
    QString objectName;       // Nama readable object
    QString featureClass;     // Feature class dari S-57
    QString information;      // Information field dari attributes (FISHING STAKES, dll)
    int guardZoneId;
    QString guardZoneName;
    QString eventType;        // "DETECTED" atau "CLEARED"
    QDateTime timestamp;
    double lat;
    double lon;
    QString dangerLevel;      // "WARNING", "DANGEROUS", "NOTE"
    QString attributes;       // Detail attributes dari pick report
    QString status;          // "ACTIVE" atau "CLEARED"
};

// Custom delegate to display danger level without prefix
class DangerLevelDelegate : public QStyledItemDelegate
{
public:
    DangerLevelDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.column() == 0) { // Danger Level column
            QString text = index.data(Qt::DisplayRole).toString();
            
            // Remove prefix for display
            if (text.startsWith("A_")) text = text.mid(2);
            else if (text.startsWith("B_")) text = text.mid(2);
            else if (text.startsWith("C_")) text = text.mid(2);
            
            QStyleOptionViewItem opt = option;
            opt.text = text;
            QStyledItemDelegate::paint(painter, opt, index);
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

// Simple tree widget class 
class CustomObstacleTreeWidget : public QTreeWidget
{
public:
    CustomObstacleTreeWidget(QWidget* parent = nullptr) : QTreeWidget(parent) {}
};

class ObstacleDetectionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ObstacleDetectionPanel(EcWidget* ecWidget, GuardZoneManager* gzManager, QWidget *parent = nullptr);
    ~ObstacleDetectionPanel();

    void refreshObstacleList();
    void clearAllObstacles();
    void addPickReportObstacle(const PickReportObstacle& obstacle);

public slots:
    void onPickReportObstacleDetected(int guardZoneId, const QString& details);
    void updateObstacleData();

private slots:
    void onFilterChanged();
    void onObstacleSelectionChanged();
    void showContextMenu(const QPoint& pos);
    void clearSelectedObstacle();
    void clearObstaclesForGuardZone();
    void exportObstacleList();
    void centerMapOnObstacle();

private:
    void setupUI();
    void addObstacleToList(const PickReportObstacle& obstacle);
    void updateObstacleInList(const QString& objectType, const QString& newStatus);
    QTreeWidgetItem* findObstacleItem(const QString& objectType);
    QString formatEventType(const QString& eventType);
    QString formatTimestamp(const QDateTime& timestamp);
    void applyFilters();
    void updateStatistics();

    // UI Components
    QVBoxLayout* mainLayout;
    QHBoxLayout* controlLayout;
    QGroupBox* filterGroup;
    QHBoxLayout* filterLayout;
    
    QLabel* eventTypeLabel;
    QComboBox* eventTypeFilter;
    QLabel* dangerLevelLabel;
    QComboBox* dangerLevelFilter;
    
    // Danger Level Checkboxes
    QLabel* dangerCheckboxLabel;
    QCheckBox* dangerousCheckbox;
    QCheckBox* warningCheckbox;
    QCheckBox* noteCheckbox;
    
    QLineEdit* searchFilter;
    QPushButton* clearAllButton;
    QPushButton* refreshButton;
    QPushButton* exportButton;

    CustomObstacleTreeWidget* obstacleList;
    QLabel* statisticsLabel;

    // Data
    EcWidget* ecWidget;
    GuardZoneManager* guardZoneManager;
    QList<PickReportObstacle> detectedObstacles;
    QTimer* updateTimer;

    // Context menu
    QMenu* contextMenu;
    QAction* clearObstacleAction;
    QAction* clearGuardZoneAction;
    QAction* centerMapAction;
    QAction* exportAction;

    // Statistics
    int totalObstacles;
    int activeObstacles;
    int dangerousObstacles;
    int warningObstacles;
    int noteObstacles;
};

Q_DECLARE_METATYPE(PickReportObstacle)

#endif // OBSTACLEDETECTIONPANEL_H