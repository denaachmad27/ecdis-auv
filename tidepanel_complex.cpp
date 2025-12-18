#include "tidepanel.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QHeaderView>
#include <QInputDialog>
#include <QStandardPaths>

TidePanel::TidePanel(QWidget *parent)
    : QWidget(parent)
    , m_tideManager(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_currentLatitude(0.0)
    , m_currentLongitude(0.0)
{
    qDebug() << "[TIDE] TidePanel constructor started";
    setupUI();
    qDebug() << "[TIDE] TidePanel UI setup completed";

    // Setup update timer for current tide
    m_updateTimer->setInterval(60000); // Update every minute
    connect(m_updateTimer, &QTimer::timeout, this, &TidePanel::updateCurrentTide);

    // Set default dates
    QDate today = QDate::currentDate();
    m_startDateEdit->setDate(today);
    m_endDateEdit->setDate(today.addDays(1));

    // Auto-load tide data after initialization
    qDebug() << "[TIDE] Scheduling auto-load in 500ms";
    QTimer::singleShot(500, this, &TidePanel::onLoadDataClicked);
}

TidePanel::~TidePanel()
{
}

void TidePanel::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // ========== DATA SECTION ==========
    dataGroup = new QGroupBox("Tide Data", this);
    QVBoxLayout* dataLayout = new QVBoxLayout(dataGroup);

    m_dataPathEdit = new QLineEdit(QApplication::applicationDirPath() + "/../data/Tides/7C0TIDES.7CB", this);
    m_dataPathEdit->setReadOnly(true);

    m_loadDataButton = new QPushButton("Load Data", this);
    m_dataStatusLabel = new QLabel("No data loaded", this);
    m_dataStatusLabel->setStyleSheet("color: red;");

    dataLayout->addWidget(new QLabel("Data File:"));
    dataLayout->addWidget(m_dataPathEdit);
    dataLayout->addWidget(m_loadDataButton);
    dataLayout->addWidget(m_dataStatusLabel);

    mainLayout->addWidget(dataGroup);

    // ========== LOCATION SECTION ==========
    locationGroup = new QGroupBox("Location", this);
    QGridLayout* locationLayout = new QGridLayout(locationGroup);

    locationLayout->addWidget(new QLabel("Latitude:"), 0, 0);
    m_latitudeSpinBox = new QDoubleSpinBox(this);
    m_latitudeSpinBox->setRange(-90.0, 90.0);
    m_latitudeSpinBox->setDecimals(6);
    m_latitudeSpinBox->setSingleStep(0.001);
    m_latitudeSpinBox->setValue(53.87); // Default: Cuxhaven
    locationLayout->addWidget(m_latitudeSpinBox, 0, 1);

    locationLayout->addWidget(new QLabel("Longitude:"), 1, 0);
    m_longitudeSpinBox = new QDoubleSpinBox(this);
    m_longitudeSpinBox->setRange(-180.0, 180.0);
    m_longitudeSpinBox->setDecimals(6);
    m_longitudeSpinBox->setSingleStep(0.001);
    m_longitudeSpinBox->setValue(8.71); // Default: Cuxhaven
    locationLayout->addWidget(m_longitudeSpinBox, 1, 1);

    m_useCurrentLocationButton = new QPushButton("Use Current Position", this);
    locationLayout->addWidget(m_useCurrentLocationButton, 2, 0, 1, 2);

    locationLayout->addWidget(new QLabel("Station:"), 3, 0);
    m_stationComboBox = new QComboBox(this);
    m_stationComboBox->setEditable(true);
    locationLayout->addWidget(m_stationComboBox, 3, 1);

    locationLayout->addWidget(new QLabel("Search:"), 4, 0);
    m_stationSearchEdit = new QLineEdit(this);
    m_stationSearchEdit->setPlaceholderText("Search stations...");
    locationLayout->addWidget(m_stationSearchEdit, 4, 1);

    mainLayout->addWidget(locationGroup);

    // ========== TIME SECTION ==========
    timeGroup = new QGroupBox("Time Range", this);
    QGridLayout* timeLayout = new QGridLayout(timeGroup);

    timeLayout->addWidget(new QLabel("Start Date:"), 0, 0);
    m_startDateEdit = new QDateEdit(this);
    m_startDateEdit->setCalendarPopup(true);
    timeLayout->addWidget(m_startDateEdit, 0, 1);

    timeLayout->addWidget(new QLabel("End Date:"), 1, 0);
    m_endDateEdit = new QDateEdit(this);
    m_endDateEdit->setCalendarPopup(true);
    timeLayout->addWidget(m_endDateEdit, 1, 1);

    m_refreshButton = new QPushButton("Refresh Predictions", this);
    timeLayout->addWidget(m_refreshButton, 2, 0, 1, 2);

    mainLayout->addWidget(timeGroup);

    // ========== CURRENT TIDE SECTION ==========
    currentGroup = new QGroupBox("Current Tide", this);
    QGridLayout* currentLayout = new QGridLayout(currentGroup);

    currentLayout->addWidget(new QLabel("Station:"), 0, 0);
    m_currentStationLabel = new QLabel("No station selected", this);
    currentLayout->addWidget(m_currentStationLabel, 0, 1);

    currentLayout->addWidget(new QLabel("Time:"), 1, 0);
    m_currentTimeLabel = new QLabel("--", this);
    currentLayout->addWidget(m_currentTimeLabel, 1, 1);

    currentLayout->addWidget(new QLabel("Height:"), 2, 0);
    m_currentHeightLabel = new QLabel("--", this);
    currentLayout->addWidget(m_currentHeightLabel, 2, 1);

    currentLayout->addWidget(new QLabel("Trend:"), 3, 0);
    m_currentTrendLabel = new QLabel("--", this);
    currentLayout->addWidget(m_currentTrendLabel, 3, 1);

    mainLayout->addWidget(currentGroup);

    // ========== PREDICTIONS SECTION ==========
    predictionsGroup = new QGroupBox("Tide Predictions", this);
    QVBoxLayout* predictionsLayout = new QVBoxLayout(predictionsGroup);

    m_predictionsList = new QListWidget(this);
    m_predictionsList->setMaximumHeight(200);
    predictionsLayout->addWidget(m_predictionsList);

    mainLayout->addWidget(predictionsGroup);

    // ========== STATUS SECTION ==========
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Ready", this);
    mainLayout->addWidget(m_statusLabel);

    // Connect signals
    connect(m_loadDataButton, &QPushButton::clicked, this, &TidePanel::onLoadDataClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &TidePanel::onRefreshClicked);
    connect(m_useCurrentLocationButton, &QPushButton::clicked, this, &TidePanel::onUseCurrentLocationClicked);
    connect(m_latitudeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TidePanel::onLocationChanged);
    connect(m_longitudeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TidePanel::onLocationChanged);
    connect(m_stationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TidePanel::onStationComboBoxChanged);
    connect(m_stationSearchEdit, &QLineEdit::textChanged, this, &TidePanel::onStationSearchTextChanged);
    connect(m_startDateEdit, &QDateEdit::dateChanged, this, &TidePanel::onDateRangeChanged);
    connect(m_endDateEdit, &QDateEdit::dateChanged, this, &TidePanel::onDateRangeChanged);
    connect(m_predictionsList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem *) {
                if (current) {
                    // Show details in status label for now
                    m_statusLabel->setText(current->data(Qt::UserRole).toString());
                }
            });

    setControlsEnabled(false);
}

