#ifndef AOI_H
#define AOI_H

#include <QString>
#include <QColor>
#include <QVector>
#include <QPointF>

enum class AOIType {
    AOI,          // Generic Area of Interest (kept for backward compatibility)
    ROZ,
    MEZ,
    WEZ,
    PatrolArea,
    SOA,
    AOR,
    JOA
};

// 5 basic color choices for AOI
enum class AOIColorChoice {
    Red,
    Blue,
    Green,
    Yellow,
    Orange
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

inline QColor aoiColorFromChoice(AOIColorChoice choice) {
    switch (choice) {
        case AOIColorChoice::Red:    return QColor(255, 0, 0);     // Red
        case AOIColorChoice::Blue:   return QColor(0, 120, 255);   // Blue
        case AOIColorChoice::Green:  return QColor(0, 200, 0);     // Green
        case AOIColorChoice::Yellow: return QColor(255, 220, 0);   // Yellow
        case AOIColorChoice::Orange: return QColor(255, 140, 0);   // Orange
    }
    return QColor(255, 220, 0); // Default Yellow
}

inline QString aoiColorChoiceToString(AOIColorChoice choice) {
    switch (choice) {
        case AOIColorChoice::Red:    return "Red";
        case AOIColorChoice::Blue:   return "Blue";
        case AOIColorChoice::Green:  return "Green";
        case AOIColorChoice::Yellow: return "Yellow";
        case AOIColorChoice::Orange: return "Orange";
    }
    return "Yellow";
}

inline AOIColorChoice aoiColorChoiceFromString(const QString& s) {
    QString t = s.trimmed().toLower();
    if (t == "red") return AOIColorChoice::Red;
    if (t == "blue") return AOIColorChoice::Blue;
    if (t == "green") return AOIColorChoice::Green;
    if (t == "yellow") return AOIColorChoice::Yellow;
    if (t == "orange") return AOIColorChoice::Orange;
    return AOIColorChoice::Yellow;
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

