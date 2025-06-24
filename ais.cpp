\
#include "ais.h"
#include "IAisDvrPlugin.h"
#include "PluginManager.h"
#include "aisdecoder.h"
#include "pickwindow.h"
#include "moosdb.h"

Ais::Ais( EcWidget *parent, EcView *view, EcDictInfo *dict, 
         EcCoordinate ownShipLat, EcCoordinate ownShipLon,
         double oSpd, double oCrs, 
         double warnDist, double warnCPA, int warnTCPA,
         const QString &aisLib, int timeOut, 
         Bool internalGPS, Bool *symbolize, const QString &strErrLogAis )
{
  _wParent = parent;
  _view = view;
  _dictInfo = dict;
  _ownShipLat = ownShipLat;
  _ownShipLon = ownShipLon;
  _dSpeed = oSpd;
  _dCourse = oCrs;
  _dWarnDist = warnDist;
  _dWarnCPA = warnCPA;
  _iWarnTCPA = warnTCPA;
  _iTimeOut = timeOut;
  _bInternalGPS = internalGPS;
  _bSymbolize = *symbolize;
  _sAisLib = aisLib;
  _errLog = new QFile( strErrLogAis );
  _transponder = NULL;
  _fAisFile = new QFile( "" );
  _bReadFromFile = False;
  _bReadFromVariable = False;
  _bReadFromServer = False;

  _errLog->open(QIODevice::WriteOnly);
  if( createTransponderObject() == False )
  {
    addLogFileEntry( QString( "createTransponderObject() failed!" ) );
  }

  _tcpSocket = new QTcpSocket;
  connect( _tcpSocket, SIGNAL( readyRead() ), this, SLOT( slotReadAISServerData() ) );
  connect( _tcpSocket, SIGNAL( error( QAbstractSocket::SocketError ) ), this, SLOT( slotShowTCPError( QAbstractSocket::SocketError ) ) );
  //connect( _tcpSocket, SIGNAL( hostFound() ), this, SLOT( slotHostFound() ) );      // TEST
  //connect( _tcpSocket, SIGNAL( connected() ), this, SLOT( slotConnected() ) );      // TEST

  // icon ship
  ownShipLat = 0.0;
  ownShipLon = 0.0;
  ownShipCog = 0.0;
  ownShipSog = 0.0;

  deleteOldOwnShipFeature();

}

Ais::~Ais()
{
  if( _transponder != NULL && _tcpSocket->state() == 0 )
  {
    if( EcAISDeleteTransponder( &_transponder ) == False )
    {
      addLogFileEntry( QString( "Error in ~Ais(): EcAISDeleteTransponder() failed!" ) );
    }
  }

  if( _tcpSocket && _tcpSocket->state() == 0 )
  {
    delete _tcpSocket;
  }

  if( _errLog )
  {
    delete _errLog;
  }

  if( _fAisFile )
  {
    delete _fAisFile;
  }
}

void Ais::setOwnShipPos(EcCoordinate lat, EcCoordinate lon)
{
  _ownShipLat = lat;
  _ownShipLon = lon;
}

void Ais::getOwnShipPos(EcCoordinate & lat, EcCoordinate & lon) const
{
  lat = _ownShipLat;
  lon = _ownShipLon;
}


// Close AIs file.
//////////////////
void Ais::closeAisFile()
{
  if( _fAisFile->isOpen() == true )
  {
    _fAisFile->close();
  }
}

// Close Error logfile.
///////////////////////
void Ais::closeErrLogFile()
{
  if( _errLog->isOpen() == true )
  {
    _errLog->close();
  }
}

void Ais::closeSocketConnection()
{
  _tcpSocket->abort();

  if( _tcpSocket->state() != 0 )
  {
    // If connected to AIS server, try to disconnect.
    _tcpSocket->disconnectFromHost();        
    if( _tcpSocket->waitForDisconnected( 5000 ) == false )
    {
      addLogFileEntry( QString( "Error in closeSocketConnection(): Could not disconnect from server." ) );
    }
  }
  if( disconnect( _tcpSocket, 0, this, 0 ) == False )
  {
    addLogFileEntry( QString( "Error in ~Ais(): : disconnect failed!" ) );
  }
}

