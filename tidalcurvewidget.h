#ifndef TIDALCURVEWIDGET_H
#define TIDALCURVEWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QDateTime>
#include <QDebug>
#include <QPainterPath>
#include "tidemanager.h"

class TidalCurveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TidalCurveWidget(QWidget *parent = nullptr);
    ~TidalCurveWidget();

    void setTideManager(TideManager* manager);
    void setStation(const TideStation& station);
    void setPredictions(const QList<TidePrediction>& predictions);
    void setTimeRange(int hours = 24); // Hours to display (default: 24)
    void refresh();

    // Styling options
    void setGridColor(const QColor& color) { m_gridColor = color; update(); }
    void setCurveColor(const QColor& color) { m_curveColor = color; update(); }
    void setHighTideColor(const QColor& color) { m_highTideColor = color; update(); }
    void setLowTideColor(const QColor& color) { m_lowTideColor = color; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawGrid(QPainter& painter);
    void drawAxes(QPainter& painter);
    void drawCurve(QPainter& painter);
    void drawTidePoints(QPainter& painter);
    void drawInfo(QPainter& painter);
    void calculateBounds();
    QPointF mapToWidget(double timeHours, double height) const;
    double mapFromWidgetX(int x) const;
    double mapFromWidgetY(int y) const;

    TideManager* m_tideManager;
    TideStation m_currentStation;
    QList<TidePrediction> m_predictions;
    QDateTime m_startTime;
    QDateTime m_endTime;
    int m_timeRangeHours;

    // Calculated bounds
    double m_minHeight;
    double m_maxHeight;
    double m_minTime;
    double m_maxTime;

    // Margins and layout
    int m_leftMargin;
    int m_rightMargin;
    int m_topMargin;
    int m_bottomMargin;

    // Colors
    QColor m_backgroundColor;
    QColor m_gridColor;
    QColor m_textColor;
    QColor m_curveColor;
    QColor m_highTideColor;
    QColor m_lowTideColor;

    bool m_dataValid;
};

#endif // TIDALCURVEWIDGET_H