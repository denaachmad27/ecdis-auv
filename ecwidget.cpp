// #include <QtGui>
#include <QtWidgets>
#include <QtWin>
#ifndef _WIN32
#include <QX11Info>
#endif

#include "ecwidget.h"
#include "alertsystem.h"
#include "ais.h"
#include "pickwindow.h"
#include "aistooltip.h"
#include "mainwindow.h"

// Waypoint
#include "SettingsManager.h"
#include "aisdatabasemanager.h"
#include "aivdoencoder.h"
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

// Guardzone
#include "IAisDvrPlugin.h"
#include "PluginManager.h"
#include "guardzonecheckdialog.h"
#include "guardzonemanager.h"
#include <QtMath>
#include <cmath>

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

ShipStruct mapShip = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

ShipStruct navShip = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

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

  // Waypoint
  loadWaypoints(); // Muat waypoint dari file JSON
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

  attachedGuardZoneId = -1;
  attachedGuardZoneName = "";

  // ====== INITIALIZE ROUTE/WAYPOINT SYSTEM ======
  // Initialize route variables
  isRouteMode = false;
  routeWaypointCounter = 1;
  currentRouteId = 1;
  activeFunction = PAN;
  
  // Load existing waypoints from JSON file
  loadWaypoints();
  
  qDebug() << "[ECWIDGET] Route/Waypoint system initialized";

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
  // Check projection because it depends on viewport
  SetProjection(projectionMode);
}

/*---------------------------------------------------------------------------*/

void EcWidget::SetHeading (double newHeading)
{
  currentHeading = newHeading;
  if (currentScale > maxScale) currentScale = maxScale; // in case the world overview has been shown before
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

void EcWidget::draw(bool upd)
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

/*---------------------------------------------------------------------------*/

void EcWidget::Draw()
{
    draw(true);
    if(showAIS)
        drawAISCell();

    // Gambar waypoint dengan warna sesuai routeId  
    QList<QColor> routeColors = {
        QColor(255, 140, 0),     // Orange untuk single waypoints (routeId = 0)
        QColor(255, 100, 100),   // Merah terang untuk Route 1
        QColor(100, 255, 100),   // Hijau terang untuk Route 2  
        QColor(100, 100, 255),   // Biru terang untuk Route 3
        QColor(255, 255, 100),   // Kuning untuk Route 4
        QColor(255, 100, 255),   // Magenta untuk Route 5
        QColor(100, 255, 255),   // Cyan untuk Route 6
    };
    
    for (const Waypoint &wp : waypointList)
    {
        // Pilih warna berdasarkan routeId
        QColor waypointColor;
        if (wp.routeId == 0) {
            waypointColor = routeColors[0]; // Orange untuk single waypoints
        } else {
            waypointColor = routeColors[wp.routeId % routeColors.size()];
        }
        
        drawSingleWaypoint(wp.lat, wp.lon, wp.label, waypointColor);
    }

    // HAPUS: Garis legline antar waypoint - sekarang digantikan oleh drawRouteLines()
    // Kode ini DIHAPUS untuk menghindari duplikasi dengan drawRouteLines()
    // drawRouteLines() sudah menggambar garis dengan warna yang benar per route

    drawLeglineLabels();
    
    // Gambar garis route dengan warna berbeda per route (enhancement)
    drawRouteLines();

    // Tidak perlu memanggil drawGuardZone() di sini,
    // karena akan dipanggil secara otomatis di paintEvent

    // ========== REFRESH GUARDZONE HANDLE POSITIONS ==========
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        guardZoneManager->refreshHandlePositions();
    }
    // ======================================================

    update();
}
/*---------------------------------------------------------------------------*/

void EcWidget::paintEvent (QPaintEvent *e)
{
  if (! initialized) return;

  QPainter painter(this);
  painter.drawPixmap(e->rect(), drawPixmap, e->rect());
  
  // FORCE CLEANUP: Run cleanup from paint event as fallback
  static int paintCleanupCounter = 0;
  if (++paintCleanupCounter % 100 == 0) { // Every ~100 paint events (less frequent)
      qDebug() << "[PAINT-CLEANUP] Running obstacle cleanup from paintEvent";
      if (!obstacleMarkers.isEmpty()) {
          removeOutdatedObstacleMarkers();
      }
  }

  // PLEASE WAIT

  // Draw ghost waypoint saat move mode
  if (ghostWaypoint.visible) {
      drawGhostWaypoint(ghostWaypoint.lat, ghostWaypoint.lon, ghostWaypoint.label);
  }

  drawGuardZone();
  // TEMPORARY: Disabled untuk presentasi - obstacle area menyebabkan crash
  // drawObstacleDetectionArea(painter); // Show obstacle detection area (now safe)
  //drawRedDotTracker();
  
  // Re-enabled with enhanced safety protections
  drawObstacleMarkers(painter); // Draw obstacle markers with color-coded dots
  
  // Draw chart flashing overlay for dangerous obstacles
  drawChartFlashOverlay(painter);
  
  drawRedDotTracker();

  // ========== DRAW TEST GUARDZONE ==========

  drawTestGuardSquare(painter);
  if (testGuardZoneEnabled) {

  }
  // =======================================

  EcCoordinate lat, lon;
  _aisObj->getOwnShipPos(lat, lon);

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
  else if (!qIsNaN(lat) && !qIsNaN(lon) && lat != 0 && lon != 0) {
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

  // Draw red box around dangerous AIS targets
  if (_aisObj) {
      QPainter painter(this);
      painter.setRenderHint(QPainter::Antialiasing);
      QMap<unsigned int, AISTargetData> targetMap = _aisObj->getTargetMap();
      for (const AISTargetData &target : targetMap) {
          if (target.isDangerous) {
              int x, y;
              if (LatLonToXy(target.lat, target.lon, x, y)) {
                  painter.setPen(QPen(Qt::red, 2, Qt::SolidLine));
                  painter.setBrush(QColor(255, 0, 0, 40)); // Solid red border, 40/255 transparent fill
                  painter.drawRect(x - 20, y - 20, 40, 40);
              }
          }
      }
  }
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
  int wi  = event->size().width();
  int hei = event->size().height();

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
    EcDrawEnd (view);
  }

  chartPixmap = QPixmap(wi, hei);
  drawPixmap = QPixmap(wi, hei);
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
  if(showAIS)
    drawAISCell();
}

