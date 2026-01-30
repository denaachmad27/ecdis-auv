#ifndef GRIBVISUALISATION_H
#define GRIBVISUALISATION_H

#include <QObject>
#include <QPainter>
#include <QColor>
#include "gribdata.h"

// Forward declarations
class EcWidget;

/**
 * @brief Visualization class for drawing GRIB wave data
 *
 * Handles:
 * - Colored heatmap for wave heights
 * - Directional arrows for wave direction
 * - Color scale management
 */
class GribVisualisation : public QObject
{
    Q_OBJECT

public:
    explicit GribVisualisation(QObject *parent = nullptr);
    ~GribVisualisation();

    /**
     * @brief Draw the GRIB data on the painter
     * @param painter The QPainter to draw on
     * @param ecWidget The main widget for coordinate conversion
     * @param message The GRIB message (time step) to draw
     * @param viewportRect The visible viewport rectangle
     * @param showHeatmap Show the colored wave height heatmap
     * @param showArrows Show directional arrows
     * @param arrowDensity Arrow grid density (1-10)
     */
    void draw(QPainter& painter,
              EcWidget* ecWidget,
              const GribMessage& message,
              const QRect& viewportRect,
              bool showHeatmap = true,
              bool showArrows = true,
              int arrowDensity = 5);

    /**
     * @brief Get color for a given wave height
     * @param waveHeight Wave height in meters
     * @return QColor for the wave height
     */
    QColor getColorForWaveHeight(double waveHeight) const;

    /**
     * @brief Draw the color legend
     * @param painter The QPainter to draw on
     * @param rect The rectangle to draw the legend in
     */
    void drawLegend(QPainter& painter, const QRect& rect);

    /**
     * @brief Set custom color scale
     * @param colors Vector of colors for the scale
     */
    void setColorScale(const QVector<QColor>& colors);

    /**
     * @brief Set the maximum wave height for color scaling
     */
    void setMaxWaveHeight(double maxH) { m_maxWaveHeight = maxH; }

    /**
     * @brief Get the maximum wave height
     */
    double getMaxWaveHeight() const { return m_maxWaveHeight; }

    /**
     * @brief Set heatmap opacity (0-255)
     */
    void setHeatmapOpacity(int opacity) { m_heatmapOpacity = opacity; }

    /**
     * @brief Get heatmap opacity
     */
    int getHeatmapOpacity() const { return m_heatmapOpacity; }

private:
    /**
     * @brief Draw wave height as colored grid cells
     */
    void drawWaveHeightGrid(QPainter& painter,
                           EcWidget* ecWidget,
                           const GribMessage& message,
                           const QRect& viewportRect);

    /**
     * @brief Draw directional arrows
     */
    void drawDirectionArrows(QPainter& painter,
                            EcWidget* ecWidget,
                            const GribMessage& message,
                            const QRect& viewportRect,
                            int density);

    /**
     * @brief Convert lat/lon to screen coordinates
     * Returns QPointF(x, y) or invalid point if outside viewport
     */
    QPointF latLonToScreen(double lat, double lon,
                          EcWidget* ecWidget,
                          const QRect& viewportRect) const;

    /**
     * @brief Create arrow polygon for given direction
     */
    QPolygonF createArrow(double x, double y, double size, double directionDegrees) const;

    /**
     * @brief Initialize default color scale
     */
    void initializeColorScale();

private:
    QVector<QColor> m_colorScale;
    QVector<double> m_colorBreakpoints;  // Wave height values for color transitions
    double m_maxWaveHeight;
    int m_heatmapOpacity;
    int m_arrowSize;
};

#endif // GRIBVISUALISATION_H
