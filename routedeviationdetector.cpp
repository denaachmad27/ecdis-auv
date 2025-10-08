#include "routedeviationdetector.h"
#include "ecwidget.h"
#include "eckernel.h"
#include <QtMath>
#include <QDebug>

RouteDeviationDetector::RouteDeviationDetector(EcWidget* parent)
    : QObject(parent)
    , ecWidget(parent)
    , deviationThreshold(0.1)  // Default 0.1 NM (sekitar 185 meter)
    , autoCheckEnabled(true)
    , checkInterval(2000)      // Check setiap 2 detik
    , pulseEnabled(true)
    , pulseColor(QColor(255, 165, 0, 180))  // Orange semi-transparent
    , labelVisible(true)
    , pulseOpacity(1.0)
    , pulseRadius(0.0)
    , pulseExpanding(true)
{
    // Setup auto-check timer
    autoCheckTimer = new QTimer(this);
    connect(autoCheckTimer, &QTimer::timeout, this, &RouteDeviationDetector::performAutoCheck);

    if (autoCheckEnabled) {
        autoCheckTimer->start(checkInterval);
    }

    // Setup pulse animation timer
    pulseTimer = new QTimer(this);
    connect(pulseTimer, &QTimer::timeout, this, &RouteDeviationDetector::updatePulseAnimation);
    pulseTimer->start(50);  // 20 FPS untuk animasi smooth
}

RouteDeviationDetector::~RouteDeviationDetector()
{
    if (autoCheckTimer) {
        autoCheckTimer->stop();
        delete autoCheckTimer;
    }

    if (pulseTimer) {
        pulseTimer->stop();
        delete pulseTimer;
    }
}

RouteDeviationDetector::DeviationInfo RouteDeviationDetector::checkDeviation(
    double ownshipLat, double ownshipLon, double ownshipHeading, int attachedRouteId)
{
    DeviationInfo info;

    // Validasi input
    if (attachedRouteId < 0 || !ecWidget) {
        return info;
    }

    // Get route dari ecWidget
    QList<EcWidget::Route> routes = ecWidget->getRoutes();
    EcWidget::Route targetRoute;
    bool routeFound = false;

    for (const auto& route : routes) {
        if (route.routeId == attachedRouteId) {
            targetRoute = route;
            routeFound = true;
            break;
        }
    }

    if (!routeFound || targetRoute.waypoints.size() < 2) {
        return info;
    }

    // USE NEAREST SEGMENT (Simple deviation detection - for guidance only)
    // Find the nearest segment to ownship position
    double minDistToSegment = 1e9;
    int nearestSegIdx = findNearestSegment(ownshipLat, ownshipLon, attachedRouteId, minDistToSegment);

    if (nearestSegIdx < 0 || nearestSegIdx >= targetRoute.waypoints.size() - 1) {
        qDebug() << "[ROUTE-DEVIATION] No valid segment found";
        return info;  // No valid segment
    }

    // Get waypoints for nearest segment
    const auto& wp1 = targetRoute.waypoints[nearestSegIdx];
    const auto& wp2 = targetRoute.waypoints[nearestSegIdx + 1];

    qDebug() << "[ROUTE-DEVIATION] Using NEAREST SEGMENT: WP" << nearestSegIdx << "→ WP" << (nearestSegIdx + 1);

    // Hitung cross-track distance
    double crossTrack = calculateCrossTrackDistance(
        wp1.lat, wp1.lon,
        wp2.lat, wp2.lon,
        ownshipLat, ownshipLon
    );

    // Hitung bearing dari segment
    double segmentDistance = 0.0;
    double segmentBearing = 0.0;
    EcCalculateRhumblineDistanceAndBearing(
        EC_GEO_DATUM_WGS84,
        wp1.lat, wp1.lon,
        wp2.lat, wp2.lon,
        &segmentDistance, &segmentBearing
    );

    // Hitung sudut deviasi
    double deviationAngle = calculateDeviationAngle(segmentBearing, ownshipHeading);

    // Find closest point on segment
    QPointF closestPt = findClosestPointOnSegment(
        wp1.lat, wp1.lon,
        wp2.lat, wp2.lon,
        ownshipLat, ownshipLon
    );

    // Calculate actual distance from ownship to closest point
    double distToClosestPoint = 0.0;
    double bearingToClosest = 0.0;
    EcCalculateRhumblineDistanceAndBearing(
        EC_GEO_DATUM_WGS84,
        ownshipLat, ownshipLon,
        closestPt.x(), closestPt.y(),
        &distToClosestPoint, &bearingToClosest
    );

    // Populate deviation info
    info.isDeviated = (distToClosestPoint > deviationThreshold);  // Use actual distance, not crossTrack
    info.crossTrackDistance = distToClosestPoint;  // Store actual perpendicular distance
    info.deviationAngle = deviationAngle;
    info.routeId = attachedRouteId;
    info.nearestSegmentIndex = nearestSegIdx;
    info.distanceToSegment = distToClosestPoint;
    info.closestPoint = closestPt;

    qDebug() << "[ROUTE-DEVIATION] Nearest segment distance:" << distToClosestPoint << "NM, XTE:" << crossTrack
             << "NM, Deviation Angle:" << deviationAngle << "°";

    // Update current state
    bool wasDeviated = currentDeviation.isDeviated;
    currentDeviation = info;

    // Emit signals
    if (info.isDeviated && !wasDeviated) {
        emit deviationDetected(info);
        qDebug() << "[ROUTE-DEVIATION] Deviation detected! XTE:" << crossTrack
                 << "NM, Angle:" << deviationAngle << "°";
    } else if (!info.isDeviated && wasDeviated) {
        emit deviationCleared();
        qDebug() << "[ROUTE-DEVIATION] Back on track";
    }

    return info;
}

