#ifndef GUARDZONE_H
#define GUARDZONE_H

#include <QColor>
#include <QVector>

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

    // Default constructor
    GuardZone() : id(-1), shape(GUARD_ZONE_CIRCLE), active(true),
        attachedToShip(false), color(Qt::red),
        centerLat(0.0), centerLon(0.0), radius(0.5) {}
};

#endif // GUARDZONE_H