/*---------------------------------------------------------------------------*/

/*
// Mouseevent lama
void EcWidget::mousePressEvent(QMouseEvent *e)
{
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
            else if (activeFunction == CREATE_WAYP)
            {
                // Single waypoint creation logic
                createWaypointAt(lat, lon);
                
                // End single waypoint mode
                activeFunction = PAN;
                if (mainWindow) {
                    mainWindow->statusBar()->showMessage(tr("Waypoint created"), 3000);
                    mainWindow->setWindowTitle("ECDIS AUV");
                }
                emit waypointCreated();
            }
            else if (activeFunction == CREATE_ROUTE)
            {
                // Route mode - continuous waypoint creation
                createWaypointAt(lat, lon);
                
                // Update status for next waypoint
                if (mainWindow) {
                    mainWindow->statusBar()->showMessage(
                        tr("Route Mode: Waypoint %1 created. Click for next waypoint or ESC/right-click to end")
                        .arg(routeWaypointCounter), 0);
                }
                
                routeWaypointCounter++;
                
                // Stay in CREATE_ROUTE mode for continuous creation
            }
            else
            {
                // Normal pan/click
                SetCenter(lat, lon);
                Draw();
            }
        }
    }
    else if (e->button() == Qt::RightButton && !creatingGuardZone) {
        
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

        // Check if right-clicked on a guardzone
        if (guardZoneManager) {
            int clickedGuardZoneId = guardZoneManager->getGuardZoneAtPosition(e->x(), e->y());

            if (clickedGuardZoneId != -1) {
                guardZoneManager->showGuardZoneContextMenu(e->pos(), clickedGuardZoneId);
                return; // Return early - jangan lanjut ke normal right click
            }
        }

        // Normal right click behavior (existing)
        activeFunction = PAN;
        pickX = e->x();
        pickY = e->y();
        emit mouseRightClick(e->pos());
    }
}



/*---------------------------------------------------------------------------*/

