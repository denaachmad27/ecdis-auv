#include "ais.h"
#include "IAisDvrPlugin.h"
#include "PluginManager.h"
#include "aisdecoder.h"
#include "pickwindow.h"
#include "aisdatabasemanager.h"
#include "aivdoencoder.h"
#include "SettingsManager.h"

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

Ais::~Ais(){
    // Pastikan transponder dibersihkan
    if( _transponder != NULL ){
        if( EcAISDeleteTransponder( &_transponder ) == False ){
            addLogFileEntry( QString( "Error in ~Ais(): EcAISDeleteTransponder() failed!" ) );
        }
        _transponder = NULL;
    }

    // Putuskan dan bebaskan socket secara aman
    if( _tcpSocket ){
        _tcpSocket->abort();
        _tcpSocket->disconnect(this);
        delete _tcpSocket;
        _tcpSocket = nullptr;
    }
    
    if( _errLog ){
        delete _errLog;
    }

    if( _fAisFile ){
        delete _fAisFile;
    }

    if (_myAis == this){
        _myAis = nullptr;
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

void Ais::setTargetPos(EcCoordinate lat, EcCoordinate lon)
{
    _targetLat = lat;
    _targetLon = lon;
}

void Ais::getTargetPos(EcCoordinate & lat, EcCoordinate & lon) const
{
    lat = _targetLat;
    lon = _targetLon;
}

void Ais::setAISTrack(AISTargetData aisTrack)
{
    _aisTrack = aisTrack;
}

void Ais::getAISTrack(AISTargetData & aisTrack) const
{
    aisTrack = _aisTrack;
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
void Ais::AISTargetUpdateCallbackOld( EcAISTargetInfo *ti )
{
  EcCoordinate ownShipLat = 0, ownShipLon = 0;
  double ownShipSog = _dSpeed, ownShipCog = _dCourse;

  double dist = 0, bear = 0;
  double lat = ( (double) ti->latitude  / 10000.0 ) / 60.0;
  double lon = ( (double) ti->longitude / 10000.0 ) / 60.0;

  _myAis->getOwnShipPos(ownShipLat, ownShipLon);
  EcCalculateRhumblineDistanceAndBearing( EC_GEO_DATUM_WGS84, lat, lon, ownShipLat, ownShipLon, &dist, &bear);

  // QSet<unsigned int> mmsiSet = {
  //     236391000, 366811570, 368834350,
  //     366934050, 366995280, 366996920,
  //     367048970, 367155310, 367159080,
  //     367163760, 367193790, 368299000,
  //     636013091
  // };

  // QSet<unsigned int> mmsiSet = {
  //     367155310
  // };

  //  (mmsiSet.contains(ti->mmsi)) &&


  // Process only targets which are further away than 48 nm or no own ship position exists yet
  if( dist < 48 || (ownShipLat == 0 && ownShipLon == 0))
  {
    // Filter the ais targets which shall be displayed
    // Some transponders do not send the official numbers in case of invalid positions
    if( ti->ownShip == False &&
        abs(ti->latitude) < 90 * 60 * 10000 &&
        abs(ti->longitude) < 180 * 60 * 10000 &&
        ti->navStatus != eNavS_baseStation)
    {

      //EcAISTrackingStatus aisTrkStatus = EcAISCalcTargetTrackingStatus( ti, ownShipLat, ownShipLon, _dSpeed, _dCourse, _dWarnDist, _dWarnCPA, _iWarnTCPA, _iTimeOut );

        // COG dan SOG diubah ke data real
        EcAISTrackingStatus aisTrkStatus = EcAISCalcTargetTrackingStatus( ti, ownShipLat, ownShipLon, ownShipSog, ownShipCog, _dWarnDist, _dWarnCPA, _iWarnTCPA, _iTimeOut );
        EcFeature feat = EcAISFindTargetObject( _cid, _dictInfo, ti );

      // if there is no feature yet create one
      if( !ECOK( feat ) && aisTrkStatus != aisLost )
      {
        if( EcAISCreateTargetObject( _cid, _dictInfo, ti, &feat ) )
        {
          // Set default activation status to sleeping
          EcAISSetTargetActivationStatus( feat, _dictInfo, aisSleeping, NULL );
          _bSymbolize = True;
        }
        else
        {
          addLogFileEntry( QString( "EcAISCreateTargetObject() failed! New target object could not be created." ) );
        }
      }

      // Set the status of the ais target to active if it is located within a certain distance
      if( dist < 4 )
        EcAISSetTargetActivationStatus( feat, _dictInfo, aisActivated, NULL );

      // Set the tracking status of the ais target feature
      EcAISSetTargetTrackingStatus( feat, _dictInfo, aisTrkStatus, NULL );

      // Set the remaining attributes of the ais target feature
      EcAISSetTargetObjectData( feat, _dictInfo, ti, &_bSymbolize );


      // SIMPEN DATA AIS
      if (ti && ti->mmsi != 0)
      {
          AISTargetData data;
          data.mmsi = QString::number(ti->mmsi);
          data.lat = lat;
          data.lon = lon;
          // Scale AIS motion fields and handle 'not available' sentinels
          if (ti->sog >= 1023) {
              data.sog = -1;
          } else {
              data.sog = ti->sog / 10.0;
          }
          if (ti->cog >= 3600) {
              data.cog = -1;
          } else {
              data.cog = ti->cog / 10.0;
          }
          //data.cpa = ti->cpa;
          //data.tcpa = ti->tcpa;
          //data.isDangerous = (ti->status == eAIS_dangerous);
          data.lastUpdate = QDateTime::currentDateTime();
          data.currentRange = dist;
          data.relativeBearing = bear;
          //data.cpaCalculationValid = (ti->cpa >= 0 && ti->tcpa >= 0);
          data.cpaCalculatedAt = QDateTime::currentDateTime();

          data.feat = feat;
          data._dictInfo = _dictInfo;

          Ais::instance()->_aisTargetMap[ti->mmsi] = data;
          Ais::instance()->_aisTargetInfoMap[ti->mmsi] = *ti;
      }

      // if (aisTrkStatusManual == aisDangerous){
      //     _wParent->drawShipGuardianSquare(lat, lon);
      // }
    }
    else
    {
      // For simulation purposes we take the actual ship position of the own ship from transponder.
      // In reality the own ship handling and display is NOT implemented within the AIS handling.
        if( ( ti->ownShip == True ) &&
            abs(ti->latitude) < 90 * 60 * 10000 &&
            abs(ti->longitude) < 180 * 60 * 10000)
        {
            if( ti->ownShip == True )
            {
                _oLat = ((double)ti->latitude / 10000) / 60;
                _oLon = ((double)ti->longitude / 10000) / 60;

                // Update feature object of own ship.
                double deltaLat = _oLat - ownShipLat;
                double deltaLon = _oLon - ownShipLon;
                EcEasyMoveObject( _featureOwnShip, deltaLat, deltaLon );
                ownShipLat = _oLat;
                ownShipLon = _oLon;

                _dSpeed = ti->sog;
                _dCourse = ti->cog / 10;

                _myAis->setOwnShipPos(ownShipLat, ownShipLon);

                // ⭐ SIMPEN DATA AIS DENGAN HEADING
                AISTargetData dataOS;
                dataOS.lat = _oLat;
                dataOS.lon = _oLon;
                dataOS.cog = ti->cog / 10.0;        // Course over ground
                dataOS.sog = ti->sog;               // Speed over ground
                dataOS.heading = ti->heading;       // ⭐ TAMBAHAN: True heading
                dataOS.cpaCalculatedAt = QDateTime::currentDateTime();

                Ais::instance()->_aisOwnShip = dataOS;
            }
        }
    } // if( ti->ownShip == False ...
  } // if (dist ...

  if (ti->mmsi == 366884150){
      QMap<unsigned int, AISTargetData> targets = Ais::instance()->getTargetMap();
      for (const auto &target : targets) {
          if (target.mmsi != "366884150") continue;

          //qDebug() << target.lat << ", " << target.lon << ", " << target.sog << ", " << target.cog;
      }
  }

  // The callback informs the application to refresh the chart display with AIS target by sending an event.
  /////////////////////////////////////////////////////////////////////////////////////////////////////////
  _myAis->emitSignal( ownShipLat, ownShipLon, ti->heading );
}

void Ais::AISTargetUpdateCallback( EcAISTargetInfo *ti )
{
    // Pisahkan handling berdasarkan jenis target

    // 1. Handle ownship update terlebih dahulu
    if (ti->ownShip == True) {
        _myAis->handleOwnShipUpdate(ti);

        EcCoordinate ownLat, ownLon;
        _myAis->getOwnShipPos(ownLat, ownLon);
        _myAis->emitSignal(ownLat, ownLon, ti->heading);

        return;
    }

    // 2. Handle AIS targets (bukan ownship)
    else if (ti->ownShip == False) {
        _myAis->handleAISTargetUpdate(ti);

        // KURANGI emit signal - hanya emit sekali per detik atau sesuai kebutuhan
        // static QDateTime lastEmit = QDateTime::currentDateTime();

        // =========== CARA 1: SET KE CENTER KALO ADA PERUBAHAN/PERTAMBAHAN DATA =============

        /*
        if (_wParent->getTrackMMSI() == QString::number(ti->mmsi))
        {
            EcCoordinate ownLat, ownLon;
            _myAis->getTargetPos(ownLat, ownLon);

            AISTargetData ais;
            ais.mmsi = QString::number(ti->mmsi);
            ais.lat = ownLat;
            ais.lon = ownLon;

            _myAis->setAISTrack(ais);
            _myAis->emitSignalTarget(ownLat, ownLon);
        }
        */

        // =========== CARA 2: SET KE CENTER TERUS ADA ATAU PUN TIADA PERUBAHAN DATA ===========
        AISTargetData ais;
        _myAis->getAISTrack(ais);
        _myAis->emitSignalTarget(ais.lat, ais.lon);

        if (!ais.mmsi.isEmpty())
        {

            if (ais.mmsi == QString::number(ti->mmsi)){
                EcCoordinate ownLat, ownLon;
                _myAis->getTargetPos(ownLat, ownLon);

                ais.lat = ownLat;
                ais.lon = ownLon;

                _myAis->setAISTrack(ais);
            }
        }


        return;
    }
}

void Ais::AISTargetUpdateCallbackThread(EcAISTargetInfo *ti)
{
    if (!ti || !_myAis) return;

    EcAISTargetInfo tiCopy = *ti; // copy biar aman

    // =================== OWN SHIP ===================
    if (tiCopy.ownShip == True)
    {
        _myAis->handleOwnShipUpdate(&tiCopy);

        EcCoordinate ownLat, ownLon;
        _myAis->getOwnShipPos(ownLat, ownLon);

        // ===== emit via queued connection =====
        QMetaObject::invokeMethod(_myAis, [snapshotLat = ownLat, snapshotLon = ownLon, snapshotHead = tiCopy.heading]() {
            emit _myAis->signalRefreshChartDisplay(snapshotLat, snapshotLon, snapshotHead);
        }, Qt::QueuedConnection);

        return;
    }

    // =================== OTHER AIS TARGET ===================
    if (tiCopy.ownShip == False)
    {
        _myAis->handleAISTargetUpdate(&tiCopy);

        AISTargetData ais;
        _myAis->getAISTrack(ais);

        if (!ais.mmsi.isEmpty() && ais.mmsi == QString::number(tiCopy.mmsi))
        {
            EcCoordinate targetLat, targetLon;
            _myAis->getTargetPos(targetLat, targetLon);
            ais.lat = targetLat;
            ais.lon = targetLon;
            _myAis->setAISTrack(ais);
        }

        // ===== emit target update via queued connection =====
        QMetaObject::invokeMethod(_myAis, [ais]() {
            emit _myAis->signalRefreshCenter(ais.lat, ais.lon);
        }, Qt::QueuedConnection);

        return;
    }
}

// Fungsi khusus untuk handle ownship update
void Ais::handleOwnShipUpdate(EcAISTargetInfo *ti)
{
    // DEBUG COMMENT TEMP
    //qDebug() << "Processing OWNSHIP update - MMSI:" << ti->mmsi;

    if (ti->ownShip != True) return;

    if (abs(ti->latitude) < 90 * 60 * 10000 &&
        abs(ti->longitude) < 180 * 60 * 10000)
    {
        _oLat = ((double)ti->latitude / 10000) / 60;
        _oLon = ((double)ti->longitude / 10000) / 60;

        ownShipLat = _oLat;
        ownShipLon = _oLon;
        ownShipCog = ti->cog / 10;
        ownShipSog = ti->sog;

        _myAis->setOwnShipPos(ownShipLat, ownShipLon);

        // ⭐ SIMPEN DATA OS UTK PANEL
        AISTargetData dataOS;
        dataOS.lat = _oLat;
        dataOS.lon = _oLon;
        dataOS.cog = ti->cog / 10.0;        // Course over ground
        dataOS.sog = ti->sog;               // Speed over ground
        dataOS.heading = ti->heading;       // ⭐ TAMBAHAN: True heading dari compass
        dataOS.cpaCalculatedAt = QDateTime::currentDateTime();

        Ais::instance()->_aisOwnShip = dataOS;

        // Record parsed ownship data to unified database (with protection against infinite loops)
        try {
            //qDebug() << "Ownship received - Lat:" << ((double)ti->latitude / 10000.0) / 60.0 << "| Lon:" << ((double)ti->longitude / 10000.0) / 60.0;

              // RE-ENABLED RECORDING AFTER STABILITY CONFIRMATION
            // Convert SevenCs coordinates to decimal degrees
            double dbOSLat = ((double)ti->latitude / 10000.0) / 60.0;
            double dbOSLon = ((double)ti->longitude / 10000.0) / 60.0;
            double dbOSSog = (ti->sog < 1023) ? ti->sog / 10.0 : -1.0;
            double dbOSCog = (ti->cog < 3600) ? ti->cog / 10.0 : -1.0;
            double dbOSHeading = (ti->heading < 3600) ? ti->heading / 10.0 : -1.0;

            // RE-ENABLED: AIS ownship recording with throttling
            try {
                // Simple throttling - only record every 2 seconds
                static QDateTime lastOwnshipRecord;
                QDateTime currentTime = QDateTime::currentDateTime();

                if (!lastOwnshipRecord.isValid() || lastOwnshipRecord.secsTo(currentTime) >= 2) {
                    // Create AIVDO NMEA string for ownship data
                    QString ownshipNmea = AIVDOEncoder::encodeAIVDO1(dbOSLat, dbOSLon, dbOSCog, dbOSSog, dbOSHeading, 0, 1);

                    AisDatabaseManager::instance().insertParsedOwnshipData(
                        ownshipNmea,
                        "ownship",
                        dbOSLat, dbOSLon, dbOSSog, dbOSCog, dbOSHeading
                    );
                    lastOwnshipRecord = currentTime;
                }
            } catch (const std::exception& e) {
                qWarning() << "Error recording ownship data:" << e.what();
            }
        } catch (const std::exception& e) {
            qWarning() << "Error in ownship data processing:" << e.what();
        }

        // EKOR OWNSHIP
        if (ownShipLat != 0 && ownShipLon != 0 && _wParent->getOwnShipTrail()) {
            int setting = _wParent->getTrackLine();
            if (setting == 1){
                int minute = SettingsManager::instance().data().trailMinute;
                QDateTime now = QDateTime::currentDateTime();
                if (lastTrailDrawTime.isNull() || lastTrailDrawTime.secsTo(now) >= minute * 60) {
                    _wParent->ownShipTrailPoints.append(qMakePair(QString::number(ownShipLat, 'f', 10), QString::number(ownShipLon, 'f', 10)));
                    lastTrailDrawTime = now;
                }
            }
            else if (setting == 2){
                double trailDistanceMeters = SettingsManager::instance().data().trailDistance * 1852.0;

                if (!_wParent->ownShipTrailPoints.isEmpty()) {
                    auto last = _wParent->ownShipTrailPoints.last();
                    double lastLat = last.first.toDouble();
                    double lastLon = last.second.toDouble();
                    double dist = _wParent->haversine(lastLat, lastLon, ownShipLat, ownShipLon);

                    if (dist >= trailDistanceMeters) {
                        _wParent->ownShipTrailPoints.append(qMakePair(
                            QString::number(ownShipLat, 'f', 10),
                            QString::number(ownShipLon, 'f', 10)
                            ));
                    }
                } else {
                    // Titik pertama langsung disimpan
                    _wParent->ownShipTrailPoints.append(qMakePair(
                        QString::number(ownShipLat, 'f', 10),
                        QString::number(ownShipLon, 'f', 10)
                        ));
                }
            }
            else {
                _wParent->ownShipTrailPoints.append(qMakePair(QString::number(ownShipLat, 'f', 10), QString::number(ownShipLon, 'f', 10)));
            }
        }
    }
}

// Fungsi khusus untuk handle AIS targets (bukan ownship)
void Ais::handleAISTargetUpdate(EcAISTargetInfo *ti)
{
    if (ti->ownShip == True) return; // Skip jika ini ownship

    EcCoordinate ownShipLat = 0, ownShipLon = 0;
    double ownShipSog = _dSpeed, ownShipCog = _dCourse;

    double dist = 0, bear = 0;
    double lat = ((double)ti->latitude / 10000.0) / 60.0;
    double lon = ((double)ti->longitude / 10000.0) / 60.0;

    _myAis->getOwnShipPos(ownShipLat, ownShipLon);
    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, lat, lon, ownShipLat, ownShipLon, &dist, &bear);

    // Process only targets which are further away than 48 nm or no own ship position exists yet
    if (true)
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
            // EcAISSetTargetTrackingStatus(feat, _dictInfo, aisTrkStatus, NULL);

            // Set the remaining attributes of the ais target feature
            EcAISSetTargetObjectData(feat, _dictInfo, ti, &_bSymbolize);

            _myAis->setTargetPos(lat, lon);

            // SIMPEN DATA AIS TARGET
            if (ti && ti->mmsi != 0)
            {
                AISTargetData data;
                data.mmsi = QString::number(ti->mmsi);
                data.lat = lat;
                data.lon = lon;
                // Scale AIS motion fields and handle 'not available' sentinels
                // SOG: 0.1 kt units, 1023 = not available
                if (ti->sog >= 1023) {
                    data.sog = -1; // mark invalid
                } else {
                    data.sog = ti->sog / 10.0;
                }
                // COG: 0.1 deg units, 3600 = not available
                if (ti->cog >= 3600) {
                    data.cog = -1; // mark invalid
                } else {
                    data.cog = ti->cog / 10.0;
                }
                data.lastUpdate = QDateTime::currentDateTime();
                data.currentRange = dist;
                data.relativeBearing = bear;
                data.cpaCalculatedAt = QDateTime::currentDateTime();
                data.feat = feat;
                data._dictInfo = _dictInfo;
                data.isDangerous = (aisTrkStatus == aisDangerous);

                data.rawInfo = *ti;
                _myAis->postTargetUpdate(data);

                // RE-ENABLED: AIS target recording with throttling
                try {
                    // Simple throttling - only record every 5 seconds per MMSI
                    static QHash<quint32, QDateTime> lastTargetRecords;
                    QDateTime currentTime = QDateTime::currentDateTime();

                    if (!lastTargetRecords.contains(ti->mmsi) ||
                        lastTargetRecords[ti->mmsi].secsTo(currentTime) >= 5) {

                        qDebug() << "AIS Target received - MMSI:" << ti->mmsi << "| Lat:" << ((double)ti->latitude / 10000.0) / 60.0;

                        // Convert position for database (from SevenCs 1/10000° to decimal degrees)
                        double dbLat = ((double)ti->latitude / 10000.0) / 60.0;
                        double dbLon = ((double)ti->longitude / 10000.0) / 60.0;
                        double dbSog = (ti->sog < 1023) ? ti->sog / 10.0 : -1.0;
                        double dbCog = (ti->cog < 3600) ? ti->cog / 10.0 : -1.0;
                        double dbHeading = (ti->heading < 3600) ? ti->heading / 10.0 : -1.0;

                        // Create NMEA string from parsed data (for record keeping)
                        QString reconstructedNmea = QString("!AIVDM,,A,,%1,%2,%3,%4,5*55")
                            .arg(ti->mmsi)
                            .arg(QString::number(dbLat, 'f', 6))
                            .arg(QString::number(dbLon, 'f', 6))
                            .arg(QString::number(ti->navStatus));

                        AisDatabaseManager::instance().insertParsedAisData(
                            reconstructedNmea,
                            "aistarget",
                            ti->mmsi,
                            *ti
                        );

                        lastTargetRecords[ti->mmsi] = currentTime;
                    }
                } catch (const std::exception& e) {
                    qWarning() << "Error recording AIS data:" << e.what();
                }

                // Ais::instance()->_aisTargetInfoMap[ti->mmsi] = *ti;
                // Ais::instance()->_aisTargetMap[ti->mmsi] = data;
            }

            // EMIT KE CPA/TCPA PANEL

            // Legacy database insert (commented out - using unified table instead)
            // AisDatabaseManager::instance().insertOrUpdateAisTarget(*ti); // EcAISTargetInfo
        }
    }

    // Emit signal untuk refresh chart display
    //EcCoordinate ownLat, ownLon;
    //_myAis->getTargetPos(ownLat, ownLon);
    //_myAis->emitSignalTarget(ownLat, ownLon);
}

