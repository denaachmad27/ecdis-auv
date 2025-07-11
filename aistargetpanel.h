#ifndef AISTARGETPANEL_H
#define AISTARGETPANEL_H

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

#include "ecwidget.h"
#include "guardzonemanager.h"
#include "guardzone.h"

class EcWidget;
class GuardZoneManager;

// Struktur untuk menyimpan data AIS target yang terdeteksi
struct AISTargetDetection {
    int mmsi;
    QString shipName;
    QString shipType;
    int guardZoneId;
    QString guardZoneName;
    QString eventType;        // "ENTERED" atau "EXITED"
    QDateTime timestamp;
    double lat;
    double lon;
    double sog;              // Speed over ground
    double cog;              // Course over ground
    QString status;          // "ACTIVE" atau "CLEARED"
};

class AISTargetPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AISTargetPanel(EcWidget* ecWidget, GuardZoneManager* gzManager, QWidget *parent = nullptr);
    ~AISTargetPanel();

    void refreshTargetList();
    void clearAllTargets();

public slots:
    void onGuardZoneAlert(int guardZoneId, int mmsi, const QString& message);
    void onTargetEntered(int guardZoneId, int mmsi, const QString& details);
    void onTargetExited(int guardZoneId, int mmsi, const QString& details);
    void updateTargetData();

private slots:
    void onFilterChanged();
    void onTargetSelectionChanged();
    void showContextMenu(const QPoint& pos);
    void clearSelectedTarget();
    void clearTargetsForGuardZone();
    void exportTargetList();
    void centerMapOnTarget();

private:
    void setupUI();
    void addTargetToList(const AISTargetDetection& detection);
    void updateTargetInList(int mmsi, int guardZoneId, const QString& newStatus);
    QTreeWidgetItem* findTargetItem(int mmsi, int guardZoneId);
    QString formatEventType(const QString& eventType);
    QString formatTimestamp(const QDateTime& timestamp);
    void populateGuardZoneFilter();
    void applyFilters();
    void updateStatistics();

    // UI Components
    QVBoxLayout* mainLayout;
    QHBoxLayout* controlLayout;
    QGroupBox* filterGroup;
    QHBoxLayout* filterLayout;
    
    QLabel* guardZoneLabel;
    QComboBox* guardZoneFilter;
    QLabel* eventTypeLabel;
    QComboBox* eventTypeFilter;
    QLabel* statusLabel;
    QComboBox* statusFilter;
    QLineEdit* searchFilter;
    QPushButton* clearAllButton;
    QPushButton* refreshButton;
    QPushButton* exportButton;

    QTreeWidget* targetList;
    QLabel* statisticsLabel;

    // Data
    EcWidget* ecWidget;
    GuardZoneManager* guardZoneManager;
    QList<AISTargetDetection> detectedTargets;
    QTimer* updateTimer;

    // Context menu
    QMenu* contextMenu;
    QAction* clearTargetAction;
    QAction* clearGuardZoneAction;
    QAction* centerMapAction;
    QAction* exportAction;

    // Statistics
    int totalTargets;
    int activeTargets;
    int enteredTargets;
    int exitedTargets;
};

Q_DECLARE_METATYPE(AISTargetDetection)

#endif // AISTARGETPANEL_H