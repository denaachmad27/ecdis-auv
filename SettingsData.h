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

    // ALERT SETTINGS
    bool visualFlashingEnabled;
    QString soundAlarmFile;
    bool soundAlarmEnabled;
    int soundAlarmVolume;

    // TRAIL SETTING
    int trailMode;
    int trailMinute;
    double trailDistance;
};

#endif // SETTINGSDATA_H