void TidePanel::setTideManager(TideManager *tideManager)
{
    qDebug() << "[TIDE] setTideManager called with" << (tideManager ? "valid" : "null") << "manager";

    if (m_tideManager) {
        disconnect(m_tideManager, nullptr, this, nullptr);
    }

    m_tideManager = tideManager;

    if (m_tideManager) {
        qDebug() << "[TIDE] Connecting tide manager signals";
        connect(m_tideManager, &TideManager::tideDataLoaded,
                this, &TidePanel::onTideDataLoaded);
        connect(m_tideManager, &TideManager::predictionsUpdated,
                this, &TidePanel::onPredictionsUpdated);
        connect(m_tideManager, &TideManager::errorOccurred,
                this, &TidePanel::onErrorOccurred);
    } else {
        qDebug() << "[TIDE] Tide manager is null!";
    }
}

void TidePanel::updateCurrentPosition(double latitude, double longitude)
{
    m_currentLatitude = latitude;
    m_currentLongitude = longitude;
    m_useCurrentLocationButton->setEnabled(true);
}

void TidePanel::onLoadDataClicked()
{
    qDebug() << "[TIDE] onLoadDataClicked called";

    if (!m_tideManager) {
        qDebug() << "[TIDE] Tide manager is null!";
        QMessageBox::warning(this, "Error", "Tide manager not initialized");
        return;
    }

    QString dataPath = m_dataPathEdit->text();
    qDebug() << "[TIDE] Looking for tide data at:" << dataPath;

    // Try multiple possible paths for the tide data file
    QStringList possiblePaths = {
        dataPath,
        QCoreApplication::applicationDirPath() + "/../data/Tides/7C0TIDES.7CB",
        QCoreApplication::applicationDirPath() + "/data/Tides/7C0TIDES.7CB",
        "data/Tides/7C0TIDES.7CB",
        "C:/Projects/ecdis-auv/data/Tides/7C0TIDES.7CB"
    };

    qDebug() << "[TIDE] Checking" << possiblePaths.size() << "possible paths";

    QString foundPath;
    for (const QString &path : possiblePaths) {
        qDebug() << "[TIDE] Checking path:" << path;
        if (QFileInfo::exists(path)) {
            foundPath = path;
            qDebug() << "[TIDE] Found tide data at:" << foundPath;
            break;
        }
    }

    if (foundPath.isEmpty()) {
        qDebug() << "[TIDE] Tide data file not found!";
        QMessageBox::warning(this, "Error",
            "Tide data file not found. Please ensure 7C0TIDES.7CB is in the data/Tides folder.\n"
            "Attempted paths:\n" + possiblePaths.join("\n"));
        return;
    }

    // Update the path display if we found a different path
    if (foundPath != dataPath) {
        m_dataPathEdit->setText(foundPath);
        qDebug() << "Found tide data at:" << foundPath;
    }

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    showStatus("Loading tide data...");

    qDebug() << "[TIDE] Calling loadTideData with path:" << foundPath;

    if (m_tideManager->loadTideData(foundPath)) {
        qDebug() << "[TIDE] loadTideData returned true";
        // Success handled in onTideDataLoaded signal
    } else {
        qDebug() << "[TIDE] loadTideData returned false";
        m_progressBar->setVisible(false);
        showError("Failed to load tide data: " + m_tideManager->getLastError());
    }
}

