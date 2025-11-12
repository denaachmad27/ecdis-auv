#include "collisionrisk.h"
#include "ecwidget.h"
#include "eckernel.h"
#include <QDebug>
#include <QtMath>
#include <QElapsedTimer>
#include <algorithm>
#include <limits>

// Constants for performance optimization
static const double EARTH_RADIUS_NM = 3440.065; // Nautical miles
static const double DEG_TO_RAD = M_PI / 180.0;
static const double RAD_TO_DEG = 180.0 / M_PI;

CollisionRiskCalculator::CollisionRiskCalculator(QObject* parent)
    : QObject(parent)
    , currentHighestRiskLevel(NO_RISK)
    , realTimeUpdateTimer(new QTimer(this))
    , cleanupTimer(new QTimer(this))
    , activeAssessments(0)
    , isProcessingQueue(false)
    , realTimeMonitoringActive(false)
    , lastFullAssessment(QDateTime::currentDateTime())
{
    // Initialize for single-threaded processing to avoid QtConcurrent conflicts

    // Setup real-time update timer (10Hz = 100ms)
    realTimeUpdateTimer->setInterval(config.updateFrequencyMs);
    connect(realTimeUpdateTimer, &QTimer::timeout, this, &CollisionRiskCalculator::onRealTimeUpdateTimer);

    // Setup cleanup timer (every 5 seconds)
    cleanupTimer->setInterval(5000);
    connect(cleanupTimer, &QTimer::timeout, this, &CollisionRiskCalculator::onCleanupStaleData);

    // Initialize performance timer
    performanceTimer.start();

    // Initialize prediction cache
    predictionCache.setMaxCost(100); // Cache up to 100 target predictions

    qDebug() << "CollisionRiskCalculator initialized with real-time architecture";
}

CollisionRiskCalculator::~CollisionRiskCalculator()
{
    stopRealTimeMonitoring();
    qDebug() << "CollisionRiskCalculator destroyed";
}

void CollisionRiskCalculator::startRealTimeMonitoring()
{
    if (realTimeMonitoringActive) {
        return;
    }

    qDebug() << "Starting real-time collision risk monitoring at"
             << config.updateFrequencyMs << "ms intervals";

    realTimeMonitoringActive = true;
    realTimeUpdateTimer->start();
    cleanupTimer->start();

    emit performanceAlert("Real-time collision risk monitoring started");
}

void CollisionRiskCalculator::stopRealTimeMonitoring()
{
    if (!realTimeMonitoringActive) {
        return;
    }

    qDebug() << "Stopping real-time collision risk monitoring";

    realTimeMonitoringActive = false;
    realTimeUpdateTimer->stop();
    cleanupTimer->stop();

    emit performanceAlert("Real-time collision risk monitoring stopped");
}

bool CollisionRiskCalculator::isRealTimeMonitoring() const
{
    return realTimeMonitoringActive;
}

void CollisionRiskCalculator::updateOwnShipData(double lat, double lon, double heading, double sog, double rot)
{
    QMutexLocker locker(&dataMutex);

    ownShipState.lat = lat;
    ownShipState.lon = lon;
    ownShipState.cog = heading;
    ownShipState.sog = sog;
    ownShipState.rot = rot;
    ownShipState.lastUpdate = QDateTime::currentDateTime();

    // Apply environmental compensation if enabled
    if (config.enableEnvironmentalCompensation) {
        applyEnvironmentalCompensation(ownShipState, environmentalConditions, 0.0);
    }
}

void CollisionRiskCalculator::updateAISTarget(const QString& mmsi, const AISTargetData& target)
{
    if (target.lat == 0.0 || target.lon == 0.0) {
        return; // Skip invalid positions
    }

    QMutexLocker locker(&dataMutex);

    updateTargetMotionState(mmsi, target);

    // Trigger immediate assessment for close targets
    double distance = qSqrt(qPow(ownShipState.lat - target.lat, 2) + qPow(ownShipState.lon - target.lon, 2));
    if (distance < config.immediateAssessmentRange / 60.0) { // Rough conversion to degrees
        // Queue for immediate assessment
        queueTargetAssessment(mmsi);
    }
}

