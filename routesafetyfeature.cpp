#include "routesafetyfeature.h"

#include "ecwidget.h"
#include "SettingsManager.h"
#include "SettingsData.h"

#include <QPainter>
#include <QPen>
#include <QColor>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QByteArray>
#include <QDebug>
#include <QtMath>

#include <cmath>
#include <cstring>

RouteSafetyFeature::RouteSafetyFeature(EcWidget* widget, QObject* parent)
    : QObject(parent), ecWidget(widget)
{
    refreshSafetyParams();
}

void RouteSafetyFeature::startFrame()
{
    ++frameCounter;
    routesPreparedThisFrame.clear();
    hazardMarkers.clear();
}

void RouteSafetyFeature::prepareForRoute(int routeId, const QVector<RouteWaypointSample>& routePoints)
{
    if (!ecWidget || routeId <= 0 || routePoints.size() < 2) {
        return;
    }

    routesPreparedThisFrame.insert(routeId);
    refreshSafetyParams();

    QVector<QPair<double, double>> key;
    key.reserve(routePoints.size());
    for (const RouteWaypointSample& point : routePoints) {
        if (!point.active) {
            continue;
        }
        key.append(qMakePair(point.lat, point.lon));
    }

    if (key.size() < 2) {
        routeCaches.remove(routeId);
        return;
    }

    RouteCache& cache = routeCaches[routeId];
    const bool topologyChanged = (cache.pointKey != key);
    const bool paramsChanged = (cache.paramsRevision != paramsRevisionCounter);

    if (topologyChanged || paramsChanged) {
        cache.pointKey = key;
        cache.paramsRevision = paramsRevisionCounter;
        evaluateRoute(routeId, key, cache);
    }

    cache.lastPreparedFrame = frameCounter;
}

void RouteSafetyFeature::render(QPainter& painter)
{
    if (!ecWidget || routesPreparedThisFrame.isEmpty()) {
        return;
    }

    hazardMarkers.clear();

    const QColor warningColor(255, 204, 0, 160);
    const QColor dangerColor(220, 20, 60, 160);
    constexpr int warningRadius = 5;
    constexpr int dangerRadius = 6;

    painter.save();
    painter.setPen(Qt::NoPen);

    for (int routeId : routesPreparedThisFrame) {
        const auto cacheIt = routeCaches.constFind(routeId);
        if (cacheIt == routeCaches.constEnd()) {
            continue;
        }

        const QList<HazardSegment>& segments = cacheIt->segments;
        for (const HazardSegment& segment : segments) {
            int markerX = 0;
            int markerY = 0;
            if (!ecWidget->LatLonToXy(segment.markerLat, segment.markerLon, markerX, markerY)) {
                continue;
            }

            const bool isDanger = (segment.level == HazardLevel::Danger);
            const QColor color = isDanger ? dangerColor : warningColor;
            const int radius = isDanger ? dangerRadius : warningRadius;

            painter.setPen(QPen(color.darker(200), 2));
            painter.setBrush(color);
            painter.drawEllipse(QPoint(markerX, markerY), radius, radius);

            HazardMarker marker;
            marker.screenPos = QPoint(markerX, markerY);
            marker.segment = segment;
            marker.radius = radius;
            hazardMarkers.append(marker);
        }
    }

    painter.restore();
}

void RouteSafetyFeature::finishFrame()
{
    routesPreparedThisFrame.clear();
}

void RouteSafetyFeature::invalidateRoute(int routeId)
{
    routeCaches.remove(routeId);
    hazardMarkers.clear();
}

void RouteSafetyFeature::invalidateAll()
{
    routeCaches.clear();
    hazardMarkers.clear();
}

void RouteSafetyFeature::refreshSafetyParams()
{
    const SettingsData& settings = SettingsManager::instance().data();
    SafetyParams current;

    double draft = settings.shipDraftMeters;
    if (draft <= 0.0) {
        draft = settings.shipHeight;
    }
    if (draft <= 0.0 && settings.shipLength > 0.0) {
        draft = settings.shipLength * 0.05;
    }
    if (draft <= 0.0) {
        draft = 3.0;
    }

    double ukcDanger = settings.ukcDangerMeters;
    if (ukcDanger < 0.0) {
        ukcDanger = 0.0;
    }

    double ukcWarning = settings.ukcWarningMeters;
    if (ukcWarning <= 0.0) {
        ukcWarning = qMax(0.5, draft * 0.2);
    }

    current.shipDraft = draft;
    current.ukcDanger = ukcDanger;
    current.ukcWarning = qMax(ukcWarning, ukcDanger + 0.5);

    if (safetyParamsChanged(current)) {
        safetyParams = current;
        ++paramsRevisionCounter;
    }

    qDebug() << "[ROUTE-SAFETY] params" << draft << ukcDanger << current.ukcWarning;
}

