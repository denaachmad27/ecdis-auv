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
    void updateWaypoint(const EcWidget::Waypoint& waypoint);
    
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
    
    // Route Management
    void onAddRouteClicked();
    void onImportRoutesClicked();
    void onExportRoutesClicked();
    void onRefreshClicked();
    void onClearAllClicked();
    
    // Waypoint Management
    void onAddWaypointClicked();
    void onEditWaypointClicked();
    void onDeleteWaypointClicked();
    void onMoveWaypointUp();
    void onMoveWaypointDown();
    void onDuplicateWaypointClicked();
    void onToggleWaypointActive();
    
    // Context menu actions
    void onRenameRoute();
    void onToggleRouteVisibility();
    void onDeleteRoute();
    void onRouteProperties();
    void onEditWaypointFromContext();
    void onDuplicateWaypointFromContext();
    void onDeleteWaypointFromContext();
    void onInsertWaypointBefore();
    void onInsertWaypointAfter();
    void onMoveWaypointUpFromContext();
    void onMoveWaypointDownFromContext();

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
    
    // Control buttons - Route Management
    QHBoxLayout* routeButtonLayout;
    QPushButton* addRouteButton;
    QPushButton* importRoutesButton;
    QPushButton* exportRoutesButton;
    QPushButton* refreshButton;
    QPushButton* clearAllButton;
    
    // Waypoint Management buttons
    QHBoxLayout* waypointButtonLayout;
    QPushButton* addWaypointButton;
    QPushButton* editWaypointButton;
    QPushButton* deleteWaypointButton;
    QPushButton* moveUpButton;
    QPushButton* moveDownButton;
    QPushButton* duplicateWaypointButton;
    QPushButton* toggleActiveButton;
    
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
    void updateButtonStates();
    void updateWaypointButtonStates();
    
    // Waypoint operations
    void showWaypointEditDialog(int routeId, int waypointIndex = -1);
    void reorderWaypoint(int routeId, int fromIndex, int toIndex);
    void duplicateWaypoint(int routeId, int waypointIndex);
    void toggleWaypointActiveStatus(int routeId, int waypointIndex);
    QTreeWidgetItem* getSelectedWaypointItem();
    int getWaypointIndex(QTreeWidgetItem* waypointItem);
    int getRouteIdFromItem(QTreeWidgetItem* item);
    
    // Context menu
    QMenu* routeContextMenu;
    QMenu* waypointContextMenu;
    
    // Route context menu actions
    QAction* renameRouteAction;
    QAction* toggleVisibilityAction;
    QAction* deleteRouteAction;
    QAction* routePropertiesAction;
    QAction* duplicateRouteAction;
    QAction* exportRouteAction;
    
    // Waypoint context menu actions
    QAction* editWaypointAction;
    QAction* duplicateWaypointAction;
    QAction* deleteWaypointAction;
    QAction* toggleActiveAction;
    QAction* insertBeforeAction;
    QAction* insertAfterAction;
    QAction* moveUpAction;
    QAction* moveDownAction;
    
    int selectedRouteId;
    QList<EcWidget::Waypoint> getWaypointById(int routeId);
    void publishToMOOSDB();
    RouteTreeItem* findRouteItem(int routeId);
    bool isWaypointItem(QTreeWidgetItem* item) const;
};

#endif // ROUTEPANEL_H
