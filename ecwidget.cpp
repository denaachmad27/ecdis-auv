// #include <QtGui>
#include <QtWidgets>
#include <QtWin>
#ifndef _WIN32
#include <QX11Info>
#endif

#include "ecwidget.h"
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

  // Initialize feedback system
  feedbackMessage = "";
  feedbackType = FEEDBACK_INFO;
  flashOpacity = 0;

  // Connect timer untuk auto-hide feedback
  connect(&feedbackTimer, &QTimer::timeout, [this]() {
      feedbackMessage.clear();
      update();
  });

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
    // Handle edit guardzone via manager
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        if (guardZoneManager->handleMouseMove(e)) {
            return; // Event handled by manager
        }
    }

    // Update current mouse position untuk preview
    if (creatingGuardZone) {
        currentMousePos = e->pos();

        update(); // Trigger repaint untuk menampilkan preview
        return; // Return early untuk avoid normal mouse move processing
    }

    EcCoordinate lat, lon;
    if (XyToLatLon(e->x(), e->y(), lat, lon))
    {
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
                navShip.lat = jsonData["NAV_LAT"].toDouble();
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

                _aisObj->readAISVariable(nmeaData);

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
    }

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

            // RECORD NMEA
            IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");

            if (dvr && dvr->isRecording()) {
                dvr->recordRawNmea(aivdo);
            }

            QList<EcFeature> pickedFeatureList;
            GetPickedFeaturesSubs(pickedFeatureList, lat, lon);

            PickWindow *pickWindow = new PickWindow(this, dictInfo, denc);;
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

}


void EcWidget::stopAISSubscribeThread() {
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
        _aisObj->deleteObject();
        _aisObj->setOwnShipNull();
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
        qDebug() << "[INFO] AIS subscription thread finished.";
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
        // Jika disable, clear semua legacy variables
        guardZoneShape = GUARD_ZONE_CIRCLE;  // Reset ke default
        guardZoneCenterLat = 0.0;
        guardZoneCenterLon = 0.0;
        guardZoneRadius = 0.5;
        guardZoneAttachedToShip = false;
        guardZoneLatLons.clear();

        qDebug() << "GuardZone system disabled and legacy variables cleared";
    } else {
        // Jika enable dan ada guardzone, set legacy variables dari yang pertama
        if (!guardZones.isEmpty()) {
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

            qDebug() << "GuardZone system enabled with" << guardZones.size() << "guardzones";
        }
    }

    qDebug() << "guardZoneActive now" << guardZoneActive;
    update();  // Force redraw
}

void EcWidget::startCreateGuardZone(::GuardZoneShape shape)
{
    qDebug() << "Starting GuardZone creation with shape:" << shape;

    // ========== PERBAIKAN FINAL UNTUK PREVIEW ==========
    // JANGAN reset creatingGuardZone ke false dulu!
    guardZonePoints.clear();

    // Set creation mode variables
    newGuardZoneShape = shape;
    creatingGuardZone = true;

    // Pastikan mouse tracking aktif dan force update current mouse position
    setMouseTracking(true);
    currentMousePos = mapFromGlobal(QCursor::pos());  // Get current cursor position

    qDebug() << "Initial mouse position:" << currentMousePos;
    qDebug() << "Mouse tracking enabled:" << hasMouseTracking();
    qDebug() << "Creation mode set:" << creatingGuardZone;
    // =================================================

    // Ubah cursor untuk menunjukkan mode pembuatan
    setCursor(Qt::CrossCursor);

    // Set status message sesuai dengan shape
    QString message;
    if (shape == GUARD_ZONE_CIRCLE) {
        message = tr("Creating Circular GuardZone: Click to set center, then click again to set radius");
    } else {
        message = tr("Creating Polygon GuardZone: Click to add points, right-click to finish");
    }

    // Emit status message
    emit statusMessage(message);

    // Force update untuk memastikan drawing state benar
    update();

    qDebug() << "GuardZone creation mode activated";
}

