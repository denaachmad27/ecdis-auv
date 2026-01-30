#ifndef _ec_widget_h_
#define _ec_widget_h_

#include <QPixmap>
#include <QWidget>
#include <QtWinExtras/QtWin>

#include <QTimer>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QTextEdit>
#include <QMenu>
#include <QAction>
#include <QMap>
#include <QVector>

// Forward declarations
class TideManager;

// SevenCs Kernel EC2007
#ifdef _WIN32
#include <windows.h>
#pragma pack(push, 4)
#include <eckernel.h>
#include <ecs63.h>
#include <qtoolbutton.h>
#pragma pack (pop)
#else
#include <stdio.h>
#include <X11/Xlib.h>
#include <eckernel.h>
#endif

#include "guardzone.h"
#include "aoi.h"
#include "AISSubscriber.h"
#include "autorouteplanner.h"
#include "autoroutedialog.h"
#include "poi.h"

// GRIB visualization
#include "gribvisualisation.h"
#include "gribdata.h"
class GribManager;

//popup
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QProgressDialog>
#include <QHBoxLayout>

class CPATCPAPanel;
class CurrentVisualisation; // Forward declaration for current visualization
class VisualisationPanel; // Forward declaration for visualization panel
class GribVisualisation; // Forward declaration for GRIB visualization
class SatelliteTileLayer; // Forward declaration for satellite tile layer

struct AISTargetData {
    QString mmsi;
    double lat;
    double lon;
    double cog;         // Course over ground
    double sog;         // Speed over ground
    double heading;
    double cpa;         // Closest Point of Approach (nautical miles)
    double tcpa;        // Time to CPA (minutes)
    bool isDangerous;   // Apakah target berbahaya
    QDateTime lastUpdate;

    double currentRange;    // Current distance in NM
    double relativeBearing; // Relative bearing
    bool cpaCalculationValid; // Whether CPA calculation is valid
    QDateTime cpaCalculatedAt; // When CPA was last calculated

    EcFeature feat;
    EcDictInfo *_dictInfo;

    EcAISTargetInfo rawInfo;
};

// Struktur untuk menyimpan data kapal
struct ShipStruct {
    double lat = std::numeric_limits<double>::quiet_NaN();           // Latitude
    double lon = std::numeric_limits<double>::quiet_NaN();          // Longitude
    double x = std::numeric_limits<double>::quiet_NaN();           // Pixel X
    double y = std::numeric_limits<double>::quiet_NaN();          // Pixel Y
    double heading = std::numeric_limits<double>::quiet_NaN();      // Heading dalam derajat
    double heading_og = std::numeric_limits<double>::quiet_NaN();   // Heading over ground
    double speed = std::numeric_limits<double>::quiet_NaN();        // Kecepatan dalam knot
    double speed_og = std::numeric_limits<double>::quiet_NaN();   // Kecepatan over ground
    double sog = std::numeric_limits<double>::quiet_NaN();   // Kecepatan over ground
    double course_og = std::numeric_limits<double>::quiet_NaN();
    double yaw = std::numeric_limits<double>::quiet_NaN();        // Sudut yaw kapal
    double depth = std::numeric_limits<double>::quiet_NaN();        // Kedalaman
    double draft = std::numeric_limits<double>::quiet_NaN();        // Kedalaman
    double z = std::numeric_limits<double>::quiet_NaN();        // Vertikal kapal
    double stw = std::numeric_limits<double>::quiet_NaN();     // Speed through water
    double drift = std::numeric_limits<double>::quiet_NaN();   // Drift in knots
    double drift_angle = std::numeric_limits<double>::quiet_NaN();  // Drift angle
    double set = std::numeric_limits<double>::quiet_NaN();     // current set
    double rot = std::numeric_limits<double>::quiet_NaN();     // Rate of Turn
    double depth_below_keel = std::numeric_limits<double>::quiet_NaN(); // distance from transducer to seabed'
    QString slat; // String Lat
    QString slon; // String Lon
    QString lat_dms;
    QString lon_dms;
    QString lat_dmm;
    QString lon_dmm;
    QString deadReckon;
};

struct ActiveRouteStruct {
    double rteWpBrg = std::numeric_limits<double>::quiet_NaN();    // Bearing ke waypoint
    QString rteXtd;      // Cross track distance
    double rteCrs = std::numeric_limits<double>::quiet_NaN();      // Course ke waypoint
    double rteCtm = std::numeric_limits<double>::quiet_NaN();      // Current time to waypoint
    double rteDtg = std::numeric_limits<double>::quiet_NaN();      // Distance to go
    double rteDtgM = std::numeric_limits<double>::quiet_NaN();     // Distance to go (manual)
    QString rteTtg;      // Time to go
    QString rteEta;      // Estimated time of arrival
};

// GLOBAL VARAIBLE (DELETE LATER)
extern ShipStruct navShip;
extern ShipStruct mapShip;
extern ActiveRouteStruct activeRoute;

extern QString bottomBarText;
extern QString aivdo;
extern QString nmea;

extern QTextEdit *nmeaText;
extern QTextEdit *aisText;
extern QTextEdit *ownShipText;

extern QTextEdit *aisTemp;
extern QTextEdit *ownShipTemp;

class AlertSystem;
struct AlertData;
class GuardZoneManager; // Forward declaration
class RouteDeviationDetector; // Forward declaration


// Defines for S-63 Chart Import
// The Manufacturer is in charge of generating unique user permits for each installation of his application
// S-63 Uuser permit (must have 28 characters)
//#define USERPERMIT "66B5CBFDF7E4139D5B6086C23130"

// DEVELOPMENT
#define USERPERMIT "E12FF5B1C9BC7AFAC30030B64246"
#define HWID "95C7-DAC8-182A-403D-257C-C"

// PRODUCTION
//#define USERPERMIT "E12FF5B1C9BC7AFAC30030B64246"
//#define HWID "95C7DAC8182A403D257CC"

// The Manufacturer key is supplied by the IHO on request (application required)
// S-63 Manufacturer key (M_KEY must have either 5 or 10 characters)
//#define M_KEY "10121"

#define M_KEY "82115"
#define MID "BF"

// Satellite view display category (custom, not from SevenCs SDK)
#define EC_SATELLITE 4

// Waypoint
#define PICKRADIUS  (0.03 * GetRange)
#define PAN_MARGIN 500

//Waypoint
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include "eblvrm.h"
#include "aitargettracker.h"