// CALLBACK for updating AIS targets.
/////////////////////////////////////
void Ais::AISTargetUpdateCallback( EcAISTargetInfo *ti )
{
    // Pisahkan handling berdasarkan jenis target

    // 1. Handle ownship update terlebih dahulu
    if (ti->ownShip == True) {
        _myAis->handleOwnShipUpdate(ti);
        // KURANGI emit signal - hanya emit sekali per detik atau sesuai kebutuhan
        static QDateTime lastEmit = QDateTime::currentDateTime();
        if (lastEmit.msecsTo(QDateTime::currentDateTime()) > 3000) {
            EcCoordinate ownLat, ownLon;
            _myAis->getOwnShipPos(ownLat, ownLon);
            _myAis->emitSignal(ownLat, ownLon);
            lastEmit = QDateTime::currentDateTime();
        }
        return;
    }

    // 2. Handle AIS targets (bukan ownship)
    if (ti->ownShip == False) {
        _myAis->handleAISTargetUpdate(ti);
        return;
    }
}

void Ais::emitSignal( double lat, double lon )
{
  emit signalRefreshChartDisplay( lat, lon );
}

void Ais::stopAnimation()
{
  _bReadFromFile = False;
  _bReadFromVariable = False;
  _bReadFromServer = False;

  closeErrLogFile();
  closeAisFile();
  closeSocketConnection();
}

Ais* Ais::instance() {
    return _myAis;
}

QMap<unsigned int, AISTargetData>& Ais::getTargetMap() {
    return _aisTargetMap;
}

AISTargetData& Ais::getOwnShipVar() {
    return _aisOwnShip;
}

// Create transponder object.
/////////////////////////////
Bool Ais::createTransponderObject()
{
  // Init static ais instance.
  ////////////////////////////
  _myAis = this;

  if( EcAISNewTransponder( &_transponder, _sAisLib.toLatin1(), _bInternalGPS ? eGPSt_internalGPS : eGPSt_externalGPS ) == False )
  {
    if( _transponder )
    {
      EcAISDeleteTransponder( &_transponder );
      _transponder = NULL;
    }

    addLogFileEntry( QString( "EcAISNewTransponder() failed! Could not create new transponder object." ) );
    return False;
  }

  // Set AIS callback.
  EcAISSetTargetUpdateCallBack( _transponder, AISTargetUpdateCallback );
  return True;
}

// Read AIS logfile.
////////////////////
void Ais::readAISLogfile( const QString &logFile )
{
  _bReadFromFile = True;
  _bReadFromVariable = False;
  _bReadFromServer = False;

  //closeAisFile();
  closeSocketConnection();

  //QFile fAisFile( logFile );
  _fAisFile->setFileName( logFile );
  if( _fAisFile->exists() == False )
  {
    addLogFileEntry( QString( "Error in readAISLogfile(): AIS logfile doesn't exist." ) );
    return;
  }   

  if( _fAisFile->open( QIODevice::ReadOnly | QIODevice::Text ) == False )
  {
    addLogFileEntry( QString( "Error in readAISLogfile(): Could not open AIS logfile: %1." ).arg( logFile ) );
    return;
  }

  if( !_transponder )
  {
    addLogFileEntry( QString( "Error in readAISLogfile(): Transponder object is not initialized!" ) );
    return;
  }

  if( _bReadFromFile == False )
  {
    addLogFileEntry( QString( "Error in readAISLogfile(): Read from AIS logfile permitted!" ) );
    return;
  }

  // NULL DECLARATION
  EcDENC *denc = nullptr;
  EcDictInfo *dictInfo = nullptr;
  QWidget *parentWidget = nullptr;

  // Read AIS logfile line by line and add each line to the AIS transponder object by calling EcAISAddTransponderOutput.
  // EcAISAddTransponderOutput calls the callback AISTargetUpdateCallback for each line read from the logfile.
  int iLineNo = 1;
  QTextStream in( _fAisFile );
  while( in.atEnd() == False )
  {

    if( _bReadFromFile == False )
    {
      break;
    }

    if( _transponder == NULL )
    {
      break;
    }

    QString sLine = in.readLine();
    sLine = sLine.append( "\r\n" );

    nmeaText->append(sLine);
    extractNMEA(sLine);

    // OWNSHIP NMEA
    PickWindow *pickWindow = new PickWindow(parentWidget, dictInfo, denc);
    if (navShip.lat != 0){
        ownShipText->setHtml(pickWindow->ownShipAutoFill());
    }

    // RECORD NMEA
    IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");

    if (dvr && dvr->isRecording()) {
        dvr->recordRawNmea(sLine);
    }

    // OWNSHIP RIGHT PANEL
    if (navShip.lat != 0){
        _cpaPanel->updateOwnShipInfo(navShip.lat, navShip.lon, navShip.speed_og, navShip.heading_og);
    }

    //qDebug() << sLine;

    if( EcAISAddTransponderOutput( _transponder, (unsigned char*)sLine.toStdString().c_str(), sLine.count() ) == False )
    {
      addLogFileEntry( QString( "Error in readAISLogfile(): EcAISAddTransponderOutput() failed in input line %1" ).arg( iLineNo ) );
      break;
    }

    // hapus pickwindow
    delete pickWindow;

    iLineNo++;
  }
}