void CollisionRiskCalculator::updateGuardZones(const QVector<GuardZone>& zones)
{
    QMutexLocker locker(&dataMutex);
    guardZones = zones;
}

void CollisionRiskCalculator::updateEnvironmentalConditions(const EnvironmentalConditions& env)
{
    QMutexLocker locker(&dataMutex);
    environmentalConditions = env;
}

void CollisionRiskCalculator::performImmediateAssessment(const QString& targetId)
{
    if (!realTimeMonitoringActive) {
        return;
    }

    qDebug() << "Performing immediate assessment for target:" << targetId;

    // Run assessment directly (simplified single-threaded approach)
    QTimer::singleShot(0, this, [this, targetId]() {
        QElapsedTimer timer;
        timer.start();

        CollisionRiskResult result;
        {
            QMutexLocker locker(&dataMutex);
            if (!targetStates.contains(targetId) || !ownShipState.isValid()) {
                return;
            }

            result = assessAISCollisionRisk(ownShipState, targetStates[targetId]);
        }

        qint64 elapsedMs = timer.elapsed();

        if (result.isValid()) {
            emit targetAssessmentComplete(targetId, result);
            emit assessmentCompleted(targetId, elapsedMs);

            // Update performance metrics
            QMutexLocker perfLocker(&performanceMutex);
            metrics.totalAssessmentsPerformed++;
            metrics.maxAssessmentTimeMs = qMax(metrics.maxAssessmentTimeMs, elapsedMs);
            metrics.averageAssessmentTimeMs =
                (metrics.averageAssessmentTimeMs * (metrics.totalAssessmentsPerformed - 1) + elapsedMs) /
                metrics.totalAssessmentsPerformed;
        }
    });
}

void CollisionRiskCalculator::performFullAssessment()
{
    if (!realTimeMonitoringActive) {
        return;
    }

    qDebug() << "Performing full collision risk assessment";

    QElapsedTimer timer;
    timer.start();

    // Get relevant targets
    QVector<QString> relevantTargets;
    {
        QMutexLocker locker(&dataMutex);
        if (!ownShipState.isValid()) {
            return;
        }
        relevantTargets = filterRelevantTargets(ownShipState);
    }

    if (relevantTargets.isEmpty()) {
        return;
    }

    // Process targets sequentially (simplified approach to avoid QtConcurrent conflicts)
    QVector<CollisionRiskResult> newRisks;
    for (const QString& targetId : relevantTargets) {
        QElapsedTimer targetTimer;
        targetTimer.start();

        CollisionRiskResult result;
        {
            QMutexLocker locker(&dataMutex);
            if (!targetStates.contains(targetId)) {
                continue; // Skip invalid target
            }

            result = assessAISCollisionRisk(ownShipState, targetStates[targetId]);
        }

        qint64 targetElapsedMs = targetTimer.elapsed();
        emit assessmentCompleted(targetId, targetElapsedMs);

        if (result.isValid()) {
            newRisks.append(result);
        }
    }

    // Update current risks
    {
        QMutexLocker locker(&dataMutex);
        currentRisks.clear();
        for (const auto& risk : newRisks) {
            if (risk.isValid()) {
                currentRisks.append(risk);
            }
        }

        // Update highest risk level
        RiskLevel newHighest = NO_RISK;
        for (const auto& risk : currentRisks) {
            if (risk.riskLevel > newHighest) {
                newHighest = risk.riskLevel;
            }
        }

        if (newHighest != currentHighestRiskLevel) {
            currentHighestRiskLevel = newHighest;
            emit riskLevelChanged(newHighest);
        }
    }

    qint64 totalElapsedMs = timer.elapsed();
    qDebug() << "Full assessment completed in" << totalElapsedMs << "ms for"
             << relevantTargets.size() << "targets";

    emit performanceAlert(QString("Full assessment completed: %1 targets in %2ms")
                         .arg(relevantTargets.size()).arg(totalElapsedMs));
}

