#ifndef VISUALISATIONPANEL_H
#define VISUALISATIONPANEL_H

#include <QWidget>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>

#include "currentvisualisation.h"

class VisualisationPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit VisualisationPanel(QWidget *parent = nullptr);
    ~VisualisationPanel();

    // Get visualization settings
    bool getShowCurrents() const { return m_showCurrents; }
    bool getShowTides() const { return m_showTides; }
    double getCurrentScale() const { return m_currentScale; }
    double getTideScale() const { return m_tideScale; }

    // Get current data
    QList<CurrentStation> getCurrentStations() const { return m_currentStations; }
    QList<TideVisualization> getTideVisualizations() const { return m_tideVisualizations; }

signals:
    void settingsChanged();
    void dataUpdated();
    void loadFromFile();
    void saveToFile();

public slots:
    void setShowCurrents(bool show);
    void setShowTides(bool show);
    void setCurrentScale(double scale);
    void setTideScale(double scale);

    void updateCurrentData(const QList<CurrentStation>& stations);
    void updateTideData(const QList<TideVisualization>& tides);

    void loadCurrentData();
    void saveCurrentData();
    void clearCurrentData();
    void addCurrentStation();

    void loadTideData();
    void saveTideData();
    void clearTideData();
    void addTideVisualization();

    void refreshVisualization();

private slots:
    void onShowCurrentsToggled(bool checked);
    void onShowTidesToggled(bool checked);
    void onCurrentScaleChanged(int value);
    void onTideScaleChanged(int value);
    void onCurrentTableItemChanged(QTableWidgetItem* item);
    void onTideTableItemChanged(QTableWidgetItem* item);
    void onCurrentTableSelectionChanged();
    void onTideTableSelectionChanged();

private:
    void setupUI();
    void setupCurrentControls();
    void setupTideControls();
    void setupCurrentTable();
    void setupTideTable();

    void updateCurrentRow(int row);
    void updateTideRow(int row);
    void populateCurrentTable();
    void populateTideTable();
    void loadDefaultData();

    // UI Components
    QWidget* m_mainWidget;
    QVBoxLayout* m_mainLayout;

    // Current controls group
    QGroupBox* m_currentGroup;
    QCheckBox* m_showCurrentsCheck;
    QLabel* m_currentScaleLabel;
    QSlider* m_currentScaleSlider;
    QPushButton* m_currentLoadBtn;
    QPushButton* m_currentSaveBtn;
    QPushButton* m_currentClearBtn;
    QPushButton* m_currentAddBtn;

    // Tide controls group
    QGroupBox* m_tideGroup;
    QCheckBox* m_showTidesCheck;
    QLabel* m_tideScaleLabel;
    QSlider* m_tideScaleSlider;
    QPushButton* m_tideLoadBtn;
    QPushButton* m_tideSaveBtn;
    QPushButton* m_tideClearBtn;
    QPushButton* m_tideAddBtn;

    // Current data table
    QTableWidget* m_currentTable;
    QPushButton* m_currentRemoveBtn;
    QPushButton* m_currentRefreshBtn;

    // Tide data table
    QTableWidget* m_tideTable;
    QPushButton* m_tideRemoveBtn;
    QPushButton* m_tideRefreshBtn;

    // Data storage
    QList<CurrentStation> m_currentStations;
    QList<TideVisualization> m_tideVisualizations;

    // Settings
    bool m_showCurrents;
    bool m_showTides;
    double m_currentScale;
    double m_tideScale;

    // Constants
    static constexpr int SCALE_SLIDER_MAX = 100;
    static constexpr double DEFAULT_CURRENT_SCALE = 20.0;
    static constexpr double DEFAULT_TIDE_SCALE = 10.0;
};

#endif // VISUALISATIONPANEL_H