void EcWidget::mouseMoveEvent(QMouseEvent *e)
{

    // Simpan posisi mouse terakhir
    lastMousePos = e->pos();

    // Cek apakah mouse masih di atas AIS target yang sama
    EcAISTargetInfo* targetInfo = findAISTargetInfoAtPosition(lastMousePos);

        if (targetInfo) {
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

    // Normal mouse move processing
    EcCoordinate lat, lon;
    if (XyToLatLon(e->x(), e->y(), lat, lon)) {
        emit mouseMove(lat, lon);
    }
}

/*---------------------------------------------------------------------------*/


void EcWidget::mouseReleaseEvent(QMouseEvent *e)
{
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
    unsigned char *mId = NULL;
    s63HwId = NULL;
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

  double dWarnCPA = 0.2;
  int iWarnTCPA = 1;

  int iTimeOut = 1;
  Bool bInternalGPS = False;
  Bool bAISSymbolize;
  QString strErrLogAis = QString( "%1%2%3" ).arg( QCoreApplication::applicationDirPath() ).arg( "/" ).arg( "errorAISLog.txt" );

  _aisObj = new Ais( this, view, dict, ownShip.lat, ownShip.lon,
    ownShip.sog, ownShip.cog, dWarnDist, dWarnCPA,
    iWarnTCPA, strAisLib, iTimeOut, bInternalGPS, &bAISSymbolize, strErrLogAis );

  QObject::connect( _aisObj, SIGNAL( signalRefreshChartDisplay( double, double, double ) ), this, SLOT( slotRefreshChartDisplay( double, double, double ) ) );
  QObject::connect( _aisObj, SIGNAL( signalRefreshCenter( double, double ) ), this, SLOT( slotRefreshCenter( double, double ) ) );
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

    subscriber->moveToThread(threadAIS);

    QString sshIP = SettingsManager::instance().data().moosIp;
    quint16 sshPort = 5000;

    connect(threadAIS, &QThread::started, [=]() {
        subscriber->connectToHost(sshIP, sshPort);
    });

    connect(subscriber, &AISSubscriber::navLatReceived, this, [=](double lat) {
        navShip.lat = lat;
    });

    connect(subscriber, &AISSubscriber::navLongReceived, this, [=](double lon) {
        navShip.lon = lon;
    });

    connect(subscriber, &AISSubscriber::navDepthReceived, this, [=](double depth) {
        navShip.depth = depth;
    });

    connect(subscriber, &AISSubscriber::navHeadingReceived, this, [=](double hdg) {
        navShip.heading = hdg;
    });

    connect(subscriber, &AISSubscriber::navHeadingOGReceived, this, [=](double cog) {
        navShip.heading_og = cog;
    });

    connect(subscriber, &AISSubscriber::navSpeedReceived, this, [=](double sog) {
        navShip.speed_og = sog;
    });

    connect(subscriber, &AISSubscriber::navYawReceived, this, [=](double yaw) {
        navShip.yaw = yaw;
    });

    connect(subscriber, &AISSubscriber::navZReceived, this, [=](double z) {
        navShip.z = z;
    });

    connect(subscriber, &AISSubscriber::mapInfoReqReceived, this, &EcWidget::processMapInfoReq);

    connect(subscriber, &AISSubscriber::processingAis, this, &EcWidget::processAis);

    connect(subscriber, &AISSubscriber::processingData, this, &EcWidget::processData);

    connect(subscriber, &AISSubscriber::errorOccurred, this, [](const QString &msg) {
        qWarning() << "Error:" << msg;
    });

    connect(subscriber, &AISSubscriber::disconnected, this, []() {
        qDebug() << "Disconnected from AIS source.";
    });

    connect(threadAIS, &QThread::finished, subscriber, &QObject::deleteLater);
    threadAIS->start();
}

void EcWidget::stopAISConnection()
{
    if (subscriber) {
        // Pastikan disconnectFromHost dijalankan di thread-nya subscriber
        QMetaObject::invokeMethod(subscriber, "disconnectFromHost", Qt::QueuedConnection);
    }

    if (threadAIS) {
        threadAIS->quit();
        threadAIS->wait();
        threadAIS->deleteLater();
        threadAIS = nullptr;

        // Pastikan pointer subscriber tidak langsung dihapus,
        // biarkan dia deleteLater() sendiri kalau perlu.
        subscriber = nullptr;
    }
}

void EcWidget::processData(double lat, double lon, double cog, double sog, double hdg, double spd, double dep, double yaw, double z){
    QString nmea = AIVDOEncoder::encodeAIVDO1(lat, lon, cog, sog/10, hdg, 0, 1);

    _aisObj->readAISVariable({nmea});

    IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");
    if (dvr && dvr->isRecording() && !nmea.isEmpty()) {
        dvr->recordRawNmea(nmea);
    }

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
    if (sendSocket->waitForConnected(3000)) {
        sendSocket->write(sendData);
        sendSocket->waitForBytesWritten(3000);
        sendSocket->disconnectFromHost();
    }
    else {
        qCritical() << "Could not connect to data server.";
    }

    sendSocket->deleteLater();
    delete pickWindow;

    // OWNSHIP PANEL
    ownShipText->setHtml(pickWindow->ownShipAutoFill());

    // INSERT TO DATABASE
    // PLEASE WAIT!!
    // AisDatabaseManager::instance().insertOwnShipToDB(lat, lon, dep, hdg, cog, spd, sog, yaw, z);

    // EKOR OWNSHIP
    //ownShipTrailPoints.append(qMakePair(EcCoordinate(lat), EcCoordinate(lon)));
}

void EcWidget::processAis(QString ais){
    if (ais.startsWith("!AIVDM")) {
        QStringList nmeaData{ais};
        IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");

        if (dvr && dvr->isRecording() && !ais.isEmpty()) {
            dvr->recordRawNmea(ais);
        }

        _aisObj->readAISVariableString(ais);
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

    if (mapInfo.lat != lat || mapInfo.lon != lon){
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

void EcWidget::publishToMOOSDB(QString varName, QString data){
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
    }
    else {
        qCritical() << "Could not connect to data server.";
    }

    sendSocket->deleteLater();
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
  if(showAIS)
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
  // Draw red dot tracker overlay
  drawRedDotTracker();

  // DRAWING OWNSHIP CUSTOM
  if (showCustomOwnShip) {
      AISTargetData ownShipData = Ais::instance()->getOwnShipVar();

      // Untuk simulasi, gunakan data simulasi
      if (simulationActive && ownShipInSimulation) {
          ownShipData.lat = ownShip.lat;
          ownShipData.lon = ownShip.lon;
          ownShipData.cog = ownShip.cog;          // ⭐ Gunakan COG dari simulasi
          ownShipData.heading = ownShip.heading;  // ⭐ Gunakan heading dari simulasi
          ownShipData.sog = ownShip.sog;
      }

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

              // ⭐ PANGGIL DENGAN PARAMETER BARU: COG, Heading, SOG
              drawOwnShipIcon(painter, x, y, cog, heading, ownShipData.sog);

              // GAMBAR EKOR OWNSHIP
              if (showOwnShipTrail){
                  drawOwnShipTrail(painter);
              }

              painter.end();
          }
      }

      if (SettingsManager::instance().data().orientationMode == HeadUp){
          SetHeading(ownShipData.heading);
          mainWindow->oriEditSetText(ownShipData.heading);
      }
      else if (SettingsManager::instance().data().orientationMode == CourseUp){
          SetHeading(SettingsManager::instance().data().courseUpHeading);
          mainWindow->oriEditSetText(SettingsManager::instance().data().courseUpHeading);
      }
      else {
          SetHeading(0);
      }
  }

  update();

  emit projection();
  emit scale( currentScale );
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

    if (showAIS)
    {
        if ((lat != 0 && lon != 0) && trackTarget.isEmpty())
        {
            if (SettingsManager::instance().data().centeringMode == LookAhead) // ⭐ Look-Ahead mode
            {
                double offsetNM = GetRange(currentScale) * 0.5;
                double headingRad = head * M_PI / 180.0;
                double offsetLat = offsetNM * cos(headingRad) / 60.0;
                double offsetLon = offsetNM * sin(headingRad) / (60.0 * cos(lat * M_PI / 180.0));

                double centerLat = lat + offsetLat;
                double centerLon = lon + offsetLon;

                SetCenter(centerLat, centerLon);
            }
            else if (SettingsManager::instance().data().centeringMode == Centered)
            {
                SetCenter(lat, lon);
            }
        }

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
  if (attachedGuardZoneId == -1 && lat != 0 && lon != 0 && !qIsNaN(lat) && !qIsNaN(lon) && redDotAttachedToShip) {
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
          createAttachedGuardZone();
          
          // PERBAIKAN: Emit signal untuk sync UI setelah guardzone dibuat
          emit attachToShipStateChanged(true);
      } else {
          qDebug() << "[POSITION-UPDATE] Skipping creation - attached guardzone already exists (" << attachedGuardZoneCount << " found)";
      }
  }
  // ==================================================
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

    draw(true);
    slotUpdateAISTargets( true );
  }
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
}