void CollisionRiskCalculator::onRealTimeUpdateTimer()
{
    if (!realTimeMonitoringActive || activeAssessments.loadAcquire() >= config.maxConcurrentAssessments) {
        return;
    }

    // Process queued assessments
    processAssessmentQueue();

    // Periodic full assessment every 10 seconds
    static int fullAssessmentCounter = 0;
    fullAssessmentCounter++;

    if (fullAssessmentCounter >= (10000 / config.updateFrequencyMs)) {
        performFullAssessment();
        fullAssessmentCounter = 0;
    }
}

void CollisionRiskCalculator::onTargetAssessmentComplete(const QString& targetId, const CollisionRiskResult& result)
{
    if (!result.isValid()) {
        return;
    }

    // Update current risks atomically
    {
        QMutexLocker locker(&dataMutex);

        // Remove old risk for this target if exists
        currentRisks.erase(std::remove_if(currentRisks.begin(), currentRisks.end(),
            [&targetId](const CollisionRiskResult& existing) {
                return existing.threatId == targetId;
            }), currentRisks.end());

        // Add new risk
        currentRisks.append(result);

        // Update highest risk level
        if (result.riskLevel > currentHighestRiskLevel) {
            currentHighestRiskLevel = result.riskLevel;
            emit riskLevelChanged(currentHighestRiskLevel);
        }
    }

    // Emit appropriate alerts
    emit collisionRiskDetected(result);

    if (result.riskLevel >= HIGH_RISK) {
        emit highRiskAlert(result);
    }

    if (result.riskLevel >= CRITICAL_RISK) {
        emit criticalRiskAlert(result);
    }
}

void CollisionRiskCalculator::onCleanupStaleData()
{
    cleanupStaleTargetData();

    // Clean up stale risks
    {
        QMutexLocker locker(&dataMutex);
        QDateTime now = QDateTime::currentDateTime();

        currentRisks.erase(std::remove_if(currentRisks.begin(), currentRisks.end(),
            [now, this](const CollisionRiskResult& risk) {
                return risk.isStale(now, config.riskFadeTimeSec);
            }), currentRisks.end());
    }

    emit performanceAlert("Stale data cleanup completed");
}

CollisionRiskCalculator::CollisionRiskResult CollisionRiskCalculator::assessAISCollisionRisk(
    const TargetMotionState& ownShip,
    const TargetMotionState& target)
{
    QElapsedTimer timer;
    timer.start();

    CollisionRiskResult result;

    // Predict collision point - look ahead multiple time horizons
    double minCpa = std::numeric_limits<double>::max();
    double minTcpa = std::numeric_limits<double>::max();
    double maxProbability = 0.0;
    QPointF bestCollisionPoint;

    // Check multiple time horizons for accurate prediction
    for (double lookAheadMin = 1.0; lookAheadMin <= config.maxPredictionTime; lookAheadMin += 1.0) {
        TargetMotionState ownFuture = predictTargetPosition(ownShip, lookAheadMin * 60.0);
        TargetMotionState targetFuture = predictTargetPosition(target, lookAheadMin * 60.0);

        // Calculate CPA/TCPA using existing calculator
        CPATCPACalculator calculator;
        VesselState ownVessel = {ownFuture.lat, ownFuture.lon, ownFuture.cog, ownFuture.sog};
        VesselState targetVessel = {targetFuture.lat, targetFuture.lon, targetFuture.cog, targetFuture.sog};

        CPATCPAResult cpaResult = calculator.calculateCPATCPA(ownVessel, targetVessel);

        if (cpaResult.isValid && cpaResult.status != CPATCPAResult::Diverging) {
            if (cpaResult.cpa < minCpa) {
                minCpa = cpaResult.cpa;
                minTcpa = cpaResult.tcpa;
                maxProbability = calculateCollisionProbability(cpaResult.cpa, cpaResult.tcpa,
                                                                qAbs(ownFuture.sog - targetFuture.sog));

                // Calculate collision point
                double relativeBearing = qAtan2(targetFuture.lat - ownFuture.lat,
                                              targetFuture.lon - ownFuture.lon) * RAD_TO_DEG;
                bestCollisionPoint = QPointF(targetFuture.lat, targetFuture.lon);
            }
        }
    }

    // Assess risk level
    RiskLevel level = determineRiskLevel(minCpa, minTcpa, qAbs(ownShip.sog - target.sog));

    if (level > NO_RISK) {
        result.riskLevel = level;
        result.collisionProbability = maxProbability;
        result.timeToCollision = minTcpa;
        result.minDistance = minCpa;
        result.collisionPoint = bestCollisionPoint;
        result.threatId = QString::number(static_cast<int>(target.lat * 1000000)); // Unique ID from coordinates
        result.threatType = "AIS_VESSEL";
        result.predictedCollisionTime = QDateTime::currentDateTime().addSecs(minTcpa * 60.0);
        result.threatSpeed = target.sog;
        result.threatCourse = target.cog;
        result.relativeSpeed = qAbs(ownShip.sog - target.sog);
        result.calculationTimeMs = timer.elapsed();
        result.calculationTimestamp = QDateTime::currentDateTime();
    }

    return result;
}

