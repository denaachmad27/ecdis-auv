#include "gribvisualisation.h"
#include "ecwidget.h"
#include <QPainterPath>
#include <QDebug>
#include <QtMath>

GribVisualisation::GribVisualisation(QObject *parent)
    : QObject(parent)
    , m_maxWaveHeight(5.0)
    , m_heatmapOpacity(180)  // Semi-transparent
    , m_arrowSize(20)
{
    initializeColorScale();
}

GribVisualisation::~GribVisualisation()
{
}

void GribVisualisation::initializeColorScale()
{
    // Wave height color breakpoints (meters) and corresponding colors
    // Blue (calm) -> Cyan -> Green -> Yellow -> Orange -> Red (rough) -> Purple (extreme)
    m_colorBreakpoints.clear();
    m_colorBreakpoints << 0.0 << 0.5 << 1.0 << 1.5 << 2.0 << 3.0 << 4.0 << 6.0;

    m_colorScale.clear();
    m_colorScale << QColor(0, 0, 255)        // Deep Blue: 0m
                 << QColor(0, 150, 255)      // Light Blue: 0.5m
                 << QColor(0, 255, 255)      // Cyan: 1.0m
                 << QColor(0, 255, 0)        // Green: 1.5m
                 << QColor(255, 255, 0)      // Yellow: 2.0m
                 << QColor(255, 165, 0)      // Orange: 3.0m
                 << QColor(255, 0, 0)        // Red: 4.0m
                 << QColor(128, 0, 128);     // Purple: 6m+
}

QColor GribVisualisation::getColorForWaveHeight(double waveHeight) const
{
    if (waveHeight < 0 || !qIsFinite(waveHeight)) {
        return QColor(0, 0, 0, 0);  // Transparent for invalid data
    }

    // Find the color range
    for (int i = 0; i < m_colorBreakpoints.size() - 1; ++i) {
        if (waveHeight >= m_colorBreakpoints[i] && waveHeight <= m_colorBreakpoints[i + 1]) {
            // Interpolate between colors
            double t = (waveHeight - m_colorBreakpoints[i]) /
                       (m_colorBreakpoints[i + 1] - m_colorBreakpoints[i]);

            QColor c1 = m_colorScale[i];
            QColor c2 = m_colorScale[i + 1];

            int r = static_cast<int>(c1.red() + t * (c2.red() - c1.red()));
            int g = static_cast<int>(c1.green() + t * (c2.green() - c1.green()));
            int b = static_cast<int>(c1.blue() + t * (c2.blue() - c1.blue()));
            int a = m_heatmapOpacity;

            return QColor(r, g, b, a);
        }
    }

    // Above maximum - return the highest color
    QColor maxColor = m_colorScale.last();
    maxColor.setAlpha(m_heatmapOpacity);
    return maxColor;
}