void EcWidget::createSeparateRouteWaypoint(const Waypoint &waypoint)
{
    // Simple approach: Just add to list and save, similar to GuardZone
    Waypoint wp = waypoint;
    wp.featureHandle.id = EC_NOCELLID;
    wp.featureHandle.offset = 0;
    
    // Add to our list
    waypointList.append(wp);
    
    qDebug() << "[ROUTE] Added waypoint" << wp.label << "to route" << wp.routeId;
    
    // Save to JSON file (like GuardZone does)
    saveWaypoints();
    
    // Simple visual marker
    drawWaypointMarker(wp.lat, wp.lon);
    
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

    // 🚀 Simpan waypoint baru
    Waypoint wp;
    wp.lat = lat;
    wp.lon = lon;
    wp.label = QString("WP%1").arg(waypointList.size() + 1, 3, 10, QChar('0')); // Label: WP001, WP002
    wp.remark = ""; // Default empty remark
    wp.turningRadius = 10.0; // Default turning radius
    wp.active = true; // Default active status
    waypointList.append(wp);
    saveWaypoints(); // Save to JSON file



    // 🚀 Langsung redraw semua
    Draw();
}


void EcWidget::drawSingleWaypoint(double lat, double lon, const QString& label, const QColor& color)
{
    int x, y;

    if (!LatLonToXy(lat, lon, x, y))
        return;

    QPainter painter(&drawPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(color); // Menggunakan warna yang diberikan
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // Gambar Donut
    painter.drawEllipse(QPoint(x, y), 8, 8);

    // Gambar Label di kanan atas
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 8));
    painter.drawText(x + 10, y - 10, label);

    painter.end();
}

void EcWidget::drawGhostWaypoint(double lat, double lon, const QString& label)
{
    int x, y;

    if (!LatLonToXy(lat, lon, x, y))
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Style ghost waypoint: semi-transparent dengan outline dashed
    QPen pen(QColor(255, 140, 0, 120)); // Orange semi-transparent
    pen.setWidth(2);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    
    QBrush brush(QColor(255, 140, 0, 60)); // Fill semi-transparent
    painter.setBrush(brush);

    // Gambar ghost donut
    painter.drawEllipse(QPoint(x, y), 8, 8);

    // Gambar ghost label
    painter.setPen(QColor(0, 0, 0, 120)); // Black semi-transparent
    painter.setFont(QFont("Arial", 8));
    painter.drawText(x + 10, y - 10, label);

    painter.end();
}

void EcWidget::saveWaypoints()
{
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
                saveWaypoints();
                Draw();

                QMessageBox::information(this, tr("Waypoint Removed"),
                                         tr("Waypoint '%1' has been removed.").arg(waypointLabel));
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
                    
                    qDebug() << "[DEBUG] Waypoint selected for moving:" << i;
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
            saveWaypoints();
            Draw();

            // Tampilkan Pick Report setelah pindah waypoint
            QList<EcFeature> pickedList;
            GetPickedFeaturesSubs(pickedList, newLat, newLon);
            PickWindow *pw = new PickWindow(this, dictInfo, denc);
            pw->fill(pickedList);
            pw->exec();

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
                    waypointList[i].label = dialog.getLabel();
                    waypointList[i].remark = dialog.getRemark();
                    waypointList[i].turningRadius = dialog.getTurnRadius();
                    waypointList[i].active = dialog.isActive();

                    saveWaypoints();
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
    
    // Warna yang konsisten dengan route colors
    QList<QColor> routeColors = {
        QColor(255, 140, 0),     // Orange untuk single waypoints (routeId = 0)
        QColor(255, 100, 100),   // Merah terang untuk Route 1
        QColor(100, 255, 100),   // Hijau terang untuk Route 2  
        QColor(100, 100, 255),   // Biru terang untuk Route 3
        QColor(255, 255, 100),   // Kuning untuk Route 4
        QColor(255, 100, 255),   // Magenta untuk Route 5
        QColor(100, 255, 255),   // Cyan untuk Route 6
    };
    
    painter.setFont(QFont("Arial", 8, QFont::Bold));

    double defaultSpeed = 10.0; // knot (bisa kamu ubah sesuai kebutuhan)

    for (int i = 0; i < waypointList.size() - 1; ++i)
    {
        // MODIFIKASI: Hanya gambar label untuk waypoint dalam route yang sama
        if (waypointList[i].routeId != waypointList[i + 1].routeId) {
            qDebug() << "[LEGLINE] Skipping label between different routes:" 
                     << waypointList[i].routeId << "and" << waypointList[i + 1].routeId;
            continue; // Skip jika beda route
        }
        
        EcCoordinate lat1 = waypointList[i].lat;
        EcCoordinate lon1 = waypointList[i].lon;
        EcCoordinate lat2 = waypointList[i+1].lat;
        EcCoordinate lon2 = waypointList[i+1].lon;

        double dist = 0.0;
        double bearing = 0.0;

        EcCalculateRhumblineDistanceAndBearing(
            EC_GEO_DATUM_WGS84,
            lat1, lon1,
            lat2, lon2,
            &dist, &bearing
            );

        // ETA
        double hours = dist / defaultSpeed;
        int h = static_cast<int>(hours);
        int m = static_cast<int>((hours - h) * 60 + 0.5); // dibulatkan ke menit terdekat

        // Posisi tengah garis
        int x1, y1, x2, y2;
        if (LatLonToXy(lat1, lon1, x1, y1) && LatLonToXy(lat2, lon2, x2, y2))
        {
            // Set warna teks sesuai dengan routeId
            int routeId = waypointList[i].routeId;
            QColor textColor = routeColors[routeId % routeColors.size()];
            painter.setPen(textColor);
            
            int midX = (x1 + x2) / 2;
            int midY = (y1 + y2) / 2;

            QString degree = QString::fromUtf8("\u00B0"); // simbol derajat
            QString text = QString("%1 NM @ %2%3\nETA: %4h %5m")
                               .arg(QString::number(dist, 'f', 1))
                               .arg(QString::number(bearing, 'f', 0))
                               .arg(degree)
                               .arg(h)
                               .arg(m);

            painter.drawText(midX + 5, midY - 5, text);
        }
    }

    painter.end();
}

