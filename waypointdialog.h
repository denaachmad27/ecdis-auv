#ifndef WAYPOINTDIALOG_H
#define WAYPOINTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QDoubleValidator>

class WaypointDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WaypointDialog(QWidget *parent = nullptr);
    
    // Getters for form data
    double getLatitude() const;
    double getLongitude() const;
    QString getLabel() const;
    QString getRemark() const;
    int getRouteId() const;
    double getTurningRadius() const;

    // Setters for default values
    void setLatitude(double lat);
    void setLongitude(double lon);
    void setLabel(const QString& label);
    void setRemark(const QString& remark);
    void setRouteId(int routeId);
    void setTurningRadius(double radius);

private slots:
    void validateAndAccept();
    void onCoordinateChanged();

private:
    void setupUI();
    void connectSignals();
    bool validateCoordinates();
    
    // UI components
    QLineEdit *latitudeEdit;
    QLineEdit *longitudeEdit;
    QLineEdit *labelEdit;
    QTextEdit *remarkEdit;
    QComboBox *routeComboBox;
    QLineEdit *turningRadiusEdit;
    
    QPushButton *okButton;
    QPushButton *cancelButton;
    
    QLabel *statusLabel;
    
    // Validators
    QDoubleValidator *latValidator;
    QDoubleValidator *lonValidator;
    QDoubleValidator *radiusValidator;
};

#endif // WAYPOINTDIALOG_H