#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QDebug>
#include "SettingsData.h"

class QLineEdit;
class QComboBox;
class QLabel;
class QButtonGroup;
class QCheckBox;
class QSlider;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void loadSettings();
    void saveSettings();
    SettingsData loadSettingsFromFile(const QString &filePath = "config.ini");

    EcWidget::DisplayOrientationMode orientation(const QString &str);
    EcWidget::OSCenteringMode centering(const QString &str);

signals:
    void dialogOpened();
    void dialogClosed();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

public slots:
    void onConnectionStatusChanged(const bool &connection);

private slots:
    void updateAisWidgetsVisibility(const QString &text);

private:
    void setupUI();
    void accept() override;
    void reject() override;

    // MOOSDB
    QLineEdit *moosIpLineEdit;
    QLineEdit *moosPortLineEdit;

    // AIS
    QComboBox *aisSourceCombo;
    QLabel *ipLabel;
    QLineEdit *ipAisLineEdit;
    QLabel *logFileLabel;
    QLineEdit *logFileLineEdit;

    // Display
    QComboBox *displayModeCombo;

    // GuardZone
    QButtonGroup *shipTypeButtonGroup;
    QButtonGroup *alertDirectionButtonGroup;

    // OWNSHIP
    QComboBox *orientationCombo;
    QComboBox *centeringCombo;
    QSpinBox *headingSpin;
    QLabel *headingLabel;

    // ALERT SETTINGS
    QCheckBox *visualFlashingCheckBox;
    QComboBox *soundAlarmCombo;
    QCheckBox *soundAlarmEnabledCheckBox;
    QSlider *soundVolumeSlider;
    QLabel *soundVolumeLabel;

    // TRAIL MINUTES
    QComboBox *trailCombo;
    QLabel *trailLabel;
    QLabel *trailLabelDistance;
    QSpinBox *trailSpin;
    QDoubleSpinBox *trailSpinDistance;
};

#endif // SETTINGSDIALOG_H