bool RouteSafetyFeature::safetyParamsChanged(const SafetyParams& other) const
{
    auto diff = [](double a, double b) {
        return std::abs(a - b) > 1e-3;
    };

    return diff(safetyParams.shipDraft, other.shipDraft) ||
           diff(safetyParams.ukcDanger, other.ukcDanger) ||
           diff(safetyParams.ukcWarning, other.ukcWarning);
}

void RouteSafetyFeature::evaluateRoute(int routeId, const QVector<QPair<double, double>>& points, RouteCache& cache)
{
    cache.segments.clear();
    cache.segments.reserve(points.size());

    for (int i = 0; i < points.size() - 1; ++i) {
        analyzeSegment(routeId, i, points[i], points[i + 1], cache.segments);
    }
}

bool RouteSafetyFeature::analyzeSegment(int routeId,
                                        int segmentIndex,
                                        const QPair<double, double>& start,
                                        const QPair<double, double>& end,
                                        QList<HazardSegment>& outSegments)
{
    const double lat1 = start.first;
    const double lon1 = start.second;
    const double lat2 = end.first;
    const double lon2 = end.second;

    double distanceNm = 0.0;
    double bearingDeg = 0.0;
    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                           lat1, lon1,
                                           lat2, lon2,
                                           &distanceNm, &bearingDeg);

    const double segmentLengthNm = qAbs(distanceNm);
    if (segmentLengthNm < 1e-6) {
        return false;
    }

    static constexpr double samplingStepNm = 0.05;
    static constexpr int minSamples = 3;
    static constexpr int maxSamples = 200;

    int sampleCount = static_cast<int>(std::ceil(segmentLengthNm / samplingStepNm));
    sampleCount = qBound(minSamples, sampleCount, maxSamples);

    HazardLevel worstLevel = HazardLevel::Safe;
    double worstDepth = std::numeric_limits<double>::quiet_NaN();
    double worstUkc = std::numeric_limits<double>::quiet_NaN();
    double worstLat = (lat1 + lat2) / 2.0;
    double worstLon = (lon1 + lon2) / 2.0;

    for (int i = 0; i <= sampleCount; ++i) {
        const double travelledNm = (segmentLengthNm * i) / sampleCount;
        double sampleLat = lat1;
        double sampleLon = lon1;
        EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                     lat1, lon1,
                                     travelledNm,
                                     bearingDeg,
                                     &sampleLat, &sampleLon);

        double minDepth = std::numeric_limits<double>::quiet_NaN();
        double ukc = std::numeric_limits<double>::quiet_NaN();
        HazardLevel level = classifyPoint(sampleLat, sampleLon, minDepth, ukc);

        if (static_cast<int>(level) > static_cast<int>(worstLevel) ||
            (level == worstLevel && ((qIsNaN(worstDepth) && !qIsNaN(minDepth)) ||
                                     (!qIsNaN(minDepth) && minDepth < worstDepth)))) {
            worstLevel = level;
            worstDepth = minDepth;
            worstUkc = ukc;
            worstLat = sampleLat;
            worstLon = sampleLon;
        }
    }

    if (worstLevel == HazardLevel::Safe) {
        qDebug() << "[ROUTE-SAFETY] segment safe" << routeId << segmentIndex;
        return false;
    }

    HazardSegment segment;
    segment.routeId = routeId;
    segment.segmentIndex = segmentIndex;
    segment.startLat = lat1;
    segment.startLon = lon1;
    segment.endLat = lat2;
    segment.endLon = lon2;
    segment.markerLat = worstLat;
    segment.markerLon = worstLon;
    segment.minDepthMeters = worstDepth;
    segment.underKeelClearance = worstUkc;
    segment.level = worstLevel;

    qDebug() << "[ROUTE-SAFETY] segment" << routeId << segmentIndex
             << "level" << static_cast<int>(worstLevel)
             << "depth" << worstDepth
             << "ukc" << worstUkc;

    outSegments.append(segment);
    return true;
}

