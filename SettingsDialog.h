#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QDebug>
#include "SettingsData.h"
#include "appconfig.h"

class QLineEdit;
class QComboBox;
class QLabel;
class QButtonGroup;
class QCheckBox;
class QSlider;
class QDoubleSpinBox;
class QTimer;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void loadSettings();
    void saveSettings();
    SettingsData loadSettingsFromFile(const QString &filePath = "config.ini");

    EcWidget::DisplayOrientationMode orientation(const QString &str);
    EcWidget::OSCenteringMode centering(const QString &str);
    AppConfig::AppTheme theme(const QString &str);

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
    QComboBox *themeModeCombo;

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

    // NAVIGATION SAFETY
    QDoubleSpinBox *shipDraftSpin;
    QDoubleSpinBox *ukcDangerSpin;
    QDoubleSpinBox *ukcWarningSpin;

    // Safety notice UI
    QLabel *ukcNoticeLabel;
    QTimer *ukcNoticeTimer;
};

#endif // SETTINGSDIALOG_H
