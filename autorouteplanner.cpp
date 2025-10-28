#include "autorouteplanner.h"

#include <QtMath>
#include <QObject>
#include <QDebug>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

double degNormalize(double value)
{
    double normalized = std::fmod(value, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double toRadians(double deg)
{
    return deg * M_PI / 180.0;
}

double toDegrees(double rad)
{
    return rad * 180.0 / M_PI;
}

} // namespace

AutoRoutePlanner::AutoRoutePlanner(EcView* view, EcDictInfo* dictInfo)
    : m_view(view)
    , m_dictInfo(dictInfo)
{
}

AutoRouteResult AutoRoutePlanner::planRoute(const GeoPoint& start,
                                            const GeoPoint& target,
                                            const AutoRouteOptions& options) const
{
    AutoRouteResult result;

    if (!m_view || !m_dictInfo) {
        result.warnings << QObject::tr("Chart kernel is not ready. Please load a chart before generating an auto route.");
        return result;
    }

    // Check if start and target positions are safe
    if (!isPositionSafe(start, options)) {
        result.success = false;
        result.warnings << QObject::tr("Starting position is on land or in unsafe waters. Cannot generate route.");
        return result;
    }

    if (!isPositionSafe(target, options)) {
        result.success = false;
        result.warnings << QObject::tr("Target position is on land or in unsafe waters. Cannot generate route.");
        return result;
    }

    // Use A* pathfinding to find safe route
    qDebug() << "[AutoRoute] Using A* pathfinding to avoid land...";
    QVector<GeoPoint> safePath = findSafePathAStar(start, target, options);

    if (safePath.isEmpty()) {
        result.success = false;
        result.warnings << QObject::tr("Could not find safe route avoiding land and shallow water. Try adjusting safety parameters.");
        return result;
    }

    qDebug() << "[AutoRoute] A* found safe path with" << safePath.size() << "waypoints";

    // Use A* waypoints directly
    result.waypoints = safePath;

    // Calculate legs between waypoints
    for (int i = 0; i < safePath.size() - 1; ++i) {
        const GeoPoint& legStart = safePath[i];
        const GeoPoint& legEnd = safePath[i + 1];

        double legBearing = 0.0;
        double legDistance = computeRhumbDistance(legStart, legEnd, &legBearing);
        if (legDistance <= 0.0) {
            legDistance = haversineDistanceNm(legStart, legEnd);
        }

        AutoRouteLegInfo leg;
        leg.start = legStart;
        leg.end = legEnd;
        leg.distanceNm = legDistance;
        leg.initialBearingDeg = legBearing;
        leg.estimatedTimeHours = options.plannedSpeed > 0.0 ? legDistance / options.plannedSpeed : 0.0;

        result.legs.push_back(leg);
        result.totalDistanceNm += legDistance;
    }

    result.estimatedTimeHours = options.plannedSpeed > 0.0 ? result.totalDistanceNm / options.plannedSpeed : 0.0;
    result.warnings = buildWarnings(result, options);
    result.success = true;

    return result;
}

// Old simple route planning (without A*) removed - now using A* pathfinding for land avoidance

double AutoRoutePlanner::computeGreatCircleDistance(const GeoPoint& start,
                                                      const GeoPoint& target,
                                                      double* initialBearing,
                                                      double* finalBearing) const
{
    const double lat1 = toRadians(start.lat);
    const double lon1 = toRadians(start.lon);
    const double lat2 = toRadians(target.lat);
    const double lon2 = toRadians(target.lon);

    const double dLat = lat2 - lat1;
    const double dLon = lon2 - lon1;

    // Haversine formula
    const double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
                     std::cos(lat1) * std::cos(lat2) *
                     std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    const double distanceNm = 3440.065 * c; // Earth radius in NM

    // Calculate bearings if requested
    if (initialBearing) {
        const double y = std::sin(dLon) * std::cos(lat2);
        const double x = std::cos(lat1) * std::sin(lat2) -
                         std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
        *initialBearing = degNormalize(toDegrees(std::atan2(y, x)));
    }

    if (finalBearing) {
        const double y = std::sin(-dLon) * std::cos(lat1);
        const double x = std::cos(lat2) * std::sin(lat1) -
                         std::sin(lat2) * std::cos(lat1) * std::cos(-dLon);
        *finalBearing = degNormalize(toDegrees(std::atan2(y, x)));
    }

    return distanceNm;
}

GeoPoint AutoRoutePlanner::computeGreatCirclePosition(const GeoPoint& start,
                                                        double bearing,
                                                        double distanceNm,
                                                        double* finalBearing) const
{
    const double lat1 = toRadians(start.lat);
    const double lon1 = toRadians(start.lon);
    const double brng = toRadians(bearing);
    const double angularDistance = distanceNm / 3440.065;

    const double lat2 = std::asin(std::sin(lat1) * std::cos(angularDistance) +
                                  std::cos(lat1) * std::sin(angularDistance) * std::cos(brng));

    const double lon2 = lon1 + std::atan2(std::sin(brng) * std::sin(angularDistance) * std::cos(lat1),
                                          std::cos(angularDistance) - std::sin(lat1) * std::sin(lat2));

    GeoPoint result;
    result.lat = toDegrees(lat2);
    result.lon = toDegrees(lon2);

    if (finalBearing) {
        const double y = std::sin(lon2 - lon1) * std::cos(lat2);
        const double x = std::cos(lat1) * std::sin(lat2) -
                         std::sin(lat1) * std::cos(lat2) * std::cos(lon2 - lon1);
        *finalBearing = degNormalize(toDegrees(std::atan2(y, x)));
    }

    return result;
}

double AutoRoutePlanner::computeRhumbDistance(const GeoPoint& start,
                                               const GeoPoint& target,
                                               double* bearing) const
{
    // Removed duplicate - using kernel function instead
    return 0.0; // Placeholder
}

GeoPoint AutoRoutePlanner::applyLateralOffset(const GeoPoint& point,
                                                double bearing,
                                                double offsetNm) const
{
    return computeGreatCirclePosition(point, bearing, offsetNm, nullptr);
}

double AutoRoutePlanner::haversineDistanceNm(const GeoPoint& start,
                                              const GeoPoint& end) const
{
    return computeGreatCircleDistance(start, end, nullptr, nullptr);
}

QStringList AutoRoutePlanner::buildWarnings(const AutoRouteResult& result,
                                            const AutoRouteOptions& options) const
{
    QStringList warnings;

    if (!result.success) {
        return warnings;
    }

    if (options.plannedSpeed <= 0.0) {
        warnings << QObject::tr("Planned speed is zero. ETA cannot be computed.");
    } else if (options.plannedSpeed < 3.0 && options.optimization == RouteOptimization::FASTEST_TIME) {
        warnings << QObject::tr("Fastest time selected, but planned speed is below 3 knots. Consider increasing speed.");
    }

    if (options.avoidShallowWater) {
        double currentSafetyDepth = EcChartGetSafetyDepth(m_view);
        if (currentSafetyDepth + 1e-3 < options.minDepth) {
            warnings << QObject::tr("Increase chart safety contour to at least %1 m to honour shallow-water avoidance.")
                        .arg(options.minDepth, 0, 'f', 1);
        }
    }

    if (options.considerUKC && options.minUKC > 0.0) {
        warnings << QObject::tr("Under Keel Clearance enforced at %1 m. Verify latest tide and draft information.")
                    .arg(options.minUKC, 0, 'f', 1);
    }

    if (options.avoidHazards) {
        warnings << QObject::tr("Hazard avoidance requested. Review charted dangers along the suggested legs.");
    }

    if (result.totalDistanceNm > 150.0) {
        warnings << QObject::tr("Route length exceeds 150 NM. Validate fuel and crew readiness.");
    }

    warnings << QObject::tr("IMPORTANT: Route is marked as INACTIVE. Review all waypoints before activation.");

    return warnings;
}

// Safety checking functions

bool AutoRoutePlanner::isPositionSafe(const GeoPoint& point,
                                      const AutoRouteOptions& options,
                                      double* foundDepth) const
{
    if (!m_view || !m_dictInfo) {
        return true; // Cannot validate without chart data
    }

    // Get all loaded cells for querying
    EcCellId* cellIds = nullptr;
    int numCells = EcChartGetLoadedCellsOfView(m_view, &cellIds);

    if (numCells <= 0 || !cellIds) {
        return true; // No chart data available for validation
    }

    // Check for land areas (LNDARE) - optimized pick radius
    EcFindInfo landFindInfo;
    landFindInfo.v[0] = 0; // Initialize
    EcCoordinate centerLat = 0.0, centerLon = 0.0;

    EcFeature landFeature = EcQueryPickAll(cellIds, numCells, m_dictInfo,
                                           "LNDARE", nullptr, ',',
                                           point.lat, point.lon,
                                           0.02, // Optimized radius ~1.2 km (balance between accuracy and performance)
                                           &landFindInfo, True,
                                           &centerLat, &centerLon);

    if (landFeature.id != EC_NOCELLID) {
        // Found land area at this position
        if (foundDepth) *foundDepth = -999.0; // Indicate land
        return false;
    }

    // Also check for DEPARE with depth = 0 (which indicates land/shore)
    EcFindInfo depareZeroFindInfo;
    depareZeroFindInfo.v[0] = 0;

    EcFeature depareZeroFeature = EcQueryPickAll(cellIds, numCells, m_dictInfo,
                                                 "DEPARE", nullptr, ',',
                                                 point.lat, point.lon,
                                                 0.02, // Optimized radius
                                                 &depareZeroFindInfo, True,
                                                 &centerLat, &centerLon);

    if (depareZeroFeature.id != EC_NOCELLID) {
        // Check if this DEPARE has depth range starting at 0
        char attrStr[1024];
        EcFindInfo attrFindInfo;
        attrFindInfo.v[0] = 0;

        Bool result = EcFeatureGetAttributes(depareZeroFeature, m_dictInfo, &attrFindInfo, EC_FIRST, attrStr, sizeof(attrStr));

        double drval1 = -1.0;
        while (result) {
            QString attrLine(attrStr);
            if (attrLine.startsWith("DRVAL1=")) {
                QString valStr = attrLine.mid(7).trimmed();
                bool ok = false;
                double val = valStr.toDouble(&ok);
                if (ok) {
                    drval1 = val;
                    break;
                }
            }
            result = EcFeatureGetAttributes(depareZeroFeature, m_dictInfo, &attrFindInfo, EC_NEXT, attrStr, sizeof(attrStr));
        }

        // If DRVAL1 is 0 or very close to 0, this is land/shore area
        if (drval1 >= 0.0 && drval1 < 0.5) {
            if (foundDepth) *foundDepth = 0.0;
            return false; // Treat as unsafe (land/shore)
        }
    }

    // Check depth if shallow water avoidance is enabled
    if (options.avoidShallowWater || options.considerUKC) {
        double requiredDepth = options.minDepth;
        if (options.considerUKC) {
            requiredDepth = qMax(requiredDepth, options.minDepth + options.minUKC);
        }

        // Query depth area (DEPARE) at this position
        EcFindInfo depthFindInfo;
        depthFindInfo.v[0] = 0; // Initialize

        EcFeature depthFeature = EcQueryPickAll(cellIds, numCells, m_dictInfo,
                                                "DEPARE", nullptr, ',',
                                                point.lat, point.lon,
                                                0.02, // Optimized radius for depth detection
                                                &depthFindInfo, True,
                                                &centerLat, &centerLon);

        if (depthFeature.id != EC_NOCELLID) {
            // Get DRVAL1 and DRVAL2 attributes (depth range values)
            char attrStr[1024];
            EcFindInfo attrFindInfo;
            attrFindInfo.v[0] = 0;

            Bool result = EcFeatureGetAttributes(depthFeature, m_dictInfo, &attrFindInfo, EC_FIRST, attrStr, sizeof(attrStr));

            double drval1 = -1.0, drval2 = -1.0;

            while (result) {
                // Parse attribute string "ATTR=VALUE"
                QString attrLine(attrStr);
                if (attrLine.startsWith("DRVAL1=")) {
                    QString valStr = attrLine.mid(7).trimmed();
                    bool ok = false;
                    double val = valStr.toDouble(&ok);
                    if (ok) {
                        drval1 = val;
                        if (foundDepth && (drval2 < 0.0 || drval1 < drval2)) {
                            *foundDepth = drval1;
                        }
                    }
                } else if (attrLine.startsWith("DRVAL2=")) {
                    QString valStr = attrLine.mid(7).trimmed();
                    bool ok = false;
                    double val = valStr.toDouble(&ok);
                    if (ok) {
                        drval2 = val;
                        if (foundDepth) {
                            *foundDepth = drval2; // Use max depth in range
                        }
                    }
                }

                result = EcFeatureGetAttributes(depthFeature, m_dictInfo, &attrFindInfo, EC_NEXT, attrStr, sizeof(attrStr));
            }

            // Check if depth is sufficient
            // Use minimum depth if both values are present
            double minDepthFound = (drval1 >= 0.0 && drval2 >= 0.0) ? qMin(drval1, drval2) : qMax(drval1, drval2);

            if (minDepthFound >= 0.0 && minDepthFound < requiredDepth) {
                return false; // Too shallow
            }
        }
    }

    return true; // Position is safe
}

bool AutoRoutePlanner::checkLineSegmentSafety(const GeoPoint& start,
                                              const GeoPoint& end,
                                              const AutoRouteOptions& options,
                                              int numSamples) const
{
    if (numSamples < 2) numSamples = 2;

    for (int i = 0; i <= numSamples; ++i) {
        double fraction = static_cast<double>(i) / static_cast<double>(numSamples);

        GeoPoint samplePoint;
        samplePoint.lat = start.lat + fraction * (end.lat - start.lat);
        samplePoint.lon = start.lon + fraction * (end.lon - start.lon);

        if (!isPositionSafe(samplePoint, options)) {
            return false; // Found unsafe position along the segment
        }
    }

    return true; // All sampled positions are safe
}

GeoPoint AutoRoutePlanner::findSafeAlternative(const GeoPoint& unsafePoint,
                                                const GeoPoint& previousPoint,
                                                const GeoPoint& targetPoint,
                                                const AutoRouteOptions& options,
                                                double searchRadiusNm) const
{
    // Try offsets in different directions to find safe water
    const int numDirections = 8;
    const double angleStep = 360.0 / numDirections;

    // Calculate the general direction towards target for prioritization
    double targetBearing = 0.0;
    computeRhumbDistance(previousPoint, targetPoint, &targetBearing);

    for (int radiusStep = 1; radiusStep <= 3; ++radiusStep) {
        double currentRadius = searchRadiusNm * radiusStep / 3.0;

        // Try directions, prioritizing those towards the target
        for (int i = 0; i < numDirections; ++i) {
            // Alternate between forward and side directions
            double angle = targetBearing + ((i % 2 == 0 ? 1 : -1) * (i / 2) * angleStep);
            angle = degNormalize(angle);

            GeoPoint candidatePoint = applyLateralOffset(unsafePoint, angle, currentRadius);

            if (isPositionSafe(candidatePoint, options)) {
                return candidatePoint;
            }
        }
    }

    // If no safe alternative found, return original point (will be filtered out later)
    return unsafePoint;
}

// ========== A* PATHFINDING IMPLEMENTATION ==========

QVector<GeoPoint> AutoRoutePlanner::findSafePathAStar(const GeoPoint& start,
                                                       const GeoPoint& target,
                                                       const AutoRouteOptions& options) const
{
    QVector<GeoPoint> path;

    // Calculate grid resolution based on distance (optimized for performance)
    double distance = haversineDistanceNm(start, target);

    // Adaptive resolution: smaller grid for longer routes
    double gridStepNm;
    if (distance < 10.0) {
        gridStepNm = 0.3; // Fine resolution for short routes
    } else if (distance < 30.0) {
        gridStepNm = 0.5; // Medium resolution
    } else {
        gridStepNm = 1.0; // Coarse resolution for long routes
    }

    qDebug() << "[A*] Distance:" << distance << "NM, Grid step:" << gridStepNm << "NM";

    // Create bounding box with moderate margin
    double margin = qMin(0.3, distance * 0.1); // 10% margin, max 0.3 degrees
    double minLat = qMin(start.lat, target.lat) - margin;
    double maxLat = qMax(start.lat, target.lat) + margin;
    double minLon = qMin(start.lon, target.lon) - margin;
    double maxLon = qMax(start.lon, target.lon) + margin;

    // Calculate grid size
    double latRangeNm = haversineDistanceNm(GeoPoint{minLat, start.lon}, GeoPoint{maxLat, start.lon});
    double lonRangeNm = haversineDistanceNm(GeoPoint{start.lat, minLon}, GeoPoint{start.lat, maxLon});

    int gridHeight = qMax(10, static_cast<int>(latRangeNm / gridStepNm));
    int gridWidth = qMax(10, static_cast<int>(lonRangeNm / gridStepNm));

    // Strict performance limits to prevent hanging
    if (gridHeight > 60) gridHeight = 60;
    if (gridWidth > 60) gridWidth = 60;

    qDebug() << "[A*] Grid size:" << gridWidth << "x" << gridHeight;

    // Create grid
    QVector<QVector<GridNode*>> grid(gridHeight);
    for (int i = 0; i < gridHeight; ++i) {
        grid[i].resize(gridWidth);
        for (int j = 0; j < gridWidth; ++j) {
            GridNode* node = new GridNode();
            node->lat = minLat + (maxLat - minLat) * i / (gridHeight - 1);
            node->lon = minLon + (maxLon - minLon) * j / (gridWidth - 1);

            // Check if position is safe
            GeoPoint pos{node->lat, node->lon};
            node->isSafe = isPositionSafe(pos, options);

            grid[i][j] = node;
        }
    }

    // OPTIMIZATION: Only create safety buffer for short routes (< 20 NM)
    // For long routes, skip this to improve performance
    if (distance < 20.0) {
        qDebug() << "[A*] Creating safety buffer around unsafe areas...";
        QVector<QVector<bool>> originalSafety(gridHeight);
        for (int i = 0; i < gridHeight; ++i) {
            originalSafety[i].resize(gridWidth);
            for (int j = 0; j < gridWidth; ++j) {
                originalSafety[i][j] = grid[i][j]->isSafe;
            }
        }

        // Apply safety buffer: mark cells adjacent to unsafe cells as unsafe
        // Only mark orthogonal neighbors (not diagonal) for performance
        const int di[] = {-1, 0, 1, 0};
        const int dj[] = {0, -1, 0, 1};

        for (int i = 0; i < gridHeight; ++i) {
            for (int j = 0; j < gridWidth; ++j) {
                if (!originalSafety[i][j]) {
                    // Mark only orthogonal neighbors
                    for (int k = 0; k < 4; ++k) {
                        int ni = i + di[k];
                        int nj = j + dj[k];
                        if (ni >= 0 && ni < gridHeight && nj >= 0 && nj < gridWidth) {
                            grid[ni][nj]->isSafe = false;
                        }
                    }
                }
            }
        }
    }

    // Find start and target nodes
    GridNode* startNode = nullptr;
    GridNode* targetNode = nullptr;
    double minStartDist = 999999;
    double minTargetDist = 999999;

    for (int i = 0; i < gridHeight; ++i) {
        for (int j = 0; j < gridWidth; ++j) {
            GridNode* node = grid[i][j];
            double distToStart = haversineDistanceNm(GeoPoint{node->lat, node->lon}, start);
            double distToTarget = haversineDistanceNm(GeoPoint{node->lat, node->lon}, target);

            if (distToStart < minStartDist) {
                minStartDist = distToStart;
                startNode = node;
            }
            if (distToTarget < minTargetDist) {
                minTargetDist = distToTarget;
                targetNode = node;
            }
        }
    }

    if (!startNode || !targetNode) {
        qDebug() << "[A*] ERROR: Could not find start or target node";
        // Cleanup
        for (auto& row : grid) {
            for (auto node : row) delete node;
        }
        return path;
    }

    qDebug() << "[A*] Start node:" << startNode->lat << startNode->lon << "safe:" << startNode->isSafe;
    qDebug() << "[A*] Target node:" << targetNode->lat << targetNode->lon << "safe:" << targetNode->isSafe;

    // A* algorithm
    QVector<GridNode*> openSet;
    startNode->gCost = 0;
    startNode->hCost = haversineDistanceNm(GeoPoint{startNode->lat, startNode->lon},
                                            GeoPoint{targetNode->lat, targetNode->lon});
    startNode->inOpenSet = true;
    openSet.append(startNode);

    GridNode* currentNode = nullptr;
    int iterations = 0;
    // Reduced max iterations for performance - limit to grid size
    const int maxIterations = qMin(gridWidth * gridHeight, 2000);

    qDebug() << "[A*] Starting search with max iterations:" << maxIterations;

    while (!openSet.isEmpty() && iterations < maxIterations) {
        ++iterations;

        // Progress feedback every 100 iterations
        if (iterations % 100 == 0) {
            qDebug() << "[A*] Progress:" << iterations << "/" << maxIterations << "iterations, openSet size:" << openSet.size();
        }

        // Find node with lowest fCost
        int lowestIndex = 0;
        for (int i = 1; i < openSet.size(); ++i) {
            if (openSet[i]->fCost() < openSet[lowestIndex]->fCost()) {
                lowestIndex = i;
            }
        }

        currentNode = openSet[lowestIndex];
        openSet.removeAt(lowestIndex);
        currentNode->inOpenSet = false;
        currentNode->inClosedSet = true;

        // Check if reached target
        if (currentNode == targetNode) {
            qDebug() << "[A*] Path found after" << iterations << "iterations";
            break;
        }

        // Check neighbors
        QVector<GridNode*> neighbors = getNeighbors(currentNode, grid, gridWidth, gridHeight);
        for (GridNode* neighbor : neighbors) {
            if (!neighbor->isSafe || neighbor->inClosedSet) {
                continue;
            }

            double tentativeGCost = currentNode->gCost +
                                    haversineDistanceNm(GeoPoint{currentNode->lat, currentNode->lon},
                                                       GeoPoint{neighbor->lat, neighbor->lon});

            if (!neighbor->inOpenSet) {
                neighbor->inOpenSet = true;
                openSet.append(neighbor);
            } else if (tentativeGCost >= neighbor->gCost) {
                continue;
            }

            neighbor->parent = currentNode;
            neighbor->gCost = tentativeGCost;
            neighbor->hCost = haversineDistanceNm(GeoPoint{neighbor->lat, neighbor->lon},
                                                  GeoPoint{targetNode->lat, targetNode->lon});
        }
    }

    // Reconstruct path
    if (currentNode == targetNode) {
        GridNode* node = targetNode;
        while (node != nullptr) {
            path.prepend(GeoPoint{node->lat, node->lon});
            node = node->parent;
        }

        qDebug() << "[A*] Reconstructed path with" << path.size() << "nodes";

        // Simplify path (Douglas-Peucker-like simplification)
        QVector<GeoPoint> simplifiedPath;
        if (!path.isEmpty()) {
            simplifiedPath.append(path.first());

            for (int i = 1; i < path.size() - 1; i += 2) { // Take every 2nd point
                simplifiedPath.append(path[i]);
            }

            simplifiedPath.append(path.last());
        }

        path = simplifiedPath;
        qDebug() << "[A*] Simplified path to" << path.size() << "waypoints";
    } else {
        qDebug() << "[A*] No path found after" << iterations << "iterations";
    }

    // Cleanup
    for (auto& row : grid) {
        for (auto node : row) delete node;
    }

    return path;
}

QVector<AutoRoutePlanner::GridNode*> AutoRoutePlanner::getNeighbors(GridNode* node,
                                                                     QVector<QVector<GridNode*>>& grid,
                                                                     int gridWidth, int gridHeight) const
{
    QVector<GridNode*> neighbors;

    // Find node position in grid
    int nodeRow = -1, nodeCol = -1;
    for (int i = 0; i < gridHeight; ++i) {
        for (int j = 0; j < gridWidth; ++j) {
            if (grid[i][j] == node) {
                nodeRow = i;
                nodeCol = j;
                break;
            }
        }
        if (nodeRow >= 0) break;
    }

    if (nodeRow < 0 || nodeCol < 0) return neighbors;

    // 8-direction neighbors
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int i = 0; i < 8; ++i) {
        int newRow = nodeRow + dy[i];
        int newCol = nodeCol + dx[i];

        if (newRow >= 0 && newRow < gridHeight && newCol >= 0 && newCol < gridWidth) {
            GridNode* neighbor = grid[newRow][newCol];

            // For diagonal moves (i = 0, 2, 5, 7), check that adjacent cells are also safe
            // This prevents cutting corners through land
            bool isDiagonal = (i == 0 || i == 2 || i == 5 || i == 7);
            if (isDiagonal) {
                // For diagonal movement, check the two adjacent orthogonal cells
                int orthogonal1Row = nodeRow + dy[i];
                int orthogonal1Col = nodeCol;
                int orthogonal2Row = nodeRow;
                int orthogonal2Col = nodeCol + dx[i];

                bool orthogonal1Safe = (orthogonal1Row >= 0 && orthogonal1Row < gridHeight &&
                                       orthogonal1Col >= 0 && orthogonal1Col < gridWidth &&
                                       grid[orthogonal1Row][orthogonal1Col]->isSafe);

                bool orthogonal2Safe = (orthogonal2Row >= 0 && orthogonal2Row < gridHeight &&
                                       orthogonal2Col >= 0 && orthogonal2Col < gridWidth &&
                                       grid[orthogonal2Row][orthogonal2Col]->isSafe);

                // Only allow diagonal movement if both orthogonal cells are also safe
                if (orthogonal1Safe && orthogonal2Safe) {
                    neighbors.append(neighbor);
                }
            } else {
                // Orthogonal movement - always allowed if target cell is safe
                neighbors.append(neighbor);
            }
        }
    }

    return neighbors;
}
