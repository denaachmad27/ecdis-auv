#include "tidalcurvewidget.h"
#include <QPaintEvent>
#include <QResizeEvent>
#include <QtMath>
#include <QFontMetrics>

TidalCurveWidget::TidalCurveWidget(QWidget *parent)
    : QWidget(parent)
    , m_tideManager(nullptr)
    , m_timeRangeHours(24)
    , m_minHeight(-2.0)
    , m_maxHeight(4.0)
    , m_minTime(0.0)
    , m_maxTime(24.0)
    , m_leftMargin(60)
    , m_rightMargin(20)
    , m_topMargin(30)
    , m_bottomMargin(50)
    , m_backgroundColor(245, 245, 245)
    , m_gridColor(200, 200, 200)
    , m_textColor(50, 50, 50)
    , m_curveColor(0, 100, 200)
    , m_highTideColor(220, 50, 50)  // Red for high tide
    , m_lowTideColor(50, 150, 50)    // Green for low tide
    , m_dataValid(false)
{
    // Ensure widget is properly created
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    setMinimumSize(400, 250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background-color: white; border: 1px solid gray;");

    // Initialize with current time
    m_startTime = QDateTime::currentDateTime();
    m_endTime = m_startTime.addSecs(m_timeRangeHours * 3600);

    // Start with empty data - will be populated when tide manager is set
    m_dataValid = false;
    m_predictions.clear();

    qDebug() << "[TIDAL-CURVE] TidalCurveWidget created successfully";
}

TidalCurveWidget::~TidalCurveWidget()
{
    qDebug() << "[TIDAL-CURVE] TidalCurveWidget destroyed";
}

void TidalCurveWidget::setTideManager(TideManager* manager)
{
    m_tideManager = manager;
    refresh();
}

void TidalCurveWidget::setStation(const TideStation& station)
{
    m_currentStation = station;
    refresh();
}

void TidalCurveWidget::setPredictions(const QList<TidePrediction>& predictions)
{
    m_predictions = predictions;

    // Debug: Print prediction details
    qDebug() << "[TIDAL-CURVE] Set" << predictions.size() << "predictions";
    for (int i = 0; i < predictions.size(); ++i) {
        const TidePrediction& pred = predictions[i];
        qDebug() << "[TIDAL-CURVE] Prediction" << i << ":" << pred.dateTime.toString()
                 << "Height:" << pred.height << "Type:" << (pred.isHighTide ? "HIGH" : "LOW");
    }

    calculateBounds();
    m_dataValid = !m_predictions.isEmpty();
    update();

    qDebug() << "[TIDAL-CURVE] Data valid:" << m_dataValid;
    qDebug() << "[TIDAL-CURVE] Bounds - Height:" << m_minHeight << "to" << m_maxHeight;
    qDebug() << "[TIDAL-CURVE] Bounds - Time:" << m_minTime << "to" << m_maxTime;
}

void TidalCurveWidget::setTimeRange(int hours)
{
    m_timeRangeHours = hours;
    m_endTime = m_startTime.addSecs(m_timeRangeHours * 3600);
    refresh();
}

void TidalCurveWidget::refresh()
{
    if (!m_tideManager || m_currentStation.id.isEmpty()) {
        m_dataValid = false;
        update();
        return;
    }

    // Get fresh predictions for current station
    QList<TidePrediction> predictions = m_tideManager->getTidePredictions(
        m_startTime, m_endTime, false); // Get all predictions, not just high/low

    setPredictions(predictions);
}

void TidalCurveWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    // Check if widget is properly initialized
    if (!isVisible() || width() <= 0 || height() <= 0) {
        return;
    }

    // Create painter with proper error handling
    QPainter painter;
    if (!painter.begin(this)) {
        qDebug() << "[TIDAL-CURVE] Failed to initialize painter";
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Background
    painter.fillRect(rect(), m_backgroundColor);

    // Draw components only if we have valid data
    drawGrid(painter);
    drawAxes(painter);

    if (m_dataValid && !m_predictions.isEmpty()) {
        drawCurve(painter);
        drawTidePoints(painter);
    } else {
        // Draw "No Data" message
        painter.setPen(Qt::red);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "No tidal data available");
    }

    drawInfo(painter);

    // End painting explicitly
    painter.end();
}

