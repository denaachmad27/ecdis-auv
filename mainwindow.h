#ifndef _mainwindow_h_
#define _mainwindow_h_

// #include <QtGui>
#include <QtWidgets>
#include <QDebug>

#include "IAisDvrPlugin.h"
#include "ecwidget.h"
#include "guardzonepanel.h"
#include "aoipanel.h"
#include "poipanel.h"
#include "alertpanel.h"
#include "alertsystem.h"
#include "aistargetpanel.h"
#include "obstacledetectionpanel.h"
#include "routepanel.h"
#include "SettingsData.h"
#include "compasswidget.h"

#include <QPluginLoader>

#include "cpatcpacalculator.h"
#include "cpatcpasettings.h"
#include <QTimer>
#include "cpatcpapanel.h"
#include "tidemanager.h"
#include "tidepanel.h"
#include "testpanel.h"

// forward declerations
class PickWindow;
class SearchWindow;
class Ais;
class AISSubscriber;
class QFile;

// Defines for S-63 Chart Import
// user permit (must have 28 characters)
//#define USERPERMIT "66B5CBFDF7E4139D5B6086C23130"
// manufacturer key (M_KEY must have either 5 or 10 characters)
//#define M_KEY "10121"
// data server id

#define DSID "0"
#define APP_TITLE "ECDIS AUV v1.62"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

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

    void onNmeaReceived(const QString&);
    void oriEditSetText(const double&);

    // ROUTES STATUS BAR - Hidden
    QLabel *routesStatusText;

    // RECONNECT STATUS BAR - Hidden
    QLabel *reconnectStatusText;
    void setReconnectStatusText(const QString);

    // POSTGRE STATUS BAR
    QLabel *postgreLedCircle;
    QLabel *postgreStatusText;
    bool m_isDatabaseConnected;
    SettingsData getSettingsForwarder();

    // COMPASS
    CompassWidget *compass = nullptr;
    void setCompassHeading(const int &hdg);
    void setCompassRotation(const int &rot);
    void setCompassHeadRot(const int &rot);

    enum class PlayerState { Stopped, Playing, Paused };

public slots:
    void onEnableRedDotTracker(bool enabled);
    void onAttachRedDotToShip(bool attached);

    // Enable and Disable icon
    void onMoosConnectionStatusChanged(bool connected);
    bool isMoosConnected() const { return conn; }

    // Tracking status update
    void updateTrackingStatus(const QString& mode, const bool& change);

    // Node Ships Panel
    void updateNodeShipsPanel();
    bool getNodeShipVisibility(const QString& nodeName) const;

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

    void onAIS();
    void onAIS(bool);
    void onOwnship(bool);
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
    void stopSubscribeMOOSDB();

    // Route - Comprehensive navigation planning
    void onCreateRoute();
    void onCreateRouteByForm();
    void onEditRouteByForm(int routeId);
    void onToggleRoutePanel();
    void onEditRoute();
    void onInsertWaypoint();
    void onMoveWaypoint();
    void onDeleteWaypoint();
    void onDeleteRoute();
    void onImportRoute();
    void onExportRoute();
    void onExportAllRoutes();
    void onClearRoutes();
    void onWaypointCreated(); // Keep for backward compatibility

    void createDockWindows();
    //void createLogPanel();
    void addTextToBar(QString);
    void createActions();
    void onClearWaypoints();
    void onExportWaypoints();
    void onImportWaypoints();

    void openSettingsDialog();
    void restartApplication();
    void openReleaseNotesDialog();
    void fetchNmea();
    void debugPurpose();
    void setDisplay();

    void openRouteManager();

    // Guardzone
    void onEnableGuardZone(bool enabled);
    void onCreateCircularGuardZone();
    void onCreatePolygonGuardZone();
    void onCheckGuardZone();
    void onAttachGuardZoneToShip(bool attached);
    void onAttachToShipStateChanged(bool attached);

    // GuardZone Panel slots
    void onGuardZoneSelected(int guardZoneId);
    void onGuardZoneEditRequested(int guardZoneId);
    void onGuardZoneVisibilityChanged(int guardZoneId, bool visible);

    // AOI
    void onCreateAOIByClick();
    void onOpenAOIPanel();
    void onOpenPOIPanel();
    void onToggleAoiLabels(bool checked);
    // EBL/VRM
    void onToggleEBL(bool checked);
    void onToggleVRM(bool checked);
    void onStartMeasure();

    // Route Panel slots
    void onRouteSelected(int routeId);
    void onRouteVisibilityChanged(int routeId, bool visible);

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
    QLineEdit   *rngEdit, *posEdit, *proEdit, *sclEdit, *oriEdit, *clockEdit;

    // Top-right tracking status widget
    QLabel *trackingStatusWidget;

    QString      dencPath;
    QString      impPath;

    int lookupTable, displayCategory;
    bool showLights, showText, showNationalText, showSoundings, showGrid, showAIS, trackShip, showDangerTarget, showOwnship;
    bool savedDangerTargetState; // Store previous state when ownship is disabled

    QActionGroup *dencActionGroup;
    QAction *autoProjectionAction, *mercatorAction, *gnomonicAction, *stereographicAction;
    QAction *baseAction, *standardAction, *otherAction;
    QAction *simplifiedAction, *fullChartAction;
    QAction *dayAction, *duskAction, *nightAction;
    QAction *logfileAction, *serverAction;

    QAction* startAisRecAction;
    QAction* stopAisRecAction;
    QAction* attachToShipAction;
    QAction* aisDangerAction = nullptr; // AIS Dangerous Box action pointer
    // Measurement actions to sync check state
    QAction* eblAction = nullptr;
    QAction* vrmAction = nullptr;

    IAisDvrPlugin* aisDvr;
    AISSubscriber  *aisSub;