void Ais::readAISLogfileWDelay(const QString &logFile, int delayMs, std::atomic<bool>* stopFlag)
{
    _bReadFromFile = True;
    _bReadFromVariable = False;
    _bReadFromServer = False;

    closeSocketConnection();

    _fAisFile->setFileName(logFile);
    if (_fAisFile->exists() == False) {
        addLogFileEntry("Error in readAISLogfile(): AIS logfile doesn't exist.");
        return;
    }

    if (_fAisFile->open(QIODevice::ReadOnly | QIODevice::Text) == False) {
        addLogFileEntry(QString("Error in readAISLogfile(): Could not open AIS logfile: %1.").arg(logFile));
        return;
    }

    if (!_transponder) {
        addLogFileEntry("Error in readAISLogfile(): Transponder object is not initialized!");
        return;
    }

    QTextStream in(_fAisFile);
    QWidget *parentWidget = nullptr;
    EcDictInfo *dictInfo = nullptr;
    EcDENC *denc = nullptr;

    int iLineNo = 1;
    while (!in.atEnd())
    {
        if (!_bReadFromFile || !_transponder || (stopFlag && stopFlag->load())) {
            break;
        }

        QString sLine = in.readLine().append("\r\n");

        nmeaText->append(sLine);
        extractNMEA(sLine);

        PickWindow *pickWindow = new PickWindow(parentWidget, dictInfo, denc);
        if (navShip.lat != 0) {
            ownShipText->setHtml(pickWindow->ownShipAutoFill());
        }

        IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");
        if (dvr && dvr->isRecording()) {
            dvr->recordRawNmea(sLine);
        }

        if (navShip.lat != 0) {
            _cpaPanel->updateOwnShipInfo(navShip.lat, navShip.lon, navShip.speed_og, navShip.heading_og);
        }

        if (!EcAISAddTransponderOutput(_transponder, (unsigned char*)sLine.toStdString().c_str(), sLine.count())) {
            addLogFileEntry(QString("Error in readAISLogfile(): EcAISAddTransponderOutput() failed at line %1").arg(iLineNo));
            delete pickWindow;
            break;
        }

        delete pickWindow;
        iLineNo++;

        if (delayMs > 0)
            QThread::msleep(delayMs);
    }

    _fAisFile->close();
}


void Ais::extractNMEA(QString nmea){
    navShip.lat = AisDecoder::decodeAisOption(nmea, "latitude", "!AIVDO");
    navShip.lon = AisDecoder::decodeAisOption(nmea, "longitude", "!AIVDO");
    navShip.speed_og = AisDecoder::decodeAisOption(nmea, "sog", "!AIVDO");
    navShip.heading_og = AisDecoder::decodeAisOption(nmea, "cog", "!AIVDO") / 10;
}

