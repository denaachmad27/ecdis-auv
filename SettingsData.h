#ifndef SETTINGSDATA_H
#define SETTINGSDATA_H

#include <QString>
#include "ecwidget.h"

struct SettingsData {
    // MOOSDB
    QString moosIp;
    QString moosPort;

    // AIS
    QString aisSource;
    QString aisIp;
    QString aisLogFile;

    // DISPLAY
    QString displayMode;

    // GUARDZONE
    int defaultShipTypeFilter;
    int defaultAlertDirection;

    // OWNSHIP
    EcWidget::DisplayOrientationMode orientationMode;
    EcWidget::OSCenteringMode centeringMode;
    int courseUpHeading;
};

#endif // SETTINGSDATA_H