private slots:
    // CPA/TCPA slots
    void onCPASettings();
    void onShowCPATargets(bool enabled);
    void onShowDangerTargets(bool enabled);
    void onShowTCPAInfo(bool enabled);
    void onCPATCPAAlarms(bool enabled);

    // ORIENTATION MODE
    void onNorthUp(bool);
    void onHeadUp(bool);
    void onCourseUp(bool);

    // OS CENTERING MODE
    void onAutoRecenter(bool);
    void onCentered(bool);
    void onLookAhead(bool);
    void onManual(bool);

    // test guardzone
    void onTestGuardZone(bool enabled);

    void onAutoCheckShipGuardian(bool enabled);
    void onCheckShipGuardianNow();

    // GuardZone auto-check slots
    void onToggleGuardZoneAutoCheck(bool enabled);
    void onConfigureGuardZoneAutoCheck();
    void onShowGuardZoneStatus();

    // DARK MODE
    void setDarkMode();
    void setLightMode();
    void setDimMode();

    void onLogFileClicked(QListWidgetItem *item);
    void onPlayPauseClicked();
    void onStopClicked();
    void onRefreshClicked();
    void onSliderPressed();
    void onSliderValueChanged(int position);
    void onSliderReleased();
    void onPlaybackTimerTimeout();
    void onDrawTimerTimeout();

    // PLAYBACK DB
    void onPlayClickedDB();
    void onStopClickedDB();
    void processNextNmeaDataDB();
    void onIncreaseSpeedClickedDB();
    void onDecreaseSpeedClickedDB();
    void showLoadingDialog();
    void hideLoadingDialog();
    void loadAndStartPlayback(const QDateTime& startTime, const QDateTime& endTime);

    // Database connection status
    void onDatabaseConnectionStatusChanged(bool connected);
    bool getDatabaseConnectionStatus() const;

