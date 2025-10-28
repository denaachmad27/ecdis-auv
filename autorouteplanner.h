#ifndef AUTOROUTEPLANNER_H
#define AUTOROUTEPLANNER_H

#include <QVector>
#include <QStringList>
#include <QPair>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#endif

#include "eckernel.h"
#include "autoroutedialog.h"

/**
 * @brief Helper structures for representing auto-generated routes.
 */
struct GeoPoint {
    double lat = 0.0;
    double lon = 0.0;
};

struct AutoRouteLegInfo {
    GeoPoint start;
    GeoPoint end;
    double distanceNm = 0.0;
    double initialBearingDeg = 0.0;
    double estimatedTimeHours = 0.0;
};

struct AutoRouteResult {
    bool success = false;
    QVector<GeoPoint> waypoints;
    QVector<AutoRouteLegInfo> legs;
    double totalDistanceNm = 0.0;
    double estimatedTimeHours = 0.0;
    QStringList warnings;
};

/**
 * @brief Planner responsible for generating automatic routes between two points.
 *
 * The planner uses the SevenCs kernel geodesy routines (great-circle & rhumbline)
 * to derive waypoint positions and produces high-level safety hints that can be
 * surfaced to the operator prior to activation.
 */
class AutoRoutePlanner
{
public:
    AutoRoutePlanner(EcView* view, EcDictInfo* dictInfo);

    AutoRouteResult planRoute(const GeoPoint& start,
                              const GeoPoint& target,
                              const AutoRouteOptions& options) const;

private:
    EcView* m_view;
    EcDictInfo* m_dictInfo;

    double computeGreatCircleDistance(const GeoPoint& start,
                                      const GeoPoint& target,
                                      double* startCourseDeg,
                                      double* endCourseDeg) const;

    GeoPoint computeGreatCirclePosition(const GeoPoint& start,
                                        double startCourseDeg,
                                        double distanceNm,
                                        double* endCourseDeg) const;

    GeoPoint applyLateralOffset(const GeoPoint& basePoint,
                                double bearingDeg,
                                double offsetNm) const;

    double computeRhumbDistance(const GeoPoint& start,
                                const GeoPoint& end,
                                double* bearingDeg) const;

    double haversineDistanceNm(const GeoPoint& start,
                               const GeoPoint& end) const;

    QStringList buildWarnings(const AutoRouteResult& result,
                              const AutoRouteOptions& options) const;

    bool isPositionSafe(const GeoPoint& point,
                        const AutoRouteOptions& options,
                        double* foundDepth = nullptr) const;

    bool checkLineSegmentSafety(const GeoPoint& start,
                                const GeoPoint& end,
                                const AutoRouteOptions& options,
                                int numSamples = 5) const;

    GeoPoint findSafeAlternative(const GeoPoint& unsafePoint,
                                  const GeoPoint& previousPoint,
                                  const GeoPoint& targetPoint,
                                  const AutoRouteOptions& options,
                                  double searchRadiusNm = 1.0) const;

    // A* pathfinding for safe route
    QVector<GeoPoint> findSafePathAStar(const GeoPoint& start,
                                        const GeoPoint& target,
                                        const AutoRouteOptions& options) const;

    struct GridNode {
        double lat;
        double lon;
        double gCost;  // Cost from start
        double hCost;  // Heuristic cost to target
        double fCost() const { return gCost + hCost; }
        GridNode* parent;
        bool isSafe;
        bool inClosedSet;
        bool inOpenSet;

        GridNode() : lat(0), lon(0), gCost(999999), hCost(0), parent(nullptr),
                     isSafe(true), inClosedSet(false), inOpenSet(false) {}
    };

    QVector<GridNode*> getNeighbors(GridNode* node,
                                     QVector<QVector<GridNode*>>& grid,
                                     int gridWidth, int gridHeight) const;
};

#endif // AUTOROUTEPLANNER_H