RouteSafetyFeature::HazardLevel RouteSafetyFeature::classifyPoint(double lat, double lon, double& minDepthOut, double& ukcOut)
{
    minDepthOut = std::numeric_limits<double>::quiet_NaN();
    ukcOut = std::numeric_limits<double>::quiet_NaN();

    if (!ecWidget) {
        qDebug() << "[ROUTE-SAFETY] no widget";
        return HazardLevel::Safe;
    }

    QList<EcFeature> features;
    ecWidget->GetPickedFeaturesSubs(features, lat, lon);
    qDebug() << "[ROUTE-SAFETY] classifyPoint" << lat << lon << "features" << features.size();

    if (features.isEmpty()) {
        qDebug() << "[ROUTE-SAFETY] no features";
        return HazardLevel::Safe;
    }

    bool depthFound = false;
    bool landDetected = false;
    double shallowestDepth = std::numeric_limits<double>::quiet_NaN();

    auto considerDepth = [&](double value)
    {
        if (qIsNaN(value)) {
            return;
        }
        if (!depthFound || value < shallowestDepth) {
            depthFound = true;
            shallowestDepth = value;
        }
    };

    auto parseDepthValues = [&](const char* raw) -> bool
    {
        if (!raw) return false;
        QString attrString = QString::fromLatin1(raw, EC_LENATRCODE).trimmed();
        QString valuePart = QString::fromLatin1(raw + EC_LENATRCODE).trimmed();
        if (valuePart.isEmpty()) {
            qDebug() << "[ROUTE-SAFETY] parseDepth empty value" << attrString;
            return false;
        }

        QStringList tokens = valuePart.split(QRegularExpression("[ ,;\t]+"), Qt::SkipEmptyParts);
        if (tokens.isEmpty()) {
            qDebug() << "[ROUTE-SAFETY] parseDepth tokens empty" << attrString;
            return false;
        }

        bool parsed = false;
        for (const QString& token : tokens) {
            bool ok = false;
            double value = token.toDouble(&ok);
            if (!ok) {
                qDebug() << "[ROUTE-SAFETY] parseDepth token fail" << token;
                continue;
            }
            considerDepth(value);
            parsed = true;
            qDebug() << "[ROUTE-SAFETY] depth token" << token;
        }
        return parsed;
    };

    static const QSet<QString> landTokens = {
        QStringLiteral("LNDARE"),
        QStringLiteral("SLCONS"),
        QStringLiteral("COALNE"),
        QStringLiteral("UNSARE"),
        QStringLiteral("PONTON"),
        QStringLiteral("OBSTRN"),
        QStringLiteral("UWTROC"),
        QStringLiteral("BRIDGE"),
        QStringLiteral("DMPGRD")
    };

    for (const EcFeature& feature : features) {
        char classToken[EC_LENATRCODE + 1] = {};
        EcFeatureGetClass(feature, ecWidget->getDictionaryInfo(), classToken, sizeof(classToken));
        const QString token = QString::fromLatin1(classToken).toUpper();

        qDebug() << "[ROUTE-SAFETY] feature token" << token;

        if (token == QStringLiteral("DEPARE")) {
            EcFindInfo attrInfo;
            char attrStr[1024];
            Bool result = EcFeatureGetAttributes(feature, ecWidget->getDictionaryInfo(), &attrInfo, EC_FIRST, attrStr, sizeof(attrStr));
            while (result) {
                EcAttributeToken attrToken;
                std::memcpy(attrToken, attrStr, EC_LENATRCODE);
                attrToken[EC_LENATRCODE] = '\0';
                QString attrName = QString::fromLatin1(attrToken).trimmed();

                if (attrName.compare(QStringLiteral("DRVAL1"), Qt::CaseInsensitive) == 0 ||
                    attrName.compare(QStringLiteral("DRVAL2"), Qt::CaseInsensitive) == 0) {
                    bool parsed = parseDepthValues(attrStr);
                    qDebug() << "[ROUTE-SAFETY] DEPARE" << attrName << QString::fromLatin1(attrStr).trimmed() << "parsed" << parsed;
                }

                result = EcFeatureGetAttributes(feature, ecWidget->getDictionaryInfo(), &attrInfo, EC_NEXT, attrStr, sizeof(attrStr));
            }
        } else if (token == QStringLiteral("SOUNDG")) {
            EcFindInfo attrInfo;
            char attrStr[1024];
            Bool result = EcFeatureGetAttributes(feature, ecWidget->getDictionaryInfo(), &attrInfo, EC_FIRST, attrStr, sizeof(attrStr));
            while (result) {
                EcAttributeToken attrToken;
                std::memcpy(attrToken, attrStr, EC_LENATRCODE);
                attrToken[EC_LENATRCODE] = '\0';
                if (QString::fromLatin1(attrToken).compare(QStringLiteral("VALSOU"), Qt::CaseInsensitive) == 0) {
                    bool parsed = parseDepthValues(attrStr);
                    qDebug() << "[ROUTE-SAFETY] SOUNDG" << QString::fromLatin1(attrStr).trimmed() << "parsed" << parsed;
                }
                result = EcFeatureGetAttributes(feature, ecWidget->getDictionaryInfo(), &attrInfo, EC_NEXT, attrStr, sizeof(attrStr));
            }
        } else if (landTokens.contains(token)) {
            landDetected = true;
            qDebug() << "[ROUTE-SAFETY] land token detected";
        }
    }

    if (landDetected) {
        qDebug() << "[ROUTE-SAFETY] land detected => danger";
        minDepthOut = 0.0;
        ukcOut = -qMax(0.0, safetyParams.shipDraft);
        return HazardLevel::Danger;
    }

    if (!depthFound) {
        qDebug() << "[ROUTE-SAFETY] no depth found";
        return HazardLevel::Safe;
    }

    double ukc = std::numeric_limits<double>::quiet_NaN();
    HazardLevel level = classifyDepth(shallowestDepth, ukc);
    qDebug() << "[ROUTE-SAFETY] depth result" << shallowestDepth
             << "ukc" << ukc
             << "level" << static_cast<int>(level)
             << "draft" << safetyParams.shipDraft
             << "warn" << safetyParams.ukcWarning
             << "danger" << safetyParams.ukcDanger;

    minDepthOut = shallowestDepth;
    ukcOut = ukc;
    return level;
}

