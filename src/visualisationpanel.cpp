#include "visualisationpanel.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QInputDialog>

VisualisationPanel::VisualisationPanel(QWidget *parent)
    : QDockWidget(parent)
    , m_showCurrents(true)
    , m_showTides(true)
    , m_currentScale(DEFAULT_CURRENT_SCALE)
    , m_tideScale(DEFAULT_TIDE_SCALE)
{
    setWindowTitle("Visualization Control Panel");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    setupUI();
    loadDefaultData();
}

VisualisationPanel::~VisualisationPanel()
{
}

void VisualisationPanel::setupUI()
{
    m_mainWidget = new QWidget();
    m_mainLayout = new QVBoxLayout(m_mainWidget);

    // Setup control groups
    setupCurrentControls();
    setupTideControls();

    // Add a separator
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_mainLayout->addWidget(line);

    // Setup data tables
    setupCurrentTable();
    setupTideTable();

    // Add spacer at bottom
    m_mainLayout->addStretch();

    setWidget(m_mainWidget);
}

void VisualisationPanel::setupCurrentControls()
{
    m_currentGroup = new QGroupBox("Ocean Currents");
    QVBoxLayout* layout = new QVBoxLayout(m_currentGroup);

    // Show/Hide checkbox
    m_showCurrentsCheck = new QCheckBox("Show Current Arrows");
    m_showCurrentsCheck->setChecked(m_showCurrents);
    connect(m_showCurrentsCheck, &QCheckBox::toggled,
            this, &VisualisationPanel::onShowCurrentsToggled);
    layout->addWidget(m_showCurrentsCheck);

    // Scale slider
    QHBoxLayout* scaleLayout = new QHBoxLayout();
    m_currentScaleLabel = new QLabel(QString("Scale: %1").arg(m_currentScale));
    m_currentScaleSlider = new QSlider(Qt::Horizontal);
    m_currentScaleSlider->setRange(10, SCALE_SLIDER_MAX);
    m_currentScaleSlider->setValue(static_cast<int>(m_currentScale));
    connect(m_currentScaleSlider, &QSlider::valueChanged,
            this, &VisualisationPanel::onCurrentScaleChanged);
    scaleLayout->addWidget(m_currentScaleLabel);
    scaleLayout->addWidget(m_currentScaleSlider);
    layout->addLayout(scaleLayout);

    // Control buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_currentLoadBtn = new QPushButton("Load");
    m_currentSaveBtn = new QPushButton("Save");
    m_currentClearBtn = new QPushButton("Clear");
    m_currentAddBtn = new QPushButton("Add");

    connect(m_currentLoadBtn, &QPushButton::clicked,
            this, &VisualisationPanel::loadCurrentData);
    connect(m_currentSaveBtn, &QPushButton::clicked,
            this, &VisualisationPanel::saveCurrentData);
    connect(m_currentClearBtn, &QPushButton::clicked,
            this, &VisualisationPanel::clearCurrentData);
    connect(m_currentAddBtn, &QPushButton::clicked,
            this, &VisualisationPanel::addCurrentStation);

    btnLayout->addWidget(m_currentLoadBtn);
    btnLayout->addWidget(m_currentSaveBtn);
    btnLayout->addWidget(m_currentClearBtn);
    btnLayout->addWidget(m_currentAddBtn);
    layout->addLayout(btnLayout);

    m_mainLayout->addWidget(m_currentGroup);
}