void TidalCurveWidget::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    update();
}

void TidalCurveWidget::drawGrid(QPainter& painter)
{
    painter.setPen(QPen(m_gridColor, 1, Qt::DotLine));

    QRect plotRect(m_leftMargin, m_topMargin,
                   width() - m_leftMargin - m_rightMargin,
                   height() - m_topMargin - m_bottomMargin);

    // Vertical grid lines (time)
    int timeDivisions = 6;
    for (int i = 0; i <= timeDivisions; i++) {
        double timeRatio = (double)i / timeDivisions;
        int x = m_leftMargin + (int)(timeRatio * (width() - m_leftMargin - m_rightMargin));
        painter.drawLine(x, plotRect.top(), x, plotRect.bottom());
    }

    // Horizontal grid lines (height)
    int heightDivisions = 6;
    for (int i = 0; i <= heightDivisions; i++) {
        double heightRatio = (double)i / heightDivisions;
        int y = m_topMargin + (int)(heightRatio * (height() - m_topMargin - m_bottomMargin));
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);
    }
}

void TidalCurveWidget::drawAxes(QPainter& painter)
{
    painter.setPen(QPen(m_textColor, 2));
    QFont font("Arial", 9);
    painter.setFont(font);

    QRect plotRect(m_leftMargin, m_topMargin,
                   width() - m_leftMargin - m_rightMargin,
                   height() - m_topMargin - m_bottomMargin);

    // Draw axes
    painter.drawLine(plotRect.left(), plotRect.bottom(), plotRect.right(), plotRect.bottom()); // X-axis
    painter.drawLine(plotRect.left(), plotRect.top(), plotRect.left(), plotRect.bottom()); // Y-axis

    // X-axis labels (time)
    QFontMetrics fm(font);
    int timeDivisions = 6;
    for (int i = 0; i <= timeDivisions; i++) {
        double timeRatio = (double)i / timeDivisions;
        int x = m_leftMargin + (int)(timeRatio * (width() - m_leftMargin - m_rightMargin));

        // Calculate time
        double hours = m_minTime + timeRatio * (m_maxTime - m_minTime);
        QDateTime time = m_startTime.addSecs((qint64)(hours * 3600));
        QString timeStr = time.time().toString("hh:mm");

        QRect textRect = fm.boundingRect(timeStr);
        textRect.moveCenter(QPoint(x, plotRect.bottom() + fm.height() / 2 + 5));
        painter.drawText(textRect, Qt::AlignCenter, timeStr);
    }

    // Y-axis labels (height)
    int heightDivisions = 6;
    painter.setPen(m_textColor);
    for (int i = 0; i <= heightDivisions; i++) {
        double heightRatio = (double)i / heightDivisions;
        double height = m_minHeight + heightRatio * (m_maxHeight - m_minHeight);

        QString heightStr = QString::number(height, 'f', 1) + "m";
        QRect textRect = fm.boundingRect(heightStr);

        int y = plotRect.bottom() - (int)(heightRatio * (QWidget::height() - m_topMargin - m_bottomMargin));
        textRect.moveCenter(QPoint(m_leftMargin - fm.width(heightStr) / 2 - 5, y));
        painter.drawText(textRect, Qt::AlignCenter, heightStr);
    }

    // Axis labels
    QFont boldFont("Arial", 10, QFont::Bold);
    painter.setFont(boldFont);

    // X-axis label
    QString xAxisLabel = "Time (hours)";
    QRect xAxisRect = fm.boundingRect(xAxisLabel);
    xAxisRect.moveCenter(QPoint(width() / 2, height() - 10));
    painter.drawText(xAxisRect, Qt::AlignCenter, xAxisLabel);

    // Y-axis label
    QString yAxisLabel = "Height (m)";
    painter.save();
    painter.translate(15, height() / 2);
    painter.rotate(-90);
    QRect yAxisRect = fm.boundingRect(yAxisLabel);
    yAxisRect.moveCenter(QPoint(0, 0));
    painter.drawText(yAxisRect, Qt::AlignCenter, yAxisLabel);
    painter.restore();
}

