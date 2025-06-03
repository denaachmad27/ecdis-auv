#include "SettingsDialog.h"
#include "SettingsManager.h"

#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QSizePolicy>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI() {
    setWindowTitle("Settings Manager");
    resize(400, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QTabWidget *tabWidget = new QTabWidget(this);

    // MOOSDB tab
    QWidget *moosTab = new QWidget;
    QFormLayout *moosLayout = new QFormLayout;
    moosIpLineEdit = new QLineEdit;
    moosPortLineEdit = new QLineEdit;
    moosLayout->addRow("MOOSDB IP:", moosIpLineEdit);
    //moosLayout->addRow("MOOSDB Port:", moosPortLineEdit);
    moosTab->setLayout(moosLayout);

    // --- AIS Tab ---
    QWidget *aisTab = new QWidget;
    QFormLayout *aisLayout = new QFormLayout;

    // AIS Source
    aisSourceCombo = new QComboBox;
    aisSourceCombo->addItems({"log", "moosdb", "ip"});
    aisLayout->addRow("AIS Source:", aisSourceCombo);

    // AIS IP
    ipLabel = new QLabel("AIS IP:");
    ipAisLineEdit = new QLineEdit;
    aisLayout->addRow(ipLabel, ipAisLineEdit);

    // AIS Log File
    logFileLabel = new QLabel("AIS Log File:");
    logFileLineEdit = new QLineEdit;
    logFileLineEdit->setPlaceholderText("Pilih file log...");
    QPushButton *logFileButton = new QPushButton("Browse...");
    QHBoxLayout *logInputLayout = new QHBoxLayout;
    logInputLayout->addWidget(logFileLineEdit);
    logInputLayout->addWidget(logFileButton);
    logInputLayout->setContentsMargins(0, 0, 0, 0); // biar rapat

    QWidget *logInputWidget = new QWidget;
    logInputWidget->setLayout(logInputLayout);
    aisLayout->addRow(logFileLabel, logInputWidget);

    aisTab->setLayout(aisLayout);

    // Browse button action
    connect(logFileButton, &QPushButton::clicked, this, [=]() {
        QString file = QFileDialog::getOpenFileName(this, "Select AIS Log File");
        if (!file.isEmpty()) {
            logFileLineEdit->setText(file);
        }
    });

    // Update visibility
    connect(aisSourceCombo, &QComboBox::currentTextChanged, this, &SettingsDialog::updateAisWidgetsVisibility);

    // Display tab
    QWidget *displayTab = new QWidget;
    QFormLayout *displayLayout = new QFormLayout;
    displayModeCombo = new QComboBox;
    displayModeCombo->addItems({"Day", "Dusk", "Night"});
    displayLayout->addRow("Display Mode:", displayModeCombo);
    displayTab->setLayout(displayLayout);

    tabWidget->addTab(moosTab, "MOOSDB");
    tabWidget->addTab(aisTab, "AIS");
    tabWidget->addTab(displayTab, "Display");

    mainLayout->addWidget(tabWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

void SettingsDialog::loadSettings() {
    QSettings settings("config.ini", QSettings::IniFormat);

    // MOOSDB
    moosIpLineEdit->setText(settings.value("MOOSDB/ip", "127.0.0.1").toString());
    moosPortLineEdit->setText(settings.value("MOOSDB/port", "9000").toString());

    // AIS
    QString aisSource = settings.value("AIS/source", "log").toString();
    int aisIndex = aisSourceCombo->findText(aisSource);
    if (aisIndex >= 0) {
        aisSourceCombo->setCurrentIndex(aisIndex);
    } else {
        aisSourceCombo->setCurrentIndex(0); // default to first if not found
    }

    ipAisLineEdit->setText(settings.value("AIS/ip", "").toString());
    logFileLineEdit->setText(settings.value("AIS/log_file", "").toString());

    // Perbarui visibilitas sesuai source
    updateAisWidgetsVisibility(aisSource);

    // Display
    QString displayMode = settings.value("Display/mode", "Day").toString();
    int displayIndex = displayModeCombo->findText(displayMode);
    if (displayIndex >= 0) {
        displayModeCombo->setCurrentIndex(displayIndex);
    } else {
        displayModeCombo->setCurrentIndex(0); // fallback default
    }
}

void SettingsDialog::saveSettings() {
    QSettings settings("config.ini", QSettings::IniFormat);

    // MOOSDB
    settings.setValue("MOOSDB/ip", moosIpLineEdit->text());
    settings.setValue("MOOSDB/port", moosPortLineEdit->text());

    // AIS
    settings.setValue("AIS/source", aisSourceCombo->currentText());
    settings.setValue("AIS/ip", ipAisLineEdit->text());
    settings.setValue("AIS/log_file", logFileLineEdit->text());

    // Display
    settings.setValue("Display/mode", displayModeCombo->currentText());
}

void SettingsDialog::updateAisWidgetsVisibility(const QString &text) {
    bool isIp = (text == "ip");
    bool isLog = (text == "log");

    ipLabel->setVisible(isIp);
    ipAisLineEdit->setVisible(isIp);

    logFileLabel->setVisible(isLog);
    logFileLineEdit->parentWidget()->setVisible(isLog); // parentWidget karena ada layout dalam widget
}

// Fungsi static atau anggota kelas
SettingsData SettingsDialog::loadSettingsFromFile(const QString &filePath) {
    SettingsData data;

    QSettings settings(filePath, QSettings::IniFormat);

    data.moosIp = settings.value("MOOSDB/ip", "127.0.0.1").toString();
    //data.moosPort = settings.value("MOOSDB/port", "9000").toString();

    data.aisSource = settings.value("AIS/source", "log").toString();
    data.aisIp = settings.value("AIS/ip", "").toString();
    data.aisLogFile = settings.value("AIS/log_file", "").toString();

    data.displayMode = settings.value("Display/mode", "Day").toString();

    return data;
}

void SettingsDialog::accept() {
    SettingsData data;
    data.moosIp = moosIpLineEdit->text();
    //data.moosPort = moosPortLineEdit->text();
    data.aisSource = aisSourceCombo->currentText();
    data.aisIp = ipAisLineEdit->text();
    data.aisLogFile = logFileLineEdit->text();
    data.displayMode = displayModeCombo->currentText();

    SettingsManager::instance().save(data);

    QDialog::accept();
}
