#ifndef AUTOROUTEDIALOG_H
#define AUTOROUTEDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#endif

#include "eckernel.h"

/**
 * @brief Route optimization strategies
 */
enum class RouteOptimization {
    SHORTEST_DISTANCE,  // Minimize total distance (nautical miles)
    FASTEST_TIME,       // Minimize time considering speed and current
    SAFEST_ROUTE,       // Maximize depth clearance and avoid hazards
    BALANCED            // Balance between distance, time, and safety
};

/**
 * @brief Options for auto route generation
 */
struct AutoRouteOptions {
    RouteOptimization optimization = RouteOptimization::FASTEST_TIME;
    double plannedSpeed = 8.0;           // knots
    bool useWaypointNetwork = false;     // Use existing waypoint database (future)
    bool avoidShallowWater = true;
    double minDepth = 10.0;              // meters - minimum safe depth
    bool avoidHazards = true;            // Avoid dangerous objects
    bool considerUKC = true;             // Consider Under Keel Clearance
    double minUKC = 2.0;                 // meters - minimum UKC
    bool stayInSafetyCorridors = false;  // Prefer established shipping lanes
    int waypointDensity = 5;             // waypoint every X nautical miles

    AutoRouteOptions() {}
};

/**
 * @brief Dialog for configuring auto route generation
 *
 * This dialog allows users to specify parameters for automatic route generation
 * including optimization strategy, safety margins, and hazard avoidance options.
 */
class AutoRouteDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param targetLat Target latitude
     * @param targetLon Target longitude
     * @param startLat Starting latitude (ownship position)
     * @param startLon Starting longitude (ownship position)
     * @param parent Parent widget
     */
    AutoRouteDialog(EcCoordinate targetLat, EcCoordinate targetLon,
                   EcCoordinate startLat, EcCoordinate startLon,
                   QWidget* parent = nullptr);

    ~AutoRouteDialog() = default;

    /**
     * @brief Get configured route options
     * @return AutoRouteOptions structure with user selections
     */
    AutoRouteOptions getOptions() const;

    /**
     * @brief Get straight-line distance between start and target
     * @return Distance in nautical miles
     */
    double getStraightLineDistance() const { return straightLineDistance; }

signals:
    void routeOptionsConfirmed(const AutoRouteOptions& options);

private slots:
    void onOptimizationChanged(int index);
    void onAvoidShallowChanged(int state);
    void onGenerateClicked();
    void onCancelClicked();

private:
    void setupUI();
    void updateEstimates();
    QString formatCoordinate(double lat, double lon) const;
    QString formatDistance(double distanceNM) const;
    QString formatTime(double hours) const;
    QString getDialogStyleSheet() const;

    // Position data
    EcCoordinate targetLat, targetLon;
    EcCoordinate startLat, startLon;
    double straightLineDistance;

    // UI Components - Route Info
    QLabel* startPosLabel;
    QLabel* targetPosLabel;
    QLabel* distanceLabel;

    // UI Components - Optimization
    QRadioButton* shortestDistanceRadio;
    QRadioButton* fastestTimeRadio;
    QRadioButton* safestRouteRadio;
    QRadioButton* balancedRadio;

    // UI Components - Parameters
    QDoubleSpinBox* speedSpinBox;
    QSpinBox* waypointDensitySpinBox;

    // UI Components - Safety Options
    QCheckBox* avoidShallowCheckBox;
    QDoubleSpinBox* minDepthSpinBox;
    QCheckBox* avoidHazardsCheckBox;
    QCheckBox* considerUKCCheckBox;
    QDoubleSpinBox* minUKCSpinBox;
    QCheckBox* useSafetyCorridorsCheckBox;

    // UI Components - Estimates
    QLabel* estimatedDistanceLabel;
    QLabel* estimatedTimeLabel;
    QLabel* estimatedWaypointsLabel;

    // Buttons
    QPushButton* generateButton;
    QPushButton* cancelButton;
};

#endif // AUTOROUTEDIALOG_H
