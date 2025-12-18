#include "tidalcurvewidget_simple.h"
#include <QApplication>
#include <QtMath>
#include <QPainterPath>
#include <QShowEvent>

TidalCurveWidgetSimple::TidalCurveWidgetSimple(QWidget *parent)
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
    // Basic widget setup
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(true);

    setMinimumSize(400, 250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Set a simple background
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);

    // Initialize with current time
    m_startTime = QDateTime::currentDateTime();
    m_endTime = m_startTime.addSecs(m_timeRangeHours * 3600);

    // Start with empty data
    m_dataValid = false;
    m_predictions.clear();

    // Force widget to be visible
    setVisible(true);

    qDebug() << "[TIDAL-CURVE-SIMPLE] Widget created successfully, visible:" << isVisible();
}

TidalCurveWidgetSimple::~TidalCurveWidgetSimple()
{
    qDebug() << "[TIDAL-CURVE-SIMPLE] Widget destroyed";
}

void TidalCurveWidgetSimple::setTideManager(TideManager* manager)
{
    m_tideManager = manager;
    refresh();
}

void TidalCurveWidgetSimple::setStation(const TideStation& station)
{
    m_currentStation = station;
    refresh();
}

void TidalCurveWidgetSimple::setPredictions(const QList<TidePrediction>& predictions)
{
    m_predictions = predictions;

    qDebug() << "[TIDAL-CURVE-SIMPLE] Set" << predictions.size() << "predictions";
    for (int i = 0; i < predictions.size(); ++i) {
        const TidePrediction& pred = predictions[i];
        qDebug() << "[TIDAL-CURVE-SIMPLE] Prediction" << i << ":" << pred.dateTime.toString()
                 << "Height:" << pred.height << "Type:" << (pred.isHighTide ? "HIGH" : "LOW");
    }

    calculateBounds();
    m_dataValid = !m_predictions.isEmpty();

    qDebug() << "[TIDAL-CURVE-SIMPLE] Data valid:" << m_dataValid;
    qDebug() << "[TIDAL-CURVE-SIMPLE] Widget size:" << QWidget::width() << "x" << QWidget::height();
    qDebug() << "[TIDAL-CURVE-SIMPLE] Widget visible:" << isVisible();

    update();
    repaint();  // Force immediate repaint
}

void TidalCurveWidgetSimple::setTimeRange(int hours)
{
    m_timeRangeHours = hours;
    m_endTime = m_startTime.addSecs(m_timeRangeHours * 3600);
    refresh();
}

void TidalCurveWidgetSimple::refresh()
{
    if (!m_tideManager || m_currentStation.id.isEmpty()) {
        m_dataValid = false;
        update();
        return;
    }

    // Get fresh predictions for current station
    QList<TidePrediction> predictions = m_tideManager->getTidePredictions(
        m_startTime, m_endTime, false);

    setPredictions(predictions);
}

void TidalCurveWidgetSimple::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    // Ensure widget is visible
    if (!isVisible()) {
        return;
    }

    qDebug() << "[TIDAL-CURVE-SIMPLE] PaintEvent called! Size:" << QWidget::width() << "x" << QWidget::height()
             << "Data valid:" << m_dataValid << "Predictions:" << m_predictions.size();

    // Use the base class paint event for background
    QWidget::paintEvent(event);

    // Create painter on the widget
    QPainter painter(this);

    // Check if painter is valid
    if (!painter.isActive()) {
        qDebug() << "[TIDAL-CURVE-SIMPLE] Painter is not active!";
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw background
    painter.fillRect(rect(), m_backgroundColor);

    qDebug() << "[TIDAL-CURVE-SIMPLE] Drawing grid and axes";

    // Always draw grid and axes
    drawGrid(painter);
    drawAxes(painter);

    // Draw curve and points if we have data
    if (m_dataValid && !m_predictions.isEmpty()) {
        qDebug() << "[TIDAL-CURVE-SIMPLE] Drawing curve and points";
        drawCurve(painter);
        drawTidePoints(painter);
    } else {
        // Draw "No Data" message
        qDebug() << "[TIDAL-CURVE-SIMPLE] No data - drawing message";
        painter.setPen(Qt::red);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "No tidal data available");
    }

    drawInfo(painter);

    qDebug() << "[TIDAL-CURVE-SIMPLE] PaintEvent completed";
}

void TidalCurveWidgetSimple::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    update();
}

void TidalCurveWidgetSimple::showEvent(QShowEvent* event)
{
    Q_UNUSED(event)

    qDebug() << "[TIDAL-CURVE-SIMPLE] ShowEvent called!";

    // Force update when widget becomes visible
    update();

    // If we have data, force a repaint
    if (m_dataValid && !m_predictions.isEmpty()) {
        repaint();
    }
}

