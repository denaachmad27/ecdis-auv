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
class RouteDetailDialog;

// Custom tree widget for drag & drop waypoint reordering
class RouteDetailTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit RouteDetailTreeWidget(RouteDetailDialog* parent = nullptr);
    
protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    bool dropMimeData(QTreeWidgetItem* parent, int index, const QMimeData* data, Qt::DropAction action) override;
    Qt::DropActions supportedDropActions() const override;
    
private:
    RouteDetailDialog* parentDialog;
    
signals:
    void waypointReordered(int routeId, int fromIndex, int toIndex);
};

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
    void onWaypointReordered(int routeId, int fromIndex, int toIndex);
    void onDuplicateWaypoint();

private:
    void setupUI();
    void connectSignals();
    void loadAllRoutesData();
    void updateRouteTree();
    void updateStatistics();
    QString formatDistance(double distanceNM);
    QString formatTime(double hours);
    void updateWaypointRowStyling(QTreeWidgetItem* waypointItem, const RouteDetailWaypoint& waypoint);
    void updateEcWidgetWaypointOrder(int routeId, const QList<RouteDetailWaypoint>& newOrder);
    
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
    RouteDetailTreeWidget* routeTree;
    
    // Buttons
    QHBoxLayout* buttonLayout;
    QPushButton* exportAllButton;
    QPushButton* duplicateWaypointButton;
    QPushButton* closeButton;
    
    // Data
    QList<AllRouteData> allRoutesData;
    int selectedRouteId;
};

#endif // ROUTEDETAILDIALOG_H