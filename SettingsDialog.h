#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "SettingsData.h"

class QLineEdit;
class QComboBox;
class QLabel;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void loadSettings();
    void saveSettings();
    SettingsData loadSettingsFromFile(const QString &filePath = "config.ini");

private slots:
    void updateAisWidgetsVisibility(const QString &text);

private:
    void setupUI();
    void accept() override;

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
};

#endif // SETTINGSDIALOG_H
