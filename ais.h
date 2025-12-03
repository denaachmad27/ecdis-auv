#ifndef _ais_h_
#define _ais_h_

#include "cpatcpapanel.h"

#include <QObject>
#include <QString>
#include <QFile>
#include <QTcpSocket>
#include <QTextStream>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>


// SevenCs Kernel EC2007
#ifdef _WIN32
#include <windows.h>
#else
#include <stdio.h>
#include <X11/Xlib.h>
#endif

#include <eckernel.h>
#include <ecwidget.h>

#define LINEMAX 1024
#define OBJ_CLEAN_TIME      (10*60)
#define DEFAULT_LAT     -7.18551
#define DEFAULT_LON     112.78012

class Ais;

// For AIS Callback.
////////////////////
static EcView       *_view;
static EcCoordinate _oLat, _oLon, _ownShipLat, _ownShipLon;   // own ship lat/ lon
static QFile        *_errLog;
static EcDictInfo   *_dictInfo;
static EcCellId     _cid;
static EcFeature    _featureOwnShip;
static Bool         _bSymbolize;
static double      _dSpeed, _dCourse, _dWarnDist, _dWarnCPA;
static int         _iWarnTCPA, _iTimeOut;
static EcWidget     *_wParent;
static Ais          *_myAis;

class Ais : public QObject
{
    Q_OBJECT

public:
    Ais( EcWidget *parent, EcView *view, EcDictInfo *dict, 
         EcCoordinate oLat, EcCoordinate oLon,
         double oSpd, double oCrs, 
         double warnDist, double warnCPA, int warnTCPA,
         const QString &aislib, int timeOut, 
         Bool internalGPS, Bool *symbolize, const QString &strErrLogAis );
    
    virtual ~Ais();

    Bool createTransponderObject();
    void readAISLogfile( const QString& );
    void readAISLogfileWDelay(const QString &logFile, int delayMs, std::atomic<bool>* stopFlag);
    void readAISVariableString( const QString& );
    void readAISVariable( const QStringList& );
    void readAISVariableThread( const QStringList& );
    void nmeaSelection(const QString &line, QString &outNmea);

    void extractNMEA(QString nmea);
    void clearTargetData();
    // Reset only transponder/feature layer, keep target map in memory
    void resetTransponderPreserveTargets();
    // Fully recreate SevenCs transponder to ensure clean state after reconnect
    Bool recreateTransponder();

    void connectToAISServer( const QString&, int port );
    EcCellId getAISCell();
    void setAISCell( EcCellId cid );
    void stopAnimation();  
    void closeLogFile();
    void emitSignal( double, double, double );
    void emitSignalTarget( double, double );
    void setOwnShipPos(EcCoordinate lat, EcCoordinate lon);
    void getOwnShipPos(EcCoordinate & lat, EcCoordinate & lon) const;

    void setTargetPos(EcCoordinate lat, EcCoordinate lon);
    void getTargetPos(EcCoordinate & lat, EcCoordinate & lon) const;

    void setAISTrack(AISTargetData aisTrack);
    void getAISTrack(AISTargetData & aisTrack) const;

    void deleteObject();
    void setOwnShipNull();

    static void AISTargetUpdateCallback( EcAISTargetInfo* );
    static void AISTargetUpdateCallbackOld( EcAISTargetInfo* );
    static void AISTargetUpdateCallbackThread( EcAISTargetInfo* );

    // CPA TCPA
    void setCPAPanel(CPATCPAPanel* panel) { _cpaPanel = panel; }
    QMap<unsigned int, AISTargetData>& getTargetMap();
    AISTargetData& getOwnShipVar();

    QMap<unsigned int, AISTargetData> _aisTargetMap;
    QMap<unsigned int, EcAISTargetInfo> _aisTargetInfoMap;
    QString _latestNmea; // Cache untuk NMEA terakhir yang masuk
    QDateTime _latestNmeaTime; // Timestamp untuk memastikan NMEA masih fresh

    // Fungsi untuk NMEA cache management
    void cacheLatestNmea(const QString& nmea);
    QString getLatestNmea();

    static Ais* instance();                       // untuk ambil pointer dari class lain

    EcAISTransponder    *_transponder;

    QMap<unsigned int, EcAISTargetInfo>& getTargetInfoMap() { return _aisTargetInfoMap; }
    EcAISTargetInfo* getTargetInfo(unsigned int mmsi);

    void postTargetUpdate(const AISTargetData& info);
    void setTargetManualStatus(unsigned int mmsi, EcAISTrackingStatus status);

signals:
    void signalRefreshChartDisplay( double, double, double );
    void signalRefreshCenter( double, double );
    void nmeaTextAppend(const QString&);
    void pickWindowOwnship();

    void targetUpdateReceived(AISTargetData info);

private slots:
    void slotReadAISServerData();
    void slotShowTCPError( QAbstractSocket::SocketError );
    //void slotHostFound();     // TEST
    //void slotConnected();     // TEST

private:
    static void addLogFileEntry( const QString& );
    void closeErrLogFile();
    void closeAisFile();
    void closeSocketConnection();

    static bool _isShuttingDown; // Flag to prevent callbacks during shutdown
    
    EcCoordinate _ownShipLat, _ownShipLon;
    EcCoordinate _targetLat, _targetLon;
    AISTargetData _aisTrack;

    QTcpSocket  *_tcpSocket;
    QString     _strCurrentData;
    quint16     _uiBlockSize;
    int         _iLineCnt;

    QString     _sAisLib;
    QFile       *_fAisFile;
    Bool        _bReadFromFile;
    Bool        _bReadFromVariable;
    Bool        _bReadFromServer;
    Bool        _bInternalGPS;

    // CPA TCPA
    CPATCPAPanel* _cpaPanel = nullptr;
    AISTargetData _aisOwnShip;

    // icon ship
    EcCoordinate ownShipLat;
    EcCoordinate ownShipLon;
    double ownShipCog;
    double ownShipSog;

    void deleteOldOwnShipFeature();
    void handleOwnShipUpdate(EcAISTargetInfo *ti);
    void handleAISTargetUpdate(EcAISTargetInfo *ti);

    QDateTime lastTrailDrawTime;

    // Recording status tracking
    bool _lastRecordingState = false;
    void updateRecordingStatusUI(bool shouldRecord, const QString& reason = QString());

    struct OwnShipSnapshot {
        double lat = 0;
        double lon = 0;
        double heading = 0;
    };
};

#endif

