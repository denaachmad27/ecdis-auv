#ifndef ROUTEQUICKFORMDIALOG_H
#define ROUTEQUICKFORMDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QTableWidget>
#include <QCheckBox>
#include <QDoubleValidator>
#include <QHeaderView>
#include <QGroupBox>
#include <QRadioButton>
#include <QGridLayout>

#include "routeformdialog.h" // for RouteWaypointData

class EcWidget;

class RouteQuickFormDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RouteQuickFormDialog(QWidget* parent = nullptr, EcWidget* ecWidget = nullptr);

    QList<RouteWaypointData> getWaypoints() const { return waypoints; }
    QString getRouteName() const { return routeNameEdit ? routeNameEdit->text().trimmed() : QString(); }

private slots:
    void onAddWaypoint();
    void onClearInputs();
    void onCreateRoute();
    void onUnitChanged();

private:
    void setupUI();
    void updateTable();
    bool parseDegMinNumeric(int deg, double minutes, bool isLat, int sign, double& out);
    void metersToLatLon(double north, double east, double& outLat, double& outLon);
    int generateNextRouteId() const; // for displaying route name in title
    QString formatDegMin(double value, bool isLat) const;

    // Inputs
    QLineEdit* routeNameEdit;
    QLineEdit* labelEdit;
    // Unit selector (radio)
    QGroupBox* unitGroup;
    QRadioButton* decDegBtn;
    QRadioButton* degMinBtn;
    QRadioButton* metersBtn;
    QLabel* latLabel;
    QLabel* lonLabel;
    QLineEdit* latEdit;
    QLineEdit* lonEdit;
    QGroupBox* decMetersGroup;
    
    // Deg-Min inputs
    QGroupBox* degMinGroup;
    QLineEdit* latDegEdit;
    QLineEdit* latMinEdit;
    QComboBox* latHemCombo;
    QLineEdit* lonDegEdit;
    QLineEdit* lonMinEdit;
    QComboBox* lonHemCombo;
    QLineEdit* remarkEdit;
    QCheckBox* activeCheck;

    // Actions
    QPushButton* addBtn;
    QPushButton* clearBtn;
    QPushButton* createBtn;
    QPushButton* cancelBtn;

    // List
    QTableWidget* table;

    // Data
    QList<RouteWaypointData> waypoints;
    EcWidget* ecWidget;

    // Validators
    QDoubleValidator* latValidator;
    QDoubleValidator* lonValidator;
};

#endif // ROUTEQUICKFORMDIALOG_H