void VisualisationPanel::setupTideControls()
{
    m_tideGroup = new QGroupBox("Tide Stations");
    QVBoxLayout* layout = new QVBoxLayout(m_tideGroup);

    // Show/Hide checkbox
    m_showTidesCheck = new QCheckBox("Show Tide Rectangles");
    m_showTidesCheck->setChecked(m_showTides);
    connect(m_showTidesCheck, &QCheckBox::toggled,
            this, &VisualisationPanel::onShowTidesToggled);
    layout->addWidget(m_showTidesCheck);

    // Scale slider
    QHBoxLayout* scaleLayout = new QHBoxLayout();
    m_tideScaleLabel = new QLabel(QString("Scale: %1").arg(m_tideScale));
    m_tideScaleSlider = new QSlider(Qt::Horizontal);
    m_tideScaleSlider->setRange(5, SCALE_SLIDER_MAX);
    m_tideScaleSlider->setValue(static_cast<int>(m_tideScale));
    connect(m_tideScaleSlider, &QSlider::valueChanged,
            this, &VisualisationPanel::onTideScaleChanged);
    scaleLayout->addWidget(m_tideScaleLabel);
    scaleLayout->addWidget(m_tideScaleSlider);
    layout->addLayout(scaleLayout);

    // Control buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_tideLoadBtn = new QPushButton("Load");
    m_tideSaveBtn = new QPushButton("Save");
    m_tideClearBtn = new QPushButton("Clear");
    m_tideAddBtn = new QPushButton("Add");

    connect(m_tideLoadBtn, &QPushButton::clicked,
            this, &VisualisationPanel::loadTideData);
    connect(m_tideSaveBtn, &QPushButton::clicked,
            this, &VisualisationPanel::saveTideData);
    connect(m_tideClearBtn, &QPushButton::clicked,
            this, &VisualisationPanel::clearTideData);
    connect(m_tideAddBtn, &QPushButton::clicked,
            this, &VisualisationPanel::addTideVisualization);

    btnLayout->addWidget(m_tideLoadBtn);
    btnLayout->addWidget(m_tideSaveBtn);
    btnLayout->addWidget(m_tideClearBtn);
    btnLayout->addWidget(m_tideAddBtn);
    layout->addLayout(btnLayout);

    m_mainLayout->addWidget(m_tideGroup);
}

void VisualisationPanel::setupCurrentTable()
{
    // Current table group
    QGroupBox* tableGroup = new QGroupBox("Current Stations Data");
    QVBoxLayout* layout = new QVBoxLayout(tableGroup);

    // Create table
    m_currentTable = new QTableWidget(0, 7);
    QStringList headers;
    headers << "ID" << "Name" << "Lat" << "Lon" << "Speed (kn)" << "Dir (°)" << "Active";
    m_currentTable->setHorizontalHeaderLabels(headers);
    m_currentTable->horizontalHeader()->setStretchLastSection(true);
    m_currentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_currentTable->setSortingEnabled(true);

    connect(m_currentTable, &QTableWidget::itemChanged,
            this, &VisualisationPanel::onCurrentTableItemChanged);
    connect(m_currentTable, &QTableWidget::itemSelectionChanged,
            this, &VisualisationPanel::onCurrentTableSelectionChanged);

    layout->addWidget(m_currentTable);

    // Table control buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_currentRemoveBtn = new QPushButton("Remove Selected");
    m_currentRefreshBtn = new QPushButton("Refresh");
    m_currentRemoveBtn->setEnabled(false);

    connect(m_currentRemoveBtn, &QPushButton::clicked,
            [this]() {
                int row = m_currentTable->currentRow();
                if (row >= 0) {
                    m_currentStations.removeAt(row);
                    populateCurrentTable();
                    emit dataUpdated();
                }
            });

    connect(m_currentRefreshBtn, &QPushButton::clicked,
            this, &VisualisationPanel::refreshVisualization);

    btnLayout->addWidget(m_currentRemoveBtn);
    btnLayout->addWidget(m_currentRefreshBtn);
    btnLayout->addStretch();

    layout->addLayout(btnLayout);
    m_mainLayout->addWidget(tableGroup);
}

