#ifndef SETTINGSDATA_H
#define SETTINGSDATA_H

#include <QString>
#include "appconfig.h"
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
    AppConfig::AppTheme themeMode;

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

    // NAVIGATION SAFETY
    double shipDraftMeters; // Vessel draft used for depth-based hazard checks
    double ukcDangerMeters; // Under Keel Clearance threshold for DANGEROUS
    double ukcWarningMeters; // Under Keel Clearance threshold for CAUTION

    // CPA/TCPA
    double cpaThreshold;
    double tcpaThreshold;
};

#endif // SETTINGSDATA_H