void TidePanel::onRefreshClicked()
{
    qDebug() << "[TIDE] onRefreshClicked called";
    updateTidePredictions();
}

void TidePanel::onStationComboBoxChanged(int index)
{
    Q_UNUSED(index)
    qDebug() << "[TIDE] onStationComboBoxChanged called";
    onLocationChanged();
}

void TidePanel::onLocationChanged()
{
    qDebug() << "[TIDE] onLocationChanged called";
    if (!m_tideManager || !m_tideManager->isInitialized()) {
        return;
    }

    double latitude = m_latitudeSpinBox->value();
    double longitude = m_longitudeSpinBox->value();
    qDebug() << "[TIDE] Setting prediction location to:" << latitude << longitude;

    m_tideManager->setPredictionLocation(latitude, longitude);
    updateTidePredictions();

    emit locationUpdated(latitude, longitude);
}

void TidePanel::onDateRangeChanged()
{
    qDebug() << "[TIDE] onDateRangeChanged called";
    updateTidePredictions();
}

void TidePanel::onTideDataLoaded()
{
    qDebug() << "[TIDE] onTideDataLoaded called";
    m_progressBar->setVisible(false);
    m_dataStatusLabel->setText("Data loaded successfully");
    m_dataStatusLabel->setStyleSheet("color: green;");
    showStatus("Tide data loaded successfully");

    setControlsEnabled(true);
    refreshStationList();
    updateTidePredictions();
}