void VisualisationPanel::setupTideTable()
{
    // Tide table group
    QGroupBox* tableGroup = new QGroupBox("Tide Stations Data");
    QVBoxLayout* layout = new QVBoxLayout(tableGroup);

    // Create table
    m_tideTable = new QTableWidget(0, 7);
    QStringList headers;
    headers << "Station ID" << "Name" << "Lat" << "Lon" << "Height (m)" << "Type" << "Time";
    m_tideTable->setHorizontalHeaderLabels(headers);
    m_tideTable->horizontalHeader()->setStretchLastSection(true);
    m_tideTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tideTable->setSortingEnabled(true);

    connect(m_tideTable, &QTableWidget::itemChanged,
            this, &VisualisationPanel::onTideTableItemChanged);
    connect(m_tideTable, &QTableWidget::itemSelectionChanged,
            this, &VisualisationPanel::onTideTableSelectionChanged);

    layout->addWidget(m_tideTable);

    // Table control buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_tideRemoveBtn = new QPushButton("Remove Selected");
    m_tideRefreshBtn = new QPushButton("Refresh");
    m_tideRemoveBtn->setEnabled(false);

    connect(m_tideRemoveBtn, &QPushButton::clicked,
            [this]() {
                int row = m_tideTable->currentRow();
                if (row >= 0) {
                    m_tideVisualizations.removeAt(row);
                    populateTideTable();
                    emit dataUpdated();
                }
            });

    connect(m_tideRefreshBtn, &QPushButton::clicked,
            this, &VisualisationPanel::refreshVisualization);

    btnLayout->addWidget(m_tideRemoveBtn);
    btnLayout->addWidget(m_tideRefreshBtn);
    btnLayout->addStretch();

    layout->addLayout(btnLayout);
    m_mainLayout->addWidget(tableGroup);
}

void VisualisationPanel::loadDefaultData()
{
    // Load sample data for testing
    CurrentVisualisation temp(nullptr);
    m_currentStations = temp.generateSampleCurrentData();
    m_tideVisualizations = temp.generateSampleTideData();

    populateCurrentTable();
    populateTideTable();
}

void VisualisationPanel::populateCurrentTable()
{
    m_currentTable->setRowCount(m_currentStations.size());

    for (int i = 0; i < m_currentStations.size(); ++i) {
        updateCurrentRow(i);
    }
}

void VisualisationPanel::updateCurrentRow(int row)
{
    if (row < 0 || row >= m_currentStations.size()) return;

    const auto& station = m_currentStations[row];

    // Block signals to prevent recursive updates
    m_currentTable->blockSignals(true);

    m_currentTable->setItem(row, 0, new QTableWidgetItem(station.id));
    m_currentTable->setItem(row, 1, new QTableWidgetItem(station.name));
    m_currentTable->setItem(row, 2, new QTableWidgetItem(QString::number(station.latitude, 'f', 6)));
    m_currentTable->setItem(row, 3, new QTableWidgetItem(QString::number(station.longitude, 'f', 6)));
    m_currentTable->setItem(row, 4, new QTableWidgetItem(QString::number(station.speed, 'f', 2)));
    m_currentTable->setItem(row, 5, new QTableWidgetItem(QString::number(station.direction, 'f', 1)));

    QTableWidgetItem* activeItem = new QTableWidgetItem();
    activeItem->setCheckState(station.isActive ? Qt::Checked : Qt::Unchecked);
    m_currentTable->setItem(row, 6, activeItem);

    m_currentTable->blockSignals(false);
}

void VisualisationPanel::populateTideTable()
{
    m_tideTable->setRowCount(m_tideVisualizations.size());

    for (int i = 0; i < m_tideVisualizations.size(); ++i) {
        updateTideRow(i);
    }
}

void VisualisationPanel::updateTideRow(int row)
{
    if (row < 0 || row >= m_tideVisualizations.size()) return;

    const auto& tide = m_tideVisualizations[row];

    // Block signals to prevent recursive updates
    m_tideTable->blockSignals(true);

    m_tideTable->setItem(row, 0, new QTableWidgetItem(tide.stationId));
    m_tideTable->setItem(row, 1, new QTableWidgetItem(tide.stationName));
    m_tideTable->setItem(row, 2, new QTableWidgetItem(QString::number(tide.latitude, 'f', 6)));
    m_tideTable->setItem(row, 3, new QTableWidgetItem(QString::number(tide.longitude, 'f', 6)));
    m_tideTable->setItem(row, 4, new QTableWidgetItem(QString::number(tide.height, 'f', 2)));
    m_tideTable->setItem(row, 5, new QTableWidgetItem(tide.isHighTide ? "High" : "Low"));
    m_tideTable->setItem(row, 6, new QTableWidgetItem(tide.timestamp.toString("hh:mm")));

    m_tideTable->blockSignals(false);
}

