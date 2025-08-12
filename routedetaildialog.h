#ifndef ROUTEDETAILDIALOG_H
#define ROUTEDETAILDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>

// Forward declaration
class EcWidget;

// Waypoint data structure (copied from EcWidget to avoid dependency)
struct RouteDetailWaypoint {
    double lat;
    double lon;
    QString label;
    QString remark;
    double turningRadius;
    int routeId;
    bool active;
    
    RouteDetailWaypoint() : lat(0.0), lon(0.0), turningRadius(0.5), routeId(0), active(true) {}
};

struct AllRouteData {
    int routeId;
    QString name;
    QString description;
    int waypointCount;
    double totalDistance;
    double totalTime;
    bool visible;
    bool attachedToShip;
    QList<RouteDetailWaypoint> waypoints;
};

class RouteDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RouteDetailDialog(EcWidget* ecWidget, QWidget *parent = nullptr);
    
private slots:
    void onRouteTreeSelectionChanged();
    void onExportAllClicked();
    void onCloseClicked();
    void onTreeItemClicked(QTreeWidgetItem* item, int column);

private:
    void setupUI();
    void connectSignals();
    void loadAllRoutesData();
    void updateRouteTree();
    void updateStatistics();
    QString formatDistance(double distanceNM);
    QString formatTime(double hours);
    void updateWaypointRowStyling(QTreeWidgetItem* waypointItem, const RouteDetailWaypoint& waypoint);
    
    // UI Components
    EcWidget* ecWidget;
    
    // Main layout
    QVBoxLayout* mainLayout;
    
    // Summary section
    QGroupBox* summaryGroup;
    QLabel* totalRoutesLabel;
    QLabel* totalWaypointsLabel;
    QLabel* totalDistanceLabel;
    QLabel* activeRouteLabel;
    
    // Routes and Waypoints tree
    QGroupBox* routesGroup;
    QTreeWidget* routeTree;
    
    // Buttons
    QHBoxLayout* buttonLayout;
    QPushButton* exportAllButton;
    QPushButton* closeButton;
    
    // Data
    QList<AllRouteData> allRoutesData;
    int selectedRouteId;
};

#endif // ROUTEDETAILDIALOG_H