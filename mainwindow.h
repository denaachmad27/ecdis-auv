#ifndef _mainwindow_h_
#define _mainwindow_h_

// #include <QtGui>
#include <QtWidgets>
#include <QDebug>

#include "IAisDvrPlugin.h"
#include "ecwidget.h"
#include "moosdb.h"
#include "guardzonepanel.h"
#include "alertpanel.h"
#include "alertsystem.h"

#include <QPluginLoader>

#include "cpatcpacalculator.h"
#include "cpatcpasettings.h"
#include <QTimer>
#include "cpatcpapanel.h"

// forward declerations
class PickWindow;
class SearchWindow;
class Ais;

// Defines for S-63 Chart Import
// user permit (must have 28 characters)
#define USERPERMIT "66B5CBFDF7E4139D5B6086C23130"
// manufacturer key (M_KEY must have either 5 or 10 characters)
#define M_KEY "10121"
// data server id
#define DSID "0"

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
    class Exception{
        QStringList msgs;

        public:
            Exception(const QString & m): msgs(QString("MainWindow: %1").arg(m)){}
            Exception(const QStringList & lst, const QString & m): msgs(lst){
                msgs += QString("MainWindow: %1").arg(m);
            }

            const QStringList & GetMessages() const { return msgs;
        }
    }; // Exception

    MainWindow(QWidget *parent = (QWidget*)0);
    virtual ~MainWindow();

    EcDictInfo *GetDictionary() const { return dict; }

    QStringList dataList = {
        "!AIVDM,1,1,,B,15N9qL0P1cI>9g4@j>@oMwwn00RM,0*43",
        "!AIVDM,1,1,,A,15MwWD0P0nI=msl@j:9Qpwv005H`,0*27",
        "!AIVDM,1,1,,B,D03Own@7IN>4,0*4E",
    };

    PickWindow *pickWindow;
    SearchWindow *searchWindow;
    QTextEdit *logText;

    void loadPlugin();
    void loadPluginAis();
    void onNmeaReceived(const QString&);

public slots:
    void onEnableRedDotTracker(bool enabled);
    void onAttachRedDotToShip(bool attached);
    void onShipGuardianCircle(bool enabled);

protected slots:
    void onReload();

    void onImportTree();
    void onImportS57();
    void onImportIHOCertificate();
    void onImportS63Permits();
    void onImportS63();

    void onZoomIn();
    void onZoomOut();

    void onLeft();
    void onRight();
    void onUp();
    void onDown();

    void onRotateCW();
    void onRotateCCW();

    void onProjection(QAction *);

    void onDisplayCategory(QAction *);
    void onLookup(QAction *);

    void onLights(bool);
    void onText(bool);
    void onSoundings(bool);

    void onGrid(bool);

    void onAIS(bool);
    void onTrack(bool);

    void onSearch();

    void onColorScheme(QAction *);
    void onGreyMode(bool);

    void onScale(int);
    void onProjection();
    void onMouseMove(EcCoordinate, EcCoordinate);
    void onMouseRightClick(const QPoint&);

    void runAis();
    void slotLoadAisFile();
    void slotLoadAisVariable();
    void slotStopLoadAisVariable();
    void slotConnectToAisServer();

    void subscribeMOOSDB();
    void subscribeMOOSDBMAP();

    void subscribeMOOSDBMapInfo();
    void stopSubscribeMOOSDB();
    void publishMOOSDB();
    void stopPublishMOOSDB();

    void jsonExample();

    // Waypoint
    void onCreateWaypoint();
    void onRemoveWaypoint();
    void onMoveWaypoint();
    void onEditWaypoint();
    void onWaypointCreated();

    void createDockWindows();
    void createDockNmea();
    void createDockOwnship();
    void addTextToBar(QString);
    void createActions();
    void onClearWaypoints();
    void onExportWaypoints();
    void onImportWaypoints();

    void openSettingsDialog();
    void setDisplay();
    // Guardzone
    void onEnableGuardZone(bool enabled);
    void onCreateCircularGuardZone();
    void onCreatePolygonGuardZone();
    void onCheckGuardZone();
    void onAttachGuardZoneToShip(bool attached);

    // GuardZone Panel slots
    void onGuardZoneSelected(int guardZoneId);
    void onGuardZoneEditRequested(int guardZoneId);
    void onGuardZoneVisibilityChanged(int guardZoneId, bool visible);

    // Simulasi
    void onStartSimulation();
    void onStopSimulation();
    void onAutoCheckGuardZone(bool checked);

    // DVR PLUGIN
    void startAisRecord();
    void stopAisRecord();

    // NMEA DECODE
    void nmeaDecode();
    void showSystemStatistics();
    void createTestGuardZones();

protected:
    void DrawChart();

    virtual void closeEvent(QCloseEvent *);

    EcDictInfo  *dict;
    EcWidget    *ecchart;
    QLineEdit   *rngEdit, *posEdit, *proEdit, *sclEdit;

    QString      dencPath;
    QString      impPath;

    int lookupTable, displayCategory;
    bool showLights, showText, showNationalText, showSoundings, showGrid, showAIS, trackShip;

    QActionGroup *dencActionGroup;
    QAction *autoProjectionAction, *mercatorAction, *gnomonicAction, *stereographicAction;
    QAction *baseAction, *standardAction, *otherAction;
    QAction *simplifiedAction, *fullChartAction;
    QAction *dayAction, *duskAction, *nightAction;
    QAction *logfileAction, *serverAction;

    QAction* startAisRecAction;
    QAction* stopAisRecAction;

    IAisDvrPlugin* aisDvr;

private slots:
    // CPA/TCPA slots
    void onCPASettings();
    void onShowCPATargets(bool enabled);
    void onShowTCPAInfo(bool enabled);
    void onCPATCPAAlarms(bool enabled);

    // test guardzone
    void onTestGuardZone(bool enabled);

private:
    GuardZonePanel* guardZonePanel;
    QDockWidget* guardZoneDock;
    void setupGuardZonePanel();
    void setupTestingMenu();

    // Alert Panel
    AlertPanel* alertPanel;
    QDockWidget* alertDock;
    void setupAlertPanel();

    // Alert handling methods
    void onAlertTriggered(const AlertData& alert);
    void onCriticalAlertTriggered(const AlertData& alert);
    void onAlertSelected(int alertId);
    void onAlertSystemStatusChanged(bool enabled);

    // Alert testing methods
    void testAlertWorkflow();

    // CPA/TCPA related members
    CPATCPACalculator* m_cpaCalculator;
    QTimer* m_cpaUpdateTimer;

    // Helper methods untuk CPA/TCPA
    void updateCPATCPAForAllTargets();
    void checkCPATCPAAlarms();
    void logCPATCPAInfo(const QString& mmsi, const CPATCPAResult& result);
    void processTestTarget(const VesselState& ownShip);
    void processAISTarget(const VesselState& ownShip, const AISTargetData& target);

    // CPA/TCPA Panel
    CPATCPAPanel* m_cpatcpaPanel;
    QDockWidget* m_cpatcpaDock;

    // Helper methods untuk CPA/TCPA
    void setupCPATCPAPanel();
};

#endif // _mainwindow_h_
