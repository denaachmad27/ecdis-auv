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

// Waypoint
#include "SettingsManager.h"
#include "editwaypointdialog.h"

#include <QTime>
#include <QMessageBox>
#include <QInputDialog>

// Guardzone
#include "IAisDvrPlugin.h"
#include "PluginManager.h"
#include "guardzonecheckdialog.h"
#include "guardzonemanager.h"
#include <QtMath>
#include <cmath>

int EcWidget::minScale = 100;
int EcWidget::maxScale = 50000000;

//QString ipMoos = "172.22.25.17";
//QString ipMoos = "192.168.0.180";
//QString ipMoos = "10.5.50.3";
//QString ipMoos = SettingsManager::instance().data().moosIp;

QThread* threadAIS = nullptr;
QTcpSocket* socketAIS = nullptr;
std::atomic<bool> stopThread;

QThread* threadAISMAP = nullptr;
QTcpSocket* socketAISMAP = nullptr;
std::atomic<bool> stopThreadMAP;

std::atomic<bool> stopFlag;

// define for type of AIS overlay cell
#define AISCELL_IN_RAM

#define WAYPOINTCELL_IN_RAM


EcWidget::EcWidget (EcDictInfo *dict, QString *libStr, QWidget *parent)
: QWidget  (parent)
{
  denc              = NULL;
  dictInfo          = dict;
  currentLat        = 0;
  currentLon        = 0;
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

  ownShip.lat =     currentLat;
  ownShip.lon =     currentLon;
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

  // Initialize Ship Guardian Circle
  redDotTrackerEnabled = false;
  redDotAttachedToShip = false;
  shipGuardianEnabled = false;
  redDotLat = 0.0;
  redDotLon = 0.0;
  redDotColor = QColor(255, 0, 0, 50);
  redDotSize = 8.0;
  guardianRadius = 0.5;
  guardianFillColor = QColor(255, 0, 0, 50);
  guardianBorderColor = QColor(255, 0, 0, 150);

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
  aisTooltip = nullptr;
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
  loadGuardZones();

  // Inisialisasi variabel GuardZone
  currentGuardZone = EcFeature{nullptr, 0}; // Inisialisasi dengan objek null
  guardZoneShape = GUARD_ZONE_CIRCLE;       // Default bentuk lingkaran
  guardZoneRadius = 0.5;                    // Default radius 0.5 mil laut
  guardZoneWarningLevel = EC_WARNING_LEVEL; // Default level peringatan
  guardZoneActive = false;                  // Default tidak aktif
  creatingGuardZone = false;                // Default tidak dalam mode pembuatan
  guardZoneCenter = QPointF(-1, -1);        // Posisi tidak valid
  pixelsPerNauticalMile = 100;              // Nilai default
  guardZoneCenterLat = 0.0;
  guardZoneCenterLon = 0.0;
  guardZoneRadius = 0.5;                    // Default radius 0.5 mil laut
  guardZoneWarningLevel = EC_WARNING_LEVEL; // Default level peringatan
  guardZoneActive = false;                  // Default tidak aktif
  guardZoneAttachedToShip = false;          // Default tidak terikat ke kapal

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

    for (const Waypoint &wp : waypointList)
    {
        drawSingleWaypoint(wp.lat, wp.lon, wp.label);
    }

    // Gambar garis legline antar waypoint
    if (waypointList.size() >= 2) {
        QPainter painter(&drawPixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QPen pen(QColor(255, 140, 0)); // warna oranye
        pen.setStyle(Qt::DashLine);   // garis putus-putus
        pen.setWidth(2);
        painter.setPen(pen);

        for (int i = 0; i < waypointList.size() - 1; ++i) {
            int x1, y1, x2, y2;
            if (LatLonToXy(waypointList[i].lat, waypointList[i].lon, x1, y1) &&
                LatLonToXy(waypointList[i + 1].lat, waypointList[i + 1].lon, x2, y2)) {
                painter.drawLine(x1, y1, x2, y2);
            }
        }

        painter.end();
    }

    drawLeglineLabels();

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

  drawGuardZone();

  // ========== TAMBAHAN UNTUK RED DOT TRACKER ==========
  // Draw red dot tracker at the very end
  drawRedDotTracker();
  // ==================================================
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

// Mouseevent lama
// void EcWidget::mousePressEvent(QMouseEvent *e)
// {
//   setFocus();

//   if (e->button() == Qt::LeftButton)
//   {
//     // if (activeFunction == CREATE_WAYP) {
//     //       qDebug() << "CREATE WAYP";
//     //     EcCoordinate lat, lon;
//     //     if (XyToLatLon(e->x(), e->y(), lat, lon)) {
//     //         SetWaypointPos(lat, lon);
//     //         createWaypoint();
//     //     }
//     // }
//     // else {
//     //     EcCoordinate lat, lon;
//     //     if (XyToLatLon(e->x(), e->y(), lat, lon))
//     //     {
//     //         SetCenter(lat, lon);
//     //         //draw(true);
//     //         Draw();
//     //     }
//     // }

//       // Get mouse coordinates
//       QPoint pos = e->pos();

//       // Get position of the ecchart widget within the main window
//       QPoint chartPos = ecchart->mapFromParent(pos);

//       // Check if click is within the chart area
//       if (!ecchart->rect().contains(chartPos)) {
//           // Click is outside chart area
//           QMainWindow::mousePressEvent(event);
//           return;
//       }

//       // Convert screen coordinates to lat/lon
//       EcCoordinate latPos, lonPos;
//       EcView* view = ecchart->GetView();

//       if (EcDrawXyToLatLon(view, chartPos.x(), chartPos.y(), &latPos, &lonPos)) {
//           // Handle based on active function
//           switch (activeFunction) {
//           case CREATE_WAYP: {
//               // Define PICKRADIUS based on current range
//               double range = ecchart->GetRange();
//               double PICKRADIUS = 0.03 * range;

//               // Create waypoint at clicked position
//               EcCellId udoCid = ecchart->GetUdoCellId();
//               wp1 = EcRouteAddWaypoint(udoCid, dict, latPos, lonPos,
//                                        PICKRADIUS, TURNRADIUS);

//               if (!ECOK(wp1)) {
//                   QMessageBox::critical(this, "Error", "Waypoint could not be created");
//               } else {
//                   // Update display
//                   drawUdo();
//                   ecchart->update();
//               }

//               // Reset to PAN mode
//               activeFunction = PAN;
//               showHeader();
//               statusBar()->clearMessage();
//               break;
//           }

//           // Add other cases as needed...

//           default:
//               // Pass to base class for default handling
//               QMainWindow::mousePressEvent(event);
//               break;
//           }
//       }
//   }
//   else if (e->button() == Qt::RightButton)
//   {
//       if (activeFunction == CREATE_WAYP) {
//           activeFunction = NONE;
//       }
//     pickX = e->x();
//     pickY = e->y();
//     emit mouseRightClick();
//   }
// }

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
                // Waypoint creation logic (unchanged)
                double range = GetRange(currentScale);
                double pickRadius = 0.03 * range;

                if (udoCid == EC_NOCELLID)
                {
                    if (!resetWaypointCell()) {
                        QMessageBox::warning(this, tr("Error"), tr("UDO Cell could not be created"));
                        activeFunction = PAN;
                        return;
                    }
                }
                else
                {
                    EcChartUnAssignCellFromView(view, udoCid);
                    EcCellUnmap(udoCid);
                    udoCid = EC_NOCELLID;

                    if (!createWaypointCell()) {
                        QMessageBox::warning(this, tr("Error"), tr("UDO Cell could not be created"));
                        activeFunction = PAN;
                        return;
                    }
                }

                activeFunction = PAN;
                setWindowTitle("ECDIS AUV");

                Draw();
                drawOverlayCell();
                drawWaypointMarker(lat, lon);
                emit waypointCreated();

                QList<EcFeature> pickedList;
                GetPickedFeaturesSubs(pickedList, lat, lon);
                PickWindow *pw = new PickWindow(this, dictInfo, denc);
                pw->fill(pickedList);
                pw->exec();

                update();

                activeFunction = PAN;
                emit projection();
                emit scale(currentScale);
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
        emit mouseRightClick();
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

void EcWidget::CreateWaypoint(ActiveFunction active)
{
    // if(udoCid == EC_NOCELLID){
    //     if( createUdoCell() == false )
    //     {
    //         QMessageBox::warning( this, tr( "error showroute" ), tr( "Could not create udo cell. Please restart the program." ) );
    //         return;
    //     }
    // }

    activeFunction = active;
    // qDebug() << "Active function" << activeFunction;
    // range = (int)GetRange(currentScale);
    // qDebug() << "Range: " << range;
    // double pickRadius = (0.03 * range);
    // qDebug() << "Rad" << pickRadius;
    // // add a new waypoint object to the udo cell
    // wp1 = EcRouteAddWaypoint(udoCid, dictInfo, wplat, wplon, pickRadius, 5);
    // if (!ECOK(wp1))
    //     QMessageBox::warning(this, tr ("error showroute"), tr("Waypoint could not be created"));
    // else
    // {
    //     // symbolize and redraw the udo cell
    //     drawUdo();
    //     // InvalidateRect(hWnd, NULL,0);
    // }

    createWaypointCell();
}

void EcWidget::createWaypoint()
{
    qDebug() << "Active function" << activeFunction;
    range = (int)GetRange(currentScale);
    qDebug() << "Range: " << range;
    double pickRadius = (0.03 * range);
    qDebug() << "Rad" << pickRadius;
    qDebug() << wplat << wplon;
    qDebug() << "udoCid" << udoCid;

    // add a new waypoint object to the udo cell
    wp1 = EcRouteAddWaypoint(udoCid, dictInfo, wplat, wplon, pickRadius, 5);
    if (!ECOK(wp1))
        QMessageBox::warning(this, tr ("error showroute"), tr("Waypoint could not be created"));
    else
    {
        // symbolize and redraw the udo cell
        drawWaypointCell();
        // InvalidateRect(hWnd, NULL,0);
        Draw();

    }
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
  double dWarnDist = 0.5;
  double dWarnCPA = 0.2;
  int iWarnTCPA = 10;
  int iTimeOut = 1;
  Bool bInternalGPS = False;
  Bool bAISSymbolize;
  QString strErrLogAis = QString( "%1%2%3" ).arg( QCoreApplication::applicationDirPath() ).arg( "/" ).arg( "errorAISLog.txt" );

  _aisObj = new Ais( this, view, dict, ownShip.lat, ownShip.lon,
    ownShip.sog, ownShip.cog, dWarnDist, dWarnCPA,
    iWarnTCPA, strAisLib, iTimeOut, bInternalGPS, &bAISSymbolize, strErrLogAis );

  QObject::connect( _aisObj, SIGNAL( signalRefreshChartDisplay( double, double ) ), this, SLOT( slotRefreshChartDisplay( double, double ) ) );
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

    _aisObj->setAISCell( aisCellId );
    _aisObj->readAISVariable( aisDataLines );
}

void EcWidget::startServerMOOSSubscribe() {
    QThread *thread = QThread::create([=]() { StartReadAISSubscribe(); });
    thread->start();
}

QString EcWidget::StartReadAISSubscribe ()
{
    // AIS
    if( deleteAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not remove old AIS overlay cell. Please restart the program." ) );
        return aivdo;
    }

    if( createAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not create AIS overlay cell. Please restart the program." ) );
        return aivdo;
    }

    _aisObj->setAISCell( aisCellId );


    // SERVER
    serverS = new QTcpServer();

    if (!serverS->listen(QHostAddress::Any, 8080)) {
        qDebug() << "Server failed to start.";
        return aivdo;
    }
    qDebug() << "Waiting for connection...";

    QObject::connect(serverS, &QTcpServer::newConnection, [=]() {
        clientS = serverS->nextPendingConnection();
        qDebug() << "Client connected!";

        QObject::connect(clientS, &QTcpSocket::readyRead, [=]() {
            QByteArray data = clientS->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
            QJsonObject jsonData = jsonDoc.object();

            // Pastikan JSON memiliki key yang benar
            if (jsonData.contains("NAV_LAT") && jsonData.contains("NAV_LONG")) {

                navShip.lon = jsonData["NAV_LONG"].toDouble();


                // ================ FORMAT JSON =============== //
                // qDebug().noquote() << "Received JSON:\n" << QJsonDocument(jsonData).toJson(QJsonDocument::Indented);

                // ================ FORMAT GPGGA ================ //
                //qDebug().noquote() << convertToGPGGA(navShip.lat, navShip.lon, 1, 8, 0.9, 15.0);

                // ================ FORMAT AIVDO ================ //


                // Encode AIS !AIVDO
                aivdo = AIVDOEncoder::encodeAIVDO(0, navShip.lat, navShip.lon, 0, 0);
                qDebug().noquote() << aivdo;

                QStringList nmeaData;
                nmeaData << aivdo;

                // RECORD NMEA
                IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");

                if (dvr && dvr->isRecording()) {
                    dvr->recordRawNmea(aivdo);
                }

            } else {
                qDebug().noquote() << "Received JSON:\n" << QJsonDocument(jsonData).toJson(QJsonDocument::Indented);
            }
        });

        QObject::connect(clientS, &QTcpSocket::disconnected, [=]() {
            qDebug() << "Client disconnected.";
            clientS->deleteLater();
        });
    });

    return aivdo;
}

// Thread Subscribe from MOOSDB -- SSH
QString EcWidget::StartReadAISSubscribeSSH () {
    // AIS
    if( deleteAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not remove old AIS overlay cell. Please restart the program." ) );
        return aivdo;
    }

    if( createAISCell() == false )
    {
        QMessageBox::warning( this, tr( "ReadAISLogfile" ), tr( "Could not create AIS overlay cell. Please restart the program." ) );
        return aivdo;
    }

    _aisObj->setAISCell(aisCellId);

    // SERVER
    QTcpSocket* clientS = new QTcpSocket();

    // Hubungkan ke server MOOSDB di Ubuntu (SSH)
    QString sshIP = SettingsManager::instance().data().moosIp;
    quint16 sshPort = 5000;

    clientS->connectToHost(sshIP, sshPort);

    if (!clientS->waitForConnected(sshPort)) {
        qCritical() << "Failed to connect to MOOSDB server.";
        return aivdo;
    }

    qDebug() << "Connected to MOOSDB at" << sshIP << ":" << sshPort;

    QObject::connect(clientS, &QTcpSocket::readyRead, [=]() {
        QByteArray data = clientS->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        QJsonObject jsonData = jsonDoc.object();

        // Pastikan JSON memiliki key yang benar
        if (jsonData.contains("NAV_LAT") && jsonData.contains("NAV_LONG")) {
            QJsonValue latValue = jsonData["NAV_LAT"];
            QJsonValue lonValue = jsonData["NAV_LONG"];

            if(latValue.isDouble() && lonValue.isDouble()){
                navShip.lat = jsonData["NAV_LAT"].toDouble();
                navShip.lon = jsonData["NAV_LONG"].toDouble();
            }
            else {
                navShip.lat = jsonData["NAV_LAT"].toString().toDouble();
                navShip.lon = jsonData["NAV_LONG"].toString().toDouble();
            }

            // Encode AIS !AIVDO
            aivdo = AIVDOEncoder::encodeAIVDO(0, navShip.lat, navShip.lon, 0, 0);

            QStringList nmeaData;
            nmeaData << aivdo;

            _aisObj->readAISVariable(nmeaData);

            // PUBLISH PICK INFO
        }
        else {
            qDebug() << "[ERROR] Invalid JSON received.";
        }

    });

    QObject::connect(clientS, &QTcpSocket::disconnected, [=]() {
        qDebug() << "[INFO] Disconnected from MOOSDB.";
        clientS->deleteLater();
    });

    return aivdo;
}

// Fungsi Publish PICK INFO from NAV
void EcWidget::PublishNavInfo(QTcpSocket* socket, double lat, double lon) {
    if (!socket) {
        qDebug() << "[ERROR] Socket is null.";
        return;
    }

    // Pastikan socket sudah terhubung
    if (socket->state() != QAbstractSocket::ConnectedState) {
        socket->connectToHost(SettingsManager::instance().data().moosIp, 5001);
        if (!socket->waitForConnected(3000)) {
            qDebug() << "Connection failed";
            return;
        }
    }

    QList<EcFeature> pickedFeatureList;
    GetPickedFeaturesSubs(pickedFeatureList, lat, lon);

    //PickWindow *pickWindow;  // Pakai instance, bukan pointer null
    //QJsonObject navInfo = pickWindow->fillJson(pickedFeatureList);

    QJsonObject jsonData {
        {"NAV_INFO", "navInfo"}
    };

    QJsonDocument jsonDoc(jsonData);
    QByteArray jsonString = jsonDoc.toJson();

    qDebug().noquote() << "Send: \n" << jsonDoc.toJson(QJsonDocument::Indented);

    socket->write(jsonString);
    socket->waitForBytesWritten();
    socket->waitForReadyRead();
}
// UNUSED ^^^

// WORK vvv
QString EcWidget::StartThreadSubscribeSSH(EcWidget *ecchart) {
    if (!deleteAISCell()) {
        QMessageBox::warning(this, tr("ReadAISLogfile"), tr("Could not remove old AIS overlay cell. Please restart the program."));
        return aivdo;
    }
    if (!createAISCell()) {
        QMessageBox::warning(this, tr("ReadAISLogfile"), tr("Could not create AIS overlay cell. Please restart the program."));
        return aivdo;
    }

    _aisObj->setAISCell(aisCellId);
    startAISSubscribeThread(ecchart);

    return aivdo;
}

void EcWidget::startAISSubscribeThread(EcWidget *ecchart) {
    stopThread = false;
    threadAIS = new QThread();

    QObject::connect(threadAIS, &QThread::started, [this, ecchart]() {
        // _aisObj->setOwnShipNull();
        connectToMOOSDB(ecchart);
    });

    QObject::connect(threadAIS, &QThread::finished, []() {
        qDebug() << "[INFO] AIS subscription thread finished.";
    });

    threadAIS->start();
}

void EcWidget::connectToMOOSDB(EcWidget *ecchart) {
    socketAIS = new QTcpSocket();
    socketAIS->moveToThread(QThread::currentThread());

    //qDebug() << ipMoos;

    QString sshIP = SettingsManager::instance().data().moosIp;
    quint16 sshPort = 5000;

    QObject::connect(socketAIS, &QTcpSocket::connected, []() {
        qDebug() << "[INFO] Successfully connected to MOOSDB.";
    });

    QObject::connect(socketAIS, &QTcpSocket::readyRead, [this, ecchart]() {
        processAISDataHybrid(socketAIS, ecchart);
        // Update posisi kapal
        // navShip.lat = jsonData["NAV_LAT"].toDouble();
        // navShip.lon = jsonData["NAV_LONG"].toDouble();

        // Trigger perhitungan CPA/TCPA untuk semua targets
        //_aisObj->readAISVariable(nmeaData);
    });

    QObject::connect(socketAIS, &QTcpSocket::disconnected, [this]() {
        qDebug() << "Disconnected from MOOSDB.";
        socketAIS->deleteLater();
        socketAIS = nullptr;
    });

    socketAIS->connectToHost(sshIP, sshPort);
    if (!socketAIS->waitForConnected(3000)) {
        qCritical() << "Failed to connect to MOOSDB server.";
        socketAIS->deleteLater();
        socketAIS = nullptr;

        return;
    }
    qDebug() << "Connected to MOOSDB at" << sshIP << ":" << sshPort;
}

void EcWidget::processAISData(QTcpSocket* socket) {
    QByteArray data = socket->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    QJsonObject jsonData = jsonDoc.object();

    if (jsonData.contains("NAV_LAT") && jsonData.contains("NAV_LONG")) {
        QJsonValue latValue = jsonData["NAV_LAT"];
        QJsonValue lonValue = jsonData["NAV_LONG"];

        if(latValue.isDouble() && lonValue.isDouble()){
            navShip.lat = jsonData["NAV_LAT"].toDouble();
            navShip.lon = jsonData["NAV_LONG"].toDouble();
        }
        else {
            navShip.lat = jsonData["NAV_LAT"].toString().toDouble();
            navShip.lon = jsonData["NAV_LONG"].toString().toDouble();
        }

        double lat = navShip.lat;
        double lon = navShip.lon;
        QMetaObject::invokeMethod(this, [this, lat, lon]() {
            aivdo = AIVDOEncoder::encodeAIVDO(0, lat, lon, 0, 0);

            QStringList nmeaData;
            nmeaData << aivdo;
            _aisObj->readAISVariable(nmeaData);

            QList<EcFeature> pickedFeatureList;
            GetPickedFeaturesSubs(pickedFeatureList, lat, lon);

            PickWindow *pickWindow = new PickWindow(this, dictInfo, denc);;
            QJsonObject navInfo = pickWindow->fillJsonSubs(pickedFeatureList);

            QJsonObject jsonDataOut {
                {"NAV_INFO", navInfo}
            };
            QJsonDocument jsonDocOut(jsonDataOut);
            QByteArray sendData = jsonDocOut.toJson();

            qDebug().noquote() << "[INFO] Sending Data: \n" << jsonDocOut.toJson(QJsonDocument::Indented);

            // Kirim data ke server Ubuntu (port 5001)
            QTcpSocket* sendSocket = new QTcpSocket();
            sendSocket->connectToHost(SettingsManager::instance().data().moosIp, 5001);
            if (sendSocket->waitForConnected(3000)) {
                sendSocket->write(sendData);
                sendSocket->waitForBytesWritten(3000);
                sendSocket->disconnectFromHost();
            }
            else {
                qDebug() << "[ERROR] Could not connect to data server.";
            }

            sendSocket->deleteLater();
        }, Qt::QueuedConnection);
    }
    else {
        qDebug() << "[ERROR] Invalid JSON received.";
    }
}

void EcWidget::processAISDataHybrid(QTcpSocket* socket, EcWidget *ecchart) {
    QByteArray data = socket->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    QJsonObject jsonData = jsonDoc.object();

    if (jsonData.contains("MAP_INFO_REQ")) {
        QString mapInfoString = jsonData.value("MAP_INFO_REQ").toString();

        // Split isi jadi key-value
        QStringList pairs = mapInfoString.split(",", Qt::SkipEmptyParts);
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

        //qDebug() << lat;
        //qDebug() << mapShip.lat;

        if (mapShip.lat != lat || mapShip.lon != lon){
            mapShip.lat = lat;
            mapShip.lon = lon;

            ecchart->SetCenter(lat, lon);
            ecchart->SetScale(80000);

            QApplication::setOverrideCursor(Qt::WaitCursor);
            ecchart->Draw();
            QApplication::restoreOverrideCursor();

            QMetaObject::invokeMethod(this, [this, lat, lon]() {
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

                //qDebug() << "\n";
                //qDebug().noquote() << "[INFO] SENDING DATA: \n" << jsonDocOut.toJson(QJsonDocument::Indented);
                //qDebug().noquote() << strJson;

                // Kirim data ke server Ubuntu (port 5003)
                QTcpSocket* sendSocket = new QTcpSocket();
                sendSocket->connectToHost(SettingsManager::instance().data().moosIp, 5001);
                if (sendSocket->waitForConnected(3000)) {
                    sendSocket->write(sendData);
                    sendSocket->waitForBytesWritten(3000);
                    sendSocket->disconnectFromHost();
                }
                else {
                    qDebug() << "[ERROR] Could not connect to data server.";
                }

                sendSocket->deleteLater();
            }, Qt::QueuedConnection);
        }
        // ============= ALERT SYSTEM INTEGRATION =============
        if (alertSystem && alertMonitoringEnabled) {
            alertSystem->updateOwnShipPosition(navShip.lat, navShip.lon, navShip.depth);
        }
        // ==================================================
    }

    // RECORD NMEA
    IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");

    if (jsonData.contains("NAV_LAT") && jsonData.contains("NAV_LONG")) {
        QJsonValue latValue = jsonData["NAV_LAT"];
        QJsonValue lonValue = jsonData["NAV_LONG"];

        if(latValue.isDouble() && lonValue.isDouble()){
            navShip.lat = jsonData["NAV_LAT"].toDouble();
            navShip.lon = jsonData["NAV_LONG"].toDouble();
        }
        else {
            navShip.lat = jsonData["NAV_LAT"].toString().toDouble();
            navShip.lon = jsonData["NAV_LONG"].toString().toDouble();
        }

        double lat = navShip.lat;
        double lon = navShip.lon;
        QMetaObject::invokeMethod(this, [this, lat, lon, dvr]() {
            aivdo = AIVDOEncoder::encodeAIVDO(0, lat, lon, 0, 0);

            QStringList nmeaData;
            nmeaData << aivdo;
            _aisObj->readAISVariable(nmeaData);

            if (dvr && dvr->isRecording() && !nmeaData.isEmpty()) {
                dvr->recordRawNmea(aivdo);
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
                qDebug() << "[ERROR] Could not connect to data server.";
            }

            sendSocket->deleteLater();

            // OWNSHIP PANEL
            ownShipText->setHtml(pickWindow->ownShipAutoFill());
        }, Qt::QueuedConnection);
    }

    if (jsonData.contains("WAIS_NMEA")) {
        QJsonValue aisValue = jsonData["WAIS_NMEA"];

        QString ais = jsonData["WAIS_NMEA"].toString();

        QMetaObject::invokeMethod(this, [this, ais, dvr]() {
            nmea = ais;

            QStringList nmeaData;
            nmeaData << nmea;

            if (dvr && dvr->isRecording() && !ais.isEmpty()) {
                dvr->recordRawNmea(nmea);
            }

            _aisObj->readAISVariable(nmeaData);
        }, Qt::QueuedConnection);
    }


    // BIKIN IF DI SINI UNTUK MENAMPILKAN DATA SUBS SISANYA
    if (jsonData.contains("NAV_DEPTH")) {
        QJsonValue depValue = jsonData["NAV_DEPTH"];
        if (depValue.isDouble()) {
            navShip.depth = depValue.toDouble();
        } else {
            navShip.depth = depValue.toString().toDouble();
        }
    }

    if (jsonData.contains("NAV_HEADING")) {
        QJsonValue headValue = jsonData["NAV_HEADING"];
        if (headValue.isDouble()) {
            navShip.heading = headValue.toDouble();
        } else {
            navShip.heading = headValue.toString().toDouble();
        }
    }

    if (jsonData.contains("NAV_HEADING_OVER_GROUND")) {
        QJsonValue headogValue = jsonData["NAV_HEADING_OVER_GROUND"];
        if (headogValue.isDouble()) {
            navShip.heading_og = headogValue.toDouble();
        } else {
            navShip.heading_og = headogValue.toString().toDouble();
        }
    }

    if (jsonData.contains("NAV_SPEED")) {
        QJsonValue speValue = jsonData["NAV_SPEED"];
        if (speValue.isDouble()) {
            navShip.speed = speValue.toDouble();
        } else {
            navShip.speed = speValue.toString().toDouble();
        }
    }

    if (jsonData.contains("NAV_SPEED_OVER_GROUND")) {
        QJsonValue speogValue = jsonData["NAV_SPEED_OVER_GROUND"];
        if (speogValue.isDouble()) {
            navShip.speed_og = speogValue.toDouble();
        } else {
            navShip.speed_og = speogValue.toString().toDouble();
        }
    }

    if (jsonData.contains("NAV_YAW")) {
        QJsonValue yawValue = jsonData["NAV_YAW"];
        if (yawValue.isDouble()) {
            navShip.yaw = yawValue.toDouble();
        } else {
            navShip.yaw = yawValue.toString().toDouble();
        }
    }

    if (jsonData.contains("NAV_Z")) {
        QJsonValue zValue = jsonData["NAV_Z"];
        if (zValue.isDouble()) {
            navShip.z = zValue.toDouble();
        } else {
            navShip.z = zValue.toString().toDouble();
        }
    }

    // ============= ALERT SYSTEM INTEGRATION =============
    // Update alert system with new position (TAMBAHAN BARU)
    if (alertSystem && alertMonitoringEnabled) {
        alertSystem->updateOwnShipPosition(navShip.lat, navShip.lon, navShip.depth);
    }
    // ==================================================
}

void EcWidget::stopAISSubscribeThread() {
    if (threadAIS) {
        qDebug() << "Stopping AIS subscription thread.";
        stopThread = true;

        // Jalankan abort() langsung dari threadAIS
        QMetaObject::invokeMethod(this, [this]() {
            if (socketAIS) {
                socketAIS->abort();
                socketAIS->deleteLater();
                socketAIS = nullptr;
            }
        }, Qt::QueuedConnection);

        threadAIS->quit();
        if (!threadAIS->wait(3000)) {
            qWarning() << "Thread did not quit in time, terminating.";
            threadAIS->terminate();
            threadAIS->wait();
        }
        delete threadAIS;
        threadAIS = nullptr;
        _aisObj->deleteObject();
        _aisObj->setOwnShipNull();

        ownShipText->setHtml("");
    }
}

void EcWidget::targetAISDrawTrigger() {
    qDebug() << _aisObj->getAISCell();
}

void EcWidget::jsonExample(){
    // Ambil latitude dan longitude
    double lat = -7.189417;
    double lon = 112.761347;

    QList<EcFeature> pickedFeatureList;
    GetPickedFeaturesSubs(pickedFeatureList, lat, lon);

    PickWindow *pickWindow = new PickWindow(this, dictInfo, denc);;
    QJsonObject navInfo = pickWindow->fillJsonSubs(pickedFeatureList);

    QJsonObject jsonDataOut {
        {"MAP_INFO", navInfo}
    };
    QJsonDocument jsonDocOut(jsonDataOut);
    QByteArray sendData = jsonDocOut.toJson();

    qDebug().noquote() << jsonDocOut.toJson(QJsonDocument::Indented);
}

// Read an AIS from MOOSDB -- MAP INFO
////////////////////////////////////////////////////////////////////////
void EcWidget::startAISSubscribeThreadMAP(EcWidget *ecchart) {
    stopThreadMAP = false;
    threadAISMAP = new QThread();

    QObject::connect(threadAISMAP, &QThread::started, [this, ecchart]() {
        connectToMOOSDBMAP(ecchart);
    });

    QObject::connect(threadAISMAP, &QThread::finished, []() {
        qDebug() << "AIS subscription thread finished.";
    });

    threadAISMAP->start();
}

void EcWidget::connectToMOOSDBMAP(EcWidget *ecchart) {
    socketAISMAP = new QTcpSocket();
    //socketAISMAP->moveToThread(QThread::currentThread());
    QString sshIP = SettingsManager::instance().data().moosIp;
    quint16 sshPort = 5002;

    QObject::connect(socketAISMAP, &QTcpSocket::connected, []() {
        qDebug() << "Successfully connected to MOOSDB.";
    });

    QObject::connect(socketAISMAP, &QTcpSocket::readyRead, [this, ecchart]() {
        processDataMAP(socketAISMAP, ecchart);
    });

    QObject::connect(socketAISMAP, &QTcpSocket::disconnected, [this]() {
        qDebug() << "Disconnected from MOOSDB.";
        socketAISMAP->deleteLater();
        socketAISMAP = nullptr;
    });

    socketAISMAP->connectToHost(sshIP, sshPort);
    if (!socketAISMAP->waitForConnected(3000)) {
        qCritical() << "Failed to connect to MOOSDB server.";
        socketAISMAP->deleteLater();
        socketAISMAP = nullptr;
        return;
    }
    qDebug() << "Connected to MOOSDB at" << sshIP << ":" << sshPort;
}

void EcWidget::processDataMAP(QTcpSocket* socket, EcWidget *ecchart) {
    QByteArray data = socket->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    QJsonObject jsonData = jsonDoc.object();

    if (jsonData.contains("MAP_INFO_REQ")) {
        QString mapInfoString = jsonData.value("MAP_INFO_REQ").toString();

        // Split isi jadi key-value
        QStringList pairs = mapInfoString.split(",", Qt::SkipEmptyParts);
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

        ecchart->SetCenter(lat, lon);
        ecchart->SetScale(80000);

        QApplication::setOverrideCursor(Qt::WaitCursor);
        ecchart->Draw();
        QApplication::restoreOverrideCursor();

        QMetaObject::invokeMethod(this, [this, lat, lon]() {
            QList<EcFeature> pickedFeatureList;
            GetPickedFeaturesSubs(pickedFeatureList, lat, lon);

            PickWindow *pickWindow = new PickWindow(this, dictInfo, denc);;
            QJsonObject navInfo = pickWindow->fillJsonSubs(pickedFeatureList);

            QJsonObject jsonDataOut {
                {"MAP_INFO", navInfo}
            };
            QJsonDocument jsonDocOut(jsonDataOut);
            QByteArray sendData = jsonDocOut.toJson();

            qDebug().noquote() << "Sending Data: \n" << jsonDocOut.toJson(QJsonDocument::Indented);

            // Kirim data ke server Ubuntu (port 5003)
            QTcpSocket* sendSocket = new QTcpSocket();
            sendSocket->connectToHost(SettingsManager::instance().data().moosIp, 5003);

            if (sendSocket->waitForConnected(3000)) {
                sendSocket->write(sendData);
                sendSocket->waitForBytesWritten(3000);
                sendSocket->disconnectFromHost();
            }
            else {
                qCritical() << "Could not connect to data server.";
            }

            sendSocket->deleteLater();
        }, Qt::QueuedConnection);
    }
    else {
        qCritical() << "Invalid JSON received.";
    }
}

void EcWidget::stopAISSubscribeThreadMAP() {
    if (threadAIS) {
        qDebug() << "[INFO] Stopping AIS subscription thread.";
        stopThread = true;

        // Jalankan abort() langsung dari threadAIS
        QMetaObject::invokeMethod(this, [this]() {
            if (socketAIS) {
                socketAIS->abort();
                socketAIS->deleteLater();
                socketAIS = nullptr;
            }
        }, Qt::QueuedConnection);

        threadAIS->quit();
        if (!threadAIS->wait(3000)) {
            qDebug() << "[WARN] Thread did not quit in time, terminating.";
            threadAIS->terminate();
            threadAIS->wait();
        }
        delete threadAIS;
        threadAIS = nullptr;
    }
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
  update();

  emit projection();
  emit scale( currentScale );
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
void EcWidget::slotRefreshChartDisplay( double lat, double lon )
{
  qDebug() << "slotRefreshChartDisplay called with lat:" << lat << "lon:" << lon;

  if(showAIS)
  {
    // check if center position is out of defined frame, requires drawing of chart as well
    double maxDist = GetRange(currentScale) / 60 / 2;

    if((lat != 0 && lon != 0) && (fabs(currentLat - lat) > maxDist || fabs(currentLon - lon) > maxDist))
    {
      SetCenter( lat, lon );
      draw(true);
    }
    slotUpdateAISTargets( true );
  }

  // ========== TAMBAHAN UNTUK RED DOT TRACKER ==========
  // Update red dot position if attached to ship
  if (redDotAttachedToShip && redDotTrackerEnabled) {
      qDebug() << "Updating red dot position to:" << lat << "," << lon;
      updateRedDotPosition(lat, lon);
      update(); // Force widget repaint
  }
  // ==================================================
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


void EcWidget::drawSingleWaypoint(double lat, double lon, const QString& label)
{
    int x, y;

    if (!LatLonToXy(lat, lon, x, y))
        return;

    QPainter painter(&drawPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(QColor(255, 140, 0)); // Orange
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
            if (qAbs(x - wx) <= 10 && qAbs(y - wy) <= 10) // Radius klik 10 pixel
            {
                waypointList.removeAt(i);
                saveWaypoints(); // Save waypoint baru
                Draw(); // Redraw semua waypoint
                qDebug() << "[DEBUG] Waypoint removed.";
                return;
            }
        }
    }

    qDebug() << "[DEBUG] No waypoint clicked.";
}

void EcWidget::moveWaypointAt(int x, int y)
{
    if (moveSelectedIndex == -1)
    {
        // Klik pertama: cari waypoint yang mau dipindah
        for (int i = 0; i < waypointList.size(); ++i)
        {
            int wx, wy;
            if (LatLonToXy(waypointList[i].lat, waypointList[i].lon, wx, wy))
            {
                if (qAbs(x - wx) <= 10 && qAbs(y - wy) <= 10)
                {
                    moveSelectedIndex = i;
                    QMessageBox::information(this, "Info", "Set new position of waypoint");
                    qDebug() << "[DEBUG] Waypoint selected to move: index " << i;
                    return;
                }
            }
        }
        qDebug() << "[DEBUG] No waypoint selected.";
    }
    else
    {
        // Klik kedua: geser waypoint ke posisi baru
        EcCoordinate newLat, newLon;
        if (XyToLatLon(x, y, newLat, newLon))
        {
            waypointList[moveSelectedIndex].lat = newLat;
            waypointList[moveSelectedIndex].lon = newLon;
            saveWaypoints();
            Draw();

            // 🎯 Tampilkan Pick Report setelah pindah waypoint
            QList<EcFeature> pickedList;
            GetPickedFeaturesSubs(pickedList, newLat, newLon);
            PickWindow *pw = new PickWindow(this, dictInfo, denc);
            pw->fill(pickedList);
            pw->exec();

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
    painter.setPen(QColor(255, 140, 0)); // Orange (sama seperti waypoint)
    painter.setFont(QFont("Arial", 8, QFont::Bold));

    double defaultSpeed = 10.0; // knot (bisa kamu ubah sesuai kebutuhan)

    for (int i = 0; i < waypointList.size() - 1; ++i)
    {
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

void EcWidget::loadWaypoints()
{
    QString filePath = getWaypointFilePath();
    QFile file(filePath);

    // Cek lokasi utama
    if (!file.exists())
    {
        // Coba lokasi cadangan
        QFile fallbackFile("waypoints.json");
        if (fallbackFile.exists()) {
            file.setFileName("waypoints.json");
            filePath = "waypoints.json";
        } else {
            qDebug() << "[INFO] No waypoints file found at" << filePath << "or in current directory";
            return;
        }
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QByteArray jsonData = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            qDebug() << "[ERROR] JSON parse error at" << parseError.offset << ":" << parseError.errorString();
            return;
        }

        if (!jsonDoc.isObject())
        {
            qDebug() << "[ERROR] Invalid waypoints JSON file structure (not an object)";
            return;
        }

        QJsonObject rootObject = jsonDoc.object();
        if (!rootObject.contains("waypoints") || !rootObject["waypoints"].isArray())
        {
            qDebug() << "[ERROR] Invalid waypoints JSON file (missing waypoints array)";
            return;
        }

        QJsonArray waypointArray = rootObject["waypoints"].toArray();

        waypointList.clear();

        int validWaypoints = 0;

        for (const QJsonValue &value : waypointArray)
        {
            if (!value.isObject())
                continue;

            QJsonObject wpObject = value.toObject();

            // Cek field yang diperlukan
            if (!wpObject.contains("lat") || !wpObject.contains("lon"))
                continue;

            Waypoint wp;
            wp.label = wpObject.contains("label") ? wpObject["label"].toString() :
                           QString("WP%1").arg(validWaypoints + 1, 3, 10, QChar('0'));
            wp.lat = wpObject["lat"].toDouble();
            wp.lon = wpObject["lon"].toDouble();
            wp.remark = wpObject.contains("remark") ? wpObject["remark"].toString() : "";
            wp.turningRadius = wpObject.contains("turningRadius") ? wpObject["turningRadius"].toDouble() : 10.0;
            wp.active = wpObject.contains("active") ? wpObject["active"].toBool() : true;

            waypointList.append(wp);
            validWaypoints++;
        }

        qDebug() << "[INFO] Loaded" << validWaypoints << "waypoints from" << filePath;

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
        waypointList.clear();
        saveWaypoints(); // Save empty list to file
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
    // TAMBAHAN: Performance timer
    QElapsedTimer timer;
    timer.start();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // TAMBAHAN: Early exit jika tidak ada guardzone
    if (guardZones.isEmpty()) {
        if (creatingGuardZone) {
            // Tetap gambar preview creation
            drawGuardZoneCreationPreview(painter);
        }
        return;
    }

    int drawnCount = 0;
    int skippedCount = 0;

    // TAMBAHAN: Viewport culling untuk performance
    QRect viewport = rect();

    for (const GuardZone &gz : guardZones) {
        if (!gz.active && !creatingGuardZone) {
            skippedCount++;
            continue;
        }

        // TAMBAHAN: Viewport culling check
        if (!isGuardZoneInViewport(gz, viewport)) {
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

            if (gz.attachedToShip) {
                lat = ownShip.lat;
                lon = ownShip.lon;
            }

            int centerX, centerY;
            if (LatLonToXy(lat, lon, centerX, centerY)) {
                double radiusInPixels = calculatePixelsFromNauticalMiles(gz.radius);

                // TAMBAHAN: Clamp radius untuk performance
                if (radiusInPixels > 5000) radiusInPixels = 5000;
                if (radiusInPixels < 1) radiusInPixels = 1;

                painter.drawEllipse(QPoint(centerX, centerY),
                                    static_cast<int>(radiusInPixels),
                                    static_cast<int>(radiusInPixels));

                labelX = centerX;
                labelY = centerY - static_cast<int>(radiusInPixels) - 15;
                drawnCount++;
            }
        }
        else if (gz.shape == GUARD_ZONE_POLYGON && gz.latLons.size() >= 6) {
            QPolygon poly;
            bool validPolygon = true;

            for (int i = 0; i < gz.latLons.size(); i += 2) {
                int x, y;
                if (LatLonToXy(gz.latLons[i], gz.latLons[i+1], x, y)) {
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
        }

        // Draw label jika ada posisi valid
        if (labelX != 0 && labelY != 0) {
            drawGuardZoneLabel(painter, gz, QPoint(labelX, labelY));
        }
    }

    // Draw creation preview
    if (creatingGuardZone) {
        drawGuardZoneCreationPreview(painter);
    }

    // Draw edit overlay
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        guardZoneManager->drawEditOverlay(painter);
    }

    // Draw feedback overlay
    if (feedbackTimer.isActive()) {
        drawFeedbackOverlay(painter);
    }

    painter.end();

    // TAMBAHAN: Performance logging
    qint64 elapsed = timer.elapsed();
    if (elapsed > 50) {  // Log jika lebih dari 50ms
        qDebug() << "[PERF] drawGuardZone took" << elapsed << "ms"
                 << "- Drawn:" << drawnCount << "Skipped:" << skippedCount;
    }
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
    if (!guardZoneActive || simulatedTargets.isEmpty())
        return false;

    // ========== PERBAIKAN: GUNAKAN GUARDZONE AKTIF ==========
    GuardZone* activeGuardZone = nullptr;

    // Cari guardzone aktif yang sama seperti di simulasi
    for (GuardZone& gz : guardZones) {
        if (gz.active) {
            activeGuardZone = &gz;
            break;
        }
    }

    // Jika tidak ada guardzone aktif, return false
    if (!activeGuardZone) {
        qDebug() << "[AUTO-CHECK] No active GuardZone found for checking";
        return false;
    }
    // ======================================================

    bool foundNewDanger = false;
    QString alertMessages;
    int alertCount = 0;

    for (int i = 0; i < simulatedTargets.size(); ++i) {
        bool inGuardZone = false;

        // ========== GUNAKAN DATA DARI GUARDZONE AKTIF ==========
        if (activeGuardZone->shape == GUARD_ZONE_CIRCLE) {
            // Hitung jarak ke pusat guardzone aktif
            double distance, bearing;
            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                   activeGuardZone->centerLat,
                                                   activeGuardZone->centerLon,
                                                   simulatedTargets[i].lat,
                                                   simulatedTargets[i].lon,
                                                   &distance, &bearing);

            // Jika jarak kurang dari radius guardzone aktif, target ada di dalam
            inGuardZone = (distance <= activeGuardZone->radius);

            qDebug() << "[AUTO-CHECK] Target" << simulatedTargets[i].mmsi
                     << "distance:" << distance << "vs radius:" << activeGuardZone->radius
                     << "inZone:" << inGuardZone;
        }
        else if (activeGuardZone->shape == GUARD_ZONE_POLYGON && activeGuardZone->latLons.size() >= 6) {
            // Konversi titik ke koordinat layar
            int targetX, targetY;
            if (LatLonToXy(simulatedTargets[i].lat, simulatedTargets[i].lon, targetX, targetY)) {
                QPolygon poly;
                for (int j = 0; j < activeGuardZone->latLons.size(); j += 2) {
                    int x, y;
                    LatLonToXy(activeGuardZone->latLons[j], activeGuardZone->latLons[j+1], x, y);
                    poly.append(QPoint(x, y));
                }

                inGuardZone = poly.containsPoint(QPoint(targetX, targetY), Qt::OddEvenFill);

                qDebug() << "[AUTO-CHECK] Target" << simulatedTargets[i].mmsi
                         << "polygon check - inZone:" << inGuardZone;
            }
        }
        // =====================================================

        // Jika target baru masuk guardzone, tandai sebagai berbahaya dan tambahkan ke pesan
        if (inGuardZone && !simulatedTargets[i].dangerous) {
            foundNewDanger = true;
            simulatedTargets[i].dangerous = true;

            alertMessages += tr("Target %1: Speed %2 knots, Course %3°\n")
                                 .arg(simulatedTargets[i].mmsi)
                                 .arg(simulatedTargets[i].sog, 0, 'f', 1)
                                 .arg(simulatedTargets[i].cog, 0, 'f', 0);
            alertCount++;

            qDebug() << "[AUTO-CHECK] ⚠️ NEW DANGER:" << simulatedTargets[i].mmsi
                     << "entered GuardZone" << activeGuardZone->name;
        } else if (!inGuardZone) {
            // Jika target keluar dari guardzone, tandai sebagai tidak berbahaya
            if (simulatedTargets[i].dangerous) {
                qDebug() << "[AUTO-CHECK] ✅ Target" << simulatedTargets[i].mmsi
                         << "left GuardZone" << activeGuardZone->name;
            }
            simulatedTargets[i].dangerous = false;
        }
    }

    // ============= ALERT SYSTEM INTEGRATION =============
    // Hanya tampilkan peringatan jika ada target baru yang masuk
    if (foundNewDanger && alertCount > 0) {

        // ===== ENHANCED ALERT MESSAGE =====
        QString title = tr("Guard Zone Alert - %1 Target(s) in %2").arg(alertCount).arg(activeGuardZone->name);

        // ===== ORIGINAL QMessageBox =====
        QMessageBox::warning(this, title, alertMessages);

        // ===== NEW ALERT SYSTEM INTEGRATION =====
        if (alertSystem) {
            // Trigger alert through alert system with guardzone info
            QString alertDetails = tr("%1 target(s) detected in GuardZone '%2':\n%3")
                                       .arg(alertCount)
                                       .arg(activeGuardZone->name)
                                       .arg(alertMessages.trimmed());
            triggerGuardZoneAlert(activeGuardZone->id, alertDetails);
        }
        // ====================================
    }
    // ==================================================

    // Redraw untuk memperbarui warna target
    drawSimulatedTargets();

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

    // Add to guardzone list
    guardZones.append(newGuardZone);

    // PERBAIKAN: Set system level flag hanya jika belum aktif
    if (!guardZoneActive) {
        guardZoneActive = true;
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

    // Add to guardzone list
    guardZones.append(newGuardZone);

    // PERBAIKAN: Set system level flag hanya jika belum aktif
    if (!guardZoneActive) {
        guardZoneActive = true;
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
            gzObject["radius"] = gz.radius;
        } else if (gz.shape == GUARD_ZONE_POLYGON) {
            QJsonArray latLonArray;
            for (double coord : gz.latLons) {
                latLonArray.append(coord);
            }
            gzObject["latLons"] = latLonArray;
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

        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qDebug() << "[ERROR] JSON parse error at" << parseError.offset << ":" << parseError.errorString();
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

            GuardZone gz;
            gz.id = gzObject.contains("id") ? gzObject["id"].toInt() : getNextGuardZoneId();
            gz.name = gzObject.contains("name") ? gzObject["name"].toString() :
                          QString("GuardZone_%1").arg(gz.id);
            gz.shape = static_cast<::GuardZoneShape>(gzObject["shape"].toInt());

            // PERBAIKAN: Preserve active status dari file
            gz.active = gzObject.contains("active") ? gzObject["active"].toBool() : true;

            gz.attachedToShip = gzObject.contains("attachedToShip") ?
                                    gzObject["attachedToShip"].toBool() : false;

            QString colorStr = gzObject.contains("color") ? gzObject["color"].toString() : "#ff0000";
            gz.color = QColor(colorStr);
            if (!gz.color.isValid()) gz.color = Qt::red;

            // Load shape-specific data
            bool shapeDataValid = false;

            if (gz.shape == GUARD_ZONE_CIRCLE) {
                if (gzObject.contains("centerLat") && gzObject.contains("centerLon") &&
                    gzObject.contains("radius")) {

                    gz.centerLat = gzObject["centerLat"].toDouble();
                    gz.centerLon = gzObject["centerLon"].toDouble();
                    gz.radius = gzObject["radius"].toDouble();

                    // Validate circle data
                    if (gz.centerLat >= -90 && gz.centerLat <= 90 &&
                        gz.centerLon >= -180 && gz.centerLon <= 180 &&
                        gz.radius > 0 && gz.radius <= 100) {
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
        } else {
            guardZoneActive = false;
        }

        qDebug() << "[INFO] GuardZone system state - Active:" << guardZoneActive
                 << "NextID:" << nextGuardZoneId;

        // PERBAIKAN: Trigger update hanya jika ada data
        if (!guardZones.isEmpty()) {
            update();
        }
    } else {
        qDebug() << "[ERROR] Failed to open guardzones file:" << file.errorString();
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
        double lat = gz.attachedToShip ? ownShip.lat : gz.centerLat;
        double lon = gz.attachedToShip ? ownShip.lon : gz.centerLon;

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
            if (gz.radius <= 0 || gz.radius > 100) {
                issues << QString("GuardZone %1 has invalid radius").arg(gz.id);
                allValid = false;
            }
        } else if (gz.shape == GUARD_ZONE_POLYGON) {
            if (gz.latLons.size() < 6 || gz.latLons.size() % 2 != 0) {
                issues << QString("GuardZone %1 has invalid polygon data").arg(gz.id);
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
    qDebug() << "setRedDotAttachedToShip called with:" << attached;

    redDotAttachedToShip = attached;

    if (attached) {
        // Auto-enable tracker when attaching
        redDotTrackerEnabled = true;
        qDebug() << "Red dot tracker auto-enabled";

        // Try to get current ownship position
        if (ownShip.lat != 0.0 && ownShip.lon != 0.0) {
            qDebug() << "Using ownShip position:" << ownShip.lat << "," << ownShip.lon;
            updateRedDotPosition(ownShip.lat, ownShip.lon);
        } else {
            qDebug() << "OwnShip position is zero, waiting for AIS update";
        }
    } else {
        qDebug() << "Red dot detached from ship";
    }

    update(); // Force repaint
}

bool EcWidget::isRedDotTrackerEnabled() const
{
    return redDotTrackerEnabled;
}

bool EcWidget::isRedDotAttachedToShip() const
{
    return redDotAttachedToShip;
}

void EcWidget::updateRedDotPosition(double lat, double lon)
{
    if (!redDotTrackerEnabled || !redDotAttachedToShip) {
        return;
    }

    redDotLat = lat;
    redDotLon = lon;

    qDebug() << "Red Dot position updated to:" << lat << "," << lon;
}

void EcWidget::drawRedDotTracker()
{
    // Check if tracker is enabled and position is valid
    if (!redDotTrackerEnabled || redDotLat == 0.0 || redDotLon == 0.0) {
        return;
    }

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

    // MODIFIKASI UTAMA: Draw filled circle area (seperti guardzone)
    painter.setBrush(QBrush(fillColor));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(screenX, screenY),
                       (int)pixelRadius,
                       (int)pixelRadius);

    // Draw border circle (seperti guardzone border)
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(borderColor, 2));
    painter.drawEllipse(QPoint(screenX, screenY),
                       (int)pixelRadius,
                       (int)pixelRadius);

    // Draw center dot untuk menandai posisi ship yang tepat
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
    redDotTrackerEnabled = enabled;  // Keep compatibility
    qDebug() << "Ship Guardian Circle enabled:" << enabled;

    if (!enabled) {
        redDotLat = 0.0;
        redDotLon = 0.0;
    }

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

    int centerX, centerY;
    if (!LatLonToXy(redDotLat, redDotLon, centerX, centerY)) {
        return;
    }

    // Convert nautical miles to pixels
    double radiusInPixels = calculatePixelsFromNauticalMiles(guardianRadius);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw filled circle (guardian area)
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

// Implementasi fungsi createAISTooltip
void EcWidget::createAISTooltip()
{
    // Create tooltip frame
    aisTooltip = new QFrame(this);
    aisTooltip->setFrameStyle(QFrame::NoFrame); // Hilangkan frame
    aisTooltip->setStyleSheet(
        "QFrame {"
        "background-color: rgba(248, 248, 248, 240);" // Semi-transparent background
        "border: 1px solid #cccccc;"                   // Subtle border
        "border-radius: 6px;"
        "padding: 8px;"
        "box-shadow: 0px 3px 10px rgba(0,0,0,0.2);"
        "}"
        "QLabel {"
        "background-color: transparent;"               // Transparent background
        "border: none;"                               // Hilangkan border biru
        "color: #333333;"
        "font-family: 'Segoe UI', Arial, sans-serif;"
        "font-size: 11px;"
        "font-weight: normal;"
        "margin: 2px 0px;"
        "padding: 2px 4px;"
        "}"
    );

    aisTooltip->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    aisTooltip->setAttribute(Qt::WA_ShowWithoutActivating);
    aisTooltip->hide();

    // Create main layout dengan spacing yang lebih rapi
    QVBoxLayout* mainLayout = new QVBoxLayout(aisTooltip);
    mainLayout->setSpacing(1);  // Kurangi spacing
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Create labels dengan styling yang clean
    tooltipObjectName = new QLabel("Object name: ", aisTooltip);
    tooltipShipBreadth = new QLabel("Ship breadth (beam): ", aisTooltip);
    tooltipShipLength = new QLabel("Ship length over all: ", aisTooltip);
    tooltipCOG = new QLabel("COG: ", aisTooltip);
    tooltipSOG = new QLabel("SOG: ", aisTooltip);
    tooltipShipDraft = new QLabel("Ship Draft: ", aisTooltip);
    tooltipTypeOfShip = new QLabel("Type of Ship: ", aisTooltip);
    tooltipNavStatus = new QLabel("Nav Status: ", aisTooltip);
    tooltipMMSI = new QLabel("MMSI: ", aisTooltip);
    tooltipCallSign = new QLabel("Ship Call Sign: ", aisTooltip);
    tooltipPositionSensor = new QLabel("Position Sensor Indication: ", aisTooltip);
    tooltipTrackStatus = new QLabel("Track Status: ", aisTooltip);
    tooltipListOfPorts = new QLabel("List of Ports: ", aisTooltip);
    tooltipAntennaLocation = new QLabel("Antenna Location: ", aisTooltip);

    // Set properties untuk semua label
    QList<QLabel*> labels = {tooltipObjectName, tooltipShipBreadth, tooltipShipLength,
                            tooltipCOG, tooltipSOG, tooltipShipDraft, tooltipTypeOfShip,
                            tooltipNavStatus, tooltipMMSI, tooltipCallSign, tooltipPositionSensor,
                            tooltipTrackStatus, tooltipListOfPorts, tooltipAntennaLocation};

    for (QLabel* label : labels) {
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setWordWrap(false);
        label->setTextInteractionFlags(Qt::NoTextInteraction);

        // Style individual label agar clean
        label->setStyleSheet(
            "QLabel {"
            "background-color: transparent;"
            "border: none;"
            "padding: 1px 2px;"
            "margin: 0px;"
            "}"
        );

        mainLayout->addWidget(label);
    }

    aisTooltip->setLayout(mainLayout);

    // Set fixed width untuk konsistensi
    aisTooltip->setFixedWidth(250);
}

// Implementasi fungsi updateAISTooltipContent
void EcWidget::updateAISTooltipContent(EcAISTargetInfo* ti)
{
    if (!aisTooltip || !ti) return;

    // Gunakan field yang benar dari EcAISTargetInfo
    QString objectName = QString(ti->shipName).trimmed();
    if (objectName.isEmpty()) objectName = QString::number(ti->mmsi);

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

    double lat = ((double)ti->latitude / 10000.0) / 60.0;
    double lon = ((double)ti->longitude / 10000.0) / 60.0;
    QString antennaLocation = QString("%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);

    // Update semua label
    tooltipObjectName->setText(QString("Object name: %1").arg(objectName));
    tooltipShipBreadth->setText(QString("Ship breadth (beam): %1").arg(shipBreadth));
    tooltipShipLength->setText(QString("Ship length over all: %1").arg(shipLength));
    tooltipCOG->setText(QString("COG: %1").arg(cogValue));
    tooltipSOG->setText(QString("SOG: %1").arg(sogValue));
    tooltipShipDraft->setText(QString("Ship Draft: %1").arg(shipDraft));
    tooltipTypeOfShip->setText(QString("Type of Ship: %1").arg(typeOfShip));
    tooltipNavStatus->setText(QString("Nav Status: %1").arg(navStatus));
    tooltipMMSI->setText(QString("MMSI: %1").arg(mmsiValue));
    tooltipCallSign->setText(QString("Ship Call Sign: %1").arg(callSign));
    tooltipPositionSensor->setText(QString("Position Sensor Indication: GPS"));
    tooltipTrackStatus->setText(QString("Track Status: AIS information available"));
    tooltipListOfPorts->setText(QString("List of Ports: %1").arg(destination));
    tooltipAntennaLocation->setText(QString("Antenna Location: %1").arg(antennaLocation));

    aisTooltip->adjustSize();
}

// Implementasi fungsi hideAISTooltip
void EcWidget::hideAISTooltip()
{
    if (aisTooltip) {
        aisTooltip->hide();
    }
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

    if (targetInfo && !isAISTooltipVisible) {
        showAISTooltipFromTargetInfo(lastMousePos, targetInfo);  // Gunakan fungsi yang baru
        isAISTooltipVisible = true;
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
        case 80: return "Vessel - Towing";
        case 70: return "Cargo";
        case 60: return "Passenger";
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
    if (!aisTooltip || !targetInfo) return;

    updateAISTooltipContent(targetInfo);

    // Positioning logic sama seperti sebelumnya
    QPoint tooltipPos = mapToGlobal(position);
    tooltipPos.setX(tooltipPos.x() + 15);
    tooltipPos.setY(tooltipPos.y() + 15);

    // Screen boundary checking
    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    QSize tooltipSize = aisTooltip->sizeHint();

    if (tooltipPos.x() + tooltipSize.width() > screenGeometry.right()) {
        tooltipPos.setX(position.x() - tooltipSize.width() - 15);
        tooltipPos = mapToGlobal(QPoint(tooltipPos.x(), position.y()));
    }

    if (tooltipPos.y() + tooltipSize.height() > screenGeometry.bottom()) {
        tooltipPos.setY(mapToGlobal(position).y() - tooltipSize.height() - 15);
    }

    aisTooltip->move(tooltipPos);
    aisTooltip->show();
    aisTooltip->raise();
}
