#include "aitargettracker.h"
#include "ecwidget.h"
#include <QtMath>
#include <QPainterPath>
#include <QString>
#include <QFont>
#include <QFontMetrics>
#include <QDebug>

// SevenCs Kernel EC2007
#ifdef _WIN32
#include <windows.h>
#pragma pack(push, 4)
#include <eckernel.h>
#pragma pack (pop)
#else
#include <stdio.h>
#include <X11/Xlib.h>
#include <eckernel.h>
#endif

AITargetTracker::AITargetTracker(QObject* parent)
    : QObject(parent)
{
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &AITargetTracker::onTrackingUpdate);
    updateTimer->start(100); // Update every 100ms for real-time tracking
}

void AITargetTracker::setTarget(const QString& mmsi, const QString& name)
{
    targetMMSI = mmsi;
    targetName = name.isEmpty() ? mmsi : name;
    hasValidTarget = !mmsi.isEmpty();

    if (hasValidTarget) {
        trackingEnabled = true;
        targetHistory.clear();
        lastUpdate = QDateTime::currentDateTime();

        // Initialize positions to prevent null pointer issues during initial tracking
        if (targetPos.isNull()) {
            targetPos = QPointF(0.0, 0.0); // Will be updated when real data arrives
        }
        if (ownshipPos.isNull()) {
            ownshipPos = QPointF(0.0, 0.0); // Will be updated when real data arrives
        }

        qDebug() << "[AITargetTracker] Started tracking target:" << targetName << "MMSI:" << mmsi;
    } else {
        clearTarget();
    }
}

void AITargetTracker::clearTarget()
{
    trackingEnabled = false;
    targetMMSI.clear();
    targetName.clear();
    hasValidTarget = false;
    targetHistory.clear();
    hasInterceptSolution = false;
    currentDistanceNM = 0.0;
    currentBearingDeg = 0.0;
    relativeSpeed = 0.0;
    timeToIntercept = 0.0;

    qDebug() << "[AITargetTracker] Cleared target tracking";
}

void AITargetTracker::updateTargetPosition(double lat, double lon, double course, double speed)
{
    if (!hasValidTarget || !trackingEnabled) return;

    QPointF oldPos = targetPos;
    targetPos = QPointF(lat, lon);
    targetCourse = course;
    targetSpeed = speed;
    lastUpdate = QDateTime::currentDateTime();

    qDebug() << "[AITargetTracker] Target position updated:"
             << "MMSI:" << targetMMSI
             << "OLD:" << oldPos.x() << "," << oldPos.y()
             << "NEW:" << lat << "," << lon
             << "COG:" << course << "SOG:" << speed;

    addToHistory(lat, lon, course, speed);
    calculateTrackingData();
}

void AITargetTracker::updateOwnShipPosition(double lat, double lon, double course, double speed)
{
    ownshipPos = QPointF(lat, lon);
    ownshipCourse = course;
    ownshipSpeed = speed;

    if (trackingEnabled && hasValidTarget) {
        calculateTrackingData();
    }
}

void AITargetTracker::calculateTrackingData()
{
    if (!isValidTargetData()) return;

    // Calculate distance and bearing using kernel function
    double dist = 0.0, bearing = 0.0;
    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                         ownshipPos.x(), ownshipPos.y(),
                                         targetPos.x(), targetPos.y(),
                                         &dist, &bearing);

    currentDistanceNM = dist;
    currentBearingDeg = bearing;

    // Calculate relative speed
    double ownshipVx = ownshipSpeed * qSin(qDegreesToRadians(ownshipCourse));
    double ownshipVy = ownshipSpeed * qCos(qDegreesToRadians(ownshipCourse));
    double targetVx = targetSpeed * qSin(qDegreesToRadians(targetCourse));
    double targetVy = targetSpeed * qCos(qDegreesToRadians(targetCourse));

    double relVx = targetVx - ownshipVx;
    double relVy = targetVy - ownshipVy;
    relativeSpeed = qSqrt(relVx * relVx + relVy * relVy);

    // Calculate intercept solution
    calculateInterceptSolution();

    // Predict target position
    predictTargetPosition(30.0); // 30 seconds ahead
}

