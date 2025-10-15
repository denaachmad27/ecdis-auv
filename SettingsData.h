#ifndef SETTINGSDATA_H
#define SETTINGSDATA_H

#include <QString>
#include <QList>
#include "appconfig.h"
#include "ecwidget.h"

// Struct to hold position data for a single GPS unit
struct GpsPosition {
    QString name;
    double offsetX; // Distance from centerline (Port -, Starboard +)
    double offsetY; // Distance from bow (Aft -, Fwd +)
};

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

    // CHART
    QString chartMode;

    // GUARDZONE
    int defaultShipTypeFilter;
    int defaultAlertDirection;

    // OWNSHIP
    EcWidget::DisplayOrientationMode orientationMode;
    EcWidget::OSCenteringMode centeringMode;
    int courseUpHeading;

    QString latViewMode;
    QString longViewMode;

    // TURNING PREDICTION
    bool showTurningPrediction;    // Enable/disable turning prediction display
    int predictionTimeMinutes;     // Prediction time in minutes (default: 3)
    int predictionDensity;         // Density of ship outlines: 1=Low (20s), 2=Medium (10s), 3=High (5s)

    // ALERT SETTINGS
    bool visualFlashingEnabled;
    QString soundAlarmFile;
    bool soundAlarmEnabled;
    int soundAlarmVolume;

    // TRAIL SETTING
    int trailMode;
    int trailMinute;
    double trailDistance;

    // NAVIGATION SAFETY & SHIP DIMENSIONS
    double shipLength;      // Overall length
    double shipBeam;        // Overall beam/width
    double shipHeight;      // Height above waterline
    double shipDraftMeters; // Vessel draft used for depth-based hazard checks
    double ukcDangerMeters; // Under Keel Clearance threshold for DANGEROUS
    double ukcWarningMeters; // Under Keel Clearance threshold for CAUTION

    // GPS Configuration
    QList<GpsPosition> gpsPositions;
    int primaryGpsIndex;

    // CPA/TCPA
    double cpaThreshold;
    double tcpaThreshold;
};

#endif // SETTINGSDATA_H
