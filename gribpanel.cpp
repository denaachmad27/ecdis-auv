#include "gribpanel.h"
#include "ecwidget.h"
#include <QFileDialog>
#include <QDebug>
#include <QGroupBox>

GribPanel::GribPanel(EcWidget* ecWidget, GribManager* manager, QWidget *parent)
    : QWidget(parent)
    , m_ecWidget(ecWidget)
    , m_manager(manager)
    , m_mainLayout(nullptr)
    , m_isPlaying(false)
{
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(1000);  // Default 1 second per frame

    setupUI();
    setupConnections();

    setControlsEnabled(false);
}

GribPanel::~GribPanel()
{
    if (m_animationTimer->isActive()) {
        m_animationTimer->stop();
    }
}

void GribPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);

    // ========== File Section ==========
    m_fileGroup = new QGroupBox(tr("GRIB File"), this);
    QVBoxLayout* fileLayout = new QVBoxLayout(m_fileGroup);

    m_fileLabel = new QLabel(tr("No file loaded"), m_fileGroup);
    m_fileLabel->setWordWrap(true);
    m_fileLabel->setStyleSheet("QLabel { font-weight: bold; color: #666; }");
    fileLayout->addWidget(m_fileLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_loadButton = new QPushButton(tr("Load..."), m_fileGroup);
    m_loadButton->setIcon(QIcon(":/icons/folder.png"));
    m_closeButton = new QPushButton(tr("Close"), m_fileGroup);
    m_closeButton->setEnabled(false);
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_closeButton);
    fileLayout->addLayout(buttonLayout);

    m_infoText = new QTextEdit(m_fileGroup);
    m_infoText->setReadOnly(true);
    m_infoText->setMaximumHeight(120);
    m_infoText->setStyleSheet("QTextEdit { font-size: 10px; background: #f5f5f5; }");
    fileLayout->addWidget(m_infoText);

    m_mainLayout->addWidget(m_fileGroup);

    // ========== Playback Section ==========
    m_playbackGroup = new QGroupBox(tr("Playback Controls"), this);
    QVBoxLayout* playbackLayout = new QVBoxLayout(m_playbackGroup);

    // Timeline
    m_timeLabel = new QLabel(tr("Time: --"), m_playbackGroup);
    playbackLayout->addWidget(m_timeLabel);

    m_timelineSlider = new QSlider(Qt::Horizontal, m_playbackGroup);
    m_timelineSlider->setRange(0, 0);
    m_timelineSlider->setValue(0);
    playbackLayout->addWidget(m_timelineSlider);

    // Control buttons
    QHBoxLayout* controlLayout = new QHBoxLayout();
    m_previousButton = new QPushButton(tr("|<"), m_playbackGroup);
    m_previousButton->setToolTip(tr("Previous step"));
    m_previousButton->setMaximumWidth(40);
    m_playButton = new QPushButton(tr("▶"), m_playbackGroup);
    m_playButton->setToolTip(tr("Play"));
    m_playButton->setMaximumWidth(40);
    m_pauseButton = new QPushButton(tr("||"), m_playbackGroup);
    m_pauseButton->setToolTip(tr("Pause"));
    m_pauseButton->setMaximumWidth(40);
    m_pauseButton->setEnabled(false);
    m_stopButton = new QPushButton(tr("■"), m_playbackGroup);
    m_stopButton->setToolTip(tr("Stop"));
    m_stopButton->setMaximumWidth(40);
    m_stopButton->setEnabled(false);
    m_nextButton = new QPushButton(tr(">|"), m_playbackGroup);
    m_nextButton->setToolTip(tr("Next step"));
    m_nextButton->setMaximumWidth(40);

    controlLayout->addWidget(m_previousButton);
    controlLayout->addWidget(m_playButton);
    controlLayout->addWidget(m_pauseButton);
    controlLayout->addWidget(m_stopButton);
    controlLayout->addWidget(m_nextButton);
    playbackLayout->addLayout(controlLayout);

    // Speed control
    QHBoxLayout* speedLayout = new QHBoxLayout();
    m_speedLabel = new QLabel(tr("Speed:"), m_playbackGroup);
    m_speedCombo = new QComboBox(m_playbackGroup);
    m_speedCombo->addItem(tr("0.5x"), 2000);
    m_speedCombo->addItem(tr("1x"), 1000);
    m_speedCombo->addItem(tr("2x"), 500);
    m_speedCombo->addItem(tr("5x"), 200);
    m_speedCombo->setCurrentIndex(1);  // Default 1x
    speedLayout->addWidget(m_speedLabel);
    speedLayout->addWidget(m_speedCombo);
    speedLayout->addStretch();
    playbackLayout->addLayout(speedLayout);

    m_mainLayout->addWidget(m_playbackGroup);

    // ========== Display Options Section ==========
    m_displayGroup = new QGroupBox(tr("Display Options"), this);
    QVBoxLayout* displayLayout = new QVBoxLayout(m_displayGroup);

    m_showHeatmapCheck = new QCheckBox(tr("Show Wave Height (Heatmap)"), m_displayGroup);
    m_showHeatmapCheck->setChecked(true);
    displayLayout->addWidget(m_showHeatmapCheck);

    m_showArrowsCheck = new QCheckBox(tr("Show Wave Direction (Arrows)"), m_displayGroup);
    m_showArrowsCheck->setChecked(true);
    displayLayout->addWidget(m_showArrowsCheck);

    QHBoxLayout* densityLayout = new QHBoxLayout();
    m_arrowDensityLabel = new QLabel(tr("Arrow Density:"), m_displayGroup);
    m_arrowDensitySpin = new QSpinBox(m_displayGroup);
    m_arrowDensitySpin->setRange(1, 20);
    m_arrowDensitySpin->setValue(5);
    m_arrowDensitySpin->setSuffix(tr(" grid"));
    m_arrowDensitySpin->setToolTip(tr("Higher values = fewer arrows (1 = all, 5 = every 5th point)"));
    densityLayout->addWidget(m_arrowDensityLabel);
    densityLayout->addWidget(m_arrowDensitySpin);
    densityLayout->addStretch();
    displayLayout->addLayout(densityLayout);

    m_mainLayout->addWidget(m_displayGroup);

    // ========== Status Section ==========
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    m_mainLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumHeight(3);
    m_mainLayout->addWidget(m_progressBar);

    m_mainLayout->addStretch();
}

