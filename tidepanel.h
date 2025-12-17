#ifndef TIDEPANEL_H
#define TIDEPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTimer>
#include <QListWidgetItem>

#include "tidemanager.h"

class TidalCurveWidgetSimple;

class TidePanel : public QWidget
{
    Q_OBJECT

public:
    explicit TidePanel(QWidget *parent = nullptr);
    ~TidePanel();

    void setTideManager(TideManager *tideManager);
    void updateCurrentPosition(double latitude, double longitude);

signals:
    void stationSelected(const QString &stationName, double latitude, double longitude);
    void locationUpdated(double latitude, double longitude);

public slots:
    void updateTidePredictions();

private:
    void createTideBars(const QList<TidePrediction> &predictions);

private slots:
    void onLoadDataClicked();
    void onRefreshClicked();
    void onStationComboBoxChanged(int index);
    void onLocationChanged();
    void onDateRangeChanged();
    void onTideDataLoaded();
    void onPredictionsUpdated();
    void onErrorOccurred(const QString &error);
    void updateCurrentTide();
    void onStationSearchTextChanged(const QString &text);
    void onUseCurrentLocationClicked();

private:
    void refreshStationList();
    void updateCurrentTideDisplay();
    void setControlsEnabled(bool enabled);
    void showStatus(const QString &message);
    void showError(const QString &error);

    // SUPER SIMPLE UI COMPONENTS
    QLabel *m_statusLabel;
    QListWidget *m_predictionsList;
    QWidget *m_tideBarsWidget;

    // Data
    TideManager *m_tideManager;
    QTimer *m_updateTimer;
    QList<TidePrediction> m_predictions;
    double m_currentLatitude;
    double m_currentLongitude;
};

#endif // TIDEPANEL_H