// forward declerations1
class PickWindow;
class Ais;
class MainWindow;

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

  // Visualization methods
  void setCurrentVisualisation(CurrentVisualisation* visualisation) { m_currentVisualisation = visualisation; }
  CurrentVisualisation* getCurrentVisualisation() const { return m_currentVisualisation; }

  
  // Toggle visualization display
  void setShowCurrentArrows(bool show);
  void setShowTideRectangles(bool show);
  void refreshVisualization();
  void setCurrentScale(double scale);
  void setTideScale(double scale);

  // Navigation Functions
  enum ActiveFunction {
      PAN,
      CREATE_ROUTE,
      INSERT_WAYP,     // Insert waypoint in middle of route
      REMOVE_WAYP,     // Remove waypoint from route
      EDIT_WAYP,       // Edit waypoint properties
      MOVE_WAYP        // Move waypoint position
  };

  enum DisplayOrientationMode {
      NorthUp,
      HeadUp,
      CourseUp
  };

  enum OSCenteringMode {
      LookAhead,
      Centered,
      Manual,
      AutoRecenter
  };

  struct Waypoint
  {
      // Data SevenCs native
      EcFeature featureHandle;     // Handle EcFeature dari SevenCs

      // Data aplikasi (untuk UI/UX)
      double lat;
      double lon;
      QString label;
      QString remark;
      double turningRadius;
      bool active;
      int routeId;                 // ID route untuk memisahkan route

      // Constructor - inisialisasi dengan invalid handle
      Waypoint() : lat(0), lon(0), turningRadius(10.0), active(true), routeId(0)
      {
          featureHandle.id = EC_NOCELLID;
          featureHandle.offset = 0;
      }

      // Method untuk cek validitas
      bool isValid() const { return ECOK(featureHandle); }
  };

  struct RouteWaypoint
  {
      double lat;
      double lon;
      QString label;
      QString remark;
      double turningRadius;
      bool active;
      
      RouteWaypoint() : lat(0.0), lon(0.0), turningRadius(10.0), active(true) {}
      RouteWaypoint(const Waypoint& wp) : 
          lat(wp.lat), lon(wp.lon), label(wp.label), 
          remark(wp.remark), turningRadius(wp.turningRadius), active(wp.active) {}
  };

  struct Route
  {
      int routeId;
      QString name;
      QString description;
      QDateTime createdDate;
      QDateTime modifiedDate;
      double totalDistance;      // in nautical miles
      double estimatedTime;      // in hours
      bool attachedToShip;       // Route attached to ship status
      QList<RouteWaypoint> waypoints; // Waypoints in this route with full data

      // Active waypoint tracking for navigation
      int activeWaypointIndex;   // Index of target waypoint (0 = first WP)
      double arrivalRadius;      // Radius in NM to consider waypoint "reached"

      // Distance unit visibility controls (default: all shown)
      bool showNmUnit = true;
      bool showYardUnit = true;
      bool showKmUnit = true;
      bool showMilesUnit = false;

      Route() : routeId(0), totalDistance(0.0), estimatedTime(0.0), attachedToShip(false),
                activeWaypointIndex(0), arrivalRadius(0.1) {
          createdDate = QDateTime::currentDateTime();
          modifiedDate = createdDate;
      }
  };

  struct AutoRoutePreviewState
  {
      bool active = false;
      GeoPoint start;
      GeoPoint target;
      AutoRouteOptions options;
      AutoRouteResult result;
  };

  struct AutoRouteStartSelectionState
  {
      bool active = false;
      GeoPoint target;
      GeoPoint shadowStart;
      bool hasShadowPosition = false;
  };

  void setActiveFunction(ActiveFunction func) { activeFunction = func; }

  void drawOverlayCell();

  void drawWaypointMarker(EcCoordinate lat, EcCoordinate lon);
  void drawSingleWaypoint(EcCoordinate lat, EcCoordinate lon, const QString& label, const QColor& color = QColor(255, 140, 0));
  void drawWaypointWithLabel(double lat, double lon, const QString& label, const QColor& color);
  QPoint findOptimalLabelPosition(int waypointX, int waypointY, const QSize& textSize, int minDistance);
  void drawGhostWaypoint(QPainter& painter, EcCoordinate lat, EcCoordinate lon, const QString& label);
  void drawGhostRouteLines(QPainter& painter, EcCoordinate ghostLat, EcCoordinate ghostLon, int routeId, int waypointIndex);
  void drawHighlightedWaypoint(QPainter& painter, double lat, double lon, const QString& label);
  void saveWaypoints();
  void removeWaypointAt(int x, int y);
  void moveWaypointAt(int x, int y);
  void editWaypointAt(int x, int y);
  
  // Context menu functions
  int findWaypointAt(int x, int y);
  int findLeglineAt(int x, int y, int& routeId, int& segmentIndex);
  void showWaypointContextMenu(const QPoint& pos, int waypointIndex);
  QString formatRouteDistance(double nm, bool showNm, bool showYard, bool showKm, bool showMiles);
  void updateRouteDistanceUnit(int routeId, const QString& unit, bool enabled);
  void showLeglineContextMenu(const QPoint& pos, int routeId, int segmentIndex);
  void showMapContextMenu(const QPoint& pos);
  bool resetWaypointCell();
  void drawLeglineLabels();
  void drawRouteNamesOnly(); // Draw route names at center for zoomed out view
  void drawRouteLines(); // Gambar garis route dengan warna berbeda per route (DEPRECATED)
  void drawRouteLinesOverlay(QPainter& painter); // Draw route lines directly to widget like GuardZone
  void drawAutoRoutePreview(QPainter& painter);
  void clearAutoRoutePreview(bool updateDisplay = true);
  void startAutoRouteWorkflow(const QPoint& pos);
  void presentAutoRoutePreview(const AutoRouteResult& result, const AutoRouteOptions& options);
  void commitAutoRoutePreview();

  // Auto route start selection
  void startAutoRouteStartSelection(const GeoPoint& target);
  void drawAutoRouteStartShadow(QPainter& painter);
  void confirmAutoRouteStartSelection(const GeoPoint& start);
  QVector<GeoPoint> simplifyWaypointsByDirection(const QVector<GeoPoint>& waypoints, double angleThresholdDeg);

  void iconUpdate(bool);

  void loadWaypoints();
  QString getWaypointFilePath() const;
  int getWaypointCount() const { return waypointList.size(); }
  QList<Waypoint> getWaypoints() const { return waypointList; }
  void clearWaypoints();
  void updateWaypointActiveStatus(int routeId, double lat, double lon, bool active);
  void replaceWaypointsForRoute(int routeId, const QList<Waypoint>& newWaypoints);
  bool deleteRouteWaypointAt(int routeId, int indexInRoute);
  
  // Waypoint highlighting for route panel visualization
  void highlightWaypoint(int routeId, int waypointIndex);
  void clearWaypointHighlight();
  
  // Route management functions
  void saveRoutes();
  void updateRouteFromWaypoint(int routeId);
  void loadRoutes();
  QString getRouteFilePath() const;
  void saveCurrentRoute();
  QList<Route> getRoutes() const { return routeList; }
  Route getRouteById(int routeId) const;
  void calculateRouteData(Route& route);
  void convertSingleWaypointsToRoutes(); // Convert legacy waypoints to routes
  void convertRoutesToWaypoints(); // Convert loaded routes to waypoints
  void updateRouteList(int routeId); // Update routeList when waypoints change
  bool exportWaypointsToFile(const QString &filename);
  bool importWaypointsFromFile(const QString &filename);
  bool initializeWaypointSystem();
  bool renameRoute(int routeId, const QString& newName);
  
  // Route library (save/load without file picker)
  bool saveRouteToLibrary(int routeId);
  bool saveRouteToLibraryAs(int routeId, const QString& name);
  bool loadRouteFromLibrary(const QString& routeName);
  QStringList listSavedRouteNames() const;
  QString getRouteLibraryFilePath() const;
  
  // Route color customization
  void setRouteCustomColor(int routeId, const QColor& color);
  QColor getBaseRouteColor(int routeId) const; // returns custom or default base color

  // Route reverse
  bool reverseRoute(int routeId);

  // Route mode methods
  void startRouteMode();
  void startAppendWaypointMode(int routeId);
  void endRouteMode();
  void finalizeCurrentRoute();
  void resetRouteConnections();
  void drawRoutesSeparately();
  EcCellId getOrCreateRouteCellId(int routeId);
  EcCellId createNewRouteCellId();
  bool isInRouteMode() const { return isRouteMode; }

  // Quick create route (simple form)
  void showCreateRouteQuickDialog();

  // AOI/ROI lightweight management
  QList<AOI> getAOIs() const { return aoiList; }
  int getNextAoiId() const { return nextAoiId; }
  void addAOI(const AOI& aoi);
  void removeAOI(int id);
  void toggleAOIVisibility(int id);
  void setAOIVisibility(int id, bool visible);
  void setAOILabelVisibility(int id, bool showLabel);
  bool updateAOI(const AOI& aoi);
  void drawAOIs(QPainter& painter);
  void startAOICreation(const QString& name, AOIType type);
  void startAOICreationWithColor(const QString& name, AOIColorChoice colorChoice);
  void cancelAOICreation();
  void finishAOICreation();
  bool isCreatingAOI() const { return creatingAOI; }

  // Route Deviation Detector getter
  RouteDeviationDetector* getRouteDeviationDetector() const { return routeDeviationDetector; }

  // AOI attach-to-ship behavior (single active AOI like route)
  void attachAOIToShip(int aoiId);   // pass -1 to detach all
  bool isAOIAttachedToShip(int aoiId) const;
  int  getAttachedAOIId() const { return attachedAoiId; }
  bool hasAttachedAOI() const { return attachedAoiId >= 0; }
  // AOI edit
  void startEditAOI(int aoiId);
  void finishEditAOI();
  void cancelEditAOI();
  bool exportAOIsToFile(const QString& filename);
  // AOI persistence
  void saveAOIs();
  void loadAOIs();
  QString getAOIFilePath() const;

  // POI management
  int addPoi(const PoiEntry& poi);
  bool updatePoi(int poiId, const PoiEntry& poi);
  bool removePoi(int poiId);
  bool setPoiActive(int poiId, bool active);
  bool setPoiLabelVisible(int poiId, bool show);
  QVector<PoiEntry> poiEntries() const;
  PoiEntry poiEntry(int poiId) const;
  bool focusPoi(int poiId);
  void setShowPoiLabels(bool on) { showPoiLabels = on; update(); }
  bool getShowPoiLabels() const { return showPoiLabels; }
  void showPOIDialogDirect(const PoiEntry& poi);
  // POI persistence
  void savePois();
  void loadPois();
  QString getPOIFilePath() const;
  double estimateDepthAt(EcCoordinate lat, EcCoordinate lon);

  DisplayOrientationMode displayOrientation = NorthUp;
  OSCenteringMode osCentering = Centered;

  DisplayOrientationMode orientation;
  OSCenteringMode centering;

  // End Waypoint
  //////////////////////////////////////////////////////////////////////

  // Guardzone

  enum FeedbackType {
      FEEDBACK_SUCCESS,
      FEEDBACK_WARNING,
      FEEDBACK_ERROR,
      FEEDBACK_INFO
  };

  void testRedDot();

  void setGuardZoneAttachedToShip(bool attached);
  void generateTargetsTowardsGuardZone(int count);
  void setShipGuardianEnabled(bool enabled);
  bool isShipGuardianEnabled() const;
  void setGuardianRadius(double radius);
  double getGuardianRadius() const;

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
  void drawSectorGuardZone(QPainter& painter, const GuardZone& gz, int& labelX, int& labelY);

  void createRedDotGuardian();
  void removeRedDotGuardian();
  void updateRedDotGuardianInManager();

  void createAttachedGuardZone();
  void removeAttachedGuardZone();
  void cleanupDuplicateAttachedGuardZones();

  bool isGuardZoneAutoCheckEnabled() const { return guardZoneAutoCheckEnabled; }
  int getGuardZoneCheckInterval() const { return guardZoneCheckInterval; }

  struct DetectedObstacle {
      QString type;
      QString name;
      QString description;
      int level;              // 1=Note, 2=Warning, 3=Danger
      double lat, lon;
      double distance;
      double bearing;

      DetectedObstacle() : level(1), lat(0.0), lon(0.0), distance(0.0), bearing(0.0) {}
  };

  // End Guardzone

  // Test GuardZone methods
  void setTestGuardZoneEnabled(bool enabled);
  bool isTestGuardZoneEnabled() const;
  void setTestGuardZoneRadius(double radius);
  double getTestGuardZoneRadius() const;

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
  void checkPickReportObstaclesInShipGuardian();
  QString extractInformationFromFeature(const EcFeature& feature);
  QString extractObjectNameFromFeature(const EcFeature& feature);

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
  // Import S-63 Exchange Set using a specific permit file (filtered entries)
  bool ImportS63ExchangeSetWithPermitFile(const QString & dir, const QString & permitFilePath);

  // Thread-friendly versions (no UI interaction); return false and set error on failure
  bool ImportS63ExchangeSetSilent(const QString & dir, QString* errorOut);
  bool ImportS63ExchangeSetWithPermitFileSilent(const QString & dir, const QString & permitFilePath, QString* errorOut);

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
  void ShowOwnship(bool on);

  // Satellite view layer
  void ShowSatelliteLayer(bool on);
  bool isSatelliteLayerEnabled() const { return showSatelliteLayer; }
  void updateSatelliteTiles();
  void drawSatelliteTilesOverlay();  // Draw tiles to drawPixmap with alpha blending
  void drawSatelliteTilesToChart();  // Draw tiles to chartPixmap (no flicker)