void GribPanel::setupConnections()
{
    // File operations
    connect(m_loadButton, &QPushButton::clicked, this, &GribPanel::onLoadFileClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &GribPanel::onCloseFileClicked);

    // Playback controls
    connect(m_playButton, &QPushButton::clicked, this, &GribPanel::onPlayClicked);
    connect(m_pauseButton, &QPushButton::clicked, this, &GribPanel::onPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &GribPanel::onStopClicked);
    connect(m_previousButton, &QPushButton::clicked, this, &GribPanel::onPreviousClicked);
    connect(m_nextButton, &QPushButton::clicked, this, &GribPanel::onNextClicked);
    connect(m_timelineSlider, &QSlider::valueChanged, this, &GribPanel::onTimelineChanged);
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GribPanel::onSpeedChanged);

    // Display options
    connect(m_showHeatmapCheck, &QCheckBox::stateChanged, this, &GribPanel::onShowHeatmapChanged);
    connect(m_showArrowsCheck, &QCheckBox::stateChanged, this, &GribPanel::onShowArrowsChanged);
    connect(m_arrowDensitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &GribPanel::onArrowDensityChanged);

    // Animation timer
    connect(m_animationTimer, &QTimer::timeout, this, &GribPanel::onAnimationTimer);

    // Manager signals
    if (m_manager) {
        connect(m_manager, &GribManager::fileLoaded, this, &GribPanel::onFileLoaded);
        connect(m_manager, &GribManager::loadFailed, this, &GribPanel::onLoadFailed);
        connect(m_manager, &GribManager::timeStepChanged, this, &GribPanel::onTimeStepChanged);
        connect(m_manager, &GribManager::dataCleared, this, &GribPanel::onDataCleared);
    }
}

void GribPanel::setEcWidget(EcWidget* widget)
{
    m_ecWidget = widget;
}

void GribPanel::onLoadFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open GRIB File"),
        QString(),
        tr("GRIB Files (*.grb *.grb2 *.grib *.gb1 *.gb2);;All Files (*)")
    );

    if (!filePath.isEmpty()) {
        showStatus(tr("Loading file..."));
        m_progressBar->setVisible(true);
        m_progressBar->setRange(0, 0);  // Indeterminate progress

        if (m_manager) {
            m_manager->loadFromFile(filePath);
        }
    }
}

void GribPanel::onCloseFileClicked()
{
    if (m_manager) {
        m_manager->clear();
    }
    onStopClicked();
    setControlsEnabled(false);
    m_fileLabel->setText(tr("No file loaded"));
    m_infoText->clear();
    showStatus(tr("File closed"));
}

void GribPanel::onPlayClicked()
{
    if (!m_manager || m_manager->getTimeStepCount() <= 1) {
        return;
    }

    m_isPlaying = true;
    m_playButton->setEnabled(false);
    m_pauseButton->setEnabled(true);
    m_stopButton->setEnabled(true);

    m_animationTimer->start();
    showStatus(tr("Playing..."));
}

void GribPanel::onPauseClicked()
{
    m_isPlaying = false;
    m_playButton->setEnabled(true);
    m_pauseButton->setEnabled(false);

    if (m_animationTimer->isActive()) {
        m_animationTimer->stop();
        showStatus(tr("Paused"));
    }
}

void GribPanel::onStopClicked()
{
    m_isPlaying = false;
    m_playButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);

    if (m_animationTimer->isActive()) {
        m_animationTimer->stop();
    }

    // Reset to first time step
    if (m_manager) {
        m_manager->setCurrentTimeStep(0);
    }
    showStatus(tr("Stopped"));
}

