#ifndef _ec_widget_h_
#define _ec_widget_h_

#include <QPixmap>
#include <QWidget>
#include <QtWinExtras/QtWin>

#include <QTimer>
#include <QDateTime>

// SevenCs Kernel EC2007
#ifdef _WIN32
#include <windows.h>
#pragma pack(push, 4)
#include <eckernel.h>
#include <ecs63.h>
#pragma pack (pop)
#else
#include <stdio.h>
#include <X11/Xlib.h>
#include <eckernel.h>
#endif

#include "moosdb.h"
#include "guardzone.h"

struct AISTargetData {
    QString mmsi;
    double lat;
    double lon;
    double cog;         // Course over ground
    double sog;         // Speed over ground
    double cpa;         // Closest Point of Approach (nautical miles)
    double tcpa;        // Time to CPA (minutes)
    bool isDangerous;   // Apakah target berbahaya
    QDateTime lastUpdate;
};

class AlertSystem;
struct AlertData;

class GuardZoneManager; // Forward declaration

extern QString bottomBarText;

// Defines for S-63 Chart Import
// The Manufacturer is in charge of generating unique user permits for each installation of his application
// S-63 Uuser permit (must have 28 characters)
#define USERPERMIT "66B5CBFDF7E4139D5B6086C23130"
// The Manufacturer key is supplied by the IHO on request (application required)
// S-63 Manufacturer key (M_KEY must have either 5 or 10 characters)
#define M_KEY "10121"

// Waypoint
#define PICKRADIUS  (0.03 * GetRange);

//Waypoint
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// forward declerations1
class PickWindow;
class Ais;

struct OwnShipStruct
{
	double lat;
	double lon;
	double cog;
	double sog;
  double heading;
  double length;
  double breadth;
};		

//! Widget for drawing ECDIS data
class EcWidget : public QWidget
{
  Q_OBJECT

    static int minScale, maxScale;

public:
  enum ProjectionMode
  {
    AutoProjection = 0,
    MercatorProjection = EC_GEO_PROJECTION_MERCATOR,
    GnomonicProjection = EC_GEO_PROJECTION_GNOMONIC,
    StereographicProjection = EC_GEO_PROJECTION_STEREOGRAPHIC
  };

  // Waypoint
  enum ActiveFunction {
      PAN,
      CREATE_WAYP,
      MOVE_WAYP,
      REMOVE_WAYP,
      EDIT_WAYP
  };

  struct Waypoint
  {
      double lat;
      double lon;
      QString label;
      QString remark;
      double turningRadius = 10.0;
      bool active = true;
  };

  void setActiveFunction(ActiveFunction func) { activeFunction = func; }

  void drawOverlayCell();

  void drawWaypointMarker(EcCoordinate lat, EcCoordinate lon);
  void drawSingleWaypoint(EcCoordinate lat, EcCoordinate lon, const QString& label);
  void saveWaypoints();
  void removeWaypointAt(int x, int y);
  void moveWaypointAt(int x, int y);
  void editWaypointAt(int x, int y);
  bool resetWaypointCell();
  void drawLeglineLabels();

  void loadWaypoints();
  QString getWaypointFilePath() const;
  int getWaypointCount() const { return waypointList.size(); }
  QList<Waypoint> getWaypoints() const { return waypointList; }  void clearWaypoints();
  bool exportWaypointsToFile(const QString &filename);
  bool importWaypointsFromFile(const QString &filename);
  bool initializeWaypointSystem();


  // End Waypoint
  //////////////////////////////////////////////////////////////////////

  // Guardzone

  enum FeedbackType {
      FEEDBACK_SUCCESS,
      FEEDBACK_WARNING,
      FEEDBACK_ERROR,
      FEEDBACK_INFO
  };

  void setGuardZoneAttachedToShip(bool attached);
  void generateTargetsTowardsGuardZone(int count);

  enum SimulationScenario {
      SCENARIO_STATIC_GUARDZONE = 1,
      SCENARIO_MOVING_GUARDZONE = 2
  };

  QList<GuardZone>& getGuardZones() { return guardZones; }
  int getNextGuardZoneId() { return nextGuardZoneId++; }
  void saveGuardZones();
  void loadGuardZones();

  // Method yang diperlukan oleh GuardZoneManager
  double calculatePixelsFromNauticalMiles(double nauticalMiles);

