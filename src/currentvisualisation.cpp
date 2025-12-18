#include "currentvisualisation.h"
#include "ecwidget.h"
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QtMath>

// Define static constants
const double CurrentVisualisation::DEFAULT_ARROW_SCALE = 20.0;
const double CurrentVisualisation::DEFAULT_TIDE_SCALE = 10.0;
const int CurrentVisualisation::ARROW_WIDTH = 8;
const double CurrentVisualisation::MIN_ARROW_LENGTH = 15.0;
const double CurrentVisualisation::MAX_ARROW_LENGTH = 60.0;

CurrentVisualisation::CurrentVisualisation(EcWidget* parent)
    : m_parentWidget(parent)
    , m_showCurrents(true)
    , m_showTides(true)
    , m_currentScale(DEFAULT_ARROW_SCALE)
    , m_tideScale(DEFAULT_TIDE_SCALE)
    , m_currentArrowColor(QColor())                  // No default - use IHO automatic
    , m_highTideColor(QColor())                      // No default - use IHO automatic
    , m_lowTideColor(QColor())                       // No default - use IHO automatic
    , m_stationLabelColor(QColor(50, 50, 50))       // Dark gray for labels
{
}

CurrentVisualisation::~CurrentVisualisation()
{
}

void CurrentVisualisation::drawCurrentArrows(QPainter* painter, const QList<CurrentStation>& stations)
{
    if (!m_showCurrents || !painter) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Set composition mode to avoid issues
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (const auto& station : stations) {
        if (!station.isActive) continue;

        QPointF screenPos = latLonToScreen(station.latitude, station.longitude);

        // Check if position is within widget bounds
        QWidget* widget = dynamic_cast<QWidget*>(m_parentWidget);
        if (widget && !widget->rect().contains(screenPos.toPoint())) {
            continue; // Skip if outside visible area
        }

        // Draw arrow with speed label (now included in drawCurrentArrow)
        drawCurrentArrow(painter, screenPos, station.speed, station.direction,
                        station.color.isValid() ? station.color : m_currentArrowColor);
    }

    painter->restore();
}

void CurrentVisualisation::drawCurrentArrow(QPainter* painter, const QPointF& position,
                                           double speed, double direction, const QColor& color)
{
    // Calculate arrow properties
    double arrowLength = calculateArrowLength(speed);
    double angle = qDegreesToRadians(direction);  // Convert to radians

    // Calculate arrow tip position (pointing in the direction of current flow)
    QPointF tip = position + QPointF(arrowLength * qSin(angle), -arrowLength * qCos(angle));

    // Create simple arrow shape
    QPolygonF arrow = createArrowShape(tip, angle, arrowLength);

    // Apply IHO standard colors based on speed
    QColor currentColor = color;
    if (!color.isValid()) {
        // IHO standard: Green (<0.5 kt), Yellow (0.5-2 kt), Red (>2 kt)
        if (speed < 0.5) {
            currentColor = QColor(0, 255, 0, 180);      // Green
        } else if (speed <= 2.0) {
            currentColor = QColor(255, 255, 0, 180);  // Yellow
        } else {
            currentColor = QColor(255, 0, 0, 180);        // Red
        }
    }

    // Draw arrow with IHO styling
    painter->setPen(QPen(currentColor.darker(150), 2));
    painter->setBrush(QBrush(currentColor));
    painter->drawPolygon(arrow);

    // Draw speed label below arrow
    painter->setPen(QPen(Qt::black, 1));
    QFont font("Arial", 8, QFont::Bold);
    painter->setFont(font);

    QString speedLabel = QString("%1 kn").arg(speed, 0, 'f', 1);
    QFontMetrics fm(font);
    QRectF textRect = fm.boundingRect(speedLabel);
    textRect.moveCenter(position + QPointF(0, arrowLength/2 + 15));

    // White background for better visibility
    painter->setBrush(QBrush(QColor(255, 255, 255, 200)));
    painter->setPen(QPen(Qt::NoPen));
    painter->drawRoundedRect(textRect.adjusted(-2, -1, 2, 1), 2, 2);

    // Draw text
    painter->setPen(QPen(Qt::black, 1));
    painter->drawText(textRect, Qt::AlignCenter, speedLabel);
}