QString RouteSafetyFeature::buildTooltip(const HazardSegment& segment) const
{
    QString levelText = segment.level == HazardLevel::Danger
        ? tr("Kedalaman berbahaya")
        : tr("Kedalaman waspada");

    QString depthText;
    if (qIsNaN(segment.minDepthMeters)) {
        depthText = tr("Kedalaman minimum tidak diketahui");
    } else {
        depthText = tr("Kedalaman minimum: %1 m").arg(segment.minDepthMeters, 0, 'f', 1);
    }

    QString ukcText;
    if (qIsNaN(segment.underKeelClearance)) {
        ukcText = tr("Sisa kedalaman: tidak tersedia");
    } else {
        ukcText = tr("Sisa kedalaman: %1 m").arg(segment.underKeelClearance, 0, 'f', 1);
    }

    QString routeLine = tr("Route %1 - Segmen %2")
        .arg(segment.routeId)
        .arg(segment.segmentIndex + 1);

    QStringList lines;
    lines << routeLine;
    lines << levelText;
    lines << depthText;
    lines << ukcText;
    return lines.join(QStringLiteral("\n"));
}

QString RouteSafetyFeature::tooltipForPosition(const QPoint& screenPos) const
{
    if (hazardMarkers.isEmpty()) {
        return QString();
    }

    const int hoverPadding = 4;
    QString bestTooltip;
    int bestScore = 0;

    for (const HazardMarker& marker : hazardMarkers) {
        const int radius = marker.radius + hoverPadding;
        const int dx = screenPos.x() - marker.screenPos.x();
        const int dy = screenPos.y() - marker.screenPos.y();
        if ((dx * dx + dy * dy) > radius * radius) {
            continue;
        }

        const int score = marker.segment.level == HazardLevel::Danger ? 2 : 1;
        if (score < bestScore) {
            continue;
        }

        QString tip = buildTooltip(marker.segment);
        if (tip.isEmpty()) {
            continue;
        }

        bestScore = score;
        bestTooltip = tip;

        if (score == 2) {
            break;
        }
    }

    return bestTooltip;
}

RouteSafetyFeature::HazardLevel RouteSafetyFeature::classifyDepth(double minDepth, double& ukcOut) const
{
    ukcOut = std::numeric_limits<double>::quiet_NaN();

    if (minDepth <= 0.0) {
        ukcOut = -safetyParams.shipDraft;
        qDebug() << "[ROUTE-SAFETY] classifyDepth => danger (<=0)" << minDepth;
        return HazardLevel::Danger;
    }

    if (safetyParams.shipDraft > 0.0) {
        ukcOut = minDepth - safetyParams.shipDraft;
        if (ukcOut <= 0.0) {
            qDebug() << "[ROUTE-SAFETY] classifyDepth => danger (ukc <= 0)" << ukcOut;
            return HazardLevel::Danger;
        }
        if (safetyParams.ukcDanger > 0.0 && ukcOut <= safetyParams.ukcDanger) {
            qDebug() << "[ROUTE-SAFETY] classifyDepth => danger (<= ukcDanger)" << ukcOut << safetyParams.ukcDanger;
            return HazardLevel::Danger;
        }
        if (safetyParams.ukcWarning > 0.0 && ukcOut <= safetyParams.ukcWarning) {
            qDebug() << "[ROUTE-SAFETY] classifyDepth => warning" << ukcOut << safetyParams.ukcWarning;
            return HazardLevel::Warning;
        }
    } else {
        if (minDepth < 2.0) {
            qDebug() << "[ROUTE-SAFETY] classifyDepth fallback danger" << minDepth;
            return HazardLevel::Danger;
        }
        if (minDepth < 5.0) {
            qDebug() << "[ROUTE-SAFETY] classifyDepth fallback warning" << minDepth;
            return HazardLevel::Warning;
        }
    }

    qDebug() << "[ROUTE-SAFETY] classifyDepth safe" << minDepth << ukcOut;
    return HazardLevel::Safe;
}
