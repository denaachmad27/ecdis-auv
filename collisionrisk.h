#ifndef COLLISIONRISK_H
#define COLLISIONRISK_H

// Include collision risk dependencies before Qt headers to avoid conflicts
#include "aistargetpanel.h"
#include "cpatcpacalculator.h"
#include "guardzone.h"

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QRunnable>
#include <QCache>
#include <QHash>
#include <QMutex>
#include <QAtomicInt>
#include <cmath>
#include <limits>

// Forward declarations
class EcWidget;
class AISTargetData;

/**
 * @brief Real-time Collision Risk Calculator for AUV ECDIS System
 *
 * This class provides high-performance, real-time collision risk assessment
 * with parallel processing and accurate motion prediction algorithms.
 *
 * Key Features:
 * - < 200ms response time for critical risks
 * - Parallel processing for multiple targets
 * - Circular motion prediction for turning vessels
 * - Environmental compensation (currents, etc.)
 * - Real-time AIS data integration
 * - Spatial indexing for performance optimization
 */
class CollisionRiskCalculator : public QObject
{
    // Q_OBJECT - temporarily commented out to avoid linking issues

public:
    /**
     * @brief Risk level classification
     */
    enum RiskLevel {
        NO_RISK = 0,        // Safe - Green
        LOW_RISK = 1,       // Caution - Yellow
        MEDIUM_RISK = 2,    // Warning - Orange
        HIGH_RISK = 3,      // Danger - Red
        CRITICAL_RISK = 4   // Immediate Action - Red Flashing
    };

    /**
     * @brief Processing detail level for performance optimization
     */
    enum class DetailLevel { LOW, MEDIUM, HIGH };

    /**
     * @brief Collision risk assessment result
     */
    struct CollisionRiskResult {
        RiskLevel riskLevel;
        double collisionProbability;    // 0.0 - 1.0
        double timeToCollision;         // minutes (TCPA)
        double minDistance;             // nautical miles (CPA)
        QPointF collisionPoint;         // Predicted collision coordinates (lat, lon)
        QString threatId;               // MMSI for AIS, zone ID for guard zones
        QString threatType;             // "AIS_VESSEL", "GUARD_ZONE", "CHART_OBSTACLE"
        QDateTime predictedCollisionTime;
        double threatSpeed;             // knots
        double threatCourse;            // degrees
        double relativeSpeed;           // knots
        double relativeBearing;         // degrees

        // Processing metrics
        qint64 calculationTimeMs;       // How long this calculation took
        QDateTime calculationTimestamp; // When this risk was calculated

        bool isValid() const {
            return !threatId.isEmpty() &&
                   riskLevel > NO_RISK &&
                   !qIsNaN(timeToCollision) &&
                   timeToCollision >= 0;
        }

        bool isStale(const QDateTime& now, double maxAgeSec = 2.0) const {
            return calculationTimestamp.secsTo(now) > maxAgeSec;
        }
    };

    /**
     * @brief Environmental conditions affecting collision prediction
     */
    struct EnvironmentalConditions {
        bool considerCurrents = true;
        double currentSpeed = 0.0;          // knots
        double currentDirection = 0.0;      // degrees
        bool considerWinds = false;         // For surface AUV only
        double windSpeed = 0.0;             // knots
        double windDirection = 0.0;         // degrees
        double waterDensity = 1025.0;       // kg/m³
    };

    /**
     * @brief Configuration for risk assessment parameters
     */
    struct RiskAssessmentConfig {
        // Distance thresholds (nautical miles)
        double criticalDistance = 0.1;     // 185 meters
        double highRiskDistance = 0.25;    // 463 meters
        double mediumRiskDistance = 0.5;   // 926 meters
        double lowRiskDistance = 1.0;      // 1852 meters

        // Time thresholds (minutes)
        double criticalTime = 2.0;         // Immediate action
        double highRiskTime = 5.0;         // Prepare for maneuver
        double mediumRiskTime = 10.0;      // Monitor closely
        double lowRiskTime = 15.0;         // Routine observation

        // Performance parameters
        double maxRelevantRange = 10.0;    // NM - only assess targets within this range
        double immediateAssessmentRange = 2.0; // NM - immediate assessment for close targets
        int maxConcurrentAssessments = 8;  // Parallel processing limit
        int updateFrequencyMs = 100;       // 10Hz update rate
        int predictionIntervalSec = 5;     // Prediction path interval
        double maxPredictionTime = 10.0;   // Maximum prediction time in minutes

        // Accuracy parameters
        bool enableEnvironmentalCompensation = true;
        bool enableCircularMotionPrediction = true;
        bool enableAdvancedFiltering = true;
        double safetyMargin = 1.2;         // 20% safety margin

        // Visualization parameters
        bool showRiskSymbols = true;
        bool enablePulsingWarnings = true;
        bool fadeOldRisks = true;
        double riskFadeTimeSec = 30.0;     // How long to show old risks
    };

    /**
     * @brief Target motion state for accurate prediction
     */
    struct TargetMotionState {
        double lat, lon;
        double cog, sog;
        double rot;                // Rate of turn (deg/min)
        double sogTrend;           // Speed trend (+/-)
        double cogTrend;           // Course trend (+/-)
        double turningRadius;      // Estimated turning radius
        bool isTurning;            // Currently maneuvering
        QDateTime lastUpdate;
        int updateCount;           // Number of updates received

