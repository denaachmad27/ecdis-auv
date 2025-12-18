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
class QGroupBox;
class QCheckBox;
class QSlider;
class QDoubleSpinBox;
class QTableWidget;
class QTimer;
class QProgressDialog;
class QApplication;

template<typename T>
class QFutureWatcher;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    void loadSettings();
    void saveSettings();
    SettingsData loadSettingsFromFile(const QString &filePath = "config.ini");

    EcWidget::DisplayOrientationMode orientation(const QString &str);
    EcWidget::OSCenteringMode centering(const QString &str);
    AppConfig::AppTheme theme(const QString &str);

    // Database connection settings
    struct DatabaseSettings {
        QString host;
        QString port;
        QString dbName;
        QString user;
        QString password;
    };
    static DatabaseSettings getDatabaseSettings();
    bool getDatabaseConnectionStatus() const { return isDatabaseConnected; }
    void setDatabaseConnectionStatus(bool connected);

signals:
    void dialogOpened();
    void dialogClosed();
    void databaseConnectionStatusChanged(bool connected);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

public slots:
    void onConnectionStatusChanged(const bool &connection);

private slots:
    void updateAisWidgetsVisibility(const QString &text);
    void onAddGpsRow();
    void onRemoveGpsRow();
    void updatePrimaryGpsCombo();
    void onNavDepthChanged(double value);
    void onNavDraftChanged(double value);
    void onNavDraftBelowKeelChanged(double value);
    void onDbConnectClicked();
    void checkDatabaseConnection();
    bool checkDatabaseConnectionAsync();
    void onDatabaseConnectionFinished();

private:
    void setupUI();
    void accept() override;
    void reject() override;

    // MOOSDB
    QLineEdit *moosIpLineEdit;
    QLineEdit *moosPortLineEdit;

    // Database Connection
    QLineEdit *dbHostLineEdit;
    QLineEdit *dbPortLineEdit;
    QLineEdit *dbNameLineEdit;
    QLineEdit *dbUserLineEdit;
    QLineEdit *dbPasswordLineEdit;
    QLabel *dbStatusLabel;
    QPushButton *dbConnectButton;

    // Database status management
    bool isDatabaseConnected;
    QProgressDialog *loadingDialog;
    QFutureWatcher<bool> *connectionWatcher;

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
    QDoubleSpinBox *shipLengthSpin;
    QDoubleSpinBox *shipBeamSpin;
    QDoubleSpinBox *shipHeightSpin;
    QDoubleSpinBox *shipDraftSpin;
    QDoubleSpinBox *ukcDangerSpin;
    QDoubleSpinBox *ukcWarningSpin;

    // TURNING PREDICTION
    QGroupBox *turningPredictionGroup;
    QCheckBox *showTurningPredictionCheckBox;
    QSpinBox *predictionTimeSpin;
    QLabel *predictionTimeLabel;
    QComboBox *predictionDensityCombo;
    QLabel *predictionDensityLabel;

    // COLLISION RISK
    QCheckBox *enableCollisionRiskCheckBox;
    QCheckBox *showRiskSymbolsCheckBox;
    QCheckBox *enableAudioAlertsCheckBox;
    QCheckBox *enablePulsingWarningsCheckBox;
    QDoubleSpinBox *criticalRiskDistanceSpin;
    QDoubleSpinBox *highRiskDistanceSpin;
    QDoubleSpinBox *mediumRiskDistanceSpin;
    QDoubleSpinBox *lowRiskDistanceSpin;
    QDoubleSpinBox *criticalTimeSpin;
    QDoubleSpinBox *highTimeSpin;

    // Navigation Safety Variables (for MOOSDB integration)
    QDoubleSpinBox *navDepthSpin;
    QDoubleSpinBox *navDraftSpin;
    QDoubleSpinBox *navDraftBelowKeelSpin;

    // GPS Configuration
    QTableWidget *gpsTableWidget;
    QComboBox *primaryGpsCombo;


    // Safety notice UI
    QLabel *ukcNoticeLabel;
    QTimer *ukcNoticeTimer;

    // CPA/TCPA (merged into Collision Risk Indication)
    QDoubleSpinBox *cpaSpin;
    QDoubleSpinBox *tcpaSpin;

    // CHART
    QComboBox *chartCombo;
    QComboBox *latViewCombo;
    QComboBox *longViewCombo;
};

#endif // SETTINGSDIALOG_H
