#include "cpatcpasettingsdialog.h"

CPATCPASettingsDialog::CPATCPASettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("CPA/TCPA Settings");
    setModal(true);
    resize(400, 350);

    setupUI();
    setDefaultValues();
}

void CPATCPASettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // CPA/TCPA Threshold Group
    QGroupBox *thresholdGroup = new QGroupBox("Threshold Settings");
    QFormLayout *thresholdLayout = new QFormLayout(thresholdGroup);

    cpaThresholdSpinBox = new QDoubleSpinBox();
    cpaThresholdSpinBox->setSuffix(" NM");
    cpaThresholdSpinBox->setRange(0.1, 10.0);
    cpaThresholdSpinBox->setSingleStep(0.1);
    cpaThresholdSpinBox->setDecimals(1);
    thresholdLayout->addRow("CPA Threshold:", cpaThresholdSpinBox);

    tcpaThresholdSpinBox = new QDoubleSpinBox();
    tcpaThresholdSpinBox->setSuffix(" minutes");
    tcpaThresholdSpinBox->setRange(1.0, 60.0);
    tcpaThresholdSpinBox->setSingleStep(1.0);
    tcpaThresholdSpinBox->setDecimals(0);
    thresholdLayout->addRow("TCPA Threshold:", tcpaThresholdSpinBox);

    // Alarm Settings Group
    QGroupBox *alarmGroup = new QGroupBox("Alarm Settings");
    QVBoxLayout *alarmLayout = new QVBoxLayout(alarmGroup);

    cpaAlarmCheckBox = new QCheckBox("Enable CPA Alarm");
    tcpaAlarmCheckBox = new QCheckBox("Enable TCPA Alarm");
    visualAlarmCheckBox = new QCheckBox("Visual Alarm");
    audioAlarmCheckBox = new QCheckBox("Audio Alarm");

    alarmLayout->addWidget(cpaAlarmCheckBox);
    alarmLayout->addWidget(tcpaAlarmCheckBox);
    alarmLayout->addWidget(visualAlarmCheckBox);
    alarmLayout->addWidget(audioAlarmCheckBox);

    // Update Interval Group
    QGroupBox *updateGroup = new QGroupBox("Update Settings");
    QFormLayout *updateLayout = new QFormLayout(updateGroup);

    updateIntervalSpinBox = new QSpinBox();
    updateIntervalSpinBox->setSuffix(" seconds");
    updateIntervalSpinBox->setRange(1, 30);
    updateIntervalSpinBox->setSingleStep(1);
    updateLayout->addRow("Update Interval:", updateIntervalSpinBox);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    okButton = new QPushButton("OK");
    cancelButton = new QPushButton("Cancel");
    resetButton = new QPushButton("Reset to Default");

    buttonLayout->addWidget(resetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    // Connect signals
    connect(okButton, SIGNAL(clicked()), this, SLOT(onOKClicked()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
    connect(resetButton, SIGNAL(clicked()), this, SLOT(onResetToDefaultClicked()));

    // Add to main layout
    mainLayout->addWidget(thresholdGroup);
    mainLayout->addWidget(alarmGroup);
    mainLayout->addWidget(updateGroup);
    mainLayout->addLayout(buttonLayout);
}

void CPATCPASettingsDialog::setDefaultValues()
{
    setCPAThreshold(0.5);      // 0.5 NM default
    setTCPAThreshold(6.0);     // 6 minutes default
    setCPAAlarmEnabled(true);
    setTCPAAlarmEnabled(true);
    setVisualAlarmEnabled(true);
    setAudioAlarmEnabled(false);
    setAlarmUpdateInterval(5); // 5 seconds default
}

// Getter methods
double CPATCPASettingsDialog::getCPAThreshold() const
{
    return cpaThresholdSpinBox->value();
}

double CPATCPASettingsDialog::getTCPAThreshold() const
{
    return tcpaThresholdSpinBox->value();
}

bool CPATCPASettingsDialog::isCPAAlarmEnabled() const
{
    return cpaAlarmCheckBox->isChecked();
}

bool CPATCPASettingsDialog::isTCPAAlarmEnabled() const
{
    return tcpaAlarmCheckBox->isChecked();
}

bool CPATCPASettingsDialog::isVisualAlarmEnabled() const
{
    return visualAlarmCheckBox->isChecked();
}

bool CPATCPASettingsDialog::isAudioAlarmEnabled() const
{
    return audioAlarmCheckBox->isChecked();
}

int CPATCPASettingsDialog::getAlarmUpdateInterval() const
{
    return updateIntervalSpinBox->value();
}

// Setter methods
void CPATCPASettingsDialog::setCPAThreshold(double threshold)
{
    cpaThresholdSpinBox->setValue(threshold);
}

void CPATCPASettingsDialog::setTCPAThreshold(double threshold)
{
    tcpaThresholdSpinBox->setValue(threshold);
}

void CPATCPASettingsDialog::setCPAAlarmEnabled(bool enabled)
{
    cpaAlarmCheckBox->setChecked(enabled);
}

void CPATCPASettingsDialog::setTCPAAlarmEnabled(bool enabled)
{
    tcpaAlarmCheckBox->setChecked(enabled);
}

void CPATCPASettingsDialog::setVisualAlarmEnabled(bool enabled)
{
    visualAlarmCheckBox->setChecked(enabled);
}

void CPATCPASettingsDialog::setAudioAlarmEnabled(bool enabled)
{
    audioAlarmCheckBox->setChecked(enabled);
}

void CPATCPASettingsDialog::setAlarmUpdateInterval(int interval)
{
    updateIntervalSpinBox->setValue(interval);
}

// Slots
void CPATCPASettingsDialog::onOKClicked()
{
    accept();
}

void CPATCPASettingsDialog::onCancelClicked()
{
    reject();
}

void CPATCPASettingsDialog::onResetToDefaultClicked()
{
    setDefaultValues();
}