// ====== ROUTE LINE DRAWING DENGAN KONTROL PENUH ======
void EcWidget::drawRouteLines()
{
    if (waypointList.size() < 2) return;
    
    QPainter painter(&drawPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Warna untuk route yang berbeda (harus sama dengan warna waypoint)
    QList<QColor> routeColors = {
        QColor(255, 140, 0),     // Orange untuk single waypoints (routeId = 0)
        QColor(255, 100, 100),   // Merah terang untuk Route 1
        QColor(100, 255, 100),   // Hijau terang untuk Route 2  
        QColor(100, 100, 255),   // Biru terang untuk Route 3
        QColor(255, 255, 100),   // Kuning untuk Route 4
        QColor(255, 100, 255),   // Magenta untuk Route 5
        QColor(100, 255, 255),   // Cyan untuk Route 6
    };
    
    // APPROACH BARU: Group waypoints by route ID first, then draw lines within each route
    QMap<int, QList<int>> routeWaypoints; // routeId -> list of waypoint indices
    
    // Group waypoints by routeId first
    for (int i = 0; i < waypointList.size(); ++i) {
        int routeId = waypointList[i].routeId;
        if (routeId > 0) { // Only route waypoints, skip single waypoints
            routeWaypoints[routeId].append(i);
        }
    }
    
    qDebug() << "[ROUTE-DRAW] Found" << routeWaypoints.size() << "different routes";
    
    // Draw lines within each route separately
    for (auto it = routeWaypoints.begin(); it != routeWaypoints.end(); ++it) {
        int routeId = it.key();
        QList<int> indices = it.value();
        
        if (indices.size() < 2) {
            qDebug() << "[ROUTE-DRAW] Route" << routeId << "has only" << indices.size() << "waypoints, skipping lines";
            continue;
        }
        
        // Pilih warna berdasarkan routeId
        QColor routeColor = routeColors[routeId % routeColors.size()];
        
        QPen pen(routeColor);
        pen.setStyle(Qt::DashLine); 
        pen.setWidth(2);
        painter.setPen(pen);
        
        qDebug() << "[ROUTE-DRAW] Drawing" << (indices.size() - 1) << "lines for route" << routeId;
        
        // Draw lines between consecutive waypoints in THIS route only
        for (int i = 0; i < indices.size() - 1; ++i) {
            int idx1 = indices[i];
            int idx2 = indices[i + 1];
            
            const Waypoint &wp1 = waypointList[idx1];
            const Waypoint &wp2 = waypointList[idx2];
            
            int x1, y1, x2, y2;
            if (LatLonToXy(wp1.lat, wp1.lon, x1, y1) && LatLonToXy(wp2.lat, wp2.lon, x2, y2)) {
                painter.drawLine(x1, y1, x2, y2);
                qDebug() << "[ROUTE-DRAW] Drew line between" << wp1.label << "and" << wp2.label 
                         << "in route" << routeId;
            }
        }
    }
    
    painter.end();
    qDebug() << "[ROUTE-DRAW] Finished controlled route line drawing";
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
        QJsonArray waypointsArray = doc.array();

        waypointList.clear();
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

            waypointList.append(wp);
            validWaypoints++;
            
            // Track maximum route ID
            if (wp.routeId > maxRouteId) {
                maxRouteId = wp.routeId;
            }
        }
        
        // Set currentRouteId to be higher than existing routes
        currentRouteId = maxRouteId + 1;

        qDebug() << "[INFO] Loaded" << validWaypoints << "waypoints from" << filePath << "- Max Route ID:" << maxRouteId;

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

    // Show confirmation dialog
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("Clear Waypoints"));
    msgBox.setText(tr("Are you sure you want to remove all waypoints?"));
    msgBox.setInformativeText(tr("This action cannot be undone."));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setIcon(QMessageBox::Warning);

    if (msgBox.exec() == QMessageBox::Yes)
    {
        qDebug() << "[INFO] Clearing all waypoints";

        // 🎯 Hapus waypoint menggunakan SevenCs jika handle valid
        for (int i = waypointList.size() - 1; i >= 0; i--)
        {
            if (waypointList[i].isValid())
            {
                Bool result = EcRouteDeleteWaypoint(dictInfo, waypointList[i].featureHandle);
                if (!result) {
                    qDebug() << "[WARNING] Failed to delete waypoint" << i << "from SevenCs";
                }
            }
        }

        waypointList.clear();
        saveWaypoints(); // Save empty list to file
        drawWaypointCell();
        Draw(); // Redraw without waypoints

        QMessageBox::information(this, tr("Waypoints Cleared"),
                                 tr("All waypoints have been removed."));
    }
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

