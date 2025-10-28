// A* Pathfinding Implementation for AutoRoutePlanner
// Add this to autorouteplanner.cpp at the end

QVector<GeoPoint> AutoRoutePlanner::findSafePathAStar(const GeoPoint& start,
                                                       const GeoPoint& target,
                                                       const AutoRouteOptions& options) const
{
    QVector<GeoPoint> path;

    // Calculate grid resolution based on distance
    double distance = haversineDistanceNm(start, target);
    double gridStepNm = qMax(0.5, distance / 50.0); // At least 50 grid cells, min 0.5 NM per cell

    qDebug() << "[A*] Distance:" << distance << "NM, Grid step:" << gridStepNm << "NM";

    // Create bounding box
    double minLat = qMin(start.lat, target.lat) - 0.5;
    double maxLat = qMax(start.lat, target.lat) + 0.5;
    double minLon = qMin(start.lon, target.lon) - 0.5;
    double maxLon = qMax(start.lon, target.lon) + 0.5;

    // Calculate grid size
    double latRangeNm = haversineDistanceNm(GeoPoint{minLat, start.lon}, GeoPoint{maxLat, start.lon});
    double lonRangeNm = haversineDistanceNm(GeoPoint{start.lat, minLon}, GeoPoint{start.lat, maxLon});

    int gridHeight = qMax(10, static_cast<int>(latRangeNm / gridStepNm));
    int gridWidth = qMax(10, static_cast<int>(lonRangeNm / gridStepNm));

    // Limit grid size for performance
    if (gridHeight > 100) gridHeight = 100;
    if (gridWidth > 100) gridWidth = 100;

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
    const int maxIterations = gridWidth * gridHeight * 2;

    while (!openSet.isEmpty() && iterations < maxIterations) {
        ++iterations;

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
            neighbors.append(grid[newRow][newCol]);
        }
    }

    return neighbors;
}