// Slot implementations
void VisualisationPanel::onShowCurrentsToggled(bool checked)
{
    m_showCurrents = checked;
    emit settingsChanged();
}

void VisualisationPanel::onShowTidesToggled(bool checked)
{
    m_showTides = checked;
    emit settingsChanged();
}

void VisualisationPanel::onCurrentScaleChanged(int value)
{
    m_currentScale = value;
    m_currentScaleLabel->setText(QString("Scale: %1").arg(m_currentScale));
    emit settingsChanged();
}

void VisualisationPanel::onTideScaleChanged(int value)
{
    m_tideScale = value;
    m_tideScaleLabel->setText(QString("Scale: %1").arg(m_tideScale));
    emit settingsChanged();
}

void VisualisationPanel::onCurrentTableItemChanged(QTableWidgetItem* item)
{
    if (!item) return;

    int row = item->row();
    if (row < 0 || row >= m_currentStations.size()) return;

    auto& station = m_currentStations[row];

    switch (item->column()) {
    case 0: station.id = item->text(); break;
    case 1: station.name = item->text(); break;
    case 2: station.latitude = item->text().toDouble(); break;
    case 3: station.longitude = item->text().toDouble(); break;
    case 4: station.speed = item->text().toDouble(); break;
    case 5: station.direction = item->text().toDouble(); break;
    case 6: station.isActive = (item->checkState() == Qt::Checked); break;
    }

    emit dataUpdated();
}

void VisualisationPanel::onTideTableItemChanged(QTableWidgetItem* item)
{
    if (!item) return;

    int row = item->row();
    if (row < 0 || row >= m_tideVisualizations.size()) return;

    auto& tide = m_tideVisualizations[row];

    switch (item->column()) {
    case 0: tide.stationId = item->text(); break;
    case 1: tide.stationName = item->text(); break;
    case 2: tide.latitude = item->text().toDouble(); break;
    case 3: tide.longitude = item->text().toDouble(); break;
    case 4: tide.height = item->text().toDouble(); break;
    case 5: tide.isHighTide = (item->text() == "High"); break;
    }

    emit dataUpdated();
}

void VisualisationPanel::onCurrentTableSelectionChanged()
{
    bool hasSelection = m_currentTable->currentRow() >= 0;
    m_currentRemoveBtn->setEnabled(hasSelection);
}

void VisualisationPanel::onTideTableSelectionChanged()
{
    bool hasSelection = m_tideTable->currentRow() >= 0;
    m_tideRemoveBtn->setEnabled(hasSelection);
}

// Public slot implementations
void VisualisationPanel::setShowCurrents(bool show)
{
    m_showCurrents = show;
    m_showCurrentsCheck->setChecked(show);
}

void VisualisationPanel::setShowTides(bool show)
{
    m_showTides = show;
    m_showTidesCheck->setChecked(show);
}

void VisualisationPanel::setCurrentScale(double scale)
{
    m_currentScale = scale;
    m_currentScaleSlider->setValue(static_cast<int>(scale));
    m_currentScaleLabel->setText(QString("Scale: %1").arg(m_currentScale));
}

void VisualisationPanel::setTideScale(double scale)
{
    m_tideScale = scale;
    m_tideScaleSlider->setValue(static_cast<int>(scale));
    m_tideScaleLabel->setText(QString("Scale: %1").arg(m_tideScale));
}

void VisualisationPanel::updateCurrentData(const QList<CurrentStation>& stations)
{
    m_currentStations = stations;
    populateCurrentTable();
}

void VisualisationPanel::updateTideData(const QList<TideVisualization>& tides)
{
    m_tideVisualizations = tides;
    populateTideTable();
}