void TidalCurveWidget::drawCurve(QPainter& painter)
{
    if (m_predictions.size() < 2) {
        qDebug() << "[TIDAL-CURVE] Not enough predictions for curve:" << m_predictions.size();
        return;
    }

    qDebug() << "[TIDAL-CURVE] Drawing curve with" << m_predictions.size() << "points";

    painter.setPen(QPen(m_curveColor, 3));
    painter.setBrush(Qt::NoBrush);

    // Create smooth curve path
    QPainterPath curvePath;

    for (int i = 0; i < m_predictions.size(); i++) {
        const TidePrediction& pred = m_predictions[i];

        // Calculate time offset from start in hours
        double timeOffset = m_startTime.secsTo(pred.dateTime) / 3600.0;

        // Fix negative timeOffset issue
        if (timeOffset < 0) {
            timeOffset = 0; // Clamp to minimum
            qDebug() << "[TIDAL-CURVE] Fixed negative timeOffset to 0";
        }

        QPointF point = mapToWidget(timeOffset, pred.height);

        qDebug() << "[TIDAL-CURVE] Point" << i << "TimeOffset:" << timeOffset << "Height:" << pred.height << "-> Widget:" << point.x() << point.y();

        if (i == 0) {
            curvePath.moveTo(point);
            qDebug() << "[TIDAL-CURVE] Move to first point:" << point;
        } else {
            // Simple line for now (instead of complex bezier)
            curvePath.lineTo(point);
        }
    }

    painter.drawPath(curvePath);
    qDebug() << "[TIDAL-CURVE] Curve drawn successfully";
}

void TidalCurveWidget::drawTidePoints(QPainter& painter)
{
    QFont font("Arial", 8);
    painter.setFont(font);

    for (const TidePrediction& pred : m_predictions) {
        // Calculate time offset
        double timeOffset = m_startTime.secsTo(pred.dateTime) / 3600.0;
        QPointF point = mapToWidget(timeOffset, pred.height);

        // Choose color based on tide type
        QColor color = pred.isHighTide ? m_highTideColor : m_lowTideColor;

        // Draw point
        painter.setPen(QPen(color, 2));
        painter.setBrush(QBrush(color.lighter(150)));
        painter.drawEllipse(point, 5, 5);

        // Draw tide type label
        QString label = pred.isHighTide ? "H" : "L";
        QRect labelRect = fontMetrics().boundingRect(label);
        labelRect.moveCenter(QPoint(point.x(), point.y() - 15));

        painter.setPen(color);
        painter.drawText(labelRect, Qt::AlignCenter, label);
    }
}

void TidalCurveWidget::drawInfo(QPainter& painter)
{
    QFont font("Arial", 10, QFont::Bold);
    painter.setFont(font);

    // Station info
    QString infoText;
    if (!m_currentStation.name.isEmpty()) {
        infoText = QString("Station: %1").arg(m_currentStation.name);
    } else {
        infoText = "No station selected";
    }

    if (!m_dataValid) {
        infoText += " - No data available";
    }

    painter.setPen(m_textColor);
    QRect infoRect = fontMetrics().boundingRect(infoText);
    infoRect.moveTo(10, 10);
    painter.drawText(infoRect, Qt::AlignLeft, infoText);

    // Date range info
    QFont dateFont("Arial", 8);
    painter.setFont(dateFont);
    QString dateText = QString("From: %1 To: %2")
        .arg(m_startTime.toString("dd.MM.yy hh:mm"))
        .arg(m_endTime.toString("dd.MM.yy hh:mm"));

    painter.setPen(m_textColor);
    QFontMetrics dateFm(dateFont);
    QRect dateRect = dateFm.boundingRect(dateText);
    dateRect.moveTo(10, 25);
    painter.drawText(dateRect, Qt::AlignLeft, dateText);
}