void GribPanel::onPreviousClicked()
{
    if (!m_manager) return;

    int current = m_manager->getCurrentTimeStep();
    if (current > 0) {
        m_manager->setCurrentTimeStep(current - 1);
    }
}

void GribPanel::onNextClicked()
{
    if (!m_manager) return;

    int current = m_manager->getCurrentTimeStep();
    if (current < m_manager->getTimeStepCount() - 1) {
        m_manager->setCurrentTimeStep(current + 1);
    }
}

void GribPanel::onTimelineChanged(int value)
{
    if (m_manager) {
        m_manager->setCurrentTimeStep(value);
    }
}

void GribPanel::onSpeedChanged(int index)
{
    int interval = m_speedCombo->itemData(index).toInt();
    m_animationTimer->setInterval(interval);
    showStatus(tr("Speed: %1").arg(m_speedCombo->currentText()));
}

void GribPanel::onShowHeatmapChanged(int state)
{
    if (m_manager) {
        m_manager->setShowHeatmap(state == Qt::Checked);
        emit visualizationChanged();
        emit refreshRequested();
    }
}

void GribPanel::onShowArrowsChanged(int state)
{
    if (m_manager) {
        m_manager->setShowArrows(state == Qt::Checked);
        emit visualizationChanged();
        emit refreshRequested();
    }
}

void GribPanel::onArrowDensityChanged(int value)
{
    if (m_manager) {
        m_manager->setArrowDensity(value);
        emit visualizationChanged();
        emit refreshRequested();
    }
}

void GribPanel::onAnimationTimer()
{
    if (!m_manager || !m_isPlaying) {
        return;
    }

    int current = m_manager->getCurrentTimeStep();
    int maxStep = m_manager->getTimeStepCount() - 1;

    if (current >= maxStep) {
        // Loop back to start or stop
        m_manager->setCurrentTimeStep(0);
    } else {
        m_manager->setCurrentTimeStep(current + 1);
    }
}

void GribPanel::onFileLoaded(const QString& fileName)
{
    m_progressBar->setVisible(false);
    setControlsEnabled(true);
    updateFileDisplay();
    updateTimeline();
    showStatus(tr("File loaded: %1").arg(fileName));
    emit refreshRequested();
}

void GribPanel::onLoadFailed(const QString& error)
{
    m_progressBar->setVisible(false);
    m_statusLabel->setStyleSheet("QLabel { color: red; }");
    showStatus(tr("Error: %1").arg(error));
}

void GribPanel::onTimeStepChanged(int step)
{
    // Block signals to prevent feedback loop
    m_timelineSlider->blockSignals(true);
    m_timelineSlider->setValue(step);
    m_timelineSlider->blockSignals(false);

    if (m_manager) {
        QStringList labels = m_manager->getTimeStepLabels();
        if (step >= 0 && step < labels.size()) {
            m_timeLabel->setText(tr("Time: %1").arg(labels[step]));
        }
    }

    emit refreshRequested();
}

void GribPanel::onDataCleared()
{
    setControlsEnabled(false);
    m_timelineSlider->setRange(0, 0);
    m_timelineSlider->setValue(0);
    m_timeLabel->setText(tr("Time: --"));
    m_infoText->clear();
    emit refreshRequested();
}

void GribPanel::updateFileDisplay()
{
    if (!m_manager) return;

    m_fileLabel->setText(m_manager->getData().fileName);
    m_infoText->setPlainText(m_manager->getFileInfo());
}

void GribPanel::updateTimeline()
{
    if (!m_manager) return;

    int count = m_manager->getTimeStepCount();
    m_timelineSlider->setRange(0, qMax(0, count - 1));
    m_timelineSlider->setValue(0);

    // Update time label
    QStringList labels = m_manager->getTimeStepLabels();
    if (!labels.isEmpty()) {
        m_timeLabel->setText(tr("Time: %1").arg(labels.first()));
    }
}

void GribPanel::updateDisplay()
{
    updateFileDisplay();
    updateTimeline();
}

void GribPanel::updatePlaybackControls()
{
    bool hasData = m_manager && m_manager->isLoaded() && m_manager->getTimeStepCount() > 1;
    m_playButton->setEnabled(hasData && !m_isPlaying);
    m_pauseButton->setEnabled(hasData && m_isPlaying);
    m_stopButton->setEnabled(hasData && m_isPlaying);
    m_previousButton->setEnabled(hasData);
    m_nextButton->setEnabled(hasData);
    m_timelineSlider->setEnabled(hasData);
}

void GribPanel::setControlsEnabled(bool enabled)
{
    m_closeButton->setEnabled(enabled);
    m_playButton->setEnabled(enabled && !m_isPlaying);
    m_pauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    m_previousButton->setEnabled(enabled);
    m_nextButton->setEnabled(enabled);
    m_timelineSlider->setEnabled(enabled);
    m_speedCombo->setEnabled(enabled);
}

void GribPanel::showStatus(const QString& message)
{
    m_statusLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    m_statusLabel->setText(message);
}
