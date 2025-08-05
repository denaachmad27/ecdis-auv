#ifndef ROUTEFORMDIALOG_H
#define ROUTEFORMDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QDoubleValidator>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QSplitter>
#include <QTabWidget>

// Forward declaration
class EcWidget;

struct RouteWaypointData {
    double lat;
    double lon;
    QString label;
    QString remark;
    double turningRadius;
    bool active;
    
    RouteWaypointData() : lat(0.0), lon(0.0), turningRadius(0.5), active(true) {}
};

class RouteFormDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RouteFormDialog(QWidget *parent = nullptr, EcWidget *ecWidget = nullptr);
    
    // Route getters
    QString getRouteName() const;
    QString getRouteDescription() const;
    int getRouteId() const;
    QList<RouteWaypointData> getWaypoints() const;
    
    // Route setters
    void setRouteName(const QString& name);
    void setRouteDescription(const QString& description);
    void setRouteId(int routeId);
    void loadRouteData(int routeId);

private slots:
    void onAddWaypoint();
    void onRemoveWaypoint();
    void onEditWaypoint();
    void onWaypointSelectionChanged();
    void onMoveWaypointUp();
    void onMoveWaypointDown();
    void onImportFromCSV();
    void onExportToCSV();
    void onValidateRoute();
    void onPreviewRoute();
    void validateAndAccept();

private:
    void setupUI();
    void setupRouteInfoTab();
    void setupWaypointsTab();
    void setupAdvancedTab();
    void connectSignals();
    void updateWaypointList();
    void updateButtonStates();
    bool validateRouteData();
    void showWaypointInputDialog(int editIndex = -1);
    double calculateRouteDistance();
    QString formatDistance(double distanceNM);
    int generateNextRouteId();
    
    // UI components - Route Info
    QTabWidget *tabWidget;
    QLineEdit *routeNameEdit;
    QTextEdit *routeDescriptionEdit;
    QSpinBox *routeIdSpinBox;
    QLabel *routeStatsLabel;
    
    // UI components - Waypoints
    QTableWidget *waypointTable;
    QPushButton *addWaypointBtn;
    QPushButton *removeWaypointBtn;
    QPushButton *editWaypointBtn;
    QPushButton *moveUpBtn;
    QPushButton *moveDownBtn;
    QPushButton *importBtn;
    QPushButton *exportBtn;
    
    // UI components - Advanced
    QCheckBox *autoLabelCheckBox;
    QDoubleSpinBox *defaultTurningRadiusSpinBox;
    QComboBox *routeTypeComboBox;
    QLineEdit *routeSpeedEdit;
    QCheckBox *reverseRouteCheckBox;
    
    // UI components - Dialog buttons
    QPushButton *okButton;
    QPushButton *cancelButton;
    QPushButton *previewButton;
    
    QLabel *statusLabel;
    
    // Data
    QList<RouteWaypointData> waypoints;
    EcWidget *ecWidget;
    
    // Validators
    QDoubleValidator *latValidator;
    QDoubleValidator *lonValidator;
    QDoubleValidator *radiusValidator;
};

#endif // ROUTEFORMDIALOG_H