#ifdef _WIN32
  void drawSatelliteTilesToHdc(HDC targetHdc);  // Draw tiles to HDC before chart (Windows only)
#endif
  void drawSatelliteTiles(QPainter& painter, int zoomLevel, double viewMinLat, double viewMaxLat,
                          double viewMinLon, double viewMaxLon, int offsetX, int offsetY);

  // Swiches the track of AIS Target
  void TrackTarget(QString mmsi);
  bool isTrackTarget();
  void TrackShip(bool on);
  void ShowDangerTarget(bool on);

  void applyShipDimensions();

  // Ship Guardian Circle variables (upgrade dari red dot)
  bool shipGuardianEnabled;           // NEW: Enable Guardian Circle
  double guardianRadius;              // NEW: Guardian circle radius (nautical miles)
  QColor guardianFillColor;           // NEW: Fill color for guardian area
  QColor guardianBorderColor;         // NEW: Border color

  // Red Dot Tracker methods (TAMBAHKAN INI)
  void setRedDotTrackerEnabled(bool enabled);
  void setRedDotAttachedToShip(bool attached);
  bool isRedDotTrackerEnabled() const;
  bool isRedDotAttachedToShip() const;
  bool hasAttachedGuardZone() const;

  // Transforms device coordinates from this widget to geodetic coordinates (WGS84)
  virtual bool XyToLatLon (int x, int y, EcCoordinate & lat, EcCoordinate & lon);

  // Transforms geodetic coordinates (WGS84) to device coordinates
  virtual bool LatLonToXy (EcCoordinate lat, EcCoordinate lon, int & x, int & y);

  // Draws the chart
  void Draw();
  void waypointDraw();
  void ownShipDraw();
  void setCustomOwnship(bool state);

  // Waypoint
  // Custom colors per routeId
  QMap<int, QColor> routeCustomColors;
  void SetWaypointPos(EcCoordinate lat, EcCoordinate lon);
  bool drawUdo(void);
  ActiveFunction    activeFunction;
  
  // Waypoint form functions
  void showAddWaypointDialog();
  void showCreateRouteDialog();
  void showEditRouteDialog(int routeId);
  void createWaypointFromForm(double lat, double lon, const QString& label, const QString& remark, int routeId, double turningRadius, bool active = true);
  bool createUdoCell();

  bool getOwnShipTrail();
  void setOwnShipTrail(bool);

  // GuardZone
  void enableGuardZone(bool enable);
  bool isGuardZoneActive() const { return guardZoneActive; }
  void startCreateGuardZone(::GuardZoneShape shape);
  void finishCreateGuardZone();
  void cancelCreateGuardZone();
  void checkGuardZone();
  QString getGuardZoneFilePath() const;
  bool creatingGuardZone;
  GuardZoneManager* guardZoneManager;

  // Fungsi deteksi obstacles untuk Ship Guardian Zone
  bool checkShipGuardianZone();
  bool checkAISTargetsInShipGuardian();
  bool checkStaticObstaclesInShipGuardian();
  void triggerShipGuardianAlert(const QList<DetectedObstacle>& obstacles);

  // Auto-check functions
  void setShipGuardianAutoCheck(bool enabled);
  bool isShipGuardianAutoCheckEnabled() const;

  // List untuk menyimpan obstacles yang terdeteksi
  QList<DetectedObstacle> lastDetectedObstacles;
  
  // Obstacle marker visualization
  struct ObstacleMarker {
      double lat;
      double lon;
      QString dangerLevel;
      QString objectName;
      QString information;
      QDateTime timestamp;
  };
  
  QList<ObstacleMarker> obstacleMarkers;
  void addObstacleMarker(double lat, double lon, const QString& dangerLevel, 
                        const QString& objectName, const QString& information);
  void clearObstacleMarkers();
  void removeOutdatedObstacleMarkers();
  void drawObstacleMarkers(QPainter& painter);
  
  // Chart flashing for dangerous obstacles
  bool hasDangerousObstacles() const;
  void drawChartFlashOverlay(QPainter& painter);
  QTimer* chartFlashTimer;
  bool chartFlashVisible;
  void startChartFlashing();
  void stopChartFlashing();

  // Animation timer for waypoint highlighting
  QTimer* waypointAnimationTimer;

  // Helper functions (private)
  bool checkAISTargetsInShipGuardian(QList<DetectedObstacle>& obstacles);
  bool checkStaticObstaclesInShipGuardian(QList<DetectedObstacle>& obstacles);
  void showShipGuardianAlert(const QList<DetectedObstacle>& obstacles);

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
  void ReadAISLogfileWDelay( const QString& );
  void readAISVariableString( const QString& );
  void ReadAISVariable( const QStringList& );
  void StopReadAISVariable();
  QString StartReadAISSubscribe();
  QString StartReadAISSubscribeSSH();
  void startServerMOOSSubscribe();
  void ReadFromServer( const QString& );

  void createDvrRead();

  void startAISSubscribe();
  void startAISConnection();
  void startConnectionAgain();
  void stopAISConnection();
  void stopAllThread();
  void processAISJson(const QByteArray&);
  void processData(double, double, double, double, double, double, double, double, double);
  void publishNavInfo(double, double);
  void processDataQuickFix(double, double, double, double, double, double, double, double, double);
  void processMapInfoReq(QString);
  void processAis(QString);
  void publishToMOOSDB(QString, QString);
  void publishToMOOS(QString, QString);

  QString latLonToDegMin(double value, bool isLatitude);

  QString convertJsonData(const QString &jsonString);
  void publishRoutesToMOOSDB(const QString data);
  void updateOwnshipCache(bool cache);

  // Method untuk akses AIS data dari luar
  bool hasAISData() const;
  QList<AISTargetData> getAISTargets() const;
  int getAISTargetCount() const;

  // CPA TCPA
  void setCPAPanelToAIS(CPATCPAPanel* panel);
  void setClosestCPA(double val);
  double getClosestCPA() const;

  void setClosestAIS(AISTargetData val);
  AISTargetData getClosestAIS() const;

  void setDangerousAISList(const QList<AISTargetData>& list);
  QList<AISTargetData> getDangerousAISList() const;

  void addDangerousAISTarget(const AISTargetData& target); // opsional
  void clearDangerousAISList(); // opsional

  // GUARDIAN AIS
  void drawShipGuardianSquare(double aisLat, double aisLon);      // NEW: Draw warning square area

  EcAISTargetInfo* findAISTargetInfoAtPosition(const QPoint& mousePos);
  QString getTrackMMSI();
  void setAISTrack(AISTargetData aisTrack);

  void setMainWindow(MainWindow*);

  // OWNSHIP TRAIL
  QList<QPair<QString, QString>> ownShipTrailPoints;
  void clearOwnShipTrail();
  double haversine(double lat1, double lon1, double lat2, double lon2);

  // GETTER SETTER AISSUB VAR
  AISSubscriber* getAisSub() const;
  Ais* getAis() const { return _aisObj; }
  void clearAisTargets();

  // DISPLAY CATEGORY
  void setDisplayCategoryInternal(int category);
  int getDisplayCategory();

  // WP CLICK
  void waypointRightClick(QMouseEvent *e);
  void waypointLeftClick(QMouseEvent *e);

  // Show hazard info popup at a given position
  void showHazardInfoAt(double lat, double lon);

  // ACTION
  QAction* createRouteAction;
  QAction* goHereAutoRouteAction;
  QAction* pickInfoAction;
  QAction* warningInfoAction;
  QAction* measureEblVrmAction;

  // SPEED AVG
  QVector<QPair<QDateTime, double>> speedBuffer;
  double avgSpeed1Min = 0.0;
  QTimer slidingAvgTimer;

  double getSpeedAverage();

  // EBL/VRM helpers
  void setEblVrmFixedTarget(double lat, double lon);

  // Tidal station helpers
  void drawTidalStations();
  void setTidalStationsVisible(bool visible);
  bool areTidalStationsVisible() const { return m_showTidalStations; }
  void updateTidalStationClick(EcCoordinate lat, EcCoordinate lon);
  void setTidalStationManager(class TideManager* manager) { m_tideManager = manager; }

  //
  int rangeNM = 0;
  QDateTime lastRedrawTime; // throttle external redraws

  void defaultSettingsStartUp();

