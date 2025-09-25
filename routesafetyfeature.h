#ifndef ROUTESAFETYFEATURE_H
#define ROUTESAFETYFEATURE_H

#include <QObject>
#include <QVector>
#include <QList>
#include <QHash>
#include <QSet>
#include <QPair>
#include <QtGlobal>
#include <QPoint>
#include <QString>
#include <limits>

class EcWidget;
class QPainter;

class RouteSafetyFeature : public QObject
{
    Q_OBJECT
public:
    enum class HazardLevel {
        Safe = 0,
        Warning = 1,
        Danger = 2
    };

    struct RouteWaypointSample {
        double lat = 0.0;
        double lon = 0.0;
        bool active = true;
    };

    struct HazardSegment {
        int routeId = 0;
        int segmentIndex = 0;
        double startLat = 0.0;
        double startLon = 0.0;
        double endLat = 0.0;
        double endLon = 0.0;
        double markerLat = 0.0;
        double markerLon = 0.0;
        double minDepthMeters = std::numeric_limits<double>::quiet_NaN();
        double underKeelClearance = std::numeric_limits<double>::quiet_NaN();
        HazardLevel level = HazardLevel::Safe;
    };

    struct HazardMarker {
        QPoint screenPos;
        HazardSegment segment;
        int radius = 0;
    };

    explicit RouteSafetyFeature(EcWidget* widget, QObject* parent = nullptr);

    void startFrame();
    void prepareForRoute(int routeId, const QVector<RouteWaypointSample>& routePoints);
    void render(QPainter& painter);
    QString tooltipForPosition(const QPoint& screenPos) const;
    void finishFrame();
    void invalidateRoute(int routeId);
    void invalidateAll();

private:
    struct SafetyParams {
        double shipDraft = 0.0;
        double ukcDanger = 0.0;
        double ukcWarning = 0.0;
    };

    struct RouteCache {
        QVector<QPair<double, double>> pointKey;
        QList<HazardSegment> segments;
        quint64 paramsRevision = 0;
        quint64 lastPreparedFrame = 0;
    };

    EcWidget* ecWidget;
    QHash<int, RouteCache> routeCaches;
    QSet<int> routesPreparedThisFrame;
    QVector<HazardMarker> hazardMarkers;

    SafetyParams safetyParams;
    quint64 paramsRevisionCounter = 0;
    quint64 frameCounter = 0;

    void refreshSafetyParams();
    bool safetyParamsChanged(const SafetyParams& other) const;
    void evaluateRoute(int routeId, const QVector<QPair<double, double>>& points, RouteCache& cache);
    bool analyzeSegment(int routeId,
                        int segmentIndex,
                        const QPair<double, double>& start,
                        const QPair<double, double>& end,
                        QList<HazardSegment>& outSegments);
    QString buildTooltip(const HazardSegment& segment) const;
    HazardLevel classifyPoint(double lat, double lon, double& minDepthOut, double& ukcOut);
    HazardLevel classifyDepth(double minDepth, double& ukcOut) const;
};

#endif // ROUTESAFETYFEATURE_H

