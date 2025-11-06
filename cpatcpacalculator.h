#ifndef CPATCPACALCULATOR_H
#define CPATCPACALCULATOR_H

#include <QObject>
#include <QPointF>
#include <QDateTime>
#include <QDebug>
#include <cmath>

struct CPATCPAResult {
    double cpa;           // Closest Point of Approach in nautical miles
    double tcpa;          // Time to CPA in minutes
    double currentRange;  // Current distance in nautical miles
    double relativeBearing; // Relative bearing in degrees
    bool isValid;         // Whether calculation is valid
    QDateTime calculatedAt; // When calculation was performed

    enum MotionStatus {
        Valid,              // TCPA within range and positive
        InvalidMotionData,  // Missing/invalid SOG/COG
        StationaryRelative, // Relative speed ~ 0
        Diverging,          // TCPA negative (moving away)
        OutOfRange          // TCPA beyond max horizon
    } status;

    CPATCPAResult() : cpa(0), tcpa(0), currentRange(0), relativeBearing(0), isValid(false), status(InvalidMotionData) {}
};

struct VesselState {
    double lat;           // Latitude in degrees
    double lon;           // Longitude in degrees
    double cog;           // Course over ground in degrees
    double sog;           // Speed over ground in knots
    QDateTime timestamp;  // Time of position

    VesselState() : lat(0), lon(0), cog(0), sog(0) {}
};

class CPATCPACalculator : public QObject
{
    Q_OBJECT

public:
    explicit CPATCPACalculator(QObject *parent = nullptr);

    // Main calculation method
    CPATCPAResult calculateCPATCPA(const VesselState& ownShip, const VesselState& target);

    // Utility methods
    static double calculateDistance(double lat1, double lon1, double lat2, double lon2);
    static double calculateBearing(double lat1, double lon1, double lat2, double lon2);
    static double normalizeAngle(double angle);
    static double degreesToRadians(double degrees);
    static double radiansToDegrees(double radians);

    // Position prediction
    static QPointF predictPosition(double lat, double lon, double cog, double sog, double timeMinutes);

private:
    // Constants
    static const double EARTH_RADIUS_NM; // Earth radius in nautical miles
    static const double MIN_SOG_THRESHOLD; // Minimum SOG to consider for calculation
    static const double MAX_TCPA_HOURS; // Maximum TCPA to calculate (hours)

    // Helper methods
    double calculateCPA(const VesselState& ownShip, const VesselState& target, double& tcpa);
    bool isValidForCalculation(const VesselState& vessel);
};

#endif // CPATCPACALCULATOR_H