// Read AIS variable.
////////////////////
void Ais::readAISVariable( const QStringList &dataLines )
{
    _bReadFromFile = False;
    _bReadFromVariable = True;
    _bReadFromServer = False;

    if( !_transponder )
    {
        addLogFileEntry( QString( "Error in readAISVariable(): Transponder object is not initialized!" ) );
        return;
    }

    if( _bReadFromVariable == False )
    {
        addLogFileEntry( QString( "Error in readAISVariable(): Read from AIS variable permitted!" ) );
        return;
    }

    // Read AIS logfile line by line and add each line to the AIS transponder object by calling EcAISAddTransponderOutput.
    // EcAISAddTransponderOutput calls the callback AISTargetUpdateCallback for each line read from the logfile.
    int iLineNo = 1;
    // for( const QString &sLine : dataLines )
    foreach (const QString &sLine, dataLines)
    {
        if( _bReadFromVariable == False )
        {
            break;
        }

        if( _transponder == NULL )
        {
            break;
        }

        QString line = sLine + "\r\n";
        nmeaText->append(sLine);

        if( EcAISAddTransponderOutput( _transponder, (unsigned char*)line.toStdString().c_str(), line.count() ) == False )
        {
            addLogFileEntry( QString( "Error in readAISLogfile(): EcAISAddTransponderOutput() failed in input line %1" ).arg( iLineNo ) );
            break;
        }

        iLineNo++;
    }

    stopAnimation();
}

// Connect to AIS server.
/////////////////////////
void Ais::connectToAISServer( const QString& strHost, int iPort )
{
  _iLineCnt = 0;
  _uiBlockSize = 0;
  _tcpSocket->abort();

  _bReadFromFile = False;
  _bReadFromServer = True;

  closeAisFile();
  closeSocketConnection();

  if( strHost.isEmpty() || iPort <= 0 )
  {
    addLogFileEntry( QString( "Error in connectToAISServer(): Invalid host or port." ) );
    return;
  }

  connect( _tcpSocket, SIGNAL( readyRead() ), this, SLOT( slotReadAISServerData() ) );
  connect( _tcpSocket, SIGNAL( error( QAbstractSocket::SocketError ) ), this, SLOT( slotShowTCPError( QAbstractSocket::SocketError ) ) );

  // Try to connect to AIS server. Wait up to 5 sec. for server connection.
  _tcpSocket->connectToHost( strHost, iPort );
  if( !_tcpSocket->waitForConnected( 2000 ) )
  {
    QString strErr = QString( "Error in connectToAISServer(): Could not connect to host %1 and port %2." ).arg( strHost ).arg( iPort );
    QAbstractSocket::SocketError sError = _tcpSocket->error();
    slotShowTCPError( sError );
    addLogFileEntry( strErr );
  }
}

// Read AIS data from AIS server via TCP.
/////////////////////////////////////////
void Ais::slotReadAISServerData()
{
  if( _bReadFromServer == False )
  {
    return;
  }

  if( _transponder == NULL )
  {
    addLogFileEntry( QString( "Error in slotReadAISServerData(): Transponder not initialized!" ) );
    return;
  }

  QString strNextData;
  QDataStream in( _tcpSocket );
  in.setVersion( QDataStream::Qt_4_0 );

  if( _uiBlockSize == 0 )
  {
    if( _tcpSocket->bytesAvailable() < (int)sizeof( quint16 ) )
    {
      addLogFileEntry( QString( "Error in slotReadAISServerData(): No data available!" ) );
      return;
    }

    in >> _uiBlockSize;
  }

  if( _uiBlockSize > 0 )
  {
    QByteArray baData = _tcpSocket->readLine( _uiBlockSize );
    strNextData = QString( baData ).append( "\r\n" );

    if( strNextData.isEmpty() == False )
    {
      //_bCanBeDeleted = False;

      // Add AIS data to transponder.
      if( EcAISAddTransponderOutput( _transponder, (unsigned char*)strNextData.toStdString().c_str(), strNextData.count() ) == False )
      {
        addLogFileEntry( QString( "Error in slotReadAISServerData(): EcAISAddTransponderOutput() failed!" ) );
      }

      //_bCanBeDeleted = True;

      _iLineCnt++;        // limit data from AIS server to a specific number of lines.
    }
  }

  // Close connection to AIS server after reading of max. no. of lines.
  if( ( strNextData == _strCurrentData ) ||
    ( _iLineCnt == 1000 ) )       
  {
    // Close socket connection.
    _tcpSocket->disconnectFromHost();
  }

  _strCurrentData = strNextData;    
}

