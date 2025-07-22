#ifndef GUARDZONE_H
#define GUARDZONE_H

#include <QColor>
#include <QVector>

enum ShipTypeFilter {
    SHIP_TYPE_ALL = 0,
    SHIP_TYPE_CARGO = 1,
    SHIP_TYPE_TANKER = 2,
    SHIP_TYPE_PASSENGER = 3,
    SHIP_TYPE_FISHING = 4,
    SHIP_TYPE_MILITARY = 5,
    SHIP_TYPE_PLEASURE = 6,
    SHIP_TYPE_OTHER = 7
};

enum AlertDirection {
    ALERT_BOTH = 0,        // Alert untuk in dan out
    ALERT_IN_ONLY = 1,     // Alert hanya untuk masuk
    ALERT_OUT_ONLY = 2     // Alert hanya untuk keluar
};

enum GuardZoneShape {
    GUARD_ZONE_CIRCLE,
    GUARD_ZONE_POLYGON,
    GUARD_ZONE_SECTOR
};

struct GuardZone {
    int id;
    QString name;
    GuardZoneShape shape;
    bool active;
    bool attachedToShip;
    QColor color;

    // Circle specific
    double centerLat;
    double centerLon;
    double radius;

    // Polygon specific
    QVector<double> latLons; // lat1, lon1, lat2, lon2, ...

    // Sector specific (radar/tapal kuda)
    double innerRadius;      // Inner radius (nautical miles)
    double outerRadius;      // Outer radius (nautical miles)
    double startAngle;       // Start angle in degrees (0 = North, clockwise)
    double endAngle;         // End angle in degrees (0 = North, clockwise)

    // Default constructor
    GuardZone() :
        id(-1),
        shape(GUARD_ZONE_CIRCLE),
        active(true),
        attachedToShip(false),
        color(Qt::red),
        centerLat(0.0),
        centerLon(0.0),
        radius(0.5),
        innerRadius(0.2),    // Default inner radius 0.2 NM
        outerRadius(0.5),    // Default outer radius 0.5 NM
        startAngle(0.0),     // Default start angle 0° (North)
        endAngle(90.0) {     // Default end angle 90° (East) - quarter sector
        shipTypeFilter = SHIP_TYPE_ALL;  // Default: semua jenis kapal
        alertDirection = ALERT_BOTH;     // Default: alert untuk in dan out
    }

    ShipTypeFilter shipTypeFilter;  // Filter jenis kapal
    AlertDirection alertDirection;  // Pengaturan alert in/out
};

#endif // GUARDZONE_H
