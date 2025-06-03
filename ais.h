#ifndef _ais_h_
#define _ais_h_

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
#include <moosdb.h>

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
    void readAISVariable( const QStringList& );
    void connectToAISServer( const QString&, int port );
    EcCellId getAISCell();
    void setAISCell( EcCellId cid );
    void stopAnimation();  
    void closeLogFile();
    void emitSignal( double, double );
    void setOwnShipPos(EcCoordinate lat, EcCoordinate lon);
    void getOwnShipPos(EcCoordinate & lat, EcCoordinate & lon) const;
    void deleteObject();
    void setOwnShipNull();

    static void AISTargetUpdateCallback( EcAISTargetInfo* );

signals:
    void signalRefreshChartDisplay( double, double );

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

    EcAISTransponder    *_transponder;
};

#endif