////////////////////////   TEST   /////////////////////
//void Ais::slotHostFound()
//{
//    QMessageBox::information( 0, "TEST", "Host found." );
//}
//
//void Ais::slotConnected()
//{
//    QMessageBox::information( 0, "TEST", "Connected to host." );
//}
/////////////////////////////////////////////////////////

// Return connection errors for AIS Server.
///////////////////////////////////////////
void Ais::slotShowTCPError( QAbstractSocket::SocketError socketError )
{
  QString strMsg = "";

    switch( socketError )
    {
        case QAbstractSocket::RemoteHostClosedError:
        {
            break;
        }
        case QAbstractSocket::HostNotFoundError:
        {
            strMsg = strMsg.append( tr( "The host was not found.\nPlease check the host name and port settings." ) );
            QMessageBox::warning( 0, tr( "AIS Example" ), strMsg );
            break;
        }
        case QAbstractSocket::ConnectionRefusedError:
        {
            strMsg = strMsg.append( tr( "The connection was refused by the peer.\nMake sure the AIS server is running,\nand check that the host name and port settings are correct." ) );
            QMessageBox::warning( 0, tr( "AIS Example" ), strMsg );
            break;
        }
        default:
        {
            strMsg = strMsg.append( tr( "The following error occurred: %1." ).arg( _tcpSocket->errorString() ) );
            QMessageBox::warning( 0, tr( "AIS Example" ), strMsg );
        }
    }
}


// Set AIS cell.
////////////////
void Ais::setAISCell( EcCellId cid )
{
  _cid = cid;

  EcCoordinate coorList[2] = {0,0};
  EcPrimitive primitive;
  // create the own ship feature
  // this is usually not done with the AIS transponder but independently with other positioning sensors
  _featureOwnShip = EcObjectCreate(_cid, _dictInfo, "ownshp", EC_OS_DELETABLE, "", '|', coorList, 1, EC_P_PRIM, &primitive);
}

// Return AIS cell.
///////////////////
EcCellId Ais::getAISCell()
{
  return _cid;
}

// Add entry to AIS logfile.
////////////////////////////
void Ais::addLogFileEntry( const QString &str )
{
  if( !_errLog )
  {
    return;
  }

  if( _errLog->exists() == False )
  {
    return;
  }

  if( _errLog->isOpen() == False )
  {
    if( _errLog->open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append ) == False )
    {
      return;
    }
  }

  QTextStream out( _errLog );
  out << str << "\n";
  _errLog->close();
}

void Ais::deleteObject()
{
    EcObjectDelete(_featureOwnShip);
}

void Ais::setOwnShipNull()
{
    _myAis->setOwnShipPos(0,0);
}
//////////////// AIS MOOSDB ??


EcAISTargetInfo* Ais::getTargetInfo(unsigned int mmsi)
{
    if (_aisTargetInfoMap.contains(mmsi)) {
        return &_aisTargetInfoMap[mmsi];
    }
    return nullptr;
}

// Fungsi khusus untuk handle ownship update
void Ais::handleOwnShipUpdate(EcAISTargetInfo *ti)
{
    qDebug() << "Processing OWNSHIP update - MMSI:" << ti->mmsi;

    if (ti->ownShip != True) return;

    if (abs(ti->latitude) < 90 * 60 * 10000 &&
        abs(ti->longitude) < 180 * 60 * 10000)
    {
        _oLat = ((double)ti->latitude / 10000) / 60;
        _oLon = ((double)ti->longitude / 10000) / 60;

        ownShipLat = _oLat;
        ownShipLon = _oLon;
        ownShipCog = ti->cog;
        ownShipSog = ti->sog;

        _myAis->setOwnShipPos(ownShipLat, ownShipLon);

        // SIMPEN DATA AIS OWNSHIP
        AISTargetData dataOS;
        dataOS.lat = _oLat;
        dataOS.lon = _oLon;
        dataOS.cog = ti->cog / 10.0;
        dataOS.sog = ti->sog / 10.0;
        dataOS.cpaCalculatedAt = QDateTime::currentDateTime();

        Ais::instance()->_aisOwnShip = dataOS;

        qDebug() << "Ownship data updated without using old feature object";
    }
}