void AITargetTracker::calculateInterceptSolution()
{
    if (!isValidTargetData() || missileSpeed <= 0) {
        hasInterceptSolution = false;
        return;
    }

    // Simplified intercept calculation
    // Time to intercept based on relative motion
    if (relativeSpeed > 0.1) { // Avoid division by zero
        timeToIntercept = (currentDistanceNM / relativeSpeed) * 60; // Convert to minutes
    } else {
        timeToIntercept = (currentDistanceNM / missileSpeed) * 60;
    }

    // Calculate lead angle
    calculateLeadAngle();

    // Calculate intercept point
    double interceptBearing = currentBearingDeg + leadAngle;
    EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                ownshipPos.x(), ownshipPos.y(),
                                currentDistanceNM, interceptBearing,
                                &interceptPoint.rx(), &interceptPoint.ry());

    hasInterceptSolution = true;
}

void AITargetTracker::predictTargetPosition(double timeAheadSec)
{
    if (!isValidTargetData() || targetHistory.isEmpty()) {
        return;
    }

    // Simple linear prediction based on current course and speed
    double distanceAhead = (targetSpeed * timeAheadSec) / 3600.0; // Convert to NM
    EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                targetPos.x(), targetPos.y(),
                                distanceAhead, targetCourse,
                                &predictedPosition.rx(), &predictedPosition.ry());
}

void AITargetTracker::calculateLeadAngle()
{
    if (!isValidTargetData() || relativeSpeed <= 0) {
        leadAngle = 0.0;
        return;
    }

    // Simplified lead angle calculation
    double targetCrossTrack = targetSpeed * qSin(qDegreesToRadians(targetCourse - (double)currentBearingDeg));
    leadAngle = qRadiansToDegrees(qAsin(qBound(-1.0, targetCrossTrack / missileSpeed, 1.0)));
}

void AITargetTracker::addToHistory(double lat, double lon, double course, double speed)
{
    PositionSnapshot snapshot;
    snapshot.position = QPointF(lat, lon);
    snapshot.timestamp = QDateTime::currentDateTime();
    snapshot.course = course;
    snapshot.speed = speed;

    targetHistory.append(snapshot);

    // Keep history size limited
    while (targetHistory.size() > MAX_HISTORY_SIZE) {
        targetHistory.removeFirst();
    }
}

bool AITargetTracker::isValidTargetData() const
{
    return hasValidTarget &&
           trackingEnabled &&
           (targetPos != QPointF(0.0, 0.0)) &&  // Check for actual position data
           (ownshipPos != QPointF(0.0, 0.0)); // Check for actual ownship data
}

