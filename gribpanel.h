#ifndef GRIBPANEL_H
#define GRIBPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QTimer>
#include <QTextEdit>
#include <QProgressBar>

#include "gribmanager.h"

// Forward declarations
class EcWidget;

/**
 * @brief Panel UI for GRIB Wave Data Viewer
 *
 * Provides:
 * - File loading and info display
 * - Playback controls for time animation
 * - Display options (heatmap, arrows, density)
 * - Color legend
 */
class GribPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GribPanel(EcWidget* ecWidget, GribManager* manager, QWidget *parent = nullptr);
    ~GribPanel();

    /**
     * @brief Update the panel display
     */
    void updateDisplay();

    /**
     * @brief Set the EcWidget for coordinate conversion
     */
    void setEcWidget(EcWidget* widget);

public slots:
    /**
     * @brief Called when file is loaded
     */
    void onFileLoaded(const QString& fileName);

    /**
     * @brief Called when file loading fails
     */
    void onLoadFailed(const QString& error);

    /**
     * @brief Called when time step changes
     */
    void onTimeStepChanged(int step);

    /**
     * @brief Called when data is cleared
     */
    void onDataCleared();

signals:
    /**
     * @brief Emitted when visualization settings change
     */
    void visualizationChanged();

    /**
     * @brief Emitted to request map update
     */
    void refreshRequested();

private slots:
    // File operations
    void onLoadFileClicked();
    void onCloseFileClicked();

    // Playback controls
    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    void onPreviousClicked();
    void onNextClicked();
    void onTimelineChanged(int value);
    void onSpeedChanged(int index);

    // Display options
    void onShowHeatmapChanged(int state);
    void onShowArrowsChanged(int state);
    void onArrowDensityChanged(int value);

    // Animation timer
    void onAnimationTimer();

private:
    void setupUI();
    void setupConnections();
    void updateFileDisplay();
    void updatePlaybackControls();
    void updateTimeline();
    void setControlsEnabled(bool enabled);
    void showStatus(const QString& message);

private:
    // Core components
    EcWidget* m_ecWidget;
    GribManager* m_manager;

    // Main layout
    QVBoxLayout* m_mainLayout;

    // File section
    QGroupBox* m_fileGroup;
    QPushButton* m_loadButton;
    QPushButton* m_closeButton;
    QLabel* m_fileLabel;
    QTextEdit* m_infoText;

    // Playback section
    QGroupBox* m_playbackGroup;
    QPushButton* m_playButton;
    QPushButton* m_pauseButton;
    QPushButton* m_stopButton;
    QPushButton* m_previousButton;
    QPushButton* m_nextButton;
    QSlider* m_timelineSlider;
    QLabel* m_timeLabel;
    QLabel* m_speedLabel;
    QComboBox* m_speedCombo;

    // Display options section
    QGroupBox* m_displayGroup;
    QCheckBox* m_showHeatmapCheck;
    QCheckBox* m_showArrowsCheck;
    QLabel* m_arrowDensityLabel;
    QSpinBox* m_arrowDensitySpin;

    // Status section
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;

    // Animation
    QTimer* m_animationTimer;
    bool m_isPlaying;
    int m_animationSpeed;  // milliseconds per frame
};

#endif // GRIBPANEL_H