        bool isValid() const {
            return lat != 0.0 && lon != 0.0 &&
                   !qIsNaN(cog) && !qIsNaN(sog) &&
                   lastUpdate.isValid();
        }

        double dataAgeSeconds() const {
            return lastUpdate.secsTo(QDateTime::currentDateTime());
        }
    };

    explicit CollisionRiskCalculator(QObject* parent = nullptr);
    ~CollisionRiskCalculator();

    // Main real-time assessment methods
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    bool isRealTimeMonitoring() const;

    // Data update methods (real-time)
    void updateOwnShipData(double lat, double lon, double heading, double sog, double rot = 0.0);
    void updateAISTarget(const QString& mmsi, const AISTargetData& target);
    void updateGuardZones(const QVector<GuardZone>& zones);
    void updateEnvironmentalConditions(const EnvironmentalConditions& env);

    // Assessment methods
    void performImmediateAssessment(const QString& targetId);
    void performFullAssessment();
    void forceAssessmentUpdate();

    // Configuration methods
    void updateConfiguration(const RiskAssessmentConfig& newConfig);
    const RiskAssessmentConfig& getCurrentConfiguration() const { return config; }

    // Results access methods (thread-safe)
    QVector<CollisionRiskResult> getCurrentRisks() const;
    RiskLevel getHighestRiskLevel() const;
    int getRiskCount(RiskLevel level) const;

    // Performance monitoring
    struct PerformanceMetrics {
        double averageAssessmentTimeMs = 0.0;
        double maxAssessmentTimeMs = 0.0;
        int totalAssessmentsPerformed = 0;
        int activeTargetsBeingTracked = 0;
        qint64 uptimeSeconds = 0;
        double cpuUsagePercentage = 0.0;
    };

    PerformanceMetrics getPerformanceMetrics() const;
    void resetPerformanceMetrics();

public:
    // Callback functions (replacing signals/slots)
    std::function<void(const CollisionRiskResult&)> onCollisionRiskDetected;
    std::function<void(RiskLevel)> onRiskLevelChanged;
    std::function<void(const CollisionRiskResult&)> onHighRiskAlert;
    std::function<void(const CollisionRiskResult&)> onCriticalRiskAlert;
    std::function<void(const QString&, qint64)> onAssessmentCompleted;
    std::function<void(const QString&)> onPerformanceAlert;

private:
    // Real-time processing slots
    void onRealTimeUpdateTimer();
    void onTargetAssessmentComplete(const QString& targetId, const CollisionRiskResult& result);
    void onCleanupStaleData();

private:
    // Core assessment algorithms
    CollisionRiskResult assessAISCollisionRisk(const TargetMotionState& ownShip,
                                              const TargetMotionState& target);
    void assessGuardZoneCollision(const TargetMotionState& ownShip,
                                 const QVector<GuardZone>& zones,
                                 QVector<CollisionRiskResult>& risks);

    // Accurate prediction methods
    TargetMotionState predictTargetPosition(const TargetMotionState& current,
                                          double timeAheadSec) const;
    TargetMotionState predictCircularMotion(const TargetMotionState& current,
                                           double timeAheadSec) const;
    TargetMotionState predictLinearMotion(const TargetMotionState& current,
                                         double timeAheadSec) const;

    // Risk assessment logic
    RiskLevel determineRiskLevel(double cpa, double tcpa, double relativeSpeed) const;
    double calculateCollisionProbability(double cpa, double tcpa, double relativeSpeed) const;
    void applyEnvironmentalCompensation(TargetMotionState& state,
                                      const EnvironmentalConditions& env,
                                      double timeSec) const;

    // Performance optimization methods
    QVector<QString> filterRelevantTargets(const TargetMotionState& ownShip) const;
    DetailLevel determineDetailLevel(double distance, double speed) const;

    // Data management
    void updateTargetMotionState(const QString& mmsi, const AISTargetData& target);
    void cleanupStaleTargetData();
    void updateSpatialIndices();

    // Threading and performance
    void queueTargetAssessment(const QString& targetId);
    void processAssessmentQueue();

    // Visualization helpers
    CollisionRiskResult createRiskResult(const QString& threatId, const QString& threatType,
                                       RiskLevel level, double cpa, double tcpa,
                                       const QPointF& position) const;

    // Core data members
    RiskAssessmentConfig config;
    EnvironmentalConditions environmentalConditions;

    // Real-time data (thread-safe access)
    mutable QMutex dataMutex;
    TargetMotionState ownShipState;
    QHash<QString, TargetMotionState> targetStates;
    QVector<GuardZone> guardZones;
    QVector<CollisionRiskResult> currentRisks;
    RiskLevel currentHighestRiskLevel;

    // Real-time processing components
    QTimer* realTimeUpdateTimer;
    QTimer* cleanupTimer;
        QElapsedTimer performanceTimer;

    // Performance tracking
    mutable QMutex performanceMutex;
    PerformanceMetrics metrics;
    QAtomicInt activeAssessments;

    // Optimization
    QCache<QString, QVector<CollisionRiskResult>> predictionCache;
    QVector<QString> assessmentQueue;
    bool isProcessingQueue;

    // State management
    bool realTimeMonitoringActive;
    QDateTime lastFullAssessment;
    static const int MAX_STALE_AGE_SECONDS = 30;
};

#endif // COLLISIONRISK_H