void TidalCurveWidget::calculateBounds()
{
    if (m_predictions.isEmpty()) {
        m_minHeight = -2.0;
        m_maxHeight = 4.0;
        m_minTime = 0.0;
        m_maxTime = (double)m_timeRangeHours;
        qDebug() << "[TIDAL-CURVE] Using default bounds - Height:" << m_minHeight << "to" << m_maxHeight << "Time:" << m_minTime << "to" << m_maxTime;
        return;
    }

    m_minHeight = 999.0;
    m_maxHeight = -999.0;
    m_minTime = 0.0; // Start from 0
    m_maxTime = (double)m_timeRangeHours; // End at 24 hours

    qDebug() << "[TIDAL-CURVE] Initial time bounds:" << m_minTime << "to" << m_maxTime;

    // Calculate height bounds and debug time offsets
    for (const TidePrediction& pred : m_predictions) {
        double timeOffset = m_startTime.secsTo(pred.dateTime) / 3600.0;

        qDebug() << "[TIDAL-CURVE] Prediction time offset:" << timeOffset << "Height:" << pred.height;

        m_minHeight = qMin(m_minHeight, pred.height);
        m_maxHeight = qMax(m_maxHeight, pred.height);

        // Don't let timeOffset change bounds, use fixed 24h range
        // m_minTime = qMin(m_minTime, timeOffset);
        // m_maxTime = qMax(m_maxTime, timeOffset);
    }

    // Add padding
    double heightPadding = (m_maxHeight - m_minHeight) * 0.1;
    if (heightPadding < 0.2) heightPadding = 0.2; // Minimum padding
    m_minHeight -= heightPadding;
    m_maxHeight += heightPadding;

    double timePadding = 0.5; // 0.5 hour padding
    m_minTime = qMax(0.0, m_minTime - timePadding);
    m_maxTime = qMin((double)m_timeRangeHours, m_maxTime + timePadding);

    qDebug() << "[TIDAL-CURVE] Final bounds - Height:" << m_minHeight << "to" << m_maxHeight
             << "Time:" << m_minTime << "to" << m_maxTime;
}

QPointF TidalCurveWidget::mapToWidget(double timeHours, double height) const
{
    int plotWidth = QWidget::width() - m_leftMargin - m_rightMargin;
    int plotHeight = QWidget::height() - m_topMargin - m_bottomMargin;

    // Clamp timeHours to valid range
    timeHours = qMax(m_minTime, qMin(m_maxTime, timeHours));
    height = qMax(m_minHeight, qMin(m_maxHeight, height));

    // Handle division by zero
    double timeRange = m_maxTime - m_minTime;
    double heightRange = m_maxHeight - m_minHeight;

    double timeRatio = 0.0;
    double heightRatio = 0.0;

    if (timeRange > 0.001) {
        timeRatio = (timeHours - m_minTime) / timeRange;
    }
    if (heightRange > 0.001) {
        heightRatio = (height - m_minHeight) / heightRange;
    }

    int x = m_leftMargin + (int)(timeRatio * plotWidth);
    int y = m_topMargin + plotHeight - (int)(heightRatio * plotHeight); // Inverted Y

    // Debug mapping
    // qDebug() << "[TIDAL-CURVE] Mapping - Time:" << timeHours << "-> Ratio:" << timeRatio << "-> X:" << x
    //          << "Height:" << height << "-> Ratio:" << heightRatio << "-> Y:" << y;

    return QPointF(x, y);
}

double TidalCurveWidget::mapFromWidgetX(int x) const
{
    int plotWidth = QWidget::width() - m_leftMargin - m_rightMargin;
    double timeRatio = (double)(x - m_leftMargin) / plotWidth;
    return m_minTime + timeRatio * (m_maxTime - m_minTime);
}

double TidalCurveWidget::mapFromWidgetY(int y) const
{
    int plotHeight = QWidget::height() - m_topMargin - m_bottomMargin;
    double heightRatio = (double)(m_topMargin + plotHeight - y) / plotHeight;
    return m_minHeight + heightRatio * (m_maxHeight - m_minHeight);
}