void RouteDeviationDetector::setDeviationThreshold(double thresholdNM)
{
    deviationThreshold = qMax(0.01, thresholdNM);  // Minimum 0.01 NM
}

void RouteDeviationDetector::setAutoCheckEnabled(bool enabled)
{
    autoCheckEnabled = enabled;

    if (enabled) {
        if (!autoCheckTimer->isActive()) {
            autoCheckTimer->start(checkInterval);
        }
    } else {
        if (autoCheckTimer->isActive()) {
            autoCheckTimer->stop();
        }
    }
}

void RouteDeviationDetector::setCheckInterval(int milliseconds)
{
    checkInterval = qMax(500, milliseconds);  // Minimum 500ms

    if (autoCheckTimer->isActive()) {
        autoCheckTimer->stop();
        autoCheckTimer->start(checkInterval);
    }
}

void RouteDeviationDetector::clearDeviation()
{
    bool wasDeviated = currentDeviation.isDeviated;

    // Reset deviation info
    currentDeviation = DeviationInfo();

    // Reset pulse animation
    pulseOpacity = 1.0;
    pulseRadius = 0.0;
    pulseExpanding = true;

    // Emit signal if there was a deviation before
    if (wasDeviated) {
        emit deviationCleared();
        emit visualUpdateRequired();
        qDebug() << "[ROUTE-DEVIATION] Deviation cleared (route detached)";
    }
}

void RouteDeviationDetector::performAutoCheck()
{
    if (!ecWidget || !autoCheckEnabled) {
        return;
    }

    // Get ownship position dari ecWidget
    ShipStruct navShip = ecWidget->getNavShip();

    // Check attached route
    QList<EcWidget::Route> routes = ecWidget->getRoutes();
    int attachedRouteId = -1;

    for (const auto& route : routes) {
        if (route.attachedToShip) {
            attachedRouteId = route.routeId;
            break;
        }
    }

    if (attachedRouteId >= 0) {
        checkDeviation(navShip.lat, navShip.lon, navShip.heading, attachedRouteId);

        if (currentDeviation.isDeviated) {
            emit visualUpdateRequired();
        }
    } else {
        // No route attached, clear any existing deviation
        clearDeviation();
    }
}

void RouteDeviationDetector::updatePulseAnimation()
{
    if (!pulseEnabled || !currentDeviation.isDeviated) {
        pulseOpacity = 1.0;
        pulseRadius = 0.0;
        return;
    }

    // Animasi pulse expanding/contracting
    if (pulseExpanding) {
        pulseRadius += 0.15;  // Kecepatan ekspansi
        pulseOpacity -= 0.03;

        if (pulseRadius >= 3.0) {  // Maksimum radius 3 NM
            pulseExpanding = false;
        }
    } else {
        pulseRadius -= 0.15;
        pulseOpacity += 0.03;

        if (pulseRadius <= 0.0) {
            pulseExpanding = true;
            pulseOpacity = 1.0;
        }
    }

    // Clamp values
    pulseOpacity = qBound(0.1, pulseOpacity, 1.0);
    pulseRadius = qBound(0.0, pulseRadius, 3.0);

    emit visualUpdateRequired();
}

