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

// forward declerations
class PickWindow;
class SearchWindow;

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
        "$GPGGA,111000,2924.2304,N,09444.9053,W,2,8,1.5,-18,M,,M,,*70",
        "!AIVDM,1,1,,B,15N9qL0P1cI>9g4@j>@oMwwn00RM,0*43",
        "!AIVDM,1,1,,A,15MwWD0P0nI=msl@j:9Qpwv005H`,0*27",
        "!AIVDM,1,1,,B,D03Own@7IN>4,0*4E",
        "!AIVDM,1,1,,A,D03Ownh=MN>4,0*6B",
        "!AIVDM,1,1,,B,D03Own@=MN>4,0*40",
        "!AIVDM,1,1,,B,19NWrEh00oq=:J8@tO;JNHH200Sn,0*4B",
        "!AIVDM,1,1,,A,403OwnAuQ4c:0q><;D@l5To0080>,0*67",
        "!AIVDM,1,1,,A,15N0>d0P12I=StB@qpe5dgv00L3J,0*25",
        "!AIVDM,1,1,,A,403OwniuQ4c:0q=0OBA0nLW00<4E,0*6F",
        "!AIVDM,1,1,,A,15N3cj3P00I>0`b@k9P@0?wn0H0C,0*6F",
        "!AIVDM,1,1,,A,15N:220000I=BwN@o:GaV3@200SJ,0*64",
        "$GPHDT,226.7,T*34",
        "$GPROT,0.0,A*31",
        "$GPVTG,198.7,T,195.4,M,0.1,N,0.1,K*40",
        "$GPGGA,111001,2924.2304,N,09444.9053,W,2,8,1.5,-18,M,,M,,*71",
        "!AIVDO,1,1,,,15Mw0k0001q>Ac6@lk@Gio6005H`,0*37",
        "!AIVDM,1,1,,B,15Msp8PP00I>@0@@l`JqDwv2080P,0*78",
        "!AIVDM,1,1,,A,15MmCAP000I>6n2@ievtMSn20<1V,0*1A",
        "!AIVDM,1,1,,A,34V6dF1001q>uH`@e2AJ0QP2P000,0*0E",
        "!AIVDM,1,1,,A,18IlqP001kq=UAt@qflUbTUn0l37,0*66",
        "!AIVDM,1,1,,B,35MvBS?Oi2I?0bh@hbQK=9>401>@,0*4F",
        "!AIVDM,1,1,,A,15Mpm=PP0BI>GWD@mFs`t?v20D2M,0*62",
        "!AIVDM,1,1,,B,19NS:dh000q?AC4@iiT7F1V000Rr,0*5C",
        "!AIVDM,1,1,,B,15N7=hPP01I>8c0@k@D:Tgv2080o,0*25",
        "!AIVDM,1,1,,A,13orQ2000hI?I94@fJsccaR08D2?,0*5D",
        "!AIVDM,1,1,,A,35N39J5P00I=jRj@h;1h0?wn02w1,0*75",
        "!AIVDM,1,1,,B,15MlId`P0tI=oo8@mr<Etww@08Gi,0*21",
        "!AIVDM,1,1,,A,15N9HKP001I>@Q2@lclHwo840D2?,0*65",
        "!AIVDO,1,1,,,15Mw0k0001q>Ac6@lk@7ho6205H`,0*44",
        "!AIVDM,1,1,,B,15N;fgg000I>?`:@lV:Pj6l400SM,0*1A",
        "$GPHDT,226.8,T*3B",
        "$GPROT,0.0,A*31",
        "$GPVTG,199.6,T,196.3,M,0.1,N,0.1,K*44",
        "$GPGGA,111002,2924.2304,N,09444.9054,W,2,8,1.5,-17,M,,M,,*7A",
        "!AIVDM,1,1,,A,15MkIIPP00I=WCn@kDlh0?v005H`,0*05",
        "!AIVDM,1,1,,B,19NRw4h000I>sC@@cnv=r2B0081;,0*1B",
        "!AIVDM,1,1,,A,38153vh02<q?<l`@fQ;E53v221dA,0*29",
        "!AIVDM,1,1,,B,15N0G=?P01I>SBN@nPsbbOv6081?,0*67",
        "!AIVDM,1,1,,A,15MqSMPP01I>KG<@mbfstOv40H1E,0*69",
        "!AIVDM,1,1,,B,15Mv070P00I>7QF@ikJpu?v4081F,0*25",
        "!AIVDM,2,1,5,B,58;Fh401uLehAU@d001aDpV118Tp<E=<0000001=AhV3D52E,0*2A",
        "!AIVDM,2,2,5,B,N?B5Clm3kc5Dh0000000000,2*74",
        "!AIVDM,1,1,,A,15McsbPP00I>=a:@jr5okwv42<7T,0*3D",
        "!AIVDM,1,1,,B,15Mw`E0P0cI=b>`@q0Fdi?v605H`,0*5D",
        "!AIVDM,1,1,,A,15MnLQ0P00I>81<@i`19vOv62L2<,0*76",
        "!AIVDM,1,1,,A,14WFm@3P00I>3Bn@iQDb:gv620S1,0*37",
        "$GPHDT,226.8,T*3B",
        "$GPROT,0.0,A*31",
        "$GPVTG,198.8,T,195.5,M,0.1,N,0.1,K*4E",
        "!AIVDM,1,1,,A,15N4OU0P01I=`ht@hnipewv60D1S,0*30",
        "$GPGGA,111003,2924.2304,N,09444.9054,W,2,8,1.5,-17,M,,M,,*7B",
        "!AIVDM,1,1,,B,15N8>O?P00I>75>@iMo:Fgv60@1U,0*46",
        "!AIVDM,1,1,,B,38ID3t1000I>m9P@eA=6mQH60000,0*3B",
        "!AIVDM,1,1,,A,15?E7f0P00I>6Sh@ifAafgv600Ri,0*06",
        "!AIVDM,1,1,,A,15?C6l0025q=cO>@pfhLdb>400S2,0*42",
        "!AIVDO,1,1,,,15Mw0k0001q>Ac6@lk@7k76405H`,0*19",
        "!AIVDM,1,1,,A,15N8>LPP00I>7:<@iMp9s?v62D1u,0*73",
        "!AIVDM,1,1,,B,18IlqP001kq=UCl@qfPUbTT40UH`,0*0D",
        "!AIVDM,1,1,,A,15N64B0000I=vk@@iBmVdFB205H`,0*2A",
        "!AIVDM,1,1,,A,15MGw40000I>3qD@iUs4EWR60D2:,0*2B",
        "!AIVDM,1,1,,A,?03OwnAGOUV0D00,2*70",
        "!AIVDM,1,1,,B,15O?;v0001I>9@n@jppa?kt40@22,0*52",
        "!AIVDM,1,1,,A,15N0MW=P0mI=ojb@pW?Grwv800SL,0*2E",
        "$GPHDT,226.8,T*3B",
        "$GPROT,0.0,A*31",
        "$GPVTG,201.4,T,198.1,M,0.1,N,0.1,K*48",
        "$GPGGA,111004,2924.2304,N,09444.9054,W,2,8,1.5,-17,M,,M,,*7C",
        "!AIVDM,1,1,,B,14V;M20001q?Nah@fvv6wRp805H`,0*6A",
        "!AIVDM,1,1,,B,15Mpd70P0eI>QON@nCrHggv6082C,0*33",
        "!AIVDM,1,1,,A,15N05q`P0kI=`sj@kato0wvp0H@v,0*20",
        "!AIVDO,1,1,,,15Mw0k0001q>Ac4@lk@7oW6805H`,0*73",
        "!AIVDM,1,1,,B,15MnLNPP00I>83L@iW5b;Ov825H`,0*0C",
        "!AIVDM,1,1,,B,15N0Mn0P01I?2On@p13c`Ov80D2;,0*35",
        "!AIVDM,1,1,,A,15MwIO0000I>6p`@ifw9uIP80<28,0*1C",
        "!AIVDM,1,1,,B,15N:DgPP0MI>AjR@liGpoOv800RI,0*02",
        "!AIVDM,1,1,,B,15MR3V0P00I>762@il;auwv80L2>,0*5C",
        "!AIVDM,1,1,,B,143JFj0P00I>3eD@iP;Wrwv:0@2l,0*02",
        "!AIVDM,1,1,,A,15NC?1?000I>6pN@ifoW=kf:0<28,0*2A",
        "$GPHDT,226.9,T*3A",
        "$GPROT,0.0,A*31",
        "$GPVTG,206.0,T,202.7,M,0.1,N,0.1,K*4D",
        "$GPGGA,111005,2924.2303,N,09444.9054,W,2,8,1.5,-17,M,,M,,*7A",
        "!AIVDM,1,1,,A,15MwOsP000I=BnT@o;Dsa0P800R8,0*38",
        "!AIVDM,1,1,,A,15MsB80P00I=`0j@kTPHwgv:0H2v,0*4F",
        "!AIVDM,1,1,,A,15MwD;0P1SI>i<<@hQ@LF?v<0@2w,0*08",
        "!AIVDO,1,1,,,15Mw0k0001q>Ac4@lk@8376:05H`,0*42",
        "!AIVDM,1,1,,B,15Mv0v0001I>8ch@iOd:=s4<0<1G,0*1D",
        "!AIVDM,1,1,,A,18IlqP001kq=UCl@qfPUbTT60l37,0*19",
        "!AIVDM,1,1,,B,15Mwc40P0oI>=4B@lAj1lOv:0839,0*14",
        "!AIVDM,1,1,,A,15MpAAPP01I>7o@@in=c3gv<05H`,0*5B",
        "!AIVDM,1,1,,B,13QL:F0P1OI=qs>@mNFF8DpV05H`,0*04",
        "$GPHDT,227.0,T*32",
        "$GPROT,0.0,A*31",
        "$GPVTG,206.0,T,202.7,M,0.1,N,0.1,K*4D",
        "$GPGGA,111006,2924.2303,N,09444.9054,W,2,8,1.5,-17,M,,M,,*79",
        "!AIVDM,1,1,,B,?03OwnAGOUV0D00,2*73",
        "!AIVDM,1,1,,B,15N:V=0000I>7Id@ikNR?Rl<0<2A,0*4F",
        "!AIVDM,1,1,,B,15NCqegP00I=C7T@o:W@0?v:0@3a,0*38",
        "!AIVDO,1,1,,,15Mw0k0001q>Ac4@lk?p376<05H`,0*73",
        "!AIVDM,1,1,,B,15Mo930000q=`2L@kavcfRf>0<2D,0*12",
        "!AIVDM,1,1,,B,15N0=oPP00I>0PR@k8n7@?v<0D2=,0*4C",
        "!AIVDM,1,1,,A,15MwOK0P00I>KGN@mbL8>wv>0@3j,0*5A",
        "!AIVDM,1,1,,B,?03OwnQGlF9PD00,2*5C",
        "$GPHDT,227.0,T*32",
        "$GPROT,0.0,A*31",
        "$GPVTG,206.4,T,203.1,M,0.1,N,0.1,K*4E"
    };

    PickWindow *pickWindow;
    SearchWindow *searchWindow;
    QTextEdit *logText;

    void loadPlugin();
    void loadPluginAis();
    void onNmeaReceived(const QString&);

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

    void onSearch();

    void onColorScheme(QAction *);
    void onGreyMode(bool);

    void onScale(int);
    void onProjection();
    void onMouseMove(EcCoordinate, EcCoordinate);
    void onMouseRightClick();

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
    bool showLights, showText, showNationalText, showSoundings, showGrid, showAIS;

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
};

#endif // _mainwindow_h_
