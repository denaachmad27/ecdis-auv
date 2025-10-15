// #include <QtGui>
#include <QtWidgets>
#include <QtWin>
#include <QTimer>
#include <QToolTip>
#include <QVector>
#include <limits>
#include <cmath>
#ifndef _WIN32
#include <QX11Info>
#endif

#include "ecwidget.h"
#include "waypointdialog.h"
#include "routeformdialog.h"
#include "routequickformdialog.h"
#include "routesafetyfeature.h"
#include "routedeviationdetector.h"
#include "alertsystem.h"
#include "ais.h"
#include "pickwindow.h"
#include "aistooltip.h"
#include "mainwindow.h"
#include "aoi.h"

// Waypoint
#include "SettingsManager.h"
#include "aisdatabasemanager.h"
#include "aivdoencoder.h"
#include "appconfig.h"
#include "editwaypointdialog.h"

#include <QTime>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMenu>
#include <QAction>
#include <QSvgRenderer>

// Guardzone
#include "IAisDvrPlugin.h"
#include "PluginManager.h"
#include "guardzonecheckdialog.h"
#include "guardzonemanager.h"
#include <QtMath>
#include <cmath>

// Helper: format decimal degrees to Deg-Min representation with hemisphere
static QString formatDegMinCoord(double value, bool isLat)
{
    double absVal = std::abs(value);
    int deg = static_cast<int>(std::floor(absVal));
    double minutes = (absVal - deg) * 60.0;
    QChar hemi;
    if (isLat) hemi = (value >= 0.0) ? 'N' : 'S';
    else hemi = (value >= 0.0) ? 'E' : 'W';
    return QString("%1\u00B0 %2' %3")
        .arg(deg)
        .arg(minutes, 0, 'f', 3)
        .arg(hemi);
}

int EcWidget::minScale = 100;
int EcWidget::maxScale = 50000000;

QThread* threadAIS = nullptr;
QTcpSocket* socketAIS = nullptr;
std::atomic<bool> stopThread;

QThread* threadAISMAP = nullptr;
QTcpSocket* socketAISMAP = nullptr;
std::atomic<bool> stopThreadMAP;

std::atomic<bool> stopFlag;

QTextEdit *nmeaText;
QTextEdit *aisText;
QTextEdit *ownShipText;

QTextEdit *aisTemp;
QTextEdit *ownShipTemp;

ShipStruct mapShip = {};
ShipStruct navShip = {};
ActiveRouteStruct activeRoute = {};

QString aivdo;
QString nmea;

// define for type of AIS overlay cell
#define AISCELL_IN_RAM

#define WAYPOINTCELL_IN_RAM


EcWidget::EcWidget (EcDictInfo *dict, QString *libStr, QWidget *parent)
: QWidget  (parent)
{
  denc              = NULL;
  dictInfo          = dict;
  currentLat        = qQNaN();
  currentLon        = qQNaN();
  currentHeading    = 0.0;
  currentScale      = 42000;
  projectionMode    = MercatorProjection;
  currentProjection = EC_GEO_PROJECTION_MERCATOR;
  view              = NULL;
  initialized       = false;
  currentLookupTable    = EC_LOOKUP_TRADITIONAL;
  currentColorScheme    = EC_DAY_BRIGHT;
  currentBrightness     = 100;
  currentGreyMode       = false;

  // PERBAIKAN: Set default position jika currentLat/Lon belum diset
  if (qIsNaN(currentLat) || qIsNaN(currentLon)) {
      // Default position (Jakarta, Indonesia - bisa disesuaikan)
      ownShip.lat = -6.2088;  // Jakarta latitude
      ownShip.lon = 106.8456; // Jakarta longitude
      qDebug() << "[INIT] Using default ownship position: Jakarta" << ownShip.lat << "," << ownShip.lon;
  } else {
      ownShip.lat = currentLat;
      ownShip.lon = currentLon;
  }
  ownShip.cog =     331;
  ownShip.sog =     13;
  ownShip.heading = 325;
  ownShip.length =  300;
  ownShip.breadth = 45;

  // Inisialisasi variabel GuardZone (yang sudah ada)
  currentGuardZone = EcFeature{nullptr, 0};
  guardZoneShape = GUARD_ZONE_CIRCLE;
  guardZoneRadius = 0.5;
  guardZoneWarningLevel = EC_WARNING_LEVEL;
  guardZoneActive = false;
  creatingGuardZone = false;
  guardZoneCenter = QPointF(-1, -1);
  pixelsPerNauticalMile = 100;
  guardZoneCenterLat = 0.0;
  guardZoneCenterLon = 0.0;
  guardZoneRadius = 0.5;
  guardZoneWarningLevel = EC_WARNING_LEVEL;
  guardZoneActive = false;
  creatingGuardZone = false;
  guardZoneAttachedToShip = false;
  newGuardZoneShape = GUARD_ZONE_CIRCLE;

  guardZoneManager = new GuardZoneManager(this);
  routeSafetyFeature = new RouteSafetyFeature(this, this);
  routeDeviationDetector = nullptr; // Will be initialized later
  connect(guardZoneManager, &GuardZoneManager::editModeChanged,
          [this](bool isEditing) {
              qDebug() << "GuardZone edit mode changed:" << isEditing;
              update();
          });

  connect(guardZoneManager, &GuardZoneManager::guardZoneModified,
          [this](int guardZoneId) {
              qDebug() << "GuardZone modified:" << guardZoneId;
              update();
          });

  connect(guardZoneManager, &GuardZoneManager::statusMessage,
          this, &EcWidget::statusMessage);

  // Initialize guardzone ID counter jika belum ada
  if (nextGuardZoneId == 0) {
      nextGuardZoneId = 1;
  }


  // Initialize test guardzone
      testGuardZoneEnabled = false;
      testGuardZoneRadius = 2.0;  // default 2 nautical miles
      testGuardZoneColor = QColor(255, 165, 0, 128);  // orange semi-transparent

  // Initialize auto-check system
  guardZoneAutoCheckTimer = new QTimer(this);
  guardZoneAutoCheckEnabled = true;  // PERBAIKAN: Default true agar auto-check langsung aktif
  guardZoneCheckInterval = 5000; // 5 detik default
  lastGuardZoneCheck = QDateTime::currentDateTime();

  connect(guardZoneAutoCheckTimer, &QTimer::timeout,
          this, &EcWidget::performAutoGuardZoneCheck);

  // PERBAIKAN: Start timer otomatis jika auto-check enabled by default
  if (guardZoneAutoCheckEnabled) {
      guardZoneAutoCheckTimer->start(guardZoneCheckInterval);
      qDebug() << "GuardZone auto-check system initialized and started";
  } else {
      qDebug() << "GuardZone auto-check system initialized but not started";
  }

  // Initialize Ship Guardian Circle
  redDotTrackerEnabled = false;
  redDotAttachedToShip = false;
  shipGuardianEnabled = false;
  redDotLat = 0.0;
  redDotLon = 0.0;
  redDotColor = QColor(255, 0, 0, 50);
  redDotSize = 8.0;
  guardianRadius = 0.5;  // PERBAIKAN: Set to 0.5 NM (half of previous size)
  guardianFillColor = QColor(255, 0, 0, 50);
  guardianBorderColor = QColor(255, 0, 0, 150);
  redDotGuardianEnabled = false;
  redDotGuardianId = -1;
  redDotGuardianName = "";

  // Inisialisasi Ship Guardian auto-check
  shipGuardianAutoCheck = false;
  shipGuardianCheckTimer = new QTimer(this);
  shipGuardianCheckTimer->setInterval(5000); // Check setiap 5 detik
  connect(shipGuardianCheckTimer, &QTimer::timeout, this, &EcWidget::checkShipGuardianZone);

  // Initialize feedback system
  feedbackMessage = "";
  feedbackType = FEEDBACK_INFO;
  flashOpacity = 0;

  // Connect timer untuk auto-hide feedback
  connect(&feedbackTimer, &QTimer::timeout, [this]() {
      feedbackMessage.clear();
      update();
  });

  // Initialize Alert System
  alertSystem = nullptr;
  alertMonitoringEnabled = true;
  lastDepthReading = 0.0;
  depthAlertThreshold = 5.0;  // 5 meters
  proximityAlertThreshold = 0.5;  // 0.5 nautical miles
  autoDepthMonitoring = true;
  autoProximityMonitoring = true;

  // Initialize alert check timer
  alertCheckTimer = new QTimer(this);
  connect(alertCheckTimer, &QTimer::timeout, this, &EcWidget::performPeriodicAlertChecks);
  alertCheckTimer->start(10000); // Check every 10 seconds

  // Initialize chart flashing for dangerous obstacles
  chartFlashTimer = new QTimer(this);
  chartFlashVisible = false;
  connect(chartFlashTimer, &QTimer::timeout, [this]() {
      chartFlashVisible = !chartFlashVisible;
      update(); // Trigger repaint
  });

  // Initialize waypoint animation timer for consistent highlighting
  waypointAnimationTimer = new QTimer(this);
  waypointAnimationTimer->setInterval(50); // 20 FPS for smooth animation
  connect(waypointAnimationTimer, &QTimer::timeout, [this]() {
      if (highlightedWaypoint.visible) {
          update(); // Trigger repaint for animation
      }
  });

  // Initialize alert system (delayed to ensure EcWidget is fully constructed)
  QTimer::singleShot(100, this, &EcWidget::initializeAlertSystem);

  // PERBAIKAN: Initialize AlertSystem SEBELUM other setup
  qDebug() << "[ECWIDGET] About to initialize Alert System...";
  initializeAlertSystem();

  // Verify AlertSystem initialization
  if (alertSystem) {
      qDebug() << "[ECWIDGET] ✓ AlertSystem initialized successfully at:" << alertSystem;
  } else {
      qCritical() << "[ECWIDGET] ✗ AlertSystem initialization FAILED!";
  }

  // Initialize Route Deviation Detector
  QTimer::singleShot(150, this, &EcWidget::initializeRouteDeviationDetector);

  //popup
  // Initialize AIS tooltip
  aisTooltip = new AISTooltip(this);
  createAISTooltip();
  // Initialize AIS tooltip timer
  aisTooltipTimer = new QTimer(this);
  aisTooltipTimer->setSingleShot(true);
  aisTooltipTimer->setInterval(500); // 500ms delay sebelum tooltip muncul
  connect(aisTooltipTimer, &QTimer::timeout, this, &EcWidget::checkMouseOverAISTarget);

  isAISTooltipVisible = false;

  // Enable mouse tracking untuk widget
  setMouseTracking(true);

  // Inisialisasi variabel simulasi
  simulationTimer = nullptr;
  simulationActive = false;
  autoCheckGuardZone = false;

  ownShipTimer = nullptr;
  ownShipInSimulation = false;
  ownShipSimCourse = 0.0;
  ownShipSimSpeed = 0.0;
  currentScenario = SCENARIO_STATIC_GUARDZONE;

  creatingGuardZone = false;
  newGuardZoneShape = GUARD_ZONE_CIRCLE;
  guardZonePoints.clear();
  currentMousePos = QPoint(0, 0);

  // Initialize next guardzone ID if not already done
  if (nextGuardZoneId == 0) {
      nextGuardZoneId = 1;
  }

  // Clear any existing guardzone list untuk start fresh
  guardZones.clear();

  // Inisialisasi random seed untuk simulasi
  qsrand(QTime::currentTime().msec());

  QByteArray tmp = libStr->toLatin1();
  strcpy(lib7csStr, tmp.data());

#ifdef _WIN32
  hPalette = NULL;
  hdc = NULL;
  hBitmap = NULL;
#else
  dpy = x11Info().display();
  cmap = x11Info().appColormap();
  drawGC = NULL;
#endif

  QDesktopWidget *dt = QApplication::desktop();
  if (dt->width() > 1024)
    view = EcChartViewCreate (dictInfo, EC_RESOLUTION_HIGH);
  else if (dt->width() > 800)
    view = EcChartViewCreate (dictInfo, EC_RESOLUTION_MEDIUM);
  else
    view = EcChartViewCreate (dictInfo, EC_RESOLUTION_LOW);

  if (!view)
    throw Exception("Cannot create view.");

  if (!initColors())
  {
    EcChartViewDelete(view);
    view = NULL;
    throw Exception("Cannot read color definitions.");
  }

  // Initialize the AIS overlay cell
  aisCellId = EC_NOCELLID;

  // Open the error log file which indicates discrepancies in presentation library
  errlog = fopen("errlog.txt", "w");
  EcChartSetErrorLog(view, errlog);

  //Increase the font size for Windows, very much depends on the used font
#ifdef _WIN32
  EcDrawSetTextSizeFactor(view, 1.5);
#endif

  //Don't draw overlapping symbols of the same type
  EcDrawSetSymbolFilter(view, True);

  //Define some hard coded chart display settings which should be set by the user as well
  //Safety Contour which more or less reflects the draft and influences the display of underwater hazards, displayed as thick black line
  double safetyContour = 10.0;
  EcChartSetSafetyContour(view, safetyContour);
  //Deep contour defines the limit between grey and light grey depth areas in safe waters
  EcChartSetDeepContour(view, safetyContour+5);
  //Shallow contour defines the limit between dark and light blue depth areas in unsafe waters
  EcChartSetShallowContour(view, safetyContour-5);
  //Safety depth influences the display of black and grey soundings
  EcChartSetSafetyDepth(view, safetyContour);

  //Indicate the outline of next better usages (magenta lines) and the currently loaded usages (grey line)
  EcChartSetShowUsages(view, True);

  //Don't show the overscale pattern (vertical lines)
  EcChartSetShowOverScale(view, False);

  setAttribute (Qt::WA_NoSystemBackground);
  setMouseTracking (true);

  // Routes (load first to populate waypointList from routes)
  loadRoutes(); // Muat route dari file JSON dan konversi ke waypoints

  // Waypoint (load after routes for single waypoints)
  loadWaypoints(); // Muat single waypoint dari file JSON

  // AOIs
  loadAOIs(); // Muat AOI dari file JSON

  // Convert legacy single waypoints to routes for compatibility
  convertSingleWaypointsToRoutes();

  // DEBUGGING: Comment loadGuardZones() to test without auto-loading
  // loadGuardZones();

  // EMERGENCY: Manually clear guardZones list to prevent loading existing attached guardzone
  guardZones.clear();
  attachedGuardZoneId = -1;
  attachedGuardZoneName.clear();
  qDebug() << "[EMERGENCY] Cleared guardZones list and attached guardzone ID";

  // Inisialisasi variabel GuardZone
  currentGuardZone = EcFeature{nullptr, 0}; // Inisialisasi dengan objek null
  guardZoneShape = GUARD_ZONE_CIRCLE;       // Default bentuk lingkaran
  guardZoneRadius = 0.5;                    // Default radius 0.5 mil laut
  guardZoneWarningLevel = EC_WARNING_LEVEL; // Default level peringatan
  guardZoneActive = true;                   // PERBAIKAN: Default aktif sesuai menu checkbox
  creatingGuardZone = false;                // Default tidak dalam mode pembuatan
  guardZoneCenter = QPointF(-1, -1);        // Posisi tidak valid
  pixelsPerNauticalMile = 100;              // Nilai default
  guardZoneCenterLat = 0.0;
  guardZoneCenterLon = 0.0;
  guardZoneRadius = 0.5;                    // Default radius 0.5 mil laut
  guardZoneWarningLevel = EC_WARNING_LEVEL; // Default level peringatan
  guardZoneActive = true;                   // PERBAIKAN: Default aktif sesuai menu checkbox
  guardZoneAttachedToShip = false;          // Default tidak terikat ke kapal

  attachedGuardZoneName = "";

  attachedAoiId = -1;

  // ====== INITIALIZE ROUTE/WAYPOINT SYSTEM ======
  // Initialize route variables
  isRouteMode = false;
  routeWaypointCounter = 1;
  currentRouteId = 1;
  activeFunction = PAN;

  // NOTE: Do not call loadWaypoints() again here; routes have already
  // been converted to waypoints above. Calling it again would clear
  // waypointList and drop route waypoints on startup.

  qDebug() << "[ECWIDGET] Route/Waypoint system initialized";

  buttonInit();
  iconUpdate(AppConfig::isDark());

  // SETTINGS STARTUP
  defaultSettingsStartUp();
}

void EcWidget::setEblVrmFixedTarget(double lat, double lon)
{
    // Set fixed EBL/VRM target to a specific position
    eblvrm.eblHasFixedPoint = true;
    eblvrm.eblFixedLat = lat;
    eblvrm.eblFixedLon = lon;
    eblvrm.setMeasureMode(false);
    eblvrm.setEblEnabled(true);
    eblvrm.setVrmEnabled(true);
    emit statusMessage(tr("EBL/VRM set from ownship to clicked point"));
    update();
}

/*---------------------------------------------------------------------------*/

EcWidget::~EcWidget ()
{

  // Simulasi Guardzone
    if (simulationTimer) {
        simulationTimer->stop();
        delete simulationTimer;
        simulationTimer = nullptr;
    }
    simulationActive = false;

    if (ownShipTimer) {
        ownShipTimer->stop();
        delete ownShipTimer;
        ownShipTimer = nullptr;
    }

    // Cleanup Alert System
    if (alertSystem) {
        delete alertSystem;
        alertSystem = nullptr;
    }

    if (alertCheckTimer) {
        alertCheckTimer->stop();
        delete alertCheckTimer;
        alertCheckTimer = nullptr;
    }

    if (waypointAnimationTimer) {
        waypointAnimationTimer->stop();
        delete waypointAnimationTimer;
        waypointAnimationTimer = nullptr;
    }

  // Release AIS object.
  if( _aisObj )
  {
    _aisObj->stopAnimation();

    int iCnt = 0;
    while( QCoreApplication::hasPendingEvents() == True && iCnt < 20 )
    {
#ifdef _WINNT_SOURCE
      Sleep( 100 );
 #endif
      iCnt++;
    }

    if( QObject::disconnect( _aisObj, 0, this, 0 ) == False )
    {
      QObject::disconnect( _aisObj, 0, this, 0 );
    }

    deleteAISCell();

    _aisObj = NULL;
  }

  if (view)
  {
    // Release graphic resources
    EcDrawEnd (view);
    // Unmap all cells from the view
    EcChartUnloadView(view);
    // Release the view structure
    EcChartViewDelete (view);
    view = NULL;
  }
  // Release the DENC structure
  if (denc)
  {
    EcDENCDelete(denc);
    denc = NULL;
  }

  // Release the error log file
  if (errlog)
  {
    fclose(errlog);
  }

  // Cleanup AIS tooltip
  if (aisTooltip) {
      delete aisTooltip;
      aisTooltip = nullptr;
  }

  if (aisTooltipTimer) {
      aisTooltipTimer->stop();
      delete aisTooltipTimer;
      aisTooltipTimer = nullptr;
  }
}

/*---------------------------------------------------------------------------*/

void EcWidget::GetCenter (EcCoordinate & lat, EcCoordinate & lon) const
{
  lat = currentLat;
  lon = currentLon;
}

/*---------------------------------------------------------------------------*/

double EcWidget::GetRange (int scaleVal) const
{
  return EcDrawScaleToRange (view, scaleVal);
}

/*---------------------------------------------------------------------------*/


QString EcWidget::GetProjectionName () const
{
  QString str = "Unknown";
  switch (currentProjection)
  {
  case EC_GEO_PROJECTION_CYLINDRIC:
  case EC_GEO_PROJECTION_NONE:
    str = "Cylindrical Equidistant";
    break;
  case EC_GEO_PROJECTION_MERCATOR:
    str = "Mercator";
    break;
  case EC_GEO_PROJECTION_POLAR_STEREOGRAPHIC:
    str = "Polar Stereographic";
    break;
  case EC_GEO_PROJECTION_STEREOGRAPHIC:
    str = "Stereographic";
    break;
  case EC_GEO_PROJECTION_GNOMONIC:
    str = "Gnomonic";
    break;
  }
  return str;
}

/*---------------------------------------------------------------------------*/

void EcWidget::SetCenter (EcCoordinate lat, EcCoordinate lon)
{
  currentLat = lat;
  currentLon = lon;
  // Invalidate AOI screen cache due to view change
  viewChangeCounter++;
  if (currentScale > maxScale) currentScale = maxScale; // in case the world overview has been shown before
  // Check projection because it depends on viewport
  SetProjection(projectionMode);
}

/*---------------------------------------------------------------------------*/

void EcWidget::SetScale (int newScale)
{
  currentScale = newScale;
  if (currentScale < minScale) currentScale = minScale;
  if (currentScale > maxScale) currentScale = maxScale;
  // Invalidate AOI screen cache due to view change
  viewChangeCounter++;
  // Check projection because it depends on viewport
  SetProjection(projectionMode);
}

/*---------------------------------------------------------------------------*/

void EcWidget::SetHeading (double newHeading)
{
  // ROUTE FIX: Flush SevenCs cache when heading changes significantly to prevent stale route rendering
  static double lastHeading = -999.0;
  bool headingChanged = (qAbs(newHeading - lastHeading) > 0.1);

  currentHeading = newHeading;
  if (currentScale > maxScale) currentScale = maxScale; // in case the world overview has been shown before

  // Invalidate AOI screen cache due to view change
  viewChangeCounter++;

  // Flush SevenCs drawing cache when heading changes to prevent double route display
  if (headingChanged && view && initialized) {
    qDebug() << "[ROUTE-FIX] Heading changed significantly, flushing SevenCs cache";
    EcDrawFlushCache(view);
    lastHeading = newHeading;
  }

  // Check projection because it depends on viewport
  SetProjection(projectionMode);
}

/*---------------------------------------------------------------------------*/

void EcWidget::SetProjection (ProjectionMode p)
{
  projectionMode = p;
  double ll_lat = 0, ll_lon = 0, ur_lat = 0, ur_lon = 0;

  switch (projectionMode)
  {
  case AutoProjection:
    EcProjectionType pt;
    // Get the advised projection for the current viewport
    if (EcDrawAdviseProjection(view, currentLat, currentLon, GetRange(currentScale), currentHeading, &pt))
    {
      if (pt != EC_GEO_PROJECTION_NONE)
        currentProjection = pt;
    }
    break;
  case MercatorProjection:
    currentProjection = EC_GEO_PROJECTION_MERCATOR;
    // Plese note that Mercator does not work properly in polar regions.
    // Therefore check if the new viewport will extend 85� north or south
    //EcDrawSetProjection(view, currentProjection, currentLat, currentLon, 0, 0);
    if (EcDrawSetViewport(view, currentLat, currentLon, GetRange(currentScale), currentHeading))
    {
      EcDrawGetViewportBoundingBox( view, &ll_lat, &ll_lon, &ur_lat, &ur_lon );
      if ( fabs(ur_lat) > 80.0 || fabs(ll_lat) > 80.0 || fabs(currentLat) > 80.0)
      {
        //QMessageBox::information(this, "Set Projection", "Viewport extends to polar regions, switched to Stereographic.\nThis note can be disabled");
        currentProjection = EC_GEO_PROJECTION_STEREOGRAPHIC;
      }
    }
    break;
  case GnomonicProjection:
    currentProjection = EC_GEO_PROJECTION_GNOMONIC;
    break;
  case StereographicProjection:
    currentProjection = EC_GEO_PROJECTION_STEREOGRAPHIC;
    break;
  }
}

/*---------------------------------------------------------------------------*/

void EcWidget::SetColorScheme (int newScheme, bool greyMode, int brightness)
{
  currentColorScheme = newScheme;
  currentGreyMode    = greyMode;
  currentBrightness  = brightness;

#ifdef _WIN32
  if (hPalette)
    DeleteObject (hPalette);
  hPalette = (HPALETTE)EcDrawNTSetColorSchemeExt (view, NULL, currentColorScheme, greyMode, brightness, 1);
#else
  EcDrawX11SetColorScheme (view, dpy, cmap, currentColorScheme, greyMode, brightness);
#endif

  EcDrawFlushCache(view);

  int red, green, blue;
  EcDrawGetTokenRGB (view, const_cast<char*>("NODTA"), currentColorScheme, &red, &green, &blue);
  bg.setRgb (red, green, blue);
}

/*---------------------------------------------------------------------------*/

void EcWidget::SetDisplayCategory(int dc)
{
  EcChartSetViewClass(view, dc);
}

/*---------------------------------------------------------------------------*/

void EcWidget::SetLookupTable(int lut)
{
  currentLookupTable = lut;
  EcChartSetLookupTable(view, lut);
}

/*---------------------------------------------------------------------------*/

void EcWidget::ShowLights(bool on)
{
  EcChartSetShowLightFeatures(view, on);
}

/*---------------------------------------------------------------------------*/

void EcWidget::ShowText(bool on)
{
  EcChartSetShowText(view, on);
}

/*---------------------------------------------------------------------------*/

void EcWidget::ShowSoundings(bool on)
{
  EcChartSetShowDeepSoundings(view, on);
  EcChartSetShowShallowSoundings(view, on);
}

/*---------------------------------------------------------------------------*/

void EcWidget::ShowGrid(bool on)
{
  showGrid = on;
}

/*---------------------------------------------------------------------------*/

void EcWidget::ShowAIS(bool on)
{
  showAIS = on;
  int vg = 54030;	// Viewing group for AIS targets

  if(on)
  {
      EcChartSetViewingGroup(view, EC_VG_SET, &vg, 1);
  }
  else
  {
    EcChartSetViewingGroup(view, EC_VG_CLEAR, &vg, 1);
  }
}

void EcWidget::TrackTarget(QString mmsi){
    trackTarget = mmsi;
}

bool EcWidget::isTrackTarget(){
    return !trackTarget.isEmpty();
}

void EcWidget::TrackShip(bool on)
{
  trackShip = on;
}

void EcWidget::ShowDangerTarget(bool on)
{
  showDangerTarget = on;
}

/*---------------------------------------------------------------------------*/

bool EcWidget::XyToLatLon (int x, int y, EcCoordinate & lat, EcCoordinate & lon)
{
  return EcDrawXyToLatLon(view, x, y, &lat, &lon);
}

/*---------------------------------------------------------------------------*/

bool EcWidget::LatLonToXy (EcCoordinate lat, EcCoordinate lon, int & x, int & y)
{
  return EcDrawLatLonToXy(view, lat, lon, &x, &y);
}

/*---------------------------------------------------------------------------*/

bool EcWidget::CreateDENC(const QString & dp, bool updateCatalog)
{
  dencPath = dp;
  QByteArray tmp = dencPath.toLatin1();
  denc = EcDENCCreate(dictInfo, tmp.constBegin(), updateCatalog);
  if (denc == NULL) return false;

  // Create a so-called installation passport which contains all necessary information for charts purchased from Chartworld
  EcDENCCreateInstallationPass(denc, "InstallationPass.txt", "showDENC", "myPC", NULL, NULL, NULL, NULL);

  // If DENC path is still empty, import the world data
  QString cellDirName = dencPath + "/CELLS";
  QDir cellDir(cellDirName);
  if (cellDir.count() == 2) // . and .. count
  {
    if (EcKernelGetEnv("EC2007DIR"))
    {
      QString impPath = QString(EcKernelGetEnv("EC2007DIR")) + "/data/World/12Mio_Usage1";
      int nCharts = ImportTree(impPath);
      if (nCharts == 0)
      {
        QString hlpStr = "No charts found in " + impPath;
        QMessageBox::information(this, "Initialize DENC", hlpStr);
      }
    }
  }


  // Define the application's certificate and S-63 permit file
  // These are the directories where ChartHandler expects or creates the files
  QString certificateDirName = dencPath + "/CRTS";
  QDir certificateDir(certificateDirName);
  if (!certificateDir.exists())
  {
      certificateDir.mkdir(certificateDirName);
  }
  certificateFileName = certificateDirName + "/IHO.CRT";
  s63permitFileName = dencPath + "/S63permits.txt";

  return true;
}

/*---------------------------------------------------------------------------*/

int EcWidget::ImportTree(const QString & dir)
{
  if (denc == NULL) return -1;
  QByteArray tmp = dir.toLatin1();
  // Before new data are loaded all cells should be removed from the view
  EcChartUnloadView(view);
  EcDENCSetCallbackExt(denc, ImportCB, this);
  int ret =  EcDENCImportTree(denc, dictInfo, tmp.constBegin());
  EcDENCRemoveCallbackExt(denc);
  return ret;
}

/*---------------------------------------------------------------------------*/

int EcWidget::ImportS57ExchangeSet(const QString & dir)
{
    if (denc == NULL) return -1;
    QByteArray tmp = dir.toLatin1();
    // Before new data are loaded all cells should be removed from the view
    EcChartUnloadView(view);

    char catName[256];
    sprintf(catName,"%s/CATALOG.031", tmp.constBegin());
    EcS57ExchangeSet *exSet = EcS57V3CatalogueRead(catName, NULL);
    int nVol = 0;
    int nFiles = EcS57V3CatalogueGetNumberOfFiles(exSet, &nVol);

    int ret =  EcDENCImportS57ExchangeSet(denc, dictInfo, tmp.constBegin(), S57CB, True);
    return ret;
}

/*---------------------------------------------------------------------------*/

bool EcWidget::ImportIHOCertificate(const QString & certificate)
{
    if(QFile::copy(certificate, certificateFileName))
        return true;
    else
        return false;
}

/*---------------------------------------------------------------------------*/

bool EcWidget::ImportS63Permits(const QString & s63Permits)
{
    QByteArray tmp;

    // Before new data are loaded all cells should be removed from the view
    EcChartUnloadView(view);

    QFile s63PermitFile(s63Permits);
    if( s63PermitFile.exists() && s63PermitFile.size() > 0 )
    {
        // release the Kernel internal permit list
        EcCellSetChartPermitList (NULL);
        tmp = s63Permits.toLatin1();
        int ret = EcS63ReadCellPermitsIntoPermitList(permitList, tmp.constBegin(), s63HwId, True, NULL, NULL);
        if(ret != 0)
            return false;
        // Update the overall S-63 permit file with the new permits, i.e. write the permit list to a file
        tmp = s63permitFileName.toLatin1();
        EcS63WriteCellPermitsToFile(permitList, tmp.constBegin());
        // Activate the Kernel internal permit list again
        EcCellSetChartPermitList (permitList);
    }
    return true;
}

/*---------------------------------------------------------------------------*/

bool EcWidget::ImportS63ExchangeSet(const QString & dir)
{
    QFile tmpFile;

    // Check if right source directory has been selected
    QString catalogue = dir + "/CATALOG.031";
    tmpFile.setFileName(catalogue);
    if(!tmpFile.exists())
    {
        QString hlpStr = QString("No CATALOG.031 file found in %1").arg(dir);
        QMessageBox::critical(this, "showDENC - S-63", hlpStr);
        return false;
    }

    // Check if S-63 permit file exists
    tmpFile.setFileName(s63permitFileName);
    if(!tmpFile.exists())
    {
        QString hlpStr = QString("No S-63 permits (%1) found").arg(dencPath);
        QMessageBox::critical(this, "showDENC - S-63", hlpStr);
        return false;
    }

    // Check if the IHO certificate exists
    tmpFile.setFileName(certificateFileName);
    if(!tmpFile.exists())
    {
        QString hlpStr = QString("No IHO.CRT file found in %1").arg(dencPath);
        QMessageBox::critical(this, "showDENC - S-63", hlpStr);
        return false;
    }

    QByteArray tmp1 = dir.toLatin1();
    QByteArray tmp2 = s63permitFileName.toLatin1();
    QByteArray tmp3 = certificateFileName.toLatin1();
    int errNo = 0;

    if (!EcS63ImportExt(denc, dictInfo, s63HwId, tmp1.constBegin(), tmp2.constBegin(), tmp3.constBegin(), True, NULL, &errNo, False))
    {
        QString hlpStr = QString("Import of S-63 Exhange Set failed (error = %1)").arg(errNo);
        QMessageBox::critical(this, "showDENC - S-63", hlpStr);
        return false;
    }

    return true;
}
/*---------------------------------------------------------------------------*/

int EcWidget::ApplyUpdate()
{
  if (denc == NULL) return -1;
  int ret = 0;
  if (EcDENCCheckUpdates(denc, dictInfo))
  {
    ret = EcDENCApplyUpdates(denc, dictInfo);
  }
  return ret;
}

// Protected members
/*---------------------------------------------------------------------------*/

void EcWidget::drawWorks(bool upd)
{
  if (!initialized) return;

  clearBackground();

  // draw the outline ship symbols if scale is bigger than 1:10000
  if(currentScale < 10000)
  {
    EcChartSetOutlinedShipSymbol(view, True);
    EcChartSetOutlinedTargetSymbol(view, True);
  }
  else
  {
    EcChartSetOutlinedShipSymbol(view, False);
    EcChartSetOutlinedTargetSymbol(view, False);
  }

  EcDrawSetProjection(view, currentProjection, currentLat, currentLon, 0, 0);

  if (denc != NULL)
  {
    // Draw chart to chartPixmap
    EcCatList *catList = EcDENCGetCatalogueList(denc);
    #ifdef _WIN32
        HPALETTE oldPal = SelectPalette (hdc, hPalette, true);

        // Temporarily unassign the AIS overlay cell from the view to get a pure chart image
        aisCellId = _aisObj->getAISCell();
        if( aisCellId != EC_NOCELLID )
          EcChartUnAssignCellFromView(view, aisCellId);

        // Draw the charts
        EcDrawNTDrawChart (view, hdc, NULL, dictInfo, catList, currentLat, currentLon, GetRange(currentScale), currentHeading);

        // Assign the AIS overlay cell again
        if( aisCellId != EC_NOCELLID )
          EcChartAssignCellToView(view, aisCellId);

        if(showGrid)
          EcDrawNTDrawGrid (view, hdc, chartPixmap.width(), chartPixmap.height(), 8, 8, True);

        chartPixmap = QtWin::fromHBITMAP(hBitmap);
        drawPixmap = chartPixmap;
        SelectPalette (hdc, oldPal, false);
    #else
        #if QT_VERSION > 0x040400
            EcDrawX11DrawChart (view, drawGC, x11pixmap, dictInfo, catList, currentLat, currentLon, GetRange(currentScale), currentHeading);

            if(showGrid)
              EcDrawX11DrawGrid (view, drawGC, x11pixmap, chartPixmap.width(), chartPixmap.height(), 8, 8, True);

            chartPixmap = QPixmap::fromX11Pixmap(x11pixmap);
            drawPixmap = chartPixmap;
        #else
            EcDrawX11DrawChart (view, drawGC, chartPixmap.handle(), dictInfo, catList, currentLat, currentLon, GetRange(currentScale), currentHeading);

            if(showGrid)
              EcDrawX11DrawGrid (view, drawGC, chartPixmap.handle(), chartPixmap.width(), chartPixmap.height(), 8, 8, True);

            drawPixmap = chartPixmap;
        #endif
    #endif
  }
  else
  {
    QMessageBox::critical( this, tr( "showAIS - Drawing" ), tr( "DENC structure could not be found" ) );
    return;
  }

  if (upd) update();

  emit projection();
  emit scale(currentScale);
}

// AUV WORKS
void EcWidget::draw(bool upd)
{
    static bool inDraw = false;
    if(inDraw) return;        // ❌ prevent reentrant crash
    if(!initialized) return;

    inDraw = true;

    clearBackground();

    // Outline ship/target symbols
    if(currentScale < 10000)
    {
        EcChartSetOutlinedShipSymbol(view, True);
        EcChartSetOutlinedTargetSymbol(view, True);
    }
    else
    {
        EcChartSetOutlinedShipSymbol(view, False);
        EcChartSetOutlinedTargetSymbol(view, False);
    }

    EcDrawSetProjection(view, currentProjection, currentLat, currentLon, 0, 0);

    if(!denc) {
        QMessageBox::critical(this, tr("showAIS - Drawing"), tr("DENC structure could not be found"));
        inDraw = false;
        return;
    }

    EcCatList *catList = EcDENCGetCatalogueList(denc);
    if(!catList) {
        inDraw = false;
        return;
    }

    // ----------------------------
    // Buat pixmap lebih besar untuk extra margin
    // ----------------------------
    // const int extra_margin = 200;
    // QPixmap extendedPixmap(width() + 2*extra_margin, height() + 2*extra_margin);
    // extendedPixmap.fill(QColor(204, 197, 123)); // background

#ifdef _WIN32
    if(!hdc || !hBitmap) { inDraw = false; return; }
    HPALETTE oldPal = SelectPalette(hdc, hPalette, true);

    EcCellId aisCellId = (_aisObj) ? _aisObj->getAISCell() : EC_NOCELLID;
    if(aisCellId != EC_NOCELLID)
        EcChartUnAssignCellFromView(view, aisCellId);

    EcDrawNTDrawChart(view, hdc, NULL, dictInfo, catList, currentLat, currentLon, GetRange(currentScale), currentHeading);

    if(aisCellId != EC_NOCELLID)
        EcChartAssignCellToView(view, aisCellId);

    if(showGrid)
        EcDrawNTDrawGrid(view, hdc, chartPixmap.width(), chartPixmap.height(), 8, 8, True);

    if(hBitmap)
        chartPixmap = QtWin::fromHBITMAP(hBitmap);

    drawPixmap = chartPixmap;
    SelectPalette(hdc, oldPal, false);

#else
    #if QT_VERSION > 0x040400
        if(!drawGC || !x11pixmap) { inDraw = false; return; }

        EcDrawX11DrawChart(view, drawGC, x11pixmap, dictInfo, catList, currentLat, currentLon, GetRange(currentScale), currentHeading);

        if(showGrid)
            EcDrawX11DrawGrid(view, drawGC, x11pixmap, chartPixmap.width(), chartPixmap.height(), 8, 8, True);

        chartPixmap = QPixmap::fromX11Pixmap(x11pixmap);
        drawPixmap = chartPixmap;
    #else
        if(!drawGC) { inDraw = false; return; }

        EcDrawX11DrawChart(view, drawGC, chartPixmap.handle(), dictInfo, catList, currentLat, currentLon, GetRange(currentScale), currentHeading);

        if(showGrid)
            EcDrawX11DrawGrid(view, drawGC, chartPixmap.handle(), chartPixmap.width(), chartPixmap.height(), 8, 8, True);

        drawPixmap = chartPixmap;
    #endif
#endif

    // Copy ke extendedPixmap agar ada margin ekstra
    // QPainter p(&extendedPixmap);
    // p.drawPixmap(extra_margin, extra_margin, chartPixmap); // titik tengah chart tetap di tengah pixmap
    // p.end();

    // drawPixmap = extendedPixmap;

    if(upd) update();

    emit projection();
    emit scale(currentScale);

    inDraw = false;
}


/*---------------------------------------------------------------------------*/

void EcWidget::Draw()
{    
    draw(true);

    // ROUTE FIX: Enhanced conditional drawAISCell() for route stability
    // Always call drawAISCell() if routes exist to ensure proper pixmap management
    bool hasRoutes = !waypointList.isEmpty();
    if(showAIS || hasRoutes){
        drawAISCell();
    } else {
        // Only call waypointDraw() if no AIS and no routes (fallback case)
        waypointDraw();
    }

    // Tidak perlu memanggil drawGuardZone() di sini,
    // karena akan dipanggil secara otomatis di paintEvent

    // ========== REFRESH GUARDZONE HANDLE POSITIONS ==========
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        guardZoneManager->refreshHandlePositions();
    }
    // ======================================================

    // UPDATE WP TOOLBAR
    hideToolbox();

    update();
}

void EcWidget::waypointDraw(){
    //qDebug() << "[WAYPOINT-DRAW] Drawing" << waypointList.size() << "waypoints";

    // Gambar waypoint dengan warna konsisten
    for (const Waypoint &wp : waypointList)
    {
        // Check visibility - skip hidden routes
        if (wp.routeId > 0 && !isRouteVisible(wp.routeId)) {
            //qDebug() << "[WAYPOINT-DRAW] Skipping waypoint" << wp.label << "from hidden route" << wp.routeId;
            continue; // Skip waypoints from hidden routes
        }

        //qDebug() << "[WAYPOINT-DRAW] Drawing waypoint" << wp.label << "route:" << wp.routeId << "active:" << wp.active;
        QColor waypointColor;
        if (wp.active) {
            waypointColor = getRouteColor(wp.routeId);
        } else {
            waypointColor = QColor(128, 128, 128); // Grey for inactive waypoints
        }
        drawWaypointWithLabel(wp.lat, wp.lon, wp.label, waypointColor);
    }

    drawLeglineLabels();

    // Re-enable stable route drawing for presentation
    drawRouteLines();
}
/*---------------------------------------------------------------------------*/


void EcWidget::paintEvent (QPaintEvent *e)
{
    if (!initialized) return;
    QPainter painter(this);

    if (dragMode){
        painter.fillRect(rect(), QColor(204,197,123));

        // hitung offset supaya currentLat/currentLon ada di tengah window
        int x, y;
        LatLonToXy(currentLat, currentLon, x, y);
        QPoint centerOffset = QPoint(width()/2, height()/2) - QPoint(x, y);

        // live drag
        if (isDragging)
            centerOffset += tempOffset;

        // geser seluruh painter, semua layer ikut
        painter.translate(centerOffset);

        // gambar chart pixmap di (0,0)
        painter.drawPixmap(0, 0, drawPixmap);

        //painter.drawPixmap(e->rect(), drawPixmap, e->rect());
    }
    else {
        // ======== STABLE ========= //
        painter.drawPixmap(e->rect(), drawPixmap, e->rect());
        // ======== STABLE ========= //
    }



  // FORCE CLEANUP: Run cleanup from paint event as fallback
  static int paintCleanupCounter = 0;
  if (++paintCleanupCounter % 100 == 0) { // Every ~100 paint events (less frequent)
      qDebug() << "[PAINT-CLEANUP] Running obstacle cleanup from paintEvent";
      if (!obstacleMarkers.isEmpty()) {
          removeOutdatedObstacleMarkers();
      }
  }

  // Draw ghost waypoint saat move mode
  if (ghostWaypoint.visible) {
      drawGhostWaypoint(painter, ghostWaypoint.lat, ghostWaypoint.lon, ghostWaypoint.label);
  }

  // Draw highlighted waypoint for route panel selection
  if (highlightedWaypoint.visible) {
      drawHighlightedWaypoint(painter, highlightedWaypoint.lat, highlightedWaypoint.lon, highlightedWaypoint.label);
  }

  if (AppConfig::isDevelopment()){
      // drawGuardZone moved to use main painter later in paintEvent

      // TEMPORARY: Disabled untuk presentasi - obstacle area menyebukang crash
      // drawObstacleDetectionArea(painter); // Show obstacle detection area (now safe)
      //drawRedDotTracker();

      // Re-enabled with enhanced safety protections
      drawObstacleMarkers(painter); // Draw obstacle markers with color-coded dots

      // TEMPORARY DISABLE: Route lines drawing to prevent crash during presentation
      // drawRouteLinesOverlay(painter);

      // Draw chart flashing overlay for dangerous obstacles
      drawChartFlashOverlay(painter);
      drawRedDotTracker();
  }

  // Draw AOIs always on top of chart
  drawAOIs(painter);
  // Draw GuardZones using the same painter
  drawGuardZone(painter);
  // Draw Route Deviation Indicator
  drawRouteDeviationIndicator(painter);
  // Draw EBL/VRM overlays
  eblvrm.draw(this, painter);
  // Draw ship dot if enabled (debug/utility)
  drawShipDot(painter);
  // Draw AOI creation preview (including first-point ghost)
  if (creatingAOI && initialized && view) {
      painter.setRenderHint(QPainter::Antialiasing, true);

      // Current mouse position
      QPoint mp = lastMousePos;

      // Theme-aware colors
      QColor win = palette().color(QPalette::Window);
      int luma = qRound(0.2126*win.red() + 0.7152*win.green() + 0.0722*win.blue());
      bool darkTheme = (luma < 128);

      // Use pending AOI color for preview line, fallback to default
      QColor lineColor = pendingAOIColor.isValid() ? pendingAOIColor : aoiDefaultColor(pendingAOIType);
      QPen pen(lineColor);
      pen.setStyle(Qt::DashLine);
      pen.setWidth(3); // Match route creation ghost line thickness
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);

      // Build existing vertices (screen)
      QVector<QPoint> pts;
      for (const auto& ll : aoiVerticesLatLon) {
          int x=0,y=0; if (LatLonToXy(ll.x(), ll.y(), x, y)) pts.append(QPoint(x,y));
      }

      if (!pts.isEmpty()) {
          // Add current mouse position as last temp point and draw preview segments
          pts.append(mp);
          for (int i=1;i<pts.size();++i) painter.drawLine(pts[i-1], pts[i]);

          // Draw ghost circle at cursor for next point placement (like first-point preview)
          QPen ghostPen2(lineColor);
          ghostPen2.setStyle(Qt::DashLine);
          ghostPen2.setWidth(2);
          painter.setPen(ghostPen2);
          painter.drawEllipse(mp, 6, 6);
      } else {
          // First point preview: show a ghost vertex following the cursor
          QPen ghostPen(lineColor);
          ghostPen.setStyle(Qt::DashLine);
          ghostPen.setWidth(2);
          painter.setPen(ghostPen);
          painter.drawEllipse(mp, 6, 6);
      }

          // Show cursor lat/lon in deg-min near the cursor (obey showAoiLabels)
          EcCoordinate lat, lon;
          if (XyToLatLon(mp.x(), mp.y(), lat, lon)) {
              const QString latStr = latLonToDegMin(lat, true);
              const QString lonStr = latLonToDegMin(lon, false);

          // Compute distance from last vertex to cursor (segment length preview)
          QString distText;
          if (!aoiVerticesLatLon.isEmpty()) {
              const QPointF& last = aoiVerticesLatLon.last();
              double distNM = 0.0, bearing = 0.0;
              EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                     last.x(), last.y(),
                                                     lat, lon,
                                                     &distNM, &bearing);
              // Show only distance (NM) without bearing for cleaner UI
              distText = QString("\n%1 NM").arg(QString::number(distNM, 'f', 2));
          }

          // Compute perimeter (includes segment to cursor) and area (if >=3 points incl. cursor)
          QString perimText;
          QString areaText;
          if (!aoiVerticesLatLon.isEmpty()) {
              double perimNM = 0.0;
              // Build list including cursor as last point
              QVector<QPointF> ptsLL = aoiVerticesLatLon;
              ptsLL.append(QPointF(lat, lon));

              for (int i = 1; i < ptsLL.size(); ++i) {
                  double d=0.0, b=0.0;
                  EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                         ptsLL[i-1].x(), ptsLL[i-1].y(),
                                                         ptsLL[i].x(),   ptsLL[i].y(),
                                                         &d, &b);
                  perimNM += d;
              }
              perimText = QString("\nPerim: %1 NM").arg(QString::number(perimNM, 'f', 2));

              if (ptsLL.size() >= 3) {
                  // Approximate area in NM^2 using local tangent plane
                  double lat0 = 0.0, lon0 = 0.0;
                  for (const auto& p : ptsLL) { lat0 += p.x(); lon0 += p.y(); }
                  lat0 /= ptsLL.size(); lon0 /= ptsLL.size();
                  double lat0rad = lat0 * M_PI / 180.0;
                  double k = 60.0; // deg -> NM
                  QVector<QPointF> ptsXY; ptsXY.reserve(ptsLL.size());
                  for (const auto& p : ptsLL) {
                      double x = (p.y() - lon0) * k * std::cos(lat0rad);
                      double y = (p.x() - lat0) * k;
                      ptsXY.append(QPointF(x, y));
                  }
                  // Shoelace area for closed polygon: close back to first
                  double area = 0.0;
                  for (int i = 0; i < ptsXY.size(); ++i) {
                      const QPointF& a = ptsXY[i];
                      const QPointF& b = ptsXY[(i+1) % ptsXY.size()];
                      area += a.x()*b.y() - b.x()*a.y();
                  }
                  area = std::fabs(area) * 0.5; // NM^2
                  areaText = QString::fromUtf8("\nArea: %1 NM²").arg(QString::number(area, 'f', 2));
              }
          }

          // Cursor overlay: show coordinates and (if available) segment distance
          QString text = latStr + "  " + lonStr;
          if (!distText.isEmpty()) text += distText;

          QFont f = painter.font();
          f.setPointSizeF(9.0);
          f.setBold(false);
          painter.setFont(f);
          QFontMetrics fm(f);
          int pad = 4;
          QRect br = fm.boundingRect(QRect(0,0, 600, 2000), Qt::AlignLeft|Qt::AlignTop, text);
          QRect bg(mp.x() + 12, mp.y() + 12, br.width() + pad*2, br.height() + pad*2);

          if (showAoiLabels) {
              // Background for readability (theme-aware)
              painter.setPen(Qt::NoPen);
              QColor bgCol = darkTheme ? QColor(0,0,0,160) : QColor(255,255,255,210);
              QColor fgCol = darkTheme ? QColor(240,240,240) : QColor(30,30,30);
              painter.setBrush(bgCol);
              painter.drawRoundedRect(bg, 4, 4);

              // Text (theme-aware)
              painter.setPen(fgCol);
              painter.drawText(bg.adjusted(pad, pad, -pad, -pad), Qt::AlignLeft | Qt::AlignTop, text);
          }

          // Center label: show AOI title and area at polygon centroid during creation
          if (pts.size() >= 3) {
              // Compute screen centroid
              double cx = 0.0, cy = 0.0;
              for (const QPoint& p : pts) { cx += p.x(); cy += p.y(); }
              cx /= pts.size(); cy /= pts.size();

              QString title = pendingAOIName.isEmpty() ? QString("AOI %1").arg(nextAoiId) : pendingAOIName;
              QString areaLine = areaText.isEmpty() ? QString("") : areaText.mid(1); // remove leading \n
              if (showAoiLabels) {
                  QString centerText = title;
                  if (!areaLine.isEmpty()) centerText += "\n" + areaLine;

                  QFont tf = painter.font();
                  tf.setPointSizeF(10.0);
                  tf.setBold(true);
                  painter.setFont(tf);
                  QFontMetrics tfm(tf);
                  QRect tb = tfm.boundingRect(QRect(0,0, 800, 2000), Qt::AlignHCenter|Qt::AlignTop, centerText);
                  int w = tb.width();
                  int h = tb.height();
                  QRect labelRect(static_cast<int>(cx - w/2) - 6, static_cast<int>(cy - h/2) - 6,
                                  w + 12, h + 12);
                  // Background
                  painter.setPen(Qt::NoPen);
                  painter.setBrush(QColor(0,0,0,120));
                  painter.drawRoundedRect(labelRect, 4, 4);

                  // Text colored by pending AOI color
                  QColor aoiColor = pendingAOIColor.isValid() ? pendingAOIColor : aoiDefaultColor(pendingAOIType);
                  painter.setPen(aoiColor);
                  painter.drawText(labelRect, Qt::AlignCenter, centerText);
              }
          }
      }
  }


  // Draw AOI edit handles (vertex squares) when editing
  if (editingAOI && initialized && view) {
      const AOI* target = nullptr;
      for (const auto& a : aoiList) { if (a.id == editingAoiId) { target = &a; break; } }
      if (target && target->vertices.size() >= 3) {
          QPen pen(QColor(255,255,255)); pen.setWidth(1);
          QBrush hb(QColor(0,0,0));
          painter.setPen(pen);
          painter.setBrush(hb);
          for (const auto& ll : target->vertices) {
              int x=0,y=0; if (LatLonToXy(ll.x(), ll.y(), x, y)) {
                  painter.drawRect(x-4,y-4,8,8);
              }
          }

          // When moving a vertex (draggedAoiVertex >= 0), draw ghost polygon and live labels
          if (draggedAoiVertex >= 0) {
              // Cursor position and theme-aware colors
              QPoint mp = lastMousePos;
              QColor win = palette().color(QPalette::Window);
              int luma = qRound(0.2126*win.red() + 0.7152*win.green() + 0.0722*win.blue());
              bool darkTheme = (luma < 128);

              // Draw ghost dashed lines only for affected edges (prev->ghost) and (ghost->next)
              QVector<QPoint> ghostPts;
              ghostPts.reserve(target->vertices.size());
              for (int i = 0; i < target->vertices.size(); ++i) {
                  double vlat = target->vertices[i].x();
                  double vlon = target->vertices[i].y();
                  if (i == draggedAoiVertex && aoiVertexDragging) { vlat = aoiGhostLat; vlon = aoiGhostLon; }
                  int x=0,y=0; if (LatLonToXy(vlat, vlon, x, y)) ghostPts.append(QPoint(x,y));
              }
              if (ghostPts.size() >= 2) {
                  int n = ghostPts.size();
                  int gi = draggedAoiVertex;
                  if (gi >= 0 && gi < n) {
                      int pi = (gi - 1 + n) % n;
                      int ni = (gi + 1) % n;
                      QPen ghostPen(QColor(128,128,128)); ghostPen.setStyle(Qt::DashLine); ghostPen.setWidth(3);
                      painter.setPen(ghostPen); painter.setBrush(Qt::NoBrush);
                      painter.drawLine(ghostPts[pi], ghostPts[gi]);
                      painter.drawLine(ghostPts[gi], ghostPts[ni]);
                  }
              }

              // Live overlay: lat/lon at cursor with adjacent segment distances (obey showAoiLabels)
              EcCoordinate lat, lon;
              if (XyToLatLon(mp.x(), mp.y(), lat, lon)) {
                  const QString latStr = latLonToDegMin(lat, true);
                  const QString lonStr = latLonToDegMin(lon, false);

                  QString overlay = latStr + "  " + lonStr;
                  // Adjacent distances: prev->ghost and ghost->next
                  int n = target->vertices.size();
                  if (n >= 2 && draggedAoiVertex >= 0 && draggedAoiVertex < n) {
                      int prev = (draggedAoiVertex - 1 + n) % n;
                      int next = (draggedAoiVertex + 1) % n;
                      double d1=0.0, b1=0.0, d2=0.0, b2=0.0;
                      EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                             target->vertices[prev].x(), target->vertices[prev].y(),
                                                             lat, lon, &d1, &b1);
                      EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                             lat, lon,
                                                             target->vertices[next].x(), target->vertices[next].y(),
                                                             &d2, &b2);
                      overlay += QString("\nPrev: %1 NM  Next: %2 NM")
                                     .arg(QString::number(d1, 'f', 2))
                                     .arg(QString::number(d2, 'f', 2));
                  }

                  // Small ghost indicator at cursor
                  QPen ghostPen2(QColor(80,80,80));
                  ghostPen2.setStyle(Qt::DashLine);
                  ghostPen2.setWidth(2);
                  painter.setPen(ghostPen2);
                  painter.setBrush(Qt::NoBrush);
                  painter.drawEllipse(mp, 6, 6);

                  // Also draw segment distance labels for ghost-adjacent edges at midpoints with offset from line
                  if (ghostPts.size() == target->vertices.size()) {
                      int n = ghostPts.size();
                      int gi = draggedAoiVertex;
                      int pi = (gi - 1 + n) % n;
                      int ni = (gi + 1) % n;
                      // prev -> ghost
                      QPoint p1 = ghostPts[pi]; QPoint p2 = ghostPts[gi];
                      int dxpx = p2.x() - p1.x(); int dypx = p2.y() - p1.y();
                      if ((dxpx*dxpx + dypx*dypx) >= 18*18) {
                          double d1=0.0, b1=0.0; EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                              target->vertices[pi].x(), target->vertices[pi].y(), lat, lon, &d1, &b1);
                          QString t = QString("%1 NM").arg(QString::number(d1, 'f', 2));
                          QFont df("Arial", 9, QFont::Bold); painter.setFont(df); QFontMetrics dfm(df);
                          double len = std::sqrt(double(dxpx*dxpx + dypx*dypx));
                          double nx = (len>0.0)? (-double(dypx)/len) : 0.0; double ny = (len>0.0)? (double(dxpx)/len) : 0.0;
                          int midX = (p1.x()+p2.x())/2; int midY = (p1.y()+p2.y())/2;
                          int offset = qMax(24, int(dfm.height()*2 + 4));
                          int lx = midX + int(nx*offset); int ly = midY + int(ny*offset);
                          int tw = dfm.horizontalAdvance(t);
                          painter.setPen(QColor(128,128,128)); // ghost label color (gray)
                          painter.drawText(QPoint(lx - tw/2, ly), t);
                      }
                      // ghost -> next
                      p1 = ghostPts[gi]; p2 = ghostPts[ni];
                      dxpx = p2.x() - p1.x(); dypx = p2.y() - p1.y();
                      if ((dxpx*dxpx + dypx*dypx) >= 18*18) {
                          double d2=0.0, b2=0.0; EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                              lat, lon, target->vertices[ni].x(), target->vertices[ni].y(), &d2, &b2);
                          QString t2 = QString("%1 NM").arg(QString::number(d2, 'f', 2));
                          QFont df("Arial", 9, QFont::Bold); painter.setFont(df); QFontMetrics dfm(df);
                          double len = std::sqrt(double(dxpx*dxpx + dypx*dypx));
                          double nx = (len>0.0)? (-double(dypx)/len) : 0.0; double ny = (len>0.0)? (double(dxpx)/len) : 0.0;
                          int midX = (p1.x()+p2.x())/2; int midY = (p1.y()+p2.y())/2;
                          int offset = qMax(24, int(dfm.height()*2 + 4));
                          int lx = midX + int(nx*offset); int ly = midY + int(ny*offset);
                          int tw = dfm.horizontalAdvance(t2);
                          painter.setPen(QColor(128,128,128)); // ghost label color (gray)
                          painter.drawText(QPoint(lx - tw/2, ly), t2);
                      }
                  }

                  if (showAoiLabels) {
                      // Text styling and background (cursor box)
                      QFont f = painter.font(); f.setPointSizeF(9.0); painter.setFont(f);
                      QFontMetrics fm(f); int pad = 4;
                      QRect br = fm.boundingRect(QRect(0,0,600,2000), Qt::AlignLeft|Qt::AlignTop, overlay);
                      QRect bg(mp.x() + 12, mp.y() + 12, br.width() + pad*2, br.height() + pad*2);
                      painter.setPen(Qt::NoPen);
                      QColor bgCol = darkTheme ? QColor(0,0,0,160) : QColor(255,255,255,210);
                      QColor fgCol = darkTheme ? QColor(240,240,240) : QColor(30,30,30);
                      painter.setBrush(bgCol);
                      painter.drawRoundedRect(bg, 4, 4);
                      painter.setPen(fgCol);
                      painter.drawText(bg.adjusted(pad, pad, -pad, -pad), Qt::AlignLeft | Qt::AlignTop, overlay);
                  }
              }
          }
      }
  }

  // ========== DRAW TEST GUARDZONE ==========

  // DRAW AIS TARGET DANGEROUS BOX
  if (displayCategory != EC_DISPLAYBASE && showAIS){
      drawTestGuardSquare(painter);
  }

  if (testGuardZoneEnabled) {

  }
  // =======================================

  if (!getTrackMMSI().isEmpty() && trackShip)
  {
      QFont font("Segoe UI", 10, QFont::Bold);
      painter.setFont(font);

      QString mmsiText = QString("Tracking AIS Target MMSI: %1").arg(getTrackMMSI());
      QRect chartRect = contentsRect();
      int margin = 10;

      // Hitung posisi kanan atas
      QFontMetrics fm(font);
      int textWidth = fm.horizontalAdvance(mmsiText);
      int textHeight = fm.height();

      QPoint topRight(chartRect.right() - margin, chartRect.top() + margin + textHeight);
      QPoint textPos(topRight.x() - textWidth, topRight.y());

      // Buat path dari teks
      QPainterPath path;
      path.addText(textPos, font, mmsiText);

      // Outline putih
      painter.setRenderHint(QPainter::Antialiasing);
      painter.setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter.setBrush(Qt::NoBrush);
      painter.drawPath(path);

      // Isi merah
      painter.setPen(Qt::NoPen);
      painter.setBrush(Qt::red);
      painter.drawPath(path);
  }
  else if (navShip.lat != 0 && navShip.lat != 0) {
      QFont font("Segoe UI", 10, QFont::Bold);
      painter.setFont(font);

      QString mmsiText = "Tracking Ownship";
      QRect chartRect = contentsRect();
      int margin = 10;

      // Hitung posisi kanan atas
      QFontMetrics fm(font);
      int textWidth = fm.horizontalAdvance(mmsiText);
      int textHeight = fm.height();

      QPoint topRight(chartRect.right() - margin, chartRect.top() + margin + textHeight);
      QPoint textPos(topRight.x() - textWidth, topRight.y());

      // Buat path dari teks
      QPainterPath path;
      path.addText(textPos, font, mmsiText);

      // Outline putih
      painter.setRenderHint(QPainter::Antialiasing);
      painter.setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter.setBrush(Qt::NoBrush);
      painter.drawPath(path);

      // Isi merah
      painter.setPen(Qt::NoPen);
      painter.setBrush(Qt::red);
      painter.drawPath(path);
  }
} // End paintEvent

/*
void EcWidget::paintEvent (QPaintEvent *e)
{
  if (! initialized) return;

  QPainter painter(this);
  painter.fillRect(rect(), QColor(204, 197, 123));

  QPoint baseOffset(-PAN_MARGIN, -PAN_MARGIN);
  if (isDragging) {
      painter.drawPixmap(baseOffset + tempOffset, drawPixmap);
  } else {
      painter.drawPixmap(baseOffset, drawPixmap);
  }

} // End paintEvent
*/

// class member (header)
// bool true = true;
// QPoint lastPanPoint;

// void EcWidget::mousePressEvent(QMouseEvent *e)
// {
//     if (e->button() == Qt::LeftButton) {
//         isDragging = true;
//         lastPanPoint = e->pos();
//         tempOffset = QPoint(0,0);

//         setCursor(Qt::OpenHandCursor);
//     }
// }

// void EcWidget::mouseMoveEvent(QMouseEvent* e)
// {
//     if (!isDragging) return;
//     tempOffset = e->pos() - lastPanPoint;
//     setCursor(Qt::ClosedHandCursor);

//     update();
// }

// void EcWidget::mouseReleaseEvent(QMouseEvent* e) {
//     if (e->button() == Qt::LeftButton && isDragging) {
//         isDragging = false;

//         QPoint releasePos = e->pos();

//         // --- ambil geo dari titik press & release ---
//         EcCoordinate pressLat, pressLon;
//         EcCoordinate releaseLat, releaseLon;

//         bool ok1 = XyToLatLon(lastPanPoint.x(), lastPanPoint.y(), pressLat, pressLon);
//         bool ok2 = XyToLatLon(releasePos.x(), releasePos.y(), releaseLat, releaseLon);

//         if (ok1 && ok2) {
//             // Hitung selisih geo
//             double deltaLat = pressLat - releaseLat;
//             double deltaLon = pressLon - releaseLon;

//             // Update center ke posisi baru
//             currentLat += deltaLat;
//             currentLon += deltaLon;
//         }

//         tempOffset = QPoint();  // reset drag offset
//         unsetCursor();

//         Draw();             // redraw chart penuh
//     }
// }

// Fungsi update koordinat sesuai pan
void EcWidget::recalcView(const QPoint& offset) {
    // Konversi offset pixel → delta koordinat lat/lon
    // (contoh sederhana, kamu bisa ganti dengan rumus proyeksi peta)
    double lonPerPixel = 0.0001;
    double latPerPixel = 0.0001;

    currentLon -= offset.x() * lonPerPixel;
    currentLat += offset.y() * latPerPixel;
}

void EcWidget::setDisplayCategoryInternal(int category){
    displayCategory = category;
}

int EcWidget::getDisplayCategory(){
    return displayCategory;
}

// Format latitude/longitude in degrees-minutes with hemisphere, e.g. 07° 12.345' S
QString EcWidget::latLonToDegMin(double value, bool isLatitude)
{
    // Determine hemisphere
    QChar hemi;
    double absVal = std::fabs(value);
    if (isLatitude) {
        hemi = (value < 0) ? 'S' : 'N';
        if (absVal > 90.0) absVal = std::fmod(absVal, 90.0); // clamp extremes defensively
    } else {
        hemi = (value < 0) ? 'W' : 'E';
        if (absVal > 180.0) absVal = std::fmod(absVal, 180.0);
    }

    int deg = static_cast<int>(std::floor(absVal));
    double min = (absVal - deg) * 60.0;

    // Zero-pad degrees for consistent width (2 for lat, 3 for lon)
    int degWidth = isLatitude ? 2 : 3;
    QString degStr = QString("%1").arg(deg, degWidth, 10, QLatin1Char('0'));
    QString minStr = QString::number(min, 'f', 3); // 3 decimals for minutes

    return QString("%1° %2' %3").arg(degStr, minStr, QString(hemi));
}

void EcWidget::addAOI(const AOI& aoi)
{
    AOI copy = aoi;
    if (copy.id <= 0) copy.id = nextAoiId++;
    else nextAoiId = qMax(nextAoiId, copy.id + 1);
    if (!copy.color.isValid()) copy.color = aoiDefaultColor(copy.type);
    aoiList.append(copy);
    emit aoiListChanged();
    saveAOIs();
}

void EcWidget::removeAOI(int id)
{
    for (int i = 0; i < aoiList.size(); ++i) {
        if (aoiList[i].id == id) {
            aoiList.removeAt(i);
            if (attachedAoiId == id) attachedAoiId = -1; // clear attachment if removed
            emit aoiListChanged();
            saveAOIs();
            break;
        }
    }
}

void EcWidget::toggleAOIVisibility(int id)
{
    for (auto& a : aoiList) {
        if (a.id == id) { a.visible = !a.visible; emit aoiListChanged(); break; }
    }
}

void EcWidget::setAOIVisibility(int id, bool visible)
{
    for (auto& a : aoiList) {
        if (a.id == id) {
            if (a.visible != visible) {
                a.visible = visible;
                emit aoiListChanged();
                saveAOIs();
            }
            break;
        }
    }
}

void EcWidget::setAOILabelVisibility(int id, bool showLabel)
{
    for (auto& a : aoiList) {
        if (a.id == id) {
            if (a.showLabel != showLabel) {
                a.showLabel = showLabel;
                emit aoiListChanged();
                saveAOIs();
            }
            break;
        }
    }
}

void EcWidget::drawAOIs(QPainter& painter)
{
    if (aoiList.isEmpty()) return;
    if (!initialized || !view) return;
    static bool aoiDebugLogged = false;
    if (!aoiDebugLogged) {
        qDebug() << "[AOI-DRAW] Start drawAOIs. Count:" << aoiList.size();
    }
    QPen pen; QBrush brush;
    // Make AOI lines smooth and crisp like route overlay
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto& a : aoiList) {
        if (!aoiDebugLogged) {
            qDebug() << "[AOI-DRAW] AOI id=" << a.id << " verts=" << a.vertices.size() << " visible=" << a.visible;
        }
        if (!a.visible || a.vertices.size() < 3) continue;

        QVector<QPoint> pts;
        QVector<QPointF> ptsXYFiltered; // NM coords aligned with pts (fallback removed below)
        QVector<int> projIndexMap;      // map to original vertex indices
        pts.reserve(a.vertices.size());
        ptsXYFiltered.reserve(a.vertices.size());
        projIndexMap.reserve(a.vertices.size());
        // Precompute local-projection scaling for NM distances
        double lat0_for_proj = 0.0, lon0_for_proj = 0.0;
        for (const QPointF& ll : a.vertices) { lat0_for_proj += ll.x(); lon0_for_proj += ll.y(); }
        lat0_for_proj /= a.vertices.size(); lon0_for_proj /= a.vertices.size();
        double lat0rad_for_proj = lat0_for_proj * M_PI / 180.0;
        const double k_nm = 60.0;
        for (int vi = 0; vi < a.vertices.size(); ++vi) {
            const QPointF& ll = a.vertices[vi];
            if (!qIsFinite(ll.x()) || !qIsFinite(ll.y())) continue;
            int x=0, y=0;
            if (LatLonToXy(ll.x(), ll.y(), x, y)) {
                pts.append(QPoint(x, y));
                double xnm = (ll.y() - lon0_for_proj) * k_nm * std::cos(lat0rad_for_proj);
                double ynm = (ll.x() - lat0_for_proj) * k_nm;
                ptsXYFiltered.append(QPointF(xnm, ynm));
                projIndexMap.append(vi);
            }
        }
        if (!aoiDebugLogged) {
            qDebug() << "[AOI-DRAW] projected points=" << pts.size() << " mapIdx=" << projIndexMap.size();
        }
        if (pts.size() < 3) continue;

        QColor c = a.color;
        // Use the selected color for outline
        QColor outline = c;

        // If an AOI is attached, make attached brighter, gray out others
        if (attachedAoiId >= 0) {
            if (a.id == attachedAoiId) {
                outline = c; // Keep selected color for attached
            } else {
                outline = QColor(128,128,128); // Gray out non-attached
            }
        }

        QColor fill = c; fill.setAlpha(50);

        pen.setColor(outline);
        pen.setWidth(2);
        pen.setStyle(Qt::DashLine); // match route style; change to SolidLine if desired
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCosmetic(true);
        brush.setColor(fill); brush.setStyle(Qt::NoBrush);

        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        // Use QPainterPath for smoother joins
        QPainterPath path;
        path.moveTo(pts[0]);
        for (int i = 1; i < pts.size(); ++i) {
            path.lineTo(pts[i]);
        }
        path.closeSubpath();
        painter.drawPath(path);

        // Draw vertex markers: smaller and filled after creation
        {
            QPen vpen(pen.color());
            vpen.setWidth(1);
            vpen.setStyle(Qt::SolidLine);
            painter.setPen(vpen);
            painter.setBrush(vpen.color());
            const int vr = 3; // vertex radius similar to waypoint donut
            for (const QPoint& vp : pts) {
                painter.drawEllipse(vp, vr, vr);
            }
        }

        // Draw label near centroid (match AOI color tone) with waypoint-like font
        double cx=0, cy=0; for (const QPoint& p : pts) { cx += p.x(); cy += p.y(); }
        cx/=pts.size(); cy/=pts.size();
        painter.setPen(outline); // same color as outline
        QFont labelFont("Arial", 9, QFont::Bold);
        painter.setFont(labelFont);
        QFontMetrics fm(labelFont);

        // Compute area in NM^2 using local tangent-plane approximation
        double lat0 = 0.0, lon0 = 0.0;
        for (const QPointF& ll : a.vertices) { lat0 += ll.x(); lon0 += ll.y(); }
        lat0 /= a.vertices.size(); lon0 /= a.vertices.size();
        double lat0rad = lat0 * M_PI / 180.0;
        const double k = 60.0; // deg -> NM
        QVector<QPointF> ptsXY; ptsXY.reserve(a.vertices.size());
        for (const QPointF& ll : a.vertices) {
            double xnm = (ll.y() - lon0) * k * std::cos(lat0rad);
            double ynm = (ll.x() - lat0) * k;
            ptsXY.append(QPointF(xnm, ynm));
        }
        double areaNM2 = 0.0;
        for (int i = 0; i < ptsXY.size(); ++i) {
            const QPointF& p1 = ptsXY[i];
            const QPointF& p2 = ptsXY[(i+1) % ptsXY.size()];
            areaNM2 += p1.x()*p2.y() - p2.x()*p1.y();
        }
        areaNM2 = std::fabs(areaNM2) * 0.5; // NM^2

        // Compose area label without prefix, e.g., "11.68 NM²"
        QString areaLine = QString::fromUtf8("%1 NM²").arg(QString::number(areaNM2, 'f', 2));

        if (showAoiLabels && a.showLabel) {
            // Draw AOI name centered at centroid (baseline positioning)
            int nameW = fm.horizontalAdvance(a.name);
            int nameH = fm.height();
            QPoint namePos(static_cast<int>(cx) - nameW/2, static_cast<int>(cy));
            painter.drawText(namePos, a.name);
            // Draw area centered below the name at centroid (baseline positioning)
            int areaW = fm.horizontalAdvance(areaLine);
            int areaH = fm.height();
            QPoint areaPos(static_cast<int>(cx) - areaW/2, static_cast<int>(cy) + areaH);
            painter.drawText(areaPos, areaLine);
        }

        // Draw AOI segment distance labels for all visible AOIs (obey showAoiLabels)
        if (showAoiLabels && a.showLabel) {
            painter.save();
            QFont distFont("Arial", 9, QFont::Bold);
            painter.setFont(distFont);
            QFontMetrics dfm(distFont);
            painter.setPen(outline);
            // Build projected points and index map (reuse existing pts/projIndexMap created above)
            if (projIndexMap.size() == pts.size() && pts.size() >= 2) {
                for (int i = 0; i < pts.size(); ++i) {
                    const QPoint& s1 = pts[i];
                    const QPoint& s2 = pts[(i+1) % pts.size()];
                    // Skip too-short segments (avoid clutter)
                    int dxpx = s2.x() - s1.x();
                    int dypx = s2.y() - s1.y();
                    if ((dxpx*dxpx + dypx*dypx) < 18*18) continue; // ~18px threshold

                    int idx1 = projIndexMap[i];
                    int idx2 = projIndexMap[(i+1) % projIndexMap.size()];
                    if (idx1 < 0 || idx2 < 0 || idx1 >= a.vertices.size() || idx2 >= a.vertices.size()) continue;
                    const QPointF &v1 = a.vertices[idx1];
                    const QPointF &v2 = a.vertices[idx2];
                    if (!qIsFinite(v1.x()) || !qIsFinite(v1.y()) || !qIsFinite(v2.x()) || !qIsFinite(v2.y())) continue;

                    EcCoordinate lat1 = v1.x(); EcCoordinate lon1 = v1.y();
                    EcCoordinate lat2 = v2.x(); EcCoordinate lon2 = v2.y();
                    double distNM = 0.0, bearingDummy = 0.0;
                    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, lat1, lon1, lat2, lon2, &distNM, &bearingDummy);

                    QString text = QString("%1 NM").arg(QString::number(distNM, 'f', 2));
                    int midX = (s1.x() + s2.x()) / 2;
                    int midY = (s1.y() + s2.y()) / 2;
                    // Offset label away from the line along its screen-space normal
                    double len = std::sqrt(double(dxpx*dxpx + dypx*dypx));
                    double nx = (len > 0.0) ? (-double(dypx) / len) : 0.0;
                    double ny = (len > 0.0) ? ( double(dxpx) / len) : 0.0;
                    // Increase offset further to avoid overlap with the line
                    int offset = qMax(24, int(dfm.height() * 2 + 4));
                    int lx = midX + int(nx * offset);
                    int ly = midY + int(ny * offset);
                    int tw = dfm.horizontalAdvance(text);
                    painter.drawText(QPoint(lx - tw/2, ly), text);
                }
            }
            painter.restore();
        }
    }
    if (!aoiDebugLogged) {
        qDebug() << "[AOI-DRAW] Completed first draw cycle";
        aoiDebugLogged = true;
    }
}

void EcWidget::attachAOIToShip(int aoiId)
{
    // -1 detaches all
    if (aoiId < 0) {
        attachedAoiId = -1;
        update();
        emit aoiListChanged();
        return;
    }
    // Verify AOI exists; if not, detach
    bool exists = false;
    for (const auto& a : aoiList) {
        if (a.id == aoiId) { exists = true; break; }
    }
    attachedAoiId = exists ? aoiId : -1;
    update();
    emit aoiListChanged();
}

bool EcWidget::isAOIAttachedToShip(int aoiId) const
{
    return (aoiId >= 0) && (aoiId == attachedAoiId);
}

void EcWidget::startAOICreation(const QString& name, AOIType type)
{
    creatingAOI = true;
    // Set default name similar to Route default if not specified
    if (name.trimmed().isEmpty() || name.trimmed().toUpper() == "AOI") {
        pendingAOIName = QString("AOI %1").arg(nextAoiId);
    } else {
        pendingAOIName = name;
    }
    pendingAOIType = type;
    aoiVerticesLatLon.clear();
    update();
}

void EcWidget::startAOICreationWithColor(const QString& name, AOIColorChoice colorChoice)
{
    creatingAOI = true;
    // Set default name
    if (name.trimmed().isEmpty() || name.trimmed().toUpper() == "AOI") {
        pendingAOIName = QString("Area %1").arg(nextAoiId);
    } else {
        pendingAOIName = name;
    }
    pendingAOIType = AOIType::AOI;  // Default type
    pendingAOIColor = aoiColorFromChoice(colorChoice);  // Store color directly
    aoiVerticesLatLon.clear();
    update();
}

void EcWidget::cancelAOICreation()
{
    creatingAOI = false;
    aoiVerticesLatLon.clear();
    update();
}

void EcWidget::finishAOICreation()
{
    if (!creatingAOI || aoiVerticesLatLon.size() < 3) { cancelAOICreation(); return; }
    AOI a;
    a.id = nextAoiId++;
    a.name = pendingAOIName;
    a.type = pendingAOIType;
    // Use pendingAOIColor if it's valid, otherwise use default
    a.color = pendingAOIColor.isValid() ? pendingAOIColor : aoiDefaultColor(a.type);
    a.visible = true;
    a.vertices = aoiVerticesLatLon;
    aoiList.append(a);
    emit aoiListChanged();
    creatingAOI = false;
    aoiVerticesLatLon.clear();
    pendingAOIColor = QColor();  // Reset color
    // Persist AOIs after creation
    saveAOIs();
    update();
}



void EcWidget::startEditAOI(int aoiId)
{
    bool exists = false;
    for (const auto& a : aoiList) { if (a.id == aoiId) { exists = true; break; } }
    if (!exists) return;
    editingAOI = true;
    editingAoiId = aoiId;
    draggedAoiVertex = -1;
    update();
}

void EcWidget::finishEditAOI()
{
    editingAOI = false;
    editingAoiId = -1;
    draggedAoiVertex = -1;
    // Persist AOIs after edit
    saveAOIs();
    update();
}

void EcWidget::cancelEditAOI()
{
    finishEditAOI();
}

bool EcWidget::exportAOIsToFile(const QString& filename)
{
    if (aoiList.isEmpty()) {
        QMessageBox::warning(this, tr("Export AOIs"), tr("No AOIs to export."));
        return false;
    }

    QJsonArray aoiArray;

    for (const AOI& a : aoiList) {
        QJsonObject obj;
        obj["id"] = a.id;
        obj["name"] = a.name;
        obj["type"] = aoiTypeToString(a.type);
        obj["visible"] = a.visible;
        obj["showLabel"] = a.showLabel;
        // Save color as hex string #RRGGBB
        obj["color"] = a.color.name(QColor::HexRgb);

        QJsonArray verts;
        for (const QPointF& p : a.vertices) {
            QJsonObject v;
            v["lat"] = p.x();
            v["lon"] = p.y();
            verts.append(v);
        }
        obj["vertices"] = verts;

        aoiArray.append(obj);
    }

    QJsonObject root;
    root["aois"] = aoiArray;
    root["exported_on"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["count"] = aoiList.size();

    QJsonDocument doc(root);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Export Failed"),
                              tr("Failed to write %1: %2").arg(filename, file.errorString()));
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    QMessageBox::information(this, tr("Export Successful"),
                             tr("Exported %1 AOI(s) to %2")
                                 .arg(aoiList.size())
                                 .arg(QFileInfo(filename).fileName()));
    return true;
}

void EcWidget::saveAOIs()
{
    QJsonArray aoiArray;
    for (const AOI& a : aoiList) {
        QJsonObject obj;
        obj["id"] = a.id;
        obj["name"] = a.name;
        obj["type"] = aoiTypeToString(a.type);
        obj["visible"] = a.visible;
        obj["showLabel"] = a.showLabel;
        obj["color"] = a.color.name(QColor::HexRgb);
        QJsonArray verts;
        for (const QPointF& p : a.vertices) {
            QJsonObject v; v["lat"] = p.x(); v["lon"] = p.y(); verts.append(v);
        }
        obj["vertices"] = verts;
        aoiArray.append(obj);
    }

    QJsonObject root; root["aois"] = aoiArray; root["attachedAoiId"] = attachedAoiId;
    QJsonDocument doc(root);

    QString filePath = getAOIFilePath();
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "[ERROR] Could not create directory for AOIs:" << dir.path();
            filePath = "aois.json"; // fallback
        }
    }

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        qDebug() << "[INFO] AOIs saved to" << filePath;
    } else {
        qDebug() << "[ERROR] Failed to save AOIs to" << filePath << ":" << file.errorString();
        QFile fallback("aois.json");
        if (fallback.open(QIODevice::WriteOnly | QIODevice::Text)) {
            fallback.write(doc.toJson(QJsonDocument::Indented));
            fallback.close();
            qDebug() << "[INFO] AOIs saved to fallback location: aois.json";
        }
    }
}

void EcWidget::loadAOIs()
{
    QString filePath = getAOIFilePath();
    QFile file(filePath);
    if (!file.exists()) {
        qDebug() << "[INFO] AOI file not found. Starting with empty AOI list.";
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[ERROR] Failed to open AOI file:" << filePath;
        return;
    }
    QByteArray data = file.readAll(); file.close();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;
    QJsonObject rootObj = doc.object();
    QJsonArray arr = rootObj.value("aois").toArray();
    aoiList.clear();
    int maxId = 0;
    for (const auto& v : arr) {
        QJsonObject o = v.toObject();
        AOI a;
        a.id = o.value("id").toInt();
        a.name = o.value("name").toString();
        a.type = aoiTypeFromString(o.value("type").toString());
        a.visible = o.value("visible").toBool(true);
        a.showLabel = o.contains("showLabel") ? o.value("showLabel").toBool(true) : true;
        QString colorStr = o.value("color").toString();
        QColor c(colorStr); a.color = c.isValid() ? c : aoiDefaultColor(a.type);
        QJsonArray verts = o.value("vertices").toArray();
        for (const auto& vv : verts) {
            QJsonObject vo = vv.toObject();
            a.vertices.append(QPointF(vo.value("lat").toDouble(), vo.value("lon").toDouble()));
        }
        aoiList.append(a);
        if (a.id > maxId) maxId = a.id;
    }
    nextAoiId = qMax(maxId + 1, nextAoiId);
    // restore attached AOI id if present and valid
    // attachedAoiId is not restored from file to prevent automatic re-attachment on startup.
    // User must explicitly attach an AOI if desired.
    attachedAoiId = -1;
    emit aoiListChanged();
    qDebug() << "[INFO] Loaded" << aoiList.size() << "AOIs from" << filePath;
}

QString EcWidget::getAOIFilePath() const
{
    QString basePath;
#ifdef _WIN32
    if (EcKernelGetEnv("APPDATA"))
        basePath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC";
#else
    if (EcKernelGetEnv("HOME"))
        basePath = QString(EcKernelGetEnv("HOME")) + "/SevenCs/EC2007/DENC";
#endif
    if (basePath.isEmpty())
        return "aois.json";
    else
        return basePath + "/aois.json";
}
bool EcWidget::getAoiVertexAtPosition(int x, int y, int& outAoiId, int& outVertexIndex)
{
    outAoiId = -1; outVertexIndex = -1;
    double best = 1e9;
    for (const auto& a : aoiList) {
        if (a.vertices.size() < 3 || !a.visible) continue;
        for (int i = 0; i < a.vertices.size(); ++i) {
            int sx=0, sy=0; if (LatLonToXy(a.vertices[i].x(), a.vertices[i].y(), sx, sy)) {
                double dx = sx - x; double dy = sy - y; double d = std::sqrt(dx*dx + dy*dy);
                if (d <= handleDetectRadiusPx && d < best) { best = d; outAoiId = a.id; outVertexIndex = i; }
            }
        }
    }
    return (outAoiId >= 0 && outVertexIndex >= 0);
}

void EcWidget::showAoiVertexContextMenu(const QPoint& pos, int aoiId, int vertexIndex)
{
    QMenu contextMenu(this);
    QAction* moveAct = contextMenu.addAction(tr("Move"));
    QAction* delAct  = contextMenu.addAction(tr("Delete"));
    QAction* chosen = contextMenu.exec(mapToGlobal(pos));
    if (!chosen) return;

    // Find AOI reference
    for (int ai = 0; ai < aoiList.size(); ++ai) if (aoiList[ai].id == aoiId) {
        auto& a = aoiList[ai];
        if (chosen == moveAct) {
            // Ensure edit mode and start click-move
            editingAOI = true;
            editingAoiId = aoiId;
            draggedAoiVertex = vertexIndex;
            emit statusMessage(tr("Moving AOI vertex: move cursor, click to drop"));
            update();
            return;
        } else if (chosen == delAct) {
            if (a.vertices.size() > 3) {
                a.vertices.remove(vertexIndex);
                emit aoiListChanged();
                update();
            } else {
                emit statusMessage(tr("Cannot remove vertex: polygon must have at least 3 vertices"));
            }
            return;
        }
        break;
    }
}

void EcWidget::showAoiToolbox(const QPoint& pos, int aoiId, int vertexIndex){
    hideToolbox();

    // Check if AOI is attached to ship
    bool isAttached = isAOIAttachedToShip(aoiId);

    // Show/hide info label
    if (isAttached && false) {
        //toolboxAoiInfoLabel->setText("Area attached - editing disabled");
        toolboxAoiInfoLabel->show();
    } else {
        toolboxAoiInfoLabel->hide();
    }

    // Disable all AOI toolbox buttons if area is attached
    btnMove->setEnabled(!isAttached);   // Move Point
    btnDelete->setEnabled(!isAttached); // Delete Point

    // Posisi toolbox relatif cursor
    toolboxAoi->adjustSize();
    QPoint posT = QCursor::pos();
    posT.setY(posT.y() - toolboxAoi->height() - 5);
    posT.setX(posT.x() - toolboxAoi->width() / 2);
    toolboxAoi->move(posT);

    // Find AOI reference
    for (int ai = 0; ai < aoiList.size(); ++ai) if (aoiList[ai].id == aoiId) {
        auto& a = aoiList[ai];

        lastAoiId = aoiId;
        lastAoiList = &a;

        break;
    }

    lastVertexIndex = vertexIndex;

    toolboxAoi->show();
}

void EcWidget::drawOwnShipTrail(QPainter &painter)
{
    if (ownShipTrailPoints.size() < 2)
        return;

    for (int i = 1; i < ownShipTrailPoints.size(); ++i)
    {
        const QPair<QString, QString> &prev = ownShipTrailPoints[i - 1];
        const QPair<QString, QString> &curr = ownShipTrailPoints[i];

        bool ok1a = false, ok1b = false, ok2a = false, ok2b = false;
        double lat1 = prev.first.toDouble(&ok1a);
        double lon1 = prev.second.toDouble(&ok1b);
        double lat2 = curr.first.toDouble(&ok2a);
        double lon2 = curr.second.toDouble(&ok2b);

        if (!(ok1a && ok1b && ok2a && ok2b))
        {
            qDebug() << "Invalid conversion to double at index" << i;
            continue;
        }

        if (!qIsFinite(lat1) || !qIsFinite(lon1) || !qIsFinite(lat2) || !qIsFinite(lon2))
        {
            qDebug() << "Non-finite coordinate at index" << i;
            continue;
        }

        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        bool ok1 = LatLonToXy(lat1, lon1, x1, y1);
        bool ok2 = LatLonToXy(lat2, lon2, x2, y2);

        if (!ok1 || !ok2)
        {
            qDebug() << "LatLonToXy failed at index" << i;
            continue;
        }

        double dist = haversine(lat1, lon1, lat2, lon2);

        // if (dist > 100.0)
        //     painter.setPen(QPen(Qt::gray, 4));
        // else
        painter.setPen(QPen(QColor(0, 150, 0), 4));

        painter.drawLine(x1, y1, x2, y2);
    }
}



// Fungsi utilitas Haversine (dalam kilometer)
double EcWidget::haversine(double lat1, double lon1, double lat2, double lon2)
{
    static const double R = 6371000.0; // Radius bumi dalam meter
    double dLat = qDegreesToRadians(lat2 - lat1);
    double dLon = qDegreesToRadians(lon2 - lon1);
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(qDegreesToRadians(lat1)) * std::cos(qDegreesToRadians(lat2)) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return R * c;
}


void EcWidget::clearOwnShipTrail()
{
    ownShipTrailPoints.clear();
    update();
}

/*---------------------------------------------------------------------------*/

void EcWidget::resizeEvent (QResizeEvent *event)
{
    int wi;
    int hei;

    if (isDragging) {
        // Tambahkan margin agar ada buffer di luar layar
        wi = event->size().width() + 2*PAN_MARGIN;
        hei = event->size().height() + 2*PAN_MARGIN;
    }
    else {
        wi  = event->size().width();
        hei = event->size().height();
    }


  if (! initialized)
  {
#ifdef _WIN32
    HDC dc = GetDC(NULL);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, wi, hei);
    hdc = CreateCompatibleDC(dc);
    ReleaseDC(NULL, dc);
    SelectObject(hdc, bitmap);
#endif
  }
  else
  {
      if (!isDragging){
          EcDrawEnd (view);
      }
  }

  if (!isDragging){
      chartPixmap = QPixmap(wi, hei);
      drawPixmap = QPixmap(wi, hei);
  }

  initialized = true;

#ifdef _WIN32
  EcDrawNTInitialize (view, NULL, wi, hei, 470, 290, False);

  hBitmap = CreateCompatibleBitmap(hdc, wi, hei);
  HGDIOBJ oBitmap = SelectObject(hdc, hBitmap);
  DeleteObject(oBitmap);

  //Very few old graphic drivers with shared memory can cause strange colours
  //for the first drawing which is automatically corrected after the second drawing.
  //In case this happens enable the following three lines
  int xx[2], yy[2];
  xx[0] =  0;yy[0] =  0;  xx[1] =  1;yy[1] =  1;
  EcDrawNTDrawLine(view, hdc, NULL, "rcrtcl04", "", 0, xx, yy, 2);
#else
  EcDrawX11Initialize (view, dpy, wi, hei, 470, 290, True);
  EcDrawX11GetResources (view, &drawGC, &x11pixmap, NULL);
#endif

  Draw();

  bool hasRoutes = !waypointList.isEmpty();
  if((showAIS || hasRoutes) && !isDragging)
    drawAISCell();
}

/*---------------------------------------------------------------------------*/

/*
// Mouseevent lama
void EcWidget::mousePressEvent(QMouseEvent *e)
{
    // Stop EBL/VRM measure on right-click
    if (eblvrm.measureMode && e->button() == Qt::RightButton) {
        eblvrm.setMeasureMode(false);
        emit statusMessage(tr("Measure stopped"));
        update();
        // do not consume; allow other handlers if needed
    }
  setFocus();

  if (e->button() == Qt::LeftButton)
  {
    // if (activeFunction == CREATE_WAYP) {
    //       qDebug() << "CREATE WAYP";
    //     EcCoordinate lat, lon;
    //     if (XyToLatLon(e->x(), e->y(), lat, lon)) {
    //         SetWaypointPos(lat, lon);
    //         createWaypoint();
    //     }
    // }
    // else {
    //     EcCoordinate lat, lon;
    //     if (XyToLatLon(e->x(), e->y(), lat, lon))
    //     {
    //         SetCenter(lat, lon);
    //         //draw(true);
    //         Draw();
    //     }
    // }

      // Get mouse coordinates
      QPoint pos = e->pos();

      // Get position of the ecchart widget within the main window
      QPoint chartPos = ecchart->mapFromParent(pos);

      // Check if click is within the chart area
      if (!ecchart->rect().contains(chartPos)) {
          // Click is outside chart area
          QMainWindow::mousePressEvent(event);
          return;
      }

      // Convert screen coordinates to lat/lon
      EcCoordinate latPos, lonPos;
      EcView* view = ecchart->GetView();

      if (EcDrawXyToLatLon(view, chartPos.x(), chartPos.y(), &latPos, &lonPos)) {
          // Handle based on active function
          switch (activeFunction) {
          case CREATE_WAYP: {
              // Define PICKRADIUS based on current range
              double range = ecchart->GetRange();
              double PICKRADIUS = 0.03 * range;

              // Create waypoint at clicked position
              EcCellId udoCid = ecchart->GetUdoCellId();
              wp1 = EcRouteAddWaypoint(udoCid, dict, latPos, lonPos,
                                       PICKRADIUS, TURNRADIUS);

              if (!ECOK(wp1)) {
                  QMessageBox::critical(this, "Error", "Waypoint could not be created");
              } else {
                  // Update display
                  drawUdo();
                  ecchart->update();
              }

              // Reset to PAN mode
              activeFunction = PAN;
              showHeader();
              statusBar()->clearMessage();
              break;
          }

          // Add other cases as needed...

          default:
              // Pass to base class for default handling
              QMainWindow::mousePressEvent(event);
              break;
          }
      }
  }
  else if (e->button() == Qt::RightButton)
  {
      if (activeFunction == CREATE_WAYP) {
          activeFunction = NONE;
      }
    pickX = e->x();
    pickY = e->y();
    emit mouseRightClick();
  }
}
*/

void EcWidget::mousePressEvent(QMouseEvent *e)
{
    setFocus();

    // EBL/VRM Measure interactions
    if (eblvrm.measureMode) {
        if (e->button() == Qt::RightButton) {
            // Place point with right-click and finalize measurement
            EcCoordinate lat, lon;
            if (XyToLatLon(e->x(), e->y(), lat, lon)) {
                eblvrm.clearFixedPoint();
                eblvrm.startMeasureSession();
                eblvrm.addMeasurePoint(lat, lon);
                eblvrm.commitFirstPointAsFixedTarget();
                eblvrm.setMeasureMode(false);
                eblvrm.clearMeasureSession();
                emit statusMessage(tr("Measure set to first point"));
                update();
                return; // consume to avoid other right-click handlers
            }
        }
        // Do not consume left-click in measure mode; allow pan/other defaults
    }

    // EBL/VRM delete menu on right-click near line or ring (works also during measure)
    else if (e->button() == Qt::RightButton) {
        bool hitEbl = false, hitVrm = false;
        const int hitTolPx = 8;

        // --- EBL hit test ---
        if (eblvrm.eblEnabled || eblvrm.eblHasFixedPoint) {
            int cx=0, cy=0, ex=0, ey=0; EcCoordinate lat2=0, lon2=0;
            if (LatLonToXy(navShip.lat, navShip.lon, cx, cy)) {
                bool haveEndpoint=false;
                if (eblvrm.eblHasFixedPoint) {
                    lat2 = eblvrm.eblFixedLat; lon2 = eblvrm.eblFixedLon; haveEndpoint = LatLonToXy(lat2, lon2, ex, ey);
                } else {
                    EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84, navShip.lat, navShip.lon, 12.0, eblvrm.eblBearingDeg, &lat2, &lon2);
                    haveEndpoint = LatLonToXy(lat2, lon2, ex, ey);
                }
                if (haveEndpoint) {
                    auto sqr = [](double v){ return v*v; };
                    auto distPtSeg = [&](double px, double py, double x1, double y1, double x2, double y2){
                        double vx=x2-x1, vy=y2-y1; double wx=px-x1, wy=py-y1; double c1=vx*wx+vy*wy; if (c1<=0) return std::sqrt(sqr(px-x1)+sqr(py-y1));
                        double c2=vx*vx+vy*vy; if (c2<=0) return std::sqrt(sqr(px-x1)+sqr(py-y1)); double t=c1/c2; if (t>=1) return std::sqrt(sqr(px-x2)+sqr(py-y2));
                        double projx=x1+t*vx, projy=y1+t*vy; return std::sqrt(sqr(px-projx)+sqr(py-projy)); };
                    double d = distPtSeg(e->x(), e->y(), cx, cy, ex, ey);
                    if (d <= hitTolPx) hitEbl = true;
                }
            }
        }

        // helper to compute min distance to a polyline
        auto segDist = [](const QPoint& p, const QPoint& a, const QPoint& b){
            auto sqr = [](double v){ return v*v; };
            double px=p.x(), py=p.y(), x1=a.x(), y1=a.y(), x2=b.x(), y2=b.y();
            double vx=x2-x1, vy=y2-y1; double wx=px-x1, wy=py-y1; double c1=vx*wx+vy*wy; if (c1<=0) return std::sqrt(sqr(px-x1)+sqr(py-y1));
            double c2=vx*vx+vy*vy; if (c2<=0) return std::sqrt(sqr(px-x1)+sqr(py-y1)); double t=c1/c2; if (t>=1) return std::sqrt(sqr(px-x2)+sqr(py-y2));
            double projx=x1+t*vx, projy=y1+t*vy; return std::sqrt(sqr(px-projx)+sqr(py-projy)); };

        // --- VRM hit test (normal ring around ownship) ---
        if (eblvrm.vrmEnabled) {
            int cx=0, cy=0; if (LatLonToXy(navShip.lat, navShip.lon, cx, cy)) {
                double vr = eblvrm.vrmRadiusNM;
                if (eblvrm.eblHasFixedPoint) {
                    double dnm=0.0, btmp=0.0;
                    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, navShip.lat, navShip.lon, eblvrm.eblFixedLat, eblvrm.eblFixedLon, &dnm, &btmp);
                    vr = dnm;
                }
                const int segs = 72;
                QPoint prev; bool hasPrev=false;
                for (int i=0;i<segs;++i){
                    double brg = (360.0 * i) / segs;
                    EcCoordinate lat2=0, lon2=0;
                    EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84, navShip.lat, navShip.lon, vr, brg, &lat2, &lon2);
                    int px=0, py=0; if (!LatLonToXy(lat2, lon2, px, py)) continue;
                    QPoint cur(px,py);
                    if (hasPrev){
                        double d = segDist(e->pos(), prev, cur);
                        if (d <= hitTolPx) { hitVrm = true; break; }
                    }
                    prev = cur; hasPrev=true;
                }
            }
        }

        // --- Temporary measuring ring (green) around last point ---
        if (!hitVrm && eblvrm.measureMode && eblvrm.measuringActive && eblvrm.liveHasCursor && eblvrm.measurePoints.size() >= 1) {
            double cLat = eblvrm.measurePoints.back().x();
            double cLon = eblvrm.measurePoints.back().y();
            double dnm=0.0, btmp=0.0;
            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, cLat, cLon, eblvrm.liveCursorLat, eblvrm.liveCursorLon, &dnm, &btmp);
            const int segs = 72; QPoint prev; bool hasPrev=false; int cx=0, cy=0; bool centerOk = LatLonToXy(cLat, cLon, cx, cy);
            if (centerOk) {
                for (int i=0;i<segs;++i){
                    double brg = (360.0 * i) / segs;
                    EcCoordinate lat2=0, lon2=0;
                    EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84, cLat, cLon, dnm, brg, &lat2, &lon2);
                    int px=0, py=0; if (!LatLonToXy(lat2, lon2, px, py)) continue;
                    QPoint cur(px,py);
                    if (hasPrev){
                        double d = segDist(e->pos(), prev, cur);
                        if (d <= hitTolPx) { hitVrm = true; break; }
                    }
                    prev = cur; hasPrev=true;
                }
            }
        }

        if (hitEbl || hitVrm) {
            QMenu menu(this);
            QAction* del = menu.addAction(tr("Delete EBL && VRM"));
            QAction* chosen = menu.exec(mapToGlobal(e->pos()));
            if (chosen == del) {
                eblvrm.clearFixedPoint();
                eblvrm.setEblEnabled(false);
                eblvrm.setVrmEnabled(false);
                eblvrm.setMeasureMode(false);
                eblvrm.clearMeasureSession();
                emit statusMessage(tr("EBL/VRM deleted"));
                update();
                return; // handled
            }
        }
    }

    // Keep waypoint highlight when clicking on the map; it will be changed
    // only when another waypoint is explicitly selected.
    // AOI context menu on right-click (global, even outside edit mode)
    if (e->button() == Qt::RightButton && !editingAOI) {
        int aoiId=-1, vIdx=-1;
        if (getAoiVertexAtPosition(e->x(), e->y(), aoiId, vIdx)) {
            showAoiToolbox(e->pos(), aoiId, vIdx);
            // showAoiVertexContextMenu(e->pos(), aoiId, vIdx);
            return; // handled
        }

        // If not on a vertex, allow adding a point on nearest edge of any visible AOI
        int bestAoiId = -1;
        int bestSeg = -1;
        double bestDist = 1e9;
        const QPoint click = e->pos();
        const int edgeProximityPx = static_cast<int>(std::round(handleDetectRadiusPx + 4));

        for (const auto& a : aoiList) {
            if (!a.visible || a.vertices.size() < 3) continue;
            // Build screen-space points
            QVector<QPoint> pts; pts.reserve(a.vertices.size());
            for (const auto& ll : a.vertices) { int x=0, y=0; if (LatLonToXy(ll.x(), ll.y(), x, y)) pts.append(QPoint(x,y)); }
            if (pts.size() < 3) continue;

            for (int i = 0; i < pts.size(); ++i) {
                QPoint p1 = pts[i]; QPoint p2 = pts[(i+1) % pts.size()];
                QPointF v = p2 - p1; QPointF w = click - p1;
                double c1 = QPointF::dotProduct(w, v);
                double c2 = QPointF::dotProduct(v, v);
                double t = c2 > 0 ? c1 / c2 : 0;
                t = std::max(0.0, std::min(1.0, t));
                QPointF proj = p1 + t * v;
                double d = std::hypot(click.x() - proj.x(), click.y() - proj.y());
                if (d < bestDist) { bestDist = d; bestAoiId = a.id; bestSeg = i; }
            }
        }

        if (bestAoiId >= 0 && bestSeg >= 0 && bestDist <= edgeProximityPx) {
            // Confirm via context menu
            // QMenu contextMenu(this);
            // QAction* addPointAct = contextMenu.addAction(tr("Add Point"));
            // QAction* chosen = contextMenu.exec(mapToGlobal(e->pos()));

            hideToolbox();

            // Check if AOI is attached to ship
            bool isAttached = isAOIAttachedToShip(bestAoiId);

            // Show/hide info label
            if (isAttached && false) {
                toolboxAoiCreateInfoLabel->setText("Area attached - editing disabled");
                toolboxAoiCreateInfoLabel->show();
            } else {
                toolboxAoiCreateInfoLabel->hide();
            }

            // Disable add point button if AOI is attached
            btnCreate->setEnabled(!isAttached);

            // Posisi toolbox relatif cursor
            toolboxAoiCreate->adjustSize();
            QPoint posT = QCursor::pos();
            posT.setY(posT.y() - toolboxAoiCreate->height() - 5);
            posT.setX(posT.x() - toolboxAoiCreate->width() / 2);
            toolboxAoiCreate->move(posT);

            EcCoordinate lat, lon;
            if (XyToLatLon(e->x(), e->y(), lat, lon)) {
                for (int ai = 0; ai < aoiList.size(); ++ai) {
                    if (aoiList[ai].id == bestAoiId) {
                        lastAoiList = &aoiList[ai];
                        lastLat = lat;
                        lastLon = lon;
                        lastBestSeg = bestSeg;
                    }
                }
            }

            toolboxAoiCreate->show();

            // if (chosen == addPointAct) {
            //     // Insert at clicked position (convert click to lat/lon)
            //     EcCoordinate lat, lon;
            //     if (XyToLatLon(e->x(), e->y(), lat, lon)) {
            //         for (int ai = 0; ai < aoiList.size(); ++ai) {
            //             if (aoiList[ai].id == bestAoiId) {
            //                 aoiList[ai].vertices.insert(bestSeg + 1, QPointF(lat, lon));
            //                 emit aoiListChanged();
            //                 saveAOIs();
            //                 update();
            //                 break;
            //             }
            //         }
            //     }
            // }
            return; // handled
        }
    }

    // ========== AOI CREATION MODE ==========
    if (creatingAOI) {
        if (e->button() == Qt::LeftButton) {
            EcCoordinate lat, lon;
            if (XyToLatLon(e->x(), e->y(), lat, lon)) {
                aoiVerticesLatLon.append(QPointF(lat, lon));
                update();
            }
        } else if (e->button() == Qt::RightButton) {
            // Right-click to finish (require >=3 points)
            if (aoiVerticesLatLon.size() >= 3) {
                finishAOICreation();
            } else {
                cancelAOICreation();
            }
        }
        return;
    }


    // ========== AOI EDIT MODE ==========
    if (editingAOI) {
        if (e->button() == Qt::LeftButton) {            // Start drag if clicking a handle; if already dragging, wait for release to commit
            if (draggedAoiVertex >= 0 && aoiVertexDragging) {
                // Do nothing on press; commit on release
                return;
            }
            // Check if clicked on a handle
            int idx = -1; double best=1e9;
            // get AOI
            for (auto& a : aoiList) {
                if (a.id == editingAoiId) {
                    for (int i=0;i<a.vertices.size();++i) {
                        int x=0,y=0; if (LatLonToXy(a.vertices[i].x(), a.vertices[i].y(), x, y)) {
                            double dx = x - e->x(); double dy = y - e->y(); double d = std::sqrt(dx*dx+dy*dy);
                            if (d < best && d <= handleDetectRadiusPx) { best = d; idx = i; }
                        }
                    }
                    break;
                }
            }
            if (idx >= 0) {
                draggedAoiVertex = idx; aoiVertexDragging = true;
                // Initialize ghost to current vertex position
                for (const auto& a : aoiList) if (a.id == editingAoiId) {
                    if (idx >= 0 && idx < a.vertices.size()) {
                        aoiGhostLat = a.vertices[idx].x();
                        aoiGhostLon = a.vertices[idx].y();
                    }
                    break;
                }
            }
        }
        else if (e->button() == Qt::RightButton) {

            finishEditAOI();
            return;

            // Right click: on vertex shows Move/Delete; else add on nearest edge
            /*
            for (int ai = 0;ai < aoiList.size(); ++ai) if (aoiList[ai].id == editingAoiId) {
                auto& a = aoiList[ai];
                // check remove
                int hit=-1; for (int i=0;i<a.vertices.size();++i) {
                    int x=0,y=0; if (LatLonToXy(a.vertices[i].x(), a.vertices[i].y(), x, y)) {
                        double dx = x - e->x(); double dy = y - e->y(); if (std::sqrt(dx*dx+dy*dy) <= handleDetectRadiusPx) { hit = i; break; }
                    }
                }
                if (hit>=0) {
                    QMenu contextMenu(this);
                    QAction* moveAct = contextMenu.addAction(tr("Move"));
                    QAction* delAct  = contextMenu.addAction(tr("Delete"));
                    QAction* chosen = contextMenu.exec(mapToGlobal(e->pos()));

                    if (chosen == moveAct) {
                        draggedAoiVertex = hit; // start click-move mode
                        emit statusMessage(tr("Moving AOI vertex: move cursor, click to drop"));
                        return;
                    } else if (chosen == delAct) {
                        if (a.vertices.size() > 3) {
                            a.vertices.remove(hit);
                            emit aoiListChanged();
                            saveAOIs();
                            update();
                        } else {
                            emit statusMessage(tr("Cannot remove vertex: polygon must have at least 3 vertices"));
                        }
                        return;
                    } else {
                        return; // cancelled
                    }
                }
                // add on nearest edge
                // convert click to lat/lon
                EcCoordinate lat, lon; if (!XyToLatLon(e->x(), e->y(), lat, lon)) { return; }
                // find nearest segment in screen space
                int bestSeg=-1; double bestDist=1e9; QPoint click(e->x(), e->y());
                QVector<QPoint> pts; pts.reserve(a.vertices.size());
                for (const auto& ll : a.vertices) { int x=0,y=0; if (LatLonToXy(ll.x(), ll.y(), x, y)) pts.append(QPoint(x,y)); }
                for (int i=0;i<pts.size();++i) {
                    QPoint p1 = pts[i]; QPoint p2 = pts[(i+1)%pts.size()];
                    QPointF v = p2 - p1; QPointF w = click - p1;
                    double c1 = QPointF::dotProduct(w,v);
                    double c2 = QPointF::dotProduct(v,v);
                    double t = c2>0? c1/c2 : 0;
                    t = std::max(0.0, std::min(1.0, t));
                    QPointF proj = p1 + t*v;
                    double d = std::hypot(click.x()-proj.x(), click.y()-proj.y());
                    if (d < bestDist) { bestDist = d; bestSeg = i; }
                }
                if (bestSeg>=0) {
                    // Show context menu to confirm adding a point on the edge
                    QMenu contextMenu(this);
                    QAction* addPointAct = contextMenu.addAction(tr("Add Point"));
                    QAction* chosen = contextMenu.exec(mapToGlobal(e->pos()));
                    if (chosen == addPointAct) {
                        // insert new vertex at clicked lat/lon after bestSeg
                        a.vertices.insert(bestSeg+1, QPointF(lat, lon));
                        emit aoiListChanged();
                        saveAOIs();
                        update();
                    }
                }
                return;
            }
            */
        }
        return; // consume
    }

    // ========== HANDLING EDIT GUARDZONE MODE VIA MANAGER ==========
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        if (guardZoneManager->handleMousePress(e)) {
            return; // Event handled by manager
        }
    }

    // ========== GUARDZONE CREATION MODE ==========
    if (creatingGuardZone) {
        if (e->button() == Qt::LeftButton) {
            EcCoordinate lat, lon;
            if (XyToLatLon(e->x(), e->y(), lat, lon)) {

                if (newGuardZoneShape == ::GUARD_ZONE_CIRCLE) {
                    if (guardZonePoints.isEmpty()) {
                        // Titik pertama - set center
                        guardZonePoints.append(QPointF(e->x(), e->y()));
                        qDebug() << "Circle center set at:" << e->x() << e->y();
                        emit statusMessage(tr("Center set. Now click to set radius."));
                    }
                    else if (guardZonePoints.size() == 1) {
                        // Titik kedua - calculate radius dan create circle
                        QPointF center = guardZonePoints.first();

                        // Convert center ke lat/lon
                        EcCoordinate centerLat, centerLon;
                        if (XyToLatLon(center.x(), center.y(), centerLat, centerLon)) {

                            // Calculate distance dalam nautical miles
                            double distNM, bearing;
                            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                                   centerLat, centerLon,
                                                                   lat, lon,
                                                                   &distNM, &bearing);

                            qDebug() << "Creating circle with radius:" << distNM << "NM";

                            // Create the guardzone
                            createCircularGuardZoneNew(centerLat, centerLon, distNM);

                            // Finish creation
                            finishCreateGuardZone();
                        }
                    }
                }
                else if (newGuardZoneShape == ::GUARD_ZONE_POLYGON) {
                    // Add point to polygon
                    guardZonePoints.append(QPointF(e->x(), e->y()));
                    qDebug() << "Polygon point added:" << e->x() << e->y() << "Total points:" << guardZonePoints.size();

                    if (guardZonePoints.size() == 1) {
                        emit statusMessage(tr("First point added. Continue clicking to add more points."));
                    } else {
                        emit statusMessage(tr("Point %1 added. Right-click to finish (minimum 3 points).").arg(guardZonePoints.size()));
                    }

                    update(); // Redraw untuk show preview
                }
            }
        }
        else if (e->button() == Qt::RightButton) {
            // Right click - finish creation atau cancel
            if (newGuardZoneShape == ::GUARD_ZONE_POLYGON && guardZonePoints.size() >= 3) {
                // Finish polygon creation
                createPolygonGuardZoneNew();
                finishCreateGuardZone();
            } else {
                // Cancel creation
                cancelCreateGuardZone();
            }
        }
        return; // Don't process further jika dalam creation mode
    }
    // ========== END GUARDZONE CREATION MODE ==========

    // ========== EXISTING WAYPOINT AND OTHER LOGIC (unchanged) ==========
    waypointLeftClick(e);
    waypointRightClick(e);
}

void EcWidget::waypointRightClick(QMouseEvent *e){
    if (e->button() == Qt::RightButton && !creatingGuardZone) {
        if (activeFunction == MOVE_WAYP && moveSelectedIndex != -1) {
           // Cancel move operation
           ghostWaypoint.visible = false;
           moveSelectedIndex = -1;
           activeFunction = PAN;
           update();
           return;
        }

        // Check if in route mode - right-click to end route
        if (activeFunction == CREATE_ROUTE) {
            // Show confirmation dialog
            QMessageBox::StandardButton result = QMessageBox::question(this,
                tr("End Route"),
                tr("Do you want to end the current route creation?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);

            if (result == QMessageBox::Yes) {
                endRouteMode();
            }
            return;
        }

        // Check if right-clicked on a waypoint first
        int clickedWaypointIndex = findWaypointAt(e->x(), e->y());
        if (clickedWaypointIndex != -1) {
            createWaypointToolbox(e->pos(), clickedWaypointIndex);
            // showWaypointContextMenu(e->pos(), clickedWaypointIndex);
            return; // Return early - jangan lanjut ke normal right click
        }

        // Check if right-clicked on a legline
        int leglineRouteId, leglineSegmentIndex;
        int leglineDistance = findLeglineAt(e->x(), e->y(), leglineRouteId, leglineSegmentIndex);

        if (leglineDistance != -1) {
            createLeglineToolbox(e->pos(), leglineRouteId, leglineSegmentIndex);
            // showLeglineContextMenu(e->pos(), leglineRouteId, leglineSegmentIndex);
            return; // Return early - jangan lanjut ke normal right click
        }

        // Check if right-clicked on a guardzone
        if (guardZoneManager) {
            int clickedGuardZoneId = guardZoneManager->getGuardZoneAtPosition(e->x(), e->y());

            if (clickedGuardZoneId != -1) {
                guardZoneManager->showGuardZoneContextMenu(e->pos(), clickedGuardZoneId);
                return; // Return early - jangan lanjut ke normal right click
            }
        }

        // Show map context menu (Create Route)
        // showMapContextMenu(e->pos());

        // =================== IMPORTANT!! DONT CHANGE IT! ===================
        pickX = e->x();
        pickY = e->y();
        emit mouseRightClick(e->pos());
        // ===================================================================

        return;
    }
}

void EcWidget::waypointLeftClick(QMouseEvent *e){
    if (e->button() == Qt::LeftButton)
    {
        EcCoordinate lat, lon;
        if (XyToLatLon(e->x(), e->y(), lat, lon))
        {
            if (activeFunction == MOVE_WAYP)
            {
                moveWaypointAt(e->x(), e->y());
                return;
            }
            else if (activeFunction == EDIT_WAYP)
            {
                editWaypointAt(e->x(), e->y());
                return;
            }
            else if (activeFunction == REMOVE_WAYP)
            {
                removeWaypointAt(e->x(), e->y());
                activeFunction = PAN;
                emit waypointCreated();
                return;
            }
            // CREATE_WAYP removed - now only route-based navigation
            else if (activeFunction == INSERT_WAYP)
            {
                // Insert waypoint mode - find closest route segment and insert
                insertWaypointAt(lat, lon);

                // Return to PAN mode after insertion
                activeFunction = PAN;
                if (mainWindow) {
                    mainWindow->statusBar()->showMessage(tr("Waypoint inserted"), 3000);
                    mainWindow->setWindowTitle(APP_TITLE);
                }
                emit waypointCreated();
            }
            else if (activeFunction == CREATE_ROUTE)
            {
                // Route mode - continuous waypoint creation
                createWaypointAt(lat, lon);

                // Update status for next waypoint
                if (mainWindow) {
                    // mainWindow->statusBar()->showMessage(
                    //     tr("Route Mode: Waypoint %1 created. Click for next waypoint or ESC/right-click to end")
                    //     .arg(routeWaypointCounter), 0);

                    mainWindow->routesStatusText->setText(
                                tr("Route Mode: Waypoint %1 created. Click for next waypoint or ESC/right-click to end").
                                arg(routeWaypointCounter));
                }

                routeWaypointCounter++;

                // Emit signal to update route panel
                emit waypointCreated();

                // Stay in CREATE_ROUTE mode for continuous creation
            }
            else
            {
                if (dragMode){
                    isDragging = true;
                    lastPanPoint = e->pos();
                    tempOffset = QPoint(0,0);

                    setCursor(Qt::OpenHandCursor);

                    QResizeEvent ev(size(), size());  // oldSize = newSize
                    this->resizeEvent(&ev);
                }
                else {
                    // ======== STABLE ========= //
                    // Normal pan/click
                    SetCenter(lat, lon);
                    Draw();
                    // ======== STABLE ========= //
                }
            }
        }
    }
}


/*---------------------------------------------------------------------------*/


void EcWidget::mouseMoveEvent(QMouseEvent *e)
{
    // Simpan posisi mouse terakhir
    lastMousePos = e->pos();

    if (editingAOI) {
        if (draggedAoiVertex >= 0) {
            // Ghost move: do not modify original AOI until drop
            EcCoordinate lat, lon; 
            if (XyToLatLon(e->x(), e->y(), lat, lon)) {
                aoiVertexDragging = true;
                aoiGhostLat = lat;
                aoiGhostLon = lon;
                update();
            }
        }
        return;
    }


    if (creatingAOI) {
        update();
        return;
    }

    // Cek apakah mouse masih di atas AIS target yang sama
    EcAISTargetInfo* targetInfo = findAISTargetInfoAtPosition(lastMousePos);

    if (targetInfo && displayCategory != EC_DISPLAYBASE) {
        // Mouse di atas AIS target
        if (!isAISTooltipVisible) {
            // Start timer untuk delay tooltip
            aisTooltipTimer->start();
        } else {
            // Update posisi tooltip jika sudah visible
            QPoint tooltipPos = mapToGlobal(lastMousePos);
            tooltipPos.setX(tooltipPos.x() + 10);
            tooltipPos.setY(tooltipPos.y() + 10);
            if (aisTooltip) {
                aisTooltip->move(tooltipPos);
            }
        }
    } else {
        // Mouse tidak di atas AIS target
        aisTooltipTimer->stop();
        if (isAISTooltipVisible) {
            hideAISTooltip();
            isAISTooltipVisible = false;
        }
    }

    if (routeSafetyFeature && routeSafeMode) {
        QString hazardTip = routeSafetyFeature->tooltipForPosition(lastMousePos);
        if (!hazardTip.isEmpty()) {
            QToolTip::showText(e->globalPos(), hazardTip, this);
        } else if (!QToolTip::text().isEmpty()) {
            QToolTip::hideText();
        }
    }

    // Panggil handling yang sudah ada
    if (guardZoneManager && guardZoneManager->handleMouseMove(e)) {
        return;
    }

    // Handle edit guardzone via manager first
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        if (guardZoneManager->handleMouseMove(e)) {
            return; // Event handled by manager
        }
    }

    // ========== PERBAIKAN: OPTIMIZED CREATION MODE TRACKING ==========
    if (creatingGuardZone) {
        // Update current mouse position
        currentMousePos = e->pos();

        // Validate position is within widget bounds
        if (rect().contains(currentMousePos)) {
            // ========== THROTTLING UPDATE ==========
            static QTime lastUpdateTime;
            QTime currentTime = QTime::currentTime();

            // Only update if enough time has passed (16ms = ~60 FPS)
            if (!lastUpdateTime.isValid() || lastUpdateTime.msecsTo(currentTime) >= 16) {
                update();
                lastUpdateTime = currentTime;
            }
            // =====================================
        }

        return; // Don't process normal mouse move during creation
    }
    // ==============================================================

    // Handle ghost waypoint preview saat move waypoint
    if (activeFunction == MOVE_WAYP && moveSelectedIndex != -1 && ghostWaypoint.visible) {
        EcCoordinate lat, lon;
        if (XyToLatLon(e->x(), e->y(), lat, lon)) {
            // Update ghost waypoint position
            ghostWaypoint.lat = lat;
            ghostWaypoint.lon = lon;

            // Throttle update untuk performance
            static QTime lastGhostUpdate;
            QTime currentTime = QTime::currentTime();

            if (!lastGhostUpdate.isValid() || lastGhostUpdate.msecsTo(currentTime) >= 16) {
                update(); // Trigger repaint untuk ghost waypoint
                lastGhostUpdate = currentTime;
            }
        }
    }

    // Handle ghost waypoint preview saat create route
    if (activeFunction == CREATE_ROUTE && isRouteMode) {
        EcCoordinate lat, lon;
        if (XyToLatLon(e->x(), e->y(), lat, lon)) {
            // Update ghost waypoint position for create route
            ghostWaypoint.visible = true;
            ghostWaypoint.lat = lat;
            ghostWaypoint.lon = lon;
            ghostWaypoint.label = QString("WP%1").arg(routeWaypointCounter);
            ghostWaypoint.routeId = currentRouteId;
            ghostWaypoint.waypointIndex = routeWaypointCounter - 1; // Index untuk next waypoint

            // Throttle update untuk performance
            static QTime lastCreateGhostUpdate;
            QTime currentTime = QTime::currentTime();

            if (!lastCreateGhostUpdate.isValid() || lastCreateGhostUpdate.msecsTo(currentTime) >= 16) {
                update(); // Trigger repaint untuk ghost waypoint dan leglines
                lastCreateGhostUpdate = currentTime;
            }
        }
    }

    // Normal mouse move processing
    EcCoordinate lat, lon;
    if (XyToLatLon(e->x(), e->y(), lat, lon)) {
        emit mouseMove(lat, lon);
    }

    // Update AOI hover segment label state
    if (enableAoiSegmentLabels) {
        updateAoiHoverLabel(e->pos());
    }

    // EBL/VRM live measure from ownship
    if (eblvrm.measureMode) {
        eblvrm.onMouseMove(this, e->x(), e->y());
        update();
    }


    // LAMUN DRAGGING
    if (isDragging && dragMode) {
        tempOffset = e->pos() - lastPanPoint;
        setCursor(Qt::ClosedHandCursor);

        update();
    }

}



/*---------------------------------------------------------------------------*/


void EcWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (editingAOI) {
        if (e->button() == Qt::LeftButton) {
            // Commit ghost move if active
            if (draggedAoiVertex >= 0 && aoiVertexDragging) {
                for (int ai = 0; ai < aoiList.size(); ++ai) if (aoiList[ai].id == editingAoiId) {
                    auto& a = aoiList[ai];
                    if (draggedAoiVertex >=0 && draggedAoiVertex < a.vertices.size()) {
                        a.vertices[draggedAoiVertex] = QPointF(aoiGhostLat, aoiGhostLon);
                        emit aoiListChanged();
                        saveAOIs();
                    }
                    break;
                }
            }
            // Reset drag state and exit edit mode
            aoiVertexDragging = false;
            draggedAoiVertex = -1;
            finishEditAOI();
            update();
            return;
        }
    }
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        if (guardZoneManager->handleMouseRelease(e)) {
            return; // Event handled by manager
        }
    }

    // Handle edit guardzone via manager - TAMBAHKAN INI
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        if (guardZoneManager->handleMouseRelease(e)) {
            return; // Event handled by manager
        }
    }


    if (e->button() == Qt::LeftButton && isDragging && dragMode) {
        isDragging = false;
        QResizeEvent ev(size(), size());
        this->resizeEvent(&ev);

        QPoint releasePos = e->pos();

        // --- ambil geo dari titik press & release ---
        EcCoordinate pressLat, pressLon;
        EcCoordinate releaseLat, releaseLon;

        bool ok1 = XyToLatLon(lastPanPoint.x(), lastPanPoint.y(), pressLat, pressLon);
        bool ok2 = XyToLatLon(releasePos.x(), releasePos.y(), releaseLat, releaseLon);

        if (ok1 && ok2) {
            // Hitung selisih geo
            double deltaLat = pressLat - releaseLat;
            double deltaLon = pressLon - releaseLon;

            // Update center ke posisi baru
            currentLat += deltaLat;
            currentLon += deltaLon;
        }

        tempOffset = QPoint();  // reset drag offset
        unsetCursor();

        Draw();             // redraw chart penuh
    }


    QWidget::mouseReleaseEvent(e);
}


void EcWidget::wheelEvent  (QWheelEvent *e)
{
  if (e->delta() > 0)
  {
    SetScale(GetScale()/1.2);
    //draw(true);
    Draw();
  }
  else if (e->delta() < 0)
  {
    SetScale(GetScale()*1.2);
    //draw(true);
    Draw();
  }
  e->accept();
}

/*---------------------------------------------------------------------------*/

void EcWidget::searchOk(EcCoordinate lat, EcCoordinate lon)
{
    int x, y;
    if(LatLonToXy(lat, lon, x, y))
    {
        pickX = x;
        pickY = y;
    }
}
/*---------------------------------------------------------------------------*/
void EcWidget::GetPickedFeatures(QList<EcFeature> &pickedFeatureList)
{
  if (!view) return;

  EcCellId * cids = NULL;
  // Get all cell which are currently loaded in the view
  int cellNum = EcChartGetLoadedCellsOfView(view, &cids);

  if (cellNum <= 0) return;

  EcFeature * featureList = NULL;
  EcFeature * allFeaturesList = NULL;
  int pickPixel = 7;
  // Get all features of all charts which are located at the pick position.
  int nAllFeatures = EcQueryPickVisible(view, dictInfo, pickX, pickY, pickPixel, "", "", '!', cids, cellNum, &allFeaturesList);
  // Only get the features of the cells of the best usage, i.e. which are drawn on top
  int nFeatures = EcQueryPickFilter(dictInfo, allFeaturesList, nAllFeatures, &featureList);
  // Free the allocated memory for the list of all features
  if (nAllFeatures > 0)
    EcFree((void*)allFeaturesList);

  if (nFeatures > 0)
  {
    for (int i=0; i<nFeatures; i++)
    {
      pickedFeatureList.append(featureList[i]);
    }
    // Free the allocated memory for the list of features
    EcFree((void*)featureList);
  }
  // Free the allocated memory for the list of cells
  if (cids)
    EcFree((void*)cids);
}

/*---------------------------------------------------------------------------*/
void EcWidget::GetSearchedFeatures(QList<EcFeature> &pickedFeatureList)
{
    if (!view) return;

    EcCellId * cids = NULL;
    // Get all cell which are currently loaded in the view
    int cellNum = EcChartGetLoadedCellsOfView(view, &cids);

    if (cellNum <= 0) return;

    EcFeature * featureList = NULL;
    EcFeature * allFeaturesList = NULL;
    int pickPixel = 7;
    // Get all features of all charts which are located at the pick position.
    int nAllFeatures = EcQueryPickVisible(view, dictInfo, pickX, pickY, pickPixel, "", "", '!', cids, cellNum, &allFeaturesList);
    // Only get the features of the cells of the best usage, i.e. which are drawn on top
  int nFeatures = EcQueryPickFilter(dictInfo, allFeaturesList, nAllFeatures, &featureList);
  // Free the allocated memory for the list of all features
  if (nAllFeatures > 0)
    EcFree((void*)allFeaturesList);

  if (nFeatures > 0)
  {
    for (int i=0; i<nFeatures; i++)
    {
      pickedFeatureList.append(featureList[i]);
    }
    // Free the allocated memory for the list of features
    EcFree((void*)featureList);
  }
  // Free the allocated memory for the list of cells
  if (cids)
    EcFree((void*)cids);
}

/*---------------------------------------------------------------------------*/
void EcWidget::GetPickedFeaturesSubs(QList<EcFeature> &pickedFeatureList, EcCoordinate lat, EcCoordinate lon)
{
    if (!view) return;
    QJsonObject featureObject;

    int pick_X, pick_Y;

    EcDrawLatLonToXy(view, lat, lon, &pick_X, &pick_Y);

    //qDebug() << "LAT: " << lat;
    //qDebug() << "LON: " << lon;

    //qDebug() << "X: " << pick_X;
    //qDebug() << "Y: " << pick_Y;

    //XyToLatLon(pickX, pickY, latt, lonn);

    EcCellId * cids = NULL;
    // Get all cell which are currently loaded in the view
    int cellNum = EcChartGetLoadedCellsOfView(view, &cids);

    if (cellNum <= 0) return;

    EcFeature * featureList = NULL;
    EcFeature * allFeaturesList = NULL;
    int pickPixel = 7;
    // Get all features of all charts which are located at the pick position.
    int nAllFeatures = EcQueryPickVisible(view, dictInfo, pick_X, pick_Y, pickPixel, "", "", '!', cids, cellNum, &allFeaturesList);
    // Only get the features of the cells of the best usage, i.e. which are drawn on top
      int nFeatures = EcQueryPickFilter(dictInfo, allFeaturesList, nAllFeatures, &featureList);
      // Free the allocated memory for the list of all features
      if (nAllFeatures > 0)
            EcFree((void*)allFeaturesList);

    if (nFeatures > 0)
    {
        for (int i=0; i<nFeatures; i++)
        {
            pickedFeatureList.append(featureList[i]);
        }
        // Free the allocated memory for the list of features
        EcFree((void*)featureList);
    }
    // Free the allocated memory for the list of cells
    if (cids)
        EcFree((void*)cids);
}

/*---------------------------------------------------------------------------*/

void EcWidget::InitS63()
{
    // check M_KEY
    unsigned char mKey[6];
    if (!asciiToByte(M_KEY, mKey))
        QMessageBox::information(this, "Initialize S-63", "Invalid Manufacturer key");

    // decrypt the user permit
    unsigned char *mId = (unsigned char *)MID;
    s63HwId = (unsigned char *)HWID;
    if (!EcS63DecryptUserPermit(USERPERMIT, mKey, &s63HwId, &mId))
        QMessageBox::information(this, "Initialize S-63", "Invalid User Permit");
    // mId is not used
    EcFree(mId);

    // create a generic permit list structure which is used for on-the-fly permit checks
    permitList = EcChartPermitListCreate();

    // read the systems S-63 permits
    QFile s63PermitFile(s63permitFileName);
    if( s63PermitFile.exists() && s63PermitFile.size() > 0 )
    {
        QByteArray tmp = s63permitFileName.toLatin1();
        int ret = EcS63ReadCellPermitsIntoPermitList(permitList, tmp.constBegin(), s63HwId, True, NULL, NULL);
        int nPermits = EcChartGetAllPermits( permitList, EC_S63_PERMIT, NULL );
        if(nPermits > 0)
            EcCellSetChartPermitList(permitList);
    }

}

/*---------------------------------------------------------------------------*/

bool EcWidget::SetS63MKey(const int s63mkey)
{
    return true;
}

/*---------------------------------------------------------------------------*/

bool EcWidget::SetS63UserPermit(const int s63up)
{
    return true;
}

/*---------------------------------------------------------------------------*/

bool EcWidget::initColors()
{
  int numOfColors = EcChartReadColors (view, NULL);
  if (numOfColors <= 0) return false;

  int *tbl = new int[numOfColors];
  for (int i=0; i<numOfColors; ++i) tbl[i] = i;
  // Set the color index mapping for conversion of chart colors into drawing colors
  EcDrawSetColorIndex(view, tbl, numOfColors);
  delete [] tbl;

  // Create a new palette and set its entries to one of the Presentation Library's color schemes
#ifdef _WIN32
  hPalette = (HPALETTE)EcDrawNTSetColorSchemeExt (view, NULL, currentColorScheme, currentGreyMode, currentBrightness, 1);
#else
  if (DefaultVisual(dpy, x11Info().appScreen() )->c_class != TrueColor)
    return false;
  EcDrawX11SetColorScheme (view, dpy, cmap, currentColorScheme, currentGreyMode, currentBrightness);
#endif

  int red, green, blue;
  // Get the RGB definition of the "No data" colour token from one of the Presentation Library's color schemes.
  EcDrawGetTokenRGB (view, const_cast<char*>("NODTA"), currentColorScheme, &red, &green, &blue);
  bg.setRgb (red, green, blue);

  return true;
}

/*---------------------------------------------------------------------------*/

void EcWidget::clearBackground()
{
#ifdef _WIN32
  HANDLE   hPen,hBrush;

  HPALETTE oldPal = SelectPalette (hdc, hPalette, true);

  COLORREF cref = RGB(bg.red(), bg.green(), bg.blue());
  LOGBRUSH myBrush;
  myBrush.lbColor = cref;
  myBrush.lbStyle = BS_SOLID;
  myBrush.lbHatch = 0;
  hBrush = CreateBrushIndirect(&myBrush);
  hPen   = CreatePen(PS_SOLID, 1, cref );

  // select the new pen and brush and save the old handles
  HANDLE hPenOld = SelectObject(hdc, hPen);
  HANDLE hBrushOld = SelectObject(hdc, hBrush);

  Rectangle(hdc, 0, 0, width(), height());

  // select the old pen and brush
  SelectObject(hdc, hPenOld);
  SelectObject(hdc, hBrushOld);

  // delete the bruch and pen handles
  DeleteObject(hPen);
  DeleteObject(hBrush);

  SelectPalette (hdc, oldPal, false);
#else
  drawPixmap.fill(bg);
#endif
}

/*---------------------------------------------------------------------------*/

bool EcWidget::_import(const char *n)
{
  bool res=true;
  emit import(n, res);
  return res;
}

/*---------------------------------------------------------------------------*/

bool EcWidget::_replace(const char *n)
{
  bool res=true;
  emit replace(n, res);
  return res;
}

/*---------------------------------------------------------------------------*/

Bool EcWidget::ImportCB(EcDENC *denc, const char *name, int reason, void *udata)
{
  EcWidget *mychart = (EcWidget*)udata;

  // As an example only the first two reasons are considered
  switch (reason)
  {
  case EC_DENC_IMPORT:
    return mychart->_import(name);
    break;
  case EC_DENC_REPLACE_PACKAGE:
    return mychart->_replace(name);
    break;
  case EC_DENC_DELETE:
    break;
  case EC_DENC_UPDATE:
    break;
  case EC_DENC_ARCHIV:
    break;
  case EC_DENC_UPDATE_APPLIED:
    break;
  case EC_DENC_UPDATE_DELETED:
    break;
  case EC_DENC_UPDATE_WRONG_UPDN:
    break;
  case EC_DENC_UPDATE_WRONG_EDTN:
    break;
  case EC_DENC_UPDATE_NO_CELL:
    break;
  case EC_DENC_UPDATE_ERROR:
    break;
  case EC_DENC_IMPORT_ERROR:
    break;
  case EC_DENC_IMPORT_ERROR_NO_PERMIT:
    break;
  case EC_DENC_IMPORT_ERROR_BAD_CRC:
    break;
  case EC_DENC_IMPORT_CANCELLED:
    break;
  case EC_DENC_IMPORT_ERROR_WRONG_HWID:
    break;
  default:
    break;
  }

  return true;
}

/*---------------------------------------------------------------------------*/

Bool EcWidget::S57CB(int volId, int volNum, const char *fileName)
{
    // diaog which asks to insert the next disk (volId) of x (volNum) CDs
    return True;
}

/*---------------------------------------------------------------------------*/

// convert 10 character string to 5 byte array
bool EcWidget::asciiToByte(const char *keyAscii, unsigned char keyByte[])
{
    // check length of key
    if (strlen(keyAscii) == 5)
    {
        for (int i=0; i<5; i++)
            keyByte[i] = keyAscii[i];
        keyByte[5] = (char)0;
        return true;
    }
    else if (strlen(keyAscii) == 10)
    {
        unsigned int i0, i1, i2, i3, i4;
        if (sscanf(keyAscii, "%02X%02X%02X%02X%02X", &i0, &i1, &i2, &i3, &i4) == 5)
        {
            keyByte[0] = (unsigned char)i0;
            keyByte[1] = (unsigned char)i1;
            keyByte[2] = (unsigned char)i2;
            keyByte[3] = (unsigned char)i3;
            keyByte[4] = (unsigned char)i4;
            keyByte[5] = (char)0;
            return true;
        }
    }
    return false;
}

void EcWidget::SetWaypointPos(EcCoordinate lat, EcCoordinate lon)
{
    wplat = lat;
    wplon = lon;
}

void EcWidget::setOwnShipTrail(bool trail){
    showOwnShipTrail = trail;
}

bool EcWidget::getOwnShipTrail(){
    return showOwnShipTrail;
}


/* draw the user defined cell */

bool EcWidget::drawUdo(void)
{
    // projection
    EcDrawSetProjection(view, EC_GEO_PROJECTION_MERCATOR, wplat, wplon, 0, 0);

    // symbolize udo cell
    if (!EcChartSymbolizeCell(view,udoCid))
        return FALSE;

    // set the new viewport
    if (!EcDrawSetViewport(view,wplat,wplon,range,0))
        return FALSE;

    qDebug() << "Start draw";
    // clear drawing area
    // SelectPalette(drawA,palHandle,TRUE);
    SelectPalette (hdc, hPalette, true);
    RealizePalette(hdc);
    // clearBackGnd(view,drawA,pixMap);
    clearBackground();

    // draw udo cell into device context
    // EcDrawNTDrawCells( view, overlayDC, NULL, 1, &aisCellId, 0 );

    if (!EcDrawNTDrawCells(view, hdc, hBitmapOverlay, 1, &udoCid, 0))
        return FALSE;

    return TRUE;
}

// Create a new udo cell
bool EcWidget::createUdoCell()
{
    // create a new udo cell
    EcCellCreate("ROUTES.7CB",4096);
    udoCid = EcCellMap("ROUTES.7CB", EC_ACCESSWRITE, 0);
    if (udoCid == EC_NOCELLID)
    {
        qDebug() << "Cell could not be created";
        // QMessageBox::warning(this, tr("error showroute"), tr("Cell could not be created"));
        // EcChartViewDelete(view);
        // EcDictionaryFree(dictInfo);
        // exit(1);
        return false;
    }

    // assign cell to view
    if( !EcChartAssignCellToView( view, udoCid ) )
    {
        qDebug() << "Cell could not be created";
        EcCellUnmap( udoCid );
        return false;
    }

    qDebug() << "ok";
    return true;
};

///////////////////////////////////////////////   AIS - Methods   //////////////////////////////////////////////////

// Initialize AIS context.
//////////////////////////
void EcWidget::InitAIS( EcDictInfo *dict)
{
#ifdef _WIN32
  QString strAisLib = "dpimaist.dll";            // Path to dpimaist.dll
#else
  QString strAisLib = "libdpimaist.so";
#endif
  // JARAK (NM) AIS WARNA MERAH
  double dWarnDist = 0.5;

  // CPATCPASettings& settings = CPATCPASettings::instance();
  // double dWarnDist = settings.getCPAThreshold();

  double dWarnCPA = SettingsManager::instance().data().cpaThreshold;
  int iWarnTCPA = SettingsManager::instance().data().cpaThreshold;

  int iTimeOut = 1;
  Bool bInternalGPS = False;
  Bool bAISSymbolize;
  QString strErrLogAis = QString( "%1%2%3" ).arg( QCoreApplication::applicationDirPath() ).arg( "/" ).arg( "errorAISLog.txt" );

  _aisObj = new Ais( this, view, dict, ownShip.lat, ownShip.lon,
    ownShip.sog, ownShip.cog, dWarnDist, dWarnCPA,
    iWarnTCPA, strAisLib, iTimeOut, bInternalGPS, &bAISSymbolize, strErrLogAis );

  // QObject::connect( _aisObj, SIGNAL( signalRefreshChartDisplay( double, double, double ) ), this, SLOT( slotRefreshChartDisplay( double, double, double ) ) );
  // QObject::connect( _aisObj, SIGNAL( signalRefreshCenter( double, double ) ), this, SLOT( slotRefreshCenter( double, double ) ) );
}

void EcWidget::setCPAPanelToAIS(CPATCPAPanel* panel) {
    if (_aisObj) {
        _aisObj->setCPAPanel(panel);
    }
}

// Read an AIS logfile and display the AIS targets on the chart display.
////////////////////////////////////////////////////////////////////////
void EcWidget::ReadAISLogfile( const QString &aisLogFile )
{
  if( deleteAISCell() == false )
  {
    QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not remove old AIS overlay cell. Please restart the program." ) );
    return;
  }

  if( createAISCell() == false )
  {
    QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not create AIS overlay cell. Please restart the program." ) );
    return;
  }

  //qDebug() << showAIS;

  //deleteAISCell();
  //createAISCell();

  clearOwnShipTrail();
  _aisObj->clearTargetData();
  _aisObj->setAISCell( aisCellId );
  _aisObj->readAISLogfile( aisLogFile );
}

void EcWidget::createDvrRead(){
    if( deleteAISCell() == false )
    {
      QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not remove old AIS overlay cell. Please restart the program." ) );
      return;
    }

    if( createAISCell() == false )
    {
      QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not create AIS overlay cell. Please restart the program." ) );
      return;
    }

    clearOwnShipTrail();
    _aisObj->clearTargetData();
    _aisObj->setAISCell( aisCellId );
}

void EcWidget::readAISVariableString(const QString &aisLogFile){
    _aisObj->readAISVariableString(aisLogFile);
}

void EcWidget::ReadAISLogfileWDelay( const QString &aisLogFile)
{
    if( deleteAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not remove old AIS overlay cell. Please restart the program." ) );
        return;
    }

    if( createAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not create AIS overlay cell. Please restart the program." ) );
        return;
    }

    //qDebug() << showAIS;

    //deleteAISCell();
    //createAISCell();

    stopFlag = false;

    _aisObj->setAISCell( aisCellId );
    _aisObj->readAISLogfileWDelay(aisLogFile, 300, &stopFlag);
}

// Read an AIS from MOOSDB -- NAV INFO
////////////////////////////////////////////////////////////////////////
void EcWidget::ReadAISVariable( const QStringList &aisDataLines )
{
    if( deleteAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not remove old AIS overlay cell. Please restart the program." ) );
        return;
    }

    if( createAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not create AIS overlay cell. Please restart the program." ) );
        return;
    }

    clearOwnShipTrail();
    _aisObj->clearTargetData();
    _aisObj->setAISCell( aisCellId );
    _aisObj->readAISVariable( aisDataLines );
}

// SUBSCRIBE-PUBLISH TO MOOSDB
void EcWidget::startAISSubscribe() {
    if (!deleteAISCell()) {
        QMessageBox::warning(this, tr("ReadAISLogfile"), tr("Could not remove old AIS overlay cell. Please restart the program."));
        return;
    }
    if (!createAISCell()) {
        QMessageBox::warning(this, tr("ReadAISLogfile"), tr("Could not create AIS overlay cell. Please restart the program."));
        return;
    }

    _aisObj->clearTargetData();
    _aisObj->setAISCell(aisCellId);
    startAISConnection();
}

void EcWidget::startAISConnection()
{
    threadAIS = new QThread(this);
    subscriber = new AISSubscriber();

    // TO TELL THE WORLD THAT U HAVE CREATE AISSUB!!
    emit aisSubCreated(subscriber);

    subscriber->moveToThread(threadAIS);

    QString sshIP = SettingsManager::instance().data().moosIp;
    quint16 sshPort = 5000;

    connect(threadAIS, &QThread::started, [=]() {
        subscriber->connectToHost(sshIP, sshPort);
    });

    // OWNSHIP
    connect(subscriber, &AISSubscriber::navLatReceived, this, [=](double lat) {
        navShip.lat = lat;

        // QString slat = latLonToDegMin(lat, true);
        // navShip.slat = slat;
        updateAttachedGuardZoneFromNavShip();
        if (shipDotEnabled) update();
    });
    connect(subscriber, &AISSubscriber::navLongReceived, this, [=](double lon) {
        navShip.lon = lon;

        // QString slon = latLonToDegMin(lon, false);
        // navShip.slon = slon;
        updateAttachedGuardZoneFromNavShip();
        if (shipDotEnabled) update();
    });


    connect(subscriber, &AISSubscriber::navDepthReceived, this, [=](double depth) { navShip.depth = depth;});

    connect(subscriber, &AISSubscriber::navHeadingReceived, this, [=](double hdg) {
        navShip.heading = hdg;
        if (mainWindow && orientation == NorthUp){
            mainWindow->setCompassHeading(hdg);
        }
        updateAttachedGuardZoneFromNavShip();
    });

    connect(subscriber, &AISSubscriber::navHeadingOGReceived, this, [=](double hog) { navShip.heading_og = hog;});

    connect(subscriber, &AISSubscriber::navCourseOGReceived, this, [=](double cog) {
        navShip.course_og = cog;
    });

    connect(subscriber, &AISSubscriber::navSOGReceived, this, [=](double sog) {
        navShip.sog = sog;
        QDateTime now = QDateTime::currentDateTime();
        speedBuffer.append(qMakePair(now, sog));
    });

    connect(subscriber, &AISSubscriber::navLatDmsReceived, this, [=](const QString &v) { navShip.lat_dms = v;});
    connect(subscriber, &AISSubscriber::navLongDmsReceived, this, [=](const QString &v) { navShip.lon_dms = v;});

    connect(subscriber, &AISSubscriber::navLatDmmReceived, this, [=](const QString &v) { navShip.lat_dmm = v;});
    connect(subscriber, &AISSubscriber::navLongDmmReceived, this, [=](const QString &v) { navShip.lon_dmm = v;});

    connect(subscriber, &AISSubscriber::navSpeedOGReceived, this, [=](double speed_og) { navShip.speed_og = speed_og;});
    connect(subscriber, &AISSubscriber::navSpeedReceived, this, [=](double spe) { navShip.speed = spe;});
    connect(subscriber, &AISSubscriber::navYawReceived, this, [=](double yaw) { navShip.yaw = yaw;});
    connect(subscriber, &AISSubscriber::navZReceived, this, [=](double z) { navShip.z = z;});
    connect(subscriber, &AISSubscriber::navStwReceived, this, [=](double stw) { navShip.stw = stw;});
    connect(subscriber, &AISSubscriber::navDraftReceived, this, [=](double draft) { navShip.draft = draft;});
    connect(subscriber, &AISSubscriber::navDriftReceived, this, [=](double drift) { navShip.drift = drift;});
    connect(subscriber, &AISSubscriber::navDriftAngleReceived, this, [=](double drift_angle) { navShip.drift_angle = drift_angle;});
    connect(subscriber, &AISSubscriber::navSetReceived, this, [=](double set) { navShip.set = set;});
    connect(subscriber, &AISSubscriber::navRotReceived, this, [=](double rot) { navShip.rot = rot;});
    connect(subscriber, &AISSubscriber::navDepthBelowKeelReceived, this, [=](double depth_below_keel) { navShip.depth_below_keel = depth_below_keel;});

    connect(subscriber, &AISSubscriber::navDeadReckonReceived, this, [=](QString deadReckon) {
        if (navShip.deadReckon != deadReckon){
            navShip.deadReckon = deadReckon;

            emit subscriber->connectionStatusChanged(true);
        }
    });

    connect(subscriber, &AISSubscriber::mapInfoReqReceived, this, &EcWidget::processMapInfoReq);
    connect(subscriber, &AISSubscriber::processingAis, this, &EcWidget::processAis);
    connect(subscriber, &AISSubscriber::processingData, this, &EcWidget::processData);

    // ROUTE INFORMATION
    connect(subscriber, &AISSubscriber::rteWpBrgReceived, this, [=](const double &v) { activeRoute.rteWpBrg = v;});
    connect(subscriber, &AISSubscriber::rteXtdReceived, this, [=](const QString &v) { activeRoute.rteXtd = v;});
    connect(subscriber, &AISSubscriber::rteCrsReceived, this, [=](const double &v) { activeRoute.rteCrs = v;});
    connect(subscriber, &AISSubscriber::rteCtmReceived, this, [=](const double &v) { activeRoute.rteCtm = v;});
    connect(subscriber, &AISSubscriber::rteDtgReceived, this, [=](const double &v) { activeRoute.rteDtg = v;});
    connect(subscriber, &AISSubscriber::rteDtgMReceived, this, [=](const double &v) { activeRoute.rteDtgM = v;});
    connect(subscriber, &AISSubscriber::rteTtgReceived, this, [=](const QString &v) { activeRoute.rteTtg = v;});
    connect(subscriber, &AISSubscriber::rteEtaReceived, this, [=](const QString &v) { activeRoute.rteEta = v;});

    connect(subscriber, &AISSubscriber::publishToMOOSDB, this, &EcWidget::publishToMOOSDB);

    // AIS
    connect(_aisObj, &Ais::nmeaTextAppend, this, [=](const QString &msg){
        nmeaText->append(msg);
    });

    connect(_aisObj, &Ais::signalRefreshChartDisplay, this, &EcWidget::slotRefreshChartDisplayThread, Qt::QueuedConnection);
    connect(_aisObj, &Ais::signalRefreshCenter, this, &EcWidget::slotRefreshCenter, Qt::QueuedConnection);


    EcDENC *denc = nullptr;
    EcDictInfo *dictInfo = nullptr;
    QWidget *parentWidget = nullptr;

    PickWindow *pickWindow = new PickWindow(parentWidget, dictInfo, denc);

    connect(_aisObj, &Ais::pickWindowOwnship, this, [=](){
        if (navShip.lat != 0){
            ownShipText->setHtml(pickWindow->ownShipAutoFill());
            //_cpaPanel->updateOwnShipInfo(navShip.lat, navShip.lon, navShip.speed_og, navShip.heading_og);
        }
    });

    /*
    // DRAW TIMER START
    timer.setInterval(1000);
    timer.setSingleShot(true);

    connect(subscriber, &AISSubscriber::startDrawTimer, this, [this]() {
        // DRAW TIMER START
        qDebug() << "TIMER STARTED!";
        timer.start();
    });

    connect(&timer, &QTimer::timeout, this, [=](){
        drawPerTime();
        canRun = true; // setelah 1 detik boleh dipanggil lagi
    });

    // PUBLISH TIMER START
    timerPublish.setInterval(1000);
    timerPublish.setSingleShot(true);

    connect(&timerPublish, &QTimer::timeout, this, [=](){
        publishPerTime();
        canPublish = true; // setelah 1 detik boleh dipanggil lagi
    });
    */

    // FOR ALL FUNCTION START
    allTimer.setInterval(1000);
    allTimer.setSingleShot(true);

    // DRAW TIMER START
    connect(subscriber, &AISSubscriber::startDrawTimer, this, [this]() {
        qDebug() << "TIMER STARTED!";
        allTimer.start();
    });

    connect(&allTimer, &QTimer::timeout, this, [=](){
        allFunctionPerTime();
        canWork = true;
    });

    //connect(this, &EcWidget::ownshipCache, this, &EcWidget::updateOwnshipCache);

    /*
    // SPEED AVERAGE TIMER
    slidingAvgTimer.setInterval(5000); // update rata-rata tiap 5 detik
    connect(&slidingAvgTimer, &QTimer::timeout, this, [=]() {
        QDateTime cutoff = QDateTime::currentDateTime().addSecs(-10);

        // Buang data yang lebih tua dari 1 menit
        while (!speedBuffer.isEmpty() && speedBuffer.first().first < cutoff) {
            speedBuffer.removeFirst();
        }

        // Hitung rata-rata dari sisa buffer
        if (!speedBuffer.isEmpty()) {
            double sum = 0;
            for (auto &p : speedBuffer) sum += p.second;
            avgSpeed1Min = sum / speedBuffer.size();
            qDebug() << "Sliding avg speed (1 menit):" << avgSpeed1Min;
        } else {
            avgSpeed1Min = 0.0;
        }
    });
    slidingAvgTimer.start();
    */

    connect(subscriber, &AISSubscriber::errorOccurred, this, [](const QString &msg) { qWarning() << "Error:" << msg; });
    connect(subscriber, &AISSubscriber::disconnected, this, []() { qDebug() << "Disconnected from AIS source.";});

    connect(threadAIS, &QThread::finished, subscriber, &QObject::deleteLater);
    threadAIS->start();
}

void EcWidget::startConnectionAgain()
{
    if (!deleteAISCell()) {
        QMessageBox::warning(this, tr("ReadAISLogfile"), tr("Could not remove old AIS overlay cell. Please restart the program."));
        return;
    }
    if (!createAISCell()) {
        QMessageBox::warning(this, tr("ReadAISLogfile"), tr("Could not create AIS overlay cell. Please restart the program."));
        return;
    }

    _aisObj->clearTargetData();
    _aisObj->setAISCell(aisCellId);

    if (subscriber) {
        QString sshIP = SettingsManager::instance().data().moosIp;
        quint16 sshPort = 5000;

        QMetaObject::invokeMethod(subscriber, "connectToHost",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, sshIP),
                                  Q_ARG(quint16, sshPort));
    } else {
        qWarning() << "[EcWidget] Subscriber not initialized.";
    }
}

void EcWidget::stopAISConnection()
{
    /*
    if (subscriber) {
        // Hubungkan ke slot lambda sementara
        connect(subscriber, &AISSubscriber::disconnected, this, [this]() {
            qDebug() << "[EcWidget] Subscriber disconnected, cleaning up";

            if (threadAIS) {
                threadAIS->quit();
                threadAIS->wait();
                threadAIS->deleteLater();
                threadAIS = nullptr;
            }

            // Aman untuk kosongkan subscriber setelah benar-benar disconnect
            // subscriber = nullptr;
        });

        // Minta disconnect secara asinkron
        QMetaObject::invokeMethod(subscriber, "disconnectFromHost", Qt::QueuedConnection);
    } else {
        // Kalau subscriber sudah null, tetap bersihkan thread kalau ada
        if (threadAIS) {
            threadAIS->quit();
            threadAIS->wait();
            threadAIS->deleteLater();
            threadAIS = nullptr;
        }
    }
    */
    subscriber->disconnectFromHost();
}

void EcWidget::stopAllThread()
{
    if (threadAIS && subscriber) {
        // Minta worker berhenti
        emit stopAISConnection();

        // Minta thread keluar event loop
        threadAIS->quit();

        // Tunggu thread benar-benar mati
        threadAIS->wait();

        // Bersihkan
        subscriber->deleteLater();
        threadAIS->deleteLater();

        subscriber = nullptr;
        threadAIS = nullptr;
    }
}


void EcWidget::processData(double lat, double lon, double cog, double sog, double hdg, double spd, double dep, double yaw, double z){
    QString nmea = AIVDOEncoder::encodeAIVDO1(lat, lon, cog, sog/10, hdg, 0, 1);

    _aisObj->readAISVariableThread({nmea});

    IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");
    if (dvr && dvr->isRecording() && !nmea.isEmpty()) {
        dvr->recordRawNmea(nmea);
    }

    // PUBLISH NAV INFO
    //publishNavInfo(lat, lon);
    //publishPerTime();

    // INSERT TO DATABASE
    // PLEASE WAIT!!
    // AisDatabaseManager::instance().insertOwnShipToDB(lat, lon, dep, hdg, cog, spd, sog, yaw, z);

    // EKOR OWNSHIP
    //ownShipTrailPoints.append(qMakePair(EcCoordinate(lat), EcCoordinate(lon)));
}

void EcWidget::publishNavInfo(double lat, double lon){

    QList<EcFeature> pickedFeatureList;
    GetPickedFeaturesSubs(pickedFeatureList, lat, lon);

    PickWindow *pickWindow = new PickWindow(this, dictInfo, denc);
    QJsonObject navInfo;

    navInfo = pickWindow->fillJsonSubs(pickedFeatureList);

    navInfo.insert("latitude", lat);
    navInfo.insert("longitude", lon);

    QJsonObject jsonDataOut {
        {"NAV_INFO", navInfo}
    };
    QJsonDocument jsonDocOut(jsonDataOut);
    //QByteArray sendData = jsonDocOut.toJson();

    QString strJson(jsonDocOut.toJson(QJsonDocument::Compact));
    QByteArray sendData = strJson.toUtf8();

    //qDebug().noquote() << "[INFO] Sending Data: \n" << jsonDocOut.toJson(QJsonDocument::Indented);

    // Kirim data ke server Ubuntu (port 5001)
    QTcpSocket* sendSocket = new QTcpSocket();
    sendSocket->connectToHost(SettingsManager::instance().data().moosIp, 5001);
    if (sendSocket->waitForConnected(1000)) {
        sendSocket->write(sendData);
        sendSocket->waitForBytesWritten(1000);
        sendSocket->disconnectFromHost();
    }
    else {
        qCritical() << "Could not connect to data server.";
    }

    sendSocket->deleteLater();
    delete pickWindow;
}

void EcWidget::publishPerTime(){
    if (!canPublish) { return;}
    if (subscriber){
        if (!subscriber->hasData()){
            qWarning() << "Automatic Publish Stopped";
            return;
        }
    }
    else {
        qCritical() << "Automatic Publish Stopped";
        return;
    }

    qWarning() << "Automatic Publish (per second)";
    publishNavInfo(navShip.lat, navShip.lon);

    canPublish = false;
    timerPublish.start(); // mulai countdown 1 detik
}

void EcWidget::drawPerTime(){
    if (!canRun) { return;}
    if (subscriber){
        if (!subscriber->hasData()){
            qWarning() << "Automatic Drawing Stopped";
            return;
        }
    }
    else {
        qCritical() << "Automatic Drawing Stopped";
        return;
    }

    qWarning() << "Automatic Drawing (per second)";
    draw(true);
    slotUpdateAISTargets(true);

    canRun = false;
    timer.start(); // mulai countdown 1 detik

}

void EcWidget::allFunctionPerTime(){
    if (!canWork) { return;}
    if (subscriber){
        if (subscriber->hasData()){
            // DRAW PER TIME
            //qDebug() << "[DRAW] Autorun...";
            if (!isDragging) {
                draw(true);
                slotUpdateAISTargets(true);
            }

            // PUBLISH PER TIME
            if (navShip.lat != 0 && navShip.lon != 0){
                //qDebug() << "[PUBLISH] Autorun";
                publishNavInfo(navShip.lat, navShip.lon);
            }
            else {
                qWarning() << "[PUBLISH] Stopped: No LAT LON";
            }

            // UPDATE ETA
            emit updateEta();

            // start countdown
            canWork = false;
            allTimer.start();
        }
        else {
            qWarning() << "[PUBLISH] Stopped";
            qWarning() << "[DRAW] Stopped";
            return;
        }
    }
    else {
        qCritical() << "[PUBLISH] Stopped";
        qCritical() << "[DRAW] Stopped";
        return;
    }
}

double EcWidget::getSpeedAverage(){
    return avgSpeed1Min;
}


void EcWidget::processDataQuickFix(double lat, double lon, double cog, double sog, double hdg, double spd, double dep, double yaw, double z){
    QString nmea = AIVDOEncoder::encodeAIVDO1(lat, lon, cog, sog/10, hdg, 0, 1);
    //_aisObj->readAISVariable({nmea});

    // AUV WORKS
    _aisObj->readAISVariableThread({nmea});

    // COMMENT BELOW DOWN FOR AUV WORKS vvv
    // PickWindow *pickWindow = new PickWindow(this, dictInfo, denc);
    // QString html = pickWindow->ownShipAutoFill();
    // delete pickWindow;

    // ownShipText->setHtml(html);
}

void EcWidget::processAis(QString ais)
{
    // pecah kalau ada beberapa kalimat dalam satu baris
    QStringList sentences = ais.split(QRegExp("(?=[!$])"), Qt::SkipEmptyParts);

    for (const QString &sentence : sentences) {
        if (sentence.startsWith("!AIVDM")) {
            IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");

            if (dvr && dvr->isRecording() && !sentence.isEmpty()) {
                dvr->recordRawNmea(sentence);
            }

            _aisObj->readAISVariableString(sentence);
        }
    }
}

void EcWidget::processMapInfoReq(QString req){
    QStringList pairs = req.split(",", Qt::SkipEmptyParts);
    QMap<QString, QString> map;

    for (const QString& pair : pairs) {
        QStringList keyValue = pair.split("=", Qt::SkipEmptyParts);
        if (keyValue.size() == 2) {
            QString key = keyValue[0].trimmed();
            QString value = keyValue[1].trimmed();
            map.insert(key, value);
        }
    }

    // Ambil latitude dan longitude
    double lat = map.value("latitude").toDouble();
    double lon = map.value("longitude").toDouble();

    if ((mapInfo.lat != lat || mapInfo.lon != lon) && (lat != 0.0 && lon != 0.0)){
        mapInfo.lat = lat;
        mapInfo.lon = lon;

        SetCenter(lat, lon);
        SetScale(80000);

        QApplication::setOverrideCursor(Qt::WaitCursor);
        Draw();
        QApplication::restoreOverrideCursor();

        QList<EcFeature> pickedFeatureList;
        GetPickedFeaturesSubs(pickedFeatureList, lat, lon);

        PickWindow *pickWindow = new PickWindow(this, dictInfo, denc);;
        QJsonObject mapInfo;

        mapInfo = pickWindow->fillJsonSubs(pickedFeatureList);

        mapInfo.insert("latitude", lat);
        mapInfo.insert("longitude", lon);

        QJsonObject jsonDataOut {
            {"MAP_INFO", mapInfo}
        };
        QJsonDocument jsonDocOut(jsonDataOut);
        //QByteArray sendData = jsonDocOut.toJson();

        QString strJson(jsonDocOut.toJson(QJsonDocument::Compact));
        QByteArray sendData = strJson.toUtf8();

        // Kirim data ke server Ubuntu (port 5003)
        QTcpSocket* sendSocket = new QTcpSocket();
        sendSocket->connectToHost(SettingsManager::instance().data().moosIp, 5001);
        if (sendSocket->waitForConnected(3000)) {
            sendSocket->write(sendData);
            sendSocket->waitForBytesWritten(3000);
            sendSocket->disconnectFromHost();
        }
        else {
            qCritical() << "Could not connect to data server.";
        }

        sendSocket->deleteLater();
    }
}

void EcWidget::processAISJson(const QByteArray& rawData){
    QByteArray data = rawData;

    data = data.simplified();
    data = data.trimmed();
    data.replace("\r", "");

    if (data.isEmpty()) {
        qWarning() << "Socket kosong, tidak ada data diterima";
        return;
    }

    qDebug() << data;
}

// (Removed duplicate latLonToDegMin definition; consolidated above)

// REAL FUNCTION
void EcWidget::publishToMOOSDB(QString varName, QString data){
    bool success = false;

    QJsonObject jsonDataOut {{varName, data}};
    QJsonDocument jsonDocOut(jsonDataOut);

    QString strJson(jsonDocOut.toJson(QJsonDocument::Compact));
    QByteArray sendData = strJson.toUtf8();

    QTcpSocket* sendSocket = new QTcpSocket();
    sendSocket->connectToHost(SettingsManager::instance().data().moosIp, 5001);
    if (sendSocket->waitForConnected(3000)) {
        sendSocket->write(sendData);
        sendSocket->waitForBytesWritten(3000);
        sendSocket->disconnectFromHost();

        success = true;
    }
    else {
        qCritical() << "Could not connect to data server.";
    }

    sendSocket->deleteLater();


    QString message;
    if (varName == "WAYPT_NEXT"){message = "Waypoint";}
    else if (varName == "WAYPT_NAV"){message = "Route";}
    else if (varName == "AREA_NAV"){message = "Area";}

    if (success && !data.isEmpty() && varName != "OWNSHIP_OOB"){
        QMessageBox::information(this, tr("%1 Published").arg(message),
                                 tr("%1 has been published at %2 variable.").arg(message).arg(varName));
    }
    else if (!success){
        QMessageBox::information(this, tr("%1 is NOT Published").arg(message),
                                 tr("There is something wrong with the MOOSDB connection."));
    }
}

// FOR EMIT PUPROSE
void EcWidget::publishToMOOS(QString varName, QString data){
    if (subscriber && subscriber->hasData()){
        emit subscriber->publishToMOOSDB(varName, data);
    }
}

QString EcWidget::convertJsonData(const QString &jsonString){
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!doc.isObject()) return {};

    QJsonArray waypoints = doc.object().value("waypoints").toArray();
    if (waypoints.isEmpty()) return "pts={}";

    QString result = "pts={";

    for (int i = 0; i < waypoints.size(); ++i) {
        auto wp = waypoints[i].toObject();
        double lat = wp["lat"].toDouble();
        double lon = wp["lon"].toDouble();

        result += QString("%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);
        if (i != waypoints.size() - 1)
            result += ":";
    }

    result += "}";

    return result;
}

void EcWidget::publishRoutesToMOOSDB(const QString data){
    QString publishData = convertJsonData(data);

    publishToMOOS("WAYPT_NAV", publishData);
}

void EcWidget::updateOwnshipCache(bool cache){
    publishToMOOS("OWNSHIP_OOB", cache ? "true" : "false");
}

AISSubscriber* EcWidget::getAisSub() const{
    return subscriber;
}

// Read AIS data from AIS Server and display the AIS targets on the chart display.
//////////////////////////////////////////////////////////////////////////////////
void EcWidget::ReadFromServer( const QString &tcpAddress )
{
    QString strHost = tcpAddress.section(":", 0, 0);
    QString strPort = tcpAddress.section(":", 1, 1);

    if( deleteAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not remove old AIS overlay cell. Please restart the program." ) );
        return;
    }

    if( createAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not create AIS overlay cell. Please restart the program." ) );
        return;
    }

    _aisObj->setAISCell( aisCellId );
    _aisObj->connectToAISServer(strHost, strPort.toInt());
}

// Stop Read an AIS variable.
////////////////////////////////////////////////////////////////////////
void EcWidget::StopReadAISVariable()
{
    int vg = 54030;	// Viewing group for AIS targets
    EcChartSetViewingGroup(view, EC_VG_CLEAR, &vg, 1);

    qDebug() << _aisObj;

    // Release AIS object.
    //     if( _aisObj )
    //     {
    //         _aisObj->stopAnimation();

    //         int iCnt = 0;
    //         while( QCoreApplication::hasPendingEvents() == True && iCnt < 20 )
    //         {
    // #ifdef _WINNT_SOURCE
    //             Sleep( 100 );
    // #endif \
    //             qDebug() << iCnt;
    //             iCnt++;
    //         }

    //         if( QObject::disconnect( _aisObj, 0, this, 0 ) == False )
    //         {
    //             QObject::disconnect( _aisObj, 0, this, 0 );
    //         }


    //         if( deleteAISCell() == false )
    //         {
    //             QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not remove old AIS overlay cell. Please restart the program." ) );
    //             return;
    //         }

    //         _aisObj = NULL;
    //     }

        // qDebug() << _aisObj;
}

// Update AIS targtes and own ship on chart.
////////////////////////////////////////////
void EcWidget::slotUpdateAISTargets( Bool bSymbolize )
{
    bool hasRoutes = !waypointList.isEmpty();
  if(showAIS || hasRoutes)
    drawAISCell();
  qApp->processEvents( QEventLoop::ExcludeSocketNotifiers );

  //draw(true);
}

// Draw AIS overlay cell on chart pixmap.
/////////////////////////////////////////
void EcWidget::drawAISCell()
{
  aisCellId = _aisObj->getAISCell();

  if( aisCellId == EC_NOCELLID )
  {
    return;
  }

  EcChartSymbolizeCell( view, aisCellId );

  // copy the chart pixmap as background for the AIS overlay
  chartAisPixmap = chartPixmap;

  // DRAW OVERLAY ON CHART PIXMAP
#ifdef _WIN32
  HDC overlayDC = CreateCompatibleDC( hdc );
  // hBitmapOverlay = chartAisPixmap.toWinHBITMAP( QPixmap::NoAlpha );
  hBitmapOverlay = QtWin::toHBITMAP(chartAisPixmap, QtWin::HBitmapNoAlpha);
  HBITMAP hBitmapOverlayOld = (HBITMAP)SelectObject( overlayDC, hBitmapOverlay );
  HPALETTE oldPal = SelectPalette( overlayDC, hPalette, TRUE );

  EcDrawNTDrawCells( view, overlayDC, NULL, 1, &aisCellId, 0 );

  BitBlt( hdc, 0, 0, chartAisPixmap.width(), chartAisPixmap.height(), overlayDC, 0, 0, SRCCOPY );

  SelectPalette( overlayDC, oldPal, FALSE );

  hBitmapOverlay = (HBITMAP)SelectObject( overlayDC, hBitmapOverlayOld );

  drawPixmap = QtWin::fromHBITMAP(hBitmapOverlay);

  DeleteObject (hBitmapOverlay);
  DeleteDC( overlayDC );
#else
  EcDrawX11DrawCells( view, drawGC, NULL, 1, &aisCellId, 0 );

  // bit blit the two pix maps

  drawPixmap = QPixmap::fromX11Pixmap(x11pixmap);

#endif

  if (AppConfig::isDevelopment()){
      // Draw red dot tracker overlay
      drawRedDotTracker();
  }

  // ROUTE FIX: Enhanced waypointDraw() to fix route and waypoint label flickering
  // Always ensure routes are drawn even when AIS cell operations occur
  waypointDraw();

  // OWNSHIP DRAW
  ownShipDraw();

  update();

  emit projection();
  emit scale( currentScale );
}

// OWNSHIP DRAW
void EcWidget::ownShipDraw(){
    // DRAWING OWNSHIP CUSTOM
    if (showCustomOwnShip && showAIS) {
        AISTargetData ownShipData = Ais::instance()->getOwnShipVar();

        if (ownShipData.lat != 0.0 && ownShipData.lon != 0.0) {
            int x, y;
            if (LatLonToXy(ownShipData.lat, ownShipData.lon, x, y)) {

                QPainter painter(&drawPixmap);
                painter.setRenderHint(QPainter::Antialiasing, true);

                // ⭐ Pastikan COG dan heading dalam range 0-360
                double cog = ownShipData.cog - GetHeading();
                while (cog < 0) cog += 360;
                while (cog >= 360) cog -= 360;

                double heading = ownShipData.heading - GetHeading();
                while (heading < 0) heading += 360;
                while (heading >= 360) heading -= 360;

                // GAMBAR EKOR OWNSHIP
                if (showOwnShipTrail){
                    drawOwnShipTrail(painter);
                }

                // ⭐ PANGGIL DENGAN PARAMETER BARU: COG, Heading, SOG
                drawOwnShipIcon(painter, x, y, cog, heading, ownShipData.sog);

                // GAMBAR TURNING PREDICTION (menggunakan data navShip untuk ROT)
                // Gunakan heading dan cog absolut (belum dikurangi GetHeading) untuk kalkulasi kernel
                drawTurningPrediction(painter, ownShipData.lat, ownShipData.lon,
                                    ownShipData.heading, ownShipData.cog,
                                    ownShipData.sog, navShip.rot);

                // AOI EXIT PULSE: If ownship attached to an AOI and outside its area, draw pulsing red ring
                if (attachedAoiId >= 0) {
                    const AOI* attached = nullptr;
                    for (const auto& a : aoiList) {
                        if (a.id == attachedAoiId) { attached = &a; break; }
                    }

                    if (attached && attached->vertices.size() >= 3) {
                        // Rebuild screen polygon only when needed (view or AOI changed)
                        bool needRebuild = (attachedAoiScreenCacheForId != attachedAoiId) ||
                                           (attachedAoiScreenCacheViewVersion != viewChangeCounter) ||
                                           (attachedAoiScreenCache.size() != attached->vertices.size());
                        if (needRebuild) {
                            attachedAoiScreenCache.clear();
                            for (const QPointF& ll : attached->vertices) {
                                int vx = 0, vy = 0;
                                if (LatLonToXy(ll.x(), ll.y(), vx, vy)) {
                                    attachedAoiScreenCache << QPoint(vx, vy);
                                }
                            }
                            attachedAoiScreenCacheForId = attachedAoiId;
                            attachedAoiScreenCacheViewVersion = viewChangeCounter;
                            attachedAoiScreenCacheBounds = attachedAoiScreenCache.boundingRect();
                        }

                        if (attachedAoiScreenCache.size() >= 3) {
                            // Throttle containment checks to reduce per-frame cost
                            if (!aoiContainmentTimer.isValid()) aoiContainmentTimer.start();
                            qint64 nowMs = aoiContainmentTimer.elapsed();
                            int dx = x - lastOwnshipScreenForAoiCheck.x();
                            int dy = y - lastOwnshipScreenForAoiCheck.y();
                            bool movedEnough = (dx*dx + dy*dy) > (2*2); // >2 px movement
                            bool timeElapsed = (lastAoiContainmentCheckMs < 0) || (nowMs - lastAoiContainmentCheckMs > 100);

                            if (movedEnough || timeElapsed || needRebuild) {
                                // Quick bounding-box reject before polygon test
                                if (!attachedAoiScreenCacheBounds.adjusted(-2,-2,2,2).contains(x, y)) {
                                    cachedOwnshipOutsideAoi = true;
                                } else {
                                    bool insideNow = attachedAoiScreenCache.containsPoint(QPoint(x, y), Qt::OddEvenFill);
                                    cachedOwnshipOutsideAoi = !insideNow;
                                }
                                lastOwnshipScreenForAoiCheck = QPoint(x, y);
                                lastAoiContainmentCheckMs = nowMs;
                            }

                            if (cachedOwnshipOutsideAoi != cachedOwnshipOutsideAoiCopy){
                                cachedOwnshipOutsideAoiCopy = cachedOwnshipOutsideAoi;
                                publishToMOOS("OWNSHIP_OOB", cachedOwnshipOutsideAoi ? "true" : "false");
                            }

                            if (cachedOwnshipOutsideAoi) {
                                // Draw pulsing red circle at ownship position (similar to waypoint pulse)
                                static QElapsedTimer aoiExitPulseTimer;
                                if (!aoiExitPulseTimer.isValid()) {
                                    aoiExitPulseTimer.start();
                                }
                                qint64 elapsedMs = aoiExitPulseTimer.elapsed();
                                double t = elapsedMs / 1000.0;

                                // Scale pulse size with zoom: ensure larger than ownship when zoomed in
                                double rangeNM_forPulse = GetRange(currentScale);
                                int baseMinRadius;

                                if (rangeNM_forPulse >= 2){
                                    baseMinRadius = 12;
                                }
                                else if (rangeNM_forPulse >= 1){
                                    baseMinRadius = 22;
                                }
                                else {
                                    baseMinRadius = 32;
                                }

                                int pulseRadius = baseMinRadius + (int)(4 * std::sin(t * 2.0 * M_PI / 1.5));
                                int opacity = 170 + (int)(60 * std::sin(t * 2.0 * M_PI / 2.0));

                                // Outer glow
                                // QPen glowPen(QColor(255, 0, 0, opacity / 3));
                                // glowPen.setWidth(3);
                                // painter.setPen(glowPen);
                                // painter.setBrush(Qt::NoBrush);
                                // painter.drawEllipse(QPoint(x, y), pulseRadius + 5, pulseRadius + 5);

                                // // Main red ring + soft fill
                                // QPen ringPen(QColor(255, 0, 0, opacity));
                                // ringPen.setWidth(4);
                                // painter.setPen(ringPen);
                                // QBrush ringBrush(QColor(255, 0, 0, opacity / 6));
                                // painter.setBrush(ringBrush);
                                // painter.drawEllipse(QPoint(x, y), pulseRadius, pulseRadius);

                                // ADJUSTMENT
                                // Outer glow
                                QPen glowPen(QColor(255, 0, 0, opacity / 3));
                                glowPen.setWidth(1);
                                painter.setPen(glowPen);
                                painter.setBrush(Qt::NoBrush);
                                painter.drawEllipse(QPoint(x, y), pulseRadius + 5, pulseRadius + 5);

                                // Main red ring + soft fill
                                QPen ringPen(QColor(255, 0, 0, opacity));
                                ringPen.setWidth(2);
                                painter.setPen(ringPen);
                                QBrush ringBrush(QColor(255, 0, 0, 0 / 6));
                                painter.setBrush(ringBrush);
                                painter.drawEllipse(QPoint(x, y), pulseRadius, pulseRadius);
                            }
                        }
                    }
                }

                painter.end();
            }
        }
    }
}

void EcWidget::setMainWindow(MainWindow *mw) {
    mainWindow = mw;
}

// Create AIS overlay cell in RAM.
//////////////////////////////////
bool EcWidget::createAISCell()
{
  // For permformance reasons it is advised to create the AIS overlay cell in RAM
  // For test purposes a real cell may be created (see below)
#ifdef AISCELL_IN_RAM
  aisCellId = EcCellCreateInRam( NULL, 0 );
  if( aisCellId == EC_NOCELLID )
    return false;

  // assign cell to view
  if( !EcChartAssignCellToView( view, aisCellId ) )
  {
    EcCellUnmap( aisCellId );
    return false;
  }

  // lock cell
  if( !EcChartCellLock( view, aisCellId, True ) )
  {
    EcChartUnAssignCellFromView( view, aisCellId );
    EcCellUnmap( aisCellId );
    return false;
  }

  // set overlay flag
  INT32 usage = EC_OVERLAY;
  if( !EcCellSetHeaderInfo( aisCellId, EC_HDR_INTU, (caddr_t)&usage ) )
  {
    EcChartUnAssignCellFromView( view, aisCellId );
    EcCellUnmap( aisCellId );
    return false;
  }
#else
  aisCellId = EcChartOpenOverlayCell(view, "./AIS.7CB", EC_ACCESSWRITE);
  if (aisCellId == EC_NOCELLID)
    return false;
  EcCellClear(aisCellId);
#endif
  return true;
}

// Delete AIS overlay cell.
////////////////////////////////////
bool EcWidget::deleteAISCell()
{
  if( aisCellId != EC_NOCELLID )
  {
#ifdef AISCELL_IN_RAM
    if( !EcChartUnAssignCellFromView( view, aisCellId ) )
    {
      return false;
    }
    if( !EcCellUnmap( aisCellId ) )
    {
      return false;
    }
#else
    EcChartCloseOverlayCell(view, aisCellId);
#endif
  }

  return true;
}

// Update the chart display with AIS targets.
/////////////////////////////////////////////
void EcWidget::slotRefreshChartDisplay( double lat, double lon, double head )
{
  //qDebug() << "slotRefreshChartDisplay called with lat:" << lat << "lon:" << lon;

    /*
  if(showAIS)
  {
    // check if center position is out of defined frame, requires drawing of chart as well
    double maxDist = GetRange(currentScale) / 60 / 2;

    if((lat != 0 && lon != 0) && (fabs(currentLat - lat) > maxDist || fabs(currentLon - lon) > maxDist))
    {
        if (trackTarget.isEmpty() && trackShip){
            SetCenter( lat, lon );
        }
        draw(true);
    }
    slotUpdateAISTargets( true );
  }
  */

     bool hasRoutes = !waypointList.isEmpty();
    if (showAIS)
    {
        if ((lat != 0 && lon != 0) && trackTarget.isEmpty())
        {
            if (centering == LookAhead) // ⭐ Look-Ahead mode
            {
                double offsetNM = GetRange(currentScale) * 0.5;
                double headingRad = head * M_PI / 180.0;
                double offsetLat = offsetNM * cos(headingRad) / 60.0;
                double offsetLon = offsetNM * sin(headingRad) / (60.0 * cos(lat * M_PI / 180.0));

                double centerLat = lat + offsetLat;
                double centerLon = lon + offsetLon;

                SetCenter(centerLat, centerLon);
            }
            else if (centering == Centered)
            {
                SetCenter(lat, lon);
            }
            else if (centering == AutoRecenter)
            {
                int x, y;
                if (LatLonToXy(lat, lon, x, y))
                {
                    // Ambil ukuran tampilan layar saat ini
                    QRect visibleRect = GetVisibleMapRect();

                    // Tentukan margin minimum (misal 10% dari ukuran layar)
                    int marginX = visibleRect.width() * 0.1;
                    int marginY = visibleRect.height() * 0.1;

                    // Buat area aman (safe area) di tengah layar
                    QRect safeRect(
                        visibleRect.left() + marginX,
                        visibleRect.top() + marginY,
                        visibleRect.width() - 2 * marginX,
                        visibleRect.height() - 2 * marginY
                    );

                    // Jika kapal berada di luar area aman, lakukan recenter
                    if (!safeRect.contains(x, y))
                    {
                        SetCenter(lat, lon);
                    }
                }
            }

            if (orientation == HeadUp){
                SetHeading(head);
                mainWindow->oriEditSetText(head);

                if (mainWindow){
                    mainWindow->setCompassHeading(0);
                    mainWindow->setCompassRotation(head);
                }
            }
            else if (orientation  == CourseUp){
                int course = SettingsManager::instance().data().courseUpHeading;

                SetHeading(course);
                mainWindow->oriEditSetText(course);

                if (mainWindow){
                    mainWindow->setCompassRotation(course);
                    mainWindow->setCompassHeadRot(course);
                }
            }
            else {
                SetHeading(0);
                mainWindow->oriEditSetText(0);

                if (mainWindow){
                    mainWindow->setCompassRotation(0);
                }
            }
        }

        draw(true);
        slotUpdateAISTargets(true);
    }
    else if (hasRoutes){
        draw(true);
        slotUpdateAISTargets(true);
    }

  // ========== TAMBAHAN UNTUK RED DOT TRACKER ==========
  // Update red dot position if attached to ship
  if (redDotAttachedToShip) {
      // PERBAIKAN: Update heading ownship dari data real-time
      double oldHeading = ownShip.heading;
      ownShip.heading = head;

      qDebug() << "Updating red dot position to:" << lat << "," << lon << "heading:" << head;
      updateRedDotPosition(lat, lon);

      // Log jika heading berubah signifikan
      if (abs(head - oldHeading) > 1.0) {
          qDebug() << "[HEADING-CHANGE] Ship heading changed from" << oldHeading << "to" << head;
      }

      update(); // Force widget repaint
  }

  // PERBAIKAN: Hanya buat guardzone jika belum ada DAN user sudah aktifkan redDotAttachedToShip
  // DAN belum ada guardzone fisik yang ter-render
  if (attachedGuardZoneId == -1 && lat != 0 && lon != 0 && !qIsNaN(lat) && !qIsNaN(lon)
      && redDotAttachedToShip && initialized && view && guardZoneManager) {
      // Cek apakah ada guardzone yang attachedToShip dari file yang belum ter-render
      bool hasAttachedFromFile = false;
      int attachedGuardZoneCount = 0;

      for (const GuardZone& gz : guardZones) {
          if (gz.attachedToShip &&
              !gz.name.contains("Ship Guardian Circle") &&
              !gz.name.contains("Red Dot Guardian")) {
              hasAttachedFromFile = true;
              attachedGuardZoneCount++;
          }
      }

      // PERBAIKAN: Hanya buat jika tidak ada guardzone attached yang sudah ada
      if (!hasAttachedFromFile && attachedGuardZoneCount == 0) {
          qDebug() << "[POSITION-UPDATE] Valid ownship position received, creating delayed attached guardzone";
          try {
              createAttachedGuardZone();
              emit attachToShipStateChanged(true);
          } catch (...) {
              qDebug() << "[ATTACH-CREATE] Suppressed exception creating attached guardzone";
          }
      } else {
          qDebug() << "[POSITION-UPDATE] Skipping creation - attached guardzone already exists (" << attachedGuardZoneCount << " found)";
      }
  }
  // ==================================================
}

void EcWidget::slotRefreshChartDisplayThread(double lat, double lon, double head)
{
    // ===== STEP 1: THREAD-SAFE SNAPSHOT =====
    {
        QMutexLocker locker(&aisDataMutex);
        lastSnapshot.lat = lat;
        lastSnapshot.lon = lon;
        lastSnapshot.heading = head;
        lastSnapshot.navShip = navShip; // copy struct
    }

    // ===== STEP 2: THROTTLE UPDATE =====
    if (showAIS){
        if (!aisGuiTimer.isValid() || aisGuiTimer.elapsed() >= 100) // max 10 Hz
        {
            aisGuiTimer.restart();

            AISSnapshot snapshot;
            {
                QMutexLocker locker(&aisDataMutex);
                snapshot = lastSnapshot; // ambil snapshot aman
            }

            // ===== STEP 3: UPDATE GUI =====
            if (!showAIS) return;

            // Pastikan data valid
            if (qIsNaN(snapshot.lat) || qIsNaN(snapshot.lon)) return;

            // Centering
            if (snapshot.lat != 0 && snapshot.lon != 0 && trackTarget.isEmpty())
            {
                if (centering == LookAhead)
                {
                    double offsetNM = GetRange(currentScale) * 0.5;
                    double headingRad = snapshot.heading * M_PI / 180.0;
                    double offsetLat = offsetNM * cos(headingRad) / 60.0;
                    double offsetLon = offsetNM * sin(headingRad) / (60.0 * cos(snapshot.lat * M_PI / 180.0));
                    SetCenter(snapshot.lat + offsetLat, snapshot.lon + offsetLon);
                }
                else if (centering == Centered)
                {
                    SetCenter(snapshot.lat, snapshot.lon);
                }
                else if (centering == AutoRecenter)
                {
                    int x, y;
                    if (LatLonToXy(snapshot.lat, snapshot.lon, x, y))
                    {
                        QRect visibleRect = GetVisibleMapRect();
                        int marginX = visibleRect.width() * 0.1;
                        int marginY = visibleRect.height() * 0.1;
                        QRect safeRect(
                            visibleRect.left() + marginX,
                            visibleRect.top() + marginY,
                            visibleRect.width() - 2 * marginX,
                            visibleRect.height() - 2 * marginY
                        );
                        if (!safeRect.contains(x, y))
                            SetCenter(snapshot.lat, snapshot.lon);
                    }
                }
            }

            // Heading
            if (orientation == HeadUp)
            {
                SetHeading(snapshot.heading);
                if (mainWindow)
                {
                    mainWindow->oriEditSetText(snapshot.heading);
                    mainWindow->setCompassHeading(0);
                    mainWindow->setCompassRotation(snapshot.heading);
                }
            }
            else if (orientation == CourseUp)
            {
                int course = SettingsManager::instance().data().courseUpHeading;
                SetHeading(course);

                if (mainWindow)
                {
                    mainWindow->oriEditSetText(course);
                    mainWindow->setCompassHeading(snapshot.heading - course);
                    mainWindow->setCompassRotation(course);
                }
            }
            else
            {
                SetHeading(0);
                if (mainWindow)
                {
                    mainWindow->oriEditSetText(0);
                    mainWindow->setCompassRotation(0);
                }
            }

            // Draw chart dan update AIS targets
            //draw(true);
            //drawPerTime();
            //slotUpdateAISTargets(true);
        }
    }
}

void EcWidget::slotRefreshCenter( double lat, double lon )
{
  if (showAIS)
  {
    // check if center position is out of defined frame, requires drawing of chart as well
    // double maxDist = GetRange(currentScale) / 60 / 2;

    // if((lat != 0 && lon != 0) && (fabs(currentLat - lat) > maxDist || fabs(currentLon - lon) > maxDist))
    // {

    if(lat != 0 && lon != 0)
    {
        if (trackShip && !trackTarget.isEmpty()){
            SetCenter( lat, lon );
        }
    }

    // draw(true);
    // slotUpdateAISTargets( true );
  }
}

QRect EcWidget::GetVisibleMapRect(){
    return QRect(0, 0, width(), height());
}
//////////////////////////////////////////////////////////////   END AIS   /////////////////////////////////////////////////////


// Draw Waypoint overlay cell on chart pixmap.
/////////////////////////////////////////
void EcWidget::drawWaypointCell()
{
    if (udoCid == EC_NOCELLID)
    {
        qCritical() << "No UDO cell to draw.";
        return;
    }

    // Symbolize UDO cell (VERY IMPORTANT)
    if (!EcChartSymbolizeCell(view, udoCid))
    {
        qCritical() << "Cannot symbolize UDO cell.";
        return;
    }

    // Force repaint
    Draw();
}



// Create Waypoint overlay cell in RAM.
//////////////////////////////////

// Create Waypoint overlay cell in RAM.
//////////////////////////////////
bool EcWidget::createWaypointCell()
{
    qDebug() << "[DEBUG] createWaypointCell() - Clean and Init Cell";

    EcCellCreate("ROUTES.7CB", 4096);
    udoCid = EcCellMap("ROUTES.7CB", EC_ACCESSWRITE, 0);
    if (udoCid == EC_NOCELLID)
    {
        qDebug() << "[ERROR] Cannot map ROUTES.7CB";
        return false;
    }

    if (!EcCellClear(udoCid))
    {
        qDebug() << "[ERROR] Cannot clear cell.";
        EcCellUnmap(udoCid);
        return false;
    }

    if (!EcChartAssignCellToView(view, udoCid))
    {
        EcCellUnmap(udoCid);
        qDebug() << "[ERROR] Cannot assign cell to view.";
        return false;
    }

    if (!EcChartCellLock(view, udoCid, True))
    {
        EcChartUnAssignCellFromView(view, udoCid);
        EcCellUnmap(udoCid);
        qDebug() << "[ERROR] Cannot lock cell.";
        return false;
    }

    INT32 usage = EC_OVERLAY;
    if (!EcCellSetHeaderInfo(udoCid, EC_HDR_INTU, (caddr_t)&usage))
    {
        EcChartUnAssignCellFromView(view, udoCid);
        EcCellUnmap(udoCid);
        qDebug() << "[ERROR] Cannot set header.";
        return false;
    }

    qDebug() << "[DEBUG] Waypoint Cell successfully initialized.";
    return true;
}


void EcWidget::drawOverlayCell()
{
    if (udoCid == EC_NOCELLID)
    {
        qDebug() << "[ERROR] No UDO cell to draw.";
        return;
    }

    if (!EcChartSymbolizeCell(view, udoCid))
    {
        qDebug() << "[ERROR] Cannot symbolize UDO cell.";
        return;
    }
    else
    {
        qDebug() << "[DEBUG] Symbolize success.";
    }

#ifdef _WIN32
    qDebug() << "[DEBUG] Trying to draw overlay (NTDrawCells)...";

    HDC overlayDC = CreateCompatibleDC(hdc);

    hBitmapOverlay = QtWin::toHBITMAP(chartPixmap, QtWin::HBitmapNoAlpha);
    HBITMAP hBitmapOverlayOld = (HBITMAP)SelectObject(overlayDC, hBitmapOverlay);

    HPALETTE oldPal = SelectPalette(overlayDC, hPalette, TRUE);

    if (!EcDrawNTDrawCells(view, overlayDC, NULL, 1, &udoCid, 0))
    {
        qDebug() << "[ERROR] Failed to draw UDO cell (NTDrawCells)";
    }
    else
    {
        qDebug() << "[DEBUG] EcDrawNTDrawCells success!";
    }

    BitBlt(hdc, 0, 0, chartPixmap.width(), chartPixmap.height(), overlayDC, 0, 0, SRCCOPY);
    qDebug() << "[DEBUG] BitBlt executed.";

    SelectPalette(overlayDC, oldPal, FALSE);

    hBitmapOverlay = (HBITMAP)SelectObject(overlayDC, hBitmapOverlayOld);

    drawPixmap = QtWin::fromHBITMAP(hBitmapOverlay);

    DeleteObject(hBitmapOverlay);
    DeleteDC(overlayDC);
#else
    qDebug() << "[DEBUG] Trying to draw overlay (X11DrawCells)...";

    EcDrawX11DrawCells(view, drawGC, NULL, 1, &udoCid, 0);
    drawPixmap = QPixmap::fromX11Pixmap(x11pixmap);

    qDebug() << "[DEBUG] X11DrawCells success.";
#endif

    update();
    qDebug() << "[DEBUG] Update() called after overlay.";
}

// waypoint
void EcWidget::buttonInit(){
    // ICON-ICON
    editAction = new QAction(tr("Edit Route Point"), this);
    moveAction = new QAction(tr("Move Waypoint"), this);
    deleteWaypointAction = new QAction(tr("Delete Waypoint"), this);
    deleteRouteAction = new QAction(tr("Delete Route"), this);
    publishAction = new QAction(tr("Publish Waypoint"), this);
    insertWaypointAction = new QAction(tr("Insert Waypoint"), this);
    createRouteAction = new QAction(tr("Create Route"), this);
    pickInfoAction = new QAction(tr("Map Information"), this);
    warningInfoAction = new QAction(tr("Caution and Restricted Info"), this);
    measureEblVrmAction = new QAction(tr("Measure Here"), this);

    btn1 = new QToolButton();
    btn2 = new QToolButton();
    btn3 = new QToolButton();
    btn4 = new QToolButton();
    btn  = new QToolButton();

    btnCreate = new QToolButton();
    btnDelete = new QToolButton();
    btnMove = new QToolButton();

    // ==============================================================
    // Buat dialog toolbox
    toolbox = new QDialog(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    toolbox->setAttribute(Qt::WA_TranslucentBackground); // transparan hanya utk dialog luar
    toolbox->setWindowTitle("Toolbox");

    // Frame dalam utk kotak rounded
    frame = new QFrame(toolbox);
    frame->setObjectName("toolboxFrame");

    QHBoxLayout *outerLayout = new QHBoxLayout(toolbox);
    outerLayout->setContentsMargins(0, 0, 0, 0);  // biar frame nempel ke dialog
    outerLayout->addWidget(frame);

    // Main layout vertical untuk label + buttons
    QVBoxLayout *mainLayout = new QVBoxLayout(frame);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    // Info label (hidden by default)
    toolboxInfoLabel = new QLabel();
    toolboxInfoLabel->setStyleSheet("QLabel { color: #FF6B6B; font-size: 10px; padding: 2px; }");
    toolboxInfoLabel->setAlignment(Qt::AlignCenter);
    toolboxInfoLabel->setWordWrap(true);
    toolboxInfoLabel->hide();
    mainLayout->addWidget(toolboxInfoLabel);

    // Layout isi tombol
    QHBoxLayout *layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Tambahkan tombol2
    btn1->setIconSize(QSize(20, 20));
    btn1->setToolTip("Edit Waypoint");
    btn1->setAutoRaise(true);
    btn1->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btn1->setStyleSheet("QToolButton { margin: 1px; padding: 1px; border: none; }");
    layout->addWidget(btn1);

    btn2->setIconSize(QSize(20, 20));
    btn2->setToolTip("Move Waypoint");
    btn2->setAutoRaise(true);
    btn2->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btn2->setStyleSheet("QToolButton { margin: 1px; padding: 1px; border: none; }");
    layout->addWidget(btn2);

    btn3->setIconSize(QSize(20, 20));
    btn3->setToolTip("Publish Waypoint");
    btn3->setAutoRaise(true);
    btn3->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btn3->setStyleSheet("QToolButton { margin: 1px; padding: 1px; border: none; }");
    layout->addWidget(btn3);

    btn4->setIconSize(QSize(20, 20));
    btn4->setToolTip("Delete Waypoint");
    btn4->setAutoRaise(true);
    btn4->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btn4->setStyleSheet("QToolButton { margin: 1px; padding: 1px; border: none; }");
    layout->addWidget(btn4);

    mainLayout->addLayout(layout);
    toolbox->setAttribute(Qt::WA_AlwaysShowToolTips, true);
    // ==============================================================

    // ==============================================================
    // Buat dialog toolbox
    toolboxLL = new QDialog(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    toolboxLL->setAttribute(Qt::WA_TranslucentBackground); // transparan hanya utk dialog luar
    toolboxLL->setWindowTitle("Toolbox");

    // Frame dalam utk kotak rounded
    frameLL = new QFrame(toolboxLL);
    frameLL->setObjectName("toolboxFrameLL");

    QHBoxLayout *outerLayoutLL = new QHBoxLayout(toolboxLL);
    outerLayoutLL->setContentsMargins(0, 0, 0, 0);  // biar frame nempel ke dialog
    outerLayoutLL->addWidget(frameLL);

    // Main layout vertical untuk label + buttons
    QVBoxLayout *mainLayoutLL = new QVBoxLayout(frameLL);
    mainLayoutLL->setContentsMargins(2, 2, 2, 2);
    mainLayoutLL->setSpacing(2);

    // Info label (hidden by default)
    toolboxLLInfoLabel = new QLabel();
    toolboxLLInfoLabel->setStyleSheet("QLabel { color: #FF6B6B; font-size: 10px; padding: 2px; }");
    toolboxLLInfoLabel->setAlignment(Qt::AlignCenter);
    toolboxLLInfoLabel->setWordWrap(true);
    toolboxLLInfoLabel->hide();
    mainLayoutLL->addWidget(toolboxLLInfoLabel);

    // Layout isi tombol
    QHBoxLayout *layoutLL = new QHBoxLayout();
    layoutLL->setContentsMargins(0, 0, 0, 0);
    layoutLL->setSpacing(0);

    // Tambahkan tombol2
    btn->setIconSize(QSize(20, 20));
    btn->setToolTip("Add Waypoint");
    btn->setAutoRaise(true);
    btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btn->setStyleSheet("QToolButton { margin: 1px; padding: 1px; border: none; }");
    layoutLL->addWidget(btn);
    mainLayoutLL->addLayout(layoutLL);
    toolboxLL->setAttribute(Qt::WA_AlwaysShowToolTips, true);
    // ==============================================================


    // ==============================================================
    // Buat dialog toolbox
    toolboxAoi = new QDialog(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    toolboxAoi->setAttribute(Qt::WA_TranslucentBackground); // transparan hanya utk dialog luar
    toolboxAoi->setWindowTitle("Toolbox");

    // Frame dalam utk kotak rounded
    frameAoi = new QFrame(toolboxAoi);
    frameAoi->setObjectName("toolboxFrameAoi");

    QHBoxLayout *outerLayoutAoi = new QHBoxLayout(toolboxAoi);
    outerLayoutAoi->setContentsMargins(0, 0, 0, 0);  // biar frame nempel ke dialog
    outerLayoutAoi->addWidget(frameAoi);

    // Main layout vertical untuk label + buttons
    QVBoxLayout *mainLayoutAoi = new QVBoxLayout(frameAoi);
    mainLayoutAoi->setContentsMargins(2, 2, 2, 2);
    mainLayoutAoi->setSpacing(2);

    // Info label (hidden by default)
    toolboxAoiInfoLabel = new QLabel();
    toolboxAoiInfoLabel->setStyleSheet("QLabel { color: #FF6B6B; font-size: 10px; padding: 2px; }");
    toolboxAoiInfoLabel->setAlignment(Qt::AlignCenter);
    toolboxAoiInfoLabel->setWordWrap(true);
    toolboxAoiInfoLabel->hide();
    mainLayoutAoi->addWidget(toolboxAoiInfoLabel);

    // Layout isi tombol
    QHBoxLayout *layoutAoi = new QHBoxLayout();
    layoutAoi->setContentsMargins(0, 0, 0, 0);
    layoutAoi->setSpacing(0);

    btnMove->setIconSize(QSize(20, 20));
    btnMove->setToolTip("Move Point");
    btnMove->setAutoRaise(true);
    btnMove->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btnMove->setStyleSheet("QToolButton { margin: 1px; padding: 1px; border: none; }");
    layoutAoi->addWidget(btnMove);

    btnDelete->setIconSize(QSize(20, 20));
    btnDelete->setToolTip("Delete Point");
    btnDelete->setAutoRaise(true);
    btnDelete->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btnDelete->setStyleSheet("QToolButton { margin: 1px; padding: 1px; border: none; }");
    layoutAoi->addWidget(btnDelete);

    mainLayoutAoi->addLayout(layoutAoi);
    toolboxAoi->setAttribute(Qt::WA_AlwaysShowToolTips, true);
    // ==============================================================

    // ==============================================================
    // Buat dialog toolbox
    toolboxAoiCreate = new QDialog(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    toolboxAoiCreate->setAttribute(Qt::WA_TranslucentBackground); // transparan hanya utk dialog luar
    toolboxAoiCreate->setWindowTitle("Toolbox");

    // Frame dalam utk kotak rounded
    frameAoiCreate = new QFrame(toolboxAoiCreate);
    frameAoiCreate->setObjectName("toolboxFrameAoiCreate");

    QHBoxLayout *outerLayoutAoiCreate = new QHBoxLayout(toolboxAoiCreate);
    outerLayoutAoiCreate->setContentsMargins(0, 0, 0, 0);  // biar frame nempel ke dialog
    outerLayoutAoiCreate->addWidget(frameAoiCreate);

    // Main layout vertical untuk label + buttons
    QVBoxLayout *mainLayoutAoiCreate = new QVBoxLayout(frameAoiCreate);
    mainLayoutAoiCreate->setContentsMargins(2, 2, 2, 2);
    mainLayoutAoiCreate->setSpacing(2);

    // Info label (hidden by default)
    toolboxAoiCreateInfoLabel = new QLabel();
    toolboxAoiCreateInfoLabel->setStyleSheet("QLabel { color: #FF6B6B; font-size: 10px; padding: 2px; }");
    toolboxAoiCreateInfoLabel->setAlignment(Qt::AlignCenter);
    toolboxAoiCreateInfoLabel->setWordWrap(true);
    toolboxAoiCreateInfoLabel->hide();
    mainLayoutAoiCreate->addWidget(toolboxAoiCreateInfoLabel);

    // Layout isi tombol
    QHBoxLayout *layoutAoiCreate = new QHBoxLayout();
    layoutAoiCreate->setContentsMargins(0, 0, 0, 0);
    layoutAoiCreate->setSpacing(0);

    btnCreate->setIconSize(QSize(20, 20));
    btnCreate->setToolTip("Add Point");
    btnCreate->setAutoRaise(true);
    btnCreate->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btnCreate->setStyleSheet("QToolButton { margin: 1px; padding: 1px; border: none; }");
    layoutAoiCreate->addWidget(btnCreate);

    mainLayoutAoiCreate->addLayout(layoutAoiCreate);
    toolboxAoiCreate->setAttribute(Qt::WA_AlwaysShowToolTips, true);
    // ==============================================================



    connect(btn1, &QToolButton::clicked, this, [this](){
        toolbox->close();
        editWaypointAt(lastClick.x(), lastClick.y());
    });

    connect(btn2, &QToolButton::clicked, this, [this](){
        toolbox->close();
        // Switch to move waypoint mode
        setActiveFunction(MOVE_WAYP);
        moveSelectedIndex = lastWaypointIndex;

        // Setup ghost waypoint for immediate preview
        ghostWaypoint.visible = true;
        ghostWaypoint.lat = lastWaypoint.lat;
        ghostWaypoint.lon = lastWaypoint.lon;
        ghostWaypoint.label = lastWaypoint.label;
        ghostWaypoint.routeId = lastWaypoint.routeId;
        ghostWaypoint.waypointIndex = lastWaypointIndex;
    });
    connect(btn3, &QToolButton::clicked, this, [this](){
        toolbox->close();
        double lat, lon;
        XyToLatLon(lastClick.x(), lastClick.y(), lat, lon);

        QString result = QString("%1, %2")
                            .arg(lat, 0, 'f', 6) // 6 angka di belakang koma
                            .arg(lon, 0, 'f', 6);

        if (subscriber){
            publishToMOOS("WAYPT_NEXT", result);
        }
    });
    connect(btn4, &QToolButton::clicked, this, [this](){
        toolbox->close();
        // Delete individual waypoint

        // Confirm deletion
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("Confirm Delete Waypoint"),
            tr("Are you sure you want to delete this waypoint?\n\nThis action cannot be undone."),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            removeWaypointAt(lastClick.x(), lastClick.y());
        }
    });

    connect(btn, &QToolButton::clicked, this, [this](){
        toolboxLL->close();

        // Convert click position to lat/lon
        EcCoordinate lat, lon;
        if (XyToLatLon(lastClick.x(), lastClick.y(), lat, lon)) {
            // Directly insert at computed position without switching modes
            insertWaypointAt(lat, lon);
            // Ensure we stay/return to PAN mode so next click does not insert again
            setActiveFunction(PAN);
        }
    });

    connect(btnMove, &QToolButton::clicked, this, [this](){
        toolboxAoi->close();

        editingAOI = true;
        editingAoiId = lastAoiId;
        draggedAoiVertex = lastVertexIndex;

        update();
    });

    connect(btnDelete, &QToolButton::clicked, this, [this](){
        toolboxAoi->close();

        if (lastAoiList->vertices.size() > 3) {

            // Confirm deletion
            QMessageBox::StandardButton reply = QMessageBox::question(this,
                tr("Confirm Delete Point"),
                tr("Are you sure you want to delete this point?\n\nThis action cannot be undone."),
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                lastAoiList->vertices.remove(lastVertexIndex);

                emit aoiListChanged();
                update();
            }

        }
        else {
            QMessageBox::information(this, "Error", tr("Cannot remove vertex: polygon must have at least 3 vertices"));
        }
    });

    connect(btnCreate, &QToolButton::clicked, this, [this](){
        toolboxAoiCreate->close();

        lastAoiList->vertices.insert(lastBestSeg + 1, QPointF(lastLat, lastLon));
        emit aoiListChanged();
        saveAOIs();
        update();
    });
}

void EcWidget::createWaypointAt(EcCoordinate lat, EcCoordinate lon)
{
    qDebug() << "[DEBUG] Creating waypoint at" << lat << lon;

    // Create new waypoint struct
    Waypoint newWaypoint;
    newWaypoint.lat = lat;
    newWaypoint.lon = lon;

    // Generate label based on mode
    if (isRouteMode) {
        newWaypoint.label = QString("R%1-WP%2").arg(currentRouteId).arg(routeWaypointCounter, 3, 10, QChar('0'));
        newWaypoint.remark = QString("Route %1 waypoint %2").arg(currentRouteId).arg(routeWaypointCounter);
        newWaypoint.routeId = currentRouteId;

        // IMPORTANT: Create separate waypoint for each route to prevent connection
        createSeparateRouteWaypoint(newWaypoint);
    } else {
        newWaypoint.label = QString("WP%1").arg(waypointList.size() + 1, 3, 10, QChar('0'));
        newWaypoint.remark = "Single waypoint";
        newWaypoint.routeId = 0; // Single waypoint tidak punya route ID

        // Create single waypoint normally
        createSingleWaypoint(newWaypoint);
    }

    qDebug() << "[INFO] Waypoint created successfully. Total waypoints:" << waypointList.size();

    // After creating waypoint, show hazard info (caution/danger) at this position
    // Show for both route and single waypoint creation

    // if (AppConfig::isProduction()){
    //     showHazardInfoAt(lat, lon);
    // }

    if (AppConfig::isDevelopment()){
        QList<EcFeature> pickedFeatureList;
        GetPickedFeaturesSubs(pickedFeatureList, lat, lon);

        mainWindow->pickWindow->fillWarningOnly(pickedFeatureList, lat, lon);
        mainWindow->pickWindow->show();
    }
}

bool EcWidget::createWaypointInRoute(int routeId, double lat, double lon, const QString& label)
{
    qDebug() << "[DEBUG] Creating waypoint in route" << routeId << "at" << lat << lon;

    // Create new waypoint struct
    Waypoint newWaypoint;
    newWaypoint.lat = lat;
    newWaypoint.lon = lon;
    newWaypoint.routeId = routeId;

    // Set label - use provided label or generate one
    if (label.isEmpty()) {
        // Count existing waypoints in this route to generate label
        int waypointCount = 0;
        for (const auto& wp : waypointList) {
            if (wp.routeId == routeId) {
                waypointCount++;
            }
        }
        newWaypoint.label = QString("R%1-WP%2").arg(routeId).arg(waypointCount + 1, 3, 10, QChar('0'));
    } else {
        newWaypoint.label = label;
    }

    newWaypoint.remark = QString("Route %1 waypoint").arg(routeId);
    newWaypoint.featureHandle.id = EC_NOCELLID;
    newWaypoint.featureHandle.offset = 0;

    // Add to waypoint list
    waypointList.append(newWaypoint);

    qDebug() << "[CREATE-WAYPOINT] Added waypoint" << newWaypoint.label << "to route" << routeId << ". Total waypoints:" << waypointList.size();

    // Note: Don't save or redraw here - will be done in batch after all waypoints are created

    return true;
}

QColor EcWidget::getBaseRouteColor(int routeId) const
{
    if (routeId == 0) {
        return QColor(255, 140, 0); // Single waypoint orange
    }
    if (routeCustomColors.contains(routeId)) {
        return routeCustomColors.value(routeId);
    }
    // Default for new routes: blue
    return QColor(0, 100, 255);
}

QColor EcWidget::getRouteColor(int routeId)
{
    // Single waypoints keep base color
    if (routeId == 0) return getBaseRouteColor(routeId);

    QColor base = getBaseRouteColor(routeId);

    // If no route attached, use base color
    if (!hasAttachedRoute()) return base;

    // If attached, show base; others dim to gray
    if (isRouteAttachedToShip(routeId)) return base;
    return QColor(128, 128, 128);
}

void EcWidget::setRouteCustomColor(int routeId, const QColor& color)
{
    if (routeId <= 0) return;
    routeCustomColors[routeId] = color;
    // Persist immediately so changes survive app restarts
    saveRoutes();
    Draw();
}

bool EcWidget::deleteRoute(int routeId)
{
    if (routeSafetyFeature && routeSafeMode) {
        routeSafetyFeature->invalidateRoute(routeId);
    }

    qDebug() << "[DEBUG] Deleting route" << routeId;

    if (routeId <= 0) {
        qDebug() << "[ERROR] Invalid routeId:" << routeId;
        return false;
    }

    // Remove all waypoints with this routeId
    QList<Waypoint> newWaypointList;
    int deletedCount = 0;

    for (const auto& wp : waypointList) {
        if (wp.routeId != routeId) {
            newWaypointList.append(wp);
        } else {
            deletedCount++;
        }
    }

    if (deletedCount == 0) {
        qDebug() << "[WARNING] No waypoints found for route" << routeId;
        return false;
    }

    // Update waypoint list
    waypointList = newWaypointList;

    // Remove route from routeList
    QList<Route> newRouteList;
    for (const auto& route : routeList) {
        if (route.routeId != routeId) {
            newRouteList.append(route);
        }
    }
    routeList = newRouteList;

    // Cleanup ancillary state
    routeVisibility.remove(routeId);
    routeCustomColors.remove(routeId);
    if (selectedRouteId == routeId) {
        selectedRouteId = -1;
        clearWaypointHighlight();
    }
    if (isRouteAttachedToShip(routeId)) {
        setRouteAttachedToShip(routeId, false);
    }

    //qDebug() << "[ROUTE] Deleted route" << routeId << "with" << deletedCount << "waypoints";

    // Save changes
    saveWaypoints();
    saveRoutes();

    // Force complete chart redraw - use Draw() to ensure AIS and waypoints are redrawn
    Draw();

    // Notify UI (RoutePanel refresh via MainWindow connection)
    emit waypointCreated();

    return true;
}

void EcWidget::setRouteVisibility(int routeId, bool visible)
{
    //qDebug() << "[ROUTE] *** setRouteVisibility called for route" << routeId << "to" << visible;
    //qDebug() << "[ROUTE] *** selectedRouteId:" << selectedRouteId;
    //qDebug() << "[ROUTE] *** Previous visibility for route" << routeId << "was:" << routeVisibility.value(routeId, true);

    routeVisibility[routeId] = visible;
    //qDebug() << "[ROUTE] *** Set route" << routeId << "visibility to" << visible;
    Draw(); // Use Draw() instead of forceRedraw() to ensure AIS and waypoints are redrawn
}

bool EcWidget::isRouteVisible(int routeId) const
{
    //qDebug() << "[ROUTE] *** isRouteVisible called for route" << routeId;
    //qDebug() << "[ROUTE] *** routeVisibility map contains" << routeVisibility.size() << "entries";
    //qDebug() << "[ROUTE] *** selectedRouteId:" << selectedRouteId;

    // Check if routeId exists in the map first
    if (!routeVisibility.contains(routeId)) {
        //qDebug() << "[ROUTE] *** Route" << routeId << "NOT FOUND in routeVisibility map";
        // Route not in map - check if route actually exists in routeList
        bool routeExists = false;
        for (const auto& route : routeList) {
            if (route.routeId == routeId) {
                routeExists = true;
                break;
            }
        }

        if (routeExists) {
            // Route exists but not in visibility map - initialize to visible
            const_cast<QMap<int, bool>&>(routeVisibility)[routeId] = true;
            //qDebug() << "[ROUTE] *** Route" << routeId << "exists but not in visibility map, initialized to visible";
            return true;
        } else {
            // Route doesn't exist at all - return false
            //qDebug() << "[ROUTE] *** Route" << routeId << "does not exist, returning false";
            return false;
        }
    }

    // Route exists in visibility map - return saved state
    bool visible = routeVisibility[routeId];
    //qDebug() << "[ROUTE] *** Route" << routeId << "FOUND in routeVisibility map, returns:" << visible;

    // Print entire routeVisibility map for debugging
    //qDebug() << "[ROUTE] *** Current routeVisibility map contents:";
    for (auto it = routeVisibility.begin(); it != routeVisibility.end(); ++it) {
        //qDebug() << "[ROUTE] ***   Route" << it.key() << "=" << it.value();
    }

    return visible;
}

void EcWidget::setRouteAttachedToShip(int routeId, bool attached)
{
    // Find and update the route in routeList
    for (auto& route : routeList) {
        if (route.routeId == routeId) {
            route.attachedToShip = attached;
            //qDebug() << "[ROUTE] Set route" << routeId << "attached to ship:" << attached;

            // Don't call saveRoutes or forceRedraw here to avoid recursive calls
            // These will be handled by the calling function
            return;
        }
    }
    //qDebug() << "[ROUTE] Warning: Route" << routeId << "not found when setting attached state";
}

bool EcWidget::isRouteAttachedToShip(int routeId) const
{
    for (const auto& route : routeList) {
        if (route.routeId == routeId) {
            return route.attachedToShip;
        }
    }
    return false; // Default not attached for non-existent routes
}

void EcWidget::attachRouteToShip(int routeId)
{
    //qDebug() << "[ROUTE] attachRouteToShip called for route" << routeId;

    // Preserve current visibility of the route being attached
    bool wasVisible = true;
    if (routeId > 0) {
        wasVisible = isRouteVisible(routeId);
        //qDebug() << "[ROUTE] Current visibility of route" << routeId << ":" << wasVisible;

        // FORCE SET visibility to preserve it BEFORE save
        routeVisibility[routeId] = wasVisible;
        //qDebug() << "[ROUTE] PRESERVED visibility in routeVisibility before attachment";
    }

    // Detach all routes first
    for (auto& route : routeList) {
        route.attachedToShip = false;
    }

    // Attach the selected route
    if (routeId > 0) {
        // Set attachment status directly without calling setRouteAttachedToShip
        for (auto& route : routeList) {
            if (route.routeId == routeId) {
                route.attachedToShip = true;

                // Initialize activeWaypointIndex to SECOND ACTIVE waypoint (or first if only one)
                // We need at least 2 waypoints to create a leg for deviation checking
                route.activeWaypointIndex = -1;
                int firstActiveIdx = -1;

                // Find first and second active waypoints
                for (int i = 0; i < route.waypoints.size(); i++) {
                    if (route.waypoints[i].active) {
                        if (firstActiveIdx < 0) {
                            firstActiveIdx = i;
                        } else {
                            // Found second active waypoint - this is our target
                            route.activeWaypointIndex = i;
                            qDebug() << "[ROUTE] Active waypoint set to index:" << i
                                     << "(leg from WP" << firstActiveIdx << "to WP" << i << ")";
                            break;
                        }
                    }
                }

                // If only one active waypoint, use it (but deviation won't work)
                if (route.activeWaypointIndex < 0 && firstActiveIdx >= 0) {
                    route.activeWaypointIndex = firstActiveIdx;
                    qDebug() << "[ROUTE] WARNING: Only one active waypoint found at index" << firstActiveIdx;
                } else if (route.activeWaypointIndex < 0) {
                    route.activeWaypointIndex = 0;
                    qDebug() << "[ROUTE] WARNING: No active waypoint found!";
                }

                break;
            }
        }

        //qDebug() << "[ROUTE] Attached route" << routeId << "to ship";
    } else {
        //qDebug() << "[ROUTE] Detached all routes from ship";
    }

    // Save the attachment state - visibility should be preserved now
    saveRoutes();
    //qDebug() << "[ROUTE] Saved routes with preserved visibility";

    // Refresh chart to update colors - use Draw() to ensure AIS and waypoints are redrawn
    Draw(); // Changed from forceRedraw() to Draw() to include drawAISCell() and waypointDraw()
}

int EcWidget::getAttachedRouteId() const
{
    for (const auto& route : routeList) {
        if (route.attachedToShip) {
            return route.routeId;
        }
    }
    return -1; // No attached route
}

bool EcWidget::hasAttachedRoute() const
{
    return getAttachedRouteId() != -1;
}

void EcWidget::setSelectedRoute(int routeId)
{
    qDebug() << "[SELECTED-ROUTE] Setting selectedRouteId from" << selectedRouteId << "to" << routeId;

    // If same route, no need to redraw
    if (selectedRouteId == routeId) {
        qDebug() << "[SELECTED-ROUTE] Same route selected, skipping redraw";
        return;
    }

    selectedRouteId = routeId;

    // Single optimized draw - no need for multiple draws
    Draw();
}

int EcWidget::getNextAvailableRouteId() const
{
    // Find lowest available route ID starting from 1
    QSet<int> usedRouteIds;

    // Collect all route IDs currently in use
    for (const auto& wp : waypointList) {
        if (wp.routeId > 0) {
            usedRouteIds.insert(wp.routeId);
        }
    }

    // Find the first available ID starting from 1
    for (int id = 1; id <= 1000; ++id) { // Reasonable upper limit
        if (!usedRouteIds.contains(id)) {
            return id;
        }
    }

    // Fallback if somehow we have 1000 routes
    return usedRouteIds.size() + 1;
}

void EcWidget::forceRedraw()
{
    // Force complete chart redraw
    draw(true);
    update();
    repaint();
}

void EcWidget::immediateRedraw()
{
    // Immediate redraw for route selection
    qDebug() << "[REDRAW] Starting immediate redraw";
    draw(true);
    draw(false);
    update();
    repaint();
    QApplication::processEvents();
    qDebug() << "[REDRAW] Immediate redraw completed";
}

void EcWidget::createSeparateRouteWaypoint(const Waypoint &waypoint)
{
    // Simple approach: Just add to list and save, similar to GuardZone
    Waypoint wp = waypoint;
    wp.featureHandle.id = EC_NOCELLID;
    wp.featureHandle.offset = 0;

    // Add to our list
    waypointList.append(wp);

    //qDebug() << "[ROUTE] Added waypoint" << wp.label << "to route" << wp.routeId;

    // Persist to single-waypoints JSON (route waypoints skipped; keep for backwards compat of singles)
    saveWaypoints();

    // Rebuild and persist route from waypointList to ensure route JSON is accurate
    if (wp.routeId > 0) {
        updateRouteList(wp.routeId);
        saveRoutes();
    }

    // Redraw everything to show new waypoint and route lines
    Draw();
    update();
}

void EcWidget::createSingleWaypoint(const Waypoint &waypoint)
{
    // Use default udoCid for single waypoints (backward compatibility)
    if (udoCid == EC_NOCELLID) {
        if (!createWaypointCell()) {
            qDebug() << "[ERROR] Failed to create default waypoint cell";
            return;
        }
    }

    // Create waypoint in default cell
    Waypoint wp = waypoint;
    double range = GetRange(currentScale);
    double pickRadius = 0.03 * range;

    EcFeature wpFeature = EcRouteAddWaypoint(udoCid, dictInfo, wp.lat, wp.lon, pickRadius, wp.turningRadius);

    if (ECOK(wpFeature)) {
        wp.featureHandle = wpFeature;
        qDebug() << "[SINGLE] Created single waypoint" << wp.label;
    } else {
        qDebug() << "[ERROR] Failed to create single waypoint in SevenCs";
        wp.featureHandle.id = EC_NOCELLID;
        wp.featureHandle.offset = 0;
    }

    // Add to our list
    waypointList.append(wp);

    // Save waypoints to file
    saveWaypoints();

    // Update display safely
    Draw();
    update();
}

void EcWidget::drawWaypointMarker(double lat, double lon)
{
    int x, y;

    if (!LatLonToXy(lat, lon, x, y))
    {
        qDebug() << "[ERROR] Failed to convert LatLon to XY.";
        return;
    }

    QPainter painter(&drawPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(QColor(255, 140, 0)); // <-- ORANGE
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    painter.drawEllipse(QPoint(x, y), 8, 8);

    painter.end();
}


void EcWidget::drawWaypointWithLabel(double lat, double lon, const QString& label, const QColor& color)
{
    int x, y;

    if (!LatLonToXy(lat, lon, x, y))
        return;

    QPainter painter(&drawPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(color);
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // Gambar Donut waypoint
    painter.drawEllipse(QPoint(x, y), 8, 8);

    // Setup font dan ukuran teks
    QFont font("Arial", 9, QFont::Bold);
    painter.setFont(font);
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(label);

    // Use fixed label position instead of findOptimalLabelPosition
    QPoint labelPos(x + 10, y - 10); // Fixed offset: 10px right and 10px up from waypoint

    // Gambar teks label tanpa background
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, 1));
    painter.drawText(labelPos, label);

    painter.end();
}

QPoint EcWidget::findOptimalLabelPosition(int waypointX, int waypointY, const QSize& textSize, int minDistance)
{
    // Static list untuk menyimpan posisi label yang sudah digunakan dalam satu frame draw
    static QList<QRect> usedLabelRects;
    static int lastDrawTime = 0;

    // Reset list setiap kali draw baru dimulai
    int currentTime = QTime::currentTime().msecsSinceStartOfDay();
    if (currentTime - lastDrawTime > 100) {
        usedLabelRects.clear();
        lastDrawTime = currentTime;
    }

    // Dapatkan bounds widget untuk memastikan label tidak keluar
    QRect widgetBounds = rect();
    int margin = 10; // Margin dari tepi widget

    // Daftar posisi kandidat dengan prioritas UI/UX yang baik
    QList<QPoint> candidatePositions;

    // Prioritas 1: Posisi terpusat dan mudah dibaca (jarak optimal)
    int distance = minDistance;

    // Urutan prioritas berdasarkan prinsip UI/UX:
    // 1. Kanan atas (paling umum dan mudah dibaca)
    // 2. Kiri atas (alternatif terbaik)
    // 3. Kanan bawah
    // 4. Kiri bawah
    // 5. Posisi tengah (jika ruang cukup)
    candidatePositions << QPoint(waypointX + distance, waypointY - distance);
    candidatePositions << QPoint(waypointX - distance, waypointY - distance);
    candidatePositions << QPoint(waypointX + distance, waypointY + distance);
    candidatePositions << QPoint(waypointX - distance, waypointY + distance);
    candidatePositions << QPoint(waypointX, waypointY - distance - 8);
    candidatePositions << QPoint(waypointX, waypointY + distance + 8);

    // Prioritas 2: Jarak sedang jika posisi dekat masih bertabrakan
    distance = minDistance + 8;
    candidatePositions << QPoint(waypointX + distance, waypointY - distance);
    candidatePositions << QPoint(waypointX - distance, waypointY - distance);
    candidatePositions << QPoint(waypointX + distance, waypointY + distance);
    candidatePositions << QPoint(waypointX - distance, waypointY + distance);

    // Cari posisi optimal yang memenuhi kriteria UI/UX
    for (const QPoint& candidate : candidatePositions) {
        // Hitung rectangle label dengan padding
        QRect labelRect(candidate.x() - 3, candidate.y() - textSize.height() - 3,
                       textSize.width() + 6, textSize.height() + 4);

        // KRITERIA 1: Pastikan label tidak keluar dari bounds widget
        if (labelRect.left() < widgetBounds.left() + margin ||
            labelRect.right() > widgetBounds.right() - margin ||
            labelRect.top() < widgetBounds.top() + margin ||
            labelRect.bottom() > widgetBounds.bottom() - margin) {
            continue; // Skip posisi yang keluar bounds
        }

        // KRITERIA 2: Check collision dengan label lain
        bool hasCollision = false;
        for (const QRect& usedRect : usedLabelRects) {
            if (labelRect.intersects(usedRect)) {
                hasCollision = true;
                break;
            }
        }

        // Jika posisi memenuhi semua kriteria, gunakan posisi ini
        if (!hasCollision) {
            usedLabelRects.append(labelRect);
            return candidate;
        }
    }

    // Fallback dengan constraining ke dalam bounds
    QPoint fallbackPos(waypointX + minDistance, waypointY - minDistance);

    // Constraint ke dalam widget bounds
    QRect fallbackRect(fallbackPos.x() - 3, fallbackPos.y() - textSize.height() - 3,
                       textSize.width() + 6, textSize.height() + 4);

    // Adjust jika keluar bounds
    if (fallbackRect.right() > widgetBounds.right() - margin) {
        fallbackPos.setX(widgetBounds.right() - margin - textSize.width() - 6);
    }
    if (fallbackRect.left() < widgetBounds.left() + margin) {
        fallbackPos.setX(widgetBounds.left() + margin + 3);
    }
    if (fallbackRect.top() < widgetBounds.top() + margin) {
        fallbackPos.setY(widgetBounds.top() + margin + textSize.height() + 3);
    }
    if (fallbackRect.bottom() > widgetBounds.bottom() - margin) {
        fallbackPos.setY(widgetBounds.bottom() - margin - 3);
    }

    // Update rectangle dengan posisi yang sudah di-adjust
    fallbackRect = QRect(fallbackPos.x() - 3, fallbackPos.y() - textSize.height() - 3,
                        textSize.width() + 6, textSize.height() + 4);
    usedLabelRects.append(fallbackRect);

    return fallbackPos;
}

void EcWidget::drawGhostWaypoint(QPainter& painter, double lat, double lon, const QString& label)
{
    int x, y;

    if (!LatLonToXy(lat, lon, x, y))
        return;

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw ghost route lines first (behind waypoint)
    if (ghostWaypoint.routeId > 0 && ghostWaypoint.waypointIndex >= 0) {
        drawGhostRouteLines(painter, lat, lon, ghostWaypoint.routeId, ghostWaypoint.waypointIndex);
    }

    // Style ghost waypoint: semi-transparent dengan outline dashed
    QPen pen(QColor(255, 140, 0, 120)); // Orange semi-transparent
    pen.setWidth(2);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);

    QBrush brush(QColor(255, 140, 0, 60)); // Fill semi-transparent
    painter.setBrush(Qt::NoBrush); // Transparent area (outline only)

    // Gambar ghost donut
    painter.drawEllipse(QPoint(x, y), 8, 8);

    // Gambar ghost label dengan info tambahan
    QFont ghostFont("Segoe UI", 9, QFont::DemiBold);
    painter.setFont(ghostFont);

    // Build display text: Name on first line, lat/lon on second line
    QString latDM = formatDegMinCoord(lat, true);
    QString lonDM = formatDegMinCoord(lon, false);
    QString displayText = label + "\n" + latDM + ", " + lonDM;
    // AOI-style lines to be drawn in a boxed label
    QString nameLine = label;
    QString coordLine = latDM + ", " + lonDM;
    QString bearingLine = QString("Brg: -");

    // Tambahkan informasi jarak dan bearing jika ada waypoint referensi
    if (ghostWaypoint.routeId > 0) {
        // Cari waypoint referensi untuk perhitungan jarak dan bearing
        const Waypoint* refWaypoint = nullptr;

        if (activeFunction == CREATE_ROUTE) {
            // Untuk CREATE_ROUTE, gunakan waypoint terakhir dalam route
            QList<int> routeIndices;
            for (int i = 0; i < waypointList.size(); ++i) {
                if (waypointList[i].routeId == ghostWaypoint.routeId) {
                    routeIndices.append(i);
                }
            }
            if (!routeIndices.isEmpty()) {
                std::sort(routeIndices.begin(), routeIndices.end());
                refWaypoint = &waypointList[routeIndices.last()];
            }
        } else if (activeFunction == MOVE_WAYP && moveSelectedIndex != -1) {
            // Untuk MOVE_WAYP, cari waypoint sebelumnya dalam route
            QList<int> routeIndices;
            for (int i = 0; i < waypointList.size(); ++i) {
                if (waypointList[i].routeId == ghostWaypoint.routeId) {
                    routeIndices.append(i);
                }
            }
            std::sort(routeIndices.begin(), routeIndices.end());

            int movingPos = -1;
            for (int i = 0; i < routeIndices.size(); ++i) {
                if (routeIndices[i] == moveSelectedIndex) {
                    movingPos = i;
                    break;
                }
            }

            if (movingPos > 0) {
                refWaypoint = &waypointList[routeIndices[movingPos - 1]];
            }
        }

        // Hitung jarak dan bearing jika ada waypoint referensi
        if (refWaypoint) {
            double distance_m = haversine(refWaypoint->lat, refWaypoint->lon, lat, lon); // meters
            double distance_nm = distance_m / 1852.0; // convert to nautical miles
            double bearing = atan2(sin((lon - refWaypoint->lon) * M_PI / 180.0) * cos(lat * M_PI / 180.0),
                                 cos(refWaypoint->lat * M_PI / 180.0) * sin(lat * M_PI / 180.0) -
                                 sin(refWaypoint->lat * M_PI / 180.0) * cos(lat * M_PI / 180.0) *
                                 cos((lon - refWaypoint->lon) * M_PI / 180.0)) * 180.0 / M_PI;

            // Konversi bearing ke 0-360 derajat
            if (bearing < 0) bearing += 360;
            // Prepare bearing line for AOI-style box
            {
                QString deg = QString::fromUtf8("\u00B0");
                bearingLine = QString("Brg: %1%2").arg(bearing, 0, 'f', 1).arg(deg);
            }

            displayText += QString("\nDist: %1 NM\nBrg: %2°")
                          .arg(distance_nm, 0, 'f', 2)
                          .arg(bearing, 0, 'f', 1);
        }
    }

    // Build AOI-like label box with 3 lines: name, coord, bearing
    // Compute bearing line text if not already set above
    // Note: nameLine, coordLine, bearingLine prepared earlier
    QFont titleFont("Segoe UI", 9, QFont::DemiBold);
    QFont textFont("Segoe UI", 9, QFont::Normal);
    QFontMetrics fmTitle(titleFont);
    QFontMetrics fmText(textFont);

    int line1H = fmTitle.height();
    int line2H = fmText.height();
    int line3H = fmText.height();
    int textW = 0;
    textW = qMax(textW, fmTitle.horizontalAdvance(nameLine));
    textW = qMax(textW, fmText.horizontalAdvance(coordLine));
    textW = qMax(textW, fmText.horizontalAdvance(bearingLine));
    int spacing = 2;
    int textH = line1H + spacing + line2H + spacing + line3H;
    int padX = 8, padY = 6;

    QRect boxRect(x + 12, y - 15 - textH - padY*2, textW + padX*2, textH + padY*2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0,0,0,160));
    painter.drawRoundedRect(boxRect, 5, 5);

    // Draw text lines
    painter.setPen(QColor(255,255,255,235));
    int tx = boxRect.x() + padX;
    int ty = boxRect.y() + padY + (line1H - fmTitle.descent());
    painter.setFont(titleFont);
    painter.drawText(tx, ty, nameLine);
    painter.setFont(textFont);
    ty += spacing + line2H - fmText.descent();
    painter.drawText(tx, ty, coordLine);
    ty += spacing + line3H - fmText.descent();
    painter.drawText(tx, ty, bearingLine);
}

void EcWidget::drawHighlightedWaypoint(QPainter& painter, double lat, double lon, const QString& label)
{
    int x, y;

    if (!LatLonToXy(lat, lon, x, y))
        return;

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Style highlighted waypoint: bright yellow/gold with pulsing effect
    // Use time-based animation instead of frame counter for consistent speed
    static QElapsedTimer animationTimer;
    if (!animationTimer.isValid()) {
        animationTimer.start();
    }

    // Get consistent time-based animation values
    qint64 elapsed = animationTimer.elapsed();
    double timeSeconds = elapsed / 1000.0;

    // Create smooth pulsing effect with consistent timing
    // Pulse period: 1.5 seconds for radius, 2 seconds for opacity (faster)
    int pulseRadius = 12 + (int)(4 * sin(timeSeconds * 2.0 * M_PI / 1.5));
    int opacity = 150 + (int)(50 * sin(timeSeconds * 2.0 * M_PI / 2.0));

    // Outer glow ring
    QPen glowPen(QColor(255, 215, 0, opacity / 3)); // Gold glow
    glowPen.setWidth(3);
    painter.setPen(glowPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPoint(x, y), pulseRadius + 5, pulseRadius + 5);

    // Main highlight ring
    QPen highlightPen(QColor(255, 215, 0, opacity)); // Bright gold
    highlightPen.setWidth(4);
    painter.setPen(highlightPen);
    QBrush highlightBrush(QColor(255, 255, 0, opacity / 4)); // Yellow fill
    painter.setBrush(highlightBrush);
    painter.drawEllipse(QPoint(x, y), pulseRadius, pulseRadius);

    // Inner core
    QPen corePen(QColor(255, 140, 0, 255)); // Solid orange core
    corePen.setWidth(2);
    painter.setPen(corePen);
    QBrush coreBrush(QColor(255, 165, 0, 200)); // Orange fill
    painter.setBrush(coreBrush);
    painter.drawEllipse(QPoint(x, y), 6, 6);

    // No additional label - let the original waypoint label remain unchanged
}

void EcWidget::drawGhostRouteLines(QPainter& painter, double ghostLat, double ghostLon, int routeId, int waypointIndex)
{
    if (routeId <= 0) return;

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Find all waypoints in the same route
    QList<int> routeWaypointIndices;
    for (int i = 0; i < waypointList.size(); ++i) {
        if (waypointList[i].routeId == routeId) {
            routeWaypointIndices.append(i);
        }
    }

    // Handle CREATE_ROUTE mode: draw line from last waypoint to ghost position
    if (activeFunction == CREATE_ROUTE && routeWaypointIndices.size() >= 1) {
        // Sort waypoints by their order in the route (by original index)
        std::sort(routeWaypointIndices.begin(), routeWaypointIndices.end());

        // Get last waypoint in current route
        int lastWaypointIndex = routeWaypointIndices.last();
        const Waypoint& lastWaypoint = waypointList[lastWaypointIndex];

        // Setup ghost line style
        QPen ghostPen(QColor(255, 140, 0, 120)); // Orange semi-transparent
        ghostPen.setWidth(3);
        ghostPen.setStyle(Qt::DashLine);
        painter.setPen(ghostPen);

        int ghostX, ghostY;
        int lastX, lastY;
        if (LatLonToXy(ghostLat, ghostLon, ghostX, ghostY) &&
            LatLonToXy(lastWaypoint.lat, lastWaypoint.lon, lastX, lastY)) {

            // Draw line from last waypoint to ghost position
            painter.drawLine(lastX, lastY, ghostX, ghostY);

            // Calculate distance and bearing
            double distance = haversine(lastWaypoint.lat, lastWaypoint.lon, ghostLat, ghostLon);
            double bearing = atan2(sin((ghostLon - lastWaypoint.lon) * M_PI / 180.0) * cos(ghostLat * M_PI / 180.0),
                                 cos(lastWaypoint.lat * M_PI / 180.0) * sin(ghostLat * M_PI / 180.0) -
                                 sin(lastWaypoint.lat * M_PI / 180.0) * cos(ghostLat * M_PI / 180.0) *
                                 cos((ghostLon - lastWaypoint.lon) * M_PI / 180.0)) * 180.0 / M_PI;

            // Konversi bearing ke 0-360 derajat
            if (bearing < 0) bearing += 360;

            // Draw arrow direction indicator
            double angle = atan2(ghostY - lastY, ghostX - lastX);
            double arrowLength = 15;
            double arrowAngle = M_PI / 6; // 30 degrees

            // Calculate arrow head position (middle of leg line)
            double midX = lastX + 0.5 * (ghostX - lastX);
            double midY = lastY + 0.5 * (ghostY - lastY);

            int arrowX1 = midX - arrowLength * cos(angle - arrowAngle);
            int arrowY1 = midY - arrowLength * sin(angle - arrowAngle);
            int arrowX2 = midX - arrowLength * cos(angle + arrowAngle);
            int arrowY2 = midY - arrowLength * sin(angle + arrowAngle);

            QPen arrowPen(QColor(255, 140, 0, 150));
            arrowPen.setWidth(2);
            arrowPen.setStyle(Qt::SolidLine);
            painter.setPen(arrowPen);

            painter.drawLine(midX, midY, arrowX1, arrowY1);
            painter.drawLine(midX, midY, arrowX2, arrowY2);

            // Draw distance and bearing label on leg line (incoming)
            double distance_nm_in = distance / 1852.0;
            QString legInfo = QString("%1 NM / %2°")
                             .arg(distance_nm_in, 0, 'f', 2)
                             .arg(bearing, 0, 'f', 1);

            painter.setFont(QFont("Arial", 8, QFont::Bold));
            QPen textPen(QColor(255, 140, 0, 200));
            painter.setPen(textPen);

            // Position label slightly offset from middle of line
            QFontMetrics fm(painter.font());
            int textWidth = fm.horizontalAdvance(legInfo);
            int textHeight = fm.height();

            // Calculate perpendicular offset for text positioning
            double perpAngle = angle + M_PI / 2;
            int textX = midX - textWidth/2 + 10 * cos(perpAngle);
            int textY = midY + textHeight/4 + 10 * sin(perpAngle);

            // Draw background rectangle for better readability
            QRect textRect(textX - 2, textY - textHeight + 2, textWidth + 4, textHeight);
            QPen bgPen(QColor(255, 255, 255, 180));
            QBrush bgBrush(QColor(255, 255, 255, 120));
            painter.setPen(bgPen);
            painter.setBrush(bgBrush);
            painter.drawRect(textRect);

            // Draw text
            painter.setPen(textPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawText(textX, textY, legInfo);
        }
        return;
    }

    // Handle MOVE_WAYP mode: existing logic
    if (waypointIndex < 0 || routeWaypointIndices.size() < 2) return;

    // Sort waypoints by their order in the route (by original index)
    std::sort(routeWaypointIndices.begin(), routeWaypointIndices.end());

    // Find position of moving waypoint in the route
    int movingWaypointPosition = -1;
    for (int i = 0; i < routeWaypointIndices.size(); ++i) {
        if (routeWaypointIndices[i] == waypointIndex) {
            movingWaypointPosition = i;
            break;
        }
    }

    if (movingWaypointPosition < 0) return;

    // Setup ghost line style
    QPen ghostPen(QColor(255, 140, 0, 100)); // Orange semi-transparent
    ghostPen.setWidth(3);
    ghostPen.setStyle(Qt::DashLine);
    painter.setPen(ghostPen);

    int ghostX, ghostY;
    if (!LatLonToXy(ghostLat, ghostLon, ghostX, ghostY)) return;

    // Draw line to previous waypoint (if exists)
    if (movingWaypointPosition > 0) {
        int prevIndex = routeWaypointIndices[movingWaypointPosition - 1];
        const Waypoint& prevWaypoint = waypointList[prevIndex];
        int prevX, prevY;
        if (LatLonToXy(prevWaypoint.lat, prevWaypoint.lon, prevX, prevY)) {
            painter.drawLine(prevX, prevY, ghostX, ghostY);

            // Calculate distance and bearing
            double distance = haversine(prevWaypoint.lat, prevWaypoint.lon, ghostLat, ghostLon);
            double bearing = atan2(sin((ghostLon - prevWaypoint.lon) * M_PI / 180.0) * cos(ghostLat * M_PI / 180.0),
                                 cos(prevWaypoint.lat * M_PI / 180.0) * sin(ghostLat * M_PI / 180.0) -
                                 sin(prevWaypoint.lat * M_PI / 180.0) * cos(ghostLat * M_PI / 180.0) *
                                 cos((ghostLon - prevWaypoint.lon) * M_PI / 180.0)) * 180.0 / M_PI;

            // Konversi bearing ke 0-360 derajat
            if (bearing < 0) bearing += 360;

            // Draw arrow direction indicator (from previous to ghost)
            double angle = atan2(ghostY - prevY, ghostX - prevX);
            double arrowLength = 12;
            double arrowAngle = M_PI / 6; // 30 degrees

            int arrowX1 = ghostX - arrowLength * cos(angle - arrowAngle);
            int arrowY1 = ghostY - arrowLength * sin(angle - arrowAngle);
            int arrowX2 = ghostX - arrowLength * cos(angle + arrowAngle);
            int arrowY2 = ghostY - arrowLength * sin(angle + arrowAngle);

            QPen arrowPen(QColor(255, 140, 0, 150));
            arrowPen.setWidth(2);
            arrowPen.setStyle(Qt::SolidLine);
            painter.setPen(arrowPen);

            painter.drawLine(ghostX, ghostY, arrowX1, arrowY1);
            painter.drawLine(ghostX, ghostY, arrowX2, arrowY2);

            // Draw distance and bearing label on leg line (incoming)
            QString legInfo = QString("%1 NM / %2°")
                             .arg(distance, 0, 'f', 2)
                             .arg(bearing, 0, 'f', 1);

            painter.setFont(QFont("Arial", 7, QFont::Bold));
            QPen textPen(QColor(255, 140, 0, 200));
            painter.setPen(textPen);

            // Position label at 1/3 of the line from previous waypoint
            double labelX = prevX + 0.33 * (ghostX - prevX);
            double labelY = prevY + 0.33 * (ghostY - prevY);

            // Calculate perpendicular offset for text positioning
            double perpAngle = angle + M_PI / 2;
            QFontMetrics fm(painter.font());
            int textWidth = fm.horizontalAdvance(legInfo);
            int textHeight = fm.height();

            int textX = labelX - textWidth/2 + 8 * cos(perpAngle);
            int textY = labelY + textHeight/4 + 8 * sin(perpAngle);

            // Draw background rectangle for better readability
            QRect textRect(textX - 2, textY - textHeight + 2, textWidth + 4, textHeight);
            QPen bgPen(QColor(255, 255, 255, 160));
            QBrush bgBrush(QColor(255, 255, 255, 100));
            painter.setPen(bgPen);
            painter.setBrush(bgBrush);
            painter.drawRect(textRect);

            // Draw text
            painter.setPen(textPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawText(textX, textY, legInfo);
        }
    }

    // Draw line to next waypoint (if exists)
    if (movingWaypointPosition < routeWaypointIndices.size() - 1) {
        int nextIndex = routeWaypointIndices[movingWaypointPosition + 1];
        const Waypoint& nextWaypoint = waypointList[nextIndex];
        int nextX, nextY;
        if (LatLonToXy(nextWaypoint.lat, nextWaypoint.lon, nextX, nextY)) {
            ghostPen.setStyle(Qt::DashLine);
            painter.setPen(ghostPen);
            painter.drawLine(ghostX, ghostY, nextX, nextY);

            // Calculate distance and bearing
            double distance = haversine(ghostLat, ghostLon, nextWaypoint.lat, nextWaypoint.lon);
            double bearing = atan2(sin((nextWaypoint.lon - ghostLon) * M_PI / 180.0) * cos(nextWaypoint.lat * M_PI / 180.0),
                                 cos(ghostLat * M_PI / 180.0) * sin(nextWaypoint.lat * M_PI / 180.0) -
                                 sin(ghostLat * M_PI / 180.0) * cos(nextWaypoint.lat * M_PI / 180.0) *
                                 cos((nextWaypoint.lon - ghostLon) * M_PI / 180.0)) * 180.0 / M_PI;

            // Konversi bearing ke 0-360 derajat
            if (bearing < 0) bearing += 360;

            // Draw arrow direction indicator (from ghost to next)
            double angle = atan2(nextY - ghostY, nextX - ghostX);
            double arrowLength = 12;
            double arrowAngle = M_PI / 6; // 30 degrees

            // Calculate arrow head position (middle of leg line)
            double midX = ghostX + 0.5 * (nextX - ghostX);
            double midY = ghostY + 0.5 * (nextY - ghostY);

            int arrowX1 = midX - arrowLength * cos(angle - arrowAngle);
            int arrowY1 = midY - arrowLength * sin(angle - arrowAngle);
            int arrowX2 = midX - arrowLength * cos(angle + arrowAngle);
            int arrowY2 = midY - arrowLength * sin(angle + arrowAngle);

            QPen arrowPen(QColor(255, 140, 0, 150));
            arrowPen.setWidth(2);
            arrowPen.setStyle(Qt::SolidLine);
            painter.setPen(arrowPen);

            painter.drawLine(midX, midY, arrowX1, arrowY1);
            painter.drawLine(midX, midY, arrowX2, arrowY2);

            // Draw distance and bearing label on leg line (outgoing)
            double distance_nm_out = distance / 1852.0;
            QString legInfo = QString("%1 NM / %2°")
                             .arg(distance_nm_out, 0, 'f', 2)
                             .arg(bearing, 0, 'f', 1);

            painter.setFont(QFont("Arial", 7, QFont::Bold));
            QPen textPen(QColor(255, 140, 0, 200));
            painter.setPen(textPen);

            // Position label at 2/3 of the line toward next waypoint
            double labelX = ghostX + 0.67 * (nextX - ghostX);
            double labelY = ghostY + 0.67 * (nextY - ghostY);

            // Calculate perpendicular offset for text positioning
            double perpAngle = angle - M_PI / 2; // Opposite side from incoming line
            QFontMetrics fm(painter.font());
            int textWidth = fm.horizontalAdvance(legInfo);
            int textHeight = fm.height();

            int textX = labelX - textWidth/2 + 8 * cos(perpAngle);
            int textY = labelY + textHeight/4 + 8 * sin(perpAngle);

            // Draw background rectangle for better readability
            QRect textRect(textX - 2, textY - textHeight + 2, textWidth + 4, textHeight);
            QPen bgPen(QColor(255, 255, 255, 160));
            QBrush bgBrush(QColor(255, 255, 255, 100));
            painter.setPen(bgPen);
            painter.setBrush(bgBrush);
            painter.drawRect(textRect);

            // Draw text
            painter.setPen(textPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawText(textX, textY, legInfo);
        }
    }
}

// ====== CONTEXT MENU IMPLEMENTATIONS ======

int EcWidget::findWaypointAt(int x, int y)
{
    for (int i = 0; i < waypointList.size(); ++i) {
        int wx, wy;
        if (LatLonToXy(waypointList[i].lat, waypointList[i].lon, wx, wy)) {
            if (qAbs(x - wx) <= 10 && qAbs(y - wy) <= 10) {
                return i; // Return waypoint index
            }
        }
    }
    return -1; // No waypoint found
}

int EcWidget::findLeglineAt(int x, int y, int& routeId, int& segmentIndex)
{
    const int tolerance = 8; // pixels

    // Group waypoints by route
    QMap<int, QList<int>> routeWaypoints;
    for (int i = 0; i < waypointList.size(); ++i) {
        if (waypointList[i].routeId > 0) {
            routeWaypoints[waypointList[i].routeId].append(i);
        }
    }

    // Check each route's leglines
    for (auto it = routeWaypoints.begin(); it != routeWaypoints.end(); ++it) {
        int currentRouteId = it.key();
        QList<int> indices = it.value();

        if (indices.size() < 2) continue;

        // Sort waypoints by their original index (creation order)
        std::sort(indices.begin(), indices.end());

        // Check each leg segment in this route
        for (int i = 0; i < indices.size() - 1; ++i) {
            const Waypoint& wp1 = waypointList[indices[i]];
            const Waypoint& wp2 = waypointList[indices[i + 1]];

            int x1, y1, x2, y2;
            if (LatLonToXy(wp1.lat, wp1.lon, x1, y1) && LatLonToXy(wp2.lat, wp2.lon, x2, y2)) {
                // Calculate distance from point to line segment
                double distance = distanceToLineSegment(x, y, x1, y1, x2, y2);

                if (distance <= tolerance) {
                    routeId = currentRouteId;
                    segmentIndex = i; // Index of the first waypoint in the segment
                    return (int)distance; // Return distance as success indicator
                }
            }
        }
    }

    return -1; // No legline found
}

void EcWidget::showWaypointContextMenu(const QPoint& pos, int waypointIndex)
{
    if (waypointIndex < 0 || waypointIndex >= waypointList.size()) return;

    const Waypoint& waypoint = waypointList[waypointIndex];

    QMenu contextMenu(this);

    contextMenu.addAction(editAction);
    contextMenu.addAction(moveAction);
    contextMenu.addSeparator();
    contextMenu.addAction(deleteWaypointAction);

    if (waypoint.routeId > 0) {
        contextMenu.addAction(deleteRouteAction);
    }

    contextMenu.addSeparator();
    contextMenu.addAction(publishAction);

    // Execute menu
    QAction* selectedAction = contextMenu.exec(mapToGlobal(pos));

    if (!selectedAction) return;

    // Handle selected action
    if (selectedAction == editAction) {
        // Switch to edit waypoint mode and edit this waypoint
        editWaypointAt(pos.x(), pos.y());
    }
    else if (selectedAction == moveAction) {
        // Switch to move waypoint mode
        setActiveFunction(MOVE_WAYP);
        moveSelectedIndex = waypointIndex;

        // Setup ghost waypoint for immediate preview
        ghostWaypoint.visible = true;
        ghostWaypoint.lat = waypoint.lat;
        ghostWaypoint.lon = waypoint.lon;
        ghostWaypoint.label = waypoint.label;
        ghostWaypoint.routeId = waypoint.routeId;
        ghostWaypoint.waypointIndex = waypointIndex;

        if (mainWindow) {
            mainWindow->setWindowTitle(QString(APP_TITLE) + " - Move Waypoint Mode");
            //mainWindow->statusBar()->showMessage("Move the waypoint to a new position and click to confirm");
            mainWindow->routesStatusText->setText(tr("Move the waypoint to a new position and click to confirm"));
        }

        qDebug() << "[CONTEXT-MENU] Waypoint" << waypointIndex << "selected for moving";
    }
    else if (selectedAction == deleteWaypointAction) {
        // Delete individual waypoint
        removeWaypointAt(pos.x(), pos.y());
    }
    else if (selectedAction == deleteRouteAction && waypoint.routeId > 0) {
        // Delete entire route
        QString routeName = QString("Route %1").arg(waypoint.routeId);

        // Count waypoints in route
        int waypointCount = 0;
        for (const Waypoint& wp : waypointList) {
            if (wp.routeId == waypoint.routeId) {
                waypointCount++;
            }
        }

        // Confirm deletion
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("Confirm Delete Route"),
            tr("Are you sure you want to delete %1?\n\nThis will remove all %2 waypoints in this route.\nThis action cannot be undone.")
                .arg(routeName).arg(waypointCount),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            if (deleteRoute(waypoint.routeId)) {
                QMessageBox::information(this, tr("Route Deleted"),
                    tr("%1 has been deleted with %2 waypoints.").arg(routeName).arg(waypointCount));
            }
        }
    }

    else if (selectedAction == publishAction){
        double lat, lon;
        XyToLatLon(pos.x(), pos.y(), lat, lon);

        QString result = QString("%1, %2")
                            .arg(lat, 0, 'f', 6) // 6 angka di belakang koma
                            .arg(lon, 0, 'f', 6);

        if (subscriber){
            publishToMOOS("WAYPT_NEXT", result);
        }
    }
}

void EcWidget::createWaypointToolbox(const QPoint& pos, int waypointIndex)
{
    hideToolbox();

    if (waypointIndex < 0 || waypointIndex >= waypointList.size()) return;
    const Waypoint& waypoint = waypointList[waypointIndex];
    lastClick = pos;
    lastWaypointIndex = waypointIndex;
    lastWaypoint = waypoint;

    // Check if route is attached to ship
    bool isAttached = isRouteAttachedToShip(waypoint.routeId);

    // Show/hide info label and update buttons
    if (isAttached && false) {
        toolboxInfoLabel->setText("Disabled");
        toolboxInfoLabel->show();
    } else {
        toolboxInfoLabel->setText("Disabled");
        toolboxInfoLabel->hide();
    }

    // Disable all toolbox buttons if route is attached
    btn1->setEnabled(!isAttached); // Edit Waypoint
    btn2->setEnabled(!isAttached); // Move Waypoint
    //btn3->setEnabled(!isAttached); // Publish Waypoint
    btn4->setEnabled(!isAttached); // Delete Waypoint

    // Highlight the waypoint with yellow pulse when right-clicked
    // Find the waypoint index within its route
    // int routeWaypointIndex = 0;
    // for (int i = 0; i < waypointIndex; ++i) {
    //     if (waypointList[i].routeId == waypoint.routeId) {
    //         routeWaypointIndex++;
    //     }
    // }
    // highlightWaypoint(waypoint.routeId, routeWaypointIndex);

    // Posisi toolbox relatif cursor
    toolbox->adjustSize();
    QPoint posT = QCursor::pos();
    posT.setY(posT.y() - toolbox->height() - 5);
    posT.setX(posT.x() - toolbox->width() / 2);
    toolbox->move(posT);

    toolbox->show();
}

void EcWidget::hideToolbox()
{
    if (toolbox){
        toolbox->close();
    }
    if (toolboxLL){
        toolboxLL->close();
    }
    if (toolboxAoi){
        toolboxAoi->close();
    }
    if (toolboxAoiCreate){
        toolboxAoiCreate->close();
    }
}

void EcWidget::showLeglineContextMenu(const QPoint& pos, int routeId, int segmentIndex)
{
    if (routeId <= 0) return;

    QMenu contextMenu(this);

    contextMenu.addAction(insertWaypointAction);
    contextMenu.addSeparator();
    contextMenu.addAction(deleteRouteAction);

    // Execute menu
    QAction* selectedAction = contextMenu.exec(mapToGlobal(pos));

    if (!selectedAction) return;

    // Handle selected action
    if (selectedAction == insertWaypointAction) {
        // Convert click position to lat/lon
        EcCoordinate lat, lon;
        if (XyToLatLon(pos.x(), pos.y(), lat, lon)) {
            // Directly insert at computed position without switching modes
            insertWaypointAt(lat, lon);
            // Ensure we stay/return to PAN mode so next click does not insert again
            setActiveFunction(PAN);

            if (mainWindow) {
                mainWindow->setWindowTitle(QString(APP_TITLE) + " - Waypoint Inserted");
                mainWindow->statusBar()->showMessage(tr("Waypoint inserted into route"), 3000);
            }

            qDebug() << "[CONTEXT-MENU] Insert waypoint at" << lat << lon << "in route" << routeId;
        }
    }
    else if (selectedAction == deleteRouteAction) {
        // Delete entire route
        QString routeName = QString("Route %1").arg(routeId);

        // Count waypoints in route
        int waypointCount = 0;
        for (const Waypoint& wp : waypointList) {
            if (wp.routeId == routeId) {
                waypointCount++;
            }
        }

        // Confirm deletion
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("Confirm Delete Route"),
            tr("Are you sure you want to delete %1?\n\nThis will remove all %2 waypoints in this route.\nThis action cannot be undone.")
                .arg(routeName).arg(waypointCount),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            if (deleteRoute(routeId)) {
                QMessageBox::information(this, tr("Route Deleted"),
                    tr("%1 has been deleted with %2 waypoints.").arg(routeName).arg(waypointCount));
            }
        }
    }
}

void EcWidget::createLeglineToolbox(const QPoint& pos, int routeId, int segmentIndex)
{
    hideToolbox();

    if (routeId <= 0) return;
    lastClick = pos;

    // Check if route is attached to ship
    bool isAttached = isRouteAttachedToShip(routeId);

    // Show/hide info label
    if (isAttached && false) {
        toolboxLLInfoLabel->setText("Route attached - editing disabled");
        toolboxLLInfoLabel->show();
    } else {
        toolboxLLInfoLabel->hide();
    }

    // Disable legline toolbox button if route is attached
    btn->setEnabled(!isAttached); // Add Waypoint to legline

    // Posisi toolbox relatif cursor
    toolboxLL->adjustSize();
    QPoint posT = QCursor::pos();
    posT.setY(posT.y() - toolboxLL->height() - 5);
    posT.setX(posT.x() - toolboxLL->width() / 2);
    toolboxLL->move(posT);

    toolboxLL->show();
}

void EcWidget::showMapContextMenu(const QPoint& pos)
{
    QMenu contextMenu(this);

    // Create Route option
    contextMenu.addAction(createRouteAction);
    contextMenu.addSeparator();
    contextMenu.addAction(pickInfoAction);
    contextMenu.addAction(warningInfoAction);
    contextMenu.addSeparator();
    contextMenu.addAction(measureEblVrmAction);

    // Execute menu
    QAction* selectedAction = contextMenu.exec(mapToGlobal(pos));

    if (selectedAction == createRouteAction) {
        // Start route creation mode
        startRouteMode();
        setActiveFunction(CREATE_ROUTE);

        if (mainWindow) {
            mainWindow->setWindowTitle(QString(APP_TITLE) + " - Create Route");
            mainWindow->routesStatusText->setText(tr("Route Mode: Click to add waypoints. Press ESC or right-click to end route creation"));
        }

        qDebug() << "[CONTEXT-MENU] Create Route mode started";
    }
    else if (selectedAction == pickInfoAction){
        QList<EcFeature> pickedFeatureList;
        GetPickedFeatures(pickedFeatureList);

        pickWindow->fill(pickedFeatureList);
        pickWindow->show();
    }
    else if (selectedAction == warningInfoAction) {
        QList<EcFeature> pickedFeatureList;
        EcCoordinate lat, lon;
        XyToLatLon(pos.x(), pos.y(), lat, lon);
        GetPickedFeatures(pickedFeatureList);
        pickWindow->fillWarningOnly(pickedFeatureList, lat, lon);
        pickWindow->show();
    }
    else if (selectedAction == measureEblVrmAction) {
        EcCoordinate lat, lon;
        if (XyToLatLon(pos.x(), pos.y(), lat, lon)) {
            setEblVrmFixedTarget(lat, lon);
        }
    }
}

void EcWidget::saveWaypoints()
{
    QJsonArray waypointArray;

    for (const Waypoint &wp : waypointList)
    {
        // Skip route waypoints - they are saved in routes.json
        if (wp.routeId > 0) {
            continue;
        }

        QJsonObject wpObject;
        wpObject["label"] = wp.label;
        wpObject["lat"] = wp.lat;
        wpObject["lon"] = wp.lon;
        wpObject["remark"] = wp.remark;
        wpObject["turningRadius"] = wp.turningRadius;
        wpObject["active"] = wp.active;
        wpObject["routeId"] = wp.routeId;

        waypointArray.append(wpObject);
    }

    QJsonObject rootObject;
    rootObject["waypoints"] = waypointArray;

    QJsonDocument jsonDoc(rootObject);

    QString filePath = getWaypointFilePath();
    QDir dir = QFileInfo(filePath).dir();

    // Pastikan direktori ada
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            qDebug() << "[ERROR] Could not create directory for waypoints:" << dir.path();
            filePath = "waypoints.json"; // Gunakan direktori saat ini sebagai fallback
        }
    }

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write(jsonDoc.toJson(QJsonDocument::Indented)); // Format yang mudah dibaca
        file.close();
        qDebug() << "[INFO] Waypoints saved to" << filePath;
    }
    else
    {
        qDebug() << "[ERROR] Failed to save waypoints to" << filePath << ":" << file.errorString();

        // Coba simpan di direktori saat ini sebagai cadangan
        QFile fallbackFile("waypoints.json");
        if (fallbackFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            fallbackFile.write(jsonDoc.toJson(QJsonDocument::Indented));
            fallbackFile.close();
            qDebug() << "[INFO] Waypoints saved to fallback location: waypoints.json";
        }
        else
        {
            qDebug() << "[ERROR] Failed to save waypoints to fallback location:" << fallbackFile.errorString();
        }
    }
}

void EcWidget::removeWaypointAt(int x, int y)
{
    for (int i = 0; i < waypointList.size(); ++i)
    {
        int wx, wy;
        if (LatLonToXy(waypointList[i].lat, waypointList[i].lon, wx, wy))
        {
            if (qAbs(x - wx) <= 10 && qAbs(y - wy) <= 10)
            {
                Waypoint& wp = waypointList[i];
                int routeIdForPersist = wp.routeId; // capture before modifications
                QString waypointLabel = wp.label; // Simpan label sebelum dihapus

                // 🎯 Cek validitas handle SevenCs dan gunakan yang sesuai
                if (wp.isValid()) {
                    qDebug() << "[DEBUG] Removing waypoint using SevenCs";
                    Bool result = EcRouteDeleteWaypoint(dictInfo, wp.featureHandle);

                    if (!result) {
                        qDebug() << "[WARNING] EcRouteDeleteWaypoint failed, removing from list anyway";
                    } else {
                        qDebug() << "[INFO] Waypoint removed using SevenCs";
                    }
                } else {
                    qDebug() << "[WARNING] Removing waypoint from list only (invalid SevenCs handle)";
                }

                // Hapus dari list lokal (untuk kedua kasus)
                waypointList.removeAt(i);
                // Persist single-waypoints file (route waypoints are skipped inside)
                saveWaypoints();

                // If this was a route waypoint, rebuild route entry and persist routes.json
                if (routeIdForPersist > 0) {
                    updateRouteList(routeIdForPersist);
                    saveRoutes();
                }

                // Redraw and notify UI to refresh panels
                Draw();
                emit waypointCreated();

                // QMessageBox::information(this, tr("Waypoint Removed"),
                //                          tr("Waypoint '%1' has been removed.").arg(waypointLabel));

                return;
            }
        }
    }
    qDebug() << "[DEBUG] No waypoint found at click position for removal.";
}

void EcWidget::moveWaypointAt(int x, int y)
{
    if (moveSelectedIndex == -1)
    {
        // Klik pertama: pilih waypoint untuk dipindah
        for (int i = 0; i < waypointList.size(); ++i)
        {
            int wx, wy;
            if (LatLonToXy(waypointList[i].lat, waypointList[i].lon, wx, wy))
            {
                if (qAbs(x - wx) <= 10 && qAbs(y - wy) <= 10)
                {
                    moveSelectedIndex = i;

                    // Setup ghost waypoint
                    ghostWaypoint.visible = true;
                    ghostWaypoint.lat = waypointList[i].lat;
                    ghostWaypoint.lon = waypointList[i].lon;
                    ghostWaypoint.label = waypointList[i].label;
                    ghostWaypoint.routeId = waypointList[i].routeId;
                    ghostWaypoint.waypointIndex = i;

                    qDebug() << "[DEBUG] Waypoint selected for moving:" << i << "route:" << waypointList[i].routeId;
                    return;
                }
            }
        }
        qDebug() << "[DEBUG] No waypoint found at click position";
    }
    else
    {
        // Klik kedua: geser waypoint ke posisi baru
        EcCoordinate newLat, newLon;
        if (XyToLatLon(x, y, newLat, newLon))
        {
            Waypoint& wp = waypointList[moveSelectedIndex];

            // 🎯 Cek validitas handle SevenCs dan gunakan yang sesuai
            if (wp.isValid()) {
                // Gunakan EcRouteMoveWaypoint dari SevenCs
                double range = GetRange(currentScale);
                double pickRadius = 0.03 * range;

                qDebug() << "[DEBUG] Moving waypoint using SevenCs";
                Bool result = EcRouteMoveWaypoint(udoCid, dictInfo, EC_GEO_DATUM_WGS84,
                                                  wp.featureHandle, newLat, newLon, pickRadius);

                if (!result) {
                    QMessageBox::warning(this, tr("Error"), tr("Failed to move waypoint using SevenCs"));
                    qDebug() << "[ERROR] EcRouteMoveWaypoint failed";
                    return;
                }
                qDebug() << "[INFO] Waypoint moved successfully using SevenCs";
            } else {
                qDebug() << "[WARNING] Using manual coordinate update (invalid SevenCs handle)";
            }

            // Update koordinat di struct waypoint (untuk kedua kasus)
            wp.lat = newLat;
            wp.lon = newLon;

            // Preserve route visibility during move
            int routeId = wp.routeId;
            bool wasRouteVisible = (routeId > 0) ? isRouteVisible(routeId) : true;

            qDebug() << "[WAYPOINT-MOVE] Route" << routeId << "visibility before move operations:" << wasRouteVisible;

            // Save waypoints or routes depending on type
            if (routeId > 0) {
                // Route waypoint - update route data and save routes
                updateRouteFromWaypoint(routeId);

                // PRESERVE EXISTING VISIBILITY - DO NOT FORCE CHANGE
                qDebug() << "[WAYPOINT-MOVE] Route" << routeId << "preserving existing visibility:" << wasRouteVisible;

                saveRoutes();

                qDebug() << "[WAYPOINT-MOVE] Route" << routeId << "saved without changing visibility";

                qDebug() << "[WAYPOINT-MOVE] Route waypoint moved, route" << routeId << "final visibility:" << isRouteVisible(routeId);
            } else {
                // Single waypoint - save to waypoints.json
                saveWaypoints();
                qDebug() << "[WAYPOINT-MOVE] Single waypoint moved";
            }

            Draw();

            // NO DELAYED VISIBILITY CHANGES - Let user control via checkbox only

            // Pick Report removed - no longer shown after waypoint move

            // Reset ghost waypoint
            ghostWaypoint.visible = false;

            moveSelectedIndex = -1; // Reset
            activeFunction = PAN; // Kembali ke mode normal
            emit waypointCreated();
            qDebug() << "[DEBUG] Waypoint moved to new position.";
        }
    }
}

void EcWidget::editWaypointAt(int x, int y)
{
    for (int i = 0; i < waypointList.size(); ++i)
    {
        int wx, wy;
        if (LatLonToXy(waypointList[i].lat, waypointList[i].lon, wx, wy))
        {
            if (qAbs(x - wx) <= 10 && qAbs(y - wy) <= 10)
            {
                EditWaypointDialog dialog(this);
                dialog.setData(waypointList[i].label,
                               waypointList[i].remark,
                               waypointList[i].turningRadius,
                               waypointList[i].active);

                if (dialog.exec() == QDialog::Accepted)
                {
                    int routeIdForPersist = waypointList[i].routeId;
                    waypointList[i].label = dialog.getLabel();
                    waypointList[i].remark = dialog.getRemark();
                    waypointList[i].turningRadius = dialog.getTurnRadius();
                    // waypointList[i].active = dialog.isActive(); // Keep original active state

                    // Persist waypoints (singles) and update routes if needed
                    saveWaypoints();
                    if (routeIdForPersist > 0) {
                        updateRouteList(routeIdForPersist);
                        saveRoutes();
                    }
                    Draw();
                }
                activeFunction = PAN;
                emit waypointCreated();
                return;
            }
        }
    }

    qDebug() << "[DEBUG] No waypoint clicked for edit.";
}

bool EcWidget::resetWaypointCell()
{
    // 🔄 Unassign & Unmap jika sudah ada cell sebelumnya
    if (udoCid != EC_NOCELLID)
    {
        EcChartUnAssignCellFromView(view, udoCid);
        EcCellUnmap(udoCid);
        udoCid = EC_NOCELLID;
        qDebug() << "[INFO] Old UDO cell unassigned and unmapped.";
    }

    // 🔁 Buat dan assign ulang
    return createWaypointCell();
}

void EcWidget::drawLeglineLabels()
{
    if (waypointList.size() < 2)
        return;

    QPainter painter(&drawPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(QFont("Arial", 8, QFont::Bold));

    // Group waypoints by route ID first - same logic as drawRouteLines
    QMap<int, QList<int>> routeWaypoints; // routeId -> list of waypoint indices

    for (int i = 0; i < waypointList.size(); ++i) {
        int routeId = waypointList[i].routeId;
        if (routeId > 0) { // Only route waypoints, skip single waypoints
            routeWaypoints[routeId].append(i);
        }
    }

    // Draw labels within each route separately
    for (auto it = routeWaypoints.begin(); it != routeWaypoints.end(); ++it) {
        int routeId = it.key();
        QList<int> indices = it.value();

        // Check visibility - skip labels for hidden routes
        if (!isRouteVisible(routeId)) {
            continue;
        }

        if (indices.size() < 2) {
            continue;
        }

        // Filter only active waypoints for this route
        QList<int> activeIndices;
        for (int idx : indices) {
            if (waypointList[idx].active) {
                activeIndices.append(idx);
            }
        }

        // Draw labels between consecutive ACTIVE waypoints only
        for (int i = 0; i < activeIndices.size() - 1; ++i) {
            int idx1 = activeIndices[i];
            int idx2 = activeIndices[i + 1];

            const Waypoint &wp1 = waypointList[idx1];
            const Waypoint &wp2 = waypointList[idx2];

            EcCoordinate lat1 = wp1.lat;
            EcCoordinate lon1 = wp1.lon;
            EcCoordinate lat2 = wp2.lat;
            EcCoordinate lon2 = wp2.lon;

            double dist = 0.0;
            double bearing = 0.0;

            EcCalculateRhumblineDistanceAndBearing(
                EC_GEO_DATUM_WGS84,
                lat1, lon1,
                lat2, lon2,
                &dist, &bearing
                );

            // Posisi tengah garis
            int x1, y1, x2, y2;
            if (LatLonToXy(lat1, lon1, x1, y1) && LatLonToXy(lat2, lon2, x2, y2))
            {
                // Set warna teks sesuai dengan routeId
                QColor textColor = getRouteColor(routeId);
                painter.setPen(textColor);

                int midX = (x1 + x2) / 2;
                int midY = (y1 + y2) / 2;

                QString degree = QString::fromUtf8("\u00B0"); // simbol derajat
                QString text = QString("%1 NM @ %2%3")
                                   .arg(QString::number(dist, 'f', 1))
                                   .arg(QString::number(bearing, 'f', 0))
                                   .arg(degree);

                painter.drawText(midX + 20, midY - 15, text);
            }
        }
    }

    painter.end();
}

void EcWidget::iconUpdate(bool dark){
    if (dark){
        editAction->setIcon(QIcon(":/icon/edit_white.svg"));
        moveAction->setIcon(QIcon(":/icon/move_white.svg"));
        deleteWaypointAction->setIcon(QIcon(":/icon/delete_wp_white.svg"));
        deleteRouteAction->setIcon(QIcon(":/icon/delete_route_white.svg"));
        publishAction->setIcon(QIcon(":/icon/publish_white.svg"));
        insertWaypointAction->setIcon(QIcon(":/icon/create_wp_white.svg"));
        createRouteAction->setIcon(QIcon(":/icon/create_route_white.svg"));
        pickInfoAction->setIcon(QIcon(":/icon/info_white.svg"));
        warningInfoAction->setIcon(QIcon(":/icon/warning_white.svg"));
        measureEblVrmAction->setIcon(QIcon(":/icon/measure_white.svg"));

        btn1->setIcon(QIcon(":/icon/edit_o.svg"));
        btn2->setIcon(QIcon(":/icon/move_o.svg"));
        btn3->setIcon(QIcon(":/icon/publish_o.svg"));
        btn4->setIcon(QIcon(":/icon/delete_wp_o.svg"));
        btn->setIcon(QIcon(":/icon/create_o.svg"));

        btnCreate->setIcon(QIcon(":/icon/create_o.svg"));
        btnMove->setIcon(QIcon(":/icon/move_o.svg"));
        btnDelete->setIcon(QIcon(":/icon/delete_wp_o.svg"));

        QString dark =  "background-color: rgba(50, 50, 50, 220); "
                        "border-radius: 6px; "
                        "} "
                        "QToolButton { "
                        "background: transparent; "
                        "border: none; "
                        "margin: 4px; "
                        "} "
                        "QToolButton:hover { "
                        "background: rgba(255, 255, 255, 40); "
                        "border-radius: 6px; "
                        "} ";
        frame->setStyleSheet("#toolboxFrame { " +dark);
        frameLL->setStyleSheet("#toolboxFrameLL { " +dark);
        frameAoi->setStyleSheet("#toolboxFrameAoi { " +dark);
        frameAoiCreate->setStyleSheet("#toolboxFrameAoiCreate { " +dark);
    }
    else {
        editAction->setIcon(QIcon(":/icon/edit.svg"));
        moveAction->setIcon(QIcon(":/icon/move.svg"));
        deleteWaypointAction->setIcon(QIcon(":/icon/delete_wp.svg"));
        deleteRouteAction->setIcon(QIcon(":/icon/delete_route.svg"));
        publishAction->setIcon(QIcon(":/icon/publish.svg"));
        insertWaypointAction->setIcon(QIcon(":/icon/create_wp.svg"));
        createRouteAction->setIcon(QIcon(":/icon/create_route.svg"));
        pickInfoAction->setIcon(QIcon(":/icon/info.svg"));
        warningInfoAction->setIcon(QIcon(":/icon/warning.svg"));
        measureEblVrmAction->setIcon(QIcon(":/icon/measure.svg"));

        btn1->setIcon(QIcon(":/icon/edit_o_light.svg"));
        btn2->setIcon(QIcon(":/icon/move_o_light.svg"));
        btn3->setIcon(QIcon(":/icon/publish_o_light.svg"));
        btn4->setIcon(QIcon(":/icon/delete_wp_o_light.svg"));
        btn->setIcon(QIcon(":/icon/create_o_light.svg"));

        btnCreate->setIcon(QIcon(":/icon/create_o_light.svg"));
        btnMove->setIcon(QIcon(":/icon/move_o_light.svg"));
        btnDelete->setIcon(QIcon(":/icon/delete_wp_o_light.svg"));

        QString light = "background-color: rgba(255, 255, 255, 230); "
                        "border-radius: 6px; "
                        "} "
                        "QToolButton { "
                        "background: transparent; "
                        "border: none; "
                        "margin: 4px; "
                        "} "
                        "QToolButton:hover { "
                        "background: rgba(0, 0, 0, 40); "
                        "border-radius: 6px; "
                        "} ";
        frame->setStyleSheet("#toolboxFrame { " +light);
        frameLL->setStyleSheet("#toolboxFrameLL { " +light);
        frameAoi->setStyleSheet("#toolboxFrameAoi { " +light);
        frameAoiCreate->setStyleSheet("#toolboxFrameAoiCreate { " +light);
    }
}

// ====== ROUTE LINE DRAWING OVERLAY (Like GuardZone approach) ======
void EcWidget::drawRouteLinesOverlay(QPainter& painter)
{
    // Safety checks to prevent crash
    if (waypointList.size() < 2) return;
    if (!painter.isActive()) {
        qDebug() << "[ROUTE-OVERLAY-ERROR] QPainter is not active - aborting";
        return;
    }

    try {
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (routeSafetyFeature && routeSafeMode) {
            routeSafetyFeature->startFrame();
        }

    // Group waypoints by route ID first
    QMap<int, QList<int>> routeWaypoints; // routeId -> list of waypoint indices

    for (int i = 0; i < waypointList.size(); ++i) {
        int routeId = waypointList[i].routeId;
        if (routeId > 0) { // Only route waypoints, skip single waypoints
            routeWaypoints[routeId].append(i);
        }
    }

    qDebug() << "[ROUTE-OVERLAY] Drawing" << routeWaypoints.size() << "different routes";

    // Draw lines within each route separately
    for (auto it = routeWaypoints.begin(); it != routeWaypoints.end(); ++it) {
        int routeId = it.key();
        QList<int> indices = it.value();

        // Check visibility
        if (!isRouteVisible(routeId)) {
            qDebug() << "[ROUTE-OVERLAY] Route" << routeId << "is not visible, skipping line drawing";
            continue;
        }

        qDebug() << "[ROUTE-OVERLAY] Drawing lines for route" << routeId << "with" << indices.size() << "waypoints";

        if (indices.size() < 2) {
            continue;
        }

        // Choose color based on routeId
        QColor routeColor = getRouteColor(routeId);

        QPen pen(routeColor);
        pen.setStyle(Qt::DashLine);
        pen.setWidth(2); // Default width

        // Make selected route thicker and more visible - ALWAYS solid when selected
        if (routeId == selectedRouteId) {
            pen.setWidth(4); // Thicker line for selected route
            pen.setStyle(Qt::SolidLine); // Solid line for selected route
            // Keep original color for selected route, don't make it lighter
            pen.setColor(routeColor);
        }

        painter.setPen(pen);

        // Filter only active waypoints for this route
        QList<int> activeIndices;
        for (int idx : indices) {
            if (waypointList[idx].active) {
                activeIndices.append(idx);
            }
        }

        if (routeSafetyFeature && !activeIndices.isEmpty() && routeSafeMode) {
            QVector<RouteSafetyFeature::RouteWaypointSample> safetyPoints;
            safetyPoints.reserve(activeIndices.size());
            for (int idx : activeIndices) {
                RouteSafetyFeature::RouteWaypointSample sample;
                const Waypoint& wp = waypointList[idx];
                sample.lat = wp.lat;
                sample.lon = wp.lon;
                sample.active = wp.active;
                safetyPoints.append(sample);
            }
            if (safetyPoints.size() >= 2) {
                routeSafetyFeature->prepareForRoute(routeId, safetyPoints);
            }
        }

        // Draw lines between consecutive ACTIVE waypoints only
        for (int i = 0; i < activeIndices.size() - 1; ++i) {
            int idx1 = activeIndices[i];
            int idx2 = activeIndices[i + 1];

            const Waypoint &wp1 = waypointList[idx1];
            const Waypoint &wp2 = waypointList[idx2];

            int x1, y1, x2, y2;
            if (LatLonToXy(wp1.lat, wp1.lon, x1, y1) && LatLonToXy(wp2.lat, wp2.lon, x2, y2)) {
                painter.drawLine(x1, y1, x2, y2);

                // Draw arrow to show direction
                double angle = atan2(y2 - y1, x2 - x1);
                double arrowLength = 15;
                double arrowAngle = M_PI / 6; // 30 degrees

                // Calculate arrow head position (middle of leg line)
                double midX = x1 + 0.5 * (x2 - x1);
                double midY = y1 + 0.5 * (y2 - y1);

                // Arrow head points
                int arrowX1 = midX - arrowLength * cos(angle - arrowAngle);
                int arrowY1 = midY - arrowLength * sin(angle - arrowAngle);
                int arrowX2 = midX - arrowLength * cos(angle + arrowAngle);
                int arrowY2 = midY - arrowLength * sin(angle + arrowAngle);

                // Draw arrow head with solid pen
                QPen arrowPen = painter.pen();
                Qt::PenStyle originalStyle = arrowPen.style();
                arrowPen.setStyle(Qt::SolidLine);
                painter.setPen(arrowPen);
                painter.drawLine(midX, midY, arrowX1, arrowY1);
                painter.drawLine(midX, midY, arrowX2, arrowY2);

                // Restore original pen style for route lines
                arrowPen.setStyle(originalStyle);
                painter.setPen(arrowPen);
            }
        }
    }

        if (routeSafetyFeature && routeSafeMode) {
            routeSafetyFeature->render(painter);
            routeSafetyFeature->finishFrame();
        }

    } catch (const std::exception& e) {
        qDebug() << "[ROUTE-OVERLAY-ERROR] Exception in drawRouteLinesOverlay:" << e.what();
    } catch (...) {
        qDebug() << "[ROUTE-OVERLAY-ERROR] Unknown exception in drawRouteLinesOverlay";
    }
}

// ====== OLD ROUTE LINE DRAWING (DEPRECATED) ======
void EcWidget::drawRouteLines()
{
    if (waypointList.size() < 2) return;

    // ROUTE FIX: Enhanced safety checks for coordinate transformation
    try {
        QPainter painter(&drawPixmap);
        if (!painter.isActive()) {
            qDebug() << "[ROUTE-LINES-ERROR] QPainter not active - aborting";
            return;
        }
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (routeSafetyFeature && routeSafeMode) {
            routeSafetyFeature->startFrame();
        }

        // ROUTE FIX: Validate coordinate transformation is ready
        if (!view || !initialized) {
            qDebug() << "[ROUTE-LINES-ERROR] View not initialized properly - aborting";
            return;
        }

    // Warna untuk route yang berbeda (harus sama dengan warna waypoint)
    // APPROACH BARU: Group waypoints by route ID first, then draw lines within each route
    QMap<int, QList<int>> routeWaypoints; // routeId -> list of waypoint indices

    // Group waypoints by routeId first
    for (int i = 0; i < waypointList.size(); ++i) {
        int routeId = waypointList[i].routeId;
        if (routeId > 0) { // Only route waypoints, skip single waypoints
            routeWaypoints[routeId].append(i);
        }
    }

    // Draw routes with visibility check

    // Draw lines within each route separately
    for (auto it = routeWaypoints.begin(); it != routeWaypoints.end(); ++it) {
        int routeId = it.key();
        QList<int> indices = it.value();

        // Check visibility
        if (!isRouteVisible(routeId)) {
            //qDebug() << "[ROUTE-DRAW] Route" << routeId << "is hidden, skipping";
            continue;
        }

        if (indices.size() < 2) {
            //qDebug() << "[ROUTE-DRAW] Route" << routeId << "has only" << indices.size() << "waypoints, skipping lines";
            continue;
        }

        // Pilih warna berdasarkan routeId
        QColor routeColor = getRouteColor(routeId);

        QPen pen(routeColor);
        pen.setStyle(Qt::DashLine);
        pen.setWidth(2); // Default width

        // Visual feedback for selected route - ALWAYS solid when selected
        if (routeId == selectedRouteId) {
            //qDebug() << "[ROUTE-DRAW] Drawing SELECTED route" << routeId << "as SOLID line";
            pen.setWidth(4); // Thicker line for selected route
            pen.setStyle(Qt::SolidLine); // Solid line for selected route
            // Keep original color for selected route, don't make it lighter
            pen.setColor(routeColor);
        }
        // Reduced debug output to improve performance during drawing

        painter.setPen(pen);

        // Draw lines for this route

        // Filter only active waypoints for this route
        QList<int> activeIndices;
        for (int idx : indices) {
            if (waypointList[idx].active) {
                activeIndices.append(idx);
            }
        }

        if (routeSafetyFeature && !activeIndices.isEmpty()) {
            QVector<RouteSafetyFeature::RouteWaypointSample> safetyPoints;
            safetyPoints.reserve(activeIndices.size());
            for (int idx : activeIndices) {
                RouteSafetyFeature::RouteWaypointSample sample;
                const Waypoint& wp = waypointList[idx];
                sample.lat = wp.lat;
                sample.lon = wp.lon;
                sample.active = wp.active;
                safetyPoints.append(sample);
            }
            if (safetyPoints.size() >= 2) {
                routeSafetyFeature->prepareForRoute(routeId, safetyPoints);
            }
        }

        // Draw lines between consecutive ACTIVE waypoints only
        for (int i = 0; i < activeIndices.size() - 1; ++i) {
            int idx1 = activeIndices[i];
            int idx2 = activeIndices[i + 1];

            const Waypoint &wp1 = waypointList[idx1];
            const Waypoint &wp2 = waypointList[idx2];

            int x1, y1, x2, y2;
            if (LatLonToXy(wp1.lat, wp1.lon, x1, y1) && LatLonToXy(wp2.lat, wp2.lon, x2, y2)) {
                painter.drawLine(x1, y1, x2, y2);

                // Draw arrow to show direction
                double angle = atan2(y2 - y1, x2 - x1);
                double arrowLength = 15;
                double arrowAngle = M_PI / 6; // 30 degrees

                // Calculate arrow head position (middle of leg line)
                double midX = x1 + 0.5 * (x2 - x1);
                double midY = y1 + 0.5 * (y2 - y1);

                // Arrow head points
                int arrowX1 = midX - arrowLength * cos(angle - arrowAngle);
                int arrowY1 = midY - arrowLength * sin(angle - arrowAngle);
                int arrowX2 = midX - arrowLength * cos(angle + arrowAngle);
                int arrowY2 = midY - arrowLength * sin(angle + arrowAngle);

                // Draw arrow head with solid pen
                QPen arrowPen = painter.pen();
                Qt::PenStyle originalStyle = arrowPen.style();
                arrowPen.setStyle(Qt::SolidLine);
                painter.setPen(arrowPen);
                painter.drawLine(midX, midY, arrowX1, arrowY1);
                painter.drawLine(midX, midY, arrowX2, arrowY2);

                // Restore original pen style for route lines
                arrowPen.setStyle(originalStyle);
                painter.setPen(arrowPen);

                //qDebug() << "[ROUTE-DRAW] Drew line with arrow between" << wp1.label << "and" << wp2.label
                         //<< "in route" << routeId << "style:" << (originalStyle == Qt::SolidLine ? "solid" : "dash");
            }
        }
    }

    if (routeSafetyFeature && routeSafeMode) {
        routeSafetyFeature->render(painter);
        routeSafetyFeature->finishFrame();
    }

    painter.end();
    //qDebug() << "[ROUTE-DRAW] Finished controlled route line drawing";

    } catch (const std::exception& e) {
        qDebug() << "[ROUTE-LINES-ERROR] Exception in drawRouteLines:" << e.what();
    } catch (...) {
        qDebug() << "[ROUTE-LINES-ERROR] Unknown exception in drawRouteLines";
    }
}

void EcWidget::loadWaypoints()
{
    QString filePath = getWaypointFilePath();
    QFile file(filePath);

    if (!file.exists())
    {
        qDebug() << "[INFO] Waypoints file not found. Starting with empty list.";
        return;
    }

    if (file.open(QIODevice::ReadOnly))
    {
        QByteArray fileData = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(fileData);
        QJsonArray waypointsArray;
        if (doc.isObject()) {
            // Newer format: object with "waypoints" array
            QJsonObject root = doc.object();
            waypointsArray = root.value("waypoints").toArray();
        } else if (doc.isArray()) {
            // Legacy format: top-level array
            waypointsArray = doc.array();
        }

        // Do NOT clear existing waypointList; routes from routes.json are already present.
        // Only append single waypoints (routeId == 0) from file to preserve routes.
        int validWaypoints = 0;
        int maxRouteId = 0;

        for (const QJsonValue& value : waypointsArray)
        {
            QJsonObject wpObject = value.toObject();

            Waypoint wp;
            wp.label = wpObject.contains("label") ?
                       wpObject["label"].toString() :
                       QString("WP%1").arg(validWaypoints + 1, 3, 10, QChar('0'));
            wp.lat = wpObject["lat"].toDouble();
            wp.lon = wpObject["lon"].toDouble();
            wp.remark = wpObject.contains("remark") ? wpObject["remark"].toString() : "";
            wp.turningRadius = wpObject.contains("turningRadius") ? wpObject["turningRadius"].toDouble() : 10.0;
            wp.active = wpObject.contains("active") ? wpObject["active"].toBool() : true;
            wp.routeId = wpObject.contains("routeId") ? wpObject["routeId"].toInt() : 0;

            // featureHandle akan invalid setelah load dari file
            wp.featureHandle.id = EC_NOCELLID;
            wp.featureHandle.offset = 0;

            // Only append single waypoints here (routeId == 0). Route waypoints are managed via routes.json
            if (wp.routeId == 0) {
                waypointList.append(wp);
                validWaypoints++;
            }

            // Track maximum route ID
            if (wp.routeId > maxRouteId) {
                maxRouteId = wp.routeId;
            }
        }

        // Set currentRouteId to the next available route ID (not just maxRouteId + 1)
        currentRouteId = getNextAvailableRouteId();

        qDebug() << "[INFO] Loaded" << validWaypoints << "waypoints from" << filePath << "- Max Route ID:" << maxRouteId;
        qDebug() << "[INFO] Total waypoints in list after loadWaypoints:" << waypointList.size();

        // Redraw waypoint setelah loading
        if (!waypointList.isEmpty())
        {
            // Pastikan cell waypoint sudah terinisialisasi
            if (udoCid == EC_NOCELLID)
            {
                if (!createWaypointCell())
                {
                    qDebug() << "[ERROR] Could not initialize waypoint cell";
                    return;
                }
            }

            // Recreate SevenCs waypoints dari data yang dimuat - SEPARATE BY ROUTE
            qDebug() << "[INFO] Recreating SevenCs waypoints from loaded data with route separation...";

            double range = GetRange(currentScale);
            double pickRadius = 0.03 * range;

            for (int i = 0; i < waypointList.size(); i++)
            {
                Waypoint& wp = waypointList[i];

                if (wp.routeId == 0) {
                    // Single waypoint - recreate in main udoCid menggunakan SevenCs
                    EcFeature wpFeature = EcRouteAddWaypoint(udoCid, dictInfo, wp.lat, wp.lon, pickRadius, wp.turningRadius);

                    if (ECOK(wpFeature))
                    {
                        wp.featureHandle = wpFeature;
                        qDebug() << "[DEBUG] Recreated single waypoint" << i << "(" << wp.label << ")";
                    }
                    else
                    {
                        qDebug() << "[ERROR] Failed to recreate single waypoint" << i << "(" << wp.label << ")";
                    }
                } else {
                    // Route waypoint - TIDAK menggunakan SevenCs engine untuk menghindari garis otomatis
                    // Hanya set invalid handle dan akan digambar manual oleh drawRouteLines()
                    wp.featureHandle.id = EC_NOCELLID;
                    wp.featureHandle.offset = 0;
                    qDebug() << "[DEBUG] Route waypoint" << i << "(" << wp.label << ") loaded without SevenCs engine (manual drawing only)";
                }
            }

            qDebug() << "[INFO] Finished recreating waypoints in SevenCs";
            Draw();
        }
    }
    else
    {
        qDebug() << "[ERROR] Failed to open waypoints file for reading:" << file.errorString();
    }
}

QString EcWidget::getWaypointFilePath() const
{
    // Simpan di direktori data aplikasi
    QString basePath;

#ifdef _WIN32
    if (EcKernelGetEnv("APPDATA"))
        basePath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC";
#else
    if (EcKernelGetEnv("HOME"))
        basePath = QString(EcKernelGetEnv("HOME")) + "/SevenCs/EC2007/DENC";
#endif

    // Jika base path tidak tersedia, gunakan direktori saat ini
    if (basePath.isEmpty())
        return "waypoints.json";
    else
        return basePath + "/waypoints.json";
}

void EcWidget::showWaypointError(const QString &message)
{
    qDebug() << "[ERROR] Waypoint Error:" << message;
    QMessageBox::warning(this, tr("Waypoint Error"), message);
}

bool EcWidget::initializeWaypointSystem()
{
    // If there's already a waypoint cell, we're good
    if (udoCid != EC_NOCELLID)
        return true;

    // Create the waypoint cell
    return createWaypointCell();
}

void EcWidget::clearWaypoints()
{
    if (waypointList.isEmpty())
        return;

    qDebug() << "[INFO] Clearing all waypoints - no confirmation needed (already confirmed by caller)";

    // Skip individual SevenCs deletion to prevent hang - just clear everything
    int waypointCount = waypointList.size();

    // Clear data structures
    waypointList.clear();
    routeList.clear();
    routeVisibility.clear();

    if (routeSafetyFeature && routeSafeMode) {
        routeSafetyFeature->invalidateAll();
    }

    // Reset route variables
    currentRouteId = 1;
    routeWaypointCounter = 1;
    qDebug() << "[SELECTED-ROUTE] CLEARING selectedRouteId (was" << selectedRouteId << ") due to clearWaypoints";
    selectedRouteId = -1;

    // Save empty state
    saveWaypoints();
    saveRoutes();

    // Redraw
    Draw();

    // Emit signal to refresh route panel
    emit waypointCreated();

    qDebug() << "[INFO] Cleared" << waypointCount << "waypoints and all routes";
}

void EcWidget::updateWaypointActiveStatus(int routeId, double lat, double lon, bool active)
{
    // Find the waypoint by route ID and coordinates
    for (auto& waypoint : waypointList) {
        if (waypoint.routeId == routeId &&
            qAbs(waypoint.lat - lat) < 0.000001 &&
            qAbs(waypoint.lon - lon) < 0.000001) {

            // Update the active status
            bool oldStatus = waypoint.active;
            waypoint.active = active;

            qDebug() << "[WAYPOINT-ACTIVE] Updated waypoint at" << lat << "," << lon
                     << "in route" << routeId << "from" << oldStatus << "to" << active;

            // Save the updated single-waypoints file (routes saved separately)
            saveWaypoints();

            // Also update the corresponding route entry and persist to routes.json (only for route waypoints)
            if (routeId > 0) {
                // Rebuild or create the route entry from current waypointList and persist
                updateRouteList(routeId);
                saveRoutes();
            }

            // Redraw to reflect changes
            Draw();

            // Emit signal to refresh UI
            emit waypointCreated();

            return;
        }
    }

    qDebug() << "[WAYPOINT-ACTIVE] Warning: Waypoint not found at" << lat << "," << lon
             << "in route" << routeId;
}

void EcWidget::replaceWaypointsForRoute(int routeId, const QList<Waypoint>& newWaypoints)
{
    qDebug() << "[WAYPOINT-REORDER] Replacing waypoints for route" << routeId << "with" << newWaypoints.size() << "waypoints";

    // Build lookup from current waypoints to preserve custom labels/remarks if needed
    QMap<QPair<int64_t,int64_t>, Waypoint> oldByCoord;
    {
        for (const auto &wp : waypointList) {
            if (wp.routeId == routeId) {
                // Quantize coordinates to avoid floating drift keys (micro-degree precision ~1e-6)
                int64_t latKey = static_cast<int64_t>(std::round(wp.lat * 1e6));
                int64_t lonKey = static_cast<int64_t>(std::round(wp.lon * 1e6));
                oldByCoord.insert(QPair<int64_t,int64_t>(latKey, lonKey), wp);
            }
        }
    }

    // Remove all existing waypoints for this route
    waypointList.erase(
        std::remove_if(waypointList.begin(), waypointList.end(),
            [routeId](const Waypoint& wp) {
                return wp.routeId == routeId;
            }),
        waypointList.end());

    // Add new waypoints in the specified order
    for (const auto& wp : newWaypoints) {
        if (wp.routeId == routeId) { // Only add waypoints for this route
            Waypoint toAdd = wp;
            // If label is empty (possible from external callers), try preserve from previous data
            if (toAdd.label.trimmed().isEmpty()) {
                int64_t latKey = static_cast<int64_t>(std::round(toAdd.lat * 1e6));
                int64_t lonKey = static_cast<int64_t>(std::round(toAdd.lon * 1e6));
                auto key = QPair<int64_t,int64_t>(latKey, lonKey);
                if (oldByCoord.contains(key)) {
                    const Waypoint &oldWp = oldByCoord[key];
                    if (!oldWp.label.trimmed().isEmpty()) toAdd.label = oldWp.label;
                    if (!oldWp.remark.trimmed().isEmpty()) toAdd.remark = oldWp.remark;
                }
            }
            waypointList.append(toAdd);
        }
    }

    qDebug() << "[WAYPOINT-REORDER] Total waypoints after reorder:" << waypointList.size();

    // Save updated waypoints (singles file) and update routes to reflect new state
    saveWaypoints();
    // Rebuild or create the route entry from current waypointList
    updateRouteList(routeId);
    saveRoutes();

    // Redraw to show new order
    Draw();

    // Emit signal to refresh UI components
    emit waypointCreated();
}

bool EcWidget::deleteRouteWaypointAt(int routeId, int indexInRoute)
{
    if (routeId <= 0 || indexInRoute < 0) return false;

    // Find the absolute index in waypointList corresponding to the indexInRoute-th
    // waypoint that belongs to this routeId
    int routeCounter = 0;
    int absoluteIndex = -1;
    for (int i = 0; i < waypointList.size(); ++i) {
        if (waypointList[i].routeId == routeId) {
            if (routeCounter == indexInRoute) { absoluteIndex = i; break; }
            routeCounter++;
        }
    }
    if (absoluteIndex < 0 || absoluteIndex >= waypointList.size()) return false;

    // Remove the waypoint
    waypointList.removeAt(absoluteIndex);

    // Persist single waypoints file (route waypoints are skipped there by design)
    saveWaypoints();

    // Rebuild and persist route entry
    updateRouteList(routeId);
    saveRoutes();

    // Redraw and notify
    Draw();
    emit waypointCreated();
    return true;
}

bool EcWidget::exportWaypointsToFile(const QString &filename)
{
    if (waypointList.isEmpty())
    {
        QMessageBox::warning(this, tr("Export Waypoints"),
                             tr("No waypoints to export."));
        return false;
    }

    QJsonArray waypointArray;

    for (const Waypoint &wp : waypointList)
    {
        QJsonObject wpObject;
        wpObject["label"] = wp.label;
        wpObject["lat"] = wp.lat;
        wpObject["lon"] = wp.lon;
        wpObject["remark"] = wp.remark;
        wpObject["turningRadius"] = wp.turningRadius;
        wpObject["active"] = wp.active;

        waypointArray.append(wpObject);
    }

    QJsonObject rootObject;
    rootObject["waypoints"] = waypointArray;
    rootObject["exported_on"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObject["waypoint_count"] = waypointList.size();

    QJsonDocument jsonDoc(rootObject);

    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write(jsonDoc.toJson(QJsonDocument::Indented));
        file.close();

        QMessageBox::information(this, tr("Export Successful"),
                                 tr("Exported %1 waypoints to %2")
                                     .arg(waypointList.size())
                                     .arg(QFileInfo(filename).fileName()));
        return true;
    }
    else
    {
        QMessageBox::critical(this, tr("Export Failed"),
                              tr("Failed to save waypoints to %1: %2")
                                  .arg(filename)
                                  .arg(file.errorString()));
        return false;
    }
}

bool EcWidget::importWaypointsFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.exists())
    {
        QMessageBox::warning(this, tr("Import Waypoints"),
                             tr("File does not exist: %1").arg(filename));
        return false;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QByteArray jsonData = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            QMessageBox::critical(this, tr("Import Failed"),
                                  tr("JSON parse error at position %1: %2")
                                      .arg(parseError.offset)
                                      .arg(parseError.errorString()));
            return false;
        }

        if (!jsonDoc.isObject())
        {
            QMessageBox::critical(this, tr("Import Failed"),
                                  tr("Invalid JSON file format (not an object)"));
            return false;
        }

        QJsonObject rootObject = jsonDoc.object();
        if (!rootObject.contains("waypoints") || !rootObject["waypoints"].isArray())
        {
            QMessageBox::critical(this, tr("Import Failed"),
                                  tr("Invalid waypoints file (missing waypoints array)"));
            return false;
        }

        QJsonArray waypointArray = rootObject["waypoints"].toArray();

        // Ask user what to do with existing waypoints
        if (!waypointList.isEmpty())
        {
            QMessageBox msgBox;
            msgBox.setWindowTitle(tr("Import Waypoints"));
            msgBox.setText(tr("You already have %1 waypoints.").arg(waypointList.size()));
            msgBox.setInformativeText(tr("Do you want to replace them or append the imported waypoints?"));
            QPushButton *replaceButton = msgBox.addButton(tr("Replace"), QMessageBox::ActionRole);
            QPushButton *appendButton = msgBox.addButton(tr("Append"), QMessageBox::ActionRole);
            QPushButton *cancelButton = msgBox.addButton(QMessageBox::Cancel);
            msgBox.setDefaultButton(appendButton);

            msgBox.exec();

            if (msgBox.clickedButton() == cancelButton)
                return false;

            if (msgBox.clickedButton() == replaceButton)
                waypointList.clear();
        }

        int importedCount = 0;
        int invalidCount = 0;

        for (const QJsonValue &value : waypointArray)
        {
            if (!value.isObject())
            {
                invalidCount++;
                continue;
            }

            QJsonObject wpObject = value.toObject();

            // Check for required fields
            if (!wpObject.contains("lat") || !wpObject.contains("lon"))
            {
                invalidCount++;
                continue;
            }

            Waypoint wp;
            wp.label = wpObject.contains("label") ? wpObject["label"].toString() :
                           QString("WP%1").arg(waypointList.size() + 1, 3, 10, QChar('0'));
            wp.lat = wpObject["lat"].toDouble();
            wp.lon = wpObject["lon"].toDouble();

            // Validate coordinates
            if (wp.lat < -90 || wp.lat > 90 || wp.lon < -180 || wp.lon > 180)
            {
                invalidCount++;
                continue;
            }

            wp.remark = wpObject.contains("remark") ? wpObject["remark"].toString() : "";
            wp.turningRadius = wpObject.contains("turningRadius") ? wpObject["turningRadius"].toDouble() : 10.0;
            wp.active = wpObject.contains("active") ? wpObject["active"].toBool() : true;

            waypointList.append(wp);
            importedCount++;
        }

        if (importedCount > 0)
        {
            // Save waypoints and redraw
            saveWaypoints();

            // Initialize the waypoint cell if needed
            if (udoCid == EC_NOCELLID)
            {
                if (!createWaypointCell())
                {
                    QMessageBox::warning(this, tr("Import Warning"),
                                         tr("Waypoints imported but failed to initialize the display cell."));
                    return true;
                }
            }

            Draw();

            QString message = tr("Successfully imported %1 waypoints").arg(importedCount);
            if (invalidCount > 0)
                message += tr("\n%1 waypoints were invalid and skipped").arg(invalidCount);

            QMessageBox::information(this, tr("Import Successful"), message);
            return true;
        }
        else
        {
            if (invalidCount > 0)
                QMessageBox::warning(this, tr("Import Failed"),
                                     tr("All %1 waypoints in the file were invalid").arg(invalidCount));
            else
                QMessageBox::warning(this, tr("Import Failed"),
                                     tr("No waypoints found in the file"));

            return false;
        }
    }
    else
    {
        QMessageBox::critical(this, tr("Import Failed"),
                              tr("Failed to open file: %1").arg(file.errorString()));
        return false;
    }
}


// End Waypoint
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Guardzone

void EcWidget::enableGuardZone(bool enable)
{
    qDebug() << "enableGuardZone called with" << enable;

    guardZoneActive = enable;

    if (!enable) {
        // Jika disable, clear semua legacy variables SAJA
        guardZoneShape = GUARD_ZONE_CIRCLE;  // Reset ke default
        guardZoneCenterLat = 0.0;
        guardZoneCenterLon = 0.0;
        guardZoneRadius = 0.5;
        guardZoneAttachedToShip = false;
        guardZoneLatLons.clear();

        qDebug() << "GuardZone system disabled and legacy variables cleared";

        if (guardZoneAutoCheckTimer->isActive()) {
                guardZoneAutoCheckTimer->stop();
                previousTargetsInZone.clear();
                qDebug() << "Auto-check stopped with GuardZone deactivation";
            }
    } else {
        // PERBAIKAN: Jangan ubah status active dari guardzone individual
        // Hanya set legacy variables untuk backward compatibility
        if (!guardZones.isEmpty()) {
            // Cari guardzone pertama yang aktif untuk legacy compatibility
            bool foundActive = false;
            for (const GuardZone& gz : guardZones) {
                if (gz.active) {
                    guardZoneShape = gz.shape;
                    guardZoneAttachedToShip = gz.attachedToShip;

                    if (gz.shape == GUARD_ZONE_CIRCLE) {
                        guardZoneCenterLat = gz.centerLat;
                        guardZoneCenterLon = gz.centerLon;
                        guardZoneRadius = gz.radius;
                    } else if (gz.shape == GUARD_ZONE_POLYGON) {
                        guardZoneLatLons = gz.latLons;
                    }

                    foundActive = true;
                    break;
                }
            }

            // Jika tidak ada yang aktif, gunakan yang pertama saja untuk legacy
            if (!foundActive && !guardZones.isEmpty()) {
                const GuardZone& firstGz = guardZones.first();
                guardZoneShape = firstGz.shape;
                guardZoneAttachedToShip = firstGz.attachedToShip;

                if (firstGz.shape == GUARD_ZONE_CIRCLE) {
                    guardZoneCenterLat = firstGz.centerLat;
                    guardZoneCenterLon = firstGz.centerLon;
                    guardZoneRadius = firstGz.radius;
                } else if (firstGz.shape == GUARD_ZONE_POLYGON) {
                    guardZoneLatLons = firstGz.latLons;
                }
            }

            qDebug() << "GuardZone system enabled with" << guardZones.size() << "guardzones";
        }

        if (guardZoneAutoCheckEnabled) {
                guardZoneAutoCheckTimer->start(guardZoneCheckInterval);
                qDebug() << "Auto-check resumed with GuardZone activation";
            }

            qDebug() << "GuardZone system enabled with" << guardZones.size() << "guardzones";
    }

    qDebug() << "guardZoneActive now" << guardZoneActive;
    update();  // Force redraw
}

void EcWidget::startCreateGuardZone(::GuardZoneShape shape)
{
    qDebug() << "Starting GuardZone creation with shape:" << shape;

    // Clear previous state
    guardZonePoints.clear();
    newGuardZoneShape = shape;
    creatingGuardZone = true;

    // ========== PERBAIKAN: MOUSE TRACKING CONSISTENCY ==========
    // Force enable mouse tracking
    setMouseTracking(true);

    // Get current mouse position immediately
    QPoint globalPos = QCursor::pos();
    currentMousePos = mapFromGlobal(globalPos);

    // Validate mouse position
    if (!rect().contains(currentMousePos)) {
        currentMousePos = QPoint(width()/2, height()/2);  // Center if outside
    }

    qDebug() << "Mouse tracking enabled - Initial pos:" << currentMousePos;
    qDebug() << "Widget rect:" << rect() << "Contains mouse:" << rect().contains(currentMousePos);
    // ===========================================================

    // Set cursor
    setCursor(Qt::CrossCursor);

    // Set status message
    QString message;
    if (shape == GUARD_ZONE_CIRCLE) {
        message = tr("Creating Circular GuardZone: Click to set center, then click again to set radius");
    } else {
        message = tr("Creating Polygon GuardZone: Click to add points, right-click to finish");
    }

    emit statusMessage(message);

    // Force immediate update
    update();

    qDebug() << "GuardZone creation mode activated";
}

void EcWidget::finishCreateGuardZone()
{
    qDebug() << "Finishing GuardZone creation";

    // ========== PERBAIKAN: ROBUST CLEANUP ==========
    // Reset creation mode variables
    creatingGuardZone = false;
    guardZonePoints.clear();

    // Reset mouse position to avoid stale data
    currentMousePos = QPoint(0, 0);

    // Restore cursor
    setCursor(Qt::ArrowCursor);

    // ========== CONDITIONAL MOUSE TRACKING ==========
    // Only disable mouse tracking if not needed by other systems
    bool needsMouseTracking = false;

    // Check if edit mode needs tracking
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        needsMouseTracking = true;
    }

    // Check if other systems need tracking (add more conditions as needed)
    // if (otherSystemNeedsTracking) {
    //     needsMouseTracking = true;
    // }

    if (!needsMouseTracking) {
        setMouseTracking(false);
        qDebug() << "Mouse tracking disabled - no active systems need it";
    } else {
        qDebug() << "Mouse tracking kept active - needed by other systems";
    }
    // ===============================================

    // Clear status message
    emit statusMessage(tr("GuardZone creation completed"));

    // Emit completion signal
    emit guardZoneCreated();

    // Final cleanup update
    update();

    qDebug() << "GuardZone creation finished successfully";
}

void EcWidget::cancelCreateGuardZone()
{
    qDebug() << "Cancelling GuardZone creation";

    // ========== PERBAIKAN: CONSISTENT CLEANUP ==========
    // Reset creation mode variables
    creatingGuardZone = false;
    guardZonePoints.clear();

    // Reset mouse position
    currentMousePos = QPoint(0, 0);

    // Restore cursor
    setCursor(Qt::ArrowCursor);

    // ========== SAME CONDITIONAL TRACKING AS FINISH ==========
    bool needsMouseTracking = false;

    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        needsMouseTracking = true;
    }

    if (!needsMouseTracking) {
        setMouseTracking(false);
    }
    // =====================================================

    // Show visual feedback
    showVisualFeedback(tr("GuardZone creation cancelled"), FEEDBACK_INFO);

    // Clear status message
    emit statusMessage(tr("GuardZone creation cancelled"));

    // Cleanup update
    update();

    qDebug() << "GuardZone creation cancelled";
}

void EcWidget::checkGuardZone()
{
    qDebug() << "EcWidget::checkGuardZone called";
    if (!guardZoneActive) {
        qDebug() << "GuardZone not active";
        QMessageBox::warning(this, tr("GuardZone"), tr("No active guard zone"));
        return;
    }

    qDebug() << "About to call highlightDangersInGuardZone()";
    highlightDangersInGuardZone();
}

void EcWidget::drawGuardZone(QPainter& painter)
{
    // Safety guards to avoid crashes in early frames
    if (!initialized || !view) return;
    QElapsedTimer timer; timer.start();

    if (!painter.isActive()) return;
    painter.setRenderHint(QPainter::Antialiasing, true);
    // Early exit jika tidak ada guardzone
    if (guardZones.isEmpty()) {
        if (creatingGuardZone) drawGuardZoneCreationPreview(painter);
        return;
    }
    int drawnCount = 0; int skippedCount = 0;
    // Viewport culling untuk performance
    QRect viewport = rect();
    // Precompute pixels-per-NM once per frame for efficiency
    double pixelsPerNMFactor = calculatePixelsFromNauticalMiles(1.0);
    if (qIsNaN(pixelsPerNMFactor) || qIsInf(pixelsPerNMFactor) || pixelsPerNMFactor <= 0.0) {
        pixelsPerNMFactor = 100.0;
    }

    // Copy to avoid concurrent modification during draw
    const QList<GuardZone> gzCopy = guardZones;
    for (const GuardZone &gz : gzCopy) {
        if (!gz.active && !creatingGuardZone) {
            skippedCount++;
            continue;
        }

        // TAMBAHAN: Viewport culling check - SKIP untuk attached guardzone
        if (!gz.attachedToShip && !isGuardZoneInViewport(gz, viewport)) {
            skippedCount++;
            continue;
        }

        bool isBeingEdited = (guardZoneManager && guardZoneManager->isEditingGuardZone() &&
                              gz.id == guardZoneManager->getEditingGuardZoneId());

        QPen pen(gz.color);
        pen.setWidth(isBeingEdited ? 4 : 2);
        if (isBeingEdited) {
            pen.setStyle(Qt::DashLine);
        }
        painter.setPen(pen);

        QColor fillColor = gz.color;
        fillColor.setAlpha(isBeingEdited ? 80 : 50);
        painter.setBrush(QBrush(fillColor));

        int labelX = 0, labelY = 0;

        if (gz.shape == GUARD_ZONE_CIRCLE) {
            double lat = gz.centerLat;
            double lon = gz.centerLon;
            // For attached guardzone, override with live ship position (no lag)
            if (gz.attachedToShip) {
                if (navShip.lat != 0.0 && navShip.lon != 0.0) { lat = navShip.lat; lon = navShip.lon; }
                else if (redDotTrackerEnabled && redDotLat != 0.0 && redDotLon != 0.0) { lat = redDotLat; lon = redDotLon; }
                else if (ownShip.lat != 0.0 && ownShip.lon != 0.0) { lat = ownShip.lat; lon = ownShip.lon; }
                if (qIsNaN(lat) || qIsNaN(lon) || (lat == 0.0 && lon == 0.0)) { lat = gz.centerLat; lon = gz.centerLon; }
            }
            int centerX, centerY;
            bool conversionResult = LatLonToXy(lat, lon, centerX, centerY);
            if (conversionResult) {
                if (abs(centerX) > 20000 || abs(centerY) > 20000) {
                    skippedCount++;
                    continue;
                }
                double circleRadiusNM = (gz.shape == GUARD_ZONE_CIRCLE) ? gz.radius : gz.outerRadius;
                double innerRadiusNM = (gz.shape == GUARD_ZONE_CIRCLE) ? 0.0 : gz.innerRadius; // Circle = solid
                double outerRadiusInPixels = pixelsPerNMFactor * circleRadiusNM;
                double innerRadiusInPixels = pixelsPerNMFactor * innerRadiusNM;

                if (qIsNaN(outerRadiusInPixels) || qIsInf(outerRadiusInPixels) ||
                    qIsNaN(innerRadiusInPixels) || qIsInf(innerRadiusInPixels)) {
                    skippedCount++;
                    continue;
                }

                // TAMBAHAN: Clamp radius untuk performance
                if (outerRadiusInPixels > 5000) outerRadiusInPixels = 5000;
                if (outerRadiusInPixels < 1) outerRadiusInPixels = 1;
                if (innerRadiusInPixels < 0) innerRadiusInPixels = 0;

                // AttachedToShip ⇒ always draw semicircle shield following heading
                bool isSemicircleShield = (gz.attachedToShip) ||
                                          (gz.shape != GUARD_ZONE_CIRCLE &&
                                           gz.startAngle != gz.endAngle &&
                                           gz.startAngle >= 0 && gz.endAngle >= 0);

                // Wrap all QPainter operations in try-catch
                try {
                    if (isSemicircleShield) {
                        // Create semicircle shield using sector rendering
                        QPainterPath semicirclePath;

                        // Determine angles: follow navShip heading if attached
                        double useHeading = (!qIsNaN(navShip.heading) && !qIsInf(navShip.heading) && navShip.heading != 0.0)
                                            ? navShip.heading : ownShip.heading;
                        double useStart = gz.startAngle;
                        double useEnd   = gz.endAngle;
                        if (gz.attachedToShip) {
                            useStart = fmod(useHeading + 90.0, 360.0);
                            useEnd   = fmod(useHeading + 270.0, 360.0);
                        }
                        // Convert angles from navigation (0=North, clockwise) to Qt (0=East, CCW)
                        double qtStartAngle = (90 - useStart + 360) * 16;    // Qt uses 1/16 degrees
                        double qtEndAngle   = (90 - useEnd   + 360) * 16;
                        double qtSpanAngle = (qtEndAngle - qtStartAngle);

                        // Normalize angles
                        qtStartAngle = fmod(qtStartAngle, 360 * 16);
                        if (qtSpanAngle < 0) qtSpanAngle += 360 * 16;
                        if (qtSpanAngle > 360 * 16) qtSpanAngle -= 360 * 16;

                        double startAngleQt = qtStartAngle / 16.0;
                        double spanAngleQt  = qtSpanAngle  / 16.0;

                        double rectX = centerX - outerRadiusInPixels;
                        double rectY = centerY - outerRadiusInPixels;
                        double rectSize = 2 * outerRadiusInPixels;
                        if (qIsNaN(rectX) || qIsInf(rectX) || qIsNaN(rectY) || qIsInf(rectY) ||
                            qIsNaN(rectSize) || qIsInf(rectSize) || rectSize <= 0) continue;

                        QRectF outerRect(rectX, rectY, rectSize, rectSize);

                        // Start from center
                        semicirclePath.moveTo(centerX, centerY);

                        // Draw arc
                        semicirclePath.arcTo(outerRect, startAngleQt, spanAngleQt);

                        // Close back to center
                        semicirclePath.closeSubpath();

                        painter.drawPath(semicirclePath);

                    } else {
                        // Create full circle or donut shape using QPainterPath
                        QPainterPath circlePath;

                        // CRITICAL: Validate ellipse parameters
                        QPointF centerPoint(centerX, centerY);
                        if (qIsNaN(centerPoint.x()) || qIsInf(centerPoint.x()) ||
                            qIsNaN(centerPoint.y()) || qIsInf(centerPoint.y())) continue;

                        circlePath.addEllipse(centerPoint, outerRadiusInPixels, outerRadiusInPixels);

                        if (innerRadiusInPixels > 0) {
                            circlePath.addEllipse(centerPoint, innerRadiusInPixels, innerRadiusInPixels);
                        }
                        painter.drawPath(circlePath);
                    }

                    labelX = centerX;
                    labelY = centerY - static_cast<int>(outerRadiusInPixels) - 15;
                    drawnCount++;
                } catch (const std::exception& e) {
                    skippedCount++;
                    continue;
                } catch (...) {
                    skippedCount++;
                    continue;
                }
            }
        }
        else if (gz.shape == GUARD_ZONE_POLYGON && gz.latLons.size() >= 6) {
            // PERBAIKAN CRITICAL: Wrap polygon drawing in try-catch
            try {
                QPolygon poly;
                bool validPolygon = true;

                for (int i = 0; i < gz.latLons.size(); i += 2) {
                    int x, y;
                    if (LatLonToXy(gz.latLons[i], gz.latLons[i+1], x, y)) {
                        // CRITICAL: Validate polygon coordinates
                        if (abs(x) > 20000 || abs(y) > 20000) {
                            qDebug() << "[DRAW-GUARDZONE-ERROR] Invalid polygon coordinate:" << x << "," << y << "- skipping polygon";
                            validPolygon = false;
                            break;
                        }
                        poly.append(QPoint(x, y));
                    } else {
                        validPolygon = false;
                        break;
                    }
                }

                if (validPolygon && poly.size() >= 3) {
                    painter.drawPolygon(poly);

                    QRect boundingRect = poly.boundingRect();
                    labelX = boundingRect.center().x();
                    labelY = boundingRect.top() - 15;
                    drawnCount++;
                }
            } catch (const std::exception& e) {
                qDebug() << "[DRAW-GUARDZONE-ERROR] Exception drawing polygon guardzone" << gz.id << ":" << e.what();
                skippedCount++;
                continue;
            } catch (...) {
                qDebug() << "[DRAW-GUARDZONE-ERROR] Unknown exception drawing polygon guardzone" << gz.id;
                skippedCount++;
                continue;
            }
        }
        // SECTOR shape is no longer used - converted to donut circle

        // Draw label jika ada posisi valid
        if (labelX != 0 && labelY != 0) {
            qDebug() << "[DRAW-GUARDZONE-DEBUG] Drawing label at position" << labelX << "," << labelY;
            try {
                drawGuardZoneLabel(painter, gz, QPoint(labelX, labelY));
                qDebug() << "[DRAW-GUARDZONE-DEBUG] Label drawn successfully for guardzone" << gz.id;
            } catch (...) {
                qDebug() << "[DRAW-GUARDZONE-ERROR] Exception drawing label for guardzone" << gz.id;
            }
        } else {
            qDebug() << "[DRAW-GUARDZONE-DEBUG] No valid label position for guardzone" << gz.id;
        }

        qDebug() << "[DRAW-GUARDZONE-DEBUG] ===== FINISHED PROCESSING GUARDZONE" << gz.id << "=====";
    }

    qDebug() << "[DRAW-GUARDZONE-DEBUG] ========== GUARDZONE ITERATION COMPLETE ==========";
    qDebug() << "[DRAW-GUARDZONE-DEBUG] Total processed:" << (drawnCount + skippedCount) << "drawn:" << drawnCount << "skipped:" << skippedCount;

    // Draw creation preview
    qDebug() << "[DRAW-GUARDZONE-DEBUG] Checking creation preview...";
    if (creatingGuardZone) {
        qDebug() << "[DRAW-GUARDZONE-DEBUG] Drawing creation preview...";
        drawGuardZoneCreationPreview(painter);
        qDebug() << "[DRAW-GUARDZONE-DEBUG] Creation preview drawn";
    }

    // Draw edit overlay
    qDebug() << "[DRAW-GUARDZONE-DEBUG] Checking edit overlay...";
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        qDebug() << "[DRAW-GUARDZONE-DEBUG] Drawing edit overlay...";
        guardZoneManager->drawEditOverlay(painter);
        qDebug() << "[DRAW-GUARDZONE-DEBUG] Edit overlay drawn";
    }

    qDebug() << "[DRAW-GUARDZONE-DEBUG] Checking feedback overlay...";
    // Draw feedback overlay
    if (feedbackTimer.isActive()) {
        drawFeedbackOverlay(painter);
    }

    qDebug() << "[DRAW-GUARDZONE-DEBUG] Ending QPainter...";
    painter.end();
    qDebug() << "[DRAW-GUARDZONE-DEBUG] QPainter ended successfully";

    // TAMBAHAN: Performance logging
    qint64 elapsed = timer.elapsed();
    qDebug() << "[DRAW-GUARDZONE-DEBUG] Performance: drawGuardZone took" << elapsed << "ms"
             << "- Drawn:" << drawnCount << "Skipped:" << skippedCount;
    if (elapsed > 50) {  // Log jika lebih dari 50ms
        qDebug() << "[PERF] drawGuardZone took" << elapsed << "ms"
                 << "- Drawn:" << drawnCount << "Skipped:" << skippedCount;
    }

    qDebug() << "[DRAW-GUARDZONE-DEBUG] ========== DRAW GUARDZONE COMPLETE ==========";
}

void EcWidget::createCircularGuardZone(double centerLat, double centerLon, double radiusNM)
{
    // Simpan informasi lingkaran sebagai koordinat geografis
    guardZoneCenterLat = centerLat;
    guardZoneCenterLon = centerLon;
    guardZoneRadius = radiusNM;

    // Aktifkan guardzone
    guardZoneActive = true;
    guardZoneShape = GUARD_ZONE_CIRCLE;

    // Default: tidak melekat pada kapal
    guardZoneAttachedToShip = false;

    // Redraw chart
    update();
}

void EcWidget::createPolygonGuardZone()
{
    if (guardZonePoints.size() < 3) {
        QMessageBox::warning(this, tr("GuardZone"), tr("Not enough points for polygon guard zone"));
        return;
    }

    // Konversi titik-titik layar ke koordinat geografis
    guardZoneLatLons.clear();
    for (int i = 0; i < guardZonePoints.size(); i++) {
        double lat, lon;
        if (XyToLatLon(guardZonePoints[i].x(), guardZonePoints[i].y(), lat, lon)) {
            guardZoneLatLons.append(lat);
            guardZoneLatLons.append(lon);
        }
    }

    // Aktifkan guardzone
    guardZoneActive = true;
    guardZoneShape = GUARD_ZONE_POLYGON;

    // Default: tidak melekat pada kapal
    guardZoneAttachedToShip = false;

    // Redraw chart
    update();
}

void EcWidget::highlightDangersInGuardZone()
{
    qDebug() << "EcWidget::highlightDangersInGuardZone called";

    if (!guardZoneActive) {
        QMessageBox::warning(this, tr("GuardZone"), tr("No active guard zone"));
        return;
    }

    // Siapkan daftar objek terdeteksi
    QList<DetectedObject> detectedObjects;

    // Objek di dalam GuardZone (berdasarkan gambar)
    {
        DetectedObject obj;
        obj.type = "WRECKS";
        obj.name = "Wreck";
        obj.description = "Dangerous wreck, depth 5m";
        obj.level = 4; // Danger
        obj.lat = guardZoneCenterLat - 0.001;  // Posisi relatif terhadap pusat GuardZone
        obj.lon = guardZoneCenterLon + 0.002;
        detectedObjects.append(obj);
    }

    {
        DetectedObject obj;
        obj.type = "CTNARE";
        obj.name = "Caution Area";
        obj.description = "Reason: Underwater operations";
        obj.level = 3; // Warning
        obj.lat = guardZoneCenterLat;
        obj.lon = guardZoneCenterLon;
        detectedObjects.append(obj);
    }

    {
        DetectedObject obj;
        obj.type = "BOYISD";
        obj.name = "Buoy, Isolated Danger";
        obj.description = "Marks isolated danger";
        obj.level = 3; // Warning
        obj.lat = guardZoneCenterLat + 0.001;
        obj.lon = guardZoneCenterLon - 0.001;
        detectedObjects.append(obj);
    }

    {
        DetectedObject obj;
        obj.type = "DEPCNT";
        obj.name = "Depth Contour";
        obj.description = "Value: 10m";
        obj.level = 3; // Warning
        obj.lat = guardZoneCenterLat - 0.0005;
        obj.lon = guardZoneCenterLon - 0.002;
        detectedObjects.append(obj);
    }

    {
        DetectedObject obj;
        obj.type = "TSSBND";
        obj.name = "Traffic Separation Scheme Boundary";
        obj.description = "Indicates traffic lanes";
        obj.level = 3; // Warning
        obj.lat = guardZoneCenterLat + 0.002;
        obj.lon = guardZoneCenterLon + 0.001;
        detectedObjects.append(obj);
    }

    {
        DetectedObject obj;
        obj.type = "BOYLAT";
        obj.name = "Buoy, Lateral";
        obj.description = "Port hand buoy";
        obj.level = 2; // Note
        obj.lat = guardZoneCenterLat + 0.0015;
        obj.lon = guardZoneCenterLon - 0.0005;
        detectedObjects.append(obj);
    }

    {
        DetectedObject obj;
        obj.type = "SOUNDG";
        obj.name = "Sounding";
        obj.description = "Depth: 8.2m";
        obj.level = 2; // Note
        obj.lat = guardZoneCenterLat - 0.0015;
        obj.lon = guardZoneCenterLon + 0.0008;
        detectedObjects.append(obj);
    }

    {
        DetectedObject obj;
        obj.type = "ACHARE";
        obj.name = "Anchorage Area";
        obj.description = "Designated anchoring area";
        obj.level = 1; // Info
        obj.lat = guardZoneCenterLat + 0.0008;
        obj.lon = guardZoneCenterLon + 0.0015;
        detectedObjects.append(obj);
    }

    // Buat dan tampilkan dialog
    GuardZoneCheckDialog dialog(this);

    // Isi data dialog
    if (guardZoneShape == GUARD_ZONE_CIRCLE) {
        dialog.setData(detectedObjects, guardZoneRadius, true, 0);
    } else if (guardZoneShape == GUARD_ZONE_POLYGON) {
        dialog.setData(detectedObjects, 0.0, false, guardZoneLatLons.size() / 2);
    }

    // Hubungkan signal untuk berpusat pada objek yang dipilih
    connect(&dialog, &GuardZoneCheckDialog::objectSelected,
            [this](double lat, double lon, bool zoom) {
                SetCenter(lat, lon);

                // Jika zoom, sesuaikan skala peta untuk fokus lebih baik
                if (zoom && currentScale > 10000) {
                    // Kurangi nilai untuk zoom in
                    SetScale(currentScale / 2);
                }

                Draw();
                // highlightSelectedObject(lat, lon);
            });

    // Tampilkan dialog
    qDebug() << "Showing GuardZone Check dialog";
    dialog.exec();
    qDebug() << "GuardZone Check dialog closed";

    // Highlight guardzone area untuk menunjukkan bahwa pemeriksaan telah dilakukan
    update();
}

double EcWidget::calculatePixelsFromNauticalMiles(double nauticalMiles)
{
    // Hitung jarak 1 mil laut dalam pixel
    double lat1, lon1, lat2, lon2;

    // Ambil koordinat saat ini
    lat1 = currentLat;
    lon1 = currentLon;

    // Cari koordinat 1 mil laut ke timur
    EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84, lat1, lon1, 1.0, 90.0, &lat2, &lon2);

    // Konversi ke pixel
    int x1, y1, x2, y2;
    if (LatLonToXy(lat1, lon1, x1, y1) && LatLonToXy(lat2, lon2, x2, y2)) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        double pixelsPerNM = sqrt(dx*dx + dy*dy);
        return pixelsPerNM * nauticalMiles;
    }

    // Default jika konversi gagal
    return 100 * nauticalMiles;
}

void EcWidget::setGuardZoneAttachedToShip(bool attached)
{
    guardZoneAttachedToShip = attached;
    update();
}

// Fungsi Simulasi AIS
void EcWidget::startAISTargetSimulation()
{
    // Tampilkan dialog pilihan skenario
    QStringList scenarios;
    scenarios << tr("Scenario 1: Static Guard Zone with Approaching Vessels")
              << tr("Scenario 2: Moving Guard Zone (Following Ship) with Fixed Obstacles");

    bool ok;
    QString selectedScenario = QInputDialog::getItem(this, tr("Select Simulation Scenario"),
                                                     tr("Choose a simulation scenario:"), scenarios, 0, false, &ok);

    if (!ok) {
        return; // Pengguna membatalkan dialog
    }

    // Tentukan skenario berdasarkan pilihan
    if (selectedScenario.contains("Scenario 1")) {
        startAISTargetSimulationStatic();
    } else {
        startAISTargetSimulationMoving();
    }
}

void EcWidget::generateTargetsTowardsGuardZone(int count)
{
    simulatedTargets.clear();

    // Batasi jumlah target maksimum menjadi 5
    if (count > 5) count = 5;

    // ========== PERBAIKAN: CARI GUARDZONE AKTIF ==========
    GuardZone* activeGuardZone = nullptr;

    // Cari guardzone aktif pertama
    for (GuardZone& gz : guardZones) {
        if (gz.active) {
            activeGuardZone = &gz;
            qDebug() << "[SIMULATION] Using active GuardZone:" << gz.name << "ID:" << gz.id;
            break;
        }
    }

    // Jika tidak ada guardzone aktif, tampilkan pesan dan keluar
    if (!activeGuardZone) {
        qDebug() << "[SIMULATION] No active GuardZone found for simulation";
        QMessageBox::warning(this, tr("Simulation Warning"),
                             tr("No active GuardZone found!\nPlease enable at least one GuardZone for simulation."));
        return;
    }
    // ===================================================

    // Dapatkan posisi guardzone aktif
    double guardZoneLat, guardZoneLon;
    double effectiveRadius = 0.5; // Default minimal jarak 0.5 mil laut

    if (activeGuardZone->shape == GUARD_ZONE_CIRCLE) {
        // ========== GUNAKAN DATA DARI GUARDZONE AKTIF ==========
        guardZoneLat = activeGuardZone->centerLat;
        guardZoneLon = activeGuardZone->centerLon;
        effectiveRadius = activeGuardZone->radius * 1.2; // Pastikan di luar guardzone

        qDebug() << "[SIMULATION] Target: Circle at" << guardZoneLat << guardZoneLon
                 << "radius" << activeGuardZone->radius;
        // =====================================================
    }
    else if (activeGuardZone->shape == GUARD_ZONE_POLYGON && activeGuardZone->latLons.size() >= 2) {
        // ========== GUNAKAN DATA DARI GUARDZONE AKTIF ==========
        // Hitung pusat polygon dari guardzone aktif
        double centerLat = 0, centerLon = 0;
        int numPoints = activeGuardZone->latLons.size() / 2;

        for (int i = 0; i < activeGuardZone->latLons.size(); i += 2) {
            centerLat += activeGuardZone->latLons[i];
            centerLon += activeGuardZone->latLons[i+1];
        }
        centerLat /= numPoints;
        centerLon /= numPoints;

        // Hitung radius efektif dari polygon aktif
        double totalDist = 0.0;
        for (int i = 0; i < activeGuardZone->latLons.size(); i += 2) {
            double dist, dummy;
            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                   centerLat, centerLon,
                                                   activeGuardZone->latLons[i],
                                                   activeGuardZone->latLons[i+1],
                                                   &dist, &dummy);
            totalDist += dist;
        }
        effectiveRadius = (totalDist / numPoints) * 1.5; // Pastikan di luar polygon
        guardZoneLat = centerLat;
        guardZoneLon = centerLon;

        qDebug() << "[SIMULATION] Target: Polygon at" << guardZoneLat << guardZoneLon
                 << "effective radius" << effectiveRadius;
        // ====================================================
    }
    else {
        // Fallback jika data guardzone tidak valid
        GetCenter(guardZoneLat, guardZoneLon);
        qDebug() << "[SIMULATION] Fallback to current center position";
    }

    // Pastikan effectiveRadius minimal 0.5 mil laut
    if (effectiveRadius < 0.5) effectiveRadius = 0.5;

    // ========== TAMBAHAN: TAMPILKAN INFO GUARDZONE YANG DIGUNAKAN ==========
    QString targetInfo = QString("Simulating %1 vessels approaching active GuardZone:\n'%2' (%3)")
                             .arg(count)
                             .arg(activeGuardZone->name)
                             .arg(activeGuardZone->shape == GUARD_ZONE_CIRCLE ? "Circle" : "Polygon");

    qDebug() << "[SIMULATION]" << targetInfo;
    // ===================================================================

    // Distribusikan target di sekitar guardzone
    double angleStep = 360.0 / count;

    for (int i = 0; i < count; ++i) {
        SimulatedAISTarget target;

        // Setiap kapal memiliki arah yang berbeda sekitar guardzone
        double startBearing = i * angleStep + (qrand() % 20 - 10);

        // Kecepatan bervariasi antara 25-45 knot
        target.sog = 25.0 + (i * 5.0); // 25, 30, 35, 40, 45 knot

        // Target kita ingin kapal mencapai guardzone dalam 0.5-1.7 detik
        double desiredTimeSeconds = 0.5 + (i * 0.3); // 0.5, 0.8, 1.1, 1.4, 1.7 detik

        // Konversi ke jam
        double desiredTimeHours = desiredTimeSeconds / 3600.0;

        // Jarak = kecepatan * waktu
        double startDistance = target.sog * desiredTimeHours + effectiveRadius;

        // Hitung posisi awal berdasarkan jarak dan arah
        EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                     guardZoneLat, guardZoneLon,
                                     startDistance, startBearing,
                                     &target.lat, &target.lon);

        // Arah menuju ke guardzone (berlawanan dengan startBearing)
        target.cog = fmod(startBearing + 180.0, 360.0);

        // MMSI dengan info guardzone
        target.mmsi = QString("FAST%1→GZ%2").arg(i+1).arg(activeGuardZone->id);
        target.dangerous = false;

        simulatedTargets.append(target);
    }

    // ========== TAMBAHAN: FEEDBACK KEPADA USER ==========
    emit statusMessage(QString("Simulation started: %1 vessels → %2")
                           .arg(count)
                           .arg(activeGuardZone->name));
    // ==================================================
}

void EcWidget::stopAISTargetSimulation()
{
    if (!simulationActive) {
        QMessageBox::information(this, tr("Simulation"), tr("No simulation is running"));
        return;
    }

    if (simulationTimer) {
        simulationTimer->stop();
    }

    if (ownShipTimer && ownShipInSimulation) {
        ownShipTimer->stop();
    }

    simulationActive = false;
    ownShipInSimulation = false;
    simulatedTargets.clear();

    // Jika ini adalah skenario 2, kembalikan flag guardzone terikat
    if (currentScenario == SCENARIO_MOVING_GUARDZONE) {
        setGuardZoneAttachedToShip(false);
    }

    // Kembalikan tampilan ke kondisi normal
    drawPixmap = chartPixmap.copy();
    update();

    QMessageBox::information(this, tr("Simulation Stopped"),
                             tr("AIS target simulation has been stopped."));
}

void EcWidget::updateSimulatedTargets()
{
    if (!simulationActive)
        return;

    QDateTime currentTime = QDateTime::currentDateTime();
    double elapsedSeconds = lastSimulationTime.msecsTo(currentTime) / 1000.0;
    lastSimulationTime = currentTime;

    // Update posisi semua target berdasarkan kecepatan dan arah
    for (int i = 0; i < simulatedTargets.size(); ++i) {
        // Update posisi berdasarkan kecepatan dan arah
        double distanceNM = simulatedTargets[i].sog * elapsedSeconds / 3600.0; // Konversi ke jam

        // Hitung posisi baru
        double lat, lon;
        EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                     simulatedTargets[i].lat,
                                     simulatedTargets[i].lon,
                                     distanceNM, simulatedTargets[i].cog,
                                     &lat, &lon);

        simulatedTargets[i].lat = lat;
        simulatedTargets[i].lon = lon;
    }

    // Jika auto check guardzone diaktifkan, periksa target
    if (autoCheckGuardZone && guardZoneActive) {
        checkTargetsInGuardZone();
    } else {
        // Redraw chart dengan target yang diupdate
        drawSimulatedTargets();
    }
}

void EcWidget::drawSimulatedTargets()
{
    if (!simulationActive)
        return;

    // Pastikan kita mulai dengan chart yang bersih
    drawPixmap = chartPixmap.copy(); // Gunakan salinan bersih dari chart

    // Gambar target simulasi ke drawPixmap
    QPainter painter(&drawPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const SimulatedAISTarget &target : simulatedTargets) {
        int x, y;
        if (LatLonToXy(target.lat, target.lon, x, y)) {
            // Gambar segitiga untuk menandakan kapal
            QPolygon triangle;
            triangle << QPoint(x, y-10) << QPoint(x-7, y+5) << QPoint(x+7, y+5);

            // Target berbahaya berwarna merah, yang lain hijau
            if (target.dangerous) {
                painter.setBrush(QColor(255, 0, 0, 200)); // Merah
            } else {
                painter.setBrush(QColor(0, 200, 0, 200)); // Hijau
            }

            painter.setPen(Qt::black);
            painter.drawPolygon(triangle);

            // Gambar arah (COG)
            double cogRad = target.cog * M_PI / 180.0;
            int lineLength = 20;
            int cogX = x + qRound(sin(cogRad) * lineLength);
            int cogY = y - qRound(cos(cogRad) * lineLength);
            painter.drawLine(x, y, cogX, cogY);

            // Tambahkan MMSI dan kecepatan
            painter.setPen(Qt::black);
            painter.setFont(QFont("Arial", 8));
            painter.drawText(x + 10, y, QString("%1\n%2kt")
                                            .arg(target.mmsi)
                                            .arg(target.sog, 0, 'f', 1));
        }
    }

    painter.end();
    update(); // Picu repaint untuk menampilkan drawPixmap yang diperbarui
}

void EcWidget::generateRandomAISTargets(int count)
{
    simulatedTargets.clear();

    // Dapatkan batas tampilan saat ini
    double centerLat, centerLon;
    GetCenter(centerLat, centerLon);
    double range = GetRange(currentScale);

    // Generate target acak di sekitar pusat tampilan
    for (int i = 0; i < count; ++i) {
        SimulatedAISTarget target;

        // Generate posisi acak
        double distance = (qrand() % 100) / 100.0 * range / 60.0; // Jarak dalam mil laut
        double bearing = (qrand() % 360); // Arah dalam derajat

        EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                     centerLat, centerLon,
                                     distance, bearing,
                                     &target.lat, &target.lon);

        // Generate kecepatan dan arah acak
        target.cog = qrand() % 360; // Arah dalam derajat
        target.sog = 5.0 + (qrand() % 150) / 10.0; // Kecepatan 5-20 knot

        // Generate MMSI acak
        target.mmsi = QString("AIS%1").arg(i+1);
        target.dangerous = false;

        simulatedTargets.append(target);
    }
}

bool EcWidget::checkTargetsInGuardZone()
{
    if (!guardZoneActive) {
        qDebug() << "[REAL-AIS-CHECK] No active GuardZone";
        return false;
    }

    if (!Ais::instance()) {
        qDebug() << "[REAL-AIS-CHECK] AIS instance not available";
        return false;
    }

    QMap<unsigned int, AISTargetData>& aisTargetMap = Ais::instance()->getTargetMap();

    if (aisTargetMap.isEmpty()) {
        qDebug() << "[REAL-AIS-CHECK] No AIS targets available";
        return false;
    }

    qDebug() << "[REAL-AIS-CHECK] Processing" << aisTargetMap.size() << "real AIS targets";

    // Cari guardzone aktif
    GuardZone* activeGuardZone = nullptr;
    for (GuardZone& gz : guardZones) {
        if (gz.active) {
            activeGuardZone = &gz;
            break;
        }
    }

    if (!activeGuardZone) {
        qDebug() << "[REAL-AIS-CHECK] No active GuardZone found";
        return false;
    }

    // ========== DEBUG GUARDZONE INFO ==========
    qDebug() << "[REAL-AIS-CHECK] Active GuardZone:" << activeGuardZone->name;
    qDebug() << "[REAL-AIS-CHECK] Shape:" << (activeGuardZone->shape == GUARD_ZONE_CIRCLE ? "CIRCLE" : "POLYGON");

    if (activeGuardZone->shape == GUARD_ZONE_POLYGON) {
        qDebug() << "[REAL-AIS-CHECK] Polygon coordinates count:" << activeGuardZone->latLons.size();
        qDebug() << "[REAL-AIS-CHECK] Polygon vertices:" << (activeGuardZone->latLons.size() / 2);

        // Debug polygon coordinates
        for (int i = 0; i < activeGuardZone->latLons.size(); i += 2) {
            qDebug() << "[REAL-AIS-CHECK] Vertex" << (i/2) << ":"
                     << activeGuardZone->latLons[i] << "," << activeGuardZone->latLons[i+1];
        }
    }
    // ========================================

    bool foundNewDanger = false;
    QString alertMessages;
    int alertCount = 0;

    for (auto it = aisTargetMap.begin(); it != aisTargetMap.end(); ++it) {
        const AISTargetData& aisTarget = it.value();

        // Skip invalid targets
        if (aisTarget.mmsi.isEmpty() || aisTarget.lat == 0.0 || aisTarget.lon == 0.0) {
            continue;
        }

        qDebug() << "[REAL-AIS-CHECK] Checking target" << aisTarget.mmsi
                 << "at" << aisTarget.lat << "," << aisTarget.lon;

        bool inGuardZone = false;

        // Check apakah target di dalam guardzone
        if (activeGuardZone->shape == GUARD_ZONE_CIRCLE) {
            double distance, bearing;
            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                   activeGuardZone->centerLat,
                                                   activeGuardZone->centerLon,
                                                   aisTarget.lat, aisTarget.lon,
                                                   &distance, &bearing);

            // For shield guardzone: check if target is within radius
            bool withinRadiusRange = (distance <= activeGuardZone->outerRadius);
            if (activeGuardZone->innerRadius > 0) {
                // If has inner radius, check donut range
                withinRadiusRange = (distance <= activeGuardZone->outerRadius && distance >= activeGuardZone->innerRadius);
            }

            // Check if this is a semicircle shield (has angle restrictions)
            bool isSemicircleShield = (activeGuardZone->startAngle != activeGuardZone->endAngle &&
                                       activeGuardZone->startAngle >= 0 && activeGuardZone->endAngle >= 0);

            if (isSemicircleShield && withinRadiusRange) {
                // Use geometric method like polygon detection for accuracy
                inGuardZone = isPointInSemicircle(aisTarget.lat, aisTarget.lon, activeGuardZone);

                qDebug() << "[REAL-AIS-CHECK] Semicircle shield - Using geometric detection";
            } else {
                inGuardZone = withinRadiusRange;
            }

            qDebug() << "[REAL-AIS-CHECK] Target" << aisTarget.mmsi
                     << "distance:" << distance << "vs inner:" << activeGuardZone->innerRadius
                     << "outer:" << activeGuardZone->outerRadius << "inZone:" << inGuardZone;
        }
        else if (activeGuardZone->shape == GUARD_ZONE_POLYGON && activeGuardZone->latLons.size() >= 6) {
            qDebug() << "[REAL-AIS-CHECK] Checking polygon GuardZone";
            qDebug() << "[REAL-AIS-CHECK] Polygon vertices:" << (activeGuardZone->latLons.size() / 2);

            // Debug polygon coordinates (hanya untuk troubleshooting)
            for (int i = 0; i < activeGuardZone->latLons.size() && i < 10; i += 2) { // Limit debug output
                qDebug() << "[REAL-AIS-CHECK] Vertex" << (i/2) << ":"
                         << activeGuardZone->latLons[i] << "," << activeGuardZone->latLons[i+1];
            }

            // Use improved polygon detection
            inGuardZone = isPointInPolygon(aisTarget.lat, aisTarget.lon, activeGuardZone->latLons);

            qDebug() << "[REAL-AIS-CHECK] Target" << aisTarget.mmsi
                     << "polygon check result:" << inGuardZone;
        }

        // Jika target di dalam guardzone, tambahkan ke alert
        if (inGuardZone) {
            foundNewDanger = true;

            alertMessages += tr("AIS Target %1: Speed %2 knots, Course %3°\n")
                                .arg(aisTarget.mmsi)
                                .arg(aisTarget.sog, 0, 'f', 1)
                                .arg(aisTarget.cog, 0, 'f', 0);
            alertCount++;

            qDebug() << "[REAL-AIS-CHECK] ⚠️ DANGER: AIS Target" << aisTarget.mmsi
                     << "entered GuardZone" << activeGuardZone->name
                     << "SOG:" << aisTarget.sog << "COG:" << aisTarget.cog;
        }
    }

    // Show alert jika ada target dalam guardzone
    if (foundNewDanger && alertCount > 0) {
        QString title = tr("Guard Zone Alert - %1 Target(s) in %2").arg(alertCount).arg(activeGuardZone->name);
        QMessageBox::warning(this, title, alertMessages);

        qDebug() << "[REAL-AIS-CHECK] Alert shown for" << alertCount << "targets";
    } else {
        qDebug() << "[REAL-AIS-CHECK] No targets detected in GuardZone";
    }

    return foundNewDanger;
}

void EcWidget::setAutoCheckGuardZone(bool enable)
{
    autoCheckGuardZone = enable;

    if (enable && simulationActive && guardZoneActive) {
        // Periksa target saat ini saat fitur diaktifkan
        checkTargetsInGuardZone();
    }
}

void EcWidget::startAISTargetSimulationStatic()
{
    if (simulationActive) {
        QMessageBox::information(this, tr("Simulation"), tr("Simulation is already running"));
        return;
    }

    // Cek apakah guardzone aktif
    if (!guardZoneActive) {
        QMessageBox::warning(this, tr("Simulation"),
                             tr("Please create and enable a Guard Zone first."));
        return;
    }

    // Inisialisasi timer jika belum ada
    if (!simulationTimer) {
        simulationTimer = new QTimer(this);
        connect(simulationTimer, &QTimer::timeout, this, &EcWidget::updateSimulatedTargets);
    }

    // Pastikan chart diperbarui sebelum simulasi dimulai
    Draw();

    // Simpan chart saat ini untuk referensi
    chartPixmap = drawPixmap.copy();

    // Set skenario saat ini
    currentScenario = SCENARIO_STATIC_GUARDZONE;

    // Generate target yang mengarah ke guardzone
    generateTargetsTowardsGuardZone(5); // Generate 5 target mengarah ke guardzone

    simulationActive = true;
    lastSimulationTime = QDateTime::currentDateTime();
    simulationTimer->start(100); // Update lebih cepat (10 kali per detik) untuk animasi lebih halus

    QMessageBox::information(this, tr("Simulation Started"),
                             tr("Static Guard Zone Simulation has started with 5 vessels approaching the guard zone.\n\n"
                                "The vessels will take between 1-5 seconds to reach the guard zone,\n"
                                "with different speeds ranging from 10 to 20 knots."));
}

void EcWidget::startAISTargetSimulationMoving()
{
    if (simulationActive) {
        QMessageBox::information(this, tr("Simulation"), tr("Simulation is already running"));
        return;
    }

    // Cek apakah guardzone aktif
    if (!guardZoneActive) {
        QMessageBox::warning(this, tr("Simulation"),
                             tr("Please create and enable a Guard Zone first."));
        return;
    }

    // Pastikan guardzone terikat ke kapal
    setGuardZoneAttachedToShip(true);

    // Inisialisasi timer jika belum ada
    if (!simulationTimer) {
        simulationTimer = new QTimer(this);
        connect(simulationTimer, &QTimer::timeout, this, &EcWidget::updateSimulatedTargets);
    }

    // Inisialisasi timer untuk pergerakan kapal
    if (!ownShipTimer) {
        ownShipTimer = new QTimer(this);
        connect(ownShipTimer, &QTimer::timeout, this, &EcWidget::updateOwnShipPosition);
    }

    // Pastikan chart diperbarui sebelum simulasi dimulai
    Draw();

    // Simpan chart saat ini untuk referensi
    chartPixmap = drawPixmap.copy();

    // Set skenario saat ini
    currentScenario = SCENARIO_MOVING_GUARDZONE;

    // Inisialisasi kapal sendiri
    ownShipInSimulation = true;
    ownShipSimCourse = qrand() % 360; // Arah acak
    ownShipSimSpeed = 5.0 + (qrand() % 50) / 10.0; // 5-10 knot

    // Generate rintangan statis di sekitar area
    generateStaticObstacles(5);

    simulationActive = true;
    lastSimulationTime = QDateTime::currentDateTime();
    simulationTimer->start(100); // Update 10 kali per detik
    ownShipTimer->start(500);    // Update posisi kapal 2 kali per detik

    QMessageBox::information(this, tr("Simulation Started"),
                             tr("Moving Guard Zone Simulation has started with 5 static obstacles.\n\n"
                                "Your ship is moving at %.1f knots with course %.0f degrees.\n"
                                "The guard zone will follow your ship's movement.\n"
                                "Warnings will appear when obstacles enter the guard zone.")
                                 .arg(ownShipSimSpeed)
                                 .arg(ownShipSimCourse));
}

void EcWidget::generateStaticObstacles(int count)
{
    simulatedTargets.clear();

    // Dapatkan posisi kapal
    double shipLat = ownShip.lat;
    double shipLon = ownShip.lon;

    // Batas area
    double viewRange = GetRange(currentScale) / 60.0; // Konversi ke mil laut
    double obstacleRange = viewRange * 0.7; // 70% dari range viewport

    // Distribusikan rintangan dalam bidang tertentu
    for (int i = 0; i < count; ++i) {
        SimulatedAISTarget target;

        // Jarak dan arah acak, tapi jangan terlalu dekat dengan kapal
        double distance = obstacleRange * 0.3 + (qrand() % 70) / 100.0 * obstacleRange;
        double bearing = (qrand() % 360);

        // Hitung posisi
        EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                     shipLat, shipLon,
                                     distance, bearing,
                                     &target.lat, &target.lon);

        // Arah dan kecepatan (obstacles statis, tapi tetap harus memiliki nilai)
        target.cog = 0;
        target.sog = 0;

        // MMSI
        target.mmsi = QString("OBS%1").arg(i+1);
        target.dangerous = false;

        simulatedTargets.append(target);
    }
}

void EcWidget::updateOwnShipPosition()
{
    if (!simulationActive || !ownShipInSimulation)
        return;

    // Waktu yang telah berlalu sejak update terakhir (dalam detik)
    QDateTime currentTime = QDateTime::currentDateTime();
    double elapsedSeconds = lastSimulationTime.msecsTo(currentTime) / 1000.0;

    // Jarak yang ditempuh (dalam mil laut)
    double distanceNM = ownShipSimSpeed * elapsedSeconds / 3600.0; // Konversi ke jam

    // Hitung posisi baru
    double newLat, newLon;
    EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                 ownShip.lat, ownShip.lon,
                                 distanceNM, ownShipSimCourse,
                                 &newLat, &newLon);

    // Update posisi kapal
    ownShip.lat = newLat;
    ownShip.lon = newLon;
    ownShip.cog = ownShipSimCourse;
    ownShip.sog = ownShipSimSpeed;
    ownShip.heading = ownShipSimCourse;

    // Karena guardzone terikat ke kapal, posisi guardzone juga terupdate
    if (guardZoneAttachedToShip) {
        guardZoneCenterLat = newLat;
        guardZoneCenterLon = newLon;
    }

    // PERBAIKAN: Update guardzone angle ketika heading berubah
    if (redDotAttachedToShip || hasAttachedGuardZone()) {
        updateRedDotPosition(newLat, newLon);
        qDebug() << "[HEADING-CHANGE] Updated guardzone angles due to heading change to:" << ownShipSimCourse;
    }

    // Ubah arah kapal sedikit secara acak (maksimal 5 derajat) untuk simulasi lebih realistis
    if (qrand() % 10 == 0) { // 10% kemungkinan untuk berubah arah
        double driftAngle = (qrand() % 10) - 5.0; // -5 hingga +5 derajat
        ownShipSimCourse += driftAngle;
        if (ownShipSimCourse < 0) ownShipSimCourse += 360.0;
        if (ownShipSimCourse >= 360.0) ownShipSimCourse -= 360.0;
    }

    // Set center view ke posisi kapal
    SetCenter(newLat, newLon);

    // Update gambar
    Draw();
    chartPixmap = drawPixmap.copy();

    // Update last simulation time
    lastSimulationTime = currentTime;
}

// Tambahkan di akhir file ecwidget.cpp (sebelum baris terakhir):

void EcWidget::startEditGuardZone(int guardZoneId)
{
    if (guardZoneManager) {
        guardZoneManager->startEditGuardZone(guardZoneId);
    }
}

// Tambahkan di akhir file ecwidget.cpp:

void EcWidget::showVisualFeedback(const QString& message, FeedbackType type)
{
    // Store feedback message dan type untuk ditampilkan
    feedbackMessage = message;
    feedbackType = type;
    feedbackTimer.start(2500); // Tampilkan selama 2.5 detik

    // Visual flash effect berdasarkan type
    flashOpacity = 255;

    // Sequence flash animation
    QTimer::singleShot(50, [this]() {
        flashOpacity = 200;
        update();
    });
    QTimer::singleShot(100, [this]() {
        flashOpacity = 150;
        update();
    });
    QTimer::singleShot(150, [this]() {
        flashOpacity = 100;
        update();
    });
    QTimer::singleShot(200, [this]() {
        flashOpacity = 50;
        update();
    });
    QTimer::singleShot(250, [this]() {
        flashOpacity = 0;
        update();
    });

    qDebug() << "[FEEDBACK]" << message << "- Type:" << type;
    update();
}

void EcWidget::playFeedbackSound(FeedbackType type)
{
    // Simple feedback dengan QApplication::beep()
    switch (type) {
    case FEEDBACK_SUCCESS:
        QApplication::beep();
        break;
    case FEEDBACK_ERROR:
        QApplication::beep();
        QTimer::singleShot(200, []() { QApplication::beep(); });
        break;
    case FEEDBACK_WARNING:
        QApplication::beep();
        QTimer::singleShot(150, []() { QApplication::beep(); });
        QTimer::singleShot(300, []() { QApplication::beep(); });
        break;
    case FEEDBACK_INFO:
        // Info - no sound, just visual
        break;
    }
}

void EcWidget::drawFeedbackOverlay(QPainter& painter)
{
    // Flash effect overlay
    if (flashOpacity > 0) {
        QColor flashColor;
        switch (feedbackType) {
        case FEEDBACK_SUCCESS:
            flashColor = QColor(0, 255, 0, flashOpacity / 4);
            break;
        case FEEDBACK_WARNING:
            flashColor = QColor(255, 165, 0, flashOpacity / 4);
            break;
        case FEEDBACK_ERROR:
            flashColor = QColor(255, 0, 0, flashOpacity / 4);
            break;
        case FEEDBACK_INFO:
            flashColor = QColor(0, 150, 255, flashOpacity / 4);
            break;
        }
        painter.fillRect(rect(), flashColor);
    }

    // Feedback message popup
    if (feedbackTimer.isActive() && !feedbackMessage.isEmpty()) {
        QRect messageRect(width() - 380, 20, 360, 100);

        QColor bgColor;
        QColor borderColor;
        QString icon;

        switch (feedbackType) {
        case FEEDBACK_SUCCESS:
            bgColor = QColor(34, 139, 34, 240);
            borderColor = QColor(0, 100, 0);
            icon = "✓";
            break;
        case FEEDBACK_WARNING:
            bgColor = QColor(255, 140, 0, 240);
            borderColor = QColor(200, 100, 0);
            icon = "⚠";
            break;
        case FEEDBACK_ERROR:
            bgColor = QColor(178, 34, 34, 240);
            borderColor = QColor(150, 0, 0);
            icon = "✗";
            break;
        case FEEDBACK_INFO:
            bgColor = QColor(70, 130, 180, 240);
            borderColor = QColor(0, 80, 150);
            icon = "ℹ";
            break;
        }

        // Shadow effect
        QRect shadowRect = messageRect.adjusted(3, 3, 3, 3);
        painter.fillRect(shadowRect, QColor(0, 0, 0, 100));

        // Main background
        painter.fillRect(messageRect, bgColor);
        painter.setPen(QPen(borderColor, 3));
        painter.drawRect(messageRect);

        // Icon
        painter.setPen(QPen(Qt::white, 2));
        painter.setFont(QFont("Arial", 20, QFont::Bold));
        painter.drawText(messageRect.adjusted(15, 15, -320, -60), Qt::AlignLeft | Qt::AlignTop, icon);

        // Message text
        painter.setFont(QFont("Arial", 9, QFont::Bold));
        painter.setPen(QPen(Qt::white, 1));
        painter.drawText(messageRect.adjusted(50, 20, -15, -15),
                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                         feedbackMessage);

        // Progress bar
        int totalTime = 2500;
        int timeLeft = feedbackTimer.remainingTime();
        int progressWidth = messageRect.width() - 20;
        int currentProgress = (timeLeft * progressWidth) / totalTime;

        painter.setPen(QPen(Qt::white, 1));
        painter.drawRect(messageRect.left() + 10, messageRect.bottom() - 15, progressWidth, 5);
        painter.fillRect(messageRect.left() + 10, messageRect.bottom() - 15, currentProgress, 5, Qt::white);
    }

}

void EcWidget::keyPressEvent(QKeyEvent *e)
{
    // AOI edit/create ESC handling first
    if (e->key() == Qt::Key_Escape) {
        // Stop EBL/VRM measure mode on ESC
        if (eblvrm.measureMode) {
            // Fix the first placed point as EBL target before closing
            eblvrm.commitFirstPointAsFixedTarget();
            eblvrm.setMeasureMode(false);
            eblvrm.clearMeasureSession();
            emit statusMessage(tr("Measure stopped"));
            update();
            return;
        }
        if (editingAOI) {
            finishEditAOI();
            return;
        }
        if (creatingAOI) {
            cancelAOICreation();
            return;
        }
    }
    // Handle key press for GuardZone editing
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        if (guardZoneManager->handleKeyPress(e)) {
            return; // Event handled by guard zone manager
        }
    }

    // Handle ESC key untuk cancel waypoint move operation dan route mode
    if (e->key() == Qt::Key_Escape) {
        if (activeFunction == CREATE_ROUTE) {
            // End route mode
            endRouteMode();
            return;
        }
        else if (activeFunction == MOVE_WAYP && moveSelectedIndex != -1) {
            // Cancel move operation
            ghostWaypoint.visible = false;
            moveSelectedIndex = -1;
            activeFunction = PAN;
            update();
            qDebug() << "[DEBUG] Waypoint move operation cancelled";
            return;
        }
    }

    // Default key handling
    QWidget::keyPressEvent(e);
}

// Tambahkan method baru ini di ecwidget.cpp

void EcWidget::createCircularGuardZoneNew(double centerLat, double centerLon, double radiusNM)
{
    qDebug() << "Creating new circular guardzone at" << centerLat << centerLon << "radius" << radiusNM;

    // Validation (existing code)
    if (centerLat < -90 || centerLat > 90 || centerLon < -180 || centerLon > 180) {
        showVisualFeedback(tr("Invalid coordinates for GuardZone center!"), FEEDBACK_ERROR);
        return;
    }

    if (radiusNM <= 0 || radiusNM > 100) {
        showVisualFeedback(tr("Invalid radius! Must be between 0.01 and 100 NM"), FEEDBACK_ERROR);
        return;
    }

    // Create new GuardZone struct
    GuardZone newGuardZone;
    newGuardZone.id = getNextGuardZoneId();
    newGuardZone.name = QString("GuardZone_%1").arg(newGuardZone.id);
    newGuardZone.shape = GUARD_ZONE_CIRCLE;
    newGuardZone.active = true;  // PENTING: Default active
    newGuardZone.attachedToShip = false;
    newGuardZone.color = Qt::red;
    newGuardZone.centerLat = centerLat;
    newGuardZone.centerLon = centerLon;
    newGuardZone.radius = radiusNM;

    // PERBAIKAN: Set untuk lingkaran penuh (tidak ada angle untuk create circular)
    newGuardZone.innerRadius = 0.0;      // Inner radius 0 untuk solid circle
    newGuardZone.outerRadius = radiusNM; // Outer radius sama dengan radius
    newGuardZone.startAngle = 0.0;       // Start dari 0°
    newGuardZone.endAngle = 0.0;         // End sama dengan start = full circle

    // Apply default filter settings from SettingsManager
    const SettingsData& settings = SettingsManager::instance().data();
    newGuardZone.shipTypeFilter = static_cast<ShipTypeFilter>(settings.defaultShipTypeFilter);
    newGuardZone.alertDirection = static_cast<AlertDirection>(settings.defaultAlertDirection);

    // Add to guardzone list
    guardZones.append(newGuardZone);

    // PERBAIKAN: Set system level flag hanya jika belum aktif
    if (!guardZoneActive) {
        guardZoneActive = true;
    }

    // PERBAIKAN: Auto-enable guardzone auto-check when creating new guardzone
    if (!guardZoneAutoCheckEnabled) {
        guardZoneAutoCheckEnabled = true;
        guardZoneAutoCheckTimer->start(guardZoneCheckInterval);
        qDebug() << "[AUTO-ACTIVATION] GuardZone auto-check automatically enabled for new circular guardzone";
    }

    // Set legacy variables untuk backward compatibility
    guardZoneShape = GUARD_ZONE_CIRCLE;
    guardZoneCenterLat = centerLat;
    guardZoneCenterLon = centerLon;
    guardZoneRadius = radiusNM;
    guardZoneAttachedToShip = false;

    qDebug() << "Circular guardzone created successfully with ID:" << newGuardZone.id;

    // PERBAIKAN: Proper sequence - Save → Feedback → Signal → Update
    saveGuardZones();

    showVisualFeedback(tr("Circular GuardZone created!\nID: %1, Radius: %2 NM")
                           .arg(newGuardZone.id)
                           .arg(radiusNM, 0, 'f', 2),
                       FEEDBACK_SUCCESS);

    // EMIT SIGNAL HANYA SEKALI di akhir
    emit guardZoneCreated();
    qDebug() << "GuardZone created signal emitted for ID:" << newGuardZone.id;

    // Final update
    update();
}

void EcWidget::createPolygonGuardZoneNew()
{
    // Validation (existing code)
    if (guardZonePoints.size() < 3) {
        showVisualFeedback(tr("Polygon needs at least 3 points!"), FEEDBACK_ERROR);
        return;
    }

    if (guardZonePoints.size() > 20) {
        showVisualFeedback(tr("Too many points! Maximum 20 points allowed"), FEEDBACK_WARNING);
        return;
    }

    qDebug() << "Creating new polygon guardzone with" << guardZonePoints.size() << "points";

    // Convert screen points to lat/lon (existing code)
    QVector<double> latLons;
    int invalidPoints = 0;

    for (const QPointF &point : guardZonePoints) {
        EcCoordinate lat, lon;
        if (XyToLatLon(point.x(), point.y(), lat, lon)) {
            if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
                latLons.append(lat);
                latLons.append(lon);
            } else {
                invalidPoints++;
            }
        } else {
            invalidPoints++;
        }
    }

    if (invalidPoints > 0) {
        showVisualFeedback(tr("Warning: %1 invalid points were skipped").arg(invalidPoints), FEEDBACK_WARNING);
    }

    if (latLons.size() < 6) {
        showVisualFeedback(tr("Failed to convert polygon points!"), FEEDBACK_ERROR);
        return;
    }

    // Create new GuardZone struct
    GuardZone newGuardZone;
    newGuardZone.id = getNextGuardZoneId();
    newGuardZone.name = QString("GuardZone_%1").arg(newGuardZone.id);
    newGuardZone.shape = GUARD_ZONE_POLYGON;
    newGuardZone.active = true;  // PENTING: Default active
    newGuardZone.attachedToShip = false;
    newGuardZone.color = Qt::red;
    newGuardZone.latLons = latLons;

    // Apply default filter settings from SettingsManager
    const SettingsData& settings = SettingsManager::instance().data();
    newGuardZone.shipTypeFilter = static_cast<ShipTypeFilter>(settings.defaultShipTypeFilter);
    newGuardZone.alertDirection = static_cast<AlertDirection>(settings.defaultAlertDirection);

    // Add to guardzone list
    guardZones.append(newGuardZone);

    // PERBAIKAN: Set system level flag hanya jika belum aktif
    if (!guardZoneActive) {
        guardZoneActive = true;
    }

    // PERBAIKAN: Auto-enable guardzone auto-check when creating new guardzone
    if (!guardZoneAutoCheckEnabled) {
        guardZoneAutoCheckEnabled = true;
        guardZoneAutoCheckTimer->start(guardZoneCheckInterval);
        qDebug() << "[AUTO-ACTIVATION] GuardZone auto-check automatically enabled for new polygon guardzone";
    }

    // Set legacy variables untuk backward compatibility
    guardZoneShape = GUARD_ZONE_POLYGON;
    guardZoneLatLons = latLons;
    guardZoneAttachedToShip = false;

    qDebug() << "Polygon guardzone created successfully with ID:" << newGuardZone.id;

    // PERBAIKAN: Proper sequence - Save → Feedback → Signal → Update
    saveGuardZones();

    showVisualFeedback(tr("Polygon GuardZone created!\nID: %1, Points: %2")
                           .arg(newGuardZone.id)
                           .arg(guardZonePoints.size()),
                       FEEDBACK_SUCCESS);

    // EMIT SIGNAL HANYA SEKALI di akhir
    emit guardZoneCreated();
    qDebug() << "GuardZone created signal emitted for ID:" << newGuardZone.id;

    // Final update
    update();
}

// Tambahkan di ecwidget.cpp (di bagian akhir file sebelum closing bracket):

void EcWidget::saveGuardZones()
{
    if (guardZones.isEmpty()) {
        qDebug() << "[INFO] No guardzones to save";
        return;
    }

    // Existing throttling code...
    static QTime lastSaveTime;
    QTime currentTime = QTime::currentTime();

    if (lastSaveTime.isValid() && lastSaveTime.msecsTo(currentTime) < 500) {
        qDebug() << "[INFO] Throttling save operation";
        return;
    }
    lastSaveTime = currentTime;

    // TAMBAHAN: Backup existing file sebelum save
    QString filePath = getGuardZoneFilePath();
    QString backupPath = filePath + ".backup";

    QFile existingFile(filePath);
    if (existingFile.exists()) {
        if (QFile::exists(backupPath)) {
            QFile::remove(backupPath);
        }
        existingFile.copy(backupPath);
        qDebug() << "[INFO] Created backup:" << backupPath;
    }

    QJsonArray guardZoneArray;
    int validCount = 0;
    int invalidCount = 0;

    for (const GuardZone &gz : guardZones) {
        // Enhanced validation
        if (gz.id <= 0) {
            qDebug() << "[WARNING] Skipping guardzone with invalid ID:" << gz.id;
            invalidCount++;
            continue;
        }

        // TAMBAHAN: Deep validation
        bool isValid = true;
        QString validationError;

        if (gz.shape == GUARD_ZONE_CIRCLE) {
            if (gz.centerLat < -90 || gz.centerLat > 90) {
                validationError = QString("Invalid latitude: %1").arg(gz.centerLat);
                isValid = false;
            } else if (gz.centerLon < -180 || gz.centerLon > 180) {
                validationError = QString("Invalid longitude: %1").arg(gz.centerLon);
                isValid = false;
            } else if (gz.radius <= 0 || gz.radius > 100) {
                validationError = QString("Invalid radius: %1").arg(gz.radius);
                isValid = false;
            }
        } else if (gz.shape == GUARD_ZONE_POLYGON) {
            if (gz.latLons.size() < 6 || gz.latLons.size() % 2 != 0) {
                validationError = QString("Invalid polygon coordinates count: %1").arg(gz.latLons.size());
                isValid = false;
            } else {
                for (int i = 0; i < gz.latLons.size(); i += 2) {
                    if (gz.latLons[i] < -90 || gz.latLons[i] > 90 ||
                        gz.latLons[i+1] < -180 || gz.latLons[i+1] > 180) {
                        validationError = QString("Invalid coordinate at index %1").arg(i);
                        isValid = false;
                        break;
                    }
                }
            }
        }

        if (!isValid) {
            qDebug() << "[ERROR] Skipping invalid guardzone" << gz.id << ":" << validationError;
            invalidCount++;
            continue;
        }

        // Create JSON object
        QJsonObject gzObject;
        gzObject["id"] = gz.id;
        gzObject["name"] = gz.name.isEmpty() ? QString("GuardZone_%1").arg(gz.id) : gz.name;
        gzObject["shape"] = static_cast<int>(gz.shape);
        gzObject["active"] = gz.active;
        gzObject["attachedToShip"] = gz.attachedToShip;
        gzObject["color"] = gz.color.isValid() ? gz.color.name() : "#ff0000";
        gzObject["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        if (gz.shape == GUARD_ZONE_CIRCLE) {
            gzObject["centerLat"] = gz.centerLat;
            gzObject["centerLon"] = gz.centerLon;
            gzObject["radius"] = gz.radius;  // Keep for backward compatibility
            gzObject["innerRadius"] = gz.innerRadius;
            gzObject["outerRadius"] = gz.outerRadius;
        } else if (gz.shape == GUARD_ZONE_POLYGON) {
            QJsonArray latLonArray;
            for (double coord : gz.latLons) {
                latLonArray.append(coord);
            }
            gzObject["latLons"] = latLonArray;
        } else if (gz.shape == GUARD_ZONE_SECTOR) {
            gzObject["centerLat"] = gz.centerLat;
            gzObject["centerLon"] = gz.centerLon;
            gzObject["innerRadius"] = gz.innerRadius;
            gzObject["outerRadius"] = gz.outerRadius;
            gzObject["startAngle"] = gz.startAngle;
            gzObject["endAngle"] = gz.endAngle;
            // Legacy radius untuk backward compatibility
            gzObject["radius"] = gz.outerRadius;
        }

        guardZoneArray.append(gzObject);
        validCount++;
    }

    // TAMBAHAN: Enhanced metadata
    QJsonObject rootObject;
    rootObject["guardzones"] = guardZoneArray;
    rootObject["version"] = "1.1";  // Increment version
    rootObject["saved_on"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObject["nextGuardZoneId"] = nextGuardZoneId;
    rootObject["app_version"] = "ECDIS_v1.1";
    rootObject["statistics"] = QJsonObject{
        {"total_count", guardZones.size()},
        {"valid_count", validCount},
        {"invalid_count", invalidCount},
        {"active_count", std::count_if(guardZones.begin(), guardZones.end(),
                                       [](const GuardZone& gz) { return gz.active; })}
    };

    QJsonDocument jsonDoc(rootObject);

    // TAMBAHAN: Atomic write operation
    QString tempPath = filePath + ".tmp";
    QFile tempFile(tempPath);

    if (tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Indented);
        qint64 bytesWritten = tempFile.write(jsonData);
        tempFile.close();

        if (bytesWritten == jsonData.size()) {
            // Atomic replace
            if (QFile::exists(filePath)) {
                QFile::remove(filePath);
            }

            if (QFile::rename(tempPath, filePath)) {
                qDebug() << "[SUCCESS] GuardZones saved atomically:" << filePath
                         << "Valid:" << validCount << "Invalid:" << invalidCount;

                // TAMBAHAN: Emit save success signal
                emit statusMessage(tr("GuardZones saved successfully (%1 valid, %2 invalid)")
                                       .arg(validCount).arg(invalidCount));
            } else {
                qDebug() << "[ERROR] Failed to replace file atomically";
                QFile::remove(tempPath);
            }
        } else {
            qDebug() << "[ERROR] Incomplete write to temp file";
            QFile::remove(tempPath);
        }
    } else {
        qDebug() << "[ERROR] Failed to create temp file:" << tempFile.errorString();

        // Fallback to direct write
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(jsonDoc.toJson(QJsonDocument::Indented));
            file.close();
            qDebug() << "[INFO] GuardZones saved via fallback method";
        }
    }
}

void EcWidget::loadGuardZones()
{
    // PERBAIKAN CRITICAL: Wrap entire function in try-catch
    try {
        QString filePath = getGuardZoneFilePath();
        QFile file(filePath);

    // Check main location
    if (!file.exists()) {
        QFile fallbackFile("guardzones.json");
        if (fallbackFile.exists()) {
            file.setFileName("guardzones.json");
            filePath = "guardzones.json";
        } else {
            qDebug() << "[INFO] No guardzones file found - starting fresh";
            // PERBAIKAN: Initialize default state
            guardZones.clear();
            guardZoneActive = false;
            nextGuardZoneId = 1;
            return;
        }
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray jsonData = file.readAll();
        file.close();

        // PERBAIKAN CRITICAL: Add safety check for empty or corrupt file
        if (jsonData.isEmpty()) {
            qDebug() << "[ERROR] Guardian zones file is empty:" << filePath;
            // Initialize with default empty state
            guardZones.clear();
            guardZoneActive = false;
            nextGuardZoneId = 1;
            return;
        }

        QJsonParseError parseError;
        QJsonDocument jsonDoc;

        // PERBAIKAN CRITICAL: Wrap JSON parsing in try-catch
        try {
            jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
        } catch (const std::exception& e) {
            qDebug() << "[ERROR] Exception during JSON parsing:" << e.what();
            return;
        } catch (...) {
            qDebug() << "[ERROR] Unknown exception during JSON parsing";
            return;
        }

        if (parseError.error != QJsonParseError::NoError) {
            qDebug() << "[ERROR] JSON parse error at" << parseError.offset << ":" << parseError.errorString();
            qDebug() << "[ERROR] Problematic JSON content around error:" << jsonData.mid(qMax(0, parseError.offset - 50), 100);
            return;
        }

        if (!jsonDoc.isObject()) {
            qDebug() << "[ERROR] Invalid guardzones JSON file structure";
            return;
        }

        QJsonObject rootObject = jsonDoc.object();

        // PERBAIKAN: Load nextGuardZoneId dari file
        if (rootObject.contains("nextGuardZoneId")) {
            nextGuardZoneId = rootObject["nextGuardZoneId"].toInt();
            if (nextGuardZoneId <= 0) nextGuardZoneId = 1;
        }

        if (!rootObject.contains("guardzones") || !rootObject["guardzones"].isArray()) {
            qDebug() << "[ERROR] Invalid guardzones JSON file (missing guardzones array)";
            return;
        }

        QJsonArray guardZoneArray = rootObject["guardzones"].toArray();

        guardZones.clear();

        int validGuardZones = 0;
        int activeGuardZones = 0;

        for (const QJsonValue &value : guardZoneArray) {
            if (!value.isObject()) continue;

            QJsonObject gzObject = value.toObject();

            // Validate required fields
            if (!gzObject.contains("shape")) continue;

            // PERBAIKAN CRITICAL: Validate shape value before casting
            int shapeValue = gzObject["shape"].toInt();
            if (shapeValue < 0 || shapeValue > 2) {
                qDebug() << "[ERROR] Invalid shape value:" << shapeValue << "- skipping guardzone";
                continue;
            }

            GuardZone gz;
            gz.id = gzObject.contains("id") ? gzObject["id"].toInt() : getNextGuardZoneId();
            gz.name = gzObject.contains("name") ? gzObject["name"].toString() :
                          QString("GuardZone_%1").arg(gz.id);
            gz.shape = static_cast<::GuardZoneShape>(shapeValue);

            // PERBAIKAN: Preserve active status dari file
            gz.active = gzObject.contains("active") ? gzObject["active"].toBool() : true;

            gz.attachedToShip = gzObject.contains("attachedToShip") ?
                                    gzObject["attachedToShip"].toBool() : false;

            // PERBAIKAN CRITICAL: Safe color parsing with validation
            QString colorStr = gzObject.contains("color") ? gzObject["color"].toString() : "#ff0000";
            if (colorStr.isEmpty() || !colorStr.startsWith("#")) {
                colorStr = "#ff0000";  // Default red color
            }
            gz.color = QColor(colorStr);
            if (!gz.color.isValid()) {
                qDebug() << "[WARNING] Invalid color:" << colorStr << "using default red";
                gz.color = Qt::red;
            }

            // Load shape-specific data
            bool shapeDataValid = false;

            if (gz.shape == GUARD_ZONE_CIRCLE) {
                if (gzObject.contains("centerLat") && gzObject.contains("centerLon") &&
                    gzObject.contains("radius")) {

                    // PERBAIKAN CRITICAL: Safe coordinate parsing with NaN check
                    gz.centerLat = gzObject["centerLat"].toDouble();
                    gz.centerLon = gzObject["centerLon"].toDouble();
                    gz.radius = gzObject["radius"].toDouble();

                    // Check for NaN or infinity values
                    if (qIsNaN(gz.centerLat) || qIsInf(gz.centerLat) ||
                        qIsNaN(gz.centerLon) || qIsInf(gz.centerLon) ||
                        qIsNaN(gz.radius) || qIsInf(gz.radius)) {
                        qDebug() << "[ERROR] NaN/Infinity values detected in circle guardzone:" << gz.name;
                        continue;  // Skip this guardzone entirely
                    }

                    // Load donut properties (with backward compatibility)
                    if (gzObject.contains("innerRadius") && gzObject.contains("outerRadius")) {
                        gz.innerRadius = gzObject["innerRadius"].toDouble();
                        gz.outerRadius = gzObject["outerRadius"].toDouble();

                        // PERBAIKAN CRITICAL: Validate innerRadius/outerRadius for NaN
                        if (qIsNaN(gz.innerRadius) || qIsInf(gz.innerRadius) ||
                            qIsNaN(gz.outerRadius) || qIsInf(gz.outerRadius)) {
                            qDebug() << "[ERROR] NaN/Infinity in radius values for guardzone:" << gz.name;
                            continue;  // Skip this guardzone entirely
                        }
                    } else {
                        // Backward compatibility - use old radius as outer radius
                        gz.innerRadius = 0.0;
                        gz.outerRadius = gz.radius;
                    }

                    // Validate circle data
                    if (gz.centerLat >= -90 && gz.centerLat <= 90 &&
                        gz.centerLon >= -180 && gz.centerLon <= 180 &&
                        gz.outerRadius > 0 && gz.outerRadius <= 100 &&
                        gz.innerRadius >= 0 && gz.innerRadius < gz.outerRadius) {
                        shapeDataValid = true;
                    }
                }
            } else if (gz.shape == GUARD_ZONE_POLYGON) {
                if (gzObject.contains("latLons") && gzObject["latLons"].isArray()) {
                    QJsonArray latLonArray = gzObject["latLons"].toArray();
                    gz.latLons.clear();

                    for (const QJsonValue &coord : latLonArray) {
                        gz.latLons.append(coord.toDouble());
                    }

                    if (gz.latLons.size() >= 6 && gz.latLons.size() % 2 == 0) {
                        // Validate all coordinates
                        bool coordsValid = true;
                        for (int i = 0; i < gz.latLons.size(); i += 2) {
                            if (gz.latLons[i] < -90 || gz.latLons[i] > 90 ||
                                gz.latLons[i+1] < -180 || gz.latLons[i+1] > 180) {
                                coordsValid = false;
                                break;
                            }
                        }
                        if (coordsValid) shapeDataValid = true;
                    }
                }
            } else if (gz.shape == GUARD_ZONE_SECTOR) {
                if (gzObject.contains("centerLat") && gzObject.contains("centerLon") &&
                    gzObject.contains("innerRadius") && gzObject.contains("outerRadius") &&
                    gzObject.contains("startAngle") && gzObject.contains("endAngle")) {

                    gz.centerLat = gzObject["centerLat"].toDouble();
                    gz.centerLon = gzObject["centerLon"].toDouble();
                    gz.innerRadius = gzObject["innerRadius"].toDouble();
                    gz.outerRadius = gzObject["outerRadius"].toDouble();
                    gz.startAngle = gzObject["startAngle"].toDouble();
                    gz.endAngle = gzObject["endAngle"].toDouble();

                    // Legacy radius untuk backward compatibility
                    gz.radius = gz.outerRadius;

                    // Validate sector data
                    if (gz.centerLat >= -90 && gz.centerLat <= 90 &&
                        gz.centerLon >= -180 && gz.centerLon <= 180 &&
                        gz.innerRadius > 0 && gz.innerRadius <= 100 &&
                        gz.outerRadius > 0 && gz.outerRadius <= 100 &&
                        gz.outerRadius > gz.innerRadius &&
                        gz.startAngle >= 0 && gz.startAngle < 360 &&
                        gz.endAngle >= 0 && gz.endAngle < 360) {
                        shapeDataValid = true;
                    }
                }
            }

            if (shapeDataValid) {
                guardZones.append(gz);
                validGuardZones++;

                if (gz.active) {
                    activeGuardZones++;
                }

                // Update nextGuardZoneId if needed
                if (gz.id >= nextGuardZoneId) {
                    nextGuardZoneId = gz.id + 1;
                }
            } else {
                qDebug() << "[WARNING] Skipping invalid guardzone:" << gz.name;
            }
        }

        qDebug() << "[INFO] Loaded" << validGuardZones << "guardzones from" << filePath;
        qDebug() << "[INFO] Active guardzones:" << activeGuardZones;

        // PERBAIKAN: Set system state berdasarkan loaded data
        if (!guardZones.isEmpty()) {
            guardZoneActive = (activeGuardZones > 0);  // Active jika ada guardzone aktif

            // Set legacy variables dari guardzone aktif pertama
            for (const GuardZone& gz : guardZones) {
                if (gz.active) {
                    guardZoneShape = gz.shape;
                    guardZoneAttachedToShip = gz.attachedToShip;

                    if (gz.shape == GUARD_ZONE_CIRCLE) {
                        guardZoneCenterLat = gz.centerLat;
                        guardZoneCenterLon = gz.centerLon;
                        guardZoneRadius = gz.radius;
                    } else if (gz.shape == GUARD_ZONE_POLYGON) {
                        guardZoneLatLons = gz.latLons;
                    }
                    break;
                }
            }

            // PERBAIKAN: Set attachedGuardZoneId jika ada guardzone dengan attachedToShip
            for (const GuardZone& gz : guardZones) {
                if (gz.attachedToShip &&
                    !gz.name.contains("Ship Guardian Circle") &&
                    !gz.name.contains("Red Dot Guardian")) {
                    attachedGuardZoneId = gz.id;
                    attachedGuardZoneName = gz.name;
                    qDebug() << "[LOAD] Found attached guardzone, setting attachedGuardZoneId:" << attachedGuardZoneId << "name:" << attachedGuardZoneName;
                    break;
                }
            }
        } else {
            guardZoneActive = false;
        }

        qDebug() << "[INFO] GuardZone system state - Active:" << guardZoneActive
                 << "NextID:" << nextGuardZoneId;

        // PERBAIKAN: Restore attachedGuardZoneId untuk attached guardzones
        int attachedCount = 0;
        for (GuardZone& gz : guardZones) {
            if (gz.attachedToShip) {
                attachedCount++;
                if (attachedCount == 1) {
                    // Ambil yang pertama sebagai attached guardzone
                    attachedGuardZoneId = gz.id;
                    attachedGuardZoneName = gz.name;
                    qDebug() << "[RESTORE] Restored attached guardzone ID:" << attachedGuardZoneId << "Name:" << attachedGuardZoneName;

                    // Set red dot attached state untuk consistency
                    redDotAttachedToShip = true;
                    redDotTrackerEnabled = false;  // PERBAIKAN: Disable untuk mencegah double rendering

                    // PERBAIKAN CRITICAL: Matikan shipGuardianEnabled
                    // untuk mencegah konflik rendering dengan attached guardzone
                    shipGuardianEnabled = false;
                    qDebug() << "[RESTORE] Disabled shipGuardian to prevent rendering conflict with attached guardzone";

                    // Emit signal untuk update UI
                    emit attachToShipStateChanged(true);

                    // PERBAIKAN: Auto-enable guardzone auto-check for attached guardzone after restart
                    if (!guardZoneAutoCheckEnabled) {
                        guardZoneAutoCheckEnabled = true;
                        guardZoneAutoCheckTimer->start(guardZoneCheckInterval);
                        qDebug() << "[AUTO-ACTIVATION] GuardZone auto-check automatically enabled for restored attached guardzone" << attachedGuardZoneId;
                    } else {
                        qDebug() << "[INFO] GuardZone auto-check already enabled for attached guardzone" << attachedGuardZoneId;
                    }
                } else {
                    // Perbaiki duplikasi - set attachedToShip = false untuk yang lain
                    qDebug() << "[CLEANUP] Found duplicate attached guardzone" << gz.id << "- removing attachedToShip flag";
                    gz.attachedToShip = false;
                }
            }
        }

        // PERBAIKAN: Jika ada duplikasi, save untuk membersihkan file
        if (attachedCount > 1) {
            qDebug() << "[CLEANUP] Found" << attachedCount << "attached guardzones, cleaned up to 1. Saving file...";
            saveGuardZones();
        }

        // PERBAIKAN: Debug final state
        qDebug() << "[LOAD-FINAL] attachedGuardZoneId:" << attachedGuardZoneId
                 << "attachedCount:" << attachedCount
                 << "totalGuardZones:" << guardZones.size();

        // PERBAIKAN: Auto-enable auto-check if there are any active guardzones
        if (activeGuardZones > 0 && !guardZoneAutoCheckEnabled) {
            guardZoneAutoCheckEnabled = true;
            guardZoneAutoCheckTimer->start(guardZoneCheckInterval);
            qDebug() << "[AUTO-ACTIVATION] GuardZone auto-check automatically enabled for" << activeGuardZones << "existing active guardzones";
        }

        // PERBAIKAN: Trigger update hanya jika ada data
        if (!guardZones.isEmpty()) {
            update();

            // Apply default filters to loaded guardzones if they have default values
            if (guardZoneManager) {
                guardZoneManager->applyDefaultFiltersToExistingGuardZones();
            }
        }
    } else {
        qDebug() << "[ERROR] Failed to open guardzones file:" << file.errorString();
    }
    } catch (const std::exception& e) {
        qDebug() << "[ERROR] Exception in loadGuardZones():" << e.what();
        // Initialize safe default state
        guardZones.clear();
        guardZoneActive = false;
        nextGuardZoneId = 1;
    } catch (...) {
        qDebug() << "[ERROR] Unknown exception in loadGuardZones() - initializing safe defaults";
        // Initialize safe default state
        guardZones.clear();
        guardZoneActive = false;
        nextGuardZoneId = 1;
    }
}

QString EcWidget::getGuardZoneFilePath() const
{
    // Simpan di direktori data aplikasi (sama seperti waypoint)
    QString basePath;

#ifdef _WIN32
    if (EcKernelGetEnv("APPDATA"))
        basePath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC";
#else
    if (EcKernelGetEnv("HOME"))
        basePath = QString(EcKernelGetEnv("HOME")) + "/SevenCs/EC2007/DENC";
#endif

    // Jika base path tidak tersedia, gunakan direktori saat ini
    if (basePath.isEmpty())
        return "guardzones.json";
    else
        return basePath + "/guardzones.json";
}

void EcWidget::debugPreviewState()
{
    qDebug() << "=== GUARDZONE PREVIEW DEBUG ===";
    qDebug() << "creatingGuardZone:" << creatingGuardZone;
    qDebug() << "newGuardZoneShape:" << newGuardZoneShape;
    qDebug() << "guardZonePoints.size():" << guardZonePoints.size();
    qDebug() << "currentMousePos:" << currentMousePos;
    qDebug() << "hasMouseTracking:" << hasMouseTracking();
    qDebug() << "==============================";
}

void EcWidget::emitGuardZoneSignals(const QString& action, int guardZoneId)
{
    static QMap<QString, QTime> lastEmitTime;
    QTime currentTime = QTime::currentTime();
    QString key = QString("%1_%2").arg(action).arg(guardZoneId);

    // Prevent rapid duplicate signals (dalam 100ms)
    if (lastEmitTime.contains(key) &&
        lastEmitTime[key].msecsTo(currentTime) < 100) {
        qDebug() << "Preventing duplicate signal emission for" << action << guardZoneId;
        return;
    }

    lastEmitTime[key] = currentTime;

    if (action == "created") {
        emit guardZoneCreated();
    } else if (action == "modified") {
        emit guardZoneModified();
    } else if (action == "deleted") {
        emit guardZoneDeleted();
    }

    qDebug() << "Signal emitted:" << action << "for GuardZone" << guardZoneId;
}

bool EcWidget::isGuardZoneInViewport(const GuardZone& gz, const QRect& viewport)
{
    // Simple viewport culling
    if (gz.shape == GUARD_ZONE_CIRCLE) {
        int centerX, centerY;
        double lat, lon;

        if (gz.attachedToShip) {
            // Konsisten dengan logic drawing - gunakan redDot position jika available
            if (redDotTrackerEnabled && redDotLat != 0.0 && redDotLon != 0.0) {
                lat = redDotLat;
                lon = redDotLon;
            } else {
                lat = ownShip.lat;
                lon = ownShip.lon;
            }
            qDebug() << "[VIEWPORT-DEBUG] Checking attached guardzone" << gz.id
                     << "at position:" << lat << "," << lon;
        } else {
            lat = gz.centerLat;
            lon = gz.centerLon;
        }

        if (LatLonToXy(lat, lon, centerX, centerY)) {
            double radiusPixels = calculatePixelsFromNauticalMiles(gz.radius);
            QRect circleRect(centerX - radiusPixels, centerY - radiusPixels,
                             radiusPixels * 2, radiusPixels * 2);
            return viewport.intersects(circleRect);
        }
    } else if (gz.shape == GUARD_ZONE_POLYGON) {
        // Check if any vertex is in viewport
        for (int i = 0; i < gz.latLons.size(); i += 2) {
            int x, y;
            if (LatLonToXy(gz.latLons[i], gz.latLons[i+1], x, y)) {
                if (viewport.contains(x, y)) {
                    return true;
                }
            }
        }
    }
    return false;
}

void EcWidget::drawGuardZoneLabel(QPainter& painter, const GuardZone& gz, const QPoint& position)
{
    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(QBrush(QColor(255, 255, 255, 200)));

    QString labelText = gz.name;
    if (!gz.active) {
        labelText += " (Disabled)";
    }
    if (gz.attachedToShip) {
        labelText += " [Ship]";
    }

    QFont labelFont("Arial", 9, QFont::Bold);
    QFontMetrics fm(labelFont);
    QRect textRect = fm.boundingRect(labelText);
    textRect.moveCenter(position);
    textRect.adjust(-3, -1, 3, 1);

    painter.drawRect(textRect);

    painter.setPen(QPen(Qt::black, 1));
    painter.setFont(labelFont);
    painter.drawText(textRect, Qt::AlignCenter, labelText);
}

void EcWidget::drawGuardZoneCreationPreview(QPainter& painter)
{
    // ========== PERBAIKAN: ERROR HANDLING ==========
    if (!creatingGuardZone) return;

    // Validate painter state
    if (!painter.isActive()) {
        qDebug() << "[ERROR] Painter not active in drawGuardZoneCreationPreview";
        return;
    }

    // Validate widget size
    if (width() <= 0 || height() <= 0) {
        qDebug() << "[ERROR] Invalid widget size in preview";
        return;
    }

    try {
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (newGuardZoneShape == GUARD_ZONE_CIRCLE) {
            if (guardZonePoints.size() == 1) {
                // Preview circle creation - show center point and radius line to mouse
                QPointF center = guardZonePoints.first();

                // Draw center point
                painter.setPen(QPen(Qt::red, 3));  // Merah
                painter.setBrush(QBrush(Qt::red));  // Merah
                painter.drawEllipse(center, 8, 8);

                // Draw center label
                painter.setPen(QPen(Qt::white, 2));  // Putih agar kontras
                painter.setFont(QFont("Arial", 10, QFont::Bold));
                painter.drawText(center.x() + 12, center.y() - 8, "CENTER");

                // Draw preview circle if mouse is valid
                if (currentMousePos.x() > 0 && currentMousePos.y() > 0) {
                    // Calculate radius in pixels
                    double dx = currentMousePos.x() - center.x();
                    double dy = currentMousePos.y() - center.y();
                    double radiusPixels = sqrt(dx*dx + dy*dy);

                    if (radiusPixels > 5) { // Minimum radius
                        // Draw preview circle
                        painter.setPen(QPen(Qt::red, 2, Qt::DashLine));  // Merah
                        painter.setBrush(Qt::NoBrush);
                        painter.drawEllipse(center, radiusPixels, radiusPixels);

                        // Draw radius line
                        painter.setPen(QPen(Qt::red, 2));  // Merah
                        painter.drawLine(center, currentMousePos);

                        // Calculate and show radius in nautical miles
                        EcCoordinate centerLat, centerLon, mouseLat, mouseLon;
                        if (XyToLatLon(center.x(), center.y(), centerLat, centerLon) &&
                            XyToLatLon(currentMousePos.x(), currentMousePos.y(), mouseLat, mouseLon)) {

                            double distNM, bearing;
                            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                                   centerLat, centerLon,
                                                                   mouseLat, mouseLon,
                                                                   &distNM, &bearing);

                            // Draw radius info
                            QPoint midPoint = (center.toPoint() + currentMousePos) / 2;
                            painter.setPen(QPen(Qt::black, 1));
                            painter.setBrush(QBrush(QColor(255, 255, 255, 200)));
                            QRect infoRect(midPoint.x() - 40, midPoint.y() - 15, 80, 30);
                            painter.drawRect(infoRect);

                            painter.setPen(QPen(Qt::black, 2));
                            painter.setFont(QFont("Arial", 8, QFont::Bold));
                            painter.drawText(infoRect, Qt::AlignCenter,
                                             QString("%1 NM").arg(distNM, 0, 'f', 2));
                        }
                    }
                }
            }
        }
        else if (newGuardZoneShape == GUARD_ZONE_POLYGON) {
            if (!guardZonePoints.isEmpty()) {
                // Draw existing points
                painter.setPen(QPen(Qt::red, 2));  // Merah
                painter.setBrush(QBrush(Qt::red));  // Merah

                for (int i = 0; i < guardZonePoints.size(); i++) {
                    QPointF point = guardZonePoints[i];
                    painter.drawEllipse(point, 6, 6);

                    // Draw point number
                    painter.setPen(QPen(Qt::white, 2));  // Putih
                    painter.setFont(QFont("Arial", 8, QFont::Bold));
                    painter.drawText(point.x() + 10, point.y() - 5, QString::number(i + 1));
                    painter.setPen(QPen(Qt::red, 2));  // Merah
                }

                // Draw lines between points
                if (guardZonePoints.size() > 1) {
                    painter.setPen(QPen(Qt::red, 2, Qt::DashLine));  // Merah
                    for (int i = 0; i < guardZonePoints.size() - 1; i++) {
                        painter.drawLine(guardZonePoints[i], guardZonePoints[i + 1]);
                    }
                }

                // Draw preview line to mouse cursor
                if (currentMousePos.x() > 0 && currentMousePos.y() > 0) {
                    painter.setPen(QPen(Qt::red, 1, Qt::DotLine));  // Merah
                    painter.drawLine(guardZonePoints.last(), currentMousePos);

                    // Draw mouse cursor position
                    painter.setPen(QPen(Qt::red, 2));  // Merah
                    painter.setBrush(QBrush(Qt::red));  // Merah
                    painter.drawEllipse(currentMousePos, 4, 4);
                }

                // Draw closing line preview if we have 3+ points
                if (guardZonePoints.size() >= 3 && currentMousePos.x() > 0) {
                    painter.setPen(QPen(Qt::red, 1, Qt::DotLine));  // Merah
                    painter.drawLine(currentMousePos, guardZonePoints.first());
                }
            }
        }

        // Draw creation instructions
        painter.setPen(QPen(Qt::black, 1));
        painter.setBrush(QBrush(QColor(255, 255, 255, 220)));

        QRect instructionRect(10, height() - 100, 400, 80);
        painter.drawRect(instructionRect);

        painter.setPen(QPen(Qt::blue, 2));
        painter.setFont(QFont("Arial", 9, QFont::Bold));

        QString instructionText;
        if (newGuardZoneShape == GUARD_ZONE_CIRCLE) {
            if (guardZonePoints.isEmpty()) {
                instructionText = "CREATING CIRCULAR GUARDZONE\n• Click to set CENTER point";
            } else {
                instructionText = "CREATING CIRCULAR GUARDZONE\n• Move mouse to set RADIUS\n• Click to confirm";
            }
        } else {
            instructionText = QString("CREATING POLYGON GUARDZONE\n• Click to add points (%1 added)\n• Right-click to finish (min 3 points)")
                                  .arg(guardZonePoints.size());
        }

        painter.drawText(instructionRect.adjusted(10, 10, -10, -10), Qt::AlignLeft | Qt::AlignTop, instructionText);

    } catch (const std::exception& e) {
        qDebug() << "[ERROR] Exception in drawGuardZoneCreationPreview:" << e.what();
        return;
    } catch (...) {
        qDebug() << "[ERROR] Unknown exception in drawGuardZoneCreationPreview";
        return;
    }
    // =============================================
}

void EcWidget::drawSectorGuardZone(QPainter& painter, const GuardZone& gz, int& labelX, int& labelY)
{
    // ========== SECTOR GUARDZONE RENDERING ==========
    // Implementasi sector guardzone (setengah lingkaran)

    qDebug() << "[DRAW-SECTOR] Drawing" << (gz.innerRadius <= 0.0 ? "semicircle" : "sector") << "guardzone" << gz.id << "at" << gz.centerLat << "," << gz.centerLon
             << "angles:" << gz.startAngle << "to" << gz.endAngle;

    // Convert center position to screen coordinates
    int centerX, centerY;
    if (!LatLonToXy(gz.centerLat, gz.centerLon, centerX, centerY)) {
        qDebug() << "[DRAW-SECTOR] Failed to convert coordinates for guardzone" << gz.id;
        labelX = labelY = 0;
        return;
    }

    // Calculate radii in pixels
    double innerRadiusPixels = calculatePixelsFromNauticalMiles(gz.innerRadius);
    double outerRadiusPixels = calculatePixelsFromNauticalMiles(gz.outerRadius);

    // Ensure minimum visible size
    if (outerRadiusPixels < 5) outerRadiusPixels = 5;

    // Handle semicircle case (inner radius = 0)
    if (gz.innerRadius <= 0.0) {
        innerRadiusPixels = 0;
    } else {
        if (innerRadiusPixels < 1) innerRadiusPixels = 1;
        if (outerRadiusPixels <= innerRadiusPixels) innerRadiusPixels = outerRadiusPixels - 4;
    }

    // Convert angles from navigation (0=North, clockwise) to Qt (0=East, counterclockwise)
    // Navigation: 0° = North, clockwise
    // Qt: 0° = East, counterclockwise
    // Conversion: qtAngle = (90 - navAngle) mod 360
    double qtStartAngle = (90 - gz.startAngle + 360) * 16;    // Qt uses 16th of degree
    double qtEndAngle = (90 - gz.endAngle + 360) * 16;
    double qtSpanAngle = (qtEndAngle - qtStartAngle);

    // Normalize angles
    qtStartAngle = fmod(qtStartAngle, 360 * 16);
    if (qtSpanAngle < 0) qtSpanAngle += 360 * 16;
    if (qtSpanAngle > 360 * 16) qtSpanAngle -= 360 * 16;

    // Set up painter for sector drawing - consistent with circle/polygon
    QPen pen(gz.color);
    pen.setWidth(2);
    painter.setPen(pen);

    QColor fillColor = gz.color;
    fillColor.setAlpha(50);  // Same transparency as circle/polygon
    painter.setBrush(QBrush(fillColor));

    // Create the sector path using QPainterPath for precise control
    QPainterPath sectorPath;

    // Calculate start and end points for outer arc
    double startAngleQt = qtStartAngle / 16.0;
    double spanAngleQt = qtSpanAngle / 16.0;

    // Move to start point of outer arc
    QRectF outerRect(centerX - outerRadiusPixels, centerY - outerRadiusPixels,
                     2 * outerRadiusPixels, 2 * outerRadiusPixels);
    sectorPath.arcMoveTo(outerRect, startAngleQt);

    // Draw outer arc
    sectorPath.arcTo(outerRect, startAngleQt, spanAngleQt);

    if (gz.innerRadius <= 0.0) {
        // For semicircle (inner radius = 0), connect back to center
        sectorPath.lineTo(centerX, centerY);
        sectorPath.closeSubpath();
    } else {
        // For ring sector, draw inner arc
        double endAngleQt = startAngleQt + spanAngleQt;
        QRectF innerRect(centerX - innerRadiusPixels, centerY - innerRadiusPixels,
                         2 * innerRadiusPixels, 2 * innerRadiusPixels);

        // Connect to inner arc end point
        QPointF innerEndPoint = QPointF(
            centerX + innerRadiusPixels * cos(endAngleQt * M_PI / 180.0),
            centerY + innerRadiusPixels * sin(endAngleQt * M_PI / 180.0)
        );
        sectorPath.lineTo(innerEndPoint);

        // Draw inner arc in reverse direction
        sectorPath.arcTo(innerRect, endAngleQt, -spanAngleQt);

        // Close the path back to start
        sectorPath.closeSubpath();
    }

    // Draw the sector
    painter.drawPath(sectorPath);

    // Draw border lines for better visibility - consistent with main border
    painter.setPen(pen);
    // Convert navigation angles to Qt coordinate system for radial lines
    double startAngleRad = (90 - gz.startAngle) * M_PI / 180.0;  // Convert to radians
    double endAngleRad = (90 - gz.endAngle) * M_PI / 180.0;

    if (gz.innerRadius <= 0.0) {
        // For semicircle, draw radial lines from center to outer arc
        painter.drawLine(centerX, centerY,
                         centerX + cos(startAngleRad) * outerRadiusPixels,
                         centerY + sin(startAngleRad) * outerRadiusPixels);

        painter.drawLine(centerX, centerY,
                         centerX + cos(endAngleRad) * outerRadiusPixels,
                         centerY + sin(endAngleRad) * outerRadiusPixels);
    } else {
        // For ring sector, draw radial lines from inner to outer arc
        painter.drawLine(centerX + cos(startAngleRad) * innerRadiusPixels,
                         centerY + sin(startAngleRad) * innerRadiusPixels,
                         centerX + cos(startAngleRad) * outerRadiusPixels,
                         centerY + sin(startAngleRad) * outerRadiusPixels);

        painter.drawLine(centerX + cos(endAngleRad) * innerRadiusPixels,
                         centerY + sin(endAngleRad) * innerRadiusPixels,
                         centerX + cos(endAngleRad) * outerRadiusPixels,
                         centerY + sin(endAngleRad) * outerRadiusPixels);
    }

    // Set label position at the center of the sector
    labelX = centerX;
    labelY = centerY - static_cast<int>(outerRadiusPixels) - 15;

    qDebug() << "[DRAW-SECTOR] Successfully drew" << (gz.innerRadius <= 0.0 ? "semicircle" : "sector") << "guardzone" << gz.id
             << "center:" << centerX << "," << centerY
             << "inner:" << innerRadiusPixels << "outer:" << outerRadiusPixels << "pixels"
             << "nav angles:" << gz.startAngle << "to" << gz.endAngle
             << "qt start:" << (qtStartAngle/16) << "span:" << (qtSpanAngle/16);
}

bool EcWidget::validateGuardZoneSystem()
{
    qDebug() << "[VALIDATION] Starting comprehensive GuardZone system validation";

    bool allValid = true;
    QStringList issues;

    // Check system state consistency
    bool hasActiveGuardZones = std::any_of(guardZones.begin(), guardZones.end(),
                                           [](const GuardZone& gz) { return gz.active; });

    if (guardZoneActive && !hasActiveGuardZones) {
        issues << "System marked active but no active guardzones found";
        allValid = false;
    }

    if (!guardZoneActive && hasActiveGuardZones) {
        issues << "System marked inactive but active guardzones exist";
        allValid = false;
    }

    // Check ID uniqueness
    QSet<int> usedIds;
    for (const GuardZone& gz : guardZones) {
        if (usedIds.contains(gz.id)) {
            issues << QString("Duplicate ID detected: %1").arg(gz.id);
            allValid = false;
        }
        usedIds.insert(gz.id);

        if (gz.id >= nextGuardZoneId) {
            issues << QString("ID %1 >= nextGuardZoneId %2").arg(gz.id).arg(nextGuardZoneId);
            allValid = false;
        }
    }

    // Check individual guardzone validity
    for (const GuardZone& gz : guardZones) {
        if (gz.name.isEmpty()) {
            issues << QString("GuardZone %1 has empty name").arg(gz.id);
        }

        if (!gz.color.isValid()) {
            issues << QString("GuardZone %1 has invalid color").arg(gz.id);
        }

        if (gz.shape == GUARD_ZONE_CIRCLE) {
            if (gz.centerLat < -90 || gz.centerLat > 90) {
                issues << QString("GuardZone %1 has invalid latitude").arg(gz.id);
                allValid = false;
            }
            if (gz.centerLon < -180 || gz.centerLon > 180) {
                issues << QString("GuardZone %1 has invalid longitude").arg(gz.id);
                allValid = false;
            }
            if (gz.outerRadius <= 0 || gz.outerRadius > 100) {
                issues << QString("GuardZone %1 has invalid outer radius").arg(gz.id);
                allValid = false;
            }
            if (gz.innerRadius < 0 || gz.innerRadius >= gz.outerRadius) {
                issues << QString("GuardZone %1 has invalid inner radius").arg(gz.id);
                allValid = false;
            }
        } else if (gz.shape == GUARD_ZONE_POLYGON) {
            if (gz.latLons.size() < 6 || gz.latLons.size() % 2 != 0) {
                issues << QString("GuardZone %1 has invalid polygon data").arg(gz.id);
                allValid = false;
            }
        } else if (gz.shape == GUARD_ZONE_SECTOR) {
            if (gz.centerLat < -90 || gz.centerLat > 90) {
                issues << QString("GuardZone %1 has invalid latitude").arg(gz.id);
                allValid = false;
            }
            if (gz.centerLon < -180 || gz.centerLon > 180) {
                issues << QString("GuardZone %1 has invalid longitude").arg(gz.id);
                allValid = false;
            }
            if (gz.innerRadius <= 0 || gz.innerRadius > 100) {
                issues << QString("GuardZone %1 has invalid inner radius").arg(gz.id);
                allValid = false;
            }
            if (gz.outerRadius <= 0 || gz.outerRadius > 100) {
                issues << QString("GuardZone %1 has invalid outer radius").arg(gz.id);
                allValid = false;
            }
            if (gz.outerRadius <= gz.innerRadius) {
                issues << QString("GuardZone %1 has outer radius <= inner radius").arg(gz.id);
                allValid = false;
            }
            if (gz.startAngle < 0 || gz.startAngle >= 360) {
                issues << QString("GuardZone %1 has invalid start angle").arg(gz.id);
                allValid = false;
            }
            if (gz.endAngle < 0 || gz.endAngle >= 360) {
                issues << QString("GuardZone %1 has invalid end angle").arg(gz.id);
                allValid = false;
            }
        }
    }

    // Log results
    if (allValid) {
        qDebug() << "[VALIDATION] ✓ All guardzones are valid";
    } else {
        qDebug() << "[VALIDATION] ✗ Found" << issues.size() << "issues:";
        for (const QString& issue : issues) {
            qDebug() << "[VALIDATION]   -" << issue;
        }
    }

    return allValid;
}

// Alert System
void EcWidget::initializeAlertSystem()
{
    qDebug() << "[ECWIDGET] Initializing Alert System";

    try {
        // Create alert system
        alertSystem = new AlertSystem(this, this);

        qDebug() << "[ECWIDGET] AlertSystem created at address:" << alertSystem;

        // Connect signals - DIRECT CONNECTION (TANPA LAMBDA)
        bool conn1 = connect(alertSystem, &AlertSystem::alertTriggered,
                             this, &EcWidget::onAlertTriggered);
        bool conn2 = connect(alertSystem, &AlertSystem::criticalAlert,
                             this, &EcWidget::onCriticalAlert);
        bool conn3 = connect(alertSystem, &AlertSystem::systemStatusChanged,
                             this, &EcWidget::onAlertSystemStatusChanged);

        qDebug() << "[ECWIDGET] Signal connections:" << conn1 << conn2 << conn3;

        // Configure alert system
        alertSystem->setDepthMonitoringEnabled(autoDepthMonitoring);
        alertSystem->setProximityMonitoringEnabled(autoProximityMonitoring);
        alertSystem->setMinimumDepth(depthAlertThreshold);
        alertSystem->setProximityThreshold(proximityAlertThreshold);

        qDebug() << "[ECWIDGET] Alert System initialized successfully";
        emit alertSystemStatusChanged(true);

    } catch (const std::exception& e) {
        qCritical() << "[ECWIDGET] Failed to initialize Alert System:" << e.what();
        alertSystem = nullptr;  // TAMBAHAN: Ensure null on failure
    } catch (...) {
        qCritical() << "[ECWIDGET] Unknown error initializing Alert System";
        alertSystem = nullptr;  // TAMBAHAN: Ensure null on failure
    }
}

void EcWidget::checkAlertConditions()
{
    if (!alertSystem || !alertMonitoringEnabled) {
        return;
    }

    // Update current position in alert system
    if (navShip.lat != 0.0 && navShip.lon != 0.0) {
        alertSystem->updateOwnShipPosition(navShip.lat, navShip.lon, navShip.depth);
    } else {
        alertSystem->updateOwnShipPosition(ownShip.lat, ownShip.lon, 0.0);
    }
}

void EcWidget::triggerNavigationAlert(const QString& message, int priority)
{
    if (!alertSystem) {
        qWarning() << "[ECWIDGET] Cannot trigger navigation alert - Alert System not initialized";
        return;
    }

    AlertPriority alertPriority = static_cast<AlertPriority>(priority);
    alertSystem->triggerAlert(ALERT_NAVIGATION_WARNING, alertPriority,
                              tr("Navigation Alert"),
                              message,
                              "Navigation_System",
                              navShip.lat, navShip.lon);
}

void EcWidget::triggerNavigationAlert(const QString& message)
{
    triggerNavigationAlert(message, 2); // Default PRIORITY_MEDIUM = 2
}

void EcWidget::triggerDepthAlert(double currentDepth, double threshold, bool isShallow)
{
    if (!alertSystem) {
        qWarning() << "[ECWIDGET] Cannot trigger depth alert - Alert System not initialized";
        return;
    }

    AlertType alertType = isShallow ? ALERT_DEPTH_SHALLOW : ALERT_DEPTH_DEEP;
    AlertPriority priority = isShallow ? PRIORITY_CRITICAL : PRIORITY_MEDIUM;

    QString title = isShallow ? tr("Shallow Water Alert") : tr("Deep Water Alert");
    QString message = tr("Current depth: %.1f m, Threshold: %.1f m").arg(currentDepth).arg(threshold);

    alertSystem->triggerAlert(alertType, priority, title, message,
                              "Depth_Monitor", navShip.lat, navShip.lon);
}

void EcWidget::triggerGuardZoneAlert(int guardZoneId, const QString& details)
{
    if (!alertSystem) {
        qWarning() << "[ECWIDGET] Cannot trigger guardzone alert - Alert System not initialized";
        return;
    }

    QString title = tr("GuardZone Alert");
    QString message = tr("GuardZone %1: %2").arg(guardZoneId).arg(details);
    QString source = QString("GuardZone_%1").arg(guardZoneId);

    alertSystem->triggerAlert(ALERT_GUARDZONE_PROXIMITY, PRIORITY_HIGH,
                              title, message, source,
                              navShip.lat, navShip.lon);
}

// SLOT IMPLEMENTATIONS - GUNAKAN CONST REFERENCE
void EcWidget::onAlertTriggered(const AlertData& alert)
{
    qDebug() << "[ECWIDGET] Alert triggered:" << alert.title;

    // Show visual feedback on chart
    if (alert.priority >= PRIORITY_HIGH) {
        showVisualFeedback(alert.title,
                           alert.priority == PRIORITY_CRITICAL ? FEEDBACK_ERROR : FEEDBACK_WARNING);
    }

    // Emit signal for external handling (e.g., by MainWindow)
    emit alertTriggered(alert);

    // Force chart update if location-based alert
    if (alert.latitude != 0.0 && alert.longitude != 0.0) {
        update();
    }
}

void EcWidget::onCriticalAlert(const AlertData& alert)
{
    qWarning() << "[ECWIDGET] CRITICAL ALERT:" << alert.title << "-" << alert.message;

    // Show prominent visual feedback
    showVisualFeedback(QString("CRITICAL: %1").arg(alert.title), FEEDBACK_ERROR);

    // Force immediate chart update
    update();

    // Emit critical alert signal
    emit criticalAlertTriggered(alert);
}

void EcWidget::onAlertSystemStatusChanged(bool enabled)
{
    qDebug() << "[ECWIDGET] Alert System status changed:" << enabled;
    alertMonitoringEnabled = enabled;

    if (enabled) {
        if (alertCheckTimer && !alertCheckTimer->isActive()) {
            alertCheckTimer->start();
        }
    } else {
        if (alertCheckTimer && alertCheckTimer->isActive()) {
            alertCheckTimer->stop();
        }
    }

    emit alertSystemStatusChanged(enabled);
}

void EcWidget::performPeriodicAlertChecks()
{
    if (!alertSystem || !alertMonitoringEnabled) {
        return;
    }

    lastAlertCheck = QDateTime::currentDateTime();
    checkAlertConditions();

    // Check for rapid depth changes
    if (navShip.lat != 0.0 && navShip.lon != 0.0 && navShip.depth > 0.0) {
        double depthDifference = abs(navShip.depth - lastDepthReading);

        if (depthDifference > 10.0 && lastDepthReading > 0.0) {
            triggerNavigationAlert(tr("Rapid depth change detected: %.1f m/check")
                                       .arg(depthDifference), 3); // 3 = PRIORITY_HIGH
        }

        lastDepthReading = navShip.depth;
    }
}

bool EcWidget::hasAISData() const
{
    return (_aisObj != nullptr);
}

QList<AISTargetData> EcWidget::getAISTargets() const
{
    aisTargetsMutex.lock();
    QList<AISTargetData> result = currentAISTargets;
    aisTargetsMutex.unlock();
    return result;
}

int EcWidget::getAISTargetCount() const
{
    aisTargetsMutex.lock();
    int count = currentAISTargets.size();
    aisTargetsMutex.unlock();
    return count;
}

void EcWidget::updateAISTargetsList()
{
    aisTargetsMutex.lock();
    currentAISTargets.clear();

    if (!_aisObj) {
        aisTargetsMutex.unlock();
        return;
    }

    // Simulasi data dari file BolivarToTexasCity.dat
    AISTargetData target1;
    target1.mmsi = "367123456";
    target1.lat = 29.3989;
    target1.lon = -94.7853;
    target1.cog = 45.0;
    target1.sog = 12.5;
    target1.lastUpdate = QDateTime::currentDateTime();
    currentAISTargets.append(target1);

    AISTargetData target2;
    target2.mmsi = "367789012";
    target2.lat = 29.4156;
    target2.lon = -94.7234;
    target2.cog = 225.0;
    target2.sog = 8.2;
    target2.lastUpdate = QDateTime::currentDateTime();
    currentAISTargets.append(target2);

    AISTargetData target3;
    target3.mmsi = "367654321";
    target3.lat = 29.3845;
    target3.lon = -94.8123;
    target3.cog = 90.0;
    target3.sog = 6.8;
    target3.lastUpdate = QDateTime::currentDateTime();
    currentAISTargets.append(target3);

    //qDebug() << "Updated AIS targets list with" << currentAISTargets.size() << "targets";
    aisTargetsMutex.unlock();
}

void EcWidget::addOrUpdateAISTarget(const AISTargetData& target)
{
    aisTargetsMutex.lock();

    // Update existing or add new
    for (int i = 0; i < currentAISTargets.size(); ++i) {
        if (currentAISTargets[i].mmsi == target.mmsi) {
            currentAISTargets[i] = target;
            aisTargetsMutex.unlock();
            return;
        }
    }

    // Add new target
    currentAISTargets.append(target);
    aisTargetsMutex.unlock();
}

void EcWidget::setRedDotTrackerEnabled(bool enabled)
{
    redDotTrackerEnabled = enabled;
    qDebug() << "Red Dot Tracker enabled:" << enabled;

    if (!enabled) {
        redDotLat = 0.0;
        redDotLon = 0.0;
    }

    update();
}

void EcWidget::setRedDotAttachedToShip(bool attached)
{
    qDebug() << "[EMERGENCY-DEBUG] ========== setRedDotAttachedToShip CALLED ==========" << attached;
    qDebug() << "[EMERGENCY-DEBUG] Current attachedGuardZoneId:" << attachedGuardZoneId;

    // CRITICAL: Wrap entire function to catch crashes
    try {
        redDotAttachedToShip = attached;
        qDebug() << "[EMERGENCY-DEBUG] redDotAttachedToShip set to:" << attached;

    if (attached) {
        // PERBAIKAN CRITICAL: Jangan enable redDotTrackerEnabled jika ada attached guardzone
        // karena akan menyebabkan double rendering
        if (attachedGuardZoneId == -1) {
            redDotTrackerEnabled = true;
            qDebug() << "[ATTACH] Enabled redDotTracker (no attached guardzone)";
        } else {
            redDotTrackerEnabled = false;
            qDebug() << "[ATTACH] Disabled redDotTracker (attached guardzone exists)";
        }

        // PERBAIKAN CRITICAL: Matikan shipGuardianEnabled untuk mencegah konflik rendering
        shipGuardianEnabled = false;
        qDebug() << "[ATTACH] Disabled shipGuardian to prevent rendering conflict";

        // Buat guardzone di manager hanya jika belum ada
        if (attachedGuardZoneId == -1) {
            qDebug() << "[EMERGENCY-DEBUG] No attached guardzone found, creating new one";

            // CRITICAL: Extra protection for createAttachedGuardZone
            try {
                qDebug() << "[EMERGENCY-DEBUG] About to call createAttachedGuardZone...";
                createAttachedGuardZone();
                qDebug() << "[EMERGENCY-DEBUG] createAttachedGuardZone completed successfully";
            } catch (const std::exception& e) {
                qDebug() << "[EMERGENCY-ERROR] Exception in createAttachedGuardZone:" << e.what();
                // Don't continue if creation failed
                return;
            } catch (...) {
                qDebug() << "[EMERGENCY-ERROR] Unknown exception in createAttachedGuardZone";
                // Don't continue if creation failed
                return;
            }

            // PERBAIKAN: Setelah guardzone dibuat, disable redDotTrackerEnabled
            redDotTrackerEnabled = false;
            qDebug() << "[ATTACH] Disabled redDotTracker after creating attached guardzone";
        } else {
            qDebug() << "[DEBUG] Attached guardzone already exists, ID:" << attachedGuardZoneId;
            // PERBAIKAN: Pastikan redDotTrackerEnabled disabled
            redDotTrackerEnabled = false;
            qDebug() << "[ATTACH] Disabled redDotTracker for existing attached guardzone";

            // Emit signal untuk update UI saja
            emit attachToShipStateChanged(true);
        }

        // PERBAIKAN: Gunakan guardZone auto-check timer, bukan ship guardian timer
        if (!guardZoneAutoCheckEnabled) {
            guardZoneAutoCheckEnabled = true;
            guardZoneAutoCheckTimer->start(guardZoneCheckInterval);
            qDebug() << "[AUTO-ACTIVATION] GuardZone auto-check automatically enabled for attached guardzone";
        }

        qDebug() << "Ship Guardian activated with obstacle detection";

        // Try to get current ship position (prefer navShip)
        if ((navShip.lat != 0.0 && navShip.lon != 0.0)) {
            updateRedDotPosition(navShip.lat, navShip.lon);
        } else if (ownShip.lat != 0.0 && ownShip.lon != 0.0) {
            updateRedDotPosition(ownShip.lat, ownShip.lon);
        }

    } else {
        qDebug() << "[DETACH] Removing attached guardzone and cleaning up...";

        // Hapus guardzone dari manager
        removeAttachedGuardZone();

        // PERBAIKAN: Cek apakah masih ada guardzone aktif lain
        bool hasOtherActiveGuardZones = false;
        for (const GuardZone& gz : guardZones) {
            if (gz.active) {
                hasOtherActiveGuardZones = true;
                break;
            }
        }

        // Stop auto-check hanya jika tidak ada guardzone aktif lain
        if (!hasOtherActiveGuardZones) {
            guardZoneAutoCheckEnabled = false;
            guardZoneAutoCheckTimer->stop();
            qDebug() << "[AUTO-CHECK] GuardZone auto-check disabled - no active guardzones";
        }

        // PERBAIKAN: Clear red dot tracker data ketika detach
        redDotTrackerEnabled = false;
        redDotLat = 0.0;
        redDotLon = 0.0;

        // Clear ship guardian specific data
        shipGuardianCheckTimer->stop();
        lastDetectedObstacles.clear();

        qDebug() << "Ship Guardian deactivated and red dot tracker cleared";

        // Emit signal untuk update UI - detach
        qDebug() << "[EMERGENCY-DEBUG] Emitting attachToShipStateChanged(false)...";
        emit attachToShipStateChanged(false);
        qDebug() << "[EMERGENCY-DEBUG] Signal emitted successfully";
    }

    qDebug() << "[ATTACH-DEBUG] Calling update() to repaint with protection...";
    // CRITICAL: Protected update() call
    try {
        update(); // Force repaint
        qDebug() << "[ATTACH-DEBUG] update() completed successfully";
    } catch (const std::exception& e) {
        qDebug() << "[ATTACH-ERROR] Exception during update():" << e.what();
    } catch (...) {
        qDebug() << "[ATTACH-ERROR] Unknown exception during update()";
    }
    qDebug() << "[EMERGENCY-DEBUG] ========== setRedDotAttachedToShip COMPLETED ==========";

    } catch (const std::exception& e) {
        qDebug() << "[EMERGENCY-ERROR] Exception in setRedDotAttachedToShip:" << e.what();
        // Try to clean up safely
        redDotAttachedToShip = false;
        redDotTrackerEnabled = false;
    } catch (...) {
        qDebug() << "[EMERGENCY-ERROR] Unknown exception in setRedDotAttachedToShip";
        // Try to clean up safely
        redDotAttachedToShip = false;
        redDotTrackerEnabled = false;
    }
}

bool EcWidget::isRedDotTrackerEnabled() const
{
    return redDotTrackerEnabled;
}

bool EcWidget::isRedDotAttachedToShip() const
{
    return redDotAttachedToShip;
}

bool EcWidget::hasAttachedGuardZone() const
{
    // PERBAIKAN: Cek baik dari ID maupun dari list guardzone
    if (attachedGuardZoneId != -1) {
        qDebug() << "[DEBUG] hasAttachedGuardZone() - found by ID:" << attachedGuardZoneId;
        return true;
    }

    // Cek juga dari guardzone list
    for (const GuardZone& gz : guardZones) {
        if (gz.attachedToShip &&
            !gz.name.contains("Ship Guardian Circle") &&
            !gz.name.contains("Red Dot Guardian")) {
            qDebug() << "[DEBUG] hasAttachedGuardZone() - found in list:" << gz.name << "ID:" << gz.id;
            return true;
        }
    }

    qDebug() << "[DEBUG] hasAttachedGuardZone() - not found";
    return false;
}

void EcWidget::updateRedDotPosition(double lat, double lon)
{
    // qDebug() << "[UPDATE-RED-DOT] Called with lat:" << lat << "lon:" << lon
    //         << "enabled:" << redDotTrackerEnabled << "attached:" << redDotAttachedToShip;

    if (!redDotAttachedToShip) {
        qDebug() << "[UPDATE-RED-DOT] Early return - not attached to ship";
        return;
    }

    redDotLat = lat;
    redDotLon = lon;

    // PERBAIKAN: Update ownShip position untuk consistency
    if (redDotAttachedToShip) {
        ownShip.lat = lat;
        ownShip.lon = lon;
        qDebug() << "[UPDATE-OWNSHIP] Synchronized ownShip position with redDot:" << lat << "," << lon;
    }

    // TAMBAHAN: Update posisi di GuardZone Manager
    if (attachedGuardZoneId != -1) {
        for (GuardZone& gz : guardZones) {
            if (gz.id == attachedGuardZoneId) {
                gz.centerLat = lat;
                gz.centerLon = lon;

                // PERBAIKAN: Update angle untuk semicircle shield berdasarkan heading
                if (gz.shape == GUARD_ZONE_CIRCLE && gz.startAngle != gz.endAngle) {
                    double currentHeading = ownShip.heading;
                    if (qIsNaN(currentHeading) || currentHeading < 0) {
                        currentHeading = 0.0;  // Default to North if no heading
                    }

                    // Update semicircle shield angles berdasarkan heading (+90° shift lagi untuk depan)
                    gz.startAngle = fmod(currentHeading + 90.0, 360.0);           // Port-forward (90° + heading)
                    gz.endAngle = fmod(currentHeading + 270.0, 360.0);            // Starboard-forward (270° + heading)

                    qDebug() << "[UPDATE-GUARDZONE] Updated semicircle shield angles - Start:" << gz.startAngle
                             << "End:" << gz.endAngle << "Based on heading:" << currentHeading;
                }

                qDebug() << "[UPDATE-GUARDZONE] Updated attached guardzone position to:" << lat << "," << lon;
                break;
            }
        }
        // Emit signal untuk update panel
        emit guardZoneModified();
    } else {
        // PERBAIKAN: Jika attachedGuardZoneId == -1 tapi ada guardzone attachedToShip, update juga
        for (GuardZone& gz : guardZones) {
            if (gz.attachedToShip &&
                !gz.name.contains("Ship Guardian Circle") &&
                !gz.name.contains("Red Dot Guardian")) {
                gz.centerLat = lat;
                gz.centerLon = lon;

                // PERBAIKAN: Update angle untuk semicircle shield berdasarkan heading
                if (gz.shape == GUARD_ZONE_CIRCLE && gz.startAngle != gz.endAngle) {
                    double currentHeading = ownShip.heading;
                    if (qIsNaN(currentHeading) || currentHeading < 0) {
                        currentHeading = 0.0;  // Default to North if no heading
                    }

                    // Update semicircle shield angles berdasarkan heading (+90° shift lagi untuk depan)
                    gz.startAngle = fmod(currentHeading + 90.0, 360.0);           // Port-forward (90° + heading)
                    gz.endAngle = fmod(currentHeading + 270.0, 360.0);            // Starboard-forward (270° + heading)

                    qDebug() << "[UPDATE-GUARDZONE] Updated semicircle shield angles - Start:" << gz.startAngle
                             << "End:" << gz.endAngle << "Based on heading:" << currentHeading;
                }

                qDebug() << "[UPDATE-GUARDZONE] Updated attached guardzone (no ID) position to:" << lat << "," << lon;
                // Emit signal untuk update panel
                emit guardZoneModified();
                break;
            }
        }
    }

    qDebug() << "Red Dot position updated to:" << lat << "," << lon;
}

void EcWidget::drawRedDotTracker()
{
    qDebug() << "[RED-DOT-DEBUG] drawRedDotTracker called - enabled:" << redDotTrackerEnabled
             << "attachedGuardZoneId:" << attachedGuardZoneId << "redDotLat:" << redDotLat
             << "redDotAttachedToShip:" << redDotAttachedToShip;

    // Check if tracker is enabled and position is valid
    if (!redDotTrackerEnabled || redDotLat == 0.0 || redDotLon == 0.0) {
        // qDebug() << "[RED-DOT-DEBUG] Early return - tracker disabled or invalid position";
        return;
    }

    // PERBAIKAN CRITICAL: Jangan gambar red dot area jika redDotAttachedToShip = true
    // Karena ini berarti ada sistem attach to ship yang aktif
    if (redDotAttachedToShip) {
        qDebug() << "[RED-DOT-DEBUG] Early return - redDotAttachedToShip is true, skip rendering";
        return;
    }

    // PERBAIKAN: Jika ada attached guardzone, jangan gambar red dot area
    // Biarkan drawGuardZone() yang menggambar area guardzone
    if (attachedGuardZoneId != -1) {
        // Area guardzone sudah digambar oleh drawGuardZone()
        // Tidak perlu gambar apapun di sini
        // qDebug() << "[RED-DOT-DEBUG] Early return - attached guardzone exists, let drawGuardZone handle it";
        return;
    }

    // PERBAIKAN CRITICAL: Cek apakah ada guardzone dengan attachedToShip = true
    bool hasAttachedGuardZone = false;
    for (const GuardZone& gz : guardZones) {
        if (gz.attachedToShip &&
            !gz.name.contains("Ship Guardian Circle") && !gz.name.contains("Red Dot Guardian")) {
            hasAttachedGuardZone = true;
            qDebug() << "[RED-DOT-DEBUG] Found attached guardzone in list:" << gz.name;
            break;
        }
    }

    if (hasAttachedGuardZone) {
        qDebug() << "[RED-DOT-DEBUG] Early return - found attached guardzone in list, skip rendering";
        return;
    }

    qDebug() << "[RED-DOT-DEBUG] Drawing red dot tracker and ship guardian circle...";

    // Convert geographic coordinates to screen coordinates
    int screenX, screenY;
    if (!LatLonToXy(redDotLat, redDotLon, screenX, screenY)) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // TAHAP 2: Perbesar ukuran menggunakan nautical miles seperti guardzone
    double nauticalMilesRadius = 0.2; // 0.2 NM untuk red dot (lebih besar dari 8 piksel)
    double pixelRadius = calculatePixelsFromNauticalMiles(nauticalMilesRadius);

    // Pastikan minimum radius untuk visibility
    if (pixelRadius < 15.0) {
        pixelRadius = 15.0; // Minimum 15 piksel
    }

    // TAHAP 3: Warna transparan seperti guardzone
    QColor fillColor(255, 0, 0, 50);    // Transparan seperti guardianFillColor
    QColor borderColor(255, 0, 0, 150); // Border seperti guardianBorderColor
    QColor centerColor(255, 0, 0, 200); // Center dot yang lebih solid

    // Draw guardian circle first if enabled
    if (shipGuardianEnabled) {
        drawShipGuardianCircle();
    }

    // PERBAIKAN CRITICAL: Jangan gambar circle jika ada attached guardzone
    if (attachedGuardZoneId != -1) {
        qDebug() << "[RED-DOT-DEBUG] Skipping main circle draw - attached guardzone exists";
        return;
    }

    // PERBAIKAN CRITICAL: Jangan gambar circle jika redDotAttachedToShip = true
    if (redDotAttachedToShip) {
        qDebug() << "[RED-DOT-DEBUG] Skipping main circle draw - redDotAttachedToShip is true";
        return;
    }

    // MODIFIKASI UTAMA: Draw filled circle area (seperti guardzone)
    qDebug() << "[RED-DOT-DEBUG] Drawing main red dot circle at:" << screenX << "," << screenY << "radius:" << pixelRadius;
    painter.setBrush(QBrush(fillColor));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(screenX, screenY),
                       (int)pixelRadius,
                       (int)pixelRadius);

    // Draw border circle (seperti guardzone border) - TIDAK PERLU LAGI
    // Karena circle sudah handle di drawGuardZone() untuk attached guardzone

    // Draw center dot untuk menandai posisi ship yang tepat - MINIMAL SAJA
    painter.setBrush(QBrush(centerColor));
    painter.setPen(QPen(centerColor, 1));
    painter.drawEllipse(QPoint(screenX, screenY), 4, 4); // Small center dot 4px

    // Draw white border untuk center dot agar terlihat jelas
    painter.setPen(QPen(Qt::white, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPoint(screenX, screenY), 5, 5); // White outline

    qDebug() << "Enhanced Red Dot drawn at:" << screenX << "," << screenY
             << "with radius:" << pixelRadius << "pixels (" << nauticalMilesRadius << "NM)";
}


void EcWidget::testRedDot()
{
    qDebug() << "=== RED DOT TEST ===";
    qDebug() << "redDotTrackerEnabled:" << redDotTrackerEnabled;
    qDebug() << "redDotAttachedToShip:" << redDotAttachedToShip;
    qDebug() << "redDotLat:" << redDotLat;
    qDebug() << "redDotLon:" << redDotLon;
    qDebug() << "ownShip.lat:" << ownShip.lat;
    qDebug() << "ownShip.lon:" << ownShip.lon;

    // Force set position for test
    if (ownShip.lat != 0.0 && ownShip.lon != 0.0) {
        updateRedDotPosition(ownShip.lat, ownShip.lon);
        update();
        qDebug() << "Test red dot position set and update called";
    } else {
        qDebug() << "Cannot test - no ownship position available";
    }
}


void EcWidget::setShipGuardianEnabled(bool enabled)
{
    shipGuardianEnabled = enabled;
    redDotTrackerEnabled = enabled;

    if (enabled) {
        // Buat red dot guardian di GuardZone Manager
        createRedDotGuardian();

        // Set posisi ke ship position
        if (ownShip.lat != 0.0 && ownShip.lon != 0.0) {
            updateRedDotPosition(ownShip.lat, ownShip.lon);
        }
    } else {
        // Hapus red dot guardian dari GuardZone Manager
        removeRedDotGuardian();

        redDotLat = 0.0;
        redDotLon = 0.0;
    }

    qDebug() << "Ship Guardian Circle enabled:" << enabled;
    update();
}

bool EcWidget::isShipGuardianEnabled() const
{
    return shipGuardianEnabled;
}

void EcWidget::setGuardianRadius(double radius)
{
    if (radius > 0.0 && radius <= 5.0) {  // Limit to reasonable range
        guardianRadius = radius;
        qDebug() << "Guardian radius set to:" << radius << "nautical miles";
        update();
    }
}

double EcWidget::getGuardianRadius() const
{
    return guardianRadius;
}

void EcWidget::drawShipGuardianCircle()
{
    if (!shipGuardianEnabled || redDotLat == 0.0 || redDotLon == 0.0) {
        return;
    }

    // PERBAIKAN: Jangan gambar ship guardian circle jika ada attached guardzone
    if (attachedGuardZoneId != -1) {
        qDebug() << "[SHIP-GUARDIAN] Skipping draw - attached guardzone exists:" << attachedGuardZoneId;
        return;
    }

    int centerX, centerY;
    if (!LatLonToXy(redDotLat, redDotLon, centerX, centerY)) {
        return;
    }

    // Convert nautical miles to pixels
    double radiusInPixels = calculatePixelsFromNauticalMiles(guardianRadius);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw filled circle (guardian area)
    qDebug() << "[SHIP-GUARDIAN] Drawing ship guardian circle at:" << centerX << "," << centerY << "radius:" << radiusInPixels;
    painter.setBrush(QBrush(guardianFillColor));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(centerX, centerY),
                      (int)radiusInPixels,
                      (int)radiusInPixels);

    // Draw border circle
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(guardianBorderColor, 2));
    painter.drawEllipse(QPoint(centerX, centerY),
                      (int)radiusInPixels,
                      (int)radiusInPixels);

    qDebug() << "Guardian circle drawn at" << centerX << "," << centerY
             << "with radius" << radiusInPixels << "pixels";
}

void EcWidget::drawShipGuardianSquare(double aisLat, double aisLon)
{
    if (!shipGuardianEnabled || aisLat == 0.0 || aisLon == 0.0) {
        return;
    }

    int centerX, centerY;
    if (!LatLonToXy(aisLat, aisLon, centerX, centerY)) {
        return;
    }

    // Convert nautical miles to pixels
    double radiusInPixels = calculatePixelsFromNauticalMiles(guardianRadius);

    // Hitung sisi persegi berdasarkan "radius"
    int sideLength = (int)(radiusInPixels * 2);
    int topLeftX = centerX - (sideLength / 2);
    int topLeftY = centerY - (sideLength / 2);

    QRect squareRect(topLeftX, topLeftY, sideLength, sideLength);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw filled square (guardian area)
    painter.setBrush(QBrush(guardianFillColor));
    painter.setPen(Qt::NoPen);
    painter.drawRect(squareRect);

    // Draw border square
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(guardianBorderColor, 2));
    painter.drawRect(squareRect);

    qDebug() << "Guardian square drawn at center"
             << centerX << "," << centerY
             << "with side length" << sideLength << "pixels";
}

// Implementasi fungsi createAISTooltip
void EcWidget::createAISTooltip()
{
    // Gunakan subclass AISTooltip yang menggambar transparansi sendiri
    aisTooltip = new AISTooltip(this);
    aisTooltip->hide();

    QVBoxLayout* mainLayout = static_cast<QVBoxLayout*>(aisTooltip->layout());

    tooltipMMSI = new QLabel("MMSI: ", aisTooltip);
    tooltipObjectName = new QLabel("NAME: ", aisTooltip);
    tooltipCOG = new QLabel("COG: ", aisTooltip);
    tooltipSOG = new QLabel("SOG: ", aisTooltip);
    tooltipAntennaLocation = new QLabel("POS: ", aisTooltip);
    //tooltipTypeOfShip = new QLabel("SHIP TYPE: ", aisTooltip);
    //tooltipTrackStatus = new QLabel("STATUS: ", aisTooltip);
    //tooltipShipBreadth = new QLabel("Ship breadth (beam): ", aisTooltip);
    //tooltipShipLength = new QLabel("Ship length over all: ", aisTooltip);
    //tooltipShipDraft = new QLabel("Ship Draft: ", aisTooltip);
    //tooltipNavStatus = new QLabel("Nav Status: ", aisTooltip);
    //tooltipCallSign = new QLabel("Ship Call Sign: ", aisTooltip);
    //tooltipPositionSensor = new QLabel("Position Sensor Indication: ", aisTooltip);
    //tooltipListOfPorts = new QLabel("List of Ports: ", aisTooltip);

    QList<QLabel*> labels = {
        tooltipMMSI,
        tooltipObjectName,
        tooltipCOG,
        tooltipSOG,
        tooltipAntennaLocation
        //tooltipTypeOfShip,
        //tooltipTrackStatus,
        //tooltipShipBreadth,
        //tooltipShipLength,
        //tooltipShipDraft,
        //tooltipNavStatus,
        //tooltipCallSign,
        //tooltipPositionSensor,
        //tooltipListOfPorts,
    };

    for (QLabel* label : labels) {
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setWordWrap(false);
        label->setTextInteractionFlags(Qt::NoTextInteraction);
        label->setStyleSheet(
            "QLabel {"
            "background-color: transparent;"
            "border: none;"
            "color: #333333;"
            "font-family: 'Segoe UI', Arial, sans-serif;"
            "font-size: 11px;"
            "font-weight: normal;"
            "padding: 1px 2px;"
            "margin: 0px;"
            "}"
        );
        mainLayout->addWidget(label);
    }

    aisTooltip->setFixedWidth(250);
}

// Implementasi fungsi updateAISTooltipContent
void EcWidget::updateAISTooltipContent(EcAISTargetInfo* ti)
{
    if (!aisTooltip || !ti) return;

    // Gunakan field yang benar dari EcAISTargetInfo
    QString objectName = QString(ti->shipName).trimmed();
    if (objectName.isEmpty()) objectName = QString::number(ti->mmsi);
    while (objectName.endsWith('@')) {
        objectName.chop(1);
    }

    // Untuk field yang tidak ada, gunakan nilai default seperti di panel
    QString shipBreadth = "7.00";  // Default value
    QString shipLength = "18.00";  // Default value
    QString cogValue = QString("%1 °T").arg(ti->cog / 10.0, 0, 'f', 2);
    QString sogValue = QString("%1 kn").arg(ti->sog / 10.0, 0, 'f', 2);
    QString shipDraft = "3.00";    // Default value

    QString typeOfShip = getShipTypeString(ti->shipType);
    QString navStatus = getNavStatusString(ti->navStatus);
    QString mmsiValue = QString::number(ti->mmsi);
    QString callSign = QString(ti->callSign).trimmed();
    QString destination = QString(ti->destination).trimmed();
    //QString trackStatus = QString(ti->trackingStatus == 2 ? "Dangerous" : "Tracking");

    double lat = ((double)ti->latitude / 10000.0) / 60.0;
    double lon = ((double)ti->longitude / 10000.0) / 60.0;
    QString antennaLocation = QString("%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);

    // Update semua label
    tooltipMMSI->setText(QString("MMSI: %1").arg(mmsiValue));
    tooltipObjectName->setText(QString("NAME: %1").arg(objectName));
    tooltipCOG->setText(QString("COG: %1").arg(cogValue));
    tooltipSOG->setText(QString("SOG: %1").arg(sogValue));
    tooltipAntennaLocation->setText(QString("POS: %1").arg(antennaLocation));
    //tooltipTypeOfShip->setText(QString("SHIP TYPE: %1").arg(typeOfShip));
    //tooltipTrackStatus->setText(QString("STATUS: %1").arg(trackStatus));
    //tooltipShipBreadth->setText(QString("Ship breadth (beam): %1").arg(shipBreadth));
    //tooltipShipLength->setText(QString("Ship length over all: %1").arg(shipLength));
    //tooltipShipDraft->setText(QString("Ship Draft: %1").arg(shipDraft));
    //tooltipNavStatus->setText(QString("Nav Status: %1").arg(navStatus));
    //tooltipCallSign->setText(QString("Ship Call Sign: %1").arg(callSign));
    //tooltipPositionSensor->setText(QString("Position Sensor Indication: GPS"));
    //tooltipListOfPorts->setText(QString("List of Ports: %1").arg(destination));

    aisTooltip->adjustSize();
}

// Implementasi fungsi hideAISTooltip
void EcWidget::hideAISTooltip()
{
    if (aisTooltip) {
        aisTooltip->hide();
    }
    currentTooltipTarget = nullptr;
    if (aisTooltipUpdateTimer) {
        aisTooltipUpdateTimer->stop();
    }
    isAISTooltipVisible = false;
}

// Tambahkan fungsi untuk mencari AIS target di posisi mouse
EcAISTargetInfo* EcWidget::findAISTargetInfoAtPosition(const QPoint& mousePos)
{
    if (!hasAISData()) return nullptr;

    // Konversi screen coordinates ke lat/lon
    EcCoordinate clickLat, clickLon;
    if (!XyToLatLon(mousePos.x(), mousePos.y(), clickLat, clickLon)) {
        return nullptr;
    }

    // Ambil semua AIS target info lengkap
    QMap<unsigned int, EcAISTargetInfo>& targetInfos = Ais::instance()->getTargetInfoMap();

    // Toleransi dalam pixel untuk deteksi hover
    const int tolerancePixels = 20;

    EcAISTargetInfo* closestTarget = nullptr;
    double closestDistance = tolerancePixels + 1;

    for (auto it = targetInfos.begin(); it != targetInfos.end(); ++it) {
        EcAISTargetInfo& targetInfo = it.value();

        // Konversi posisi AIS target ke screen coordinates
        double lat = ((double)targetInfo.latitude / 10000.0) / 60.0;
        double lon = ((double)targetInfo.longitude / 10000.0) / 60.0;

        int targetX, targetY;
        if (LatLonToXy(lat, lon, targetX, targetY)) {
            // Hitung jarak dalam pixels
            int deltaX = mousePos.x() - targetX;
            int deltaY = mousePos.y() - targetY;
            double distance = sqrt(deltaX * deltaX + deltaY * deltaY);

            if (distance <= tolerancePixels && distance < closestDistance) {
                closestDistance = distance;
                closestTarget = &targetInfo;
            }
        }
    }

    return closestTarget;
}

// Tambahkan fungsi slot untuk check mouse over AIS target
void EcWidget::checkMouseOverAISTarget()
{
    EcAISTargetInfo* targetInfo = findAISTargetInfoAtPosition(lastMousePos);

    if (targetInfo) {
        if (!isAISTooltipVisible) {
            showAISTooltipFromTargetInfo(lastMousePos, targetInfo);
            isAISTooltipVisible = true;
        }
        // else: tooltip sudah tampil, biarkan update jalan
    } else {
        if (isAISTooltipVisible) {
            hideAISTooltip();
        }
    }
}

AISTargetData EcWidget::getEnhancedAISTargetData(const QString& mmsi)
{
    AISTargetData enhancedData;

    // Ambil data dari map yang sudah ada
    QMap<unsigned int, AISTargetData>& targets = Ais::instance()->getTargetMap();
    unsigned int mmsiInt = mmsi.toUInt();

    if (targets.contains(mmsiInt)) {
        enhancedData = targets[mmsiInt];
    }

    // TODO: Di sini bisa ditambahkan pengambilan data tambahan dari transponder
    // untuk mendapatkan ship name, call sign, dll yang tidak ada di AISTargetData

    return enhancedData;
}

void EcWidget::leaveEvent(QEvent *event)
{
    // Sembunyikan tooltip ketika mouse keluar dari widget
    aisTooltipTimer->stop();
    if (isAISTooltipVisible) {
        hideAISTooltip();
        isAISTooltipVisible = false;
    }

    QWidget::leaveEvent(event);
}

QString EcWidget::getShipTypeString(int shipType)
{
    switch(shipType) {
        //case 80: return "Vessel - Towing";
        case 0:   return "Not available / Not applicable";
        case 30:  return "Fishing vessel";
        case 35:  return "Military operations";
        case 36:  return "Sailing";
        case 37:  return "Pleasure craft";
        case 60:  return "Passenger ship";
        case 61:  return "Passenger ship (carrying DG, HS, or MP)";
        case 70:  return "Cargo ship";
        case 71:  return "Cargo ship (carrying DG, HS, or MP)";
        case 80:  return "Tanker ship";
        case 8:  return "Tanker ship (carrying DG, HS, or MP)";
        case 255:  return "Invalid (EC_AIS_INVALID_SHIP_TYPE)";
        default: return "unknown";
    }
}

QString EcWidget::getNavStatusString(int navStatus)
{
    // Gunakan konstanta yang tersedia
    switch(navStatus) {
        case 0: return "Under way using engine";
        case 1: return "At anchor";
        case 2: return "Not under command";
        case 3: return "Restricted manoeuvrability";
        case 5: return "Moored";
        case 8: return "Under way sailing";
        case 15: return "Undefined";
        default: return "Under way sailing";
    }
}

void EcWidget::showAISTooltipFromTargetInfo(const QPoint& position, EcAISTargetInfo* targetInfo)
{
    if (!aisTooltip || !targetInfo)
        return;

    currentTooltipTarget = targetInfo;
    updateAISTooltipContent(currentTooltipTarget);  // Isi awal

    // Posisi tooltip relatif terhadap kursor
    QPoint tooltipPos = mapToGlobal(position) + QPoint(15, 15);
    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    QSize tooltipSize = aisTooltip->sizeHint();

    // Koreksi posisi jika melebihi layar
    if (tooltipPos.x() + tooltipSize.width() > screenGeometry.right()) {
        tooltipPos.setX(mapToGlobal(QPoint(position.x() - tooltipSize.width() - 15, position.y())).x());
    }
    if (tooltipPos.y() + tooltipSize.height() > screenGeometry.bottom()) {
        tooltipPos.setY(mapToGlobal(QPoint(position.x(), position.y() - tooltipSize.height() - 15)).y());
    }

    aisTooltip->move(tooltipPos);
    aisTooltip->show();
    aisTooltip->raise();

    // Mulai timer update isi tooltip
    if (!aisTooltipUpdateTimer) {
        aisTooltipUpdateTimer = new QTimer(this);
        connect(aisTooltipUpdateTimer, &QTimer::timeout, this, &EcWidget::updateTooltipIfVisible);
    }
    aisTooltipUpdateTimer->start(1000);  // 1 detik
}

void EcWidget::updateTooltipIfVisible()
{
    if (aisTooltip && aisTooltip->isVisible() && currentTooltipTarget) {
        updateAISTooltipContent(currentTooltipTarget);
    }
}

// icon ownship
void EcWidget::drawOwnShipIcon(QPainter& painter, int x, int y, double cog, double heading, double sog)
{
    // Selalu gunakan currentScale untuk perhitungan yang konsisten saat drag
    double viewRangeNM = GetRange(currentScale);

    if (!isDragging){
        rangeNM = viewRangeNM;  // Update rangeNM hanya untuk kompatibilitas backward
    }

    // Jika zoom terlalu jauh, tampilkan dua lingkaran sebagai simbol ownship
    if (viewRangeNM > 2.0) {
        painter.save();
        painter.setPen(QPen(Qt::black, 2));

        int r1 = 6;   // lingkaran dalam
        int r2 = 12;  // lingkaran luar

        painter.drawEllipse(QPointF(x, y), r2, r2); // Lingkaran luar
        painter.drawEllipse(QPointF(x, y), r1, r1); // Lingkaran dalam

        // Titik kecil di tengah (hitam solid)
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.drawEllipse(QPointF(x, y), 2, 2); // Titik diameter 4px

        painter.restore();
        return;  // jangan lanjut gambar kapal
    }

    else {
        // Ambil dimensi kapal dari settings
        const SettingsData& settings = SettingsManager::instance().data();
        double actualLength = settings.shipLength;   // dalam meter
        double actualBeam = settings.shipBeam;       // dalam meter

        // Konversi meter ke nautical miles (1 NM = 1852 meter)
        double lengthNM = actualLength / 1852.0;
        double beamNM = actualBeam / 1852.0;

        // Hitung pixel per nautical mile berdasarkan current scale (gunakan viewRangeNM yang sudah dihitung di awal)
        // Gunakan drawPixmap height untuk konsistensi, bukan widget height yang bisa berubah saat transform
        int viewHeightPixels = drawPixmap.height();  // tinggi pixmap chart dalam pixel
        if (viewHeightPixels == 0) viewHeightPixels = height(); // fallback jika pixmap belum diinisialisasi
        double pixelsPerNM = viewHeightPixels / (viewRangeNM * 2.0);  // *2 karena range adalah dari center ke edge

        // Konversi dimensi kapal ke pixel
        int outlineLengthPx = int(lengthNM * pixelsPerNM);
        int outlineBeamPx = int(beamNM * pixelsPerNM);

        // Gambar outline kapal jika dimensi cukup besar untuk terlihat
        // Hanya gambar outline jika kapal lebih besar dari icon default (threshold: 50 meter)
        if (actualLength > 50.0 && outlineLengthPx > 60) {
            painter.save();
            painter.translate(x, y);
            painter.rotate(heading);

            // Gambar kerangka kapal berbentuk seperti kapal dengan haluan runcing
            QPainterPath outlinePath;

            double halfLength = outlineLengthPx / 2.0;
            double halfBeam = outlineBeamPx / 2.0;

            // Mulai dari haluan (bow) - ujung depan runcing
            outlinePath.moveTo(0, -halfLength);

            // Sisi kanan dari haluan ke tengah kapal
            outlinePath.lineTo(halfBeam, -halfLength * 0.7);

            // Sisi kanan bagian tengah (paralel)
            outlinePath.lineTo(halfBeam, halfLength * 0.85);

            // Buritan kanan (stern - sedikit miring)
            outlinePath.lineTo(halfBeam * 0.7, halfLength);

            // Buritan tengah
            outlinePath.lineTo(-halfBeam * 0.7, halfLength);

            // Buritan kiri
            outlinePath.lineTo(-halfBeam, halfLength * 0.85);

            // Sisi kiri bagian tengah (paralel)
            outlinePath.lineTo(-halfBeam, -halfLength * 0.7);

            // Kembali ke haluan
            outlinePath.lineTo(0, -halfLength);

            // Gaya outline: garis solid, semi-transparan
            QPen outlinePen(QColor(0, 255, 0, 180), 2, Qt::SolidLine);  // Hijau transparan, solid line
            painter.setPen(outlinePen);
            painter.setBrush(Qt::NoBrush);  // Tanpa fill
            painter.drawPath(outlinePath);

            painter.restore();
        }

        // Skala ikon kapal berdasarkan range (gunakan viewRangeNM yang konsisten)
        double scaleFactor = 0;
        if (viewRangeNM < 1){
            scaleFactor = 1.0;
        }
        else {
            scaleFactor = 0.8;
        }

        painter.save();

        painter.translate(x, y);
        painter.rotate(heading);  // Rotasi kapal sesuai heading

        int shipLength = int(45 * scaleFactor);
        int shipWidth  = int(15 * scaleFactor);

        QPainterPath shipPath;
        shipPath.moveTo(0, -shipLength / 2);  // ujung hidung

        // Sisi kanan
        QPointF control1(shipWidth / 3, -shipLength / 2 + 3);
        QPointF point1(shipWidth / 2, -shipLength / 4);
        shipPath.quadTo(control1, point1);

        QPointF point2(shipWidth / 2, shipLength / 4);
        shipPath.lineTo(point2);

        QPointF control2(shipWidth / 2, shipLength / 2 - 2);
        QPointF point3(shipWidth / 3, shipLength / 2);
        shipPath.quadTo(control2, point3);

        // Buritan datar
        shipPath.lineTo(-shipWidth / 3, shipLength / 2);

        // Sisi kiri (mirror)
        QPointF control3(-shipWidth / 2, shipLength / 2 - 2);
        QPointF point4(-shipWidth / 2, shipLength / 4);
        shipPath.quadTo(control3, point4);

        QPointF point5(-shipWidth / 2, -shipLength / 4);
        shipPath.lineTo(point5);

        QPointF control4(-shipWidth / 3, -shipLength / 2 + 3);
        QPointF point6(0, -shipLength / 2);
        shipPath.quadTo(control4, point6);

        // Gambar kapal
        painter.setBrush(QBrush(QColor(120, 120, 120)));   // Abu-abu
        painter.setPen(QPen(Qt::black, 1));
        painter.drawPath(shipPath);

        // Titik pusat kapal
        painter.setBrush(QBrush(Qt::black));
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(-1, -1, 2, 2);

        // Garis heading
        painter.setPen(QPen(Qt::black, 2));
        painter.drawLine(0, 0, 0, -shipLength / 2 - int(10 * scaleFactor));

        painter.restore();

        // Gambar vektor COG/SOG di luar rotasi
        drawOwnShipVectors(painter, x, y, cog, heading, sog);
    }
}

void EcWidget::drawOwnShipVectors(QPainter& painter, int x, int y, double cog, double heading, double sog)
{
    // Jangan gambar vector jika kecepatan terlalu rendah
    if (sog < 0.5) return;

    // Panjang vector berdasarkan kecepatan (max 60 pixel)
    int vectorLength = qMin(60, qMax(20, (int)(sog * 3)));

    // Toleransi untuk menentukan apakah COG dan heading berbeda
    double angleDifference = qAbs(cog - heading);
    if (angleDifference > 180) angleDifference = 360 - angleDifference;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // GARIS HIJAU PUTUS-PUTUS: Vector COG/SOG (arah pergerakan)
    painter.setPen(QPen(QColor(0, 255, 0), 2, Qt::DashLine));
    double cogRad = cog * M_PI / 180.0;
    int cogEndX = x + (int)(sin(cogRad) * vectorLength);
    int cogEndY = y - (int)(cos(cogRad) * vectorLength);
    painter.drawLine(x, y, cogEndX, cogEndY);

    // GARIS HIJAU SOLID: Vector Heading (arah kepala kapal)
    // Hanya tampilkan jika heading berbeda dari COG dengan toleransi 5 derajat
    if (angleDifference > 5.0) {
        painter.setPen(QPen(QColor(0, 200, 0), 2, Qt::SolidLine));
        double headingRad = heading * M_PI / 180.0;
        int headingEndX = x + (int)(sin(headingRad) * vectorLength);
        int headingEndY = y - (int)(cos(headingRad) * vectorLength);
        painter.drawLine(x, y, headingEndX, headingEndY);
    }

    painter.restore();
}

// Helper function: Menggambar ship outline pada posisi tertentu
void EcWidget::drawShipOutlineAt(QPainter& painter, int x, int y, double heading, double alpha)
{
    const SettingsData& settings = SettingsManager::instance().data();
    double actualLength = settings.shipLength;   // dalam meter
    double actualBeam = settings.shipBeam;       // dalam meter

    // Konversi meter ke nautical miles
    double lengthNM = actualLength / 1852.0;
    double beamNM = actualBeam / 1852.0;

    // Hitung pixel per nautical mile
    double viewRangeNM = GetRange(currentScale);
    int viewHeightPixels = drawPixmap.height();
    if (viewHeightPixels == 0) viewHeightPixels = height();
    double pixelsPerNM = viewHeightPixels / (viewRangeNM * 2.0);

    // Konversi dimensi kapal ke pixel (scaled down untuk prediction)
    int outlineLengthPx = int(lengthNM * pixelsPerNM * 0.8);  // 80% ukuran untuk prediction
    int outlineBeamPx = int(beamNM * pixelsPerNM * 0.8);

    // Minimum size untuk visibility
    if (outlineLengthPx < 20) outlineLengthPx = 20;
    if (outlineBeamPx < 8) outlineBeamPx = 8;

    painter.save();
    painter.translate(x, y);
    painter.rotate(heading);

    // Gambar kerangka kapal
    QPainterPath outlinePath;

    double halfLength = outlineLengthPx / 2.0;
    double halfBeam = outlineBeamPx / 2.0;

    // Mulai dari haluan (bow) - ujung depan runcing
    outlinePath.moveTo(0, -halfLength);

    // Sisi kanan dari haluan ke tengah kapal
    outlinePath.lineTo(halfBeam, -halfLength * 0.7);

    // Sisi kanan bagian tengah (paralel)
    outlinePath.lineTo(halfBeam, halfLength * 0.85);

    // Buritan kanan (stern)
    outlinePath.lineTo(halfBeam * 0.7, halfLength);

    // Buritan tengah
    outlinePath.lineTo(-halfBeam * 0.7, halfLength);

    // Buritan kiri
    outlinePath.lineTo(-halfBeam, halfLength * 0.85);

    // Sisi kiri bagian tengah (paralel)
    outlinePath.lineTo(-halfBeam, -halfLength * 0.7);

    // Kembali ke haluan
    outlinePath.lineTo(0, -halfLength);

    // Gaya outline: dashed line dengan warna abu-abu dan alpha transparency
    QPen outlinePen(QColor(140, 140, 140, (int)alpha), 2.0, Qt::DashLine);
    painter.setPen(outlinePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(outlinePath);

    painter.restore();
}

// Fungsi untuk menggambar prediksi turning (belokan kapal)
void EcWidget::drawTurningPrediction(QPainter& painter, double shipLat, double shipLon, double heading, double cog, double sog, double rot)
{
    // Cek apakah turning prediction enabled di settings
    const SettingsData& settings = SettingsManager::instance().data();
    if (!settings.showTurningPrediction) return;

    // Jangan gambar jika kapal tidak bergerak
    if (sog < 0.5) return;

    // Jika ROT tidak valid atau terlalu kecil, coba deteksi turning dari perbedaan heading dan COG
    bool useRotData = false;
    if (!std::isnan(rot) && qAbs(rot) > 0.1) { // ROT threshold lebih rendah: 0.1 deg/min
        useRotData = true;
    } else {
        // Deteksi turning dari perbedaan heading dan COG
        double headingCogDiff = heading - cog;
        // Normalize to -180 to 180
        while (headingCogDiff > 180) headingCogDiff -= 360;
        while (headingCogDiff < -180) headingCogDiff += 360;

        // Jika perbedaan heading dan COG < 5 derajat, tidak ada turning
        if (qAbs(headingCogDiff) < 5.0) return;

        // Estimasi ROT dari perbedaan heading-COG (asumsi kapal akan align ke COG)
        rot = headingCogDiff * 0.5; // Estimasi sederhana
        useRotData = true;
    }

    if (!useRotData) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Waktu prediksi dari settings (dalam menit)
    int predictionMinutes = settings.predictionTimeMinutes;
    if (predictionMinutes <= 0) predictionMinutes = 3; // Default 3 menit

    // Hitung prediksi posisi menggunakan kernel EcMonitorCalculatePrediction
    // Interval kalkulasi path (selalu 5 detik untuk smooth curve)
    double timeIntervalSec = 5.0;
    int numPoints = (int)((predictionMinutes * 60.0) / timeIntervalSec);

    // Density untuk menggambar ship outlines (dari settings)
    int densityMode = settings.predictionDensity; // 1=Low(20s), 2=Medium(10s), 3=High(5s)
    int drawInterval;
    if (densityMode == 1) {
        drawInterval = 4; // 20 detik (setiap 4 points * 5s)
    } else if (densityMode == 3) {
        drawInterval = 1; // 5 detik (setiap point)
    } else {
        drawInterval = 2; // Default: 10 detik (setiap 2 points)
    }

    QVector<QPoint> predictionPoints;

    EcCoordinate currentLat = shipLat;
    EcCoordinate currentLon = shipLon;
    double currentHeading = heading;

    // Gambar path prediksi
    for (int i = 1; i <= numPoints; ++i) {
        double timeDiff = timeIntervalSec; // 15 detik increment untuk kurva lebih halus

        EcCoordinate predictLat, predictLon, rotLat, rotLon;
        double predictHeading;

        // Panggil kernel function untuk menghitung prediksi
        Bool success = EcMonitorCalculatePrediction(
            timeDiff,           // time difference in seconds
            currentLat,         // current latitude
            currentLon,         // current longitude
            currentHeading,     // heading (in degrees)
            cog,                // course made good
            sog,                // speed (knots)
            rot,                // rate of turn (deg/min)
            &predictLat,        // output: predicted latitude
            &predictLon,        // output: predicted longitude
            &predictHeading,    // output: predicted heading
            &rotLat,            // output: rotation center latitude
            &rotLon             // output: rotation center longitude
        );

        if (success) {
            // Konversi ke screen coordinates
            int px, py;
            if (LatLonToXy(predictLat, predictLon, px, py)) {
                predictionPoints.append(QPoint(px, py));

                // Update current position untuk iterasi berikutnya
                currentLat = predictLat;
                currentLon = predictLon;
                currentHeading = predictHeading;
            }
        }
    }

    // Gambar ship outlines di sepanjang prediction path
    if (predictionPoints.size() > 1) {
        // Store headings untuk setiap prediction point
        QVector<double> predictionHeadings;
        predictionHeadings.reserve(predictionPoints.size());

        // Recalculate untuk mendapatkan heading di setiap point
        EcCoordinate tempLat = shipLat;
        EcCoordinate tempLon = shipLon;
        double tempHeading = heading;

        for (int i = 1; i <= numPoints; ++i) {
            EcCoordinate predictLat, predictLon, rotLat, rotLon;
            double predictHeading;

            Bool success = EcMonitorCalculatePrediction(
                timeIntervalSec, tempLat, tempLon, tempHeading,
                cog, sog, rot,
                &predictLat, &predictLon, &predictHeading,
                &rotLat, &rotLon
            );

            if (success) {
                predictionHeadings.append(predictHeading);
                tempLat = predictLat;
                tempLon = predictLon;
                tempHeading = predictHeading;
            }
        }

        // Gambar ship outlines dengan interval sesuai density setting
        for (int i = 0; i < predictionPoints.size(); i += drawInterval) {
            // Hitung alpha berdasarkan jarak dari posisi saat ini (fade out lebih halus)
            double alpha = 220.0 - (i / (double)predictionPoints.size()) * 70.0;
            if (alpha < 120) alpha = 120;

            // Gambar ship outline dengan PREDICTED HEADING (bukan actual heading)
            // Ini akan membuat ship outline mengikuti turning circle
            if (i < predictionHeadings.size()) {
                drawShipOutlineAt(painter, predictionPoints[i].x(), predictionPoints[i].y(),
                                predictionHeadings[i], alpha);
            }
        }

        // Gambar outline terakhir juga untuk menunjukkan posisi final
        if (!predictionPoints.isEmpty() && !predictionHeadings.isEmpty()) {
            int lastIdx = predictionPoints.size() - 1;
            drawShipOutlineAt(painter, predictionPoints[lastIdx].x(), predictionPoints[lastIdx].y(),
                            predictionHeadings[lastIdx], 180);
        }
    }

    painter.restore();
}


// Implementasi test guardzone methods:
void EcWidget::setTestGuardZoneEnabled(bool enabled)
{
    testGuardZoneEnabled = enabled;
    qDebug() << "Test GuardZone enabled:" << enabled;
    update(); // Trigger repaint
}

bool EcWidget::isTestGuardZoneEnabled() const
{
    return testGuardZoneEnabled;
}

void EcWidget::setTestGuardZoneRadius(double radius)
{
    if (radius > 0) {
        testGuardZoneRadius = radius;
        qDebug() << "Test GuardZone radius set to:" << radius << "NM";
        if (testGuardZoneEnabled) {
            update(); // Trigger repaint if enabled
        }
    }
}

double EcWidget::getTestGuardZoneRadius() const
{
    return testGuardZoneRadius;
}

// Method untuk menggambar test guardzone:
void EcWidget::drawTestGuardZone(QPainter& painter)
{
    // Pusat guardzone dalam lat/lon
    double centerLat = -7.19806403;
    double centerLon = 112.8;

    int centerX, centerY;
    if (!LatLonToXy(centerLat, centerLon, centerX, centerY)) {
        qDebug() << "Failed to convert lat/lon to XY";
        return;
    }

    double radiusInPixels = calculatePixelsFromNauticalMiles(testGuardZoneRadius);

    painter.save();

    // Gunakan garis putus-putus tanpa fill
    QPen dashedPen(Qt::red, 2, Qt::DashLine);
    painter.setPen(dashedPen);
    painter.setBrush(Qt::NoBrush);

    QRectF circleRect(centerX - radiusInPixels,
                      centerY - radiusInPixels,
                      radiusInPixels * 2,
                      radiusInPixels * 2);

    painter.drawEllipse(circleRect);

    // Titik tengah
    painter.setPen(QPen(Qt::red, 4));
    painter.drawPoint(centerX, centerY);

    painter.restore();

    qDebug() << "Dashed guardzone drawn at center (px):" << centerX << centerY
             << "radius:" << radiusInPixels << "px";
}

void EcWidget::drawTestGuardSquare(QPainter& painter)
{
    painter.save();

    if (rangeNM > 48.0) {
        painter.restore();
        return; // zoom terlalu jauh, tidak usah gambar kotak
    }

    double scaleFactor = 1.0;
    if (rangeNM > 24.0) scaleFactor = 0.25;
    else if (rangeNM > 12.0) scaleFactor = 0.5;
    else if (rangeNM > 6.0) scaleFactor = 0.75;

    QPen cornerPen(Qt::red, 2);
    painter.setPen(cornerPen);
    painter.setBrush(Qt::NoBrush);

    const double cornerLength = 10.0; // panjang garis sudut dalam pixel

    if (showDangerTarget == true){
        for (const AISTargetData& target : dangerousAISList) {
            double centerLat = target.lat;
            double centerLon = target.lon;

            int centerX, centerY;
            if (!LatLonToXy(centerLat, centerLon, centerX, centerY)) {
                continue;
            }

            // Ukuran pixel konstan * skala
            double radiusInPixels = 20.0 * scaleFactor;
            double left = centerX - radiusInPixels;
            double right = centerX + radiusInPixels;
            double top = centerY - radiusInPixels;
            double bottom = centerY + radiusInPixels;

            // Sudut kiri atas
            painter.drawLine(QPointF(left, top), QPointF(left + cornerLength, top));           // horizontal
            painter.drawLine(QPointF(left, top), QPointF(left, top + cornerLength));           // vertical

            // Sudut kanan atas
            painter.drawLine(QPointF(right, top), QPointF(right - cornerLength, top));         // horizontal
            painter.drawLine(QPointF(right, top), QPointF(right, top + cornerLength));         // vertical

            // Sudut kiri bawah
            painter.drawLine(QPointF(left, bottom), QPointF(left + cornerLength, bottom));     // horizontal
            painter.drawLine(QPointF(left, bottom), QPointF(left, bottom - cornerLength));     // vertical

            // Sudut kanan bawah
            painter.drawLine(QPointF(right, bottom), QPointF(right - cornerLength, bottom));   // horizontal
            painter.drawLine(QPointF(right, bottom), QPointF(right, bottom - cornerLength));   // vertical

            // Titik tengah
            painter.drawPoint(centerX, centerY);
        }
    }

    // Gambar kotak khusus untuk target yang sedang di-follow
    if (!trackTarget.isEmpty()) {
        // Asumsikan kamu punya variabel followedTarget bertipe AISTargetData
        AISTargetData ais;

        _aisObj->getAISTrack(ais);

        //qDebug() << ais.lat << ", " << ais.lon;

        double centerLat = ais.lat;
        double centerLon = ais.lon;

        int centerX, centerY;
        if (LatLonToXy(centerLat, centerLon, centerX, centerY)) {
            double radiusInPixels = 24.0 * scaleFactor;  // lebih besar dari kotak AIS bahaya
            double left = centerX - radiusInPixels;
            double right = centerX + radiusInPixels;
            double top = centerY - radiusInPixels;
            double bottom = centerY + radiusInPixels;

            QPen followPen(QColor(0, 255, 255, 180));  // cyan semi-transparan
            followPen.setWidth(3);
            followPen.setStyle(Qt::DashLine);
            painter.setPen(followPen);
            painter.setBrush(Qt::NoBrush);

            painter.drawRect(QRectF(QPointF(left, top), QPointF(right, bottom)));
        }
    }

    painter.restore();
}




void EcWidget::setClosestCPA(double val)
{
    closestCPA = val;
}

double EcWidget::getClosestCPA() const
{
    return closestCPA;
}

void EcWidget::setClosestAIS(AISTargetData val)
{
    closestAIS = val;
}

QString EcWidget::getTrackMMSI(){
    return trackTarget;
}

AISTargetData EcWidget::getClosestAIS() const
{
    return closestAIS;
}

void EcWidget::setDangerousAISList(const QList<AISTargetData>& list)
{
    dangerousAISList = list;
}

QList<AISTargetData> EcWidget::getDangerousAISList() const
{
    return dangerousAISList;
}

void EcWidget::addDangerousAISTarget(const AISTargetData& target)
{
    dangerousAISList.append(target);
}

void EcWidget::clearDangerousAISList()
{
    dangerousAISList.clear();
}

void EcWidget::setAISTrack(const AISTargetData aisTrack)
{
    _aisObj->setAISTrack(aisTrack);
}

void EcWidget::createRedDotGuardian()
{
    if (redDotGuardianEnabled) {
        qDebug() << "Red Dot Guardian already exists";
        return;
    }

    // PERBAIKAN: Jangan buat red dot guardian jika sudah ada attached guardzone
    if (attachedGuardZoneId != -1) {
        qDebug() << "Attached guardzone already exists, not creating Red Dot Guardian";
        return;
    }

    // Generate unique ID untuk red dot guardian
    redDotGuardianId = getNextGuardZoneId();  // Menggunakan fungsi yang sudah ada
    redDotGuardianName = QString("Ship Guardian Circle #%1").arg(redDotGuardianId);

    // Buat GuardZone object untuk red dot
    GuardZone redDotGuardZone;
    redDotGuardZone.id = redDotGuardianId;
    redDotGuardZone.name = redDotGuardianName;
    redDotGuardZone.shape = GUARD_ZONE_CIRCLE;
    redDotGuardZone.active = true;
    redDotGuardZone.attachedToShip = true;  // Selalu attached ke ship
    redDotGuardZone.color = QColor(255, 0, 0, 150);  // Red color

    // Set posisi dan radius
    redDotGuardZone.centerLat = ownShip.lat;
    redDotGuardZone.centerLon = ownShip.lon;
    redDotGuardZone.radius = guardianRadius;  // Menggunakan radius yang sudah ada (0.2 NM)

    // Tambahkan ke list guardZones
    guardZones.append(redDotGuardZone);

    // Enable red dot guardian
    redDotGuardianEnabled = true;

    // Update GuardZone Manager menggunakan signal yang sudah ada
    if (guardZoneManager) {
        emit guardZoneCreated();  // Signal yang sudah ada
    }

    // Save guardZones
    saveGuardZones();

    qDebug() << "Red Dot Guardian created with ID:" << redDotGuardianId;
}

void EcWidget::removeRedDotGuardian()
{
    if (!redDotGuardianEnabled) {
        return;
    }

    // Hapus dari guardZones list
    for (int i = 0; i < guardZones.size(); i++) {
        if (guardZones[i].id == redDotGuardianId) {
            guardZones.removeAt(i);
            break;
        }
    }

    // Disable red dot guardian
    redDotGuardianEnabled = false;
    int oldId = redDotGuardianId;
    redDotGuardianId = -1;
    redDotGuardianName.clear();

    // Update GuardZone Manager menggunakan signal yang sudah ada
    if (guardZoneManager) {
        emit guardZoneDeleted();  // Signal yang sudah ada
    }

    // Save guardZones
    saveGuardZones();

    qDebug() << "Red Dot Guardian removed with ID:" << oldId;
}

void EcWidget::updateRedDotGuardianInManager()
{
    if (!redDotGuardianEnabled || !redDotAttachedToShip) {
        return;
    }

    // Update posisi red dot guardian di guardZones list
    for (GuardZone& gz : guardZones) {
        if (gz.id == redDotGuardianId) {
            gz.centerLat = ownShip.lat;
            gz.centerLon = ownShip.lon;
            gz.radius = guardianRadius;
            gz.active = shipGuardianEnabled;
            break;
        }
    }

    // Refresh GuardZone Manager menggunakan signal yang sudah ada
    if (guardZoneManager) {
        emit guardZoneModified();  // Signal yang sudah ada
    }
}

void EcWidget::createAttachedGuardZone()
{
    qDebug() << "[CREATE-ATTACHED-DEBUG] ========== STARTING CREATE ATTACHED GUARDZONE ==========";

    // CRITICAL: Wrap entire function in try-catch to prevent crashes
    try {
        // Cek apakah sudah ada
        if (attachedGuardZoneId != -1) {
            qDebug() << "[CREATE-ATTACHED-DEBUG] Attached GuardZone already exists with ID:" << attachedGuardZoneId;
            return;
        }

    // PERBAIKAN: Pastikan tidak ada guardzone lain yang attachedToShip = true
    for (GuardZone& gz : guardZones) {
        if (gz.attachedToShip) {
            qDebug() << "[CLEANUP] Found existing attached guardzone" << gz.id << "- removing attachedToShip flag";
            gz.attachedToShip = false;
        }
    }

    // Generate ID dan nama
    attachedGuardZoneId = getNextGuardZoneId();
    attachedGuardZoneName = QString("Ship Guardian Zone #%1").arg(attachedGuardZoneId);
    qDebug() << "[CREATE-ATTACHED] Generated ID:" << attachedGuardZoneId << "Name:" << attachedGuardZoneName;

    // Buat GuardZone object
    GuardZone attachedGZ;
    attachedGZ.id = attachedGuardZoneId;
    attachedGZ.name = attachedGuardZoneName;
    attachedGZ.shape = GUARD_ZONE_CIRCLE;  // PERBAIKAN: Gunakan bentuk circle untuk donut
    attachedGZ.active = true;
    attachedGZ.attachedToShip = true;
    attachedGZ.color = QColor(255, 0, 0, 150);  // Red color

    // Set posisi dan radius (menggunakan nilai red dot yang sudah ada)
    // PERBAIKAN: Use robust coordinate fallback system
    double useLat = (navShip.lat != 0.0 ? navShip.lat : ownShip.lat);
    double useLon = (navShip.lon != 0.0 ? navShip.lon : ownShip.lon);

    qDebug() << "[CREATE-ATTACHED-DEBUG] === CREATING ATTACHED GUARDZONE ===";
    qDebug() << "[CREATE-ATTACHED-DEBUG] OwnShip position:" << useLat << "," << useLon;
    qDebug() << "[CREATE-ATTACHED-DEBUG] OwnShip heading:" << ownShip.heading;
    qDebug() << "[CREATE-ATTACHED-DEBUG] Guardian radius:" << guardianRadius;

    // CRITICAL: Enhanced validation with safe fallback to Jakarta position
    if (qIsNaN(useLat) || qIsInf(useLat) || qIsNaN(useLon) || qIsInf(useLon) ||
        (useLat == 0.0 && useLon == 0.0) ||
        useLat < -90.0 || useLat > 90.0 ||
        useLon < -180.0 || useLon > 180.0) {

        qDebug() << "[CREATE-ATTACHED-WARNING] Invalid OwnShip position, using default Jakarta position";
        useLat = -6.2088;   // Jakarta default latitude
        useLon = 106.8456;  // Jakarta default longitude
        qDebug() << "[CREATE-ATTACHED-DEBUG] Using fallback position:" << useLat << "," << useLon;
    }

    qDebug() << "[CREATE-ATTACHED-DEBUG] Final coordinates validation passed";

    attachedGZ.centerLat = useLat;
    attachedGZ.centerLon = useLon;

    qDebug() << "[CREATE-ATTACHED] Using position:" << useLat << "," << useLon;

    // PERBAIKAN: Set properties untuk setengah lingkaran perisai
    qDebug() << "[CREATE-ATTACHED-DEBUG] Setting radius properties...";

    // CRITICAL: Validate and fix guardian radius
    if (qIsNaN(guardianRadius) || qIsInf(guardianRadius) || guardianRadius <= 0.0 || guardianRadius > 100.0) {
        qDebug() << "[CREATE-ATTACHED-WARNING] Invalid guardianRadius:" << guardianRadius;
        guardianRadius = 0.5;  // Default safe radius
        qDebug() << "[CREATE-ATTACHED-DEBUG] Using default radius:" << guardianRadius << "NM";
    } else {
        qDebug() << "[CREATE-ATTACHED-DEBUG] Guardian radius validated:" << guardianRadius << "NM";
    }

    attachedGZ.innerRadius = 0.0;              // No inner radius - full semicircle shield
    attachedGZ.outerRadius = guardianRadius;   // Outer radius
    qDebug() << "[CREATE-ATTACHED-DEBUG] Radius properties set - inner:" << attachedGZ.innerRadius << "outer:" << attachedGZ.outerRadius;

    // RESTORED: Kembali ke setengah lingkaran yang mengikuti heading untuk presentasi
    qDebug() << "[CREATE-ATTACHED-DEBUG] Using semicircle shield following heading";

    // Set angles untuk setengah lingkaran mengikuti heading
    double currentHeading = ownShip.heading;
    if (qIsNaN(currentHeading) || qIsInf(currentHeading)) {
        currentHeading = 0.0; // Default heading North jika invalid
    }

    // Setengah lingkaran di depan kapal (front semicircle shield)
    attachedGZ.startAngle = fmod(currentHeading + 90.0, 360.0);   // Port-forward (90° + heading)
    attachedGZ.endAngle = fmod(currentHeading + 270.0, 360.0);    // Starboard-forward (270° + heading)

    qDebug() << "[CREATE-ATTACHED-DEBUG] Semicircle shield properties - Radius:" << attachedGZ.outerRadius
             << "Heading:" << currentHeading << "Angles: start=" << attachedGZ.startAngle << "end=" << attachedGZ.endAngle;

    // Legacy radius untuk backward compatibility
    attachedGZ.radius = guardianRadius;

    // PERBAIKAN: Update red dot position untuk sinkronisasi
    updateRedDotPosition(useLat, useLon);

    qDebug() << "[CREATE-ATTACHED] Creating attached guardzone at position:" << useLat << "," << useLon
             << "with radius:" << attachedGZ.radius << "NM"
             << "active:" << attachedGZ.active << "attachedToShip:" << attachedGZ.attachedToShip;

    // CRITICAL: Final validation before adding to list
    qDebug() << "[CREATE-ATTACHED-DEBUG] Final validation before adding to list...";
    qDebug() << "[CREATE-ATTACHED-DEBUG] Final GuardZone data:";
    qDebug() << "[CREATE-ATTACHED-DEBUG] - ID:" << attachedGZ.id;
    qDebug() << "[CREATE-ATTACHED-DEBUG] - Name:" << attachedGZ.name;
    qDebug() << "[CREATE-ATTACHED-DEBUG] - centerLat:" << attachedGZ.centerLat << "centerLon:" << attachedGZ.centerLon;
    qDebug() << "[CREATE-ATTACHED-DEBUG] - innerRadius:" << attachedGZ.innerRadius << "outerRadius:" << attachedGZ.outerRadius;
    qDebug() << "[CREATE-ATTACHED-DEBUG] - radius:" << attachedGZ.radius;
    qDebug() << "[CREATE-ATTACHED-DEBUG] - startAngle:" << attachedGZ.startAngle << "endAngle:" << attachedGZ.endAngle;
    qDebug() << "[CREATE-ATTACHED-DEBUG] - attachedToShip:" << attachedGZ.attachedToShip << "active:" << attachedGZ.active;

    // Tambahkan ke list
    guardZones.append(attachedGZ);
    qDebug() << "[CREATE-ATTACHED-DEBUG] Added to guardZones list. Total guardZones now:" << guardZones.size();

    // PERBAIKAN: Auto-enable guardzone auto-check when creating attached guardzone
    if (!guardZoneAutoCheckEnabled) {
        guardZoneAutoCheckEnabled = true;
        guardZoneAutoCheckTimer->start(guardZoneCheckInterval);
        qDebug() << "[AUTO-ACTIVATION] GuardZone auto-check automatically enabled for new attached guardzone";
    }

    // Save dan update dengan protection
    qDebug() << "[CREATE-ATTACHED-DEBUG] Saving guardzone to file...";
    try {
        saveGuardZones();
        qDebug() << "[CREATE-ATTACHED-DEBUG] Save completed successfully";
    } catch (const std::exception& e) {
        qDebug() << "[CREATE-ATTACHED-ERROR] Exception during save:" << e.what();
    } catch (...) {
        qDebug() << "[CREATE-ATTACHED-ERROR] Unknown exception during save";
    }

    qDebug() << "[CREATE-ATTACHED-DEBUG] Emitting guardZoneCreated signal...";
    emit guardZoneCreated();
    qDebug() << "[CREATE-ATTACHED-DEBUG] Signal emitted";

    qDebug() << "[CREATE-ATTACHED-DEBUG] Attached GuardZone created with ID:" << attachedGuardZoneId;

    // Emit signal untuk update UI
    qDebug() << "[CREATE-ATTACHED-DEBUG] Emitting attachToShipStateChanged signal...";
    emit attachToShipStateChanged(true);
    qDebug() << "[CREATE-ATTACHED-DEBUG] ========== CREATE ATTACHED GUARDZONE COMPLETE ==========";

    } catch (const std::exception& e) {
        qDebug() << "[CREATE-ATTACHED-ERROR] Exception in createAttachedGuardZone():" << e.what();
        // Cleanup partial state
        if (attachedGuardZoneId != -1) {
            // Remove from list if it was added
            for (int i = 0; i < guardZones.size(); i++) {
                if (guardZones[i].id == attachedGuardZoneId) {
                    guardZones.removeAt(i);
                    break;
                }
            }
            attachedGuardZoneId = -1;
            attachedGuardZoneName.clear();
        }
    } catch (...) {
        qDebug() << "[CREATE-ATTACHED-ERROR] Unknown exception in createAttachedGuardZone() - cleanup and abort";
        // Cleanup partial state
        if (attachedGuardZoneId != -1) {
            // Remove from list if it was added
            for (int i = 0; i < guardZones.size(); i++) {
                if (guardZones[i].id == attachedGuardZoneId) {
                    guardZones.removeAt(i);
                    break;
                }
            }
            attachedGuardZoneId = -1;
            attachedGuardZoneName.clear();
        }
    }
}

void EcWidget::removeAttachedGuardZone()
{
    if (attachedGuardZoneId == -1) {
        return;
    }

    // Hapus dari list
    int originalSize = guardZones.size();
    for (int i = 0; i < guardZones.size(); i++) {
        if (guardZones[i].id == attachedGuardZoneId) {
            guardZones.removeAt(i);
            qDebug() << "[REMOVE-ATTACHED] Removed guardzone from list. Size changed from" << originalSize << "to" << guardZones.size();
            break;
        }
    }

    // Reset ID
    attachedGuardZoneId = -1;
    attachedGuardZoneName = "";

    // Save dan update
    saveGuardZones();
    emit guardZoneDeleted();

    qDebug() << "Attached GuardZone removed";

    // Emit signal untuk update UI
    emit attachToShipStateChanged(false);
}

// FUNGSI UTILITY UNTUK MEMBERSIHKAN DUPLICATE ATTACHED GUARDZONE
void EcWidget::cleanupDuplicateAttachedGuardZones()
{
    int attachedCount = 0;
    QVector<int> duplicateIds;
    int firstAttachedId = -1;

    // Hitung berapa banyak guardzone yang attachedToShip = true
    for (const GuardZone& gz : guardZones) {
        if (gz.attachedToShip) {
            attachedCount++;
            if (attachedCount == 1) {
                firstAttachedId = gz.id;  // Simpan ID yang pertama
            } else {
                duplicateIds.append(gz.id);
            }
        }
    }

    // Hapus yang duplikat
    if (attachedCount > 1) {
        qDebug() << "[CLEANUP] Found" << attachedCount << "attached guardzones, removing duplicates...";

        for (int duplicateId : duplicateIds) {
            for (GuardZone& gz : guardZones) {
                if (gz.id == duplicateId) {
                    qDebug() << "[CLEANUP] Removing duplicate attached guardzone" << duplicateId;
                    gz.attachedToShip = false;
                    break;
                }
            }
        }

        // PERBAIKAN: Update attachedGuardZoneId dengan yang pertama
        if (firstAttachedId != -1) {
            attachedGuardZoneId = firstAttachedId;
            for (const GuardZone& gz : guardZones) {
                if (gz.id == firstAttachedId) {
                    attachedGuardZoneName = gz.name;
                    break;
                }
            }
            qDebug() << "[CLEANUP] Updated attachedGuardZoneId to" << attachedGuardZoneId;
        }

        // Save perubahan
        saveGuardZones();
        qDebug() << "[CLEANUP] Duplicate attached guardzones cleaned up and saved";
    }
}

// FUNGSI UTAMA UNTUK CHECK SHIP GUARDIAN ZONE
bool EcWidget::checkShipGuardianZone()
{
    // Check if any guardzone is attached to ship and active
    bool hasAttachedGuardZone = false;
    for (const GuardZone& gz : guardZones) {
        if (gz.attachedToShip && gz.active) {
            hasAttachedGuardZone = true;
            break;
        }
    }

    if (!hasAttachedGuardZone) {
        return false;
    }

    qDebug() << "=== CHECKING SHIP GUARDIAN ZONE (Attached GuardZone) ===";

    // Clear previous detections
    lastDetectedObstacles.clear();

    bool hasObstacles = false;

    // Check AIS targets
    if (checkAISTargetsInShipGuardian(lastDetectedObstacles)) {
        hasObstacles = true;
    }

    // Check static obstacles
    if (checkStaticObstaclesInShipGuardian(lastDetectedObstacles)) {
        hasObstacles = true;
    }

    // Check pick report obstacles (chart features at ship position)
    checkPickReportObstaclesInShipGuardian();

    // Clean up outdated obstacle markers
    qDebug() << "[CLEANUP-TIMER] Running obstacle cleanup, current markers:" << obstacleMarkers.size();
    removeOutdatedObstacleMarkers();
    qDebug() << "[CLEANUP-TIMER] After cleanup, remaining markers:" << obstacleMarkers.size();

    // FORCE TEST: Also run cleanup from update paint event as fallback
    static int cleanupCounter = 0;
    if (++cleanupCounter % 50 == 0) { // Every ~50 paint events
        qDebug() << "[FORCE-CLEANUP] Running fallback cleanup from paint event";
        removeOutdatedObstacleMarkers();
    }

    // Show alert jika ada obstacles
    if (hasObstacles && !lastDetectedObstacles.isEmpty()) {
        showShipGuardianAlert(lastDetectedObstacles);
    }

    return hasObstacles;
}

// CHECK AIS TARGETS (PRIVATE HELPER)
bool EcWidget::checkAISTargetsInShipGuardian(QList<DetectedObstacle>& obstacles)
{
    if (currentAISTargets.isEmpty()) {
        return false;
    }

    bool foundDanger = false;
    double guardianRadiusLocal = guardianRadius; // PERBAIKAN: Gunakan guardianRadius yang dapat dikonfigurasi

    aisTargetsMutex.lock();

    for (const AISTargetData& target : currentAISTargets) {
        // Hitung jarak dari ship ke AIS target
        double distance, bearing;
        EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                               ownShip.lat, ownShip.lon,
                                               target.lat, target.lon,
                                               &distance, &bearing);

        // Jika AIS target dalam Ship Guardian Zone
        if (distance <= guardianRadiusLocal) {
            DetectedObstacle obstacle;
            obstacle.type = "AIS_TARGET";
            obstacle.name = QString("AIS %1").arg(target.mmsi);
            obstacle.description = QString("COG: %1°, SOG: %2 kts")
                                   .arg(target.cog, 0, 'f', 1)
                                   .arg(target.sog, 0, 'f', 1);

            // Level bahaya berdasarkan jarak dan kecepatan
            if (distance < 0.05) {
                obstacle.level = 3; // Danger - sangat dekat (< 0.05 NM)
            } else if (target.sog > 15.0 || distance < 0.1) {
                obstacle.level = 2; // Warning - kecepatan tinggi atau dekat
            } else {
                obstacle.level = 1; // Note - normal
            }

            obstacle.lat = target.lat;
            obstacle.lon = target.lon;
            obstacle.distance = distance;
            obstacle.bearing = bearing;

            obstacles.append(obstacle);
            foundDanger = true;

            qDebug() << "🚨 AIS TARGET DETECTED in Ship Guardian:" << target.mmsi
                     << "Distance:" << distance << "NM, Level:" << obstacle.level;
        }
    }

    aisTargetsMutex.unlock();
    return foundDanger;
}

// CHECK STATIC OBSTACLES (PRIVATE HELPER)
bool EcWidget::checkStaticObstaclesInShipGuardian(QList<DetectedObstacle>& obstacles)
{
    double guardianRadiusLocal = guardianRadius; // PERBAIKAN: Gunakan guardianRadius yang dapat dikonfigurasi
    bool foundDanger = false;

    // SIMULASI: Wreck di area Houston Ship Channel
    if (ownShip.lat > 29.30 && ownShip.lat < 29.50 &&
        ownShip.lon > -95.00 && ownShip.lon < -94.50) {

        DetectedObstacle obstacle;
        obstacle.type = "WRECKS";
        obstacle.name = "Dangerous Wreck";
        obstacle.description = "Depth: 3m - DANGER TO NAVIGATION";
        obstacle.level = 3; // Danger level

        // Posisi wreck (relatif terhadap ship)
        obstacle.lat = ownShip.lat + 0.0008;
        obstacle.lon = ownShip.lon + 0.0012;

        // Hitung jarak
        double distance, bearing;
        EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                               ownShip.lat, ownShip.lon,
                                               obstacle.lat, obstacle.lon,
                                               &distance, &bearing);

        if (distance <= guardianRadiusLocal) {
            obstacle.distance = distance;
            obstacle.bearing = bearing;
            obstacles.append(obstacle);
            foundDanger = true;

            qDebug() << "🚨 WRECK DETECTED in Ship Guardian:" << obstacle.name
                     << "Distance:" << distance << "NM";
        }
    }

    // SIMULASI: Buoy di area Galveston
    if (ownShip.lat > 29.35 && ownShip.lat < 29.45 &&
        ownShip.lon > -94.85 && ownShip.lon < -94.65) {

        DetectedObstacle obstacle;
        obstacle.type = "BOYISD";
        obstacle.name = "Isolated Danger Buoy";
        obstacle.description = "Red/Black buoy marking danger";
        obstacle.level = 2; // Warning level

        obstacle.lat = ownShip.lat - 0.0006;
        obstacle.lon = ownShip.lon + 0.0009;

        double distance, bearing;
        EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                               ownShip.lat, ownShip.lon,
                                               obstacle.lat, obstacle.lon,
                                               &distance, &bearing);

        if (distance <= guardianRadiusLocal) {
            obstacle.distance = distance;
            obstacle.bearing = bearing;
            obstacles.append(obstacle);
            foundDanger = true;

            qDebug() << "⚠️ BUOY DETECTED in Ship Guardian:" << obstacle.name
                     << "Distance:" << distance << "NM";
        }
    }

    return foundDanger;
}

// Check pick report obstacles in ship guardian zone
void EcWidget::checkPickReportObstaclesInShipGuardian()
{
    // Check if any guardzone is attached to ship
    bool hasAttachedGuardZone = false;
    int totalGuardZones = guardZones.size();
    int activeGuardZones = 0;
    int attachedGuardZones = 0;

    for (const GuardZone& gz : guardZones) {
        if (gz.active) activeGuardZones++;
        if (gz.attachedToShip) {
            attachedGuardZones++;
            if (gz.active) {
                hasAttachedGuardZone = true;
                qDebug() << "[OBSTACLE-DEBUG] Found active attached guardzone:" << gz.id << gz.name;
            }
        }
    }

    qDebug() << "[OBSTACLE-DEBUG] GuardZone status: total=" << totalGuardZones
             << "active=" << activeGuardZones << "attached=" << attachedGuardZones;

    if (!hasAttachedGuardZone) {
        return; // Only check when guardzone is attached to ship
    }

    // Get all attached guardzone for area checking
    GuardZone* attachedGuardZone = nullptr;
    for (GuardZone& gz : guardZones) {
        if (gz.attachedToShip && gz.active) {
            attachedGuardZone = &gz;
            break; // Use first attached guardzone
        }
    }

    if (!attachedGuardZone) {
        return;
    }

    // Check multiple points within guardzone area USING SAME LOGIC as guardzone
    QList<QPair<EcFeature, QPair<double, double>>> allPickedFeaturesWithCoords;

    qDebug() << "[OBSTACLE-DEBUG] Guardzone shape:" << attachedGuardZone->shape
             << "radius inner:" << attachedGuardZone->innerRadius
             << "outer:" << attachedGuardZone->outerRadius << "NM"
             << "angles:" << attachedGuardZone->startAngle << "to" << attachedGuardZone->endAngle;

    // Use guardzone center position (should be ship position for attached guardzone)
    double centerLat = attachedGuardZone->centerLat;
    double centerLon = attachedGuardZone->centerLon;

    // For attach to ship, center should follow ship position (prefer navShip if available)
    if (attachedGuardZone->attachedToShip) {
        if (navShip.lat != 0.0 && navShip.lon != 0.0) {
            centerLat = navShip.lat;
            centerLon = navShip.lon;
        } else {
            centerLat = ownShip.lat;
            centerLon = ownShip.lon;
        }
    }

    // Check center (guardzone center)
    QList<EcFeature> centerFeatures;
    GetPickedFeaturesSubs(centerFeatures, centerLat, centerLon);
    for (const EcFeature& feature : centerFeatures) {
        allPickedFeaturesWithCoords.append(qMakePair(feature, qMakePair(centerLat, centerLon)));
    }
    qDebug() << "[OBSTACLE-DEBUG] Center features found:" << centerFeatures.size();

    // For circular guardzone, check additional points using SEMICIRCLE logic
    if (attachedGuardZone->shape == GUARD_ZONE_CIRCLE) {
        double outerRadius = attachedGuardZone->outerRadius;
        double innerRadius = attachedGuardZone->innerRadius;

        // OPTIMIZED: Use strategic scanning pattern to reduce duplicates
        // Focus on key angles and fewer radius checks
        for (int angle = 0; angle < 360; angle += 45) { // Wider angle intervals (45°)
            double checkLat, checkLon;

            // OPTIMIZED: Reduce radius checks to prevent over-scanning
            QList<double> checkRadii;
            if (innerRadius > 0 && outerRadius > innerRadius + 0.1) {
                // Only check outer edge for annulus guardzone
                checkRadii << outerRadius * 0.9; // Near outer edge only
            } else {
                // For simple circle, check two strategic points
                if (outerRadius > 0.2) {
                    checkRadii << outerRadius * 0.7; // 70% radius for good coverage
                }
            }

            for (double checkRadius : checkRadii) {
                EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                            centerLat, centerLon,
                                            checkRadius, angle,
                                            &checkLat, &checkLon);

                // CRITICAL: Only check points that would be in the guardzone using same logic
                if (isPointInSemicircle(checkLat, checkLon, attachedGuardZone)) {
                    QList<EcFeature> pointFeatures;
                    GetPickedFeaturesSubs(pointFeatures, checkLat, checkLon);

                    for (const EcFeature& feature : pointFeatures) {
                        allPickedFeaturesWithCoords.append(qMakePair(feature, qMakePair(checkLat, checkLon)));
                    }

                    if (pointFeatures.size() > 0) {
                        qDebug() << "[OBSTACLE-DEBUG] Found" << pointFeatures.size() << "features at angle" << angle
                                 << "radius" << checkRadius << "position" << checkLat << checkLon;
                    }
                }
            }
        }
    }

    qDebug() << "[OBSTACLE-DEBUG] Total features with coordinates collected:" << allPickedFeaturesWithCoords.size();

    // Track current obstacles with counters for optimization monitoring
    QSet<QString> currentDetectedObstacles;
    int totalScanned = 0;
    int duplicatesSkipped = 0;
    int irrelevantSkipped = 0;
    int outsideGuardzone = 0;

    // Process each feature individually like in PickWindow
    for (const auto& featureWithCoords : allPickedFeaturesWithCoords) {
        totalScanned++;

        const EcFeature& feature = featureWithCoords.first;
        double obstacleLat = featureWithCoords.second.first;
        double obstacleLon = featureWithCoords.second.second;
        char featToken[EC_LENATRCODE + 1];
        char featName[256];

        // Get feature class using SevenCs API (same as PickWindow)
        EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));

        // Skip AIS targets and own ship (like in PickWindow)
        QString featureClass = QString::fromLatin1(featToken);

        if (featureClass == "aistar" || featureClass == "ownshp") {
            continue;
        }

        // OPTIMIZATION: Filter out irrelevant feature types to reduce noise
        // Only process navigationally significant obstacles
        QStringList relevantFeatures = {
            "WRECKS", "OBSTNS", "UWTROC", "BOYISD", "CTNARE", "TSSBND",
            "DEPARE", "DRGARE", "PIPARE", "CBLARE", "NAVLNE", "DWRTPT", "DWRTCL"
        };

        if (!relevantFeatures.contains(featureClass)) {
            // Skip non-navigational features to reduce clutter
            irrelevantSkipped++;
            continue;
        }

        // Translate token to human readable name (fallback)
        if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK) {
            strcpy(featName, "Unknown Chart Feature");
        }

        // Extract better object name from attributes (like "swept between 5m and 10m")
        QString betterObjectName = extractObjectNameFromFeature(feature);
        QString finalObjectName = betterObjectName.isEmpty() ? QString::fromLatin1(featName) : betterObjectName;

        // Obstacle coordinates are already available from the loop

        // Create more precise unique identifier to prevent duplicates
        // Use higher precision coordinates and include feature class
        QString obstacleId = QString("%1_%2_%3_%4")
                            .arg(featureClass)
                            .arg(finalObjectName.left(20)) // Limit name length
                            .arg(obstacleLat, 0, 'f', 6)   // Higher precision lat
                            .arg(obstacleLon, 0, 'f', 6);  // Higher precision lon

        // CRITICAL: Double-check that obstacle coordinates are actually within guardzone
        // This prevents false detections from chart features picked up during scanning
        bool obstacleInGuardZone = isPointInSemicircle(obstacleLat, obstacleLon, attachedGuardZone);

        if (!obstacleInGuardZone) {
            outsideGuardzone++;
            continue;
        }

        // OPTIMIZATION: Check for nearby duplicate obstacles (distance-based deduplication)
        bool isDuplicate = false;
        for (const QString& existingId : currentDetectedObstacles) {
            // Extract coordinates from existing obstacle IDs for comparison
            QStringList parts = existingId.split("_");
            if (parts.size() >= 4) {
                double existingLat = parts[2].toDouble();
                double existingLon = parts[3].toDouble();

                // Calculate distance between obstacles
                double distance, bearing;
                EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                       obstacleLat, obstacleLon,
                                                       existingLat, existingLon,
                                                       &distance, &bearing);

                // If obstacles are very close (< 50 meters), consider as duplicate
                if (distance < 0.027) { // 0.027 NM ≈ 50 meters
                    isDuplicate = true;
                    qDebug() << "[OBSTACLE-DEBUG] Duplicate obstacle detected - too close to existing:"
                             << finalObjectName << "distance:" << distance << "NM";
                    break;
                }
            }
        }

        if (isDuplicate) {
            duplicatesSkipped++;
            continue; // Skip this duplicate obstacle
        }

        currentDetectedObstacles.insert(obstacleId);

        // Only emit if this is a new obstacle AND it's actually in guardzone
        if (!previousDetectedObstacles.contains(obstacleId)) {
            // Determine danger level based on feature type
            QString dangerLevel = "NOTE";
            if (featureClass == "WRECKS" || featureClass == "OBSTNS" || featureClass == "UWTROC") {
                dangerLevel = "DANGEROUS";
            } else if (featureClass == "BOYISD" || featureClass == "CTNARE" || featureClass == "TSSBND") {
                dangerLevel = "WARNING";
            }

            // Extract Information field from attributes
            QString information = extractInformationFromFeature(feature);
            if (information.isEmpty()) {
                information = "Chart feature detected";
            }

            qDebug() << "[OBSTACLE-DEBUG] Feature position: " << obstacleLat << "," << obstacleLon;

            // Create pick report obstacle data with actual obstacle coordinates
            QString details = QString("PICK_REPORT|%1|%2|%3|%4|%5|%6|%7")
                             .arg(featureClass)
                             .arg(finalObjectName)
                             .arg(featureClass)
                             .arg(obstacleLat, 0, 'f', 6)
                             .arg(obstacleLon, 0, 'f', 6)
                             .arg(dangerLevel)
                             .arg(information);

            // Emit signal to add to obstacle detection panel
            emit pickReportObstacleDetected(0, details); // 0 = ship guardian zone ID

            qDebug() << "✅ Pick Report NEW Obstacle Detected:" << finalObjectName
                     << "(" << featureClass << ") Level:" << dangerLevel << "at" << obstacleLat << obstacleLon
                     << "- CONFIRMED in guardzone area";
        }
    }

    // Update previous obstacles for next check
    previousDetectedObstacles = currentDetectedObstacles;

    // OPTIMIZATION REPORT: Log performance metrics
    qDebug() << "[OBSTACLE-OPTIMIZATION] Scan completed:"
             << "Total scanned:" << totalScanned
             << "Valid obstacles:" << currentDetectedObstacles.size()
             << "Duplicates skipped:" << duplicatesSkipped
             << "Irrelevant skipped:" << irrelevantSkipped
             << "Outside guardzone:" << outsideGuardzone;
}

QString EcWidget::extractInformationFromFeature(const EcFeature& feature)
{
    QStringList infoList;

    // Extract attributes using SevenCs API like in PickWindow
    char attrStr[1024];
    char attrName[1024];
    char attrText[1024];
    EcFindInfo fI;
    Bool result;
    EcAttributeToken attrToken;
    EcAttributeType attrType;

    // Get the first attribute string of the feature
    result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_FIRST, attrStr, sizeof(attrStr));

    while (result) {
        // Extract the six character long attribute token
        strncpy(attrToken, attrStr, EC_LENATRCODE);
        attrToken[EC_LENATRCODE] = (char)0;

        // Translate the token to a human readable name
        if (EcDictionaryTranslateAttributeToken(dictInfo, attrToken, attrName, sizeof(attrName))) {
            // Get the attribute type (List, enumeration, integer, float, string, text)
            if (EcDictionaryGetAttributeType(dictInfo, attrStr, &attrType) == EC_DICT_OK) {
                QString attrTokenStr = QString(attrToken);
                QString attrNameStr = QString(attrName);

                if (attrType == EC_ATTR_ENUM || attrType == EC_ATTR_LIST) {
                    // Translate the value to a human readable text
                    if (!EcDictionaryTranslateAttributeValue(dictInfo, attrStr, attrText, sizeof(attrText))) {
                        attrText[0] = (char)0;
                    }

                    QString attrTextStr = QString(attrText);

                    // Format specific enumerated attributes with enhanced display
                    if (attrTokenStr == "trksta") {
                        infoList << QString("Track Status: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "actsta") {
                        infoList << QString("Activation: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "posint") {
                        infoList << QString("Position Integrity: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "navsta") {
                        infoList << QString("Navigation Status: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "catobs") {
                        infoList << QString("Obstruction Category: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "catwrk") {
                        infoList << QString("Wreck Category: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "catsil") {
                        infoList << QString("Silo Category: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "status") {
                        infoList << QString("Status: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "watlev") {
                        infoList << QString("Water Level: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "quasou") {
                        infoList << QString("Quality of Sounding: %1").arg(attrTextStr);
                    } else if (!attrTextStr.isEmpty()) {
                        infoList << QString("%1: %2").arg(attrNameStr).arg(attrTextStr);
                    }
                } else {
                    // For other types, use the raw value with enhanced formatting
                    strcpy(attrText, &attrStr[EC_LENATRCODE]);
                    QString attrTextStr = QString(attrText);

                    // Handle specific non-enum attributes with enhanced formatting
                    if (attrTokenStr == "cogcrs") {
                        infoList << QString("COG: %1°").arg(attrTextStr);
                    } else if (attrTokenStr == "mmsino") {
                        infoList << QString("MMSI: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "roturn") {
                        infoList << QString("ROT: %1 deg/min").arg(attrTextStr);
                    } else if (attrTokenStr == "headng") {
                        infoList << QString("Heading: %1°").arg(attrTextStr);
                    } else if (attrTokenStr == "sogspd") {
                        infoList << QString("SOG: %1 kn").arg(attrTextStr);
                    } else if (attrTokenStr == "valsou") {
                        infoList << QString("Sounding: %1 m").arg(attrTextStr);
                    } else if (attrTokenStr == "drval1") {
                        infoList << QString("Depth Range Min: %1 m").arg(attrTextStr);
                    } else if (attrTokenStr == "drval2") {
                        infoList << QString("Depth Range Max: %1 m").arg(attrTextStr);
                    } else if (attrTokenStr == "verdat") {
                        infoList << QString("Vertical Datum: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "height") {
                        infoList << QString("Height: %1 m").arg(attrTextStr);
                    } else if (attrTokenStr == "verccl") {
                        infoList << QString("Vertical Clearance: %1 m").arg(attrTextStr);
                    } else if (attrTokenStr == "horccl") {
                        infoList << QString("Horizontal Clearance: %1 m").arg(attrTextStr);
                    } else if (attrTokenStr == "INFORM" || attrTokenStr == "inform") {
                        infoList << QString("Information: %1").arg(attrTextStr);
                    } else if (attrTokenStr == "OBJNAM" || attrTokenStr == "objnam") {
                        infoList << QString("Object Name: %1").arg(attrTextStr);
                    } else if (!attrTextStr.isEmpty() && attrTextStr.length() > 1) {
                        // Skip very short or empty values but include meaningful ones
                        infoList << QString("%1: %2").arg(attrNameStr).arg(attrTextStr);
                    }
                }
            }
        }

        // Get next attribute
        result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
    }

    // Return comprehensive information or fallback message
    if (infoList.isEmpty()) {
        return "Navigational feature detected";
    }

    return infoList.join("; ");
}

QString EcWidget::extractObjectNameFromFeature(const EcFeature& feature)
{
    QString objectName;

    // Extract attributes using SevenCs API like in PickWindow
    char attrStr[512];
    EcFindInfo fI;
    Bool result;
    EcAttributeToken attrToken;
    char attrName[256];

    // Get the first attribute string of the feature
    result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_FIRST, attrStr, sizeof(attrStr));

    while (result) {
        // Extract the six character long attribute token
        strncpy(attrToken, attrStr, EC_LENATRCODE);
        attrToken[EC_LENATRCODE] = (char)0;

        // Translate the token to a human readable name
        if (EcDictionaryTranslateAttributeToken(dictInfo, attrToken, attrName, sizeof(attrName))) {
            QString tokenStr = QString::fromLatin1(attrToken);
            QString nameStr = QString::fromLatin1(attrName);
            QString attrString = QString::fromLatin1(attrStr);

            // Look for OBJNAM, INFORM, or any descriptive attribute
            if (tokenStr == "OBJNAM" || tokenStr == "INFORM" ||
                nameStr.contains("Object name") || nameStr.contains("Information") ||
                nameStr.contains("Description")) {
                // Extract value after = sign
                int equalPos = attrString.indexOf('=');
                if (equalPos > 0 && equalPos < attrString.length() - 1) {
                    QString value = attrString.mid(equalPos + 1).trimmed();
                    if (!value.isEmpty() && value != "?" && value != "0") {
                        objectName = value;
                        break; // Use first meaningful name found
                    }
                }
            }
        }

        // Get next attribute
        result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
    }

    return objectName;
}

void EcWidget::showHazardInfoAt(double lat, double lon)
{
    try {
        QList<EcFeature> pickedFeatureList;
        GetPickedFeaturesSubs(pickedFeatureList, lat, lon);

        if (pickedFeatureList.isEmpty()) {
            return; // Nothing to show
        }

        struct HazardItem {
            QString level;     // DANGEROUS / WARNING
            QString title;     // Object name or feature name
            QString feature;   // Feature class token/name
            QString details;   // Extracted information
        };

        QList<HazardItem> hazards;
        bool hasDanger = false;
        bool hasWarning = false;

        // Helper: extract min depth (DRVAL1) from feature if present
        auto extractMinDepth = [&](const EcFeature& feature) -> double {
            char attrStr[1024];
            EcFindInfo fI;
            Bool result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_FIRST, attrStr, sizeof(attrStr));
            EcAttributeToken attrToken;
            EcAttributeType attrType;
            while (result) {
                strncpy(attrToken, attrStr, EC_LENATRCODE);
                attrToken[EC_LENATRCODE] = (char)0;
                if (EcDictionaryGetAttributeType(dictInfo, attrStr, &attrType) == EC_DICT_OK) {
                    if (QString::fromLatin1(attrToken) == "drval1") {
                        // Value after token
                        QString attrString = QString::fromLatin1(attrStr);
                        int eq = attrString.indexOf('=');
                        if (eq > 0) {
                            bool ok = false;
                            double v = attrString.mid(eq + 1).toDouble(&ok);
                            if (ok) return v;
                        }
                    }
                }
                result = EcFeatureGetAttributes(feature, dictInfo, &fI, EC_NEXT, attrStr, sizeof(attrStr));
            }
            return std::numeric_limits<double>::quiet_NaN();
        };

        // Iterate features and collect relevant hazards
        for (const EcFeature &feature : pickedFeatureList) {
            char featToken[EC_LENATRCODE + 1];
            char featName[256];

            EcFeatureGetClass(feature, dictInfo, featToken, sizeof(featToken));
            QString featureClass = QString::fromLatin1(featToken);

            // Skip non-relevant features
            if (featureClass == "aistar" || featureClass == "ownshp") {
                continue;
            }

            // Determine hazard level of interest
            QString level;
            if (featureClass == "WRECKS" || featureClass == "OBSTNS" || featureClass == "UWTROC") {
                level = "DANGEROUS";
            } else if (featureClass == "CTNARE" || featureClass == "PIPARE" || featureClass == "CBLARE" || featureClass == "TSSBND" || featureClass == "BOYISD" || featureClass == "DRGARE") {
                level = "WARNING"; // Caution-type features
            } else if (featureClass == "DEPARE") {
                // Classify by minimum depth (DRVAL1) vs ship draft
                double minDepth = extractMinDepth(feature);
                double shipDraft = SettingsManager::instance().data().shipDraftMeters;
                if (!qIsNaN(minDepth)) {
                    if (shipDraft > 0.0) {
                        double ukc = minDepth - shipDraft; // under keel clearance
                        double ukcDanger = SettingsManager::instance().data().ukcDangerMeters;
                        double ukcWarning = SettingsManager::instance().data().ukcWarningMeters;
                        if (ukc < ukcDanger) level = "DANGEROUS";
                        else if (ukc < ukcWarning) level = "WARNING";
                        else continue; // safe
                    } else {
                        // Fallback fixed thresholds
                        if (minDepth < 2.0) level = "DANGEROUS";
                        else if (minDepth < 5.0) level = "WARNING";
                        else continue;
                    }
                } else {
                    // If depth unknown, treat as caution to be safe
                    level = "WARNING";
                }
            } else {
                continue; // Only show caution/danger as requested
            }

            // Translate feature name
            if (EcDictionaryTranslateObjectToken(dictInfo, featToken, featName, sizeof(featName)) != EC_DICT_OK) {
                strcpy(featName, "Unknown Feature");
            }

            // Prefer object name/INFORM if available
            QString title = extractObjectNameFromFeature(feature);
            if (title.trimmed().isEmpty()) {
                title = QString::fromLatin1(featName);
            }

            // Extract detailed information (attributes)
            QString details = extractInformationFromFeature(feature);
            // Enrich DEPARE with min depth if available
            if (featureClass == "DEPARE") {
                double minDepth = extractMinDepth(feature);
                double shipDraft = SettingsManager::instance().data().shipDraftMeters;
                if (!qIsNaN(minDepth)) {
                    if (shipDraft > 0.0) {
                        double ukc = minDepth - shipDraft;
                        details = QString("Min depth: %1 m; UKC: %2 m; %3")
                                     .arg(minDepth, 0, 'f', 1)
                                     .arg(ukc, 0, 'f', 1)
                                     .arg(details);
                    } else {
                        details = QString("Min depth: %1 m; %2")
                                     .arg(minDepth, 0, 'f', 1)
                                     .arg(details);
                    }
                }
            }

            hazards.append({level, title, QString::fromLatin1(featName), details});
            if (level == "DANGEROUS") hasDanger = true; else hasWarning = true;
        }

        if (hazards.isEmpty()) {
            return; // No caution/danger near this waypoint
        }

        // Compute distance and bearing from own ship to waypoint if available
        double distNM = -1.0, bearing = -1.0;
        if (ownShip.lat != 0.0 || ownShip.lon != 0.0) {
            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                   ownShip.lat, ownShip.lon,
                                                   lat, lon,
                                                   &distNM, &bearing);
        }

        // Build concise HTML message
        QString msg;
        msg += QString("<b>Waypoint at</b> %1, %2")
                   .arg(lat, 0, 'f', 6)
                   .arg(lon, 0, 'f', 6);
        if (distNM >= 0.0) {
            msg += QString(" &nbsp; <span style='color:#666'>(%1 NM, %2° from ship)</span>")
                       .arg(distNM, 0, 'f', 3)
                       .arg(bearing, 0, 'f', 1);
        }
        msg += "<br><br>";

        int shown = 0;
        for (const auto &h : hazards) {
            // Limit items to avoid spamming
            if (shown >= 5) break;
            QString badge = (h.level == "DANGEROUS") ? "<span style='color:#b00020;font-weight:bold'>DANGEROUS</span>"
                                                     : "<span style='color:#b58900;font-weight:bold'>CAUTION</span>";
            msg += QString("%1: %2 (%3)<br>").arg(badge, h.title.toHtmlEscaped(), h.feature.toHtmlEscaped());
            if (!h.details.trimmed().isEmpty()) {
                msg += QString("<span style='color:#555'>%1</span><br>").arg(h.details.toHtmlEscaped());
            }
            msg += "<br>";
            shown++;
        }

        QMessageBox box;
        box.setWindowTitle(tr("Waypoint Hazard Information"));
        box.setTextFormat(Qt::RichText);
        box.setText(msg);
        if (hasDanger) box.setIcon(QMessageBox::Critical);
        else if (hasWarning) box.setIcon(QMessageBox::Warning);
        else box.setIcon(QMessageBox::Information);
        box.exec();
    } catch (...) {
        // Fail silently to not interrupt routing flow
    }
}

void EcWidget::drawObstacleDetectionArea(QPainter& painter)
{
    qDebug() << "[OBSTACLE-AREA-DEBUG] ========== Drawing Obstacle Detection Area ==========";

    // CRITICAL: Wrap entire function in try-catch
    try {
        // Only draw if there's an attached guardzone
        GuardZone* attachedGuardZone = nullptr;
        for (GuardZone& gz : guardZones) {
            if (gz.attachedToShip && gz.active) {
                attachedGuardZone = &gz;
                break;
            }
        }

        if (!attachedGuardZone) {
            qDebug() << "[OBSTACLE-AREA-DEBUG] No attached guardzone found - skipping";
            return; // No attached guardzone to visualize
        }

        qDebug() << "[OBSTACLE-AREA-DEBUG] Found attached guardzone:" << attachedGuardZone->id;

        // Set yellow color with transparency for detection area
        QColor detectionColor(255, 255, 0, 80); // Yellow with 80/255 opacity
        QColor borderColor(255, 215, 0, 150);   // Gold border

        painter.setPen(QPen(borderColor, 2, Qt::DashLine));
        painter.setBrush(QBrush(detectionColor));

        // CRITICAL: Enhanced coordinate validation
        double useLat = ownShip.lat;
        double useLon = ownShip.lon;

        // Use fallback position if ownShip is invalid
        if (qIsNaN(useLat) || qIsNaN(useLon) || (useLat == 0.0 && useLon == 0.0) ||
            useLat < -90.0 || useLat > 90.0 || useLon < -180.0 || useLon > 180.0) {
            qDebug() << "[OBSTACLE-AREA-DEBUG] Invalid ownShip position, using Jakarta fallback";
            useLat = -6.2088;   // Jakarta
            useLon = 106.8456;  // Jakarta
        }

        // Get ship position in screen coordinates
        int shipX, shipY;
        qDebug() << "[OBSTACLE-AREA-DEBUG] Converting coordinates:" << useLat << "," << useLon;
        if (!LatLonToXy(useLat, useLon, shipX, shipY)) {
            qDebug() << "[OBSTACLE-AREA-DEBUG] Failed to convert coordinates - skipping";
            return; // Can't convert coordinates
        }

        // CRITICAL: Validate screen coordinates
        if (abs(shipX) > 20000 || abs(shipY) > 20000) {
            qDebug() << "[OBSTACLE-AREA-ERROR] Invalid screen coordinates:" << shipX << "," << shipY << "- skipping";
            return;
        }

        qDebug() << "[OBSTACLE-AREA-DEBUG] Screen coordinates:" << shipX << "," << shipY;

        if (attachedGuardZone->shape == GUARD_ZONE_CIRCLE) {
            // Draw circular detection area
            qDebug() << "[OBSTACLE-AREA-DEBUG] Calculating radius pixels for radius:" << attachedGuardZone->outerRadius;
            double radiusPixels = calculatePixelsFromNauticalMiles(attachedGuardZone->outerRadius);
            qDebug() << "[OBSTACLE-AREA-DEBUG] Radius in pixels:" << radiusPixels;

            // CRITICAL: Validate radius
            if (qIsNaN(radiusPixels) || qIsInf(radiusPixels) || radiusPixels <= 0 || radiusPixels > 5000) {
                qDebug() << "[OBSTACLE-AREA-ERROR] Invalid radius pixels:" << radiusPixels << "- skipping";
                return;
            }

            // CRITICAL: Protected drawing operations
            try {
                qDebug() << "[OBSTACLE-AREA-DEBUG] Drawing ellipse at:" << shipX << "," << shipY << "radius:" << radiusPixels;
                painter.drawEllipse(QPointF(shipX, shipY), radiusPixels, radiusPixels);
                qDebug() << "[OBSTACLE-AREA-DEBUG] Ellipse drawn successfully";

                // Draw center point and radius indicator
                painter.setPen(QPen(Qt::yellow, 3));
                painter.drawPoint(shipX, shipY);
                qDebug() << "[OBSTACLE-AREA-DEBUG] Center point drawn";

                // Draw simple label
                painter.setPen(QPen(Qt::black, 1));
                painter.setFont(QFont("Arial", 10, QFont::Bold));
                QString label = QString("Detection Area: %1 NM").arg(attachedGuardZone->outerRadius, 0, 'f', 1);
                painter.drawText(shipX + 10, shipY - 10, label);
                qDebug() << "[OBSTACLE-AREA-DEBUG] Label drawn";

            } catch (const std::exception& e) {
                qDebug() << "[OBSTACLE-AREA-ERROR] Exception during drawing operations:" << e.what();
            } catch (...) {
                qDebug() << "[OBSTACLE-AREA-ERROR] Unknown exception during drawing operations";
            }

            // SKIP: Comment out dangerous detection points drawing
            /*
            // Draw additional detection points (8 points around circumference)
            painter.setPen(QPen(Qt::red, 2));
            for (int angle = 0; angle < 360; angle += 45) {
                double checkLat, checkLon;
                EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                           ownShip.lat, ownShip.lon,
                                           attachedGuardZone->outerRadius, angle,
                                           &checkLat, &checkLon);

                int checkX, checkY;
                if (LatLonToXy(checkLat, checkLon, checkX, checkY)) {
                    painter.drawEllipse(QPointF(checkX, checkY), 3, 3);
                }
            }
            */
        }

        qDebug() << "[OBSTACLE-AREA-DEBUG] Obstacle detection area drawing completed successfully";

    } catch (const std::exception& e) {
        qDebug() << "[OBSTACLE-AREA-ERROR] Exception in drawObstacleDetectionArea:" << e.what();
    } catch (...) {
        qDebug() << "[OBSTACLE-AREA-ERROR] Unknown exception in drawObstacleDetectionArea";
    }
}

// SHOW ALERT (PRIVATE HELPER)
void EcWidget::showShipGuardianAlert(const QList<DetectedObstacle>& obstacles)
{
    QString alertMessage = "🚨 SHIP GUARDIAN ZONE ALERT 🚨\n\n";
    int dangerCount = 0, warningCount = 0, noteCount = 0;

    for (const DetectedObstacle& obs : obstacles) {
        QString levelIcon;
        if (obs.level == 3) {
            levelIcon = "🔴 DANGER";
            dangerCount++;
        } else if (obs.level == 2) {
            levelIcon = "🟡 WARNING";
            warningCount++;
        } else {
            levelIcon = "🔵 NOTE";
            noteCount++;
        }

        alertMessage += QString("%1 - %2\n").arg(levelIcon, obs.name);
        alertMessage += QString("📝 %1\n").arg(obs.description);
        alertMessage += QString("📏 Distance: %1 NM | 🧭 Bearing: %2°\n\n")
                       .arg(obs.distance, 0, 'f', 3)
                       .arg(obs.bearing, 0, 'f', 1);
    }

    alertMessage += QString("📊 Total: %1 Dangers, %2 Warnings, %3 Notes")
                   .arg(dangerCount).arg(warningCount).arg(noteCount);

    // Status message
    emit statusMessage(QString("Ship Guardian Alert: %1 obstacles detected").arg(obstacles.size()));

    qDebug() << "🚨 SHIP GUARDIAN ALERT:" << obstacles.size() << "obstacles";

    // Show alert dialog
    QMessageBox msgBox;
    msgBox.setWindowTitle("Ship Guardian Zone Alert");
    msgBox.setText(alertMessage);

    if (dangerCount > 0) {
        msgBox.setIcon(QMessageBox::Critical);
    } else if (warningCount > 0) {
        msgBox.setIcon(QMessageBox::Warning);
    } else {
        msgBox.setIcon(QMessageBox::Information);
    }

    msgBox.exec();
}

// AUTO-CHECK FUNCTIONS
void EcWidget::setShipGuardianAutoCheck(bool enabled)
{
    shipGuardianAutoCheck = enabled;

    // Check if any guardzone is attached to ship
    bool hasAttachedGuardZone = false;
    for (const GuardZone& gz : guardZones) {
        if (gz.attachedToShip && gz.active) {
            hasAttachedGuardZone = true;
            break;
        }
    }

    if (enabled && hasAttachedGuardZone) {
        shipGuardianCheckTimer->start();
        qDebug() << "✅ Ship Guardian auto-check ENABLED (every 5 seconds) - using attached guardzone";
        emit statusMessage("Ship Guardian auto-check enabled");
    } else {
        // FORCE START: Always run timer even if disabled for cleanup purposes
        shipGuardianCheckTimer->start();
        qDebug() << "⚠️  Ship Guardian auto-check FORCED (for cleanup) - enabled:" << enabled << "hasAttachedGuardZone:" << hasAttachedGuardZone;
        emit statusMessage("Ship Guardian auto-check disabled");
    }
}

bool EcWidget::isShipGuardianAutoCheckEnabled() const
{
    return shipGuardianAutoCheck;
}

// TRIGGER ALERT UNTUK SHIP GUARDIAN ZONE
void EcWidget::triggerShipGuardianAlert(const QList<DetectedObstacle>& obstacles)
{
    QString alertMessage = "🚨 SHIP GUARDIAN ZONE ALERT! 🚨\n\n";
    int dangerCount = 0, warningCount = 0, noteCount = 0;

    for (const DetectedObstacle& obs : obstacles) {
        QString levelText;
        if (obs.level == 3) {
            levelText = "🔴 DANGER";
            dangerCount++;
        } else if (obs.level == 2) {
            levelText = "🟡 WARNING";
            warningCount++;
        } else {
            levelText = "🔵 NOTE";
            noteCount++;
        }

        alertMessage += QString("%1 - %2\n").arg(levelText, obs.name);
        alertMessage += QString("📝 %1\n").arg(obs.description);
        alertMessage += QString("📏 Distance: %1 NM\n")
                       .arg(obs.distance, 0, 'f', 3);
        alertMessage += QString("🧭 Bearing: %1°\n\n")
                       .arg(obs.bearing, 0, 'f', 1);
    }

    alertMessage += QString("📊 Summary: %1 Dangers, %2 Warnings, %3 Notes")
                   .arg(dangerCount).arg(warningCount).arg(noteCount);

    // Emit signal untuk status message
    emit statusMessage(QString("Ship Guardian Alert: %1 obstacles detected").arg(obstacles.size()));

    // Log untuk debugging
    qDebug() << "SHIP GUARDIAN ALERT TRIGGERED:" << obstacles.size() << "obstacles detected";

    // Tampilkan message box dengan icon sesuai level bahaya
    QMessageBox msgBox;
    msgBox.setWindowTitle("Ship Guardian Zone Alert");
    msgBox.setText(alertMessage);
    msgBox.setDetailedText(QString("Ship Guardian Zone has detected obstacles within %1 nautical miles radius.").arg(guardianRadius, 0, 'f', 1));

    if (dangerCount > 0) {
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setWindowIcon(QIcon(":/icons/danger.png"));
    } else if (warningCount > 0) {
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowIcon(QIcon(":/icons/warning.png"));
    } else {
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setWindowIcon(QIcon(":/icons/info.png"));
    }

    msgBox.exec();
}

// void EcWidget::debugAISTargets()
// {
//     if (!Ais::instance()) {
//         qDebug() << "[AIS-DEBUG] AIS instance not available";
//         return;
//     }

//     QMap<unsigned int, AISTargetData>& aisTargetMap = Ais::instance()->getTargetMap();

//     qDebug() << "[AIS-DEBUG] ========== AIS TARGETS DEBUG ==========";
//     qDebug() << "[AIS-DEBUG] Total targets:" << aisTargetMap.size();

//     int i = 0;
//     for (auto it = aisTargetMap.begin(); it != aisTargetMap.end(); ++it, ++i) {
//         const AISTargetData& target = it.value();

//         qDebug() << "[AIS-DEBUG] Target" << i << ":";
//         qDebug() << "  MMSI:" << target.mmsi;
//         qDebug() << "  Lat:" << target.lat;
//         qDebug() << "  Lon:" << target.lon;
//         qDebug() << "  SOG:" << target.sog << "knots";
//         qDebug() << "  COG:" << target.cog << "degrees";
//         qDebug() << "  Last Update:" << target.lastUpdate.toString();
//         qDebug() << "  Dangerous:" << target.isDangerous;
//     }
//     qDebug() << "[AIS-DEBUG] =======================================";
// }

void EcWidget::performAutoGuardZoneCheck()
{
    // PERBAIKAN: Cek apakah ada guardzone aktif, bukan flag global guardZoneActive
    if (!Ais::instance()) {
        return;
    }

    // Cek apakah ada guardzone individual yang aktif
    bool hasAnyActiveGuardZone = false;
    for (const GuardZone& gz : guardZones) {
        if (gz.active) {
            hasAnyActiveGuardZone = true;
            break;
        }
    }

    if (!hasAnyActiveGuardZone) {
        return;
    }

    // Throttle untuk avoid excessive checking
    QDateTime now = QDateTime::currentDateTime();
    if (lastGuardZoneCheck.msecsTo(now) < 1000) { // Minimum 1 detik interval
        return;
    }
    lastGuardZoneCheck = now;

    qDebug() << "[AUTO-CHECK] === PERFORMING AUTOMATIC GUARDZONE CHECK ==="
             << "redDotTrackerEnabled:" << redDotTrackerEnabled
             << "redDotAttachedToShip:" << redDotAttachedToShip
             << "redDotLat:" << redDotLat;

    // ========== GUNAKAN AIS CLASS YANG SUDAH ADA ==========
    QMap<unsigned int, AISTargetData>& aisTargetMap = Ais::instance()->getTargetMap();

    if (aisTargetMap.isEmpty()) {
        return;
    }

    // Cari semua guardzone aktif
    QList<GuardZone*> activeGuardZones;
    int attachedCount = 0;
    for (GuardZone& gz : guardZones) {
        if (gz.active) {
            activeGuardZones.append(&gz);
            if (gz.attachedToShip) {
                attachedCount++;
            }
        }
    }

    if (activeGuardZones.isEmpty()) {
        return;
    }

    qDebug() << "[AUTO-CHECK] Processing" << activeGuardZones.size() << "active guardzones," << attachedCount << "attached to ship";
    qDebug() << "[AUTO-CHECK] Auto-check enabled:" << guardZoneAutoCheckEnabled << "Timer running:" << guardZoneAutoCheckTimer->isActive();

    // Track targets per guardzone
    QMap<int, QSet<unsigned int>> currentTargetsPerZone;
    QStringList allNewTargetAlerts;

    // ========== CHECK SEMUA AIS TARGETS DARI AIS CLASS ==========
    QMap<unsigned int, EcAISTargetInfo>& targetInfoMap = Ais::instance()->getTargetInfoMap();

    // Check setiap AIS target terhadap setiap guardzone aktif
    for (auto it = aisTargetMap.begin(); it != aisTargetMap.end(); ++it) {
        unsigned int mmsi = it.key();
        const AISTargetData& aisTarget = it.value();

        // Get corresponding EcAISTargetInfo for ship type filtering
        EcAISTargetInfo* targetInfo = nullptr;
        if (targetInfoMap.contains(mmsi)) {
            targetInfo = &targetInfoMap[mmsi];
        }

        // Skip invalid targets
        if (aisTarget.mmsi.isEmpty() || aisTarget.lat == 0.0 || aisTarget.lon == 0.0) {
            continue;
        }

        // Check terhadap setiap guardzone aktif
        for (GuardZone* activeGuardZone : activeGuardZones) {
            bool inGuardZone = false;

            // Check guardzone berdasarkan shape
            if (activeGuardZone->shape == GUARD_ZONE_CIRCLE) {
                double distance, bearing;

                // PERBAIKAN: Use current position for attached guardzone (consistent dengan drawing)
                double centerLat, centerLon;

                if (activeGuardZone->attachedToShip) {
                    // Prefer navShip live position, then redDot, then ownShip
                    if (navShip.lat != 0.0 && navShip.lon != 0.0) {
                        centerLat = navShip.lat;
                        centerLon = navShip.lon;
                        qDebug() << "[AUTO-CHECK] Using navShip position for attached guardzone" << activeGuardZone->id;
                    } else if (redDotTrackerEnabled && redDotLat != 0.0 && redDotLon != 0.0) {
                        centerLat = redDotLat;
                        centerLon = redDotLon;
                        qDebug() << "[AUTO-CHECK] Using current redDot position for attached guardzone" << activeGuardZone->id;
                    } else {
                        centerLat = ownShip.lat;
                        centerLon = ownShip.lon;
                        qDebug() << "[AUTO-CHECK] Using ownShip position for attached guardzone" << activeGuardZone->id;
                    }

                    // Fallback ke stored position jika semua invalid
                    if (qIsNaN(centerLat) || qIsNaN(centerLon) || (centerLat == 0.0 && centerLon == 0.0)) {
                        centerLat = activeGuardZone->centerLat;
                        centerLon = activeGuardZone->centerLon;
                        qDebug() << "[AUTO-CHECK] Using stored position for attached guardzone" << activeGuardZone->id;
                    }

                    qDebug() << "[AUTO-CHECK] Processing attached guardzone" << activeGuardZone->id
                             << "at position:" << centerLat << "," << centerLon
                             << "inner:" << activeGuardZone->innerRadius << "outer:" << activeGuardZone->outerRadius << "NM";
                } else {
                    centerLat = activeGuardZone->centerLat;
                    centerLon = activeGuardZone->centerLon;
                }

                EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                       centerLat,
                                                       centerLon,
                                                       aisTarget.lat, aisTarget.lon,
                                                       &distance, &bearing);
                // For shield guardzone: check if target is within radius
                bool withinRadiusRange = (distance <= activeGuardZone->outerRadius);
                if (activeGuardZone->innerRadius > 0) {
                    // If has inner radius, check donut range
                    withinRadiusRange = (distance <= activeGuardZone->outerRadius && distance >= activeGuardZone->innerRadius);
                }

                // Check if this is a semicircle shield (has angle restrictions)
                bool isSemicircleShield = (activeGuardZone->startAngle != activeGuardZone->endAngle &&
                                           activeGuardZone->startAngle >= 0 && activeGuardZone->endAngle >= 0);

                if (isSemicircleShield && withinRadiusRange) {
                    // Use geometric method like polygon detection for accuracy
                    // Create temporary guardzone with current center position
                    GuardZone tempGZ = *activeGuardZone;
                    tempGZ.centerLat = centerLat;
                    tempGZ.centerLon = centerLon;

                    inGuardZone = isPointInSemicircle(aisTarget.lat, aisTarget.lon, &tempGZ);

                    qDebug() << "[AUTO-CHECK] Semicircle shield - Using geometric detection";
                } else {
                    inGuardZone = withinRadiusRange;
                }

                // DEBUG: Log untuk attached guardzone
                if (activeGuardZone->attachedToShip) {
                    qDebug() << "[AIS-DETECT] Target" << mmsi << "distance:" << distance << "NM from attached guardzone"
                             << activeGuardZone->id << "(inner:" << activeGuardZone->innerRadius << "outer:" << activeGuardZone->outerRadius << "NM) - inZone:" << inGuardZone;
                }
            }
            else if (activeGuardZone->shape == GUARD_ZONE_POLYGON && activeGuardZone->latLons.size() >= 6) {
                // Note: Polygon guardzones attached to ship are not fully supported for auto-check
                // The polygon coordinates are stored as absolute positions, not relative to ship
                // For now, we use the stored coordinates as-is for compatibility
                // TODO: Implement polygon transformation for ship-attached guardzones

                int targetX, targetY;
                if (LatLonToXy(aisTarget.lat, aisTarget.lon, targetX, targetY)) {
                    QPolygon poly;
                    for (int j = 0; j < activeGuardZone->latLons.size(); j += 2) {
                        int x, y;
                        LatLonToXy(activeGuardZone->latLons[j], activeGuardZone->latLons[j+1], x, y);
                        poly.append(QPoint(x, y));
                    }
                    inGuardZone = poly.containsPoint(QPoint(targetX, targetY), Qt::OddEvenFill);
                }
            }

            if (inGuardZone) {
                // Track target untuk guardzone ini
                currentTargetsPerZone[activeGuardZone->id].insert(mmsi);

                // Check jika ini target baru yang masuk zone untuk guardzone ini
                QSet<unsigned int>& previousTargetsForThisZone = previousTargetsPerZone[activeGuardZone->id];
                if (!previousTargetsForThisZone.contains(mmsi)) {
                    // DEBUG: Log AIS target entering
                    qDebug() << "[AIS-ENTER] Target" << mmsi << "(" << aisTarget.mmsi << ") entering guardzone"
                             << activeGuardZone->id << activeGuardZone->name;
                    // ========== APPLY SHIP TYPE FILTER ==========
                    if (activeGuardZone->shipTypeFilter != SHIP_TYPE_ALL && targetInfo) {
                        ShipTypeFilter shipType = guardZoneManager->getShipTypeFromAIS(targetInfo->shipType);
                        if (shipType != activeGuardZone->shipTypeFilter) {
                            continue;  // Skip this ship, doesn't match filter
                        }
                    }
                    // ==========================================

                    // ========== APPLY ALERT DIRECTION FILTER ==========
                    // For ENTERING events
                    if (activeGuardZone->alertDirection == ALERT_OUT_ONLY) {
                        qDebug() << "[AUTO-CHECK] Alert filtered out - OUT_ONLY mode, ignoring ENTER event for:" << aisTarget.mmsi << "in GuardZone" << activeGuardZone->id;
                        continue;  // Skip entering events if set to OUT_ONLY
                    }
                    // ================================================

                    allNewTargetAlerts.append(tr("NEW: AIS %1 entered GuardZone '%2' - SOG: %3kts, COG: %4°")
                                              .arg(aisTarget.mmsi)
                                              .arg(activeGuardZone->name)
                                              .arg(aisTarget.sog, 0, 'f', 1)
                                              .arg(aisTarget.cog, 0, 'f', 0));

                    QString attachmentInfo = activeGuardZone->attachedToShip ? " [ATTACHED TO SHIP]" : "";
                    qDebug() << "[AUTO-CHECK] 🚨 NEW TARGET ENTERED:" << aisTarget.mmsi << "in GuardZone" << activeGuardZone->id << "(" << activeGuardZone->name << ")" << attachmentInfo;

                    // Emit signal untuk AISTargetPanel dengan format yang sama seperti GuardZoneManager
                    QString alertMessage = QString("Ship %1 entered GuardZone '%2'")
                                         .arg(aisTarget.mmsi)
                                         .arg(activeGuardZone->name);

                    // Emit signal dari EcWidget untuk AISTargetPanel
                    emit aisTargetDetected(activeGuardZone->id, aisTarget.mmsi.toUInt(), alertMessage);
                }
            }
        }
    }
    // ===============================================

    // Check targets yang keluar dari setiap guardzone
    for (GuardZone* activeGuardZone : activeGuardZones) {
        QSet<unsigned int>& previousTargetsForThisZone = previousTargetsPerZone[activeGuardZone->id];
        QSet<unsigned int>& currentTargetsForThisZone = currentTargetsPerZone[activeGuardZone->id];

        for (unsigned int mmsi : previousTargetsForThisZone) {
            if (!currentTargetsForThisZone.contains(mmsi)) {
                // DEBUG: Log AIS target exiting
                qDebug() << "[AIS-EXIT] Target" << mmsi << "exiting guardzone"
                         << activeGuardZone->id << activeGuardZone->name;
                // ========== APPLY ALERT DIRECTION FILTER ==========
                // For EXITING events
                if (activeGuardZone->alertDirection == ALERT_IN_ONLY) {
                    qDebug() << "[AUTO-CHECK] Alert filtered out - IN_ONLY mode, ignoring EXIT event for:" << mmsi << "from GuardZone" << activeGuardZone->id;
                    continue;  // Skip exiting events if set to IN_ONLY
                }
                // ================================================

                // Note: Ship type filter tidak diterapkan untuk exit events
                // karena EcAISTargetInfo mungkin sudah tidak tersedia
                QString attachmentInfo = activeGuardZone->attachedToShip ? " [ATTACHED TO SHIP]" : "";
                qDebug() << "[AUTO-CHECK] ✅ TARGET EXITED:" << mmsi << "from GuardZone" << activeGuardZone->id << "(" << activeGuardZone->name << ")" << attachmentInfo;

                // Emit signal untuk AISTargetPanel dengan format yang sama seperti GuardZoneManager
                QString alertMessage = QString("Ship %1 exited GuardZone '%2'")
                                     .arg(mmsi)
                                     .arg(activeGuardZone->name);

                // Emit signal dari EcWidget untuk AISTargetPanel
                emit aisTargetDetected(activeGuardZone->id, mmsi, alertMessage);
            }
        }
    }

    // Update cache untuk setiap guardzone
    previousTargetsPerZone = currentTargetsPerZone;

    // Show alert untuk new targets
    if (!allNewTargetAlerts.isEmpty()) {
        QString alertMessage = tr("GuardZone Alert (%1 targets detected):\n%2")
                              .arg(allNewTargetAlerts.size())
                              .arg(allNewTargetAlerts.join("\n"));

        // Show QMessageBox alert - DISABLED to remove popup
        // QMessageBox::warning(this, tr("GuardZone Auto-Check Alert"), alertMessage);

        // Emit signal untuk alert system jika ada - untuk semua guardzone yang terdeteksi
        for (GuardZone* gz : activeGuardZones) {
            if (currentTargetsPerZone[gz->id].size() > 0) {
                emit guardZoneTargetDetected(gz->id, currentTargetsPerZone[gz->id].size());
            }
        }

        qDebug() << "[AUTO-CHECK] Alert triggered for" << allNewTargetAlerts.size() << "new targets across" << activeGuardZones.size() << "guardzones";
    }

    // Check pick report obstacles for ship guardian zones
    checkPickReportObstaclesInShipGuardian();
}

void EcWidget::setGuardZoneAutoCheck(bool enabled)
{
    guardZoneAutoCheckEnabled = enabled;

    if (enabled && guardZoneActive) {
        guardZoneAutoCheckTimer->start(guardZoneCheckInterval);
        qDebug() << "[AUTO-CHECK] GuardZone auto-check ENABLED with interval:" << guardZoneCheckInterval << "ms";
    } else {
        guardZoneAutoCheckTimer->stop();
        previousTargetsInZone.clear(); // Clear cache
        qDebug() << "[AUTO-CHECK] GuardZone auto-check DISABLED";
    }
}

void EcWidget::setGuardZoneCheckInterval(int intervalMs)
{
    guardZoneCheckInterval = qMax(1000, intervalMs); // Minimum 1 detik

    if (guardZoneAutoCheckTimer->isActive()) {
        guardZoneAutoCheckTimer->setInterval(guardZoneCheckInterval);
        qDebug() << "[AUTO-CHECK] Interval updated to:" << guardZoneCheckInterval << "ms";
    }
}

bool EcWidget::isPointInPolygon(double lat, double lon, const QVector<double>& polygonLatLons)
{
    if (polygonLatLons.size() < 6) { // Minimum 3 points = 6 coordinates
        qDebug() << "[POLYGON-CHECK] Insufficient polygon points:" << polygonLatLons.size();
        return false;
    }

    // Method 1: Improved Geographic Point-in-Polygon menggunakan Winding Number Algorithm
    bool inPolygonGeo = checkPointInPolygonGeographic(lat, lon, polygonLatLons);

    // Method 2: Screen Coordinate Point-in-Polygon sebagai backup verification
    bool inPolygonScreen = checkPointInPolygonScreen(lat, lon, polygonLatLons);

    qDebug() << "[POLYGON-CHECK] Point (" << lat << "," << lon << ")";
    qDebug() << "[POLYGON-CHECK] Geographic method:" << inPolygonGeo;
    qDebug() << "[POLYGON-CHECK] Screen method:" << inPolygonScreen;

    // Clean decision logic: prioritize screen method for display accuracy
    // Geographic method untuk validasi tambahan
    if (inPolygonGeo == inPolygonScreen) {
        // Kedua method setuju - confident result
        return inPolygonGeo;
    } else {
        // Methods disagree - gunakan screen method (lebih reliable untuk visual display)
        qDebug() << "[POLYGON-CHECK] WARNING: Methods disagree! Using screen method for reliability.";
        return inPolygonScreen;
    }
}

bool EcWidget::checkPointInPolygonGeographic(double lat, double lon, const QVector<double>& polygonLatLons)
{
    int numVertices = polygonLatLons.size() / 2;
    int windingNumber = 0;

    for (int i = 0; i < numVertices; i++) {
        int j = (i + 1) % numVertices; // Next vertex (wrap around)

        double lat1 = polygonLatLons[i * 2];
        double lon1 = polygonLatLons[i * 2 + 1];
        double lat2 = polygonLatLons[j * 2];
        double lon2 = polygonLatLons[j * 2 + 1];

        // Handle longitude wraparound crossing (±180° meridian)
        double deltaLon = lon2 - lon1;
        if (deltaLon > 180.0) {
            deltaLon -= 360.0;
        } else if (deltaLon < -180.0) {
            deltaLon += 360.0;
        }
        lon2 = lon1 + deltaLon;

        // Winding number calculation for geographic coordinates
        if (lat1 <= lat) {
            if (lat2 > lat) { // Upward crossing
                if (calculateCrossProduct(lat, lon, lat1, lon1, lat2, lon2) > 0) {
                    windingNumber++;
                }
            }
        } else {
            if (lat2 <= lat) { // Downward crossing
                if (calculateCrossProduct(lat, lon, lat1, lon1, lat2, lon2) < 0) {
                    windingNumber--;
                }
            }
        }
    }

    return windingNumber != 0;
}

bool EcWidget::checkPointInPolygonScreen(double lat, double lon, const QVector<double>& polygonLatLons)
{
    // Convert target point to screen coordinates
    int targetX, targetY;
    if (!LatLonToXy(lat, lon, targetX, targetY)) {
        qDebug() << "[POLYGON-CHECK] Failed to convert target coordinates to screen";
        return false;
    }

    // Convert polygon vertices to screen coordinates
    QPolygon screenPolygon;
    int validPoints = 0;

    for (int i = 0; i < polygonLatLons.size(); i += 2) {
        int x, y;
        if (LatLonToXy(polygonLatLons[i], polygonLatLons[i+1], x, y)) {
            screenPolygon.append(QPoint(x, y));
            validPoints++;
        } else {
            qDebug() << "[POLYGON-CHECK] WARNING: Failed to convert polygon vertex" << (i/2) << "to screen coords";
        }
    }

    // Require minimum 3 valid points for a polygon
    if (validPoints < 3) {
        qDebug() << "[POLYGON-CHECK] ERROR: Insufficient valid screen coordinates:" << validPoints;
        return false;
    }

    // Use Qt's robust polygon containment check
    bool result = screenPolygon.containsPoint(QPoint(targetX, targetY), Qt::OddEvenFill);

    qDebug() << "[POLYGON-CHECK] Target screen coords: (" << targetX << "," << targetY << ")";
    qDebug() << "[POLYGON-CHECK] Valid polygon screen points:" << validPoints;

    return result;
}

double EcWidget::calculateCrossProduct(double pointLat, double pointLon,
                                      double lat1, double lon1, double lat2, double lon2)
{
    // Convert to simple Mercator-like projection untuk cross product
    // Ini cukup akurat untuk deteksi polygon dalam area regional
    const double DEG_TO_RAD = M_PI / 180.0;

    // Simple cylindrical projection (good enough for regional areas)
    double x1 = lon1 * DEG_TO_RAD;
    double y1 = log(tan(M_PI_4 + lat1 * DEG_TO_RAD / 2.0));
    double x2 = lon2 * DEG_TO_RAD;
    double y2 = log(tan(M_PI_4 + lat2 * DEG_TO_RAD / 2.0));
    double px = pointLon * DEG_TO_RAD;
    double py = log(tan(M_PI_4 + pointLat * DEG_TO_RAD / 2.0));

    // Cross product: (v2-v1) × (point-v1)
    return ((x2 - x1) * (py - y1) - (y2 - y1) * (px - x1));
}

// ========== SEMICIRCLE GUARDZONE DETECTION ==========
bool EcWidget::isPointInSemicircle(double lat, double lon, const GuardZone* gz)
{
    if (!gz || gz->shape != GUARD_ZONE_CIRCLE) {
        qDebug() << "[SEMICIRCLE-CHECK] Invalid guardzone";
        return false;
    }

    // Step 1: Check if point is within radius range
    double distance, bearing;
    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                           gz->centerLat, gz->centerLon,
                                           lat, lon,
                                           &distance, &bearing);

    bool withinRadius = (distance <= gz->outerRadius);
    if (gz->innerRadius > 0) {
        withinRadius = (distance <= gz->outerRadius && distance >= gz->innerRadius);
    }

    if (!withinRadius) {
        qDebug() << "[SEMICIRCLE-CHECK] Point" << lat << "," << lon << "outside radius range - distance:" << distance
                 << "inner:" << gz->innerRadius << "outer:" << gz->outerRadius;
        return false;
    } else {
        qDebug() << "[SEMICIRCLE-CHECK] Point" << lat << "," << lon << "within radius - distance:" << distance
                 << "inner:" << gz->innerRadius << "outer:" << gz->outerRadius;
    }

    // Step 2: Check if point is within semicircle angle range
    // Convert startAngle and endAngle from guardzone to determine the semicircle direction
    double startAngle = gz->startAngle;
    double endAngle = gz->endAngle;

    // Normalize bearing to 0-360
    bearing = fmod(bearing + 360.0, 360.0);

    // For semicircle: check if bearing is in the "front" semicircle
    // Current configuration: startAngle = heading+90°, endAngle = heading+270°
    // Visual guardzone: from starboard through BOW to port (FRONT semicircle)
    // Detection should EXCLUDE the range from startAngle to endAngle (which is the BACK semicircle)

    bool withinAngle;
    if (startAngle < endAngle) {
        // Simple case: e.g., 90° to 270° - EXCLUDE this range (back semicircle)
        withinAngle = !(bearing >= startAngle && bearing <= endAngle);
    } else {
        // Wrap-around case: e.g., 270° to 90° - EXCLUDE this range
        withinAngle = !(bearing >= startAngle || bearing <= endAngle);
    }

    qDebug() << "[SEMICIRCLE-CHECK] Point (" << lat << "," << lon << ")";
    qDebug() << "[SEMICIRCLE-CHECK] Distance:" << distance << "Bearing:" << bearing;
    qDebug() << "[SEMICIRCLE-CHECK] Angle range:" << startAngle << "to" << endAngle;
    qDebug() << "[SEMICIRCLE-CHECK] Within radius:" << withinRadius << "Within angle:" << withinAngle;
    qDebug() << "[SEMICIRCLE-CHECK] Final result:" << (withinRadius && withinAngle);

    return (withinRadius && withinAngle);
}

// Obstacle marker functions implementation
void EcWidget::addObstacleMarker(double lat, double lon, const QString& dangerLevel,
                                const QString& objectName, const QString& information)
{
    // CRITICAL: Validate input parameters
    if (qIsNaN(lat) || qIsNaN(lon) ||
        lat < -90.0 || lat > 90.0 ||
        lon < -180.0 || lon > 180.0) {
        qDebug() << "[OBSTACLE-MARKER] Invalid coordinates - marker not added:" << lat << "," << lon;
        return;
    }

    if (dangerLevel.isEmpty() || objectName.isEmpty()) {
        qDebug() << "[OBSTACLE-MARKER] Invalid marker data - marker not added";
        return;
    }

    try {
        ObstacleMarker marker;
        marker.lat = lat;
        marker.lon = lon;
        marker.dangerLevel = dangerLevel;
        marker.objectName = objectName;
        marker.information = information;
        marker.timestamp = QDateTime::currentDateTime();

        // Check for duplicates
        for (const ObstacleMarker& existing : obstacleMarkers) {
            if (qAbs(existing.lat - lat) < 0.0001 && qAbs(existing.lon - lon) < 0.0001 &&
                existing.dangerLevel == dangerLevel && existing.objectName == objectName) {
                qDebug() << "[OBSTACLE-MARKER] Duplicate marker - skipping:" << objectName;
                return; // Skip duplicate
            }
        }

        // Limit number of markers to prevent memory issues
        if (obstacleMarkers.size() >= 1000) {
            qDebug() << "[OBSTACLE-MARKER] Too many markers - removing oldest";
            obstacleMarkers.removeFirst();
        }

        obstacleMarkers.append(marker);
        qDebug() << "[OBSTACLE-MARKER] Added marker:" << objectName << "at" << lat << "," << lon << "level:" << dangerLevel;

        // Start chart flashing if this is a dangerous obstacle
        if (dangerLevel == "DANGEROUS") {
            startChartFlashing();
        }

        // Check and update flashing status based on current obstacles
        QTimer::singleShot(100, this, [this]() {
            bool hasDangerous = hasDangerousObstacles();
            qDebug() << "[FLASHING-DEBUG] After adding obstacle, has dangerous:" << hasDangerous
                     << "Total markers:" << obstacleMarkers.size() << "Flashing active:" << chartFlashTimer->isActive();
            if (!hasDangerous) {
                qDebug() << "[FLASHING-DEBUG] Stopping flashing - no dangerous obstacles";
                stopChartFlashing();
            }
        });

        // Trigger repaint to show new marker (safe update)
        QTimer::singleShot(0, this, [this]() { update(); });

    } catch (const std::exception& e) {
        qDebug() << "[OBSTACLE-MARKER] Exception in addObstacleMarker:" << e.what();
    } catch (...) {
        qDebug() << "[OBSTACLE-MARKER] Unknown exception in addObstacleMarker";
    }
}

void EcWidget::clearObstacleMarkers()
{
    try {
        obstacleMarkers.clear();
        qDebug() << "[OBSTACLE-MARKER] Cleared all obstacle markers";

        // Stop chart flashing when all obstacles cleared
        stopChartFlashing();

        // Safe update using timer
        QTimer::singleShot(0, this, [this]() { update(); });

    } catch (const std::exception& e) {
        qDebug() << "[OBSTACLE-MARKER] Exception in clearObstacleMarkers:" << e.what();
    } catch (...) {
        qDebug() << "[OBSTACLE-MARKER] Unknown exception in clearObstacleMarkers";
    }
}

void EcWidget::drawObstacleMarkers(QPainter& painter)
{
    // CRITICAL: Add extensive safety checks to prevent crash
    try {
        if (obstacleMarkers.isEmpty() || !initialized || !view) {
            return; // Early return if no markers or not initialized
        }

        // Validate painter state
        if (!painter.device()) {
            qDebug() << "[OBSTACLE-MARKER] Invalid painter device - skipping";
            return;
        }

        // Save painter state
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        for (const ObstacleMarker& marker : obstacleMarkers) {
            // CRITICAL: Validate marker data
            if (qIsNaN(marker.lat) || qIsNaN(marker.lon) ||
                marker.lat < -90.0 || marker.lat > 90.0 ||
                marker.lon < -180.0 || marker.lon > 180.0) {
                qDebug() << "[OBSTACLE-MARKER] Invalid coordinates - skipping marker";
                continue;
            }

            // Convert lat/lon to screen coordinates with validation
            int x, y;
            if (!LatLonToXy(marker.lat, marker.lon, x, y)) {
                continue; // Skip if coordinate conversion fails
            }

            // CRITICAL: Validate screen coordinates
            QRect widgetRect = rect();
            if (x < -100 || x > widgetRect.width() + 100 ||
                y < -100 || y > widgetRect.height() + 100) {
                continue; // Skip markers too far outside widget bounds
            }

            // Set color based on danger level with validation
            QColor markerColor(128, 128, 128, 200); // Default gray
            if (marker.dangerLevel == "DANGEROUS") {
                markerColor = QColor(255, 0, 0, 200); // Red
            } else if (marker.dangerLevel == "WARNING") {
                markerColor = QColor(255, 165, 0, 200); // Orange
            } else if (marker.dangerLevel == "NOTE") {
                markerColor = QColor(0, 100, 255, 200); // Blue
            }

            // Draw filled circle with safety bounds
            painter.setBrush(QBrush(markerColor));
            painter.setPen(QPen(Qt::white, 2));

            int markerSize = 8;
            QRect markerRect(x - markerSize/2, y - markerSize/2, markerSize, markerSize);
            painter.drawEllipse(markerRect);

            // Draw pulsing effect for dangerous obstacles (simplified)
            if (marker.dangerLevel == "DANGEROUS") {
                // Simplified pulsing without complex math
                int pulseSize = markerSize + 2;
                QColor pulseColor(255, 0, 0, 100);
                painter.setBrush(QBrush(pulseColor));
                painter.setPen(Qt::NoPen);
                QRect pulseRect(x - pulseSize/2, y - pulseSize/2, pulseSize, pulseSize);
                painter.drawEllipse(pulseRect);
            }

            // Skip text drawing to avoid font-related crashes
            // Text drawing disabled for stability
        }

        // Restore painter state
        painter.restore();

    } catch (const std::exception& e) {
        qDebug() << "[OBSTACLE-MARKER] Exception in drawObstacleMarkers:" << e.what();
    } catch (...) {
        qDebug() << "[OBSTACLE-MARKER] Unknown exception in drawObstacleMarkers";
    }
}

// Chart flashing functions for dangerous obstacles
bool EcWidget::hasDangerousObstacles() const
{
    int dangerousCount = 0;
    for (const ObstacleMarker& marker : obstacleMarkers) {
        if (marker.dangerLevel == "DANGEROUS") {
            dangerousCount++;
            qDebug() << "[DANGEROUS-CHECK] Found dangerous obstacle:" << marker.objectName
                     << "at" << marker.lat << "," << marker.lon
                     << "age:" << marker.timestamp.secsTo(QDateTime::currentDateTime()) << "seconds";
        }
    }
    qDebug() << "[DANGEROUS-CHECK] Total dangerous obstacles:" << dangerousCount << "Total markers:" << obstacleMarkers.size();
    return dangerousCount > 0;
}

void EcWidget::drawChartFlashOverlay(QPainter& painter)
{
    // Only flash if there are dangerous obstacles and flash is visible
    if (!hasDangerousObstacles() || !chartFlashVisible) {
        return;
    }

    // Draw red flashing overlay over entire chart
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);

    // Create semi-transparent red overlay
    QColor flashColor(255, 0, 0, 60); // Red with transparency
    painter.fillRect(rect(), flashColor);

    painter.restore();
}

void EcWidget::startChartFlashing()
{
    if (!chartFlashTimer->isActive()) {
        chartFlashTimer->start(500); // Flash every 500ms
        qDebug() << "[CHART-FLASH] Started chart flashing for dangerous obstacles";

        // Emit signal to start sound alarm
        emit dangerousObstacleDetected();
    }
}

void EcWidget::stopChartFlashing()
{
    if (chartFlashTimer->isActive()) {
        chartFlashTimer->stop();
        chartFlashVisible = false;
        update(); // Clear any remaining flash
        qDebug() << "[CHART-FLASH] Stopped chart flashing";

        // Emit signal to stop sound alarm
        emit dangerousObstacleCleared();
    }
}

// Remove obstacle markers that are no longer in guardzone or too old
void EcWidget::removeOutdatedObstacleMarkers()
{
    if (obstacleMarkers.isEmpty()) return;

    // Find active attached guardzone
    GuardZone* attachedGuardZone = nullptr;
    for (GuardZone& gz : guardZones) {
        if (gz.attachedToShip && gz.active) {
            attachedGuardZone = &gz;
            break;
        }
    }

    if (!attachedGuardZone) {
        // No attached guardzone - clear all markers
        clearObstacleMarkers();
        return;
    }

    // Remove markers that are outside guardzone or too old (>10 seconds for faster cleanup)
    QDateTime cutoffTime = QDateTime::currentDateTime().addSecs(-10);
    auto it = obstacleMarkers.begin();
    bool removedAny = false;
    bool hadDangerousObstacles = hasDangerousObstacles();

    while (it != obstacleMarkers.end()) {
        bool shouldRemove = false;

        // Check if marker is too old (reduced to 10 seconds)
        if (it->timestamp < cutoffTime) {
            shouldRemove = true;
            qDebug() << "[OBSTACLE-CLEANUP] Removing old marker:" << it->objectName;
        }
        // Check if marker is outside current guardzone
        else {
            bool inSemicircle = isPointInSemicircle(it->lat, it->lon, attachedGuardZone);
            if (!inSemicircle) {
                shouldRemove = true;
                qDebug() << "[OBSTACLE-CLEANUP] Removing marker outside guardzone:" << it->objectName
                         << "at" << it->lat << "," << it->lon;
            } else {
                qDebug() << "[OBSTACLE-CLEANUP] Keeping marker inside guardzone:" << it->objectName
                         << "at" << it->lat << "," << it->lon;
            }
        }

        if (shouldRemove) {
            it = obstacleMarkers.erase(it);
            removedAny = true;
        } else {
            ++it;
        }
    }

    if (removedAny) {
        qDebug() << "[OBSTACLE-CLEANUP] Removed obstacles, remaining markers:" << obstacleMarkers.size();

        // Check if we still have dangerous obstacles after cleanup
        bool stillHasDangerous = hasDangerousObstacles();
        qDebug() << "[OBSTACLE-CLEANUP] Had dangerous:" << hadDangerousObstacles << "Still has dangerous:" << stillHasDangerous;

        // Stop flashing if no dangerous obstacles remain
        if (hadDangerousObstacles && !stillHasDangerous) {
            qDebug() << "[OBSTACLE-CLEANUP] No more dangerous obstacles, stopping flashing";
            stopChartFlashing();
        }

        update(); // Trigger repaint
    }
}

// ====== ROUTE MODE IMPLEMENTATIONS ======

void EcWidget::startRouteMode()
{
    //qDebug() << "[ROUTE] Starting route mode";
    isRouteMode = true;
    // Ensure we start a brand-new route ID and clean state
    currentRouteId = getNextAvailableRouteId();
    routeWaypointCounter = 1;
    resetRouteConnections();
    //qDebug() << "[ROUTE] Route mode started. New route ID:" << currentRouteId;
}

void EcWidget::startAppendWaypointMode(int routeId)
{
    if (routeId <= 0) return;

    // Switch to route creation mode but target an existing route
    isRouteMode = true;
    activeFunction = CREATE_ROUTE;
    currentRouteId = routeId;

    // Set next waypoint counter to append to the end of the route
    int count = 0;
    for (const auto& wp : waypointList) {
        if (wp.routeId == routeId) count++;
    }
    routeWaypointCounter = count + 1;

    // Prepare ghost waypoint context
    ghostWaypoint.visible = false; // will become visible on mouse move
    ghostWaypoint.routeId = routeId;
    ghostWaypoint.waypointIndex = -1;

    if (mainWindow) {
        mainWindow->setWindowTitle(QString(APP_TITLE) + " - Add Waypoint by Mouse");
        mainWindow->routesStatusText->setText(
            tr("Route Mode: Click to add waypoint. Press ESC or right-click to finish")
        );
    }

    //qDebug() << "[ROUTE] Append waypoint mode started for route" << routeId
             //<< "next index:" << routeWaypointCounter;
}

void EcWidget::resetRouteConnections()
{
    // This function ensures that previous routes are properly terminated
    // and won't connect to the new route being created

    //qDebug() << "[ROUTE] Resetting route connections before starting new route" << (currentRouteId + 1);

    // Force save current waypoints to ensure proper separation
    if (!waypointList.isEmpty()) {
        saveWaypoints();
    }

    // Update display to ensure clean slate for new route
    update();
}

void EcWidget::endRouteMode()
{
    //qDebug() << "[ROUTE] Ending route mode";

    if (isRouteMode) {
        // Finalize current route
        finalizeCurrentRoute();

        // Show confirmation message
        int waypointsInRoute = 0;
        for (const Waypoint &wp : waypointList) {
            if (wp.routeId == currentRouteId) {
                waypointsInRoute++;
            }
        }

        if (waypointsInRoute > 0) {
            QMessageBox::information(this, tr("Route Created"),
                tr("Route R%1 created with %2 waypoints").arg(currentRouteId).arg(waypointsInRoute));
        }

        // Prepare for next route - find next available route ID
        currentRouteId = getNextAvailableRouteId();
        routeWaypointCounter = 1;
    }

    isRouteMode = false;
    activeFunction = PAN;

    // Clear ghost waypoint
    ghostWaypoint.visible = false;

    // Update status
    if (mainWindow) {
        //mainWindow->statusBar()->showMessage(tr("Route creation ended"), 3000);
        mainWindow->routesStatusText->setText(tr("Route creation ended"));

        mainWindow->setWindowTitle(APP_TITLE);
    }

    //qDebug() << "[ROUTE] Route mode ended. Next route ID will be:" << currentRouteId;
}

void EcWidget::drawRoutesSeparately()
{
    // Draw routes with different colors/styles to separate them visually
    // This is a workaround for SevenCs cell limitation

    if (waypointList.isEmpty()) return;

    // Group waypoints by route
    QMap<int, QList<Waypoint*>> routeGroups;
    for (Waypoint &wp : waypointList) {
        routeGroups[wp.routeId].append(&wp);
    }

    qDebug() << "[DRAW] Drawing" << routeGroups.size() << "routes separately";

    // For each route, ensure waypoints are visible
    for (auto it = routeGroups.begin(); it != routeGroups.end(); ++it) {
        int routeId = it.key();
        QList<Waypoint*> &waypoints = it.value();

        if (waypoints.isEmpty()) continue;

        qDebug() << "[DRAW] Route" << routeId << "has" << waypoints.size() << "waypoints";

        // DISABLED: Route waypoints tidak menggunakan SevenCs engine
        // Hanya visual drawing manual yang digunakan untuk route waypoints
        for (Waypoint* wp : waypoints) {
            if (wp->routeId > 0) {
                // Route waypoints: set invalid handle to ensure no SevenCs engine usage
                wp->featureHandle.id = EC_NOCELLID;
                wp->featureHandle.offset = 0;
                qDebug() << "[DRAW] Route waypoint" << wp->label << "set to manual drawing mode only";
            }
        }
    }
}

EcCellId EcWidget::getOrCreateRouteCellId(int routeId)
{
    // Check if we already have a cell for this route
    if (routeCells.contains(routeId)) {
        //qDebug() << "[ROUTE] Using existing cell for route" << routeId;
        return routeCells[routeId];
    }

    // Create new cell for this route by creating a separate waypoint cell
    EcCellId newRouteCell = createNewRouteCellId();

    if (newRouteCell != EC_NOCELLID) {
        routeCells[routeId] = newRouteCell;
        //qDebug() << "[ROUTE] Created new cell for route" << routeId;
        return newRouteCell;
    } else {
        qDebug() << "[ERROR] Failed to create new cell for route" << routeId;
        return EC_NOCELLID;
    }
}

EcCellId EcWidget::createNewRouteCellId()
{
    // Create a completely new cell for route waypoints
    // This ensures each route has its own independent cell

    // Use SevenCs API to create a new cell
    EcCellId newCell = EC_NOCELLID;

    // Try to create waypoint cell - but don't interfere with main udoCid
    EcCellId tempUdoCid = udoCid; // Save current udoCid

    if (createWaypointCell()) {
        newCell = udoCid; // Get the newly created cell
        udoCid = tempUdoCid; // Restore original udoCid
        //qDebug() << "[ROUTE] Successfully created new route cell";
        return newCell;
    } else {
        udoCid = tempUdoCid; // Restore original udoCid on failure
        qDebug() << "[ERROR] Failed to create new route cell";
        return EC_NOCELLID;
    }
}


void EcWidget::finalizeCurrentRoute()
{
    // This function ensures that the current route is properly terminated
    // and won't be connected to the next route

    if (waypointList.isEmpty()) return;

    // Count waypoints in current route
    int waypointsInCurrentRoute = 0;
    for (const Waypoint &wp : waypointList) {
        if (wp.routeId == currentRouteId) {
            waypointsInCurrentRoute++;
        }
    }

    //qDebug() << "[ROUTE] Finalized route" << currentRouteId << "with" << waypointsInCurrentRoute << "waypoints";

    // Force redraw to ensure proper visualization
    saveWaypoints();

    // Persist route only if it has waypoints; rebuild from waypointList
    if (currentRouteId > 0 && waypointsInCurrentRoute > 0) {
        updateRouteList(currentRouteId);
        saveRoutes();
    }

    update();
}

// ====== ROUTE MANAGEMENT FUNCTIONS ======

void EcWidget::saveRoutes()
{
    QJsonArray routeArray;

    for (const EcWidget::Route &route : routeList)
    {
        // Skip routes without waypoints to avoid persisting empty routes
        if (route.waypoints.isEmpty()) {
            continue;
        }
        QJsonObject routeObject;
        routeObject["routeId"] = route.routeId;
        routeObject["name"] = route.name;
        routeObject["description"] = route.description;
        routeObject["createdDate"] = route.createdDate.toString(Qt::ISODate);
        routeObject["modifiedDate"] = route.modifiedDate.toString(Qt::ISODate);
        routeObject["totalDistance"] = route.totalDistance;
        routeObject["estimatedTime"] = route.estimatedTime;
        routeObject["visible"] = isRouteVisible(route.routeId); // Save visibility state
        routeObject["attachedToShip"] = route.attachedToShip; // Save attached to ship state

        // Persist custom color if any
        if (routeCustomColors.contains(route.routeId)) {
            QColor c = routeCustomColors.value(route.routeId);
            routeObject["color"] = c.name(QColor::HexRgb); // e.g., #RRGGBB
        }

        // Save waypoints with full coordinate data
        QJsonArray waypointsArray;
        for (const EcWidget::RouteWaypoint& wp : route.waypoints) {
            QJsonObject waypointObject;
            waypointObject["lat"] = wp.lat;
            waypointObject["lon"] = wp.lon;
            waypointObject["label"] = wp.label;
            waypointObject["remark"] = wp.remark;
            waypointObject["turningRadius"] = wp.turningRadius;
            waypointObject["active"] = wp.active;
            waypointsArray.append(waypointObject);
        }
        routeObject["waypoints"] = waypointsArray;

        routeArray.append(routeObject);
    }

    QJsonObject rootObject;
    rootObject["routes"] = routeArray;

    QJsonDocument jsonDoc(rootObject);

    QString filePath = getRouteFilePath();
    QDir dir = QFileInfo(filePath).dir();

    // Pastikan direktori ada
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            qDebug() << "[ERROR] Could not create directory for routes:" << dir.path();
            filePath = "routes.json"; // Gunakan direktori saat ini sebagai fallback
        }
    }

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write(jsonDoc.toJson(QJsonDocument::Indented));
        file.close();
        qDebug() << "[INFO] Routes saved to" << filePath;
    }
    else
    {
        qDebug() << "[ERROR] Failed to save routes to" << filePath << ":" << file.errorString();

        // Coba simpan di direktori saat ini sebagai cadangan
        QFile fallbackFile("routes.json");
        if (fallbackFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            fallbackFile.write(jsonDoc.toJson(QJsonDocument::Indented));
            fallbackFile.close();
            qDebug() << "[INFO] Routes saved to fallback location: routes.json";
        }
        else
        {
            qDebug() << "[ERROR] Failed to save routes to fallback location";
        }
    }
}

void EcWidget::updateRouteFromWaypoint(int routeId)
{
    qDebug() << "[UPDATE-ROUTE] Updating route" << routeId << "from waypoint changes";

    // Preserve visibility before route update
    bool wasVisible = isRouteVisible(routeId);
    qDebug() << "[UPDATE-ROUTE] Route" << routeId << "visibility before update:" << wasVisible;

    // Find the route in routeList
    for (int i = 0; i < routeList.size(); ++i) {
        if (routeList[i].routeId == routeId) {
            Route& route = routeList[i];

            // Clear existing waypoints in route
            route.waypoints.clear();

            // Update waypoints from waypointList
            for (const Waypoint& wp : waypointList) {
                if (wp.routeId == routeId) {
                    RouteWaypoint routeWp;
                    routeWp.lat = wp.lat;
                    routeWp.lon = wp.lon;
                    routeWp.label = wp.label;
                    routeWp.remark = wp.remark;
                    routeWp.turningRadius = wp.turningRadius;
                    routeWp.active = wp.active;
                    route.waypoints.append(routeWp);
                }
            }

            // Update route metadata
            route.modifiedDate = QDateTime::currentDateTime();

            // Recalculate route distance and time
            calculateRouteData(route);

            qDebug() << "[UPDATE-ROUTE] Updated route" << routeId << "with" << route.waypoints.size() << "waypoints";

            // PRESERVE VISIBILITY - DO NOT MODIFY
            qDebug() << "[UPDATE-ROUTE] Route" << routeId << "visibility preserved (not modified):" << wasVisible;

            break;
        }
    }
}

void EcWidget::loadRoutes()
{
    QString filePath = getRouteFilePath();
    QFile file(filePath);

    if (!file.exists())
    {
        qDebug() << "[INFO] Routes file not found. Starting with empty route list.";
        return;
    }

    if (file.open(QIODevice::ReadOnly))
    {
        QByteArray fileData = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(fileData);
        QJsonObject rootObject = doc.object();
        QJsonArray routesArray = rootObject["routes"].toArray();

        routeList.clear();

        for (const QJsonValue& value : routesArray)
        {
            QJsonObject routeObject = value.toObject();

            EcWidget::Route route;
            route.routeId = routeObject["routeId"].toInt();
            route.name = routeObject["name"].toString();
            route.description = routeObject["description"].toString();
            route.createdDate = QDateTime::fromString(routeObject["createdDate"].toString(), Qt::ISODate);
            route.modifiedDate = QDateTime::fromString(routeObject["modifiedDate"].toString(), Qt::ISODate);
            route.totalDistance = routeObject["totalDistance"].toDouble();
            route.estimatedTime = routeObject["estimatedTime"].toDouble();

            // Load visibility state from file (respecting user's saved preferences)
            bool routeVisible = routeObject["visible"].toBool(true); // Default to visible for new routes only

            // Load attached to ship state from file (check both old and new key names for compatibility)
            route.attachedToShip = routeObject["attachedToShip"].toBool(
                routeObject["active"].toBool(false) // Fallback to old "active" key for compatibility
            );

            setRouteVisibility(route.routeId, routeVisible);
            qDebug() << "[ROUTE-LOAD] Route" << route.routeId << "loaded with saved visibility:" << routeVisible << "attachedToShip:" << route.attachedToShip;

            // Load custom color if present
            QString colorStr = routeObject.value("color").toString("");
            if (!colorStr.isEmpty()) {
                QColor c(colorStr);
                if (c.isValid()) {
                    routeCustomColors[route.routeId] = c;
                    qDebug() << "[ROUTE-LOAD] Route" << route.routeId << "loaded with custom color:" << c.name();
                }
            }

            // Load waypoints with coordinates
            QJsonArray waypointsArray = routeObject["waypoints"].toArray();
            for (const QJsonValue& waypointValue : waypointsArray) {
                QJsonObject waypointObject = waypointValue.toObject();

                EcWidget::RouteWaypoint routeWp;
                routeWp.lat = waypointObject["lat"].toDouble();
                routeWp.lon = waypointObject["lon"].toDouble();
                routeWp.label = waypointObject["label"].toString();
                routeWp.remark = waypointObject["remark"].toString();
                routeWp.turningRadius = waypointObject["turningRadius"].toDouble();
                routeWp.active = waypointObject["active"].toBool();

                route.waypoints.append(routeWp);
            }

            routeList.append(route);
        }

        qDebug() << "[INFO] Loaded" << routeList.size() << "routes from" << filePath;
        qDebug() << "[INFO] waypointList size before conversion:" << waypointList.size();

        // Convert loaded routes to waypoints for display
        convertRoutesToWaypoints();

        qDebug() << "[INFO] waypointList size after conversion:" << waypointList.size();

        file.close();
    }
    else
    {
        qDebug() << "[ERROR] Failed to open routes file:" << filePath;
    }
}

bool EcWidget::renameRoute(int routeId, const QString& newName)
{
    if (routeId <= 0 || newName.trimmed().isEmpty()) return false;

    bool found = false;
    QString baseName = newName.trimmed();
    // Build set of names excluding this routeId
    QSet<QString> existingNames;
    for (const auto &r : routeList) {
        if (r.routeId != routeId) existingNames.insert(r.name.trimmed().toLower());
    }
    // Ensure unique name by appending (n) if needed
    QString finalName = baseName;
    int suffix = 2;
    while (existingNames.contains(finalName.trimmed().toLower())) {
        finalName = QString("%1 (%2)").arg(baseName).arg(suffix++);
    }
    for (auto &route : routeList) {
        if (route.routeId == routeId) {
            route.name = finalName;
            route.modifiedDate = QDateTime::currentDateTime();
            found = true;
            break;
        }
    }

    if (!found) return false;

    // Persist and refresh any labels/overlays using the route name
    saveRoutes();
    updateRouteLabels(routeId);
    Draw();
    return true;
}

bool EcWidget::reverseRoute(int routeId)
{
    qDebug() << "[ROUTE-REVERSE] Reversing route" << routeId;

    if (routeId <= 0) {
        qDebug() << "[ROUTE-REVERSE] Invalid route ID";
        return false;
    }

    // Get waypoints for this route
    QList<Waypoint> routeWaypoints;
    for (const Waypoint& wp : waypointList) {
        if (wp.routeId == routeId) {
            routeWaypoints.append(wp);
        }
    }

    if (routeWaypoints.isEmpty()) {
        qDebug() << "[ROUTE-REVERSE] No waypoints found for route" << routeId;
        return false;
    }

    if (routeWaypoints.size() < 2) {
        qDebug() << "[ROUTE-REVERSE] Route has less than 2 waypoints, nothing to reverse";
        return false;
    }

    qDebug() << "[ROUTE-REVERSE] Found" << routeWaypoints.size() << "waypoints to reverse";

    // Reverse the order of waypoints
    QList<Waypoint> reversedWaypoints;
    for (int i = routeWaypoints.size() - 1; i >= 0; i--) {
        reversedWaypoints.append(routeWaypoints[i]);
    }

    // Update route modified date BEFORE replacing waypoints
    for (auto &route : routeList) {
        if (route.routeId == routeId) {
            route.modifiedDate = QDateTime::currentDateTime();
            break;
        }
    }

    // Replace waypoints for route
    // Note: replaceWaypointsForRoute() already calls saveRoutes() and Draw()
    replaceWaypointsForRoute(routeId, reversedWaypoints);

    qDebug() << "[ROUTE-REVERSE] Route reversed successfully";
    return true;
}

void EcWidget::convertRoutesToWaypoints()
{
    qDebug() << "[INFO] Converting loaded routes to waypoints for display";

    // Add waypoints from loaded routes to waypointList (only if not already present)
    for (const EcWidget::Route& route : routeList) {
        for (const EcWidget::RouteWaypoint& routeWp : route.waypoints) {

            // Check if waypoint already exists in waypointList
            bool exists = false;
            for (const Waypoint& existingWp : waypointList) {
                if (existingWp.routeId == route.routeId &&
                    existingWp.label == routeWp.label &&
                    qAbs(existingWp.lat - routeWp.lat) < 0.00001 &&
                    qAbs(existingWp.lon - routeWp.lon) < 0.00001) {
                    exists = true;
                    break;
                }
            }

            // Only add if doesn't exist
            if (!exists) {
                Waypoint wp;
                wp.lat = routeWp.lat;
                wp.lon = routeWp.lon;
                wp.label = routeWp.label;
                wp.remark = routeWp.remark;
                wp.turningRadius = routeWp.turningRadius;
                wp.active = routeWp.active;
                wp.routeId = route.routeId;
                wp.featureHandle.id = EC_NOCELLID;
                wp.featureHandle.offset = 0;

                waypointList.append(wp);
                qDebug() << "[CONVERT] Added waypoint" << wp.label << "from route" << route.routeId;
            }
        }
    }

    qDebug() << "[INFO] Conversion complete. Total waypoints in list:" << waypointList.size();
}

void EcWidget::updateRouteList(int routeId)
{
    // Find or create route in routeList
    EcWidget::Route* targetRoute = nullptr;

    for (auto& route : routeList) {
        if (route.routeId == routeId) {
            targetRoute = &route;
            break;
        }
    }

    if (!targetRoute) {
        // Create new route
        EcWidget::Route newRoute;
        newRoute.routeId = routeId;
        newRoute.name = QString("Route %1").arg(routeId);
        newRoute.description = "";
        newRoute.createdDate = QDateTime::currentDateTime();
        newRoute.modifiedDate = QDateTime::currentDateTime();
        routeList.append(newRoute);
        targetRoute = &routeList.last();
    }

    // Clear existing waypoints and rebuild from waypointList
    targetRoute->waypoints.clear();
    targetRoute->modifiedDate = QDateTime::currentDateTime();

    for (const Waypoint& wp : waypointList) {
        if (wp.routeId == routeId) {
            EcWidget::RouteWaypoint routeWp;
            routeWp.lat = wp.lat;
            routeWp.lon = wp.lon;
            routeWp.label = wp.label;
            routeWp.remark = wp.remark;
            routeWp.turningRadius = wp.turningRadius;
            routeWp.active = wp.active;

            targetRoute->waypoints.append(routeWp);
        }
    }

    // Calculate route data
    calculateRouteData(*targetRoute);

    // If after rebuild there are no waypoints, remove this route to avoid empty routes lingering
    if (targetRoute->waypoints.isEmpty()) {
        // Remove from routeList and ancillary maps
        QList<Route> cleaned;
        for (const auto &r : routeList) {
            if (r.routeId != routeId) cleaned.append(r);
        }
        routeList = cleaned;
        routeVisibility.remove(routeId);
        routeCustomColors.remove(routeId);
        qDebug() << "[INFO] Removed empty route" << routeId << "from routeList during updateRouteList";
        return;
    }

    qDebug() << "[INFO] Updated route" << routeId << "in routeList with" << targetRoute->waypoints.size() << "waypoints";
}

QString EcWidget::getRouteFilePath() const
{
    // Simpan di direktori data aplikasi yang sama dengan waypoints
    QString basePath;

#ifdef _WIN32
    if (EcKernelGetEnv("APPDATA"))
        basePath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC";
#else
    if (EcKernelGetEnv("HOME"))
        basePath = QString(EcKernelGetEnv("HOME")) + "/SevenCs/EC2007/DENC";
#endif

    // Jika base path tidak tersedia, gunakan direktori saat ini
    if (basePath.isEmpty())
        return "routes.json";
    else
        return basePath + "/routes.json";
}

QString EcWidget::getRouteLibraryFilePath() const
{
    QString basePath;
#ifdef _WIN32
    if (EcKernelGetEnv("APPDATA"))
        basePath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC";
#else
    if (EcKernelGetEnv("HOME"))
        basePath = QString(EcKernelGetEnv("HOME")) + "/SevenCs/EC2007/DENC";
#endif
    if (basePath.isEmpty())
        return "routes_library.json";
    else
        return basePath + "/routes_library.json";
}

QStringList EcWidget::listSavedRouteNames() const
{
    QString filePath = getRouteLibraryFilePath();
    QFile file(filePath);
    QStringList names;
    if (!file.exists()) return names;
    if (!file.open(QIODevice::ReadOnly)) return names;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    QJsonArray arr = doc.object().value("saved_routes").toArray();
    for (const auto& v : arr) {
        QJsonObject o = v.toObject();
        QString name = o.value("name").toString();
        if (!name.isEmpty()) names << name;
    }
    return names;
}

bool EcWidget::saveRouteToLibrary(int routeId)
{
    if (routeId <= 0) return false;
    // Get a fresh copy of the route with current waypoints
    Route routeCopy = getRouteById(routeId);
    if (routeCopy.waypoints.isEmpty()) return false;
    if (routeCopy.name.trimmed().isEmpty()) {
        routeCopy.name = QString("Route %1").arg(routeId);
    }

    // Build JSON object for this route
    QJsonObject routeObj;
    routeObj["name"] = routeCopy.name;
    routeObj["description"] = routeCopy.description;
    // Persist custom color if present
    if (routeCustomColors.contains(routeId)) {
        routeObj["color"] = routeCustomColors.value(routeId).name(QColor::HexRgb);
    }
    // Waypoints
    QJsonArray wps;
    for (const auto &wp : routeCopy.waypoints) {
        QJsonObject w;
        w["lat"] = wp.lat;
        w["lon"] = wp.lon;
        w["label"] = wp.label;
        w["remark"] = wp.remark;
        w["turningRadius"] = wp.turningRadius;
        w["active"] = wp.active;
        wps.append(w);
    }
    routeObj["waypoints"] = wps;

    // Read library, replace by name, write back
    QString filePath = getRouteLibraryFilePath();
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) dir.mkpath(".");
    QJsonArray saved;
    {
        QFile f(filePath);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            QJsonDocument d = QJsonDocument::fromJson(f.readAll());
            f.close();
            saved = d.object().value("saved_routes").toArray();
        }
    }
    // Remove existing with same name
    QJsonArray newSaved;
    for (const auto &v : saved) {
        QJsonObject o = v.toObject();
        if (o.value("name").toString() != routeCopy.name) newSaved.append(o);
    }
    newSaved.append(routeObj);
    QJsonObject root; root["saved_routes"] = newSaved;
    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    out.close();
    qDebug() << "[ROUTE-LIB] Saved route to library:" << routeCopy.name << "at" << filePath;
    return true;
}

bool EcWidget::saveRouteToLibraryAs(int routeId, const QString& name)
{
    if (routeId <= 0) return false;
    Route routeCopy = getRouteById(routeId);
    if (routeCopy.waypoints.isEmpty()) return false;

    QString saveName = name.trimmed();
    if (saveName.isEmpty()) {
        saveName = routeCopy.name.trimmed();
        if (saveName.isEmpty()) saveName = QString("Route %1").arg(routeId);
    }

    // Build JSON object for this route (same as saveRouteToLibrary but override name)
    QJsonObject routeObj;
    routeObj["name"] = saveName;
    routeObj["description"] = routeCopy.description;
    if (routeCustomColors.contains(routeId)) {
        routeObj["color"] = routeCustomColors.value(routeId).name(QColor::HexRgb);
    }
    QJsonArray wps;
    for (const auto &wp : routeCopy.waypoints) {
        QJsonObject w;
        w["lat"] = wp.lat;
        w["lon"] = wp.lon;
        w["label"] = wp.label;
        w["remark"] = wp.remark;
        w["turningRadius"] = wp.turningRadius;
        w["active"] = wp.active;
        wps.append(w);
    }
    routeObj["waypoints"] = wps;

    // Read library and write back (replace same name)
    QString filePath = getRouteLibraryFilePath();
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) dir.mkpath(".");
    QJsonArray saved;
    {
        QFile f(filePath);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            QJsonDocument d = QJsonDocument::fromJson(f.readAll());
            f.close();
            saved = d.object().value("saved_routes").toArray();
        }
    }
    QJsonArray newSaved;
    for (const auto &v : saved) {
        QJsonObject o = v.toObject();
        if (o.value("name").toString() != saveName) newSaved.append(o);
    }
    newSaved.append(routeObj);
    QJsonObject root; root["saved_routes"] = newSaved;
    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    out.close();
    qDebug() << "[ROUTE-LIB] Saved AS to library:" << saveName << "(from routeId" << routeId << ") at" << filePath;
    return true;
}

bool EcWidget::loadRouteFromLibrary(const QString& routeName)
{
    if (routeName.trimmed().isEmpty()) return false;
    QString filePath = getRouteLibraryFilePath();
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    QJsonArray arr = doc.object().value("saved_routes").toArray();
    QJsonObject found;
    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        if (o.value("name").toString() == routeName) { found = o; break; }
    }
    if (found.isEmpty()) return false;

    // Create new route with next ID
    int newRouteId = getNextAvailableRouteId();
    QJsonArray wps = found.value("waypoints").toArray();
    for (const auto &vw : wps) {
        QJsonObject w = vw.toObject();
        double lat = w.value("lat").toDouble();
        double lon = w.value("lon").toDouble();
        QString label = w.value("label").toString();
        QString remark = w.value("remark").toString();
        double turningRadius = w.value("turningRadius").toDouble(10.0);
        bool active = w.value("active").toBool(true);
        createWaypointFromForm(lat, lon, label, remark, newRouteId, turningRadius, active);
    }
    // Set name and color
    renameRoute(newRouteId, found.value("name").toString());
    QString colorStr = found.value("color").toString("");
    if (!colorStr.isEmpty()) {
        QColor c(colorStr);
        if (c.isValid()) setRouteCustomColor(newRouteId, c);
    }
    // Ensure visible
    setRouteVisibility(newRouteId, true);
    saveRoutes();
    emit waypointCreated();
    Draw();
    qDebug() << "[ROUTE-LIB] Loaded route from library:" << routeName << "as new routeId" << newRouteId;
    return true;
}

void EcWidget::saveCurrentRoute()
{
    if (currentRouteId <= 0) return;

    // Check if route already exists in the list
    bool routeExists = false;
    for (int i = 0; i < routeList.size(); ++i) {
        if (routeList[i].routeId == currentRouteId) {
            // Update existing route
            routeList[i].modifiedDate = QDateTime::currentDateTime();
            // Preserve existing custom name; only set a default if empty, ensure unique
            if (routeList[i].name.trimmed().isEmpty()) {
                QString base = QString("Route %1").arg(currentRouteId);
                QSet<QString> existing;
                for (const auto &r : routeList) if (r.routeId != currentRouteId) existing.insert(r.name.trimmed().toLower());
                QString unique = base; int n=2;
                while (existing.contains(unique.trimmed().toLower())) unique = QString("%1 (%2)").arg(base).arg(n++);
                routeList[i].name = unique;
            }
            // Ensure no stale custom color for new/updated route unless explicitly set
            routeCustomColors.remove(currentRouteId);

            // Recalculate route data
            calculateRouteData(routeList[i]);
            routeExists = true;
            break;
        }
    }

    if (!routeExists) {
        // Create new route
        EcWidget::Route newRoute;
        newRoute.routeId = currentRouteId;
        // Default name ensured unique
        QString base = QString("Route %1").arg(currentRouteId);
        QSet<QString> existing;
        for (const auto &r : routeList) existing.insert(r.name.trimmed().toLower());
        QString unique = base; int n=2;
        while (existing.contains(unique.trimmed().toLower())) unique = QString("%1 (%2)").arg(base).arg(n++);
        newRoute.name = unique;
        // Clear any stale custom color (safety)
        routeCustomColors.remove(currentRouteId);
        newRoute.description = QString("Route created on %1").arg(QDateTime::currentDateTime().toString());

        // Calculate route data
        calculateRouteData(newRoute);

        routeList.append(newRoute);
        //qDebug() << "[ROUTE] Created new route" << currentRouteId << "with" << newRoute.waypoints.size() << "waypoints";
    }

    // Save to file
    saveRoutes();
}

EcWidget::Route EcWidget::getRouteById(int routeId) const
{
    for (const EcWidget::Route& route : routeList) {
        if (route.routeId == routeId) {
            // Return COPY of route with current waypoint data synced
            EcWidget::Route routeCopy = route;
            routeCopy.waypoints.clear();

            for (const Waypoint& wp : waypointList) {
                if (wp.routeId == routeId) {
                    EcWidget::RouteWaypoint routeWp;
                    routeWp.lat = wp.lat;
                    routeWp.lon = wp.lon;
                    routeWp.label = wp.label; // PRESERVE custom names from waypointList
                    routeWp.remark = wp.remark;
                    routeWp.turningRadius = wp.turningRadius;
                    routeWp.active = wp.active;
                    routeCopy.waypoints.append(routeWp);
                }
            }
            // Recalculate aggregate data so export reflects the current map
            // This uses the latest coordinates from waypointList
            const_cast<EcWidget*>(this)->calculateRouteData(routeCopy);
            qDebug() << "[GET-ROUTE] Created route copy for" << routeId << "with" << routeCopy.waypoints.size() << "waypoints - ORIGINAL ROUTE UNCHANGED";
            return routeCopy;
        }
    }
    return EcWidget::Route(); // Return empty route if not found
}

void EcWidget::calculateRouteData(EcWidget::Route& route)
{
    route.waypoints.clear();
    route.totalDistance = 0.0;

    // Find all waypoints belonging to this route and copy their data
    for (const Waypoint& wp : waypointList) {
        if (wp.routeId == route.routeId) {
            route.waypoints.append(EcWidget::RouteWaypoint(wp));
        }
    }

    // Calculate total distance using waypoint coordinates
    if (route.waypoints.size() >= 2) {
        for (int i = 0; i < route.waypoints.size() - 1; ++i) {
            const EcWidget::RouteWaypoint& wp1 = route.waypoints[i];
            const EcWidget::RouteWaypoint& wp2 = route.waypoints[i + 1];

            // Calculate distance using Haversine formula
            double lat1 = qDegreesToRadians(wp1.lat);
            double lon1 = qDegreesToRadians(wp1.lon);
            double lat2 = qDegreesToRadians(wp2.lat);
            double lon2 = qDegreesToRadians(wp2.lon);

            double dlat = lat2 - lat1;
            double dlon = lon2 - lon1;

            double a = qSin(dlat/2) * qSin(dlat/2) + qCos(lat1) * qCos(lat2) * qSin(dlon/2) * qSin(dlon/2);
            double c = 2 * qAtan2(qSqrt(a), qSqrt(1-a));
            double distance = 6371.0 * c; // Earth radius in km

            route.totalDistance += distance * 0.539957; // Convert km to nautical miles
        }
    }

    // Calculate estimated time (assume 10 knots speed)
    route.estimatedTime = route.totalDistance / 10.0;
}

void EcWidget::insertWaypointAt(EcCoordinate lat, EcCoordinate lon)
{
    qDebug() << "[INSERT] Inserting waypoint at" << lat << lon;

    if (waypointList.size() < 2) {
        qDebug() << "[INSERT] Need at least 2 waypoints to insert between";
        return;
    }

    // Find the closest route segment to insert the waypoint
    double minDistance = std::numeric_limits<double>::max();
    int insertIndex = -1;
    int targetRouteId = -1;

    // Group waypoints by route ID
    QMap<int, QList<int>> routeWaypoints;
    for (int i = 0; i < waypointList.size(); ++i) {
        if (waypointList[i].routeId > 0) {
            routeWaypoints[waypointList[i].routeId].append(i);
        }
    }

    // For each route, check segments
    for (auto it = routeWaypoints.begin(); it != routeWaypoints.end(); ++it) {
        int routeId = it.key();
        QList<int> indices = it.value();

        if (indices.size() < 2) continue;

        // Check each segment in this route
        for (int i = 0; i < indices.size() - 1; ++i) {
            int idx1 = indices[i];
            int idx2 = indices[i + 1];

            const Waypoint& wp1 = waypointList[idx1];
            const Waypoint& wp2 = waypointList[idx2];

            // Calculate distance from point to line segment
            double distance = distanceToLineSegment(lat, lon, wp1.lat, wp1.lon, wp2.lat, wp2.lon);

            if (distance < minDistance) {
                minDistance = distance;
                insertIndex = idx2; // Insert before idx2
                targetRouteId = routeId;
            }
        }
    }

    if (insertIndex == -1 || targetRouteId == -1) {
        qDebug() << "[INSERT] No suitable insertion point found";
        return;
    }

    // Create new waypoint
    Waypoint newWaypoint;
    newWaypoint.lat = lat;
    newWaypoint.lon = lon;
    newWaypoint.routeId = targetRouteId;
    newWaypoint.remark = "Inserted waypoint";
    newWaypoint.turningRadius = 10.0;
    newWaypoint.active = true;
    newWaypoint.featureHandle.id = EC_NOCELLID;
    newWaypoint.featureHandle.offset = 0;

    // Generate appropriate label
    QList<int> routeIndices;
    for (int i = 0; i < waypointList.size(); ++i) {
        if (waypointList[i].routeId == targetRouteId) {
            routeIndices.append(i);
        }
    }

    int waypointNumber = routeIndices.size() + 1;
    newWaypoint.label = QString("R%1-WP%2").arg(targetRouteId).arg(waypointNumber, 3, 10, QChar('0'));

    // Insert waypoint at the correct position
    waypointList.insert(insertIndex, newWaypoint);

    // DON'T UPDATE LABELS - preserve custom waypoint names set by user
    qDebug() << "[INSERT] Skipping label update to preserve custom waypoint names";

    qDebug() << "[INSERT] Inserted waypoint" << newWaypoint.label << "into route" << targetRouteId;

    // Persist: single-waypoints file (route waypoints are skipped inside)
    saveWaypoints();

    // Rebuild and persist the affected route from waypointList (avoid saveCurrentRoute which can create empty routes)
    updateRouteList(targetRouteId);
    saveRoutes();

    // Redraw and notify
    Draw();
    update();
}

double EcWidget::distanceToLineSegment(double px, double py, double x1, double y1, double x2, double y2)
{
    // Calculate distance from point (px, py) to line segment (x1,y1)-(x2,y2)
    double A = px - x1;
    double B = py - y1;
    double C = x2 - x1;
    double D = y2 - y1;

    double dot = A * C + B * D;
    double len_sq = C * C + D * D;

    double param = -1;
    if (len_sq != 0) {
        param = dot / len_sq;
    }

    double xx, yy;
    if (param < 0) {
        xx = x1;
        yy = y1;
    } else if (param > 1) {
        xx = x2;
        yy = y2;
    } else {
        xx = x1 + param * C;
        yy = y1 + param * D;
    }

    double dx = px - xx;
    double dy = py - yy;
    return sqrt(dx * dx + dy * dy);
}

void EcWidget::updateRouteLabels(int routeId)
{
    // Update labels for all waypoints in the specified route
    QList<int> routeIndices;
    for (int i = 0; i < waypointList.size(); ++i) {
        if (waypointList[i].routeId == routeId) {
            routeIndices.append(i);
        }
    }

    // Renumber waypoints in sequence
    for (int i = 0; i < routeIndices.size(); ++i) {
        int index = routeIndices[i];
        waypointList[index].label = QString("R%1-WP%2").arg(routeId).arg(i + 1, 3, 10, QChar('0'));
    }

    qDebug() << "[UPDATE-LABELS] Updated" << routeIndices.size() << "labels for route" << routeId;
}

void EcWidget::convertSingleWaypointsToRoutes()
{
    qDebug() << "[ROUTE-CONVERSION] Starting conversion of single waypoints to routes";

    int singleWaypointCount = 0;
    int maxRouteId = 0;

    // Find maximum existing route ID and count single waypoints
    for (const Waypoint& wp : waypointList) {
        if (wp.routeId == 0) {
            singleWaypointCount++;
        } else if (wp.routeId > maxRouteId) {
            maxRouteId = wp.routeId;
        }
    }

    if (singleWaypointCount == 0) {
        qDebug() << "[ROUTE-CONVERSION] No single waypoints found to convert";
        return;
    }

    // Create a special route for single waypoints
    int singleWaypointsRouteId = maxRouteId + 1;

    // Convert single waypoints (routeId = 0) to a special route
    for (Waypoint& wp : waypointList) {
        if (wp.routeId == 0) {
            wp.routeId = singleWaypointsRouteId;
            qDebug() << "[ROUTE-CONVERSION] Converted waypoint" << wp.label << "to route" << singleWaypointsRouteId;
        }
    }

    // Create route entry for converted waypoints
    if (singleWaypointCount > 0) {
        EcWidget::Route convertedRoute;
        convertedRoute.routeId = singleWaypointsRouteId;
        convertedRoute.name = QString("Legacy Waypoints (Route %1)").arg(singleWaypointsRouteId);
        convertedRoute.description = QString("Converted from %1 single waypoints for compatibility").arg(singleWaypointCount);

        // Calculate route data
        calculateRouteData(convertedRoute);

        routeList.append(convertedRoute);

        qDebug() << "[ROUTE-CONVERSION] Created route" << singleWaypointsRouteId
                 << "for" << singleWaypointCount << "legacy waypoints";

        // Save both waypoints and routes
        saveWaypoints();
        saveRoutes();

        qDebug() << "[ROUTE-CONVERSION] Conversion completed successfully";
    }
}

// Waypoint Form Functions
void EcWidget::showAddWaypointDialog()
{
    WaypointDialog dialog(this);

    // Set default route based on current route mode
    if (isRouteMode) {
        dialog.setRouteId(currentRouteId);
    }

    if (dialog.exec() == QDialog::Accepted) {
        double lat = dialog.getLatitude();
        double lon = dialog.getLongitude();
        QString label = dialog.getLabel();
        QString remark = dialog.getRemark();
        int routeId = dialog.getRouteId();
        double turningRadius = dialog.getTurningRadius();

        createWaypointFromForm(lat, lon, label, remark, routeId, turningRadius);
    }
}

void EcWidget::showCreateRouteDialog()
{
    RouteFormDialog dialog(this, this);

    if (dialog.exec() == QDialog::Accepted) {
        QString routeName = dialog.getRouteName();
        QString routeDescription = dialog.getRouteDescription();
        int routeId = dialog.getRouteId();
        QList<RouteWaypointData> waypoints = dialog.getWaypoints();

        // Create route with all waypoints
        if (!waypoints.isEmpty()) {
            // Set route mode and current route ID
            isRouteMode = true;
            currentRouteId = routeId;

            // Create route object for route management
            Route newRoute;
            newRoute.routeId = routeId;
            newRoute.name = routeName;
            newRoute.description = routeDescription;
            newRoute.createdDate = QDateTime::currentDateTime();
            newRoute.modifiedDate = newRoute.createdDate;

            // Convert waypoints to RouteWaypoint format
            for (const RouteWaypointData& wp : waypoints) {
                RouteWaypoint routeWp;
                routeWp.lat = wp.lat;
                routeWp.lon = wp.lon;
                routeWp.label = wp.label;
                routeWp.remark = wp.remark;
                routeWp.turningRadius = wp.turningRadius;
                routeWp.active = wp.active;
                newRoute.waypoints.append(routeWp);
            }

            // Calculate route data (distance, time)
            calculateRouteData(newRoute);

            // Add route to routeList
            routeList.append(newRoute);

            qDebug() << "[ROUTE-CREATE] Creating" << waypoints.size() << "waypoints for route" << routeId;

            // Create all waypoints for this route in the chart FIRST
            for (const RouteWaypointData& wp : waypoints) {
                qDebug() << "[ROUTE-CREATE] Creating waypoint" << wp.label << "at" << wp.lat << "," << wp.lon << "active:" << wp.active;
                createWaypointFromForm(wp.lat, wp.lon, wp.label, wp.remark, routeId, wp.turningRadius, wp.active);
            }

            qDebug() << "[ROUTE-CREATE] After waypoint creation, waypointList size:" << waypointList.size();

            // INITIAL VISIBILITY: Set new routes visible by default (one time only)
            qDebug() << "[ROUTE-CREATE] Setting initial visibility for new route" << routeId;
            setRouteVisibility(routeId, true); // Initial visibility only
            qDebug() << "[ROUTE-CREATE] New route" << routeId << "initial visibility set to visible";

            // Save routes to file
            saveRoutes();

            qDebug() << "[ROUTE-CREATE] Route" << routeId << "saved with initial visibility:" << isRouteVisible(routeId);

            // Force chart redraw to show the new route immediately
            Draw();
            update();

            // Emit signal to notify route panel about new route/waypoints
            emit waypointCreated();

            // Log route creation
            if (mainWindow && mainWindow->logText) {
                QString logMessage = QString("Route '%1' created with %2 waypoints (ID: %3) - Distance: %.2f NM")
                    .arg(routeName)
                    .arg(waypoints.size())
                    .arg(routeId)
                    .arg(newRoute.totalDistance);
                mainWindow->logText->append(logMessage);
            }
        }
    }
}

void EcWidget::showCreateRouteQuickDialog()
{
    RouteQuickFormDialog dialog(this, this);
    if (dialog.exec() == QDialog::Accepted) {
        QList<RouteWaypointData> waypoints = dialog.getWaypoints();
        if (waypoints.isEmpty()) return;

        // Generate next route id and default name
        int maxId = 0;
        for (const auto& r : routeList) {
            if (r.routeId > maxId) maxId = r.routeId;
        }
        int routeId = maxId + 1;
        QString routeName = dialog.getRouteName().trimmed();
        if (routeName.isEmpty()) {
            routeName = QString("Route %1").arg(routeId, 3, 10, QChar('0'));
        }

        // Enable route mode and set current route id
        isRouteMode = true;
        currentRouteId = routeId;

        // Build route object
        Route newRoute;
        newRoute.routeId = routeId;
        newRoute.name = routeName;
        newRoute.description = QString();
        newRoute.createdDate = QDateTime::currentDateTime();
        newRoute.modifiedDate = newRoute.createdDate;

        for (const RouteWaypointData& wp : waypoints) {
            RouteWaypoint rw;
            rw.lat = wp.lat;
            rw.lon = wp.lon;
            rw.label = wp.label;
            rw.remark = wp.remark;
            rw.turningRadius = wp.turningRadius;
            rw.active = wp.active;
            newRoute.waypoints.append(rw);
        }

        // Calculate metadata and append
        calculateRouteData(newRoute);
        routeList.append(newRoute);

        // Create on-chart waypoints
        for (const RouteWaypointData& wp : waypoints) {
            createWaypointFromForm(wp.lat, wp.lon, wp.label, wp.remark, routeId, wp.turningRadius, wp.active);
        }

        // Make route visible initially
        setRouteVisibility(routeId, true);
        saveRoutes();
        Draw();
        update();
        emit waypointCreated();
    }
}

void EcWidget::showEditRouteDialog(int routeId)
{
    RouteFormDialog dialog(this, this);

    // Load existing route data
    dialog.loadRouteData(routeId);

    if (dialog.exec() == QDialog::Accepted) {
        QString routeName = dialog.getRouteName();
        QString routeDescription = dialog.getRouteDescription();
        QList<RouteWaypointData> waypoints = dialog.getWaypoints();

        // Update route with modified waypoints
        if (!waypoints.isEmpty()) {
            qDebug() << "[ROUTE-EDIT] Starting route update for route" << routeId << "with" << waypoints.size() << "waypoints";
            qDebug() << "[ROUTE-EDIT] Current routeList size:" << routeList.size();

            // FIRST: Preserve route visibility IMMEDIATELY to prevent race conditions
            bool wasVisible = isRouteVisible(routeId);
            qDebug() << "[ROUTE-EDIT] Route" << routeId << "was visible:" << wasVisible << "before edit - preserving immediately";

            // Find existing route in routeList
            bool routeFound = false;
            for (int i = 0; i < routeList.size(); ++i) {
                qDebug() << "[ROUTE-EDIT] Checking routeList[" << i << "] with routeId:" << routeList[i].routeId;
                if (routeList[i].routeId == routeId) {
                    qDebug() << "[ROUTE-EDIT] Updating route in routeList, old name:" << routeList[i].name << "new name:" << routeName;

                    // Update route information
                    routeList[i].name = routeName;
                    routeList[i].description = routeDescription;
                    routeList[i].modifiedDate = QDateTime::currentDateTime();

                    // Clear existing waypoints and add new ones
                    qDebug() << "[ROUTE-EDIT] Clearing" << routeList[i].waypoints.size() << "existing waypoints, adding" << waypoints.size() << "new ones";
                    routeList[i].waypoints.clear();
                    for (const RouteWaypointData& wp : waypoints) {
                        RouteWaypoint routeWp;
                        routeWp.lat = wp.lat;
                        routeWp.lon = wp.lon;
                        routeWp.label = wp.label;
                        routeWp.remark = wp.remark;
                        routeWp.turningRadius = wp.turningRadius;
                        routeWp.active = wp.active;
                        routeList[i].waypoints.append(routeWp);
                        qDebug() << "[ROUTE-EDIT] Added waypoint" << wp.label << "at" << wp.lat << "," << wp.lon;
                    }

                    // Recalculate route data
                    calculateRouteData(routeList[i]);

                    qDebug() << "[ROUTE-EDIT] Route" << routeId << "updated successfully in routeList, new name:" << routeList[i].name;
                    routeFound = true;
                    break;
                }
            }

            if (!routeFound) {
                qDebug() << "[ROUTE-EDIT-ERROR] Route" << routeId << "NOT found in routeList! This should not happen.";
            }

            // Remove existing waypoints for this route from chart
            int removedCount = 0;
            waypointList.erase(std::remove_if(waypointList.begin(), waypointList.end(),
                [routeId, &removedCount](const Waypoint& wp) {
                    if (wp.routeId == routeId) {
                        removedCount++;
                        return true;
                    }
                    return false;
                }),
                waypointList.end());
            qDebug() << "[ROUTE-EDIT] Removed" << removedCount << "waypoints for route" << routeId << "from waypointList";

            // Create updated waypoints in the chart
            currentRouteId = routeId;
            for (const RouteWaypointData& wp : waypoints) {
                createWaypointFromForm(wp.lat, wp.lon, wp.label, wp.remark, routeId, wp.turningRadius, wp.active);
            }

            // PRESERVE EXISTING VISIBILITY - DO NOT MODIFY
            qDebug() << "[ROUTE-EDIT] Route" << routeId << "preserving existing visibility:" << wasVisible;

            // Save routes to file BEFORE emitting signals
            saveRoutes();

            // Emit signal to notify route panel about route update
            emit waypointCreated();

            qDebug() << "[ROUTE-EDIT] Route" << routeId << "edited without changing visibility";

            // Ensure chart is refreshed
            update();

            // Log route update
            if (mainWindow && mainWindow->logText) {
                QString logMessage = QString("Route '%1' updated with %2 waypoints (ID: %3)")
                    .arg(routeName)
                    .arg(waypoints.size())
                    .arg(routeId);
                mainWindow->logText->append(logMessage);
            }

            // Refresh display
            update();
        }
    }
}

void EcWidget::createWaypointFromForm(double lat, double lon, const QString& label, const QString& remark, int routeId, double turningRadius, bool active)
{
    qDebug() << "[CREATE-WAYPOINT] Creating waypoint" << label << "at" << lat << "," << lon << "for route" << routeId;

    Waypoint newWaypoint;

    // Set coordinates
    newWaypoint.lat = lat;
    newWaypoint.lon = lon;

    // Set turning radius and active status
    newWaypoint.turningRadius = turningRadius;
    newWaypoint.active = active;

    // Generate label if empty
    if (label.isEmpty()) {
        if (routeId > 0) {
            // Count waypoints in this route
            int waypointCount = 0;
            for (const auto& wp : waypointList) {
                if (wp.routeId == routeId) {
                    waypointCount++;
                }
            }
            newWaypoint.label = QString("R%1-WP%2").arg(routeId).arg(waypointCount + 1, 3, 10, QChar('0'));
        } else {
            newWaypoint.label = QString("WP%1").arg(waypointList.size() + 1, 3, 10, QChar('0'));
        }
    } else {
        newWaypoint.label = label;
    }

    // Set remark
    if (remark.isEmpty()) {
        if (routeId > 0) {
            newWaypoint.remark = QString("Route %1 waypoint").arg(routeId);
        } else {
            newWaypoint.remark = "Single waypoint";
        }
    } else {
        newWaypoint.remark = remark;
    }

    // Set route ID
    newWaypoint.routeId = routeId;

    // Initialize feature handle
    newWaypoint.featureHandle.id = EC_NOCELLID;
    newWaypoint.featureHandle.offset = 0;

    // Add to waypoint list
    waypointList.append(newWaypoint);

    // Create waypoint cell if needed
    if (routeId == 0) {
        // Single waypoint - create cell
        createSingleWaypoint(newWaypoint);
    } else {
        // Route waypoint - no cell creation needed
        qDebug() << "[INFO] Route waypoint created:" << newWaypoint.label;
    }

    // Save waypoints
    saveWaypoints();

    // Update current route if needed (persist from waypointList to routes.json)
    if (routeId > 0) {
        updateRouteList(routeId);
        saveRoutes();
    }

    // Redraw
    Draw();
    update();

    qDebug() << "[INFO] Waypoint created from form - Label:" << newWaypoint.label
             << "Lat:" << lat << "Lon:" << lon << "Route:" << routeId;
}

// Waypoint highlighting methods for route panel visualization
void EcWidget::highlightWaypoint(int routeId, int waypointIndex)
{
    // Clear previous highlight
    clearWaypointHighlight();
    // Find the waypoint to highlight
    int currentIndex = 0;
    for (const auto& wp : waypointList) {
        if (wp.routeId == routeId) {
            if (currentIndex == waypointIndex) {
                // Set highlight data
                highlightedWaypoint.visible = true;
                highlightedWaypoint.lat = wp.lat;
                highlightedWaypoint.lon = wp.lon;
                highlightedWaypoint.label = wp.label;
                highlightedWaypoint.routeId = routeId;
                highlightedWaypoint.waypointIndex = waypointIndex;

                qDebug() << "[HIGHLIGHT] Highlighting waypoint" << wp.label
                         << "at" << wp.lat << "," << wp.lon << "route" << routeId << "index" << waypointIndex;

                // Start animation timer for consistent pulsing
                if (!waypointAnimationTimer->isActive()) {
                    waypointAnimationTimer->start();
                }

                // Trigger map update
                update();
                return;
            }
            currentIndex++;
        }
    }

    qDebug() << "[HIGHLIGHT] Waypoint not found - route:" << routeId << "index:" << waypointIndex;
}

void EcWidget::clearWaypointHighlight()
{
    if (highlightedWaypoint.visible) {
        highlightedWaypoint.visible = false;

        // Stop animation timer to save resources
        if (waypointAnimationTimer->isActive()) {
            waypointAnimationTimer->stop();
        }

        qDebug() << "[HIGHLIGHT] Cleared waypoint highlight";
        update();
    }
}
void EcWidget::updateAoiHoverLabel(const QPoint& mousePos)
{
    hoverAoiId = -1;
    hoverAoiEdgeIndex = -1;
    hoverAoiLabelText.clear();

    if (attachedAoiId < 0) { update(); return; }

    // Find attached AOI
    const AOI* target = nullptr;
    for (const auto& a : aoiList) { if (a.id == attachedAoiId) { target = &a; break; } }
    if (!target) { update(); return; }
    if (!target->visible || target->vertices.size() < 2) { update(); return; }

    // Project vertices; keep mapping to original indices
    QVector<QPoint> pts;
    QVector<int> idxMap;
    pts.reserve(target->vertices.size());
    idxMap.reserve(target->vertices.size());
    for (int i = 0; i < target->vertices.size(); ++i) {
        const QPointF& ll = target->vertices[i];
        if (!qIsFinite(ll.x()) || !qIsFinite(ll.y())) continue;
        int x=0, y=0;
        if (LatLonToXy(ll.x(), ll.y(), x, y)) { pts.append(QPoint(x, y)); idxMap.append(i); }
    }
    if (pts.size() < 2) { update(); return; }

    // Find nearest segment to mouse in screen space
    auto distPointToSeg2 = [](const QPoint& p, const QPoint& a, const QPoint& b) -> double {
        double px = p.x(), py = p.y();
        double x1 = a.x(), y1 = a.y();
        double x2 = b.x(), y2 = b.y();
        double vx = x2 - x1, vy = y2 - y1;
        double wx = px - x1, wy = py - y1;
        double c1 = vx*wx + vy*wy;
        if (c1 <= 0) return (px-x1)*(px-x1) + (py-y1)*(py-y1);
        double c2 = vx*vx + vy*vy;
        if (c2 <= 0) return (px-x1)*(px-x1) + (py-y1)*(py-y1);
        double t = c1 / c2; if (t < 0) t = 0; else if (t > 1) t = 1;
        double projx = x1 + t*vx, projy = y1 + t*vy;
        double dx = px - projx, dy = py - projy;
        return dx*dx + dy*dy;
    };

    int bestEdge = -1; double bestD2 = 1e18; QPoint bestMid;
    for (int i = 0; i < pts.size(); ++i) {
        const QPoint& a = pts[i];
        const QPoint& b = pts[(i+1) % pts.size()];
        double d2 = distPointToSeg2(mousePos, a, b);
        if (d2 < bestD2) { bestD2 = d2; bestEdge = i; bestMid = QPoint((a.x()+b.x())/2, (a.y()+b.y())/2); }
    }

    // Threshold: only show if cursor is close enough (~20 px)
    if (bestEdge < 0 || bestD2 > 20.0*20.0) { update(); return; }

    // Compute length of that edge using haversine on original vertices
    int idx1 = idxMap[bestEdge];
    int idx2 = idxMap[(bestEdge+1) % idxMap.size()];
    if (idx1 < 0 || idx2 < 0 || idx1 >= target->vertices.size() || idx2 >= target->vertices.size()) { update(); return; }
    double lat1 = target->vertices[idx1].x();
    double lon1 = target->vertices[idx1].y();
    double lat2 = target->vertices[idx2].x();
    double lon2 = target->vertices[idx2].y();
    if (!qIsFinite(lat1) || !qIsFinite(lon1) || !qIsFinite(lat2) || !qIsFinite(lon2)) { update(); return; }
    double meters = haversine(lat1, lon1, lat2, lon2);
    double distNM = meters / 1852.0;

    hoverAoiId = attachedAoiId;
    hoverAoiEdgeIndex = bestEdge;
    hoverAoiLabelScreenPos = QPoint(bestMid.x(), bestMid.y() - 4);
    hoverAoiLabelText = QString::number(distNM, 'f', 2) + " NM";
    update();
}

void EcWidget::drawShipDot(QPainter& painter)
{
    if (!shipDotEnabled) return;
    // Prefer navShip live position
    double lat = 0.0, lon = 0.0;
    if (navShip.lat != 0.0 && navShip.lon != 0.0) { lat = navShip.lat; lon = navShip.lon; }
    else if (ownShip.lat != 0.0 && ownShip.lon != 0.0) { lat = ownShip.lat; lon = ownShip.lon; }
    else return;

    int x=0, y=0; if (!LatLonToXy(lat, lon, x, y)) return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(Qt::black); pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(QColor(255,0,0));
    int r = 6;
    painter.drawEllipse(QPoint(x,y), r, r);
    painter.restore();
}

void EcWidget::updateAttachedGuardZoneFromNavShip()
{
    if (!(navShip.lat != 0.0 && navShip.lon != 0.0)) return;
    bool changed = false;
    for (auto &gz : guardZones) {
        if (gz.attachedToShip) {
            gz.centerLat = navShip.lat;
            gz.centerLon = navShip.lon;
            double currentHeading = navShip.heading;
            if (qIsNaN(currentHeading) || qIsInf(currentHeading)) {
                currentHeading = ownShip.heading;
            }
            if (!qIsNaN(currentHeading) && !qIsInf(currentHeading)) {
                gz.startAngle = fmod(currentHeading + 90.0, 360.0);
                gz.endAngle   = fmod(currentHeading + 270.0, 360.0);
            }
            changed = true;
        }
    }
    if (changed) {
        emit guardZoneModified();
        update();
    }
}

void EcWidget::defaultSettingsStartUp(){
    orientation = SettingsManager::instance().data().orientationMode;
    centering = SettingsManager::instance().data().centeringMode;
    trackLine = SettingsManager::instance().data().trailMode;

    trackDistance = SettingsManager::instance().data().trailDistance;
    trackMinute = SettingsManager::instance().data().trailMinute;

    latView = SettingsManager::instance().data().latViewMode;
    longView = SettingsManager::instance().data().longViewMode;

    dragMode = SettingsManager::instance().data().chartMode == "Drag";
}

void EcWidget::applyShipDimensions()
{
    const SettingsData& settings = SettingsManager::instance().data();

    EcShipDimension dim;
    dim.length = settings.shipLength;
    dim.breadth = settings.shipBeam;
    dim.draught = settings.shipDraftMeters;
    dim.airDraught = settings.shipHeight; // Menggunakan Tinggi Total untuk airDraught

    if (view) {
        // TODO: Ganti "EcChartSetOwnshipDim" dengan nama fungsi kernel yang benar dari dokumentasi Anda.
        // EcChartSetOwnshipDim(view, &dim);
    }

    // Logika untuk menerapkan offset GPS utama
    if (settings.primaryGpsIndex >= 0 && settings.primaryGpsIndex < settings.gpsPositions.size()) {
        const GpsPosition& primaryGps = settings.gpsPositions.at(settings.primaryGpsIndex);
        // Asumsi ada fungsi untuk mengatur offset referensi
        // Fungsi ini mungkin memerlukan nama lain atau tidak ada sama sekali, perlu verifikasi dari dokumentasi kernel
        // EcDrawSetReferencePointOffset(view, primaryGps.offsetX, primaryGps.offsetY);
    }

    // Memaksa penggambaran ulang untuk menerapkan perubahan visual
    forceRedraw();

    if (routeSafetyFeature) {
        routeSafetyFeature->invalidateAll();
    }
}

// ========== ROUTE DEVIATION DETECTOR IMPLEMENTATION ==========

void EcWidget::initializeRouteDeviationDetector()
{
    qDebug() << "[ROUTE-DEVIATION] Initializing Route Deviation Detector";

    if (!routeDeviationDetector) {
        routeDeviationDetector = new RouteDeviationDetector(this);

        // Connect signals
        connect(routeDeviationDetector, &RouteDeviationDetector::deviationDetected,
                [this](const RouteDeviationDetector::DeviationInfo& info) {
                    qDebug() << "[ROUTE-DEVIATION] Deviation detected signal received";
                    emit statusMessage(tr("⚠ Off Track: %.2f NM, Angle: %.1f°")
                                       .arg(std::abs(info.crossTrackDistance))
                                       .arg(std::abs(info.deviationAngle)));
                });

        connect(routeDeviationDetector, &RouteDeviationDetector::deviationCleared,
                [this]() {
                    qDebug() << "[ROUTE-DEVIATION] Back on track";
                    emit statusMessage(tr("✓ Back on track"));
                });

        connect(routeDeviationDetector, &RouteDeviationDetector::visualUpdateRequired,
                [this]() {
                    update();
                });

        // Set default parameters
        routeDeviationDetector->setDeviationThreshold(0.1);  // 0.1 NM default
        routeDeviationDetector->setAutoCheckEnabled(true && AppConfig::isDevelopment());
        routeDeviationDetector->setCheckInterval(2000);  // 2 seconds
        routeDeviationDetector->setPulseEnabled(true);
        routeDeviationDetector->setLabelVisible(true);

        qDebug() << "[ROUTE-DEVIATION] ✓ RouteDeviationDetector initialized successfully";
    }
}

void EcWidget::drawRouteDeviationIndicator(QPainter& painter)
{
    if (!routeDeviationDetector || !initialized || !view) {
        return;
    }

    // Check if auto-check is enabled (controlled by checkbox)
    if (!routeDeviationDetector->isAutoCheckEnabled()) {
        return; // Feature disabled by user
    }

    // Get current deviation info
    RouteDeviationDetector::DeviationInfo deviation = routeDeviationDetector->getCurrentDeviation();

    if (!deviation.isDeviated) {
        return; // No deviation to draw
    }

    qDebug() << "[ROUTE-DEVIATION-DRAW] Drawing deviation indicator";

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Get ownship screen position
    int shipX = 0, shipY = 0;
    if (!LatLonToXy(navShip.lat, navShip.lon, shipX, shipY)) {
        painter.restore();
        return;
    }

    // ========== DRAW PULSING CIRCLE (like red pulse with zoom-aware sizing) ==========
    if (routeDeviationDetector->isPulseEnabled()) {
        // Use pixel-based animation like AOI red pulse
        static QElapsedTimer routePulseTimer;
        if (!routePulseTimer.isValid()) {
            routePulseTimer.start();
        }
        qint64 elapsedMs = routePulseTimer.elapsed();
        double t = (elapsedMs % 1500) / 1500.0;  // 1.5 second cycle

        // Calculate zoom-aware base radius (EXACTLY like red pulse logic)
        double rangeNM = GetRange(GetScale());
        int baseMinRadius;

        if (rangeNM >= 2) {
            baseMinRadius = 12;  // Zoomed out (range >= 2 NM)
        } else if (rangeNM >= 1) {
            baseMinRadius = 22;  // Medium zoom (1-2 NM)
        } else {
            baseMinRadius = 32;  // Zoomed in (< 1 NM)
        }

        // Animated pulse radius (pixels)
        int pulseRadius = baseMinRadius + (int)(4 * std::sin(t * 2.0 * M_PI / 1.5));
        int opacity = 170 + (int)(60 * std::sin(t * 2.0 * M_PI / 2.0));

        // Outer glow (orange)
        QPen glowPen(QColor(255, 140, 0, opacity / 3));
        glowPen.setWidth(1);
        painter.setPen(glowPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPoint(shipX, shipY), pulseRadius + 5, pulseRadius + 5);

        // Main orange ring + soft fill
        QPen ringPen(QColor(255, 140, 0, opacity));
        ringPen.setWidth(2);
        painter.setPen(ringPen);
        QBrush ringBrush(QColor(255, 140, 0, 0));  // No fill
        painter.setBrush(ringBrush);
        painter.drawEllipse(QPoint(shipX, shipY), pulseRadius, pulseRadius);
    }

    // ========== DRAW LINE TO CLOSEST POINT ON ROUTE ==========
    QPointF closestPoint = deviation.closestPoint;
    int closestX = 0, closestY = 0;
    if (LatLonToXy(closestPoint.x(), closestPoint.y(), closestX, closestY)) {
        QPen linePen(QColor(255, 140, 0, 200));  // Orange
        linePen.setWidth(2);
        linePen.setStyle(Qt::DashLine);
        painter.setPen(linePen);
        painter.drawLine(shipX, shipY, closestX, closestY);

        // Draw small circle at closest point
        painter.setBrush(QColor(255, 140, 0, 150));
        painter.drawEllipse(QPoint(closestX, closestY), 5, 5);
    }

    // ========== DRAW DEVIATION LABEL ==========
    if (routeDeviationDetector->isLabelVisible()) {
        // Compose label text
        QString labelText = QString("Off Track: %1 NM\nAngle: %2°")
                           .arg(QString::number(std::abs(deviation.crossTrackDistance), 'f', 2))
                           .arg(QString::number(std::abs(deviation.deviationAngle), 'f', 1));

        // Draw label near ownship
        QFont labelFont("Arial", 10, QFont::Bold);
        painter.setFont(labelFont);
        QFontMetrics fm(labelFont);

        int labelWidth = fm.horizontalAdvance(labelText.split('\n')[0]) + 10;
        int labelHeight = fm.height() * 2 + 10;

        // Position label to the right of ownship
        int labelX = shipX + 30;
        int labelY = shipY - labelHeight / 2;

        // Draw background
        QRect labelRect(labelX, labelY, labelWidth + 20, labelHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 180));
        painter.drawRoundedRect(labelRect, 5, 5);

        // Draw text
        painter.setPen(QColor(255, 165, 0));  // Orange text
        painter.drawText(labelRect, Qt::AlignCenter, labelText);

        qDebug() << "[ROUTE-DEVIATION-DRAW] Label drawn:" << labelText;
    }

    painter.restore();
}