void TidePanel::onPredictionsUpdated()
{
    qDebug() << "[TIDE] onPredictionsUpdated called";
    updateTidePredictions();
}

void TidePanel::onErrorOccurred(const QString &error)
{
    qDebug() << "[TIDE] onErrorOccurred called with:" << error;
    m_progressBar->setVisible(false);
    showError(error);
}

void TidePanel::updateCurrentTide()
{
    qDebug() << "[TIDE] updateCurrentTide called";
    if (!m_tideManager || !m_tideManager->isInitialized()) {
        return;
    }

    TidePrediction current = m_tideManager->getCurrentTide();
    updateCurrentTideDisplay();
}

void TidePanel::onStationSearchTextChanged(const QString &text)
{
    qDebug() << "[TIDE] onStationSearchTextChanged called with:" << text;

    if (!m_tideManager || !m_tideManager->isInitialized()) {
        return;
    }

    // Filter stations based on search text
    QString currentText = m_stationComboBox->currentText();
    m_stationComboBox->clear();

    QList<TideStation> stations = m_tideManager->getAvailableStations();
    for (const TideStation &station : stations) {
        if (text.isEmpty() || station.name.contains(text, Qt::CaseInsensitive)) {
            m_stationComboBox->addItem(station.name);
        }
    }

    // Restore selection if possible
    int index = m_stationComboBox->findText(currentText);
    if (index >= 0) {
        m_stationComboBox->setCurrentIndex(index);
    }
}

void TidePanel::onUseCurrentLocationClicked()
{
    qDebug() << "[TIDE] onUseCurrentLocationClicked called";
    if (m_currentLatitude != 0.0 && m_currentLongitude != 0.0) {
        m_latitudeSpinBox->setValue(m_currentLatitude);
        m_longitudeSpinBox->setValue(m_currentLongitude);
    } else {
        QMessageBox::information(this, "Info", "No current position available");
    }
}

void TidePanel::refreshStationList()
{
    qDebug() << "[TIDE] refreshStationList called";
    if (!m_tideManager || !m_tideManager->isInitialized()) {
        return;
    }

    QList<TideStation> stations = m_tideManager->getAvailableStations();
    qDebug() << "[TIDE] Found" << stations.size() << "stations";

    m_stationComboBox->clear();
    m_stationComboBox->addItem(""); // Empty option for manual coordinates

    for (const TideStation &station : stations) {
        qDebug() << "[TIDE] Adding station:" << station.name << "at" << station.location.latitude << station.location.longitude;
        m_stationComboBox->addItem(station.name);
    }

    // Add "Use Coordinates" option
    m_stationComboBox->addItem("Use Coordinates");
    qDebug() << "[TIDE] Added 'Use Coordinates' option";
}

