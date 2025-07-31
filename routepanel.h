#ifndef ROUTEPANEL_H
#define ROUTEPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
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

// Forward declarations
class EcWidget;

struct RouteInfo {
    int routeId;
    QString name;
    int waypointCount;
    double totalDistance;
    double totalTime; // in hours
    bool visible;
    
    RouteInfo() : routeId(0), name(""), waypointCount(0), totalDistance(0.0), totalTime(0.0), visible(true) {}
};

class RouteListItem : public QListWidgetItem
{
public:
    RouteListItem(const RouteInfo& routeInfo, QListWidget* parent = nullptr);
    
    void updateFromRouteInfo(const RouteInfo& routeInfo);
    int getRouteId() const { return routeId; }

private:
    int routeId;
    void updateDisplayText(const RouteInfo& routeInfo, EcWidget* ecWidget = nullptr);
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
    void onRouteItemDoubleClicked(QListWidgetItem* item);
    void onShowContextMenu(const QPoint& pos);
    void onAddRouteClicked();
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

private:
    EcWidget* ecWidget;
    
    // UI Components
    QVBoxLayout* mainLayout;
    QLabel* titleLabel;
    QListWidget* routeListWidget;
    
    // Control buttons
    QHBoxLayout* buttonLayout;
    QPushButton* addRouteButton;
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
};

#endif // ROUTEPANEL_H