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
    auto invalidSog = [](double sog){ return (sog < 0 || sog > 102.2 || std::isnan(sog)); };
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
    // Work consistently in nautical miles (NM) and minutes
    // Position differences converted from degrees to NM using 1 deg = 60 NM and cos(latitude) for longitude
    const double latMeanRad = degreesToRadians((ownShip.lat + target.lat) * 0.5);
    double dLonDeg = target.lon - ownShip.lon;
    // Normalize delta longitude to [-180, 180] to avoid wrap issues
    dLonDeg = std::fmod(dLonDeg + 540.0, 360.0) - 180.0;
    const double dLatDeg = target.lat - ownShip.lat;

    const double deltaX_nm = dLonDeg * 60.0 * cos(latMeanRad); // Easting (NM)
    const double deltaY_nm = dLatDeg * 60.0;                    // Northing (NM)

    // Speeds: knots (NM/hour) -> NM/min
    const double ownSpeed_nm_min = ownShip.sog / 60.0;
    const double tgtSpeed_nm_min = target.sog / 60.0;

    const double ownCogRad = degreesToRadians(ownShip.cog);
    const double tgtCogRad = degreesToRadians(target.cog);

    // Velocity components in NM/min (x=east, y=north)
    const double ownVx = ownSpeed_nm_min * sin(ownCogRad);
    const double ownVy = ownSpeed_nm_min * cos(ownCogRad);
    const double tgtVx = tgtSpeed_nm_min * sin(tgtCogRad);
    const double tgtVy = tgtSpeed_nm_min * cos(tgtCogRad);

    // Relative position and velocity
    const double deltaVx = tgtVx - ownVx;
    const double deltaVy = tgtVy - ownVy;

    // If relative velocity is near zero, vessels are stationary relative to each other
    const double relativeSpeed = sqrt(deltaVx * deltaVx + deltaVy * deltaVy); // NM/min
    if (relativeSpeed < (MIN_SOG_THRESHOLD / 60.0)) {
        tcpa = -1; // N/A
        // Current separation in NM
        return sqrt(deltaX_nm * deltaX_nm + deltaY_nm * deltaY_nm);
    }

    // TCPA (minutes): minimize distance squared => tcpa = - (r·v) / |v|^2
    const double dotProduct = -(deltaX_nm * deltaVx + deltaY_nm * deltaVy);
    const double relSpeedSq = deltaVx * deltaVx + deltaVy * deltaVy;
    tcpa = dotProduct / relSpeedSq; // minutes

    // CPA distance at tcpa
    const double cpaX = deltaX_nm + deltaVx * tcpa;
    const double cpaY = deltaY_nm + deltaVy * tcpa;
    const double cpa = sqrt(cpaX * cpaX + cpaY * cpaY); // NM

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