CollisionRiskCalculator::TargetMotionState CollisionRiskCalculator::predictTargetPosition(
    const TargetMotionState& current, double timeAheadSec) const
{
    if (!current.isValid()) {
        return TargetMotionState();
    }

    TargetMotionState predicted;

    if (config.enableCircularMotionPrediction && current.isTurning && current.rot > 0.1) {
        predicted = predictCircularMotion(current, timeAheadSec);
    } else {
        predicted = predictLinearMotion(current, timeAheadSec);
    }

    // Apply environmental compensation
    if (config.enableEnvironmentalCompensation) {
        applyEnvironmentalCompensation(predicted, environmentalConditions, timeAheadSec);
    }

    predicted.lastUpdate = QDateTime::currentDateTime().addSecs(timeAheadSec);

    return predicted;
}

CollisionRiskCalculator::TargetMotionState CollisionRiskCalculator::predictCircularMotion(
    const TargetMotionState& current, double timeAheadSec) const
{
    TargetMotionState predicted = current;

    if (qAbs(current.rot) < 0.1 || current.sog < 0.1) {
        return predictLinearMotion(current, timeAheadSec);
    }

    // Calculate turning radius from rate of turn
    // rot is in degrees per minute, convert to radians per second
    double rotRadSec = qAbs(current.rot) * DEG_TO_RAD / 60.0;
    double turningRadiusNm = (current.sog * 60.0) / (qAbs(current.rot) * 2 * M_PI);

    if (turningRadiusNm < 0.01) { // Too small radius, fall back to linear
        return predictLinearMotion(current, timeAheadSec);
    }

    // Calculate angle covered during timeAheadSec
    double angleCoveredRad = rotRadSec * timeAheadSec;
    double angleCoveredDeg = qRadiansToDegrees(angleCoveredRad);

    // New course after turning
    double newCourse = current.cog;
    if (current.rot > 0) { // Turning right
        newCourse += angleCoveredDeg;
    } else { // Turning left
        newCourse -= angleCoveredDeg;
    }

    // Normalize course to 0-360
    while (newCourse < 0) newCourse += 360;
    while (newCourse >= 360) newCourse -= 360;

    // Distance traveled
    double timeHours = timeAheadSec / 3600.0;
    double distanceNm = current.sog * timeHours;

    // Use SevenCs kernel for accurate position calculation
    double newLat, newLon;
    bool success = EcCalculateRhumblinePosition(
        EC_GEO_DATUM_WGS84,
        current.lat, current.lon,
        distanceNm,
        newCourse,
        &newLat, &newLon
    );

    if (success) {
        predicted.lat = newLat;
        predicted.lon = newLon;
        predicted.cog = newCourse;
        predicted.lastUpdate = QDateTime::currentDateTime();
    }

    return predicted;
}

CollisionRiskCalculator::TargetMotionState CollisionRiskCalculator::predictLinearMotion(
    const TargetMotionState& current, double timeAheadSec) const
{
    TargetMotionState predicted = current;

    if (current.sog < 0.1) {
        return predicted; // Stationary target
    }

    // Simple linear prediction using great circle navigation
    double timeHours = timeAheadSec / 3600.0;
    double distanceNm = current.sog * timeHours;

    // Use SevenCs kernel for accurate great circle calculation
    double newLat, newLon;
    bool success = EcCalculateRhumblinePosition(
        EC_GEO_DATUM_WGS84,
        current.lat, current.lon,
        distanceNm,
        current.cog,
        &newLat, &newLon
    );

    if (success) {
        predicted.lat = newLat;
        predicted.lon = newLon;
        predicted.lastUpdate = QDateTime::currentDateTime();
    }

    return predicted;
}