void Ais::postTargetUpdate(const AISTargetData& info){
    emit targetUpdateReceived(info);
}
void Ais::setTargetManualStatus(unsigned int mmsi, EcAISTrackingStatus status)
{
    // Try to use cached feature first
    if (_aisTargetMap.contains(mmsi)) {
        AISTargetData &td = _aisTargetMap[mmsi];
        if (ECOK(td.feat) && td._dictInfo) {
            EcAISSetTargetTrackingStatus(td.feat, td._dictInfo, status, NULL);
            return;
        }
    }
    // Resolve via SevenCs API if cached feature invalid
    EcAISTargetInfo* ti = getTargetInfo(mmsi);
    if (!ti) return;
    EcFeature feat = EcAISFindTargetObject(_cid, _dictInfo, ti);
    if (ECOK(feat)) {
        EcAISSetTargetTrackingStatus(feat, _dictInfo, status, NULL);
        // Update cache
        _aisTargetMap[mmsi].feat = feat;
        _aisTargetMap[mmsi]._dictInfo = _dictInfo;
    }
}

void Ais::emitSignal( double lat, double lon, double head )
{
  emit signalRefreshChartDisplay( lat, lon, head );
}

void Ais::emitSignalTarget( double lat, double lon )
{
    emit signalRefreshCenter( lat, lon );
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
  // EcAISSetTargetUpdateCallBack( _transponder, AISTargetUpdateCallback);

  // AUV WORKS
  EcAISSetTargetUpdateCallBack( _transponder, AISTargetUpdateCallbackThread );

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

    QString nmea;
    nmeaSelection(sLine, nmea);

    emit nmeaTextAppend(nmea);
    extractNMEA(nmea);

    // OWNSHIP NMEA
    PickWindow *pickWindow = new PickWindow(parentWidget, dictInfo, denc);
    if (navShip.lat != 0 && ownShipText){
        ownShipText->setHtml(pickWindow->ownShipAutoFill());
    }

    // RECORD NMEA
    IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");

    if (dvr && dvr->isRecording()) {
        dvr->recordRawNmea(nmea);
    }

    // OWNSHIP RIGHT PANEL
    if (navShip.lat != 0 && _cpaPanel){
        _cpaPanel->updateOwnShipInfo(navShip.lat, navShip.lon, navShip.sog, navShip.heading_og);
    }

    //qDebug() << nmea;

    if( EcAISAddTransponderOutput( _transponder, (unsigned char*)nmea.toStdString().c_str(), nmea.count() ) == False )
    {
      addLogFileEntry( QString( "Error in readAISLogfile(): EcAISAddTransponderOutput() failed in input line %1" ).arg( iLineNo ) );
      break;
    }

    // hapus pickwindow
    delete pickWindow;

    iLineNo++;
  }
}

