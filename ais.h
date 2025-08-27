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

    static Ais* instance();                       // untuk ambil pointer dari class lain

    EcAISTransponder    *_transponder;

    QMap<unsigned int, EcAISTargetInfo>& getTargetInfoMap() { return _aisTargetInfoMap; }
    EcAISTargetInfo* getTargetInfo(unsigned int mmsi);

signals:
    void signalRefreshChartDisplay( double, double, double );
    void signalRefreshCenter( double, double );
    void nmeaTextAppend(const QString&);
    void pickWindowOwnship();

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
    QMap<unsigned int, AISTargetData> _aisTargetMap;
    AISTargetData _aisOwnShip;
    QMap<unsigned int, EcAISTargetInfo> _aisTargetInfoMap;

    // icon ship
    EcCoordinate ownShipLat;
    EcCoordinate ownShipLon;
    double ownShipCog;
    double ownShipSog;

    void deleteOldOwnShipFeature();
    void handleOwnShipUpdate(EcAISTargetInfo *ti);
    void handleAISTargetUpdate(EcAISTargetInfo *ti);

    QDateTime lastTrailDrawTime;

    struct OwnShipSnapshot {
        double lat = 0;
        double lon = 0;
        double heading = 0;
    };
};

#endif