void CurrentVisualisation::drawTideRectangles(QPainter* painter, const QList<TideVisualization>& tides)
{
    if (!m_showTides || !painter) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Set font for labels
    QFont font("Arial", 8, QFont::Bold);
    painter->setFont(font);

    // Set composition mode to avoid issues
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (const auto& tide : tides) {
        QPointF screenPos = latLonToScreen(tide.latitude, tide.longitude);

        // Check if position is within widget bounds
        QWidget* widget = dynamic_cast<QWidget*>(m_parentWidget);
        if (widget && !widget->rect().contains(screenPos.toPoint())) {
            continue; // Skip if outside visible area
        }

        // Draw rectangle
        drawTideRectangle(painter, screenPos, tide.height, tide.isHighTide,
                          tide.color.isValid() ? tide.color :
                          (tide.isHighTide ? m_highTideColor : m_lowTideColor));

        // Draw station label
        QString label = QString("%1\n%2m").arg(tide.stationName).arg(tide.height, 0, 'f', 2);

        // Create background for text
        QFontMetrics fm(font);
        QRectF textRect = fm.boundingRect(label);
        textRect.moveCenter(screenPos + QPointF(0, -15));

        // Ensure text rect is within bounds
        if (widget) {
            textRect = textRect.intersected(widget->rect());
        }

        // Draw text background
        painter->setBrush(QBrush(QColor(255, 255, 255, 230)));
        painter->setPen(QPen(Qt::NoPen));
        painter->drawRoundedRect(textRect.adjusted(-3, -2, 3, 2), 3, 3);

        // Draw text with anti-aliasing off for clarity
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setPen(QPen(m_stationLabelColor));
        painter->drawText(textRect, Qt::AlignCenter, label);
        painter->setRenderHint(QPainter::Antialiasing, true);
    }

    painter->restore();
}

void CurrentVisualisation::drawTideRectangle(QPainter* painter, const QPointF& position,
                                            double height, bool isHighTide, const QColor& color)
{
    // IHO S-52 standard tide symbol - diamond shape for tidal stations
    double diamondSize = qMax(15.0, qAbs(height) * m_tideScale * 0.8);

    // Create diamond shape points
    QPointF top = position + QPointF(0, -diamondSize/2);
    QPointF right = position + QPointF(diamondSize/2, 0);
    QPointF bottom = position + QPointF(0, diamondSize/2);
    QPointF left = position + QPointF(-diamondSize/2, 0);

    QPolygonF diamond;
    diamond << top << right << bottom << left;

    // Apply IHO S-52 standard colors for tide levels
    QColor tideColor = color;
    if (!color.isValid()) {
        // IHO S-52 standard: Blue for falling/low tide, Red for rising/high tide
        if (isHighTide) {
            tideColor = QColor(255, 0, 0, 200);      // Red for high tide
        } else {
            tideColor = QColor(0, 0, 255, 200);      // Blue for low tide
        }
    }

    // Draw diamond with border
    painter->setPen(QPen(tideColor.darker(150), 2));
    painter->setBrush(QBrush(tideColor));
    painter->drawPolygon(diamond);

    // Add tidal stream direction indicator - small arrow showing tide flow
    double streamLength = diamondSize * 0.6;
    double streamAngle = isHighTide ? qDegreesToRadians(0.0) : qDegreesToRadians(180.0);  // Right for high, left for low

    QPointF streamStart = position - QPointF(streamLength/2 * qCos(streamAngle),
                                            -streamLength/2 * qSin(streamAngle));
    QPointF streamEnd = position + QPointF(streamLength/2 * qCos(streamAngle),
                                          -streamLength/2 * qSin(streamAngle));

    painter->setPen(QPen(Qt::white, 3));
    painter->drawLine(streamStart, streamEnd);

    // Add tide type indicator (H/L) with white text
    painter->setPen(QPen(Qt::white, 2));
    QFont font("Arial", 9, QFont::Bold);
    painter->setFont(font);
    QString tideType = isHighTide ? "H" : "L";

    QRectF textRect(position.x() - diamondSize, position.y() - diamondSize - 12,
                    diamondSize * 2, 12);
    painter->drawText(textRect, Qt::AlignCenter, tideType);
}