private:
    GuardZonePanel* guardZonePanel;
    QDockWidget* guardZoneDock;
    AOIPanel* aoiPanel = nullptr;
    QDockWidget* aoiDock = nullptr;
    AISTargetPanel* aisTargetPanel;
    QDockWidget* aisTargetDock;
    ObstacleDetectionPanel* obstacleDetectionPanel;
    QDockWidget* obstacleDetectionDock;
    void setupGuardZonePanel();
    void setupAOIPanel();
    void setupAISTargetPanel();
    void setupObstacleDetectionPanel();
    void setupTestingMenu();

    // Alert Panel
    AlertPanel* alertPanel;
    QDockWidget* alertDock;
    void setupAlertPanel();

    // Tide Panel
    TidePanel* tidePanel;
    TideManager* tideManager;
    QDockWidget* tideDock;
    void setupTidePanel();

    
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

    // Route Panel
    RoutePanel* routePanel;
    QDockWidget* routeDock;

    // POI Panel
    POIPanel* poiPanel = nullptr;
    QDockWidget* poiDock = nullptr;

    // Node Ships Panel
    QTableWidget* nodeShipsTable = nullptr;
    QDockWidget* nodeShipsDock = nullptr;
    QMap<QString, bool> nodeShipsVisibility; // Track visibility per node ship
    QTimer* nodeShipsUpdateTimer = nullptr;  // Throttled update timer
    bool nodeShipsNeedUpdate = false;
    QElapsedTimer nodeShipsUserInteractionTimer;  // Track user interaction
    bool nodeShipsIsUpdating = false;  // Flag to prevent concurrent updates

    // Helper methods untuk CPA/TCPA
    void setupCPATCPAPanel();
    void setupRoutePanel();
    void setupPOIPanel();
    void setupNodeShipsPanel();
    void onNodeShipVisibilityChanged(int row);
    void onNavigateToNodeShip(int row);
    void onNodeShipsUpdateTimer();  // Slot untuk timer update

    // Connection status bar
    QLabel *moosStatusLabel;

    QLabel *moosLedCircle;
    QLabel *moosStatusText;

    // Connection icon
    QAction *connectAct;
    QAction *disconnectAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *rotateRightAct;
    QAction *rotateLeftAct;
    QAction *settingAct;
    QAction *routeAct;
    QAction *areaAct;
    QAction *poiAct;
    QAction *addPoiAction;
    QAction *chartManagerAct;
    // Chart Manager Panel
    class ChartManagerPanel* chartManagerPanel = nullptr;
    QDockWidget* chartManagerDock = nullptr;
    QDockWidget* m_nmeaPlaybackDock = nullptr;
    void setupChartManagerPanel();

    // BAR BAR
    void createStatusBar();
    void createMenuBar();
    void userPermitGenerate();

    // MOOSDB MENU
    QAction *restartAction;
    QAction *stopAction;

    // THEME COLOR
    void setTitleBarDark(bool dark);
    void applyPalette(const QPalette &palette, const QString &styleName);
    void updateIcon(bool dark);

    bool conn = false;

    // Fungsi helper privat
    void setupUI();
    void setupConnections();
    void updatePlayerState(PlayerState newState);
    void resetUIState(const QString& statusMessage);
    void populateLogFiles();
    void seekToLine(qint64 targetLine); // Fungsi baru untuk "fast-forward"

    // --- Widget UI ---
    QListWidget *m_logListWidget;
    QTextEdit *m_logTextEdit;
    QPushButton *m_playPauseButton;
    QPushButton *m_stopButton;
    QPushButton *m_refreshButton;
    QSlider *m_slider;
    QSpinBox *m_speedSpinBox;

    // --- State & File Handling ---
    PlayerState m_playerState;
    QTimer *m_playbackTimer;
    QTimer *m_drawTimer;

    QString m_logDirectoryPath;
    QString m_selectedFilePath;
    qint64 m_totalLines;
    qint64 m_currentLine;

    QFile *m_logFile = nullptr;
    QTextStream *m_logStream = nullptr;

    // MENU
    QMenu *viewTopMenu;
    QMenu *viewMenu;

    // PLAYBACK DB
    QPushButton *m_playButtonDB;
    QPushButton *m_stopButtonDB;
    QPushButton *m_increaseSpeedButtonDB;
    QPushButton *m_decreaseSpeedButtonDB;
    QDateEdit *m_dateEditDB;
    QTimeEdit *m_startTimeEditDB;
    QTimeEdit *m_endTimeEditDB;
    QTextEdit *m_displayEditDB;
    QLabel *m_speedLabelDB;
    QProgressBar *m_progressBarDB;  // Progress bar for playback
    QDialog *m_loadingDialog;  // Loading dialog for data fetch

    QTimer *m_playbackTimerDB;
    QQueue<QVariantList> m_nmeaDataQueueDB;
    bool m_isPlayingDB = false;
    int m_totalNmeaDataCount = 0;  // Total data for progress tracking
    bool m_isLoadingData = false;  // Flag to prevent re-entrancy during data loading

    // Playback speed and timing
    double m_playbackSpeed = 1.0; // 1.0 = normal speed, 2.0 = 2x speed
    QDateTime m_lastPlaybackTimestamp; // Track last processed timestamp

    // Store last displayed data for 2-line display
    QString m_lastOwnshipLine;
    QString m_lastAistargetLine;

    };

#endif // _mainwindow_h_