double RouteDeviationDetector::calculateCrossTrackDistance(
    double lat1, double lon1,    // Segment start
    double lat2, double lon2,    // Segment end
    double lat3, double lon3)    // Ship position
{
    // Gunakan formula cross-track distance
    // XTD = asin(sin(dist_start_ship) * sin(bearing_diff)) * R

    double distStartToShip = 0.0;
    double bearingStartToShip = 0.0;
    EcCalculateRhumblineDistanceAndBearing(
        EC_GEO_DATUM_WGS84,
        lat1, lon1,
        lat3, lon3,
        &distStartToShip, &bearingStartToShip
    );

    double distSegment = 0.0;
    double bearingSegment = 0.0;
    EcCalculateRhumblineDistanceAndBearing(
        EC_GEO_DATUM_WGS84,
        lat1, lon1,
        lat2, lon2,
        &distSegment, &bearingSegment
    );

    // Convert ke radian
    double dAngRad = qDegreesToRadians(bearingStartToShip - bearingSegment);

    // Approximate cross-track distance (untuk jarak kecil)
    double crossTrack = distStartToShip * std::sin(dAngRad);

    return crossTrack;
}

double RouteDeviationDetector::calculateDeviationAngle(double segmentBearing, double shipHeading)
{
    // Hitung selisih sudut dengan normalisasi [-180, 180]
    double angle = shipHeading - segmentBearing;

    // Normalize ke range [-180, 180]
    while (angle > 180.0) angle -= 360.0;
    while (angle < -180.0) angle += 360.0;

    return angle;
}

QPointF RouteDeviationDetector::findClosestPointOnSegment(
    double lat1, double lon1,    // Segment start
    double lat2, double lon2,    // Segment end
    double lat3, double lon3)    // Ship position
{
    // Simplified: Return midpoint untuk sekarang
    // TODO: Implementasi proyeksi tegak lurus yang lebih akurat

    double distTotal = 0.0;
    double bearingTotal = 0.0;
    EcCalculateRhumblineDistanceAndBearing(
        EC_GEO_DATUM_WGS84,
        lat1, lon1,
        lat2, lon2,
        &distTotal, &bearingTotal
    );

    // Cari titik proyeksi menggunakan bearing perpendicular
    double distStart = 0.0;
    double bearingStart = 0.0;
    EcCalculateRhumblineDistanceAndBearing(
        EC_GEO_DATUM_WGS84,
        lat1, lon1,
        lat3, lon3,
        &distStart, &bearingStart
    );

    // Hitung proyeksi distance along segment
    double dAngRad = qDegreesToRadians(bearingStart - bearingTotal);
    double projDist = distStart * std::cos(dAngRad);

    // Clamp ke segment bounds
    projDist = qBound(0.0, projDist, distTotal);

    // Calculate position at projection distance
    EcCoordinate closestLat, closestLon;
    EcCalculateRhumblinePosition(
        EC_GEO_DATUM_WGS84,
        lat1, lon1,
        projDist,
        bearingTotal,
        &closestLat, &closestLon
    );

    return QPointF(closestLat, closestLon);
}

int RouteDeviationDetector::findNearestSegment(
    double ownshipLat, double ownshipLon, int routeId, double& minDistance)
{
    minDistance = 1e9;
    int nearestIdx = -1;

    if (!ecWidget) {
        return nearestIdx;
    }

    // Get route
    QList<EcWidget::Route> routes = ecWidget->getRoutes();
    EcWidget::Route targetRoute;
    bool found = false;

    for (const auto& route : routes) {
        if (route.routeId == routeId) {
            targetRoute = route;
            found = true;
            break;
        }
    }

    if (!found || targetRoute.waypoints.size() < 2) {
        return nearestIdx;
    }

    // Iterasi semua segment
    for (int i = 0; i < targetRoute.waypoints.size() - 1; i++) {
        const auto& wp1 = targetRoute.waypoints[i];
        const auto& wp2 = targetRoute.waypoints[i + 1];

        // Calculate closest point on this segment
        QPointF closest = findClosestPointOnSegment(
            wp1.lat, wp1.lon,
            wp2.lat, wp2.lon,
            ownshipLat, ownshipLon
        );

        // Distance from ship to closest point
        double dist = 0.0;
        double bearing = 0.0;
        EcCalculateRhumblineDistanceAndBearing(
            EC_GEO_DATUM_WGS84,
            ownshipLat, ownshipLon,
            closest.x(), closest.y(),
            &dist, &bearing
        );

        if (dist < minDistance) {
            minDistance = dist;
            nearestIdx = i;
        }
    }

    return nearestIdx;
}
