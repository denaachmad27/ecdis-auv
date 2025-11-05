#include "cpatcpacalculator.h"
#include "ais.h"

// Constants
const double CPATCPACalculator::EARTH_RADIUS_NM = 3440.065; // Nautical miles
const double CPATCPACalculator::MIN_SOG_THRESHOLD = 0.1;    // 0.1 knots
const double CPATCPACalculator::MAX_TCPA_HOURS = 24.0;      // 24 hours

CPATCPACalculator::CPATCPACalculator(QObject *parent)
    : QObject(parent)
{
}

CPATCPAResult CPATCPACalculator::calculateCPATCPA(const VesselState& ownShip, const VesselState& target)
{
    CPATCPAResult result;
    result.calculatedAt = QDateTime::currentDateTime();

    // Validate positions only first (so we can still return range/bearing when motion invalid)
    if (!isValidForCalculation(ownShip) || !isValidForCalculation(target)) {
        return result;
    }

    // Always compute current range and bearing from positions
    result.currentRange = calculateDistance(ownShip.lat, ownShip.lon, target.lat, target.lon);
    result.relativeBearing = calculateBearing(ownShip.lat, ownShip.lon, target.lat, target.lon);

    // Validate motion inputs (SOG/COG). AIS 'not available' mapped to negative values.
    auto invalidCog = [](double cog){ return (cog < 0 || cog >= 360 || std::isnan(cog)); };
    auto invalidSog = [](double sog){ return (sog < 0 || sog > 100 || std::isnan(sog)); };
    if (invalidCog(ownShip.cog) || invalidCog(target.cog) || invalidSog(ownShip.sog) || invalidSog(target.sog)) {
        result.isValid = false;
        result.status = CPATCPAResult::InvalidMotionData;
        result.tcpa = -1;                // indicate N/A
        result.cpa = result.currentRange; // best we can say
        return result;
    }

    // Calculate CPA and TCPA
    double tcpa = 0;
    result.cpa = calculateCPA(ownShip, target, tcpa);
    result.tcpa = tcpa;

    // Check if TCPA is within reasonable limits and set status
    if (tcpa == -1) {
        result.isValid = false;
        result.status = CPATCPAResult::StationaryRelative;
    } else if (tcpa < 0) {
        result.isValid = false;
        result.status = CPATCPAResult::Diverging;
    } else if (tcpa > (MAX_TCPA_HOURS * 60)) {
        result.isValid = false;
        result.status = CPATCPAResult::OutOfRange;
    } else {
        result.isValid = true;
        result.status = CPATCPAResult::Valid;
    }

    return result;
}


double CPATCPACalculator::calculateCPA(const VesselState& ownShip, const VesselState& target, double& tcpa)
{
    // Convert positions to Cartesian coordinates (simplified flat earth approximation for short distances)
    double ownX = ownShip.lon * cos(degreesToRadians(ownShip.lat));
    double ownY = ownShip.lat;
    double targetX = target.lon * cos(degreesToRadians(target.lat));
    double targetY = target.lat;

    // Convert COG to velocity components (knots to degrees per minute)
    double ownVx = ownShip.sog * sin(degreesToRadians(ownShip.cog)) / 60.0 * cos(degreesToRadians(ownShip.lat));
    double ownVy = ownShip.sog * cos(degreesToRadians(ownShip.cog)) / 60.0;
    double targetVx = target.sog * sin(degreesToRadians(target.cog)) / 60.0 * cos(degreesToRadians(target.lat));
    double targetVy = target.sog * cos(degreesToRadians(target.cog)) / 60.0;

    // Relative position and velocity
    double deltaX = targetX - ownX;
    double deltaY = targetY - ownY;
    double deltaVx = targetVx - ownVx;
    double deltaVy = targetVy - ownVy;

    // If relative velocity is zero, vessels are not approaching/separating
    double relativeSpeed = sqrt(deltaVx * deltaVx + deltaVy * deltaVy);
    if (relativeSpeed < (MIN_SOG_THRESHOLD / 60.0)) {
        // Stationary relative: treat TCPA as N/A, CPA equals current separation
        tcpa = -1;
        return sqrt(deltaX * deltaX + deltaY * deltaY) * 60.0; // Current distance in NM
    }

    // Calculate TCPA using dot product
    double dotProduct = -(deltaX * deltaVx + deltaY * deltaVy);
    double relativeSpeedSquared = deltaVx * deltaVx + deltaVy * deltaVy;

    tcpa = dotProduct / relativeSpeedSquared; // Time in minutes

    // Calculate CPA
    double cpaX = deltaX + deltaVx * tcpa;
    double cpaY = deltaY + deltaVy * tcpa;
    double cpa = sqrt(cpaX * cpaX + cpaY * cpaY) * 60.0; // Convert to nautical miles

    return cpa;
}

double CPATCPACalculator::calculateDistance(double lat1, double lon1, double lat2, double lon2)
{
    double dLat = degreesToRadians(lat2 - lat1);
    double dLon = degreesToRadians(lon2 - lon1);

    double a = sin(dLat/2) * sin(dLat/2) +
               cos(degreesToRadians(lat1)) * cos(degreesToRadians(lat2)) *
                   sin(dLon/2) * sin(dLon/2);

    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return EARTH_RADIUS_NM * c;
}

double CPATCPACalculator::calculateBearing(double lat1, double lon1, double lat2, double lon2)
{
    double dLon = degreesToRadians(lon2 - lon1);
    double lat1Rad = degreesToRadians(lat1);
    double lat2Rad = degreesToRadians(lat2);

    double y = sin(dLon) * cos(lat2Rad);
    double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(dLon);

    double bearing = radiansToDegrees(atan2(y, x));
    return normalizeAngle(bearing);
}

QPointF CPATCPACalculator::predictPosition(double lat, double lon, double cog, double sog, double timeMinutes)
{
    double distanceNM = sog * (timeMinutes / 60.0);
    double distanceDegrees = distanceNM / 60.0;

    double newLat = lat + distanceDegrees * cos(degreesToRadians(cog));
    double newLon = lon + distanceDegrees * sin(degreesToRadians(cog)) / cos(degreesToRadians(lat));

    return QPointF(newLon, newLat);
}

bool CPATCPACalculator::isValidForCalculation(const VesselState& vessel)
{
    // Only validate positional coordinates here; motion validity handled in caller
    if (vessel.lat < -90 || vessel.lat > 90) return false;
    if (vessel.lon < -180 || vessel.lon > 180) return false;
    return true;
}

double CPATCPACalculator::normalizeAngle(double angle)
{
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return angle;
}

double CPATCPACalculator::degreesToRadians(double degrees)
{
    return degrees * M_PI / 180.0;
}

double CPATCPACalculator::radiansToDegrees(double radians)
{
    return radians * 180.0 / M_PI;
}