void Ais::nmeaSelection(const QString &line, QString &outNmea) {
    outNmea.clear();

    if (line.contains("!AIVDM") || line.contains("!AIVDO")) {
        outNmea = line;
        return;
    }

    //qDebug() << line;

    // Coba parse JSON
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject obj = doc.object();

    if (obj.contains("NAV_LAT") && obj["NAV_LAT"].toDouble() != 0){ navShip.lat = obj["NAV_LAT"].toDouble();}
    if (obj.contains("NAV_LONG") && obj["NAV_LONG"].toDouble() != 0){ navShip.lon = obj["NAV_LONG"].toDouble();}
    if (obj.contains("NAV_DEPTH")){ navShip.depth = obj["NAV_DEPTH"].toDouble();}
    if (obj.contains("NAV_HEADING")){ navShip.heading = obj["NAV_HEADING"].toDouble();}
    if (obj.contains("NAV_HEADING_OVER_GROUND")){ navShip.heading_og = obj["NAV_HEADING_OVER_GROUND"].toDouble();}
    if (obj.contains("NAV_SPEED")){ navShip.speed = obj["NAV_SPEED"].toDouble();}
    if (obj.contains("NAV_SOG")){ navShip.sog = obj["NAV_SOG"].toDouble();}
    if (obj.contains("NAV_YAW")){ navShip.yaw = obj["NAV_YAW"].toDouble();}
    if (obj.contains("NAV_Z")){ navShip.z = obj["NAV_Z"].toDouble();}

    //qDebug() << "LAT: " << navShip.lat;

    if (navShip.lat == 0) { qDebug() << line; }

    qDebug() << QString("%1, %2, %3, %4").arg(navShip.lat).arg(navShip.lon).arg(navShip.sog).arg(navShip.heading_og);

    //outNmea = AIVDOEncoder::encodeAIVDO(0, navShip.lat, navShip.lon, navShip.speed, navShip.heading_og);
    outNmea = AIVDOEncoder::encodeAIVDO1(navShip.lat, navShip.lon, navShip.heading_og, navShip.sog, navShip.heading, 0, 1);
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

        emit nmeaTextAppend(sLine);
        extractNMEA(sLine);

        PickWindow *pickWindow = new PickWindow(parentWidget, dictInfo, denc);
        if (navShip.lat != 0 && ownShipText) {
            ownShipText->setHtml(pickWindow->ownShipAutoFill());
        }

        IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");
        if (dvr && dvr->isRecording()) {
            dvr->recordRawNmea(sLine);
        }

        if (navShip.lat != 0 && _cpaPanel) {
            _cpaPanel->updateOwnShipInfo(navShip.lat, navShip.lon, navShip.sog, navShip.heading_og);
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
    if (nmea.contains("!AIVDO")){
        navShip.lat = AisDecoder::decodeAisOption(nmea, "latitude", "!AIVDO");
        navShip.lon = AisDecoder::decodeAisOption(nmea, "longitude", "!AIVDO");
        navShip.sog = AisDecoder::decodeAisOption(nmea, "sog", "!AIVDO");
        navShip.heading_og = AisDecoder::decodeAisOption(nmea, "cog", "!AIVDO") / 10;
    }
}

void Ais::clearTargetData()
{
    _aisTargetMap.clear();
    _aisTargetInfoMap.clear();

    if (_transponder)
    {
        EcAISDeleteAllTargets(_transponder);
    }
}

// Recreate SevenCs transponder cleanly (used on reconnect)
Bool Ais::recreateTransponder()
{
  if (_transponder) {
    EcAISDeleteTransponder(&_transponder);
    _transponder = NULL;
  }

  if (EcAISNewTransponder(&_transponder,
                          _sAisLib.toLatin1(),
                          _bInternalGPS ? eGPSt_internalGPS : eGPSt_externalGPS) == False) {
    if (_transponder) {
      EcAISDeleteTransponder(&_transponder);
      _transponder = NULL;
    }
    addLogFileEntry(QString("recreateTransponder(): EcAISNewTransponder failed"));
    return False;
  }

  EcAISSetTargetUpdateCallBack(_transponder, AISTargetUpdateCallbackThread);
  return True;
}

// Clear SevenCs transponder targets but preserve in-memory target map to continue context after reconnect
void Ais::resetTransponderPreserveTargets()
{
    // Invalidate feature handles so UI code won't touch stale features
    for (auto it = _aisTargetMap.begin(); it != _aisTargetMap.end(); ++it) {
        it.value().feat.id = EC_NOCELLID; // mark invalid
        it.value().feat.offset = 0;
        // ensure dictInfo set for rebuilds if needed
        it.value()._dictInfo = _dictInfo;
    }

    // Do not clear transponder targets here to avoid library-side race during reconnect.
    // We rely on incoming AIVDM to refresh existing targets in place.
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

    // NULL DECLARATION
    EcDENC *denc = nullptr;
    EcDictInfo *dictInfo = nullptr;
    QWidget *parentWidget = nullptr;

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
        emit nmeaTextAppend(sLine);
        extractNMEA(sLine);

        // qDebug() << sLine;

        // OWNSHIP PANEL
        PickWindow *pickWindow = new PickWindow(parentWidget, dictInfo, denc);

        if (navShip.lat != 0 && ownShipText){
            ownShipText->setHtml(pickWindow->ownShipAutoFill());
            if (_cpaPanel){
                _cpaPanel->updateOwnShipInfo(navShip.lat, navShip.lon, navShip.sog, navShip.heading_og);
            }
        }

        std::string lineStd = line.toStdString();
        if( EcAISAddTransponderOutput( _transponder, (unsigned char*)lineStd.c_str(), line.count() ) == False )
        {
            addLogFileEntry( QString( "Error in readAISLogfile(): EcAISAddTransponderOutput() failed in input line %1" ).arg( iLineNo ) );
            break;
        }

        iLineNo++;
    }

    stopAnimation();
}

