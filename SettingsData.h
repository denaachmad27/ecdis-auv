#ifndef SETTINGSDATA_H
#define SETTINGSDATA_H

#include <QString>

struct SettingsData {
    QString moosIp;
    QString moosPort;
    QString aisSource;
    QString aisIp;
    QString aisLogFile;
    QString displayMode;
    int defaultShipTypeFilter;
    int defaultAlertDirection;
};

#endif // SETTINGSDATA_H
