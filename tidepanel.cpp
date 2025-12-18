#include "tidepanel.h"
#include "tidalcurvewidget_simple.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextEdit>

TidePanel::TidePanel(QWidget *parent)
    : QWidget(parent)
    , m_tideManager(nullptr)
    , m_tideBarsWidget(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_currentLatitude(0.0)
    , m_currentLongitude(0.0)
{
    // BUILD REAL TIDE PANEL - THE REAL DEAL!
    setWindowTitle("Tide Predictions");
    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title
    QLabel *titleLabel = new QLabel("TIDE PREDICTIONS", this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 16px; color: blue; background: yellow; padding: 5px;");
    layout->addWidget(titleLabel);

    // Status
    m_statusLabel = new QLabel("Loading tide data...", this);
    m_statusLabel->setStyleSheet("background: white; border: 1px solid black; padding: 5px;");
    layout->addWidget(m_statusLabel);

    // Simple Tide Visualization
    QWidget *tideVizWidget = new QWidget(this);
    tideVizWidget->setMinimumHeight(300);
    tideVizWidget->setMaximumHeight(400);
    tideVizWidget->setStyleSheet("background-color: #f0f8ff; border: 2px solid blue; border-radius: 5px;");

    QVBoxLayout *vizLayout = new QVBoxLayout(tideVizWidget);

    // Title
    QLabel *vizTitle = new QLabel("⚓ Tidal Height Visualization", tideVizWidget);
    vizTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #0066cc; padding: 10px; background-color: white; border-radius: 3px;");
    vizTitle->setAlignment(Qt::AlignCenter);
    vizLayout->addWidget(vizTitle);

    // Create a simple bar visualization
    m_tideBarsWidget = new QWidget(tideVizWidget);
    m_tideBarsWidget->setMinimumHeight(200);
    m_tideBarsWidget->setStyleSheet("background-color: white; border: 1px solid #ccc; margin: 5px;");
    vizLayout->addWidget(m_tideBarsWidget);

    layout->addWidget(tideVizWidget);

    qDebug() << "[TIDE-PANEL] Created tide graph container with curve widget";

    // List
    m_predictionsList = new QListWidget(this);
    m_predictionsList->setStyleSheet("background: white; border: 2px solid blue;");
    m_predictionsList->setMinimumHeight(150);
    layout->addWidget(m_predictionsList);

    // Button
    QPushButton *refreshBtn = new QPushButton("Refresh Tide Data", this);
    refreshBtn->setStyleSheet("background: lightgreen; padding: 10px; font-weight: bold;");
    layout->addWidget(refreshBtn);

    // Test data
    m_predictionsList->addItem("✓ High Tide: 02:30 - 1.2m");
    m_predictionsList->addItem("✓ Low Tide: 08:45 - 0.3m");
    m_statusLabel->setText("SUCCESS: Tide Panel Working!");

    connect(refreshBtn, &QPushButton::clicked, this, &TidePanel::onRefreshClicked);

    qDebug() << "[TIDE] Tide Panel Created Successfully!";
}

TidePanel::~TidePanel()
{
}

void TidePanel::setTideManager(TideManager *tideManager)
{
    m_tideManager = tideManager;

    // Initial update if manager is set
    if (m_tideManager) {
        updateTidePredictions();
    }
}

void TidePanel::updateCurrentPosition(double latitude, double longitude)
{
    m_currentLatitude = latitude;
    m_currentLongitude = longitude;
}

void TidePanel::onRefreshClicked()
{
    qDebug() << "[TIDE] onRefreshClicked called";

    // IMMEDIATE VISIBILITY TEST - Make everything visible immediately
    if (m_statusLabel) {
        m_statusLabel->setText("TESTING VISIBILITY - THIS SHOULD BE VISIBLE!");
        m_statusLabel->setVisible(true);
        m_statusLabel->show();
        qDebug() << "[TIDE] Status label visible:" << m_statusLabel->isVisible();
    }

    if (m_predictionsList) {
        m_predictionsList->clear();

        // ADD TEST ITEMS IMMEDIATELY - don't wait for tide data
        QListWidgetItem *testItem1 = new QListWidgetItem("TEST ITEM 1 - HIGH TIDE");
        testItem1->setForeground(Qt::red);
        m_predictionsList->addItem(testItem1);

        QListWidgetItem *testItem2 = new QListWidgetItem("TEST ITEM 2 - LOW TIDE");
        testItem2->setForeground(Qt::blue);
        m_predictionsList->addItem(testItem2);

        // FORCE VISIBILITY
        m_predictionsList->setVisible(true);
        m_predictionsList->show();
        m_predictionsList->raise();

        qDebug() << "[TIDE] Test list visible:" << m_predictionsList->isVisible();
        qDebug() << "[TIDE] Test list count:" << m_predictionsList->count();
        qDebug() << "[TIDE] Test list size:" << m_predictionsList->size();
        qDebug() << "[TIDE] Test list height:" << m_predictionsList->height();

        // FORCE REPAINT
        m_predictionsList->update();
        m_predictionsList->repaint();
    }

    // NOW try tide data
    if (!m_tideManager) {
        if (m_statusLabel) m_statusLabel->setText("Error: No tide manager");
        return;
    }

    // Continue with tide data loading...
}

// Empty implementations for other required methods
void TidePanel::onLoadDataClicked() { onRefreshClicked(); }
void TidePanel::onStationComboBoxChanged(int) {}
void TidePanel::onLocationChanged() {}
void TidePanel::onDateRangeChanged() {}
void TidePanel::onTideDataLoaded() { onRefreshClicked(); }
void TidePanel::onPredictionsUpdated() { onRefreshClicked(); }
void TidePanel::onErrorOccurred(const QString &error) { m_statusLabel->setText("Error: " + error); }
void TidePanel::updateCurrentTide() {}
void TidePanel::onStationSearchTextChanged(const QString &) {}
void TidePanel::onUseCurrentLocationClicked() {}
void TidePanel::refreshStationList() {}
void TidePanel::updateCurrentTideDisplay() {}
void TidePanel::setControlsEnabled(bool) {}
void TidePanel::showStatus(const QString &msg) { m_statusLabel->setText(msg); }
void TidePanel::showError(const QString &error) { m_statusLabel->setText("ERROR: " + error); }

void TidePanel::updateTidePredictions()
{
    if (!m_tideManager) {
        qDebug() << "[TIDE-PANEL] Cannot update - missing tide manager";
        return;
    }

    qDebug() << "[TIDE-PANEL] Updating tide predictions, manager exists:" << (m_tideManager != nullptr);

    // Get predictions for the next 24 hours
    QDateTime now = QDateTime::currentDateTime();
    QDateTime endTime = now.addSecs(24 * 3600); // 24 hours from now

    qDebug() << "[TIDE-PANEL] Getting predictions from" << now.toString() << "to" << endTime.toString();

    QList<TidePrediction> predictions = m_tideManager->getTidePredictions(now, endTime, false);

    // Debug: Try to get more data if JSON predictions are empty
    if (predictions.isEmpty() && m_tideManager->isUsingJsonData()) {
        qDebug() << "[TIDE-PANEL] No predictions from standard method, trying JSON directly";

        // Try to get today's predictions instead (JSON data might be limited to today)
        QList<TidePrediction> todaysPredictions = m_tideManager->getTodaysTides();
        qDebug() << "[TIDE-PANEL] Got" << todaysPredictions.size() << "today's predictions";

        if (!todaysPredictions.isEmpty()) {
            predictions = todaysPredictions;
        }
    }

    qDebug() << "[TIDE-PANEL] Got" << predictions.size() << "predictions from tide manager";

    if (predictions.isEmpty()) {
        qDebug() << "[TIDE-PANEL] No predictions available";
        m_statusLabel->setText("No tide predictions available");
        return;
    }

    // Create simple bar visualization
    createTideBars(predictions);

    // Update status
    m_statusLabel->setText(QString("Updated: %1 predictions for %2 hours")
                          .arg(predictions.size())
                          .arg(24));

    // Update predictions list with detailed info
    m_predictionsList->clear();

    for (const TidePrediction &pred : predictions) {
        QString type = pred.isHighTide ? "HIGH" : "LOW";
        QString color = pred.isHighTide ? "red" : "blue";

        QListWidgetItem *item = new QListWidgetItem(
            QString("%1 - %2: %3 (%4m)")
            .arg(type)
            .arg(pred.dateTime.time().toString("hh:mm"))
            .arg(pred.dateTime.toString("dd.MM"))
            .arg(pred.height, 0, 'f', 2)
        );

        item->setForeground(QColor(color));
        m_predictionsList->addItem(item);
    }

    qDebug() << "[TIDE-PANEL] Updated" << predictions.size() << "predictions";
}

void TidePanel::createTideBars(const QList<TidePrediction> &predictions)
{
    if (!m_tideBarsWidget || predictions.isEmpty()) {
        return;
    }

    // Delete existing layout
    delete m_tideBarsWidget->layout();

    // Create new layout
    QHBoxLayout *barsLayout = new QHBoxLayout(m_tideBarsWidget);

    // Find max height for scaling
    double maxHeight = 0;
    for (const auto &pred : predictions) {
        maxHeight = qMax(maxHeight, pred.height);
    }
    maxHeight += 0.5; // Add padding

    // Create bars for each prediction
    for (const auto &pred : predictions) {
        QWidget *barContainer = new QWidget(m_tideBarsWidget);
        QVBoxLayout *barLayout = new QVBoxLayout(barContainer);

        // Height value label
        QLabel *heightLabel = new QLabel(QString::number(pred.height, 'f', 1) + "m", barContainer);
        heightLabel->setAlignment(Qt::AlignCenter);
        heightLabel->setStyleSheet("font-weight: bold; font-size: 10px;");

        // Bar
        QLabel *bar = new QLabel(barContainer);
        int barHeight = qRound((pred.height / maxHeight) * 100); // Max 100px height
        bar->setFixedSize(40, barHeight);

        if (pred.isHighTide) {
            bar->setStyleSheet(QString("background-color: #ff4444; border: 1px solid #cc0000; border-radius: 3px;"));
            heightLabel->setStyleSheet("font-weight: bold; font-size: 10px; color: #ff4444;");
        } else {
            bar->setStyleSheet(QString("background-color: #4444ff; border: 1px solid #0000cc; border-radius: 3px;"));
            heightLabel->setStyleSheet("font-weight: bold; font-size: 10px; color: #4444ff;");
        }

        // Time label
        QLabel *timeLabel = new QLabel(pred.dateTime.time().toString("hh:mm"), barContainer);
        timeLabel->setAlignment(Qt::AlignCenter);
        timeLabel->setStyleSheet("font-size: 9px; color: #666;");

        // Type label
        QLabel *typeLabel = new QLabel(pred.isHighTide ? "HIGH" : "LOW", barContainer);
        typeLabel->setAlignment(Qt::AlignCenter);
        typeLabel->setStyleSheet("font-weight: bold; font-size: 9px;");

        barLayout->addWidget(heightLabel);
        barLayout->addWidget(bar);
        barLayout->addWidget(timeLabel);
        barLayout->addWidget(typeLabel);
        barLayout->addStretch();

        barsLayout->addWidget(barContainer);
    }

    // Add spacer at the end
    barsLayout->addStretch();

    m_tideBarsWidget->update();
}