void TidePanel::updateTidePredictions()
{
    qDebug() << "[TIDE] updateTidePredictions called";

    if (!m_tideManager) {
        qDebug() << "[TIDE] ERROR: Tide manager is null in updateTidePredictions";
        return;
    }

    if (!m_tideManager->isInitialized()) {
        qDebug() << "[TIDE] ERROR: Tide manager not initialized in updateTidePredictions";
        return;
    }

    QDateTime startTime = QDateTime(m_startDateEdit->date(), QTime(0, 0));
    QDateTime endTime = QDateTime(m_endDateEdit->date(), QTime(23, 59));

    qDebug() << "[TIDE] Getting predictions from" << startTime.toString() << "to" << endTime.toString();
    m_predictions = m_tideManager->getTidePredictions(startTime, endTime, true);
    qDebug() << "[TIDE] Got" << m_predictions.size() << "predictions";

    m_predictionsList->clear();
    qDebug() << "[TIDE] Clearing prediction list, adding" << m_predictions.size() << "items";

    for (const TidePrediction &prediction : m_predictions) {
        QString displayText;
        QString details;

        if (prediction.isHighTide) {
            displayText = QString("High Tide: %1 - %2m")
                          .arg(prediction.dateTime.toString("dd.MM.yyyy hh:mm"))
                          .arg(prediction.height, 0, 'f', 2);
            details = QString("High Tide at %1\nStation: %2\nHeight: %3 meters\nPosition: %4, %5")
                     .arg(prediction.dateTime.toString("dd.MM.yyyy hh:mm:ss"))
                     .arg(prediction.stationName)
                     .arg(prediction.height, 0, 'f', 2)
                     .arg(prediction.stationLocation.latitude, 0, 'f', 6)
                     .arg(prediction.stationLocation.longitude, 0, 'f', 6);
        } else {
            displayText = QString("Low Tide: %1 - %2m")
                          .arg(prediction.dateTime.toString("dd.MM.yyyy hh:mm"))
                          .arg(prediction.height, 0, 'f', 2);
            details = QString("Low Tide at %1\nStation: %2\nHeight: %3 meters\nPosition: %4, %5")
                     .arg(prediction.dateTime.toString("dd.MM.yyyy hh:mm:ss"))
                     .arg(prediction.stationName)
                     .arg(prediction.height, 0, 'f', 2)
                     .arg(prediction.stationLocation.latitude, 0, 'f', 6)
                     .arg(prediction.stationLocation.longitude, 0, 'f', 6);
        }

        QListWidgetItem *item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, details);
        item->setForeground(prediction.isHighTide ? QColor(0, 100, 0) : QColor(100, 0, 0));
        m_predictionsList->addItem(item);
        qDebug() << "[TIDE] Added prediction item:" << displayText;
    }

    showStatus(QString("Loaded %1 tide predictions").arg(m_predictions.size()));
    qDebug() << "[TIDE] Update complete, list now has" << m_predictionsList->count() << "items";

    // Update current tide display
    updateCurrentTideDisplay();
}

void TidePanel::updateCurrentTideDisplay()
{
    qDebug() << "[TIDE] updateCurrentTideDisplay called";

    if (!m_tideManager) {
        qDebug() << "[TIDE] ERROR: Tide manager is null in updateCurrentTideDisplay";
        return;
    }

    if (!m_tideManager->isInitialized()) {
        qDebug() << "[TIDE] ERROR: Tide manager not initialized in updateCurrentTideDisplay";
        m_currentStationLabel->setText("No data loaded");
        m_currentTimeLabel->setText("--");
        m_currentHeightLabel->setText("--");
        m_currentTrendLabel->setText("--");
        return;
    }

    TidePrediction current = m_tideManager->getCurrentTide();
    if (current.dateTime.isValid()) {
        qDebug() << "[TIDE] Current tide valid - Station:" << current.stationName << "Height:" << current.height;
        m_currentStationLabel->setText(current.stationName);
        m_currentTimeLabel->setText(current.dateTime.toString("dd.MM.yyyy hh:mm"));
        m_currentHeightLabel->setText(QString("%1m").arg(current.height, 0, 'f', 2));

        // Simple trend calculation (would need more predictions for accurate trend)
        QString trend = "Rising"; // Default
        if (current.isHighTide) {
            trend = "Falling";
        }
        m_currentTrendLabel->setText(trend);
    } else {
        qDebug() << "[TIDE] Current tide is invalid";
        m_currentStationLabel->setText("No predictions available");
        m_currentTimeLabel->setText("--");
        m_currentHeightLabel->setText("--");
        m_currentTrendLabel->setText("--");
    }
}

void TidePanel::setControlsEnabled(bool enabled)
{
    m_latitudeSpinBox->setEnabled(enabled);
    m_longitudeSpinBox->setEnabled(enabled);
    m_useCurrentLocationButton->setEnabled(enabled && (m_currentLatitude != 0.0));
    m_stationComboBox->setEnabled(enabled);
    m_stationSearchEdit->setEnabled(enabled);
    m_startDateEdit->setEnabled(enabled);
    m_endDateEdit->setEnabled(enabled);
    m_refreshButton->setEnabled(enabled);
}

void TidePanel::showStatus(const QString &message)
{
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet("color: black;");
}

void TidePanel::showError(const QString &error)
{
    m_statusLabel->setText("Error: " + error);
    m_statusLabel->setStyleSheet("color: red;");
}