  // Method untuk start edit dari luar (menu, dll)
  void startEditGuardZone(int guardZoneId);

  void showVisualFeedback(const QString& message, FeedbackType type);
  void playFeedbackSound(FeedbackType type);
  void debugPreviewState();

  // Helper methods untuk GuardZone Panel
  GuardZoneManager* getGuardZoneManager() { return guardZoneManager; }
  void emitGuardZoneModified() { emit guardZoneModified(); }
  void emitGuardZoneDeleted() { emit guardZoneDeleted(); }
  void emitGuardZoneSignals(const QString& action, int guardZoneId);

  bool validateGuardZoneSystem();
  bool isGuardZoneInViewport(const GuardZone& gz, const QRect& viewport);
  void drawGuardZoneLabel(QPainter& painter, const GuardZone& gz, const QPoint& position);
  void drawGuardZoneCreationPreview(QPainter& painter);

  // End Guardzone

  // Alert System methods
  AlertSystem* getAlertSystem() { return alertSystem; }
  void initializeAlertSystem();

  // Alert integration with existing systems
  void checkAlertConditions();
  void triggerNavigationAlert(const QString& message, int priority);
  void triggerNavigationAlert(const QString& message);  // Overload untuk default

  void triggerDepthAlert(double currentDepth, double threshold, bool isShallow = true);
  void triggerGuardZoneAlert(int guardZoneId, const QString& details);

  struct NavShipStruct
  {
      double lat;
      double lon;
      double depth;        // TAMBAHKAN ini jika belum ada
      double heading;
      double heading_og;
      double speed;
      double speed_og;
      double yaw;
      double z;
  };

  // CPA - TCPA
  // TAMBAHAN BARU - CPA/TCPA Methods
  void updateCPADisplay();
  void checkCPAAlerts();
  QList<AISTargetData> getCurrentTargets();


  class Exception
  {
    QStringList msgs;
  public:
    Exception(const QString & m)
      : msgs(QString("EcWidget: %1").arg(m))
    {}
    Exception(const QStringList & lst, const QString & m)
      : msgs(lst)
    {
      msgs += QString("EcWidget: %1").arg(m);
    }

    const QStringList & GetMessages() const { return msgs; }
  }; // Exception

  EcWidget(EcDictInfo *dictInfo, QString *libStr, QWidget *parent);
  virtual ~EcWidget ();

  // Returns if the widget was successfully initialized
  bool IsInitialized() const { return (view != (EcView*)0) && initialized; }

  // Initializes the S-63 Settings
  void InitS63();

  // Returns the DENC structure
  EcDENC *GetDENC() const { return denc; } 

  // Checks if the DENC is valid
  bool HasValidDENC() const { return denc != (EcDENC*)0; }

  // Returns the scale of the viewport.
  int GetScale  () const { return currentScale; }

  // Returns the heading of the viewport in nautical degrees (north = 0, clockwise)
  double GetHeading() const { return currentHeading; }

  // Returns the projection mode
  ProjectionMode GetProjectionMode() const { return projectionMode; }

  // Returns the center of the viewport, the coordinates are in decimal degrees.
  void GetCenter (EcCoordinate & lat, EcCoordinate & lon) const;

  // Returns the current color scheme
  int GetColorScheme () const { return currentColorScheme; }

  // Returns the current brightness
  int GetBrightness() const { return currentBrightness; }

  // Returns the current grey mode
  bool GetGreyMode() const { return currentGreyMode; }

  // Transforms a scale to the corresponding range.
  double GetRange (int scale) const;

  // Returns the human readable projection name
  QString GetProjectionName () const;

  // Returns the picked features at a position
  void GetPickedFeatures(QList<EcFeature> &pickedFeatureList);

  // Returns the searched features at a position
  void GetSearchedFeatures(QList<EcFeature> &pickedFeatureList);

  // Returns the picked features at a position
  void GetPickedFeaturesSubs(QList<EcFeature> &pickedFeatureList, EcCoordinate lat, EcCoordinate lon);

  // Defines the DENC path
  bool CreateDENC(const QString & dp, bool updCat);

  // Imports S-57 or SENC data recursivly from a tree
  int ImportTree(const QString & dir);

  // Imports a S-57 Exchange Set
  int ImportS57ExchangeSet(const QString & dir);

  // Imports the IHO Certificate
  bool ImportIHOCertificate(const QString & ihoCertificate);