void Ais::readAISVariableThread(const QStringList &dataLines)
{
    _bReadFromFile = False;
    _bReadFromVariable = True;
    _bReadFromServer = False;

    if( !_transponder )
    {
        qCritical() << ( QString( "Error in readAISVariable(): Transponder object is not initialized!" ) );
        return;
    }

    if( _bReadFromVariable == False )
    {
        qCritical() << ( QString( "Error in readAISVariable(): Read from AIS variable permitted!" ) );
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

        // APPEND NMEA TO SIGNAL
        emit nmeaTextAppend(sLine);

        // extractNMEA(sLine);

        // qDebug() << sLine;

        // emit pickWindowOwnship();

        std::string lineStd = line.toStdString();
        if( EcAISAddTransponderOutput( _transponder, (unsigned char*)lineStd.c_str(), line.count() ) == False )
        {
            qDebug() << ( QString( "Error in readAISLogfile(): EcAISAddTransponderOutput() failed in input line %1" ).arg( iLineNo ) );
            break;
        }

        iLineNo++;
    }

    stopAnimation();
}

void Ais::readAISVariableString( const QString &data )
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

    if( _bReadFromVariable == False )
    {
        return;
    }

    if( _transponder == NULL )
    {
        return;
    }

    QString line = data + "\r\n";
    // qDebug() << data;

    if( EcAISAddTransponderOutput( _transponder, (unsigned char*)line.toStdString().c_str(), line.count() ) == False )
    {
        addLogFileEntry( QString( "Error in readAISLogfile(): EcAISAddTransponderOutput() failed in input line %1" ).arg( iLineNo ) );
        return;
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
  //_featureOwnShip = EcObjectCreate(_cid, _dictInfo, "ownshp", EC_OS_DELETABLE, "", '|', coorList, 1, EC_P_PRIM, &primitive);

  // ^^ ERASE DUMMY OWNSHIP ICON AT 0, 0
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

