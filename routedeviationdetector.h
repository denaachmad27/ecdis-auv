#ifndef ROUTEDEVIATIONDETECTOR_H
#define ROUTEDEVIATIONDETECTOR_H

#include <QObject>
#include <QPointF>
#include <QList>
#include <QTimer>
#include <QColor>

class EcWidget;

/**
 * @brief RouteDeviationDetector - Deteksi ketika ownship keluar dari jalur route
 *
 * Fitur ini mirip dengan red pulse untuk area detection, namun khusus untuk route.
 * Menampilkan visual indicator dan derajat deviasi ketika ownship menyimpang dari jalur.
 */
class RouteDeviationDetector : public QObject
{
    Q_OBJECT

public:
    struct DeviationInfo {
        bool isDeviated;              // Status apakah ownship sedang menyimpang
        double crossTrackDistance;    // Jarak tegak lurus dari jalur (NM)
        double deviationAngle;        // Sudut penyimpangan dari jalur (degrees)
        int routeId;                  // ID route yang sedang di-attach
        int nearestSegmentIndex;      // Index segment terdekat
        double distanceToSegment;     // Jarak ke segment terdekat (NM)
        QPointF closestPoint;         // Titik terdekat di jalur (lat, lon)

        DeviationInfo() : isDeviated(false), crossTrackDistance(0.0),
                         deviationAngle(0.0), routeId(-1),
                         nearestSegmentIndex(-1), distanceToSegment(0.0) {}
    };

    explicit RouteDeviationDetector(EcWidget* parent = nullptr);
    ~RouteDeviationDetector();

    // Main detection function
    DeviationInfo checkDeviation(double ownshipLat, double ownshipLon,
                                 double ownshipHeading, int attachedRouteId);

    // Configuration
    void setDeviationThreshold(double thresholdNM);
    double getDeviationThreshold() const { return deviationThreshold; }

    void setAutoCheckEnabled(bool enabled);
    bool isAutoCheckEnabled() const { return autoCheckEnabled; }

    void setCheckInterval(int milliseconds);
    int getCheckInterval() const { return checkInterval; }

    // Visual settings
    void setPulseEnabled(bool enabled) { pulseEnabled = enabled; }
    bool isPulseEnabled() const { return pulseEnabled; }

    void setPulseColor(const QColor& color) { pulseColor = color; }
    QColor getPulseColor() const { return pulseColor; }

    void setLabelVisible(bool visible) { labelVisible = visible; }
    bool isLabelVisible() const { return labelVisible; }

    // Get current deviation info
    DeviationInfo getCurrentDeviation() const { return currentDeviation; }
    bool hasDeviation() const { return currentDeviation.isDeviated; }

    // Pulse animation state
    double getPulseOpacity() const { return pulseOpacity; }
    double getPulseRadius() const { return pulseRadius; }

signals:
    void deviationDetected(const DeviationInfo& info);
    void deviationCleared();
    void visualUpdateRequired();

private slots:
    void performAutoCheck();
    void updatePulseAnimation();

private:
    EcWidget* ecWidget;

    // Deviation detection settings
    double deviationThreshold;    // Threshold dalam nautical miles
    bool autoCheckEnabled;
    int checkInterval;           // Interval pengecekan dalam ms
    QTimer* autoCheckTimer;

    // Visual settings
    bool pulseEnabled;
    QColor pulseColor;
    bool labelVisible;

    // Pulse animation
    QTimer* pulseTimer;
    double pulseOpacity;         // 0.0 - 1.0
    double pulseRadius;          // Radius untuk animasi pulse
    bool pulseExpanding;

    // Current state
    DeviationInfo currentDeviation;

    // Helper functions
    double calculateCrossTrackDistance(double lat1, double lon1,    // Start point
                                      double lat2, double lon2,    // End point
                                      double lat3, double lon3);   // Ship position

    double calculateDeviationAngle(double segmentBearing, double shipHeading);

    QPointF findClosestPointOnSegment(double lat1, double lon1,    // Segment start
                                     double lat2, double lon2,    // Segment end
                                     double lat3, double lon3);   // Ship position

    int findNearestSegment(double ownshipLat, double ownshipLon,
                          int routeId, double& minDistance);
};

#endif // ROUTEDEVIATIONDETECTOR_H
