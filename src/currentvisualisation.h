#ifndef CURRENTVISUALISATION_H
#define CURRENTVISUALISATION_H

#include <QPainter>
#include <QPointF>
#include <QDateTime>
#include <QColor>
#include <QString>
#include <QList>

class EcWidget;

// Structure for current station data
struct CurrentStation {
    QString id;
    QString name;
    double latitude;
    double longitude;
    double speed;        // Speed in knots
    double direction;    // Direction in degrees (0-360)
    QDateTime timestamp;
    bool isActive;
    QColor color;
};

// Structure for tide visualization
struct TideVisualization {
    QString stationId;
    QString stationName;
    double latitude;
    double longitude;
    double height;        // Tide height in meters
    bool isHighTide;      // True for high tide, false for low tide
    QDateTime timestamp;
    QColor color;
};

class CurrentVisualisation
{
public:
    CurrentVisualisation(EcWidget* parent);
    ~CurrentVisualisation();

    // Current visualization functions
    void drawCurrentArrows(QPainter* painter, const QList<CurrentStation>& stations);
    void drawCurrentArrow(QPainter* painter, const QPointF& position,
                          double speed, double direction, const QColor& color);

    // Tide visualization functions
    void drawTideRectangles(QPainter* painter, const QList<TideVisualization>& tides);
    void drawTideRectangle(QPainter* painter, const QPointF& position,
                           double height, bool isHighTide, const QColor& color);

    // Utility functions
    QPointF latLonToScreen(double lat, double lon);
    double calculateArrowLength(double speed);
    QPolygonF createArrowShape(const QPointF& tip, double angle, double length);

    // Settings
    void setShowCurrents(bool show) { m_showCurrents = show; }
    void setShowTides(bool show) { m_showTides = show; }
    void setCurrentScale(double scale) { m_currentScale = scale; }
    void setTideScale(double scale) { m_tideScale = scale; }

    bool getShowCurrents() const { return m_showCurrents; }
    bool getShowTides() const { return m_showTides; }

    // Data management
    void addCurrentStation(const CurrentStation& station);
    void removeCurrentStation(const QString& id);
    void updateCurrentStation(const CurrentStation& station);
    void clearCurrentStations();
    void updateCurrentData(const QList<CurrentStation>& stations);

    void addTideVisualization(const TideVisualization& tide);
    void removeTideVisualization(const QString& stationId);
    void clearTideVisualizations();
    void updateTideData(const QList<TideVisualization>& tides);

    // Sample data generation for testing
    QList<CurrentStation> generateSampleCurrentData();
    QList<TideVisualization> generateSampleTideData();

private:
    EcWidget* m_parentWidget;

    // Visualization settings
    bool m_showCurrents;
    bool m_showTides;
    double m_currentScale;    // Scale factor for arrow length
    double m_tideScale;       // Scale factor for rectangle height

    // Data storage
    QList<CurrentStation> m_currentStations;
    QList<TideVisualization> m_tideVisualizations;

    // Colors and styling
    QColor m_currentArrowColor;
    QColor m_highTideColor;
    QColor m_lowTideColor;
    QColor m_stationLabelColor;

    // Drawing parameters
    static const double DEFAULT_ARROW_SCALE;  // pixels per knot
    static const double DEFAULT_TIDE_SCALE;   // pixels per meter
    static const int ARROW_WIDTH;            // Arrow width in pixels
    static const double MIN_ARROW_LENGTH;    // Minimum arrow length
    static const double MAX_ARROW_LENGTH;    // Maximum arrow length
};

#endif // CURRENTVISUALISATION_H