void EcWidget::drawGuardZone()
{
    qDebug() << "[DRAW-GUARDZONE-DEBUG] ========== STARTING DRAW GUARDZONE ==========";
    
    // TAMBAHAN: Performance timer
    QElapsedTimer timer;
    timer.start();

    qDebug() << "[DRAW-GUARDZONE-DEBUG] Creating QPainter...";
    QPainter painter(this);
    if (!painter.isActive()) {
        qDebug() << "[DRAW-GUARDZONE-ERROR] QPainter is not active - aborting";
        return;
    }
    qDebug() << "[DRAW-GUARDZONE-DEBUG] QPainter created successfully";
    
    painter.setRenderHint(QPainter::Antialiasing, true);
    qDebug() << "[DRAW-GUARDZONE-DEBUG] Antialiasing set";

    // TAMBAHAN: Early exit jika tidak ada guardzone
    qDebug() << "[DRAW-GUARDZONE-DEBUG] Checking guardZones list, size:" << guardZones.size();
    if (guardZones.isEmpty()) {
        qDebug() << "[DRAW-GUARDZONE-DEBUG] No guardzones to draw";
        if (creatingGuardZone) {
            qDebug() << "[DRAW-GUARDZONE-DEBUG] Drawing creation preview...";
            // Tetap gambar preview creation
            drawGuardZoneCreationPreview(painter);
        }
        qDebug() << "[DRAW-GUARDZONE-DEBUG] Early return - no guardzones";
        return;
    }
    qDebug() << "[DRAW-GUARDZONE-DEBUG] Found" << guardZones.size() << "guardzones to process";

    int drawnCount = 0;
    int skippedCount = 0;

    // TAMBAHAN: Viewport culling untuk performance  
    qDebug() << "[DRAW-GUARDZONE-DEBUG] Getting viewport rect...";
    QRect viewport = rect();
    qDebug() << "[DRAW-GUARDZONE-DEBUG] Viewport rect:" << viewport;

    qDebug() << "[DRAW-GUARDZONE-DEBUG] === STARTING GUARDZONE ITERATION ===";
    
    for (const GuardZone &gz : guardZones) {
        qDebug() << "[DRAW-GUARDZONE-DEBUG] ===== PROCESSING GUARDZONE" << gz.id << "=====";
        qDebug() << "[DRAW-GUARDZONE-DEBUG] - Name:" << gz.name;
        qDebug() << "[DRAW-GUARDZONE-DEBUG] - Active:" << gz.active;
        qDebug() << "[DRAW-GUARDZONE-DEBUG] - AttachedToShip:" << gz.attachedToShip;
        qDebug() << "[DRAW-GUARDZONE-DEBUG] - Shape:" << gz.shape;
        qDebug() << "[DRAW-GUARDZONE-DEBUG] - Color:" << gz.color;
        if (gz.shape == GUARD_ZONE_CIRCLE) {
            qDebug() << "[DRAW-GUARDZONE-DEBUG] - Circle center:" << gz.centerLat << "," << gz.centerLon;
            qDebug() << "[DRAW-GUARDZONE-DEBUG] - Circle radius:" << gz.radius;
        }
        
        if (!gz.active && !creatingGuardZone) {
            qDebug() << "[DRAW-DEBUG] Skipping inactive guardzone" << gz.id;
            skippedCount++;
            continue;
        }

        // TAMBAHAN: Viewport culling check - SKIP untuk attached guardzone
        if (!gz.attachedToShip && !isGuardZoneInViewport(gz, viewport)) {
            qDebug() << "[DRAW-DEBUG] Skipping guardzone" << gz.id << "- outside viewport";
            skippedCount++;
            continue;
        }
        
        qDebug() << "[DRAW-DEBUG] Drawing guardzone" << gz.id;

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

            if (gz.attachedToShip) {
                qDebug() << "[DRAW-GUARDZONE-DEBUG] Processing attached guardzone" << gz.id;
                // PERBAIKAN: Untuk attached guardzone, gunakan redDot position yang current
                if (redDotTrackerEnabled && redDotLat != 0.0 && redDotLon != 0.0) {
                    lat = redDotLat;
                    lon = redDotLon;
                    qDebug() << "[DRAW-GUARDZONE] Using current redDot position for attached guardzone";
                } else {
                    lat = ownShip.lat;
                    lon = ownShip.lon;
                    qDebug() << "[DRAW-GUARDZONE] Using ownShip position for attached guardzone";
                }
                
                // PERBAIKAN: Pastikan posisi valid untuk attached guardzone
                if (qIsNaN(lat) || qIsNaN(lon) || (lat == 0.0 && lon == 0.0)) {
                    lat = gz.centerLat;  // Fallback ke stored position
                    lon = gz.centerLon;
                    qDebug() << "[DRAW-GUARDZONE] Using stored position instead of invalid position";
                }
                
                qDebug() << "[DRAW-GUARDZONE] Drawing attached guardzone" << gz.id 
                         << "at position:" << lat << "," << lon 
                         << "radius:" << gz.radius << "NM";
            }

            qDebug() << "[DRAW-GUARDZONE-DEBUG] Converting coordinates lat:" << lat << "lon:" << lon;
            int centerX, centerY;
            // PERBAIKAN CRITICAL: Enhanced coordinate validation untuk fullscreen
            bool conversionResult = LatLonToXy(lat, lon, centerX, centerY);
            qDebug() << "[DRAW-GUARDZONE-DEBUG] Coordinate conversion result:" << conversionResult;
            if (conversionResult) {
                qDebug() << "[DRAW-GUARDZONE-DEBUG] Screen coordinates: centerX=" << centerX << "centerY=" << centerY;
                // CRITICAL: Validate converted coordinates untuk fullscreen safety
                if (abs(centerX) > 20000 || abs(centerY) > 20000) {
                    qDebug() << "[DRAW-GUARDZONE-ERROR] Invalid screen coordinates for guardzone" << gz.id 
                             << ":" << centerX << "," << centerY << "- skipping";
                    skippedCount++;
                    continue;
                }
                
                qDebug() << "[DRAW-GUARDZONE-DEBUG] Calculating radius in pixels...";
                // PERBAIKAN: Untuk CIRCLE gunakan gz.radius, bukan outerRadius
                double circleRadiusNM = (gz.shape == GUARD_ZONE_CIRCLE) ? gz.radius : gz.outerRadius;
                double innerRadiusNM = (gz.shape == GUARD_ZONE_CIRCLE) ? 0.0 : gz.innerRadius; // Circle = solid
                qDebug() << "[DRAW-GUARDZONE-DEBUG] circleRadius NM:" << circleRadiusNM << "innerRadius NM:" << innerRadiusNM;
                double outerRadiusInPixels = calculatePixelsFromNauticalMiles(circleRadiusNM);
                double innerRadiusInPixels = calculatePixelsFromNauticalMiles(innerRadiusNM);
                qDebug() << "[DRAW-GUARDZONE-DEBUG] outerRadius pixels:" << outerRadiusInPixels << "innerRadius pixels:" << innerRadiusInPixels;
                
                // PERBAIKAN CRITICAL: Validate radius calculations
                if (qIsNaN(outerRadiusInPixels) || qIsInf(outerRadiusInPixels) ||
                    qIsNaN(innerRadiusInPixels) || qIsInf(innerRadiusInPixels)) {
                    qDebug() << "[DRAW-GUARDZONE-ERROR] Invalid radius values for guardzone" << gz.id << "- skipping";
                    skippedCount++;
                    continue;
                }

                // TAMBAHAN: Clamp radius untuk performance
                if (outerRadiusInPixels > 5000) outerRadiusInPixels = 5000;
                if (outerRadiusInPixels < 1) outerRadiusInPixels = 1;
                if (innerRadiusInPixels < 0) innerRadiusInPixels = 0;

                // Check if this is a semicircle shield (has angles)
                // PERBAIKAN: AttachedToShip tetap semicircle, create circular guardzone jadi full circle
                bool isSemicircleShield = (gz.attachedToShip) || 
                                         (gz.shape != GUARD_ZONE_CIRCLE && 
                                          gz.startAngle != gz.endAngle && 
                                          gz.startAngle >= 0 && gz.endAngle >= 0);

                qDebug() << "[DRAW-GUARDZONE-DEBUG] Starting QPainter operations for guardzone" << gz.id;
                qDebug() << "[DRAW-GUARDZONE-DEBUG] isSemicircleShield:" << isSemicircleShield;
                
                // PERBAIKAN CRITICAL: Wrap all QPainter operations in try-catch
                try {
                    if (isSemicircleShield) {
                        qDebug() << "[DRAW-GUARDZONE-DEBUG] Drawing semicircle shield...";
                        // Create semicircle shield using sector rendering
                        QPainterPath semicirclePath;
                        
                        // Convert angles from navigation (0=North, clockwise) to Qt (0=East, counterclockwise)
                        double qtStartAngle = (90 - gz.startAngle + 360) * 16;    // Qt uses 16th of degree
                        double qtEndAngle = (90 - gz.endAngle + 360) * 16;
                        double qtSpanAngle = (qtEndAngle - qtStartAngle);
                        
                        // Normalize angles
                        qtStartAngle = fmod(qtStartAngle, 360 * 16);
                        if (qtSpanAngle < 0) qtSpanAngle += 360 * 16;
                        if (qtSpanAngle > 360 * 16) qtSpanAngle -= 360 * 16;
                        
                        double startAngleQt = qtStartAngle / 16.0;
                        double spanAngleQt = qtSpanAngle / 16.0;
                        
                        // CRITICAL: Validate rectangle coordinates
                        double rectX = centerX - outerRadiusInPixels;
                        double rectY = centerY - outerRadiusInPixels;
                        double rectSize = 2 * outerRadiusInPixels;
                        
                        if (qIsNaN(rectX) || qIsInf(rectX) || qIsNaN(rectY) || qIsInf(rectY) ||
                            qIsNaN(rectSize) || qIsInf(rectSize) || rectSize <= 0) {
                            qDebug() << "[DRAW-GUARDZONE-ERROR] Invalid rectangle values - skipping semicircle";
                            continue;
                        }
                        
                        QRectF outerRect(rectX, rectY, rectSize, rectSize);
                        
                        // Start from center
                        semicirclePath.moveTo(centerX, centerY);
                        
                        // Draw arc
                        semicirclePath.arcTo(outerRect, startAngleQt, spanAngleQt);
                        
                        // Close back to center
                        semicirclePath.closeSubpath();
                        
                        painter.drawPath(semicirclePath);
                        
                        qDebug() << "[DRAW-SUCCESS] Successfully drew semicircle shield guardzone" << gz.id;
                    } else {
                        qDebug() << "[DRAW-GUARDZONE-DEBUG] Drawing circle/donut shape...";
                        // Create full circle or donut shape using QPainterPath
                        QPainterPath circlePath;
                        
                        // CRITICAL: Validate ellipse parameters
                        QPointF centerPoint(centerX, centerY);
                        if (qIsNaN(centerPoint.x()) || qIsInf(centerPoint.x()) ||
                            qIsNaN(centerPoint.y()) || qIsInf(centerPoint.y())) {
                            qDebug() << "[DRAW-GUARDZONE-ERROR] Invalid center point - skipping circle";
                            continue;
                        }
                        
                        qDebug() << "[DRAW-GUARDZONE-DEBUG] Adding outer ellipse - center:" << centerPoint << "radius:" << outerRadiusInPixels;
                        circlePath.addEllipse(centerPoint, outerRadiusInPixels, outerRadiusInPixels);
                        qDebug() << "[DRAW-GUARDZONE-DEBUG] Outer ellipse added successfully";
                        
                        if (innerRadiusInPixels > 0) {
                            qDebug() << "[DRAW-GUARDZONE-DEBUG] Adding inner ellipse - radius:" << innerRadiusInPixels;
                            circlePath.addEllipse(centerPoint, innerRadiusInPixels, innerRadiusInPixels);
                            qDebug() << "[DRAW-GUARDZONE-DEBUG] Inner ellipse added successfully";
                        }

                        qDebug() << "[DRAW-GUARDZONE-DEBUG] Drawing path...";
                        painter.drawPath(circlePath);
                        qDebug() << "[DRAW-GUARDZONE-DEBUG] Path drawn successfully";
                        
                        qDebug() << "[DRAW-SUCCESS] Successfully drew" << (innerRadiusInPixels > 0 ? "donut" : "circle") << "guardzone" << gz.id;
                    }

                    labelX = centerX;
                    labelY = centerY - static_cast<int>(outerRadiusInPixels) - 15;
                    drawnCount++;
                    
                    qDebug() << "[DRAW-SUCCESS] Successfully drew donut guardzone" << gz.id 
                             << "at screen coords:" << centerX << "," << centerY 
                             << "outer radius:" << outerRadiusInPixels << "inner radius:" << innerRadiusInPixels << "pixels";
                } catch (const std::exception& e) {
                    qDebug() << "[DRAW-GUARDZONE-ERROR] Exception drawing circle guardzone" << gz.id << ":" << e.what();
                    skippedCount++;
                    continue;
                } catch (...) {
                    qDebug() << "[DRAW-GUARDZONE-ERROR] Unknown exception drawing circle guardzone" << gz.id;
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
    rootObject["app_version"] = "ECDIS_v1.0";
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

        // Try to get current ownship position
        if (ownShip.lat != 0.0 && ownShip.lon != 0.0) {
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
    QString cogValue = QString("%1 kn").arg(ti->cog / 10.0, 0, 'f', 2);
    QString sogValue = QString("%1 °").arg(ti->sog / 10.0, 0, 'f', 2);
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
    double rangeNM = GetRange(currentScale);

    // Jika zoom terlalu jauh, tampilkan dua lingkaran sebagai simbol ownship
    if (rangeNM > 2.0) {
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

    // Skala ikon kapal berdasarkan range
    double scaleFactor = 1.0;

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

    double rangeNM = GetRange(currentScale);  // misalnya return 6, 12, 24, 48, dst.
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
    double useLat = ownShip.lat;
    double useLon = ownShip.lon;
    
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
    
    // For attach to ship, center should follow ship position
    if (attachedGuardZone->attachedToShip) {
        centerLat = ownShip.lat;
        centerLon = ownShip.lon;
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
                    // Gunakan redDot position jika available (current), fallback ke ownShip
                    if (redDotTrackerEnabled && redDotLat != 0.0 && redDotLon != 0.0) {
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
    qDebug() << "[ROUTE] Starting route mode";
    isRouteMode = true;
    routeWaypointCounter = 1;
    // Don't increment currentRouteId here - let it be incremented when we actually create first waypoint
    qDebug() << "[ROUTE] Route mode started. Current route ID:" << currentRouteId;
}

void EcWidget::resetRouteConnections()
{
    // This function ensures that previous routes are properly terminated
    // and won't connect to the new route being created
    
    qDebug() << "[ROUTE] Resetting route connections before starting new route" << (currentRouteId + 1);
    
    // Force save current waypoints to ensure proper separation
    if (!waypointList.isEmpty()) {
        saveWaypoints();
    }
    
    // Update display to ensure clean slate for new route
    update();
}

void EcWidget::endRouteMode()
{
    qDebug() << "[ROUTE] Ending route mode";
    
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
        
        // Prepare for next route
        currentRouteId++;
        routeWaypointCounter = 1;
    }
    
    isRouteMode = false;
    activeFunction = PAN;
    
    // Update status
    if (mainWindow) {
        mainWindow->statusBar()->showMessage(tr("Route creation ended"), 3000);
        mainWindow->setWindowTitle("ECDIS AUV");
    }
    
    qDebug() << "[ROUTE] Route mode ended. Next route ID will be:" << currentRouteId;
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
        qDebug() << "[ROUTE] Using existing cell for route" << routeId;
        return routeCells[routeId];
    }
    
    // Create new cell for this route by creating a separate waypoint cell
    EcCellId newRouteCell = createNewRouteCellId();
    
    if (newRouteCell != EC_NOCELLID) {
        routeCells[routeId] = newRouteCell;
        qDebug() << "[ROUTE] Created new cell for route" << routeId;
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
        qDebug() << "[ROUTE] Successfully created new route cell";
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
    
    qDebug() << "[ROUTE] Finalized route" << currentRouteId << "with" << waypointsInCurrentRoute << "waypoints";
    
    // Force redraw to ensure proper visualization
    saveWaypoints();
    update();
}