CollisionRiskCalculator::RiskLevel CollisionRiskCalculator::determineRiskLevel(
    double cpa, double tcpa, double relativeSpeed) const
{
    // Apply safety margin
    double adjustedCpa = cpa / config.safetyMargin;

    // Primary assessment based on distance
    if (adjustedCpa <= config.criticalDistance) {
        if (tcpa <= config.criticalTime) {
            return CRITICAL_RISK;
        } else if (tcpa <= config.highRiskTime) {
            return HIGH_RISK;
        } else {
            return MEDIUM_RISK;
        }
    } else if (adjustedCpa <= config.highRiskDistance) {
        if (tcpa <= config.highRiskTime) {
            return HIGH_RISK;
        } else if (tcpa <= config.mediumRiskTime) {
            return MEDIUM_RISK;
        } else {
            return LOW_RISK;
        }
    } else if (adjustedCpa <= config.mediumRiskDistance) {
        if (tcpa <= config.mediumRiskTime) {
            return MEDIUM_RISK;
        } else {
            return LOW_RISK;
        }
    } else if (adjustedCpa <= config.lowRiskDistance) {
        return LOW_RISK;
    }

    return NO_RISK;
}

double CollisionRiskCalculator::calculateCollisionProbability(double cpa, double tcpa, double relativeSpeed) const
{
    if (cpa <= 0.0 || tcpa <= 0.0) {
        return 1.0; // Certain collision
    }

    // Probability calculation based on distance and time
    double distanceFactor = qExp(-cpa / config.criticalDistance);
    double timeFactor = qExp(-tcpa / config.criticalTime);
    double speedFactor = qMin(relativeSpeed / 20.0, 1.0); // Normalize to 0-1

    double probability = distanceFactor * timeFactor * speedFactor;

    // Apply safety margin
    probability = qMin(probability * config.safetyMargin, 1.0);

    return qMax(0.0, qMin(1.0, probability));
}

void CollisionRiskCalculator::applyEnvironmentalCompensation(
    TargetMotionState& state, const EnvironmentalConditions& env, double timeSec) const
{
    if (!config.enableEnvironmentalCompensation || !env.considerCurrents) {
        return;
    }

    // Apply current drift
    if (env.currentSpeed > 0.1) {
        double currentDriftNm = env.currentSpeed * timeSec / 3600.0;

        double newLat, newLon;
        bool success = EcCalculateRhumblinePosition(
            EC_GEO_DATUM_WGS84,
            state.lat, state.lon,
            currentDriftNm,
            env.currentDirection,
            &newLat, &newLon
        );

        if (success) {
            state.lat = newLat;
            state.lon = newLon;
        }
    }
}

QVector<QString> CollisionRiskCalculator::filterRelevantTargets(const TargetMotionState& ownShip) const
{
    QVector<QString> relevant;

    for (auto it = targetStates.begin(); it != targetStates.end(); ++it) {
        const TargetMotionState& target = it.value();

        if (!target.isValid()) {
            continue;
        }

        // Skip old data
        if (target.dataAgeSeconds() > MAX_STALE_AGE_SECONDS) {
            continue;
        }

        // Calculate rough distance
        double distanceDeg = qSqrt(qPow(ownShip.lat - target.lat, 2) + qPow(ownShip.lon - target.lon, 2));
        double distanceNm = distanceDeg * 60.0; // Rough conversion

        if (distanceNm <= config.maxRelevantRange) {
            relevant.append(it.key());
        }
    }

    return relevant;
}

