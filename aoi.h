#ifndef AOI_H
#define AOI_H

#include <QString>
#include <QColor>
#include <QVector>
#include <QPointF>

enum class AOIType {
    AOI,          // Generic Area of Interest
    ROZ,          // Restricted Operations Zone
    MEZ,          // Maritime Exclusion Zone
    WEZ,          // Weapon Engagement Zone
    PatrolArea,   // Patrol/Operating Area
    SOA,          // Submarine Operating Area
    AOR,          // Area of Responsibility
    JOA           // Joint Operations Area
};

struct AOI {
    int id;
    QString name;
    AOIType type;
    QColor color;
    bool visible;
    bool showLabel; // per-AOI label (name+area) visibility
    QVector<QPointF> vertices; // lat,lon pairs as QPointF(lat, lon)

    AOI() : id(-1), type(AOIType::AOI), color(Qt::yellow), visible(true), showLabel(true) {}
};

inline QColor aoiDefaultColor(AOIType type) {
    switch (type) {
        case AOIType::AOI:        return QColor(255, 215, 0);   // Gold
        case AOIType::ROZ:        return QColor(220, 20, 60);   // Crimson
        case AOIType::MEZ:        return QColor(255, 69, 0);    // OrangeRed
        case AOIType::WEZ:        return QColor(255, 0, 0);     // Red
        case AOIType::PatrolArea: return QColor(30, 144, 255);  // DodgerBlue
        case AOIType::SOA:        return QColor(138, 43, 226);  // BlueViolet
        case AOIType::AOR:        return QColor(50, 205, 50);   // LimeGreen
        case AOIType::JOA:        return QColor(0, 191, 255);   // DeepSkyBlue
    }
    return QColor(255, 215, 0);
}

inline QString aoiTypeToString(AOIType type) {
    switch (type) {
        case AOIType::AOI:        return "AOI";
        case AOIType::ROZ:        return "ROZ";
        case AOIType::MEZ:        return "MEZ";
        case AOIType::WEZ:        return "WEZ";
        case AOIType::PatrolArea: return "Patrol Area";
        case AOIType::SOA:        return "SOA";
        case AOIType::AOR:        return "AOR";
        case AOIType::JOA:        return "JOA";
    }
    return "AOI";
}

inline AOIType aoiTypeFromString(const QString& s) {
    QString t = s.trimmed().toUpper();
    if (t == "AOI") return AOIType::AOI;
    if (t == "ROZ") return AOIType::ROZ;
    if (t == "MEZ") return AOIType::MEZ;
    if (t == "WEZ") return AOIType::WEZ;
    if (t == "PATROL AREA" || t == "PATROL") return AOIType::PatrolArea;
    if (t == "SOA") return AOIType::SOA;
    if (t == "AOR") return AOIType::AOR;
    if (t == "JOA") return AOIType::JOA;
    return AOIType::AOI;
}

#endif // AOI_H