void TidalCurveWidgetSimple::drawGrid(QPainter& painter)
{
    painter.setPen(QPen(m_gridColor, 1, Qt::DotLine));

    QRect plotRect(m_leftMargin, m_topMargin,
                   QWidget::width() - m_leftMargin - m_rightMargin,
                   QWidget::height() - m_topMargin - m_bottomMargin);

    // Vertical grid lines (time)
    int timeDivisions = 6;
    for (int i = 0; i <= timeDivisions; i++) {
        double timeRatio = (double)i / timeDivisions;
        int x = m_leftMargin + (int)(timeRatio * (QWidget::width() - m_leftMargin - m_rightMargin));
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

void TidalCurveWidgetSimple::drawAxes(QPainter& painter)
{
    painter.setPen(QPen(m_textColor, 2));
    QFont font("Arial", 9);
    painter.setFont(font);

    QRect plotRect(m_leftMargin, m_topMargin,
                   QWidget::width() - m_leftMargin - m_rightMargin,
                   QWidget::height() - m_topMargin - m_bottomMargin);

    // Draw axes
    painter.drawLine(plotRect.left(), plotRect.bottom(), plotRect.right(), plotRect.bottom()); // X-axis
    painter.drawLine(plotRect.left(), plotRect.top(), plotRect.left(), plotRect.bottom()); // Y-axis

    // X-axis labels (time)
    QFontMetrics fm(font);
    int timeDivisions = 6;
    for (int i = 0; i <= timeDivisions; i++) {
        double timeRatio = (double)i / timeDivisions;
        int x = m_leftMargin + (int)(timeRatio * (QWidget::width() - m_leftMargin - m_rightMargin));

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
    QString xAxisLabel = "Time";
    QRect xAxisRect = fm.boundingRect(xAxisLabel);
    xAxisRect.moveCenter(QPoint(QWidget::width() / 2, QWidget::height() - 10));
    painter.drawText(xAxisRect, Qt::AlignCenter, xAxisLabel);

    // Y-axis label
    QString yAxisLabel = "Height (m)";
    painter.save();
    painter.translate(15, QWidget::height() / 2);
    painter.rotate(-90);
    QRect yAxisRect = fm.boundingRect(yAxisLabel);
    yAxisRect.moveCenter(QPoint(0, 0));
    painter.drawText(yAxisRect, Qt::AlignCenter, yAxisLabel);
    painter.restore();
}

void TidalCurveWidgetSimple::drawCurve(QPainter& painter)
{
    if (m_predictions.size() < 2) {
        return;
    }

    painter.setPen(QPen(m_curveColor, 3));
    painter.setBrush(Qt::NoBrush);

    // Create smooth curve path using cubic interpolation
    QPainterPath curvePath;

    for (int i = 0; i < m_predictions.size(); i++) {
        const TidePrediction& pred = m_predictions[i];

        // Calculate time offset from start in hours
        double timeOffset = m_startTime.secsTo(pred.dateTime) / 3600.0;

        // Clamp to valid range
        if (timeOffset < 0) timeOffset = 0;
        if (timeOffset > 24) timeOffset = 24;

        QPointF point = mapToWidget(timeOffset, pred.height);

        if (i == 0) {
            curvePath.moveTo(point);
        } else {
            // Create smooth curve
            const TidePrediction& prevPred = m_predictions[i-1];
            double prevTimeOffset = m_startTime.secsTo(prevPred.dateTime) / 3600.0;
            if (prevTimeOffset < 0) prevTimeOffset = 0;
            if (prevTimeOffset > 24) prevTimeOffset = 24;

            QPointF prevPoint = mapToWidget(prevTimeOffset, prevPred.height);

            // Control points for cubic bezier
            QPointF cp1 = prevPoint + QPointF((point.x() - prevPoint.x()) * 0.3, 0);
            QPointF cp2 = point - QPointF((point.x() - prevPoint.x()) * 0.3, 0);

            curvePath.cubicTo(cp1, cp2, point);
        }
    }

    painter.drawPath(curvePath);
}

void TidalCurveWidgetSimple::drawTidePoints(QPainter& painter)
{
    QFont font("Arial", 8);
    painter.setFont(font);

    for (const TidePrediction& pred : m_predictions) {
        // Calculate time offset
        double timeOffset = m_startTime.secsTo(pred.dateTime) / 3600.0;
        if (timeOffset < 0) timeOffset = 0;
        if (timeOffset > 24) timeOffset = 24;

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

void TidalCurveWidgetSimple::drawInfo(QPainter& painter)
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

void TidalCurveWidgetSimple::calculateBounds()
{
    if (m_predictions.isEmpty()) {
        m_minHeight = -2.0;
        m_maxHeight = 4.0;
        m_minTime = 0.0;
        m_maxTime = (double)m_timeRangeHours;
        return;
    }

    m_minHeight = 999.0;
    m_maxHeight = -999.0;
    m_minTime = 0.0;
    m_maxTime = (double)m_timeRangeHours;

    // Calculate height bounds
    for (const TidePrediction& pred : m_predictions) {
        double timeOffset = m_startTime.secsTo(pred.dateTime) / 3600.0;

        if (timeOffset >= 0 && timeOffset <= 24) {
            m_minHeight = qMin(m_minHeight, pred.height);
            m_maxHeight = qMax(m_maxHeight, pred.height);
        }
    }

    // Add padding
    double heightPadding = (m_maxHeight - m_minHeight) * 0.1;
    if (heightPadding < 0.2) heightPadding = 0.2;
    m_minHeight -= heightPadding;
    m_maxHeight += heightPadding;

    // Ensure minimum range
    if (m_maxHeight - m_minHeight < 1.0) {
        m_minHeight -= 0.5;
        m_maxHeight += 0.5;
    }
}

QPointF TidalCurveWidgetSimple::mapToWidget(double timeHours, double height) const
{
    int plotWidth = QWidget::width() - m_leftMargin - m_rightMargin;
    int plotHeight = QWidget::height() - m_topMargin - m_bottomMargin;

    // Clamp to valid range
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

    return QPointF(x, y);
}

double TidalCurveWidgetSimple::mapFromWidgetX(int x) const
{
    int plotWidth = QWidget::width() - m_leftMargin - m_rightMargin;
    double timeRatio = (double)(x - m_leftMargin) / plotWidth;
    return m_minTime + timeRatio * (m_maxTime - m_minTime);
}

double TidalCurveWidgetSimple::mapFromWidgetY(int y) const
{
    int plotHeight = QWidget::height() - m_topMargin - m_bottomMargin;
    double heightRatio = (double)(m_topMargin + plotHeight - y) / plotHeight;
    return m_minHeight + heightRatio * (m_maxHeight - m_minHeight);
}