void VisualisationPanel::loadCurrentData()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Load Current Data", "", "JSON Files (*.json)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);

            if (error.error == QJsonParseError::NoError) {
                QJsonArray array = doc.array();
                m_currentStations.clear();

                for (const auto& value : array) {
                    QJsonObject obj = value.toObject();
                    CurrentStation station;
                    station.id = obj["id"].toString();
                    station.name = obj["name"].toString();
                    station.latitude = obj["latitude"].toDouble();
                    station.longitude = obj["longitude"].toDouble();
                    station.speed = obj["speed"].toDouble();
                    station.direction = obj["direction"].toDouble();
                    station.isActive = obj["active"].toBool(true);

                    m_currentStations.append(station);
                }

                populateCurrentTable();
                emit dataUpdated();
            }
        }
    }
}

void VisualisationPanel::saveCurrentData()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Current Data", "", "JSON Files (*.json)");

    if (!fileName.isEmpty()) {
        QJsonArray array;

        for (const auto& station : m_currentStations) {
            QJsonObject obj;
            obj["id"] = station.id;
            obj["name"] = station.name;
            obj["latitude"] = station.latitude;
            obj["longitude"] = station.longitude;
            obj["speed"] = station.speed;
            obj["direction"] = station.direction;
            obj["active"] = station.isActive;

            array.append(obj);
        }

        QJsonDocument doc(array);
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
        }
    }
}

void VisualisationPanel::clearCurrentData()
{
    if (QMessageBox::question(this, "Confirm Clear",
                              "Clear all current station data?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_currentStations.clear();
        populateCurrentTable();
        emit dataUpdated();
    }
}

void VisualisationPanel::addCurrentStation()
{
    CurrentStation station;
    station.id = QString("CUR%1").arg(m_currentStations.size() + 1, 3, 10, QChar('0'));
    station.name = "New Station";
    station.latitude = -6.0;
    station.longitude = 106.0;
    station.speed = 1.0;
    station.direction = 0.0;
    station.isActive = true;

    m_currentStations.append(station);
    populateCurrentTable();
    emit dataUpdated();
}

void VisualisationPanel::loadTideData()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Load Tide Data", "", "JSON Files (*.json)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);

            if (error.error == QJsonParseError::NoError) {
                QJsonArray array = doc.array();
                m_tideVisualizations.clear();

                for (const auto& value : array) {
                    QJsonObject obj = value.toObject();
                    TideVisualization tide;
                    tide.stationId = obj["stationId"].toString();
                    tide.stationName = obj["stationName"].toString();
                    tide.latitude = obj["latitude"].toDouble();
                    tide.longitude = obj["longitude"].toDouble();
                    tide.height = obj["height"].toDouble();
                    tide.isHighTide = obj["isHighTide"].toBool();

                    m_tideVisualizations.append(tide);
                }

                populateTideTable();
                emit dataUpdated();
            }
        }
    }
}

void VisualisationPanel::saveTideData()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Tide Data", "", "JSON Files (*.json)");

    if (!fileName.isEmpty()) {
        QJsonArray array;

        for (const auto& tide : m_tideVisualizations) {
            QJsonObject obj;
            obj["stationId"] = tide.stationId;
            obj["stationName"] = tide.stationName;
            obj["latitude"] = tide.latitude;
            obj["longitude"] = tide.longitude;
            obj["height"] = tide.height;
            obj["isHighTide"] = tide.isHighTide;

            array.append(obj);
        }

        QJsonDocument doc(array);
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
        }
    }
}

void VisualisationPanel::clearTideData()
{
    if (QMessageBox::question(this, "Confirm Clear",
                              "Clear all tide station data?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_tideVisualizations.clear();
        populateTideTable();
        emit dataUpdated();
    }
}

void VisualisationPanel::addTideVisualization()
{
    TideVisualization tide;
    tide.stationId = "TIDE001";
    tide.stationName = "New Tide Station";
    tide.latitude = -6.0;
    tide.longitude = 106.0;
    tide.height = 1.5;
    tide.isHighTide = true;
    tide.timestamp = QDateTime::currentDateTime();

    m_tideVisualizations.append(tide);
    populateTideTable();
    emit dataUpdated();
}

void VisualisationPanel::refreshVisualization()
{
    emit dataUpdated();
}