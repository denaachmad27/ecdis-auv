#ifndef CPATCPAPANEL_H
#define CPATCPAPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTimer>
#include <QTime>
#include <QAbstractItemView>
#include <QRadioButton>

#include "ecwidget.h"
#include "cpatcpasettings.h"
#include "cpatcpacalculator.h"

// SevenCs Kernel EC2007
#ifdef _WIN32
#include <windows.h>
#else
#include <stdio.h>
#include <X11/Xlib.h>
#endif

#include <eckernel.h>

class CPATCPAPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CPATCPAPanel(QWidget *parent = nullptr);

    void setEcWidget(EcWidget* widget);
    void updateTargetsDisplay();
    void updateOwnShipInfo(double lat, double lon, double cog, double sog);

public slots:
    void refreshData();
    void onSettingsClicked();
    void onClearAlarmsClicked();
    void onTargetsContextMenuRequested(const QPoint& pos);

    // Clear all targets from table
    void clearTargets();

private slots:
    void onTimerTimeout();  // Ganti nama dari updateTimer
    void onTargetSelected();
    void onTableSorted(int column, Qt::SortOrder order);

private:
    void setupUI();
    void setupTargetsTable();
    void setupStatusPanel();
    void updateTargetRow(int row, const AISTargetData& target, const CPATCPAResult& result);
    QString formatTime(double minutes);
    QString formatDistance(double nauticalMiles);
    void applyColumnVisibility();
    void reorderColumns(const QList<int>& logicalOrder);

    // UI Components
    QVBoxLayout* mainLayout;
    QGroupBox* statusGroup;
    QGroupBox* targetsGroup;
    QGroupBox* ownShipGroup;

    // Status Panel
    QLabel* systemStatusLabel;
    QLabel* activeTargetsLabel;
    QLabel* dangerousTargetsLabel;
    QLabel* lastUpdateLabel;

    // Own Ship Panel
    QLabel* ownShipLatLabel;
    QLabel* ownShipLonLabel;
    QLabel* ownShipCogLabel;
    QLabel* ownShipSogLabel;

    // Targets Table
    QTableWidget* targetsTable;
    QRadioButton* modeCpaTcpAButton;
    QRadioButton* modeBearingRangeButton;

    // Control Buttons
    QPushButton* refreshButton;
    QPushButton* settingsButton;
    QPushButton* clearAlarmsButton;

    // Timer
    QTimer* refreshTimer;  // Ganti nama dari updateTimer

    // Data
    EcWidget* ecWidget;
    int dangerousCount;
    int totalTargets;

    // SELECTION
    QString selectedMmsi;
    bool scrollToTrackedOnNextRefresh = false; // request scrolling once to tracked row
    bool preserveScrollOnRefresh = true;       // preserve user scroll position on periodic refresh
    bool isRefreshing = false;                 // guard to ignore selection changes during refresh

    enum DisplayMode { ModeCpaTcpa, ModeBearingRange };
    DisplayMode displayMode = ModeBearingRange;
};

#endif // CPATCPAPANEL_H
