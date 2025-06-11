#ifndef CPATCPASETTINGSDIALOG_H
#define CPATCPASETTINGSDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QComboBox>

class CPATCPASettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CPATCPASettingsDialog(QWidget *parent = nullptr);

    // Getter methods untuk nilai settings
    double getCPAThreshold() const;
    double getTCPAThreshold() const;
    bool isCPAAlarmEnabled() const;
    bool isTCPAAlarmEnabled() const;
    bool isVisualAlarmEnabled() const;
    bool isAudioAlarmEnabled() const;
    int getAlarmUpdateInterval() const;

    // Setter methods untuk nilai settings
    void setCPAThreshold(double threshold);
    void setTCPAThreshold(double threshold);
    void setCPAAlarmEnabled(bool enabled);
    void setTCPAAlarmEnabled(bool enabled);
    void setVisualAlarmEnabled(bool enabled);
    void setAudioAlarmEnabled(bool enabled);
    void setAlarmUpdateInterval(int interval);

private slots:
    void onOKClicked();
    void onCancelClicked();
    void onResetToDefaultClicked();

private:
    void setupUI();
    void setDefaultValues();

    // UI Components
    QDoubleSpinBox *cpaThresholdSpinBox;
    QDoubleSpinBox *tcpaThresholdSpinBox;
    QCheckBox *cpaAlarmCheckBox;
    QCheckBox *tcpaAlarmCheckBox;
    QCheckBox *visualAlarmCheckBox;
    QCheckBox *audioAlarmCheckBox;
    QSpinBox *updateIntervalSpinBox;

    QPushButton *okButton;
    QPushButton *cancelButton;
    QPushButton *resetButton;
};

#endif // CPATCPASETTINGSDIALOG_H