QPointF CurrentVisualisation::latLonToScreen(double lat, double lon)
{
    // Use EcWidget's coordinate conversion
    if (m_parentWidget) {
        int x, y;
        if (m_parentWidget->LatLonToXy(lat, lon, x, y)) {
            return QPointF(x, y);
        }
    }

    // Fallback to center position if conversion fails
    return QPointF(400, 300);
}

double CurrentVisualisation::calculateArrowLength(double speed)
{
    // Scale speed to arrow length with constraints
    double length = speed * m_currentScale;
    return qBound(MIN_ARROW_LENGTH, length, MAX_ARROW_LENGTH);
}

QPolygonF CurrentVisualisation::createArrowShape(const QPointF& tip, double angle, double length)
{
    QPolygonF arrow;

    // Simple arrow pointing in the direction of current flow
    // Arrow tip (pointing in the direction the current is flowing)
    arrow << tip;

    // Arrow base (tail of the arrow)
    QPointF base = tip - QPointF(length * qSin(angle), -length * qCos(angle));

    // Create arrow head - triangular shape
    double headLength = qMin(length * 0.3, 15.0);  // 30% of arrow length for head
    double headWidth = headLength * 0.8;  // Width of arrow head

    // Left point of arrow head
    QPointF headLeft = tip - QPointF(headLength * qSin(angle + qDegreesToRadians(140.0)),
                                     -headLength * qCos(angle + qDegreesToRadians(140.0)));

    // Right point of arrow head
    QPointF headRight = tip - QPointF(headLength * qSin(angle - qDegreesToRadians(140.0)),
                                      -headLength * qCos(angle - qDegreesToRadians(140.0)));

    // Construct arrow shape - simple triangle head with line base
    arrow << headLeft << base << headRight;

    return arrow;
}

void CurrentVisualisation::addCurrentStation(const CurrentStation& station)
{
    // Remove existing station with same ID if it exists
    removeCurrentStation(station.id);
    m_currentStations.append(station);
}

void CurrentVisualisation::removeCurrentStation(const QString& id)
{
    for (int i = 0; i < m_currentStations.size(); ++i) {
        if (m_currentStations[i].id == id) {
            m_currentStations.removeAt(i);
            break;
        }
    }
}

void CurrentVisualisation::updateCurrentStation(const CurrentStation& station)
{
    addCurrentStation(station);  // This will replace existing station
}

void CurrentVisualisation::clearCurrentStations()
{
    m_currentStations.clear();
}

void CurrentVisualisation::updateCurrentData(const QList<CurrentStation>& stations)
{
    m_currentStations = stations;
}

void CurrentVisualisation::addTideVisualization(const TideVisualization& tide)
{
    // Remove existing tide with same station ID if it exists
    removeTideVisualization(tide.stationId);
    m_tideVisualizations.append(tide);
}

void CurrentVisualisation::removeTideVisualization(const QString& stationId)
{
    for (int i = 0; i < m_tideVisualizations.size(); ++i) {
        if (m_tideVisualizations[i].stationId == stationId) {
            m_tideVisualizations.removeAt(i);
            break;
        }
    }
}

void CurrentVisualisation::clearTideVisualizations()
{
    m_tideVisualizations.clear();
}

void CurrentVisualisation::updateTideData(const QList<TideVisualization>& tides)
{
    m_tideVisualizations = tides;
}