void CollisionRiskCalculator::updateTargetMotionState(const QString& mmsi, const AISTargetData& target)
{
    TargetMotionState& state = targetStates[mmsi];

    // Calculate trends if we have previous data
    if (state.isValid()) {
        double sogDelta = target.sog - state.sog;
        double cogDelta = target.cog - state.cog;

        // Normalize course difference
        while (cogDelta > 180) cogDelta -= 360;
        while (cogDelta < -180) cogDelta += 360;

        state.sogTrend = sogDelta;
        state.cogTrend = cogDelta;

        // Detect turning
        state.isTurning = qAbs(cogDelta) > 2.0; // 2 degree threshold

        // Estimate turning radius if turning
        if (state.isTurning && qAbs(target.rot) > 0.1) {
            state.turningRadius = (target.sog * 60.0) / (qAbs(target.rot) * 2 * M_PI);
        }
    }

    // Update current state
    state.lat = target.lat;
    state.lon = target.lon;
    state.cog = target.cog;
    state.sog = target.sog;
    state.rot = target.rot;
    state.lastUpdate = QDateTime::currentDateTime();
    state.updateCount++;
}

void CollisionRiskCalculator::cleanupStaleTargetData()
{
    QMutexLocker locker(&dataMutex);
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-MAX_STALE_AGE_SECONDS);

    for (auto it = targetStates.begin(); it != targetStates.end();) {
        if (it.value().lastUpdate < cutoff) {
            it = targetStates.erase(it);
        } else {
            ++it;
        }
    }

    QMutexLocker perfLocker(&performanceMutex);
    metrics.activeTargetsBeingTracked = targetStates.size();
}

void CollisionRiskCalculator::queueTargetAssessment(const QString& targetId)
{
    if (!assessmentQueue.contains(targetId)) {
        assessmentQueue.append(targetId);
    }
}

void CollisionRiskCalculator::processAssessmentQueue()
{
    if (isProcessingQueue || assessmentQueue.isEmpty()) {
        return;
    }

    isProcessingQueue = true;

    // Process up to max concurrent assessments
    int processCount = qMin(config.maxConcurrentAssessments - activeAssessments.loadAcquire(),
                           assessmentQueue.size());

    for (int i = 0; i < processCount; ++i) {
        if (!assessmentQueue.isEmpty()) {
            QString targetId = assessmentQueue.takeFirst();
            performImmediateAssessment(targetId);
        }
    }

    isProcessingQueue = false;
}

// Additional implementation methods...

void CollisionRiskCalculator::updateConfiguration(const RiskAssessmentConfig& newConfig)
{
    config = newConfig;
    realTimeUpdateTimer->setInterval(config.updateFrequencyMs);

    // Update cache settings
    predictionCache.setMaxCost(100);

    emit performanceAlert("Collision risk configuration updated");
}

QVector<CollisionRiskCalculator::CollisionRiskResult> CollisionRiskCalculator::getCurrentRisks() const
{
    QMutexLocker locker(&dataMutex);
    return currentRisks;
}

CollisionRiskCalculator::RiskLevel CollisionRiskCalculator::getHighestRiskLevel() const
{
    QMutexLocker locker(&dataMutex);
    return currentHighestRiskLevel;
}

int CollisionRiskCalculator::getRiskCount(RiskLevel level) const
{
    QMutexLocker locker(&dataMutex);
    int count = 0;
    for (const auto& risk : currentRisks) {
        if (risk.riskLevel == level) {
            count++;
        }
    }
    return count;
}

CollisionRiskCalculator::PerformanceMetrics CollisionRiskCalculator::getPerformanceMetrics() const
{
    QMutexLocker locker(&performanceMutex);
    PerformanceMetrics result = metrics;
    result.uptimeSeconds = performanceTimer.elapsed() / 1000;
    return result;
}

void CollisionRiskCalculator::resetPerformanceMetrics()
{
    QMutexLocker locker(&performanceMutex);
    metrics = PerformanceMetrics();
    performanceTimer.restart();
}

void CollisionRiskCalculator::forceAssessmentUpdate()
{
    performFullAssessment();
}

CollisionRiskCalculator::DetailLevel CollisionRiskCalculator::determineDetailLevel(
    double distance, double speed) const
{
    // High detail for close or fast-moving targets
    if (distance < 1.0 || speed > 15.0) {
        return DetailLevel::HIGH;
    }
    // Medium detail for moderate distance/speed
    else if (distance < 3.0 || speed > 8.0) {
        return DetailLevel::MEDIUM;
    }
    // Low detail for distant or slow targets
    else {
        return DetailLevel::LOW;
    }
}