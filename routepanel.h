#ifndef ROUTEPANEL_H
#define ROUTEPANEL_H

#include "ecwidget.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QTimer>
#include <QSet>

// Forward declarations
class EcWidget;

struct RouteInfo {
    int routeId;
    QString name;
    int waypointCount;
    double totalDistance;
    double totalTime; // in hours
    bool visible;
    bool attachedToShip;
    
    RouteInfo() : routeId(0), name(""), waypointCount(0), totalDistance(0.0), totalTime(0.0), visible(true), attachedToShip(false) {}
};


// Tree widget item for routes (parent items)
class RouteTreeItem : public QTreeWidgetItem
{
public:
    RouteTreeItem(const RouteInfo& routeInfo, QTreeWidget* parent = nullptr, EcWidget* ecWidget = nullptr);
    
    void updateFromRouteInfo(const RouteInfo& routeInfo);
    int getRouteId() const { return routeId; }
    
private:
    int routeId;
    EcWidget* ecWidget;
    void updateDisplayText(const RouteInfo& routeInfo);
};

// Tree widget item for waypoints (child items)
class WaypointTreeItem : public QTreeWidgetItem
{
public:
    WaypointTreeItem(const EcWidget::Waypoint& waypoint, RouteTreeItem* parent = nullptr);
    const EcWidget::Waypoint& getWaypoint() const { return waypointData; }
    
private:
    EcWidget::Waypoint waypointData;
    void updateDisplayText();
};

class RoutePanel : public QWidget
{
    Q_OBJECT

public:
    explicit RoutePanel(EcWidget* ecWidget, QWidget *parent = nullptr);
    ~RoutePanel();

    void refreshRouteList();
    void updateRouteInfo(int routeId);

public slots:
    void onRouteCreated();
    void onRouteModified();
    void onRouteDeleted();
    
    // Manual update functions for specific changes
    void onWaypointAdded();
    void onWaypointRemoved();
    void onWaypointMoved();

private slots:
    void onRouteItemSelectionChanged();
    void onRouteItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onShowContextMenu(const QPoint& pos);
    void onAddRouteClicked();
    void onRouteDetailClicked();
    void onRefreshClicked();
    void onClearAllClicked();
    
    // Context menu actions
    void onRenameRoute();
    void onToggleRouteVisibility();
    void onDeleteRoute();
    void onRouteProperties();

signals:
    void routeSelectionChanged(int routeId);
    void routeVisibilityChanged(int routeId, bool visible);
    void statusMessage(const QString& message);
    void requestCreateRoute();
    void requestEditRoute(int routeId);

private:
    EcWidget* ecWidget;
    
    // UI Components
    QVBoxLayout* mainLayout;
    QLabel* titleLabel;
    QTreeWidget* routeTreeWidget;
    
    // Control buttons
    QHBoxLayout* buttonLayout;
    QPushButton* addRouteButton;
    QPushButton* routeDetailButton;
    QPushButton* refreshButton;
    QPushButton* clearAllButton;
    
    // Route info group
    QGroupBox* routeInfoGroup;
    QLabel* routeNameLabel;
    QLabel* waypointCountLabel;
    QLabel* totalDistanceLabel;
    QLabel* totalTimeLabel;
    QCheckBox* visibilityCheckBox;
    QPushButton* addToShipButton;
    QPushButton* detachFromShipButton;
    
    
    // Helper methods
    void setupUI();
    void setupConnections();
    RouteInfo calculateRouteInfo(int routeId);
    QString formatDistance(double distanceNM);
    QString formatTime(double hours);
    void updateRouteInfoDisplay(const RouteInfo& info);
    void clearRouteInfoDisplay();
    
    // Context menu
    QMenu* contextMenu;
    QAction* renameAction;
    QAction* toggleVisibilityAction;
    QAction* deleteAction;
    QAction* propertiesAction;
    
    int selectedRouteId;
    QList<EcWidget::Waypoint> getWaypointById(int routeId);
    void publishToMOOSDB();
    RouteTreeItem* findRouteItem(int routeId);
    bool isWaypointItem(QTreeWidgetItem* item) const;
};

#endif // ROUTEPANEL_H