// Fungsi khusus untuk handle AIS targets (bukan ownship)
void Ais::handleAISTargetUpdate(EcAISTargetInfo *ti)
{
    if (ti->ownShip == True) return; // Skip jika ini ownship

    EcCoordinate ownShipLat = 0, ownShipLon = 0;
    double ownShipSog = 0, ownShipCog = 0;

    double dist = 0, bear = 0;
    double lat = ((double)ti->latitude / 10000.0) / 60.0;
    double lon = ((double)ti->longitude / 10000.0) / 60.0;

    _myAis->getOwnShipPos(ownShipLat, ownShipLon);
    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, lat, lon, ownShipLat, ownShipLon, &dist, &bear);

    // Process only targets which are further away than 48 nm or no own ship position exists yet
    if (dist < 48 || (ownShipLat == 0 && ownShipLon == 0))
    {
        // Filter the ais targets which shall be displayed
        if (abs(ti->latitude) < 90 * 60 * 10000 &&
            abs(ti->longitude) < 180 * 60 * 10000 &&
            ti->navStatus != eNavS_baseStation)
        {
            EcAISTrackingStatus aisTrkStatus = EcAISCalcTargetTrackingStatus(ti, ownShipLat, ownShipLon, ownShipSog, ownShipCog, _dWarnDist, _dWarnCPA, _iWarnTCPA, _iTimeOut);
            EcFeature feat = EcAISFindTargetObject(_cid, _dictInfo, ti);

            // if there is no feature yet create one
            if (!ECOK(feat) && aisTrkStatus != aisLost)
            {
                if (EcAISCreateTargetObject(_cid, _dictInfo, ti, &feat))
                {
                    EcAISSetTargetActivationStatus(feat, _dictInfo, aisSleeping, NULL);
                    _bSymbolize = True;
                }
                else
                {
                    addLogFileEntry(QString("EcAISCreateTargetObject() failed! New target object could not be created."));
                }
            }

            // Set the status of the ais target to active if it is located within a certain distance
            if (dist < 4)
                EcAISSetTargetActivationStatus(feat, _dictInfo, aisActivated, NULL);

            // Set the tracking status of the ais target feature
            EcAISSetTargetTrackingStatus(feat, _dictInfo, aisTrkStatus, NULL);

            // Set the remaining attributes of the ais target feature
            EcAISSetTargetObjectData(feat, _dictInfo, ti, &_bSymbolize);

            // SIMPEN DATA AIS TARGET
            if (ti && ti->mmsi != 0)
            {
                AISTargetData data;
                data.mmsi = QString::number(ti->mmsi);
                data.lat = lat;
                data.lon = lon;
                data.cog = ti->cog / 10.0;
                data.sog = ti->sog / 10.0;
                data.lastUpdate = QDateTime::currentDateTime();
                data.currentRange = dist;
                data.relativeBearing = bear;
                data.cpaCalculatedAt = QDateTime::currentDateTime();

                Ais::instance()->_aisTargetMap[ti->mmsi] = data;
                Ais::instance()->_aisTargetInfoMap[ti->mmsi] = *ti;
            }
        }
    }

    // Emit signal untuk refresh chart display
    EcCoordinate ownLat, ownLon;
    _myAis->getOwnShipPos(ownLat, ownLon);
    _myAis->emitSignal(ownLat, ownLon);
}

// Fungsi untuk menghapus feature object ownship lama
void Ais::deleteOldOwnShipFeature()
{
    if (ECOK(_featureOwnShip)) {
        qDebug() << "Deleting old ownship feature object";

        // Set status menjadi deletable
        EcFeatureSetStatus(_featureOwnShip, EC_OS_DELETABLE);

        // Hapus feature object
        EcFeatureDelete(_featureOwnShip);

        // Jangan reset handle, biarkan sistem yang handle
        qDebug() << "Old ownship feature deleted successfully";
    }
}