void AITargetTracker::draw(EcWidget* w, QPainter& p)
{
    if (!w || !w->isReady() || !trackingEnabled || !hasValidTarget) return;

    // Convert positions to screen coordinates
    int ownshipX = 0, ownshipY = 0;
    int targetX = 0, targetY = 0;

    if (!w->LatLonToXy(ownshipPos.x(), ownshipPos.y(), ownshipX, ownshipY) ||
        !w->LatLonToXy(targetPos.x(), targetPos.y(), targetX, targetY)) {
        return;
    }

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // Draw target line (main line from ownship to target)
    if (showTargetLine) {
        QPen targetPen(QColor(255, 0, 0)); // Red line for targeting
        targetPen.setWidth(3);
        targetPen.setStyle(Qt::SolidLine);
        p.setPen(targetPen);
        p.drawLine(ownshipX, ownshipY, targetX, targetY);

        // Draw arrow head
        double angle = qAtan2(targetY - ownshipY, targetX - ownshipX);
        int arrowLength = 15;
        double arrowAngle = 25.0;

        QPolygonF arrow;
        arrow << QPointF(targetX, targetY);
        arrow << QPointF(targetX - arrowLength * qCos(angle - qDegreesToRadians(arrowAngle)),
                        targetY - arrowLength * qSin(angle - qDegreesToRadians(arrowAngle)));
        arrow << QPointF(targetX - arrowLength * qCos(angle + qDegreesToRadians(arrowAngle)),
                        targetY - arrowLength * qSin(angle + qDegreesToRadians(arrowAngle)));

        p.setBrush(QColor(255, 0, 0));
        p.drawPolygon(arrow);
    }

    // Draw prediction line if enabled
    if (showPredictionLine && hasInterceptSolution) {
        int interceptX = 0, interceptY = 0;
        if (w->LatLonToXy(interceptPoint.x(), interceptPoint.y(), interceptX, interceptY)) {
            QPen predictionPen(QColor(255, 165, 0)); // Orange for prediction
            predictionPen.setWidth(2);
            predictionPen.setStyle(Qt::DashLine);
            p.setPen(predictionPen);
            p.drawLine(targetX, targetY, interceptX, interceptY);

            // Draw intercept point
            p.setBrush(QColor(255, 165, 0));
            p.setPen(Qt::NoPen);
            p.drawEllipse(interceptX, interceptY, 6, 6);
        }
    }

    // Draw distance and tracking information
    if (showDistanceInfo) {
        QFont font("Arial", 10, QFont::Bold);
        p.setFont(font);
        QFontMetrics fm(font);

        // Build info text
        QStringList infoLines;
        infoLines << QString("Target: %1").arg(targetName);
        infoLines << QString("Distance: %1").arg(formatDistance(currentDistanceNM));
        infoLines << QString("Bearing: %1°").arg(qRound(currentBearingDeg));
        infoLines << QString("Rel.Speed: %1").arg(formatSpeed(relativeSpeed));

        if (hasInterceptSolution) {
            infoLines << QString("Time to Intercept: %1").arg(formatTime(timeToIntercept));
            infoLines << QString("Lead Angle: %1°").arg(qRound(leadAngle));
        }

        // Draw background rectangle
        int padding = 8;
        int lineHeight = fm.height();
        int boxWidth = 0;

        for (const QString& line : infoLines) {
            boxWidth = qMax(boxWidth, fm.horizontalAdvance(line));
        }

        int boxHeight = infoLines.size() * lineHeight + padding * 2;
        int boxX = targetX + 20;
        int boxY = targetY - boxHeight / 2;

        // Theme-aware colors
        QColor win = w->palette().color(QPalette::Window);
        int luma = qRound(0.2126 * win.red() + 0.7152 * win.green() + 0.0722 * win.blue());
        bool darkTheme = (luma < 128);

        QColor bgCol = darkTheme ? QColor(0, 0, 0, 200) : QColor(255, 255, 255, 220);
        QColor fgCol = darkTheme ? QColor(255, 255, 255) : QColor(0, 0, 0);

        p.setPen(Qt::NoPen);
        p.setBrush(bgCol);
        p.drawRoundedRect(boxX, boxY, boxWidth + padding * 2, boxHeight, 6, 6);

        // Draw text
        p.setPen(fgCol);
        int textY = boxY + padding + fm.ascent();
        for (const QString& line : infoLines) {
            p.drawText(boxX + padding, textY, line);
            textY += lineHeight;
        }
    }

    p.restore();
}

QString AITargetTracker::formatDistance(double nm) const
{
    if (nm < 1.0) {
        return QString("%1 m").arg(qRound(nm * 1852));
    } else {
        return QString("%1 NM").arg(QString::number(nm, 'f', 2));
    }
}

QString AITargetTracker::formatSpeed(double knots) const
{
    return QString("%1 kt").arg(QString::number(knots, 'f', 1));
}

QString AITargetTracker::formatTime(double minutes) const
{
    if (minutes < 1.0) {
        return QString("%1 s").arg(qRound(minutes * 60));
    } else {
        return QString("%1 min").arg(QString::number(minutes, 'f', 1));
    }
}

QString AITargetTracker::getTargetInfo() const
{
    if (!hasValidTarget) {
        return "No Target";
    }

    return QString("Target: %1 (%2) - Distance: %3")
           .arg(targetName)
           .arg(targetMMSI)
           .arg(formatDistance(currentDistanceNM));
}

void AITargetTracker::onTrackingUpdate()
{
    if (trackingEnabled && hasValidTarget) {
        // Auto-update calculations for real-time tracking
        calculateTrackingData();
    }
}