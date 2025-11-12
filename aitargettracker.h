#ifndef AITARGETTRACKER_H
#define AITARGETTRACKER_H

#include <QObject>
#include <QPainter>
#include <QPointF>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QTimer>

class EcWidget; // fwd

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

/**
 * @brief AIS Target Tracker untuk Missile Targeting System
 *
 * Mirip EBL tetapi mengikuti pergerakan dinamis antara ownship dan AIS target.
 * Menghitung jarak real-time antara kedua kapal yang bergerak.
 */
class AITargetTracker : public QObject {
    Q_OBJECT
public:
    explicit AITargetTracker(QObject* parent = nullptr);

    // Status tracking
    bool trackingEnabled = false;      // Apakah sedang tracking target
    bool showTargetLine = true;        // Tampilkan garis target
    bool showDistanceInfo = true;      // Tampilkan informasi jarak
    bool showPredictionLine = false;   // Tampilkan garis prediksi (untuk missile)

    // Target data
    QString targetMMSI;               // MMSI dari target AIS
    QString targetName;               // Nama kapal target
    bool hasValidTarget = false;      // Apakah target valid

    // Tracking data
    double currentDistanceNM = 0.0;    // Jarak current dalam NM
    double currentBearingDeg = 0.0;   // Bearing dari ownship ke target
    double relativeSpeed = 0.0;       // Relative speed (knots)
    double timeToIntercept = 0.0;     // Estimasi waktu intercept (menit)

    // Missile targeting parameters
    double missileSpeed = 200.0;      // Missile speed (knots)
    double leadAngle = 0.0;           // Lead angle untuk moving target
    double interceptDistance = 0.0;   // Jarak intercept point

    // Prediction
    QPointF predictedPosition;        // Prediksi posisi target
    QPointF interceptPoint;           // Titik intercept untuk missile
    bool hasInterceptSolution = false; // Apakah ada solusi intercept

    // History untuk tracking quality
    struct PositionSnapshot {
        QPointF position;
        QDateTime timestamp;
        double course;
        double speed;
    };
    QVector<PositionSnapshot> targetHistory;
    QDateTime lastUpdate;
    static const int MAX_HISTORY_SIZE = 10;

    // Control methods
    void setTarget(const QString& mmsi, const QString& name = "");
    void clearTarget();
    void updateTargetPosition(double lat, double lon, double course, double speed);
    void updateOwnShipPosition(double lat, double lon, double course, double speed);

    // Calculation methods
    void calculateTrackingData();
    void calculateInterceptSolution();
    void predictTargetPosition(double timeAheadSec);

    // Visualization
    void draw(EcWidget* w, QPainter& p);

    // Format methods
    QString formatDistance(double nm) const;
    QString formatSpeed(double knots) const;
    QString formatTime(double minutes) const;
    QString getTargetInfo() const;

    // Access methods
    QPointF getTargetPosition() const { return targetPos; }

private slots:
    void onTrackingUpdate();

private:
    // Internal data
    QPointF ownshipPos;
    QPointF targetPos;
    double ownshipCourse = 0.0;
    double ownshipSpeed = 0.0;
    double targetCourse = 0.0;
    double targetSpeed = 0.0;

    // Update timer
    QTimer* updateTimer;

    // Internal methods
    void addToHistory(double lat, double lon, double course, double speed);
    void calculateLeadAngle();
    bool isValidTargetData() const;
};

#endif // AITARGETTRACKER_H