void EcWidget::finishCreateGuardZone()
{
    qDebug() << "Finishing GuardZone creation";

    // Reset creation mode
    creatingGuardZone = false;
    guardZonePoints.clear();

    // Restore cursor
    setCursor(Qt::ArrowCursor);

    // Disable mouse tracking if not needed elsewhere
    setMouseTracking(false);

    // Clear status message
    emit statusMessage(tr("GuardZone creation completed"));

    // Redraw to remove any preview artifacts
    update();

    qDebug() << "GuardZone creation finished successfully";
}

void EcWidget::cancelCreateGuardZone()
{
    qDebug() << "Cancelling GuardZone creation";

    // Reset creation mode
    creatingGuardZone = false;
    guardZonePoints.clear();

    // Restore cursor
    setCursor(Qt::ArrowCursor);

    // Disable mouse tracking
    // setMouseTracking(false);

    // Show feedback
    showVisualFeedback(tr("GuardZone creation cancelled"), FEEDBACK_INFO);

    // Clear status message
    emit statusMessage(tr("GuardZone creation cancelled"));

    // Redraw to remove any preview artifacts
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
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // ========== HANYA GAMBAR GUARDZONE DARI GUARDZONE LIST ==========
    // 1. Gambar guardzone yang sudah ada (dari guardZones list)
    for (const GuardZone &gz : guardZones) {
        if (!gz.active) continue;

        // Tentukan apakah guardzone ini sedang diedit
        bool isBeingEdited = (guardZoneManager && guardZoneManager->isEditingGuardZone() &&
                              gz.id == guardZoneManager->getEditingGuardZoneId());

        // Set pen dan brush yang konsisten untuk semua guardzone
        QPen pen(gz.color);  // Gunakan warna guardzone individual
        pen.setWidth(isBeingEdited ? 4 : 2);
        if (isBeingEdited) {
            pen.setStyle(Qt::DashLine);
        }
        painter.setPen(pen);

        // Gunakan alpha yang sama untuk semua guardzone
        QColor fillColor = gz.color;  // Gunakan warna guardzone individual
        fillColor.setAlpha(isBeingEdited ? 80 : 50);
        painter.setBrush(QBrush(fillColor));

        int labelX = 0, labelY = 0;  // Untuk posisi label

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
                painter.drawEllipse(QPoint(centerX, centerY),
                                    static_cast<int>(radiusInPixels),
                                    static_cast<int>(radiusInPixels));

                // Label untuk circle
                labelX = centerX;
                labelY = centerY - static_cast<int>(radiusInPixels) - 15;  // Di atas circle
            }
        }
        else if (gz.shape == GUARD_ZONE_POLYGON && gz.latLons.size() >= 6) {
            QPolygon poly;

            for (int i = 0; i < gz.latLons.size(); i += 2) {
                int x, y;
                if (LatLonToXy(gz.latLons[i], gz.latLons[i+1], x, y)) {
                    poly.append(QPoint(x, y));
                }
            }

            if (poly.size() >= 3) {
                painter.drawPolygon(poly);

                // Label untuk polygon
                QRect boundingRect = poly.boundingRect();
                labelX = boundingRect.center().x();
                labelY = boundingRect.top() - 15;  // Di atas polygon
            }
        }

        // Gambar label guardzone
        if (labelX != 0 && labelY != 0) {
            // Background untuk label
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
            textRect.moveCenter(QPoint(labelX, labelY));
            textRect.adjust(-3, -1, 3, 1);

            painter.drawRect(textRect);

            // Text label
            painter.setPen(QPen(Qt::black, 1));
            painter.setFont(labelFont);
            painter.drawText(textRect, Qt::AlignCenter, labelText);
        }
    }

    // ========== HANYA GAMBAR PREVIEW JIKA SEDANG CREATING ==========
    // 2. Preview creation mode - HANYA preview, BUKAN guardzone yang sudah jadi
    if (creatingGuardZone) {
        // Set pen untuk preview
        QPen pen(Qt::red);
        pen.setWidth(2);
        painter.setPen(pen);

        if (newGuardZoneShape == GUARD_ZONE_CIRCLE) {
            // Preview circle code
            if (guardZonePoints.isEmpty()) {
                painter.setPen(QPen(Qt::red, 2));
                painter.setFont(QFont("Arial", 12, QFont::Bold));
                painter.drawText(10, height() - 50, tr("Click to set center of circle"));
            }
            else if (guardZonePoints.size() == 1) {
                // Preview untuk circular
                QPointF center = guardZonePoints.first();
                QPointF mousePos = currentMousePos;

                double dx = mousePos.x() - center.x();
                double dy = mousePos.y() - center.y();
                double radiusPixels = sqrt(dx*dx + dy*dy);

                QRectF circleRect(center.x() - radiusPixels,
                                  center.y() - radiusPixels,
                                  radiusPixels * 2,
                                  radiusPixels * 2);

                painter.setPen(QPen(Qt::red, 2, Qt::DashLine));
                painter.setBrush(QBrush(QColor(255, 0, 0, 50)));
                painter.drawEllipse(circleRect);

                // Gambar center point
                painter.setPen(QPen(Qt::red, 3));
                painter.setBrush(QBrush(Qt::red));
                painter.drawEllipse(center, 8, 8);

                // Gambar garis radius
                painter.setPen(QPen(Qt::yellow, 2, Qt::DotLine));
                painter.drawLine(center, mousePos);

                // Gambar mouse indicator
                painter.setPen(QPen(Qt::yellow, 2));
                painter.setBrush(QBrush(QColor(255, 255, 0, 150)));
                painter.drawEllipse(mousePos, 6, 6);

                // Info radius
                double centerLat, centerLon, mouseLat, mouseLon;
                if (XyToLatLon(center.x(), center.y(), centerLat, centerLon) &&
                    XyToLatLon(mousePos.x(), mousePos.y(), mouseLat, mouseLon)) {

                    double distNM, bearing;
                    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                           centerLat, centerLon,
                                                           mouseLat, mouseLon,
                                                           &distNM, &bearing);

                    painter.setPen(QPen(Qt::black, 1));
                    painter.setBrush(QBrush(QColor(255, 255, 255, 200)));

                    QString radiusText = tr("Radius: %1 NM").arg(distNM, 0, 'f', 2);
                    QRect textRect = painter.fontMetrics().boundingRect(radiusText);
                    textRect.moveTopLeft(mousePos.toPoint() + QPoint(10, -25));
                    textRect.adjust(-5, -2, 5, 2);

                    painter.drawRect(textRect);

                    painter.setPen(QPen(Qt::black, 2));
                    painter.setFont(QFont("Arial", 10, QFont::Bold));
                    painter.drawText(textRect, Qt::AlignCenter, radiusText);
                }
            }
        }
        else if (newGuardZoneShape == GUARD_ZONE_POLYGON) {
            // Preview polygon code
            if (guardZonePoints.isEmpty()) {
                painter.setPen(QPen(Qt::red, 2));
                painter.setFont(QFont("Arial", 12, QFont::Bold));
                painter.drawText(10, height() - 50, tr("Click to add first point"));
            }
            else {
                // Garis-garis yang sudah ada
                if (guardZonePoints.size() >= 2) {
                    painter.setPen(QPen(Qt::red, 3, Qt::SolidLine));
                    for (int i = 0; i < guardZonePoints.size() - 1; ++i) {
                        painter.drawLine(guardZonePoints[i], guardZonePoints[i+1]);
                    }
                }

                // Garis preview ke mouse
                if (guardZonePoints.size() >= 1) {
                    painter.setPen(QPen(Qt::yellow, 2, Qt::DotLine));
                    painter.drawLine(guardZonePoints.last(), currentMousePos);
                }

                // Garis penutup preview
                if (guardZonePoints.size() >= 3) {
                    painter.setPen(QPen(Qt::blue, 2, Qt::DashLine));
                    painter.drawLine(guardZonePoints.first(), currentMousePos);
                }

                // Area preview
                if (guardZonePoints.size() >= 3) {
                    QPolygon previewPoly;
                    for (const QPointF &point : guardZonePoints) {
                        previewPoly.append(point.toPoint());
                    }
                    previewPoly.append(currentMousePos);

                    painter.setPen(QPen(Qt::red, 1, Qt::DashLine));
                    painter.setBrush(QBrush(QColor(255, 0, 0, 40)));
                    painter.drawPolygon(previewPoly);
                }

                // Titik-titik dengan nomor
                painter.setPen(QPen(Qt::red, 2));
                painter.setBrush(QBrush(Qt::red));
                for (int i = 0; i < guardZonePoints.size(); ++i) {
                    const QPointF &point = guardZonePoints[i];

                    painter.drawEllipse(point, 8, 8);

                    painter.setPen(QPen(Qt::white, 2));
                    painter.setFont(QFont("Arial", 8, QFont::Bold));
                    painter.drawText(point.toPoint() + QPoint(-4, 3), QString::number(i + 1));

                    painter.setPen(QPen(Qt::red, 2));
                    painter.setBrush(QBrush(Qt::red));
                }

                // Titik preview di mouse
                painter.setPen(QPen(Qt::yellow, 2));
                painter.setBrush(QBrush(QColor(255, 255, 0, 150)));
                painter.drawEllipse(currentMousePos, 6, 6);
            }
        }

        // Preview creation instructions
        QRect createInstructionRect(10, height() - 120, 550, 110);

        QLinearGradient gradient(createInstructionRect.topLeft(), createInstructionRect.bottomLeft());
        gradient.setColorAt(0, QColor(0, 100, 200, 220));
        gradient.setColorAt(1, QColor(0, 150, 255, 180));
        painter.fillRect(createInstructionRect, gradient);

        painter.setPen(QPen(Qt::white, 2));
        painter.drawRect(createInstructionRect);

        painter.setFont(QFont("Arial", 11, QFont::Bold));
        QString createTitle;
        if (newGuardZoneShape == GUARD_ZONE_CIRCLE) {
            createTitle = tr("🛡️ CREATING CIRCULAR GUARDZONE");
        } else {
            createTitle = tr("🛡️ CREATING POLYGON GUARDZONE");
        }
        painter.drawText(createInstructionRect.adjusted(10, 8, -10, -80), Qt::AlignTop | Qt::AlignCenter, createTitle);

        painter.setFont(QFont("Arial", 9));
        QString stepInstructions;

        if (newGuardZoneShape == GUARD_ZONE_CIRCLE) {
            if (guardZonePoints.isEmpty()) {
                stepInstructions = tr("STEP 1/2: Click anywhere on map to set CENTER point of guard zone");
            } else if (guardZonePoints.size() == 1) {
                stepInstructions = tr("STEP 2/2: Move cursor to desired radius and click to FINISH\n"
                                      "• Yellow line shows current radius\n"
                                      "• Radius info displayed near cursor");
            }
        } else {
            if (guardZonePoints.isEmpty()) {
                stepInstructions = tr("STEP 1/∞: Click to place FIRST point of polygon guard zone");
            } else if (guardZonePoints.size() == 1) {
                stepInstructions = tr("STEP 2/∞: Click to place SECOND point\n"
                                      "• Yellow line shows next edge");
            } else if (guardZonePoints.size() == 2) {
                stepInstructions = tr("STEP 3/∞: Click to place THIRD point\n"
                                      "• Blue line shows closing edge preview\n"
                                      "• Right-click when satisfied (minimum 3 points)");
            } else {
                stepInstructions = tr("STEP %1/∞: Click to add more points OR right-click to FINISH\n"
                                      "• Red area shows current polygon shape\n"
                                      "• Minimum 3 points required").arg(guardZonePoints.size() + 1);
            }
        }

        painter.drawText(createInstructionRect.adjusted(15, 30, -15, -10), Qt::AlignTop | Qt::AlignLeft, stepInstructions);

        // Progress indicator for circle
        if (newGuardZoneShape == GUARD_ZONE_CIRCLE) {
            painter.setPen(QPen(Qt::yellow, 3));
            int progressWidth = 100;
            int progress = guardZonePoints.size() * (progressWidth / 2);
            painter.drawRect(createInstructionRect.right() - 120, createInstructionRect.bottom() - 25, progressWidth, 10);
            painter.fillRect(createInstructionRect.right() - 120, createInstructionRect.bottom() - 25, progress, 10, Qt::yellow);
        }
    }

    // 3. Edit mode overlay via manager
    if (guardZoneManager && guardZoneManager->isEditingGuardZone()) {
        guardZoneManager->drawEditOverlay(painter);
    }

    // 4. Draw feedback overlay
    if (feedbackTimer.isActive()) {
        drawFeedbackOverlay(painter);
    }

    painter.end();
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

    // Dapatkan posisi guardzone
    double guardZoneLat, guardZoneLon;
    double effectiveRadius = 0.5; // Default minimal jarak 0.5 mil laut

    if (guardZoneShape == GUARD_ZONE_CIRCLE) {
        guardZoneLat = guardZoneCenterLat;
        guardZoneLon = guardZoneCenterLon;
        effectiveRadius = guardZoneRadius * 1.2; // Pastikan di luar guardzone
    } else if (guardZoneShape == GUARD_ZONE_POLYGON && guardZoneLatLons.size() >= 2) {
        // Gunakan titik pertama polygon sebagai referensi
        guardZoneLat = guardZoneLatLons[0];
        guardZoneLon = guardZoneLatLons[1];

        // Perkirakan ukuran polygon dengan menghitung jarak rata-rata dari pusat
        double totalDist = 0.0;
        int numPoints = guardZoneLatLons.size() / 2;

        // Hitung pusat polygon
        double centerLat = 0, centerLon = 0;
        for (int i = 0; i < guardZoneLatLons.size(); i += 2) {
            centerLat += guardZoneLatLons[i];
            centerLon += guardZoneLatLons[i+1];
        }
        centerLat /= numPoints;
        centerLon /= numPoints;

        // Hitung radius efektif
        for (int i = 0; i < guardZoneLatLons.size(); i += 2) {
            double dist, dummy;
            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                   centerLat, centerLon,
                                                   guardZoneLatLons[i], guardZoneLatLons[i+1],
                                                   &dist, &dummy);
            totalDist += dist;
        }
        effectiveRadius = (totalDist / numPoints) * 1.5; // Pastikan di luar polygon
        guardZoneLat = centerLat;
        guardZoneLon = centerLon;
    } else {
        // Jika tidak ada guardzone yang valid, gunakan posisi saat ini
        GetCenter(guardZoneLat, guardZoneLon);
    }

    // Pastikan effectiveRadius minimal 0.5 mil laut
    if (effectiveRadius < 0.5) effectiveRadius = 0.5;

    // Distribusikan target di sekitar guardzone
    double angleStep = 360.0 / count;

    for (int i = 0; i < count; ++i) {
        SimulatedAISTarget target;

        // Setiap kapal memiliki arah yang berbeda sekitar guardzone
        double startBearing = i * angleStep + (qrand() % 20 - 10);

        // Jarak ke guardzone berdasarkan waktu yang ingin dicapai (5 detik)
        // Kecepatan bervariasi antara 10-20 knot
        target.sog = 10.0 + (i * 2.5); // 10, 12.5, 15, 17.5, 20 knot

        // Target kita ingin kapal mencapai guardzone dalam 1-5 detik
        // Waktu yang diinginkan dalam detik, semakin besar i semakin lama
        double desiredTimeSeconds = 1.0 + i;

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

        // MMSI
        target.mmsi = QString("AIS%1").arg(i+1);
        target.dangerous = false;

        simulatedTargets.append(target);
    }
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

    bool foundNewDanger = false;
    QString alertMessages;
    int alertCount = 0;

    for (int i = 0; i < simulatedTargets.size(); ++i) {
        bool inGuardZone = false;

        if (guardZoneShape == GUARD_ZONE_CIRCLE) {
            // Hitung jarak ke pusat guardzone
            double distance, bearing;
            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                   guardZoneCenterLat, guardZoneCenterLon,
                                                   simulatedTargets[i].lat, simulatedTargets[i].lon,
                                                   &distance, &bearing);

            // Jika jarak kurang dari radius guardzone, target ada di dalam
            inGuardZone = (distance <= guardZoneRadius);
        }
        else if (guardZoneShape == GUARD_ZONE_POLYGON && guardZoneLatLons.size() >= 6) {
            // Konversi titik ke koordinat layar
            int targetX, targetY;
            if (LatLonToXy(simulatedTargets[i].lat, simulatedTargets[i].lon, targetX, targetY)) {
                QPolygon poly;
                for (int j = 0; j < guardZoneLatLons.size(); j += 2) {
                    int x, y;
                    LatLonToXy(guardZoneLatLons[j], guardZoneLatLons[j+1], x, y);
                    poly.append(QPoint(x, y));
                }

                inGuardZone = poly.containsPoint(QPoint(targetX, targetY), Qt::OddEvenFill);
            }
        }

        // Jika target baru masuk guardzone, tandai sebagai berbahaya dan tambahkan ke pesan
        if (inGuardZone && !simulatedTargets[i].dangerous) {
            foundNewDanger = true;
            simulatedTargets[i].dangerous = true;

            alertMessages += tr("Target %1: Speed %2 knots, Course %3°\n")
                                 .arg(simulatedTargets[i].mmsi)
                                 .arg(simulatedTargets[i].sog, 0, 'f', 1)
                                 .arg(simulatedTargets[i].cog, 0, 'f', 0);
            alertCount++;
        } else if (!inGuardZone) {
            // Jika target keluar dari guardzone, tandai sebagai tidak berbahaya
            simulatedTargets[i].dangerous = false;
        }
    }

    // Hanya tampilkan peringatan jika ada target baru yang masuk
    if (foundNewDanger && alertCount > 0) {
        QString title = tr("Guard Zone Alert - %1 Target(s) Detected").arg(alertCount);

        QMessageBox::warning(this, title, alertMessages);
    }

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

    // ========== VALIDATION ==========
    // Validate coordinates
    if (centerLat < -90 || centerLat > 90 || centerLon < -180 || centerLon > 180) {
        showVisualFeedback(tr("Invalid coordinates for GuardZone center!"), FEEDBACK_ERROR);
        return;
    }

    // Validate radius
    if (radiusNM <= 0 || radiusNM > 100) { // Maximum 100 NM radius
        showVisualFeedback(tr("Invalid radius! Must be between 0.01 and 100 NM"), FEEDBACK_ERROR);
        return;
    }

    // HAPUS: Check if there are too many guardzones
    // if (guardZones.size() >= 10) { // Maximum 10 guardzones
    //     showVisualFeedback(tr("Maximum 10 GuardZones allowed!"), FEEDBACK_WARNING);
    //     return;
    // }
    // ===============================

    // Create new GuardZone struct
    GuardZone newGuardZone;
    newGuardZone.id = getNextGuardZoneId();
    newGuardZone.name = QString("GuardZone_%1").arg(newGuardZone.id);
    newGuardZone.shape = GUARD_ZONE_CIRCLE;
    newGuardZone.active = true;
    newGuardZone.attachedToShip = false;
    newGuardZone.color = Qt::red;
    newGuardZone.centerLat = centerLat;
    newGuardZone.centerLon = centerLon;
    newGuardZone.radius = radiusNM;

    // Add to guardzone list
    guardZones.append(newGuardZone);

    // Set legacy variables untuk backward compatibility
    guardZoneActive = true;
    guardZoneShape = GUARD_ZONE_CIRCLE;
    guardZoneCenterLat = centerLat;
    guardZoneCenterLon = centerLon;
    guardZoneRadius = radiusNM;
    guardZoneAttachedToShip = false;

    qDebug() << "Circular guardzone created successfully with ID:" << newGuardZone.id;

    // Show feedback
    showVisualFeedback(tr("Circular GuardZone created!\nID: %1, Radius: %2 NM")
                           .arg(newGuardZone.id)
                           .arg(radiusNM, 0, 'f', 2),
                       FEEDBACK_SUCCESS);

    // Save guardzones to file
    saveGuardZones();

    // Redraw
    update();
}

void EcWidget::createPolygonGuardZoneNew()
{
    // ========== VALIDATION ==========
    if (guardZonePoints.size() < 3) {
        showVisualFeedback(tr("Polygon needs at least 3 points!"), FEEDBACK_ERROR);
        return;
    }

    if (guardZonePoints.size() > 20) { // Maximum 20 points untuk performa
        showVisualFeedback(tr("Too many points! Maximum 20 points allowed"), FEEDBACK_WARNING);
        return;
    }

    // HAPUS: Check if there are too many guardzones
    // if (guardZones.size() >= 10) { // Maximum 10 guardzones
    //     showVisualFeedback(tr("Maximum 10 GuardZones allowed!"), FEEDBACK_WARNING);
    //     return;
    // }
    // ===============================

    qDebug() << "Creating new polygon guardzone with" << guardZonePoints.size() << "points";

    // Convert screen points to lat/lon
    QVector<double> latLons;
    int invalidPoints = 0;

    for (const QPointF &point : guardZonePoints) {
        EcCoordinate lat, lon;
        if (XyToLatLon(point.x(), point.y(), lat, lon)) {
            // Validate coordinates
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

    if (latLons.size() < 6) { // 3 points = 6 coordinates
        showVisualFeedback(tr("Failed to convert polygon points!"), FEEDBACK_ERROR);
        return;
    }

    // Create new GuardZone struct
    GuardZone newGuardZone;
    newGuardZone.id = getNextGuardZoneId();
    newGuardZone.name = QString("GuardZone_%1").arg(newGuardZone.id);
    newGuardZone.shape = GUARD_ZONE_POLYGON;
    newGuardZone.active = true;
    newGuardZone.attachedToShip = false;
    newGuardZone.color = Qt::red;
    newGuardZone.latLons = latLons;

    // Add to guardzone list
    guardZones.append(newGuardZone);

    // Set legacy variables untuk backward compatibility
    guardZoneActive = true;
    guardZoneShape = GUARD_ZONE_POLYGON;
    guardZoneLatLons = latLons;
    guardZoneAttachedToShip = false;

    qDebug() << "Polygon guardzone created successfully with ID:" << newGuardZone.id;

    // Show feedback
    showVisualFeedback(tr("Polygon GuardZone created!\nID: %1, Points: %2")
                           .arg(newGuardZone.id)
                           .arg(guardZonePoints.size()),
                       FEEDBACK_SUCCESS);

    // Save guardzones to file
    saveGuardZones();

    // Redraw
    update();
}

// Tambahkan di ecwidget.cpp (di bagian akhir file sebelum closing bracket):

void EcWidget::saveGuardZones()
{
    if (guardZones.isEmpty()) {
        qDebug() << "[INFO] No guardzones to save";
        return;
    }

    QJsonArray guardZoneArray;

    for (const GuardZone &gz : guardZones)
    {
        QJsonObject gzObject;
        gzObject["id"] = gz.id;
        gzObject["name"] = gz.name;
        gzObject["shape"] = static_cast<int>(gz.shape);
        gzObject["active"] = gz.active;
        gzObject["attachedToShip"] = gz.attachedToShip;
        gzObject["color"] = gz.color.name();

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
    }

    QJsonObject rootObject;
    rootObject["guardzones"] = guardZoneArray;
    rootObject["version"] = "1.0";
    rootObject["saved_on"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonDocument jsonDoc(rootObject);

    QString filePath = getGuardZoneFilePath();
    QDir dir = QFileInfo(filePath).dir();

    // Pastikan direktori ada
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            qDebug() << "[ERROR] Could not create directory for guardzones:" << dir.path();
            filePath = "guardzones.json"; // Gunakan direktori saat ini sebagai fallback
        }
    }

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write(jsonDoc.toJson(QJsonDocument::Indented));
        file.close();
        qDebug() << "[INFO] GuardZones saved to" << filePath;
    }
    else
    {
        qDebug() << "[ERROR] Failed to save guardzones to" << filePath << ":" << file.errorString();

        // Coba simpan di direktori saat ini sebagai cadangan
        QFile fallbackFile("guardzones.json");
        if (fallbackFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            fallbackFile.write(jsonDoc.toJson(QJsonDocument::Indented));
            fallbackFile.close();
            qDebug() << "[INFO] GuardZones saved to fallback location: guardzones.json";
        }
        else
        {
            qDebug() << "[ERROR] Failed to save guardzones to fallback location:" << fallbackFile.errorString();
        }
    }
}

void EcWidget::loadGuardZones()
{
    QString filePath = getGuardZoneFilePath();
    QFile file(filePath);

    // Cek lokasi utama
    if (!file.exists())
    {
        // Coba lokasi cadangan
        QFile fallbackFile("guardzones.json");
        if (fallbackFile.exists()) {
            file.setFileName("guardzones.json");
            filePath = "guardzones.json";
        } else {
            qDebug() << "[INFO] No guardzones file found at" << filePath << "or in current directory";
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
            qDebug() << "[ERROR] Invalid guardzones JSON file structure (not an object)";
            return;
        }

        QJsonObject rootObject = jsonDoc.object();
        if (!rootObject.contains("guardzones") || !rootObject["guardzones"].isArray())
        {
            qDebug() << "[ERROR] Invalid guardzones JSON file (missing guardzones array)";
            return;
        }

        QJsonArray guardZoneArray = rootObject["guardzones"].toArray();

        guardZones.clear();

        int validGuardZones = 0;

        for (const QJsonValue &value : guardZoneArray)
        {
            if (!value.isObject())
                continue;

            QJsonObject gzObject = value.toObject();

            // Cek field yang diperlukan
            if (!gzObject.contains("shape"))
                continue;

            GuardZone gz;
            gz.id = gzObject.contains("id") ? gzObject["id"].toInt() : getNextGuardZoneId();
            gz.name = gzObject.contains("name") ? gzObject["name"].toString() : QString("GuardZone_%1").arg(gz.id);
            gz.shape = static_cast<::GuardZoneShape>(gzObject["shape"].toInt());
            gz.active = gzObject.contains("active") ? gzObject["active"].toBool() : true;
            gz.attachedToShip = gzObject.contains("attachedToShip") ? gzObject["attachedToShip"].toBool() : false;
            gz.color = gzObject.contains("color") ? QColor(gzObject["color"].toString()) : Qt::red;

            if (gz.shape == GUARD_ZONE_CIRCLE) {
                if (gzObject.contains("centerLat") && gzObject.contains("centerLon") && gzObject.contains("radius")) {
                    gz.centerLat = gzObject["centerLat"].toDouble();
                    gz.centerLon = gzObject["centerLon"].toDouble();
                    gz.radius = gzObject["radius"].toDouble();
                } else {
                    continue; // Skip invalid circle
                }
            } else if (gz.shape == GUARD_ZONE_POLYGON) {
                if (gzObject.contains("latLons") && gzObject["latLons"].isArray()) {
                    QJsonArray latLonArray = gzObject["latLons"].toArray();
                    gz.latLons.clear();
                    for (const QJsonValue &coord : latLonArray) {
                        gz.latLons.append(coord.toDouble());
                    }

                    if (gz.latLons.size() < 6) { // Minimal 3 points = 6 coordinates
                        continue; // Skip invalid polygon
                    }
                } else {
                    continue; // Skip invalid polygon
                }
            }

            guardZones.append(gz);
            validGuardZones++;

            // Update nextGuardZoneId jika perlu
            if (gz.id >= nextGuardZoneId) {
                nextGuardZoneId = gz.id + 1;
            }
        }

        qDebug() << "[INFO] Loaded" << validGuardZones << "guardzones from" << filePath;

        // Set guardzone active jika ada guardzone yang loaded
        if (!guardZones.isEmpty()) {
            guardZoneActive = true;

            // Set legacy variables dari guardzone pertama untuk backward compatibility
            const GuardZone &firstGz = guardZones.first();
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

        // Redraw untuk menampilkan guardzone yang loaded
        update();
    }
    else
    {
        qDebug() << "[ERROR] Failed to open guardzones file for reading:" << file.errorString();
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