void GribVisualisation::draw(QPainter& painter,
                            EcWidget* ecWidget,
                            const GribMessage& message,
                            const QRect& viewportRect,
                            bool showHeatmap,
                            bool showArrows,
                            int arrowDensity)
{
    if (!ecWidget || message.dataPoints.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw heatmap first (background layer)
    if (showHeatmap) {
        drawWaveHeightGrid(painter, ecWidget, message, viewportRect);
    }

    // Draw arrows on top (foreground layer)
    if (showArrows) {
        drawDirectionArrows(painter, ecWidget, message, viewportRect, arrowDensity);
    }

    painter.restore();
}

void GribVisualisation::drawWaveHeightGrid(QPainter& painter,
                                           EcWidget* ecWidget,
                                           const GribMessage& message,
                                           const QRect& viewportRect)
{
    if (message.ni == 0 || message.nj == 0) {
        return;
    }

    // Calculate cell size in screen coordinates
    // Use first and second points to estimate grid cell size
    int x1, y1, x2, y2;
    if (!ecWidget->LatLonToXy(message.dataPoints[0].latitude,
                             message.dataPoints[0].longitude, x1, y1)) {
        return;
    }

    // Find a point in next row/column to estimate cell size
    int nextIdx = message.ni;  // Next row
    if (nextIdx < message.dataPoints.size()) {
        if (!ecWidget->LatLonToXy(message.dataPoints[nextIdx].latitude,
                                  message.dataPoints[nextIdx].longitude, x2, y2)) {
            return;
        }
    } else {
        x2 = x1;
        y2 = y1 + 20;  // Default cell height
    }

    int cellHeight = qAbs(y2 - y1);
    int cellWidth = static_cast<int>(cellHeight * 1.5);  // Aspect ratio for mid-latitudes

    // Clamp cell sizes for performance
    cellHeight = qBound(4, cellHeight, 50);
    cellWidth = qBound(6, cellWidth, 75);

    // Draw grid cells
    for (const auto& data : message.dataPoints) {
        if (!data.isValid || data.waveHeight < -900) {
            continue;
        }

        int x, y;
        if (!ecWidget->LatLonToXy(data.latitude, data.longitude, x, y)) {
            continue;
        }

        // Check if point is in viewport (with margin)
        QPoint topLeft(x - cellWidth / 2, y - cellHeight / 2);
        QRect cellRect(topLeft.x(), topLeft.y(), cellWidth, cellHeight);

        if (!viewportRect.intersects(cellRect)) {
            continue;
        }

        QColor color = getColorForWaveHeight(data.waveHeight);
        painter.fillRect(cellRect, color);
    }
}

void GribVisualisation::drawDirectionArrows(QPainter& painter,
                                            EcWidget* ecWidget,
                                            const GribMessage& message,
                                            const QRect& viewportRect,
                                            int density)
{
    // Clamp density
    density = qBound(1, density, 20);

    // Calculate grid step
    int iStep = qMax(1, message.ni / (message.ni / density));
    int jStep = qMax(1, message.nj / (message.nj / density));

    // Use actual density parameter
    iStep = density;
    jStep = density;

    QPen arrowPen(QColor(50, 50, 50, 200), 2);
    painter.setPen(arrowPen);
    painter.setBrush(QColor(50, 50, 50, 180));

    for (int j = 0; j < message.nj; j += jStep) {
        for (int i = 0; i < message.ni; i += iStep) {
            int idx = j * message.ni + i;
            if (idx >= message.dataPoints.size()) {
                continue;
            }

            const auto& data = message.dataPoints[idx];
            if (!data.isValid || data.waveDirection < -900) {
                continue;
            }

            int x, y;
            if (!ecWidget->LatLonToXy(data.latitude, data.longitude, x, y)) {
                continue;
            }

            // Check if point is in viewport
            if (!viewportRect.contains(x, y)) {
                continue;
            }

            // Scale arrow size by wave height
            double sizeScale = qBound(0.5, data.waveHeight / 2.0, 2.0);
            int arrowSize = static_cast<int>(m_arrowSize * sizeScale);

            // Draw arrow
            QPolygonF arrow = createArrow(x, y, arrowSize, data.waveDirection);
            painter.drawPolygon(arrow);
        }
    }
}

QPolygonF GribVisualisation::createArrow(double x, double y, double size, double directionDegrees) const
{
    // Convert meteorological direction (from North, clockwise) to mathematical angle
    // Met: 0° = North, 90° = East
    // Math: 0° = East, counter-clockwise
    double angleRad = qDegreesToRadians(90.0 - directionDegrees);

    QPolygonF arrow;
    double cosA = std::cos(angleRad);
    double sinA = std::sin(angleRad);

    // Arrow head (point)
    arrow << QPointF(x + size * cosA, y - size * sinA);

    // Arrow base left
    double baseWidth = size * 0.5;
    double baseOffset = size * 0.6;
    arrow << QPointF(x + baseOffset * cosA + baseWidth * sinA,
                     y - baseOffset * sinA + baseWidth * cosA);

    // Arrow base center (indent)
    double indent = size * 0.3;
    arrow << QPointF(x + indent * cosA, y - indent * sinA);

    // Arrow base right
    arrow << QPointF(x + baseOffset * cosA - baseWidth * sinA,
                     y - baseOffset * sinA - baseWidth * cosA);

    return arrow;
}

QPointF GribVisualisation::latLonToScreen(double lat, double lon,
                                          EcWidget* ecWidget,
                                          const QRect& viewportRect) const
{
    int x, y;
    if (ecWidget->LatLonToXy(lat, lon, x, y)) {
        return QPointF(x, y);
    }
    return QPointF();  // Invalid point
}

void GribVisualisation::drawLegend(QPainter& painter, const QRect& rect)
{
    painter.save();

    // Draw legend background
    painter.setPen(QPen(QColor(0, 0, 0, 150), 1));
    painter.setBrush(QColor(255, 255, 255, 200));

    const int legendWidth = 30;
    const int legendHeight = rect.height() - 20;
    const int legendX = rect.x() + 10;
    const int legendY = rect.y() + 10;

    QRect legendRect(legendX, legendY, legendWidth, legendHeight);
    painter.drawRoundedRect(legendRect.adjusted(-5, -5, 5, 5), 5, 5);

    // Draw color gradient
    for (int i = 0; i < legendHeight; ++i) {
        double t = 1.0 - (static_cast<double>(i) / legendHeight);  // Top to bottom
        double height = t * m_colorBreakpoints.last();
        QColor color = getColorForWaveHeight(height);
        color.setAlpha(255);  // Full opacity for legend
        painter.fillRect(legendX, legendY + i, legendWidth, 1, color);
    }

    // Draw scale labels
    painter.setPen(QColor(0, 0, 0));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (int i = 0; i < m_colorBreakpoints.size(); ++i) {
        double height = m_colorBreakpoints[i];
        double t = height / m_colorBreakpoints.last();
        int yPos = legendY + static_cast<int>((1.0 - t) * legendHeight);

        QString label = QString::number(height, 'f', 1) + "m";
        painter.drawText(legendX + legendWidth + 5, yPos + 4, label);
    }

    // Draw title
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(legendX, legendY - 10, "Wave Height");

    painter.restore();
}

void GribVisualisation::setColorScale(const QVector<QColor>& colors)
{
    if (colors.size() >= 2) {
        m_colorScale = colors;
        // Recalculate breakpoints based on number of colors
        m_colorBreakpoints.clear();
        double step = m_maxWaveHeight / (colors.size() - 1);
        for (int i = 0; i < colors.size(); ++i) {
            m_colorBreakpoints.append(i * step);
        }
    }
}