  // Imports S-63 Permits, either existing or new ones
  bool ImportS63Permits(const QString & s63Permits);

  // Imports S-63 Exchange Set
  bool ImportS63ExchangeSet(const QString & dir);

  // Sets the S-63 Manufacturer Key
  bool SetS63MKey(const int s63mkey);

  // Sets the S-63 User Permit
  bool SetS63UserPermit(const int s63up);

  // Applies the updates
  int ApplyUpdate();

  // Sets the scale of the viewport
  virtual void  SetScale   (int sc);

  // Sets the heading of the viewport, heading is in degrees, north is 0 counting clockwise.
  virtual void SetHeading (double h);

  // Sets the projection used for presenting the chart
  // Please note that the projection should be carefully choosen depending on scale and position.
  virtual void SetProjection (ProjectionMode p);

  // Sets the center of the viewport, the coordinates are in decimal degrees
  virtual void SetCenter (EcCoordinate lat, EcCoordinate lon);

  // Set the color scheme of the widget 
  // EC_DAY_BRIGHT, EC_DAY_WHITEBACK, EC_DAY_BLACKBACK, EC_DUSK, EC_NIGHT
  // greyMode switches the grey mode on or off
  // With brightness one can control the brightness [1 .. 100]
  virtual void SetColorScheme (int scheme, bool greyMode, int brightness);

  // Sets the View class 
  // EC_DISPLAYBASE, EC_STANDARD, EC_OTHER
  void SetDisplayCategory(int dc);

  // Switches the lookup table
  // EC_LOOKUP_SIMPLIFIE, EC_LOOKUP_TRADITIONAL
  void SetLookupTable(int lut);

  // Switches the display of light features on or off
  void ShowLights(bool on);

  // Switches the display of text on or off
  void ShowText(bool on);

  // Swiches the display of soundings on or off
  void ShowSoundings(bool on);

  // Swiches the display of the grid on or off (only supported for rectengular projection)
  void ShowGrid(bool on);

  // Swiches the display of AIS targets on or off (only if AIS telegrams are read)
  void ShowAIS(bool on);

  // Transforms device coordinates from this widget to geodetic coordinates (WGS84)
  virtual bool XyToLatLon (int x, int y, EcCoordinate & lat, EcCoordinate & lon);

  // Transforms geodetic coordinates (WGS84) to device coordinates
  virtual bool LatLonToXy (EcCoordinate lat, EcCoordinate lon, int & x, int & y);

  // Draws the chart
  void Draw();

  // Waypoint
  void CreateWaypoint(ActiveFunction active);
  void SetWaypointPos(EcCoordinate lat, EcCoordinate lon);
  bool drawUdo(void);
  bool createUdoCell();
  void createWaypoint();

  // GuardZone
  void enableGuardZone(bool enable);
  bool isGuardZoneActive() const { return guardZoneActive; }
  void startCreateGuardZone(::GuardZoneShape shape);
  void finishCreateGuardZone();
  void cancelCreateGuardZone();
  void checkGuardZone();
  QString getGuardZoneFilePath() const;

  // Simulasi AIS
  void startAISTargetSimulation();
  void stopAISTargetSimulation();
  void simulateAISTarget(double lat, double lon, double cog, double sog);
  void updateSimulatedTargets();
  void setAutoCheckGuardZone(bool enable);
  bool checkTargetsInGuardZone();
  void generateRandomAISTargets(int count);
  void startAISTargetSimulationStatic();  // Skenario 1
  void startAISTargetSimulationMoving();  // Skenario 2
  void updateOwnShipPosition();           // Update posisi kapal sendiri

  // Draw AIS cell
  void InitAIS( EcDictInfo *dict );
  void ReadAISLogfile( const QString& );
  void ReadAISVariable( const QStringList& );
  void StopReadAISVariable();
  QString StartReadAISSubscribe();
  QString StartReadAISSubscribeSSH();
  void startServerMOOSSubscribe();
  void ReadFromServer( const QString& );

  void processAISLoop();
  void handleDisconnection(QTcpSocket* socket);
  void PublishNavInfo(QTcpSocket* socket, double lat, double lon);
  void targetAISDrawTrigger();

  // NAV INFO
  QString StartThreadSubscribeSSH(EcWidget *ecchart);
  void startAISSubscribeThread(EcWidget *ecchart);
  void connectToMOOSDB(EcWidget *ecchart);
  void processAISData(QTcpSocket* socket);
  void processAISDataHybrid(QTcpSocket* socket, EcWidget *ecchart);
  void stopAISSubscribeThread();

  // MAP INFO
  void startAISSubscribeThreadMAP(EcWidget *ecchart);
  void connectToMOOSDBMAP(EcWidget *ecchart);
  void processDataMAP(QTcpSocket* socket, EcWidget *ecchart);
  void stopAISSubscribeThreadMAP();

  void jsonExample();

signals:
  // Drawing signals
  void mouseMove(EcCoordinate, EcCoordinate);
  void mouseRightClick();
  void projection();
  void scale(int);

  // DENC signals
  void import(const char *, bool &);
  void replace(const char *, bool &);

  // Waypoint Signals
  void waypointCreated();

  // GuardZone Signals
  void statusMessage(const QString &message);

  // GuardZone signals untuk panel
  void guardZoneCreated();
  void guardZoneModified();
  void guardZoneDeleted();

  // Alert system signals
  void alertTriggered(const AlertData& alert);
  void criticalAlertTriggered(const AlertData& alert);
  void alertSystemStatusChanged(bool enabled);

private slots:
  void slotUpdateAISTargets( Bool bSymbolize );
  void slotRefreshChartDisplay( double lat, double lon );

  // Alert Systems
  void onAlertTriggered(const AlertData& alert);
  void onCriticalAlert(const AlertData& alert);
  void onAlertSystemStatusChanged(bool enabled);
  void performPeriodicAlertChecks();

protected:
  // Drawing functions
  void clearBackground();

  virtual void draw (bool update);
  virtual void drawAISCell ();

  virtual void paintEvent  (QPaintEvent*);
  virtual void resizeEvent (QResizeEvent*);

  virtual void mousePressEvent(QMouseEvent*);

  virtual void mouseMoveEvent(QMouseEvent*);

  virtual void wheelEvent  (QWheelEvent*);

  virtual void keyPressEvent(QKeyEvent*);

  virtual void searchOk (EcCoordinate, EcCoordinate);

  //virtual void keyPressEvent( QKeyEvent *e );

  QString        dencPath;
  QString        certificateFileName;
  QString        s63permitFileName;
  char           lib7csStr[256];
  EcDictInfo    *dictInfo;
  EcDENC        *denc;
  EcView        *view;
  EcCellId       aisCellId, udoCid;
  EcChartPermitList    *permitList;
  unsigned char        *s63HwId;
  FILE          *errlog;

  OwnShipStruct ownShip;

  // Waypoint
  EcFeature         wp1;
  ActiveFunction    activeFunction;
  EcCoordinate      wplat, wplon;
  int range;
  virtual void drawWaypointCell ();

  // GuardZone
  void drawGuardZone();
  void createCircularGuardZone(EcCoordinate lat, EcCoordinate lon, double radius);
  void createPolygonGuardZone();
  void highlightDangersInGuardZone();
  virtual void mouseReleaseEvent(QMouseEvent *e) override;

  // Fungsi untuk simulasi AIS
  void drawSimulatedTargets();
  void generateStaticObstacles(int count);

  // DENC functions
  bool _import(const char *n);
  bool _replace(const char *n);

  // EC2007 Kernel Callback functions
  static Bool ImportCB(EcDENC*, const char *, int, void *);
  static Bool S57CB(int, int, const char *);

  // VIEWPORT PARAMETER
  bool             geoSymLoaded;
  bool             showGrid;
  bool             showAIS;
  bool             showNationalText;
  EcCoordinate     currentLat, currentLon;
  int              currentLookupTable;
  int              currentScale;
  double           currentHeading;

  // PICKWINDOW
	PickWindow *pickWindow;
	int         pickX, pickY;

  // PROJECTION MODE AND TYPE 
  ProjectionMode   projectionMode;
  EcProjectionType currentProjection;

  // PLATTFORM DEPENDED MEMBER
#ifdef _WIN32
  HDC      hdc;
  HBITMAP  hBitmap;
  HBITMAP  hBitmapOverlay;
  HPALETTE hPalette;
#else
  Display *dpy;
  Colormap cmap;
  GC       drawGC;
  Pixmap   x11pixmap;
#endif

  // COLOR STATE
  int     currentColorScheme;
  int     currentBrightness;
  bool    currentGreyMode;

  QColor  bg;
  QPixmap chartPixmap; // contains and stores the chart image
  QPixmap chartAisPixmap; // contains the chart image plus the AIS overlay image
  QPixmap drawPixmap; // pixmap used for the final drawing
  QPixmap chartWaypointPixmap;

  bool initialized;
  Ais  *_aisObj;

private:
  bool asciiToByte(const char *keyAscii, unsigned char keyByte[]);
  bool initColors();
  bool createOverlayCellinRam();
  bool createAISCell();
  bool deleteAISCell();

  bool ensureAISCellEx();
  bool deleteAISCellEx();

  // Waypoint
  bool createWaypointCell();

  QList<Waypoint> waypointList;
  int moveSelectedIndex = -1; // -1 artinya belum ada waypoint dipilih

  void showWaypointError(const QString &message);

  // end Waypoint

  // GuardZone variables
  EcFeature currentGuardZone;  // Handle untuk guardzone saat ini
  int guardZoneWarningLevel;   // Level peringatan
  QPointF guardZoneCenter;  // Titik pusat untuk mode lingkaran
  double pixelsPerNauticalMile;  // Rasio pixel per nautical mile

  GuardZoneManager* guardZoneManager;
  QList<GuardZone> guardZones;
  int nextGuardZoneId;

  // Create GuardZone variables
  bool creatingGuardZone;
  ::GuardZoneShape newGuardZoneShape;
  QVector<QPointF> guardZonePoints;
  QPoint currentMousePos;

  QString feedbackMessage;
  FeedbackType feedbackType;
  QTimer feedbackTimer;
  int flashOpacity;

  // Legacy GuardZone variables (untuk kompatibilitas)
  bool guardZoneActive;
  ::GuardZoneShape guardZoneShape;
  bool guardZoneAttachedToShip;
  double guardZoneCenterLat, guardZoneCenterLon;
  double guardZoneRadius;
  QVector<double> guardZoneLatLons;

  // Methods untuk feedback
  void drawFeedbackOverlay(QPainter& painter);

  void createCircularGuardZoneNew(double centerLat, double centerLon, double radiusNM);
  void createPolygonGuardZoneNew();

  // End Guardzone

  // Alert System
  AlertSystem* alertSystem;

  // Alert monitoring variables
  bool alertMonitoringEnabled;
  QTimer* alertCheckTimer;
  double lastDepthReading;
  QDateTime lastAlertCheck;

  // Alert thresholds and settings
  double depthAlertThreshold;
  double proximityAlertThreshold;
  bool autoDepthMonitoring;
  bool autoProximityMonitoring;

  NavShipStruct navShip;
  NavShipStruct mapShip;

  // Variabel simulasi AIS
  struct SimulatedAISTarget {
      double lat;
      double lon;
      double cog;  // Course over ground (arah) dalam derajat
      double sog;  // Speed over ground (kecepatan) dalam knot
      QString mmsi;
      bool dangerous;
  };
  QList<SimulatedAISTarget> simulatedTargets;
  QTimer* simulationTimer;
  bool simulationActive;
  bool autoCheckGuardZone;
  QDateTime lastSimulationTime;

  SimulationScenario currentScenario;
  QTimer* ownShipTimer;                  // Timer untuk menggerakkan kapal sendiri
  bool ownShipInSimulation;              // Flag apakah kapal sendiri dalam simulasi
  double ownShipSimCourse;               // Arah kapal dalam simulasi
  double ownShipSimSpeed;                // Kecepatan kapal dalam simulasi

  // End

  QThread* threadAIS;
  QTcpSocket* socketAIS;
  std::atomic<bool> stopThread;

  QThread* threadAISMAP;
  QTcpSocket* socketAISMAP;
  std::atomic<bool> stopThreadMAP;

  // LOOPING IF DATA NOT UPDATE

  // TAMBAHAN BARU - CPA/TCPA Variables
  QList<AISTargetData> aisTargets;
  QTimer* cpaUpdateTimer;
  bool showCPAInfo;

  // TAMBAHAN BARU - CPA/TCPA Drawing Methods
  void drawCPAInfo(QPainter& painter);
  void drawCPAInfoBox(QPainter& painter, int x, int y, const AISTargetData& target);
  void drawTargetHeading(QPainter& painter, int x, int y, const AISTargetData& target);



}; // EcWidget

#endif // _ec_widget_h_