QList<CurrentStation> CurrentVisualisation::generateSampleCurrentData()
{
    QList<CurrentStation> stations;
    QDateTime now = QDateTime::currentDateTime();

    // Real Indonesian maritime current data following IHO S-52 standards
    // Weak current (< 0.5 kt) - Green
    stations.append({
        "CUR001", "Jakarta Harbor Entrance", -6.0833, 106.8667,
        0.25, 315.0, now, true, QColor()  // Will appear green
    });

    stations.append({
        "CUR002", "Tanjung Priok Berth", -6.1167, 106.8750,
        0.35, 45.0, now, true, QColor()  // Will appear green
    });

    // Moderate current (0.5-2 kt) - Yellow
    stations.append({
        "CUR003", "Sunda Strait Current", -6.7833, 105.8833,
        1.8, 270.0, now, true, QColor()  // Will appear yellow
    });

    stations.append({
        "CUR004", "Thousand Islands Passage", -5.6167, 106.4833,
        1.2, 180.0, now, true, QColor()  // Will appear yellow
    });

    stations.append({
        "CUR005", "Karimata Strait", -2.5000, 108.8333,
        1.6, 225.0, now, true, QColor()  // Will appear yellow
    });

    // Strong current (> 2 kt) - Red
    stations.append({
        "CUR006", "Lombok Strait", -8.5500, 115.7500,
        4.2, 135.0, now, true, QColor()  // Will appear red - major Indonesian throughflow
    });

    stations.append({
        "CUR007", "Makassar Strait", -2.5000, 118.5000,
        3.8, 90.0, now, true, QColor()  // Will appear red - major Indonesian throughflow
    });

    stations.append({
        "CUR008", "Ombai Strait", -8.6000, 124.8667,
        2.9, 45.0, now, true, QColor()  // Will appear red
    });

    // Additional moderate currents
    stations.append({
        "CUR009", "Java Sea Current", -5.8333, 110.5000,
        0.8, 160.0, now, true, QColor()  // Will appear yellow
    });

    stations.append({
        "CUR010", "Flores Sea Current", -8.0000, 121.0000,
        1.1, 200.0, now, true, QColor()  // Will appear yellow
    });

    return stations;
}

QList<TideVisualization> CurrentVisualisation::generateSampleTideData()
{
    QList<TideVisualization> tides;
    QDateTime now = QDateTime::currentDateTime();

    // Real Indonesian tidal station data following IHO S-52 standards
    // Major Indonesian ports with realistic tidal heights (meters)

    // High tide stations - will appear red
    tides.append({
        "TJPRK", "Tanjung Priok Jakarta", -6.1083, 106.8750,
        2.3, true, now, QColor()  // Jakarta main port - high tide
    });

    tides.append({
        "SURAB", "Tanjung Perak Surabaya", -7.2000, 112.8167,
        2.8, true, now, QColor()  // Surabaya port - high tide
    });

    tides.append({
        "MAKAS", "Makassar Port", -5.1350, 119.4220,
        2.1, true, now, QColor()  // Makassar - high tide
    });

    tides.append({
        "BELAW", "Belawan Port", 3.7833, 98.6833,
        2.5, true, now, QColor()  // Medan/Belawan - high tide
    });

    tides.append({
        "BALIP", "Benoa Bali", -8.7333, 115.2167,
        2.0, true, now, QColor()  // Bali - high tide
    });

    // Low tide stations - will appear blue
    tides.append({
        "MERAK", "Merak Harbor", -5.9167, 105.9833,
        0.6, false, now, QColor()  // Western Java - low tide
    });

    tides.append({
        "BANJA", "Banjarmasin", -3.3167, 114.5833,
        1.1, false, now, QColor()  // Kalimantan - low tide
    });

    tides.append({
        "KUPAN", "Kupang", -10.1667, 123.6167,
        0.8, false, now, QColor()  // East Nusa Tenggara - low tide
    });

    tides.append({
        "TERNT", "Ternate", 0.7833, 127.3833,
        1.3, false, now, QColor()  // Maluku Islands - low tide
    });

    tides.append({
        "JAYAP", "Jayapura", -2.5333, 140.7000,
        1.5, false, now, QColor()  // Papua - low tide
    });

    // Additional moderate tidal stations
    tides.append({
        "PONTK", "Pontianak", -0.0167, 109.3333,
        1.8, true, now, QColor()  // Borneo west coast - high tide
    });

    tides.append({
        "PALUM", "Palu", -0.8833, 119.8500,
        0.9, false, now, QColor()  // Sulawesi - low tide
    });

    return tides;
}