public slots:
  void updateAISTargetsList();
  void addOrUpdateAISTarget(const AISTargetData& target);

  // NMEA Playback control methods
  void startNmeaPlaybackTimer();
  void stopNmeaPlaybackTimer();
  void pauseNmeaPlaybackTimer();

  // waypoint
  void createWaypointAt(EcCoordinate lat, EcCoordinate lon);
  bool createWaypointInRoute(int routeId, double lat, double lon, const QString& label = "");
  void createSeparateRouteWaypoint(const Waypoint &waypoint);
  void createSingleWaypoint(const Waypoint &waypoint);
  void insertWaypointAt(EcCoordinate lat, EcCoordinate lon);
  bool insertWaypointAtPosition(int routeId, const Waypoint& newWaypoint, int position);
  double distanceToLineSegment(double px, double py, double x1, double y1, double x2, double y2);
  void updateRouteLabels(int routeId);
  QColor getRouteColor(int routeId);
  bool deleteRoute(int routeId);
  void forceRedraw();
  void immediateRedraw(); // Immediate redraw for UI updates
  // 1 Hz tick to sync panels

  void setRouteVisibility(int routeId, bool visible);
  bool isRouteVisible(int routeId) const;
  void setRouteAttachedToShip(int routeId, bool attached);
  bool isRouteAttachedToShip(int routeId) const;
  void attachRouteToShip(int routeId); // Attach route to ship, detach others
  int getAttachedRouteId() const;
  bool hasAttachedRoute() const; // Check if any route is attached
  void setSelectedRoute(int routeId);
  int getSelectedRoute() const { return selectedRouteId; }
  int getNextAvailableRouteId() const; // Find lowest available route ID

  // Guardzone
  void performAutoGuardZoneCheck();
  void setGuardZoneAutoCheck(bool enabled);
  void setGuardZoneCheckInterval(int intervalMs);

signals:
  void poiListChanged();
  void editPOIRequested(int poiId, bool* handled = nullptr);
  // Drawing signals
  void mouseMove(EcCoordinate, EcCoordinate);
  void mouseRightClick(QPoint);
  void projection();
  void scale(int);
  
  // Dangerous obstacle alarm signals
  void dangerousObstacleDetected();
  void dangerousObstacleCleared();

  // DENC signals
  void import(const char *, bool &);
  void replace(const char *, bool &);

  // Waypoint Signals
  void waypointCreated();
  // 1 Hz tick to sync panels
  void tickPerSecond();

  // GuardZone Signals
  void statusMessage(const QString &message);
  void debugAISTargets();
  void guardZoneTargetDetected(int guardZoneId, int targetCount);
  void aisTargetDetected(int guardZoneId, int mmsi, const QString& message);
  void pickReportObstacleDetected(int guardZoneId, const QString& details);

  // GuardZone signals untuk panel
  void guardZoneCreated();
  void guardZoneModified();
  void guardZoneDeleted();
  void attachToShipStateChanged(bool attached);
  // AOI signals
  void aoiListChanged();

  // Alert system signals
  void alertTriggered(const AlertData& alert);
  void criticalAlertTriggered(const AlertData& alert);
  void alertSystemStatusChanged(bool enabled);

  // CONNECTION STATUS
  void aisSubCreated(AISSubscriber *subscriber);

  // ROUTE
  void updateEta();

  // Tracking
  void trackTargetChanged(const QString& mmsi);

  void ownshipCache(bool cache);

private slots:
  void slotUpdateAISTargets( Bool bSymbolize );
  void slotRefreshChartDisplay( double lat, double lon, double head );
  void slotRefreshChartDisplayThread( double lat, double lon, double head );
  void slotRefreshCenter( double lat, double lon );

  // Alert Systems
  void onAlertTriggered(const AlertData& alert);
  void onCriticalAlert(const AlertData& alert);
  void onAlertSystemStatusChanged(bool enabled);
  void performPeriodicAlertChecks();

  //popup
  void checkMouseOverAISTarget();

protected:
  // Drawing functions
  void clearBackground();

  virtual void draw (bool update);
  virtual void drawWorks (bool update);
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
public:
  // Ownship getters for helpers
  inline EcCoordinate getOwnShipLat() const { return ownShip.lat; }
  inline EcCoordinate getOwnShipLon() const { return ownShip.lon; }
  inline bool isReady() const { return initialized && (view != nullptr); }
  inline int getTrackLine() const {return trackLine;}
  inline int getTrackDistance() const {return trackDistance;}
  inline int getTrackMinute() const {return trackMinute;}
  inline ShipStruct getNavShip() const {return navShip;}
  inline EcDictInfo* getDictionaryInfo() const { return dictInfo; }

  // EBL/VRM
  void setEblEnabled(bool on) { eblvrm.setEblEnabled(on); update(); }
  void setVrmEnabled(bool on) { eblvrm.setVrmEnabled(on); update(); }
  void setEblVrmMeasureMode(bool on) { eblvrm.setMeasureMode(on); update(); }

  // AI Target Tracker
  void setAITarget(const QString& mmsi, const QString& name = "");
  void clearAITarget();
  void updateAITargetData(const QString& mmsi, const AISTargetData& target);
  bool isAITargetTracking() const { return aiTargetTracker.trackingEnabled; }
  bool isPointNearAITargetLine(const QPoint& clickPos, int tolerance = 10);
  void toggleInterceptVisualization();
  void setShipDotEnabled(bool on) { shipDotEnabled = on; update(); }

  // Red Dot Tracker variables
  bool redDotTrackerEnabled;
  bool redDotAttachedToShip;
  EcCoordinate redDotLat, redDotLon;
  QColor redDotColor;
  double redDotSize;
  bool shipDotEnabled = false;

  // Waypoint
  EcFeature         wp1;
  EcCoordinate      wplat, wplon;
  int range;
  virtual void drawWaypointCell ();

  // GuardZone
  void drawGuardZone(QPainter& painter);
  void updateAttachedGuardZoneFromNavShip();
  void drawShipDot(QPainter& painter);

  // Route Deviation Detector
  void drawRouteDeviationIndicator(QPainter& painter);
  void drawWaypointsOverlay(QPainter& painter);
  void initializeRouteDeviationDetector();
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
  bool             showOwnship;
  bool             showNationalText;
  EcCoordinate     currentLat, currentLon;
  int              currentLookupTable;
  int              currentScale;
  double           currentHeading;

  QString          trackTarget, showDangerTarget;
  bool             trackShip;

  // PICKWINDOW
	PickWindow *pickWindow;
	int         pickX, pickY;

  // EBL/VRM manager
  EblVrm eblvrm;

  // AI Target Tracker for missile targeting
  AITargetTracker aiTargetTracker;

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
  QColor  textColor;
  QPixmap chartPixmap; // contains and stores the chart image
  QPixmap chartAisPixmap; // contains the chart image plus the AIS overlay image
  QPixmap drawPixmap; // pixmap used for the final drawing
  QPixmap chartWaypointPixmap;

  bool initialized;
  Ais  *_aisObj;

  // AOI store
  QList<AOI> aoiList;
  int nextAoiId = 1;
  bool creatingAOI = false;
  QString pendingAOIName;
  AOIType pendingAOIType = AOIType::AOI;
  QColor pendingAOIColor;
  QVector<QPointF> aoiVerticesLatLon; // lat,lon pairs during creation
  bool editingAOI = false;
  int editingAoiId = -1;  
  int draggedAoiVertex = -1;

  int lastAoiId;
  int lastVertexIndex;

  double handleDetectRadiusPx = 10.0;
  // AOI vertex drag ghost preview
  bool aoiVertexDragging = false;
  double aoiGhostLat = 0.0;
  double aoiGhostLon = 0.0;
  // AOI attach state
  int attachedAoiId = -1;
  // AOI screen cache for performance
  QPolygon attachedAoiScreenCache;        // Cached screen-space polygon of attached AOI
  int attachedAoiScreenCacheForId = -1;   // AOI id for which cache is valid
  quint64 attachedAoiScreenCacheViewVersion = 0; // View version when cache was built
  quint64 viewChangeCounter = 0;          // Incremented when view (center/scale/heading) changes
  QRect attachedAoiScreenCacheBounds;     // Cached bounding rect for quick reject
  // Throttled containment check state
  bool cachedOwnshipOutsideAoi = false;   // Result cache of last check
  bool cachedOwnshipOutsideAoiCopy = false;   // Result cache of last check
  QPoint lastOwnshipScreenForAoiCheck = QPoint(0,0);
  QElapsedTimer aoiContainmentTimer;      // Timer to throttle containment checks
  qint64 lastAoiContainmentCheckMs = -1;  // Last check timestamp in ms
  // AOI render options
  bool enableAoiSegmentLabels = false; // safety default: off
  bool showAoiLabels = true;           // master toggle for AOI labels

  // Satellite tile layer
  SatelliteTileLayer *satelliteLayer;
  bool showSatelliteLayer = false;     // Enable/disable satellite view

  // POI store
  QVector<PoiEntry> poiList;
  int nextPoiId = 1;
  int highlightedPoiId = -1;
  bool showPoiLabels = true;

  // POI move state
  int movingPoiId = -1;
  QPoint movingPoiStartPos;
  bool isMovingPoi = false;

  AOI *lastAoiList;
  int lastBestSeg;
  EcCoordinate lastLat, lastLon;

public:
  void setEnableAoiSegmentLabels(bool on) { enableAoiSegmentLabels = on; update(); }
  void setShowAoiLabels(bool on) { showAoiLabels = on; update(); }
  // AOI hover segment label state (hover-only labeling)
  void updateAoiHoverLabel(const QPoint& mousePos);
  int hoverAoiId = -1;
  int hoverAoiEdgeIndex = -1;
  QPoint hoverAoiLabelScreenPos;
  QString hoverAoiLabelText;

  // AOI context helpers
  void showAoiVertexContextMenu(const QPoint& pos, int aoiId, int vertexIndex);
  void showAoiToolbox(const QPoint& pos, int aoiId, int vertexIndex);
  bool getAoiVertexAtPosition(int x, int y, int& outAoiId, int& outVertexIndex);

  //popup
  void leaveEvent(QEvent *event) override;
  void hideToolbox();
  bool isDragging = false;

private:
  MainWindow *mainWindow = nullptr;

  bool asciiToByte(const char *keyAscii, unsigned char keyByte[]);
  bool initColors();
  bool createOverlayCellinRam();
  bool createAISCell();
  bool deleteAISCell();
  int findPoiIndex(int poiId) const;

  bool ensureAISCellEx();
  bool deleteAISCellEx();

  // Waypoint
  bool createWaypointCell();

  QList<Waypoint> waypointList;
  QList<Route> routeList;
  AutoRoutePreviewState autoRoutePreview;
  AutoRouteStartSelectionState autoRouteStartSelection;
  RouteDeviationDetector* routeDeviationDetector = nullptr;
  QMap<int, bool> routeVisibility; // Track visibility per route
  int selectedRouteId = -1; // Currently selected route for visual feedback
  int moveSelectedIndex = -1; // -1 artinya belum ada waypoint dipilih
  
  // Route mode variables
  bool isRouteMode = false;
  int routeWaypointCounter = 1;
  int currentRouteId = 1;
  QMap<int, EcCellId> routeCells; // Separate cell for each route
  
  // Ghost waypoint untuk preview saat move
  struct GhostWaypoint {
      bool visible;
      double lat;
      double lon;
      QString label;
      int routeId;
      int waypointIndex;  // Index of waypoint being moved
      
      GhostWaypoint() : visible(false), lat(0), lon(0), routeId(0), waypointIndex(-1) {}
  } ghostWaypoint;

  // Label collision tracking (cleared at start of each Draw())
  QList<QRect> usedLabelRects;

  // Highlighted waypoint for route panel selection visualization
  struct HighlightedWaypoint {
      bool visible;
      double lat;
      double lon;
      QString label;
      int routeId;
      int waypointIndex;

      HighlightedWaypoint() : visible(false), lat(0), lon(0), routeId(0), waypointIndex(-1) {}
  } highlightedWaypoint;

  void showWaypointError(const QString &message);

  // end Waypoint

  // GuardZone variables
  EcFeature currentGuardZone;  // Handle untuk guardzone saat ini
  int guardZoneWarningLevel;   // Level peringatan
  QPointF guardZoneCenter;  // Titik pusat untuk mode lingkaran
  double pixelsPerNauticalMile;  // Rasio pixel per nautical mile
  bool isPointInPolygon(double lat, double lon, const QVector<double>& polygonLatLons);
  bool checkPointInPolygonGeographic(double lat, double lon, const QVector<double>& polygonLatLons);
  bool checkPointInPolygonScreen(double lat, double lon, const QVector<double>& polygonLatLons);
  double calculateCrossProduct(double pointLat, double pointLon, double lat1, double lon1, double lat2, double lon2);
  bool isPointInSemicircle(double lat, double lon, const GuardZone* gz);

  QList<GuardZone> guardZones;
  int nextGuardZoneId;

  // Create GuardZone variables
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
  void drawPois(QPainter& painter);

  void createCircularGuardZoneNew(double centerLat, double centerLon, double radiusNM);
  void createPolygonGuardZoneNew();

  bool redDotGuardianEnabled;
  int redDotGuardianId;           // ID untuk GuardZone Manager
  QString redDotGuardianName;     // Nama untuk GuardZone Manager

  int attachedGuardZoneId;           // ID guardzone untuk "Attach to Ship"
  QString attachedGuardZoneName;     // Nama guardzone

  // Auto-check timer untuk real-time detection
  QTimer *guardZoneAutoCheckTimer;
  bool guardZoneAutoCheckEnabled;
  int guardZoneCheckInterval; // dalam milliseconds

  // Cache untuk tracking target status
  QSet<unsigned int> previousTargetsInZone; // Legacy - untuk backward compatibility
  QMap<int, QSet<unsigned int>> previousTargetsPerZone; // Per guardzone tracking
  QDateTime lastGuardZoneCheck;

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

  //NavShipStruct navShip;
  //NavShipStruct mapShip;

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

  //QThread* threadAIS;
  QTcpSocket* socketAIS;
  std::atomic<bool> stopThread;

  // NEW SUBS FUNC
  QThread* threadAIS = nullptr;
  AISSubscriber *subscriber = nullptr;

  QThread* threadAISMAP;
  QTcpSocket* socketAISMAP;
  std::atomic<bool> stopThreadMAP;
  
  // AIS targets storage
  QList<AISTargetData> currentAISTargets;
  mutable QMutex aisTargetsMutex;

  // ========== RED DOT TRACKER VARIABLES ==========
  bool attachedToShip;                // Flag untuk attachment ke ship
  QSet<QString> previousDetectedObstacles; // Track obstacles yang sudah terdeteksi
  // ==============================================

  // ========== RED DOT TRACKER METHODS ==========
  void drawRedDotTracker();           // Keep existing (now draws guardian circle)
  void updateRedDotPosition(double lat, double lon);  // Keep existing
  void drawShipGuardianCircle();      // NEW: Draw guardian circle area
  // ============================================

  //popup
  // AIS Tooltip variables
  QFrame* aisTooltip;
  QLabel* tooltipObjectName;
  QLabel* tooltipShipBreadth;
  QLabel* tooltipShipLength;
  QLabel* tooltipCOG;
  QLabel* tooltipSOG;
  QLabel* tooltipShipDraft;
  QLabel* tooltipTypeOfShip;
  QLabel* tooltipNavStatus;
  QLabel* tooltipMMSI;
  QLabel* tooltipCallSign;
  QLabel* tooltipPositionSensor;
  QLabel* tooltipTrackStatus;
  QLabel* tooltipListOfPorts;
  QLabel* tooltipAntennaLocation;
  QLabel* tooltipRangeBearing;

  QTimer* aisTooltipUpdateTimer = nullptr;
  EcAISTargetInfo* currentTooltipTarget = nullptr;

  // Helper functions for tooltip
  void createAISTooltip();
  void showAISTooltip(const QPoint& position, const AISTargetData& targetData);
  void hideAISTooltip();
  
  // Obstacle detection area visualization
  void drawObstacleDetectionArea(QPainter& painter);

  QTimer* aisTooltipTimer;
  QPoint lastMousePos;
  bool isAISTooltipVisible;

  // Helper functions
  AISTargetData* findAISTargetAtPosition(const QPoint& mousePos);
  AISTargetData getEnhancedAISTargetData(const QString& mmsi);

  QString getShipTypeString(int shipType);
  QString getNavStatusString(int navStatus);

  void showAISTooltipFromTargetInfo(const QPoint& position, EcAISTargetInfo* targetInfo);
  void updateTooltipIfVisible();
  void updateAISTooltipContent(EcAISTargetInfo* targetInfo);

  void getAISDataFromFeature(EcFeature feature, QString& objectName, QString& shipBreadth,
                             QString& shipLength, QString& cog, QString& sog, QString& shipDraft,
                             QString& typeOfShip, QString& navStatus, QString& mmsi,
                             QString& callSign, QString& trackStatus, QString& listOfPorts,
                             QString& antennaLocation);
  void updateAISTooltipFromFeature(EcFeature feature);
  void showAISTooltipFromFeature(const QPoint& position, EcFeature feature);
  unsigned int currentHoveredMMSI;
  EcFeature findAISFeatureAtPosition(const QPoint& mousePos);
  void updateAISTooltipFromMMSI(unsigned int mmsi);
  void showAISTooltipFromMMSI(const QPoint& position, unsigned int mmsi);

  // icon vessel
  void drawOwnShipIcon(QPainter& painter, int x, int y, double cog, double heading, double sog);
  void drawOwnShipVectors(QPainter& painter, int x, int y, double cog, double heading, double sog, double actualLength, bool actualSize);
  void drawTurningPrediction(QPainter& painter, double shipLat, double shipLon, double heading, double cog, double sog, double rot);
  void drawShipOutlineAt(QPainter& painter, int x, int y, double heading, double alpha = 100.0);

  // Visual indicators for CPA/TCPA warnings
  void drawPulsingWarning(QPainter& painter, int x, int y, const QColor& color, int size);
  void drawWarningTriangle(QPainter& painter, int x, int y, int size, const QColor& color);
  void drawCPATCPAIndicators(QPainter& painter);

  AISTargetData ownShipData;
  bool showCustomOwnShip = false; // Mulai tersembunyi sampai posisi ownship valid
  bool showOwnShipTrail;

  // Test GuardZone properties
  bool testGuardZoneEnabled;
  double testGuardZoneRadius;  // dalam nautical miles
  QColor testGuardZoneColor;

  // Test GuardZone drawing
  void drawTestGuardZone(QPainter& painter);
  void drawTestGuardSquare(QPainter& painter);

  // CLOSEST CPA VAR
  AISTargetData closestAIS;
  QList<AISTargetData> dangerousAISList;
  double closestCPA = 999;
  double closestLat = -1;
  double closestLon = -1;

  // Auto-check timer untuk Ship Guardian Zone
  QTimer* shipGuardianCheckTimer;
  bool shipGuardianAutoCheck;
  QDateTime lastShipGuardianCheck;

  // MOOSDB VAR
  ShipStruct mapInfo;
  ShipStruct navInfo;

  // OWNSHIP TRACK VAR
  void addOwnShipPoint(double, double);
  void drawOwnShipTrail(QPainter &painter);

  // AUTO RECENTER
  QRect GetVisibleMapRect();
  // Convert geographic to on-screen widget point, respecting drag translation
  bool latLonToWidgetPoint(double lat, double lon, QPoint& out);

  // DISPLAY VAR
  int displayCategory;

  QWidget* waypointToolbox = nullptr;
  int currentWaypointIndex = -1;

  void createWaypointToolbox(const QPoint& pos, int waypointIndex);
  void createLeglineToolbox(const QPoint& pos, int routeId, int segmentIndex);

  QDialog *toolbox = nullptr;
  QDialog *toolboxLL = nullptr;
  QDialog *toolboxAoi = nullptr;
  QDialog *toolboxAoiCreate = nullptr;
  EcCoordinate toolboxLat, toolboxLon;

  QMutex aisDataMutex;
  QElapsedTimer aisGuiTimer;

  // Struct snapshot untuk thread-safe copy
  struct AISSnapshot {
      double lat = 0;
      double lon = 0;
      double heading = 0;
      ShipStruct navShip;
  };
  AISSnapshot lastSnapshot;

  // DRAW TIME
  void drawPerTime();
  bool canRun = true;
  QTimer timer;

  // NAV INFO TIME
  void publishPerTime();
  bool canPublish = true;
  QTimer timerPublish;

  // ALL TIME
  void allFunctionPerTime(PickWindow *pickWindow);
  QTimer allTimer;
  QTimer dbTimer;
  bool canWork = true;
  bool canRecord = true;

  // NMEA PLAYBACK TIME
  void allFunctionPerTimeNMEA(PickWindow *pickWindow);
  QTimer nmeaPlaybackTimer;
  bool canWorkNMEA = true;

  // RIGHT CLICK ICON
  QAction* editAction;
  QAction* moveAction;
  QAction* deleteWaypointAction;
  QAction* deleteRouteAction;
  QAction* publishAction;

  QAction* insertWaypointAction;

  QToolButton *btn;
  QToolButton *btn1;
  QToolButton *btn2;
  QToolButton *btn3;
  QToolButton *btn4;

  QToolButton *btnCreate;
  QToolButton *btnDelete;
  QToolButton *btnMove;

  QIcon editIcon;
  QIcon moveIcon;
  QIcon deleteWaypointIcon;
  QIcon deleteRouteIcon;
  QIcon publishIcon;

  QIcon insertWaypointIcon;

  QFrame *frame;
  QFrame *frameLL;
  QFrame *frameAoi;
  QFrame *frameAoiCreate;

  QLabel *toolboxInfoLabel;
  QLabel *toolboxLLInfoLabel;
  QLabel *toolboxAoiInfoLabel;
  QLabel *toolboxAoiCreateInfoLabel;

  QPoint lastClick;
  EcWidget::Waypoint lastWaypoint;
  int lastWaypointIndex;

  void buttonInit();

  // DEFAULTSETTINGS
  int courseUpHeading;
  int trackLine;
  double trackDistance;
  double trackMinute;
  QString latView;
  QString longView;

  // Flag & data
  bool inDraw;
  bool dragMode = true;
  QPoint lastPanPoint;
  QPoint tempOffset;   // offset sementara saat drag
  QPoint totalOffset;  // offset akumulasi pan

  // Max-zoom drag guard: prevent dragging to top boundary when fully zoomed out
  bool maxZoomDragActive = false;
  bool maxZoomTriedUpDrag = false;

public:
  // Chart Manager helper: focus viewport to a tile/cell by ID (e.g., "ID300081").
  // Returns true on success, false if extent cannot be determined.
  bool focusTileById(const QString& cellId);
  // Focus by explicitly provided Exchange Set root (directory containing CATALOG.031)
  // Returns true on success.
  bool focusTileByIdFromCatalog(const QString& cellId, const QString& catalogRootDir);

  // Chart maintenance helpers to safely modify/delete chart files on disk
  void beginChartMaintenance();
  void endChartMaintenance();

  // Rescan and reload DENC catalogue from current dencPath
  void rescanDenc();
  // Stronger maintenance helpers: fully close and reopen DENC to release file locks
  void closeDencForMaintenance();
  bool reopenDenc();
  double savedCenterLat = 0.0;
  double savedCenterLon = 0.0;

  // Teardown guard to avoid event handling during shutdown
  bool shuttingDown = false;

  void recalcView(const QPoint& offset);
  QPoint mapToScene(const QPoint &widgetPos) const;

  QPointF mapMouseToMapCoordinates(const QPoint& mousePos);

  // Compute dynamic pan margin based on current scale
  int effectivePanMargin() const;

  // Enhanced POI visualization using EC2007 kernel
  void drawEnhancedPOI(QPainter &painter, const PoiEntry &poi, const QPoint &screenPoint);
  const char* getPOISymbolName(EcPoiCategory category);
  int getPOIColorIndex(EcPoiCategory category);
  double getPOISizeFactor(EcPoiCategory category);
  void drawCriticalGlow(HDC hdc, int x, int y, int width, int height);
  void drawCustomPOIIcon(QPainter &painter, const PoiEntry &poi, const QPoint &screenPoint);
  void drawSafeGlow(QPainter &painter, const QPoint &screenPoint);
  void setupS52ColorScheme();
  bool isManOverboardCritical(const PoiEntry &poi);

  // Enhanced POI animation
  QTimer* poiAnimationTimer;
  bool hasVisibleManOverboardPOI();

  // Tidal station visualization
  bool m_showTidalStations = false;
  class TideManager* m_tideManager = nullptr;

  // Visualization components
  CurrentVisualisation* m_currentVisualisation = nullptr;
    bool m_showCurrentArrows = true;
  bool m_showTideRectangles = false;

  // GRIB Visualization components
  GribVisualisation* m_gribVisualisation = nullptr;
  GribManager* m_gribManager = nullptr;
  bool m_showGribData = true;

  // Method to set GRIB manager
  void setGribManager(GribManager* manager) { m_gribManager = manager; }

  // Method to draw GRIB data
  void drawGribData(QPainter& painter);

}; // EcWidget

#endif // _ec_widget_h_

