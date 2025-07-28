// #include <QtGui>
#include <QtWidgets>

#include <QPluginLoader>
#include <QDir>
#include <QMessageBox>
#include <QTimer>

#include "mainwindow.h"
#include "pickwindow.h"
#include "searchwindow.h"
#include "iplugininterface.h"
#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "PluginManager.h"
#include "aisdecoder.h"
#include "guardzone.h"
#include "guardzonemanager.h"
#include "alertpanel.h"
#include "alertsystem.h"
#include "cpatcpasettingsdialog.h"
#include "cpatcpasettings.h"
#include "ecwidget.h"

#include "aisdecoder.h"
#include "aivdoencoder.h"

QTextEdit *informationText;

void MainWindow::createDockWindows()
{
    QMenu* viewMenu = menuBar()->addMenu(tr("&Sidebar"));
    QDockWidget *dock = new QDockWidget(tr("AIS Target"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    aisText = new QTextEdit(dock);
    aisText->setText("");
    aisText->setReadOnly(true); // Membuat teks tidak dapat diedit
    dock->setWidget(aisText);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());
    dock->hide();

    dock = new QDockWidget(tr("NMEA Received"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    nmeaText = new QTextEdit(dock);
    nmeaText->setText("");
    nmeaText->setReadOnly(true); // Membuat teks tidak dapat diedit
    dock->setWidget(nmeaText);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());
    dock->hide();

    dock = new QDockWidget(tr("Ownship Info"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    ownShipText = new QTextEdit(dock);
    ownShipText->setText("");
    ownShipText->setReadOnly(true); // Membuat teks tidak dapat diedit
    dock->setWidget(ownShipText);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());
    dock->hide();

    dock = new QDockWidget(tr("Log"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    logText = new QTextEdit(dock);
    logText->setText("");
    logText->setReadOnly(true); // Membuat teks tidak dapat diedit
    dock->setWidget(logText);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());

    // Panel kanan sebagai dock widget
    // dock = new QDockWidget("Informasi", this);
    // QWidget *formWidget = new QWidget;
    // QFormLayout *formLayout = new QFormLayout;

    // QLineEdit *nameEdit = new QLineEdit("Keterangan 1");
    // nameEdit->setReadOnly(true);

    // QLineEdit *ageEdit = new QLineEdit("Keterangan 2");
    // ageEdit->setReadOnly(true);

    // formLayout->addRow("Titel 1", nameEdit);
    // formLayout->addRow("Titel 2", ageEdit);

    // formWidget->setLayout(formLayout);
    // dock->setWidget(formWidget);
    // addDockWidget(Qt::BottomDockWidgetArea, dock);
}

void MainWindow::createDockNmea()
{

}

void MainWindow::createDockOwnship()
{

}

void MainWindow::addTextToBar(QString text){
    nmeaText->append(text);
}

void MainWindow::createActions()
{
    QToolBar *fileToolBar = addToolBar(tr("File"));

    fileToolBar->setStyleSheet("QToolButton { margin-right: 5px; margin-left: 5px; margin-bottom: 5px; margin-top: 5px}");

    const QIcon importIcon = QIcon::fromTheme("import-tree", QIcon(":/images/import.png"));
    QAction *importAct = new QAction(importIcon, tr("&Import Tree"), this);
    importAct->setShortcuts(QKeySequence::New);
    //importAct->setStatusTip(tr("Import tree"));
    connect(importAct, SIGNAL(triggered()), this, SLOT(onImportTree()));
    fileToolBar->addAction(importAct);
    importAct->setToolTip(tr("Import"));

    const QIcon zoomInIcon = QIcon::fromTheme("import-zoomin", QIcon(":/images/zoom-in.png"));
    QAction *zoomInAct = new QAction(zoomInIcon, tr("&Zoom In"), this);
    zoomInAct->setShortcuts(QKeySequence::New);
    //zoomInAct->setStatusTip(tr("Zoom in"));
    connect(zoomInAct, SIGNAL(triggered()), this, SLOT(onZoomIn()));
    fileToolBar->addAction(zoomInAct);
    zoomInAct->setToolTip(tr("Zoom in"));

    const QIcon zoomOutIcon = QIcon::fromTheme("import-zoomout", QIcon(":/images/zoom-out.png"));
    QAction *zoomOutAct = new QAction(zoomOutIcon, tr("&Zoom Out"), this);
    zoomOutAct->setShortcuts(QKeySequence::New);
    //zoomOutAct->setStatusTip(tr("Zoom out"));
    connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(onZoomOut()));
    fileToolBar->addAction(zoomOutAct);
    zoomOutAct->setToolTip(tr("Zoom out"));

    const QIcon rotateLeftIcon = QIcon::fromTheme("import-rotateleft", QIcon(":/images/rotate-left.png"));
    QAction *rotateLeftAct = new QAction(rotateLeftIcon, tr("&Rotate Left"), this);
    rotateLeftAct->setShortcuts(QKeySequence::New);
    //rotateLeftAct->setStatusTip(tr("Rotate Left"));
    connect(rotateLeftAct, SIGNAL(triggered()), this, SLOT(onRotateCW()));
    fileToolBar->addAction(rotateLeftAct);
    rotateLeftAct->setToolTip(tr("Rotate left"));

    const QIcon rotateRightIcon = QIcon::fromTheme("import-rotateright", QIcon(":/images/rotate-right.png"));
    QAction *rotateRightAct = new QAction(rotateRightIcon, tr("&Rotate Right"), this);
    rotateRightAct->setShortcuts(QKeySequence::New);
    //rotateRightAct->setStatusTip(tr("Rotate Right"));
    connect(rotateRightAct, SIGNAL(triggered()), this, SLOT(onRotateCCW()));
    fileToolBar->addAction(rotateRightAct);
    rotateRightAct->setToolTip(tr("Rotate right"));

    const QIcon aisIcon = QIcon::fromTheme("import-ais", QIcon(":/images/ais.png"));
    QAction *aisAct = new QAction(aisIcon, tr("&Ais"), this);
    aisAct->setShortcuts(QKeySequence::New);
    //aisAct->setStatusTip(tr("Ais"));
    connect(aisAct, SIGNAL(triggered()), this, SLOT(onAIS()));
    fileToolBar->addAction(aisAct);
    aisAct->setToolTip(tr("AIS"));

    const QIcon connectIcon = QIcon::fromTheme("import-connect", QIcon(":/images/connect.png"));
    QAction *connectAct = new QAction(connectIcon, tr("&connect"), this);
    connectAct->setShortcuts(QKeySequence::New);
    //connectAct->setStatusTip(tr("Reconnect MOOSDB"));
    connect(connectAct, SIGNAL(triggered()), this, SLOT(subscribeMOOSDB()));
    fileToolBar->addAction(connectAct);
    connectAct->setToolTip(tr("Reconnect MOOSDB"));

    const QIcon disconnectIcon = QIcon::fromTheme("import-disconnect", QIcon(":/images/disconnect.png"));
    QAction *disconnectAct = new QAction(disconnectIcon, tr("&disconnect"), this);
    disconnectAct->setShortcuts(QKeySequence::New);
    //disconnectAct->setStatusTip(tr("Disconnect MOOSDB"));
    disconnect(disconnectAct, SIGNAL(triggered()), this, SLOT(stopSubscribeMOOSDB()));
    fileToolBar->addAction(disconnectAct);
    disconnectAct->setToolTip(tr("Disconnect MOOSDB"));

    // const QIcon saveIcon = QIcon::fromTheme("document-save", QIcon(":/images/save.png"));
    // QAction *saveAct = new QAction(saveIcon, tr("&Save..."), this);
    // saveAct->setShortcuts(QKeySequence::Save);
    // saveAct->setStatusTip(tr("Save the current form letter"));
    // connect(saveAct, &QAction::triggered, this, &MainWindow::save);
    // fileToolBar->addAction(saveAct);

    // const QIcon printIcon = QIcon::fromTheme("document-print", QIcon(":/images/print.png"));
    // QAction *printAct = new QAction(printIcon, tr("&Print..."), this);
    // printAct->setShortcuts(QKeySequence::Print);
    // printAct->setStatusTip(tr("Print the current form letter"));
    // connect(printAct, &QAction::triggered, this, &MainWindow::print);
    // fileToolBar->addAction(printAct);

    // QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    // QToolBar *editToolBar = addToolBar(tr("Edit"));
    // const QIcon undoIcon = QIcon::fromTheme("edit-undo", QIcon(":/images/undo.png"));
    // QAction *undoAct = new QAction(undoIcon, tr("&Undo"), this);
    // undoAct->setShortcuts(QKeySequence::Undo);
    // undoAct->setStatusTip(tr("Undo the last editing action"));
    // connect(undoAct, &QAction::triggered, this, &MainWindow::undo);
    // editMenu->addAction(undoAct);
    // editToolBar->addAction(undoAct);

    // menuBar()->addSeparator();

    // QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    // QAction *aboutAct = helpMenu->addAction(tr("&About"), this, &MainWindow::about);
    // aboutAct->setStatusTip(tr("Show the application's About box"));

    // QAction *aboutQtAct = helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    // aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));

    // ========== GUARDZONE PANEL SHORTCUTS ==========
    QAction* toggleGuardZonePanelAction = new QAction(tr("Toggle GuardZone Panel"), this);
    toggleGuardZonePanelAction->setShortcut(QKeySequence("Ctrl+G"));
    connect(toggleGuardZonePanelAction, &QAction::triggered, [this]() {
        if (guardZoneDock) {
            guardZoneDock->setVisible(!guardZoneDock->isVisible());
        }
    });
    addAction(toggleGuardZonePanelAction);

    // Add to view menu if it exists
    QMenu* viewMenu = menuBar()->findChild<QMenu*>("&Sidebar");
    if (viewMenu) {
        viewMenu->addSeparator();
        viewMenu->addAction(toggleGuardZonePanelAction);
    }
    // ==============================================
}

void MainWindow::loadPlugin()
{
    QString pluginPath = QDir::currentPath() + "/genericplugin.dll";  // sesuaikan path plugin

    QPluginLoader loader(pluginPath);
    QObject *pluginObj = loader.instance();

    if (!pluginObj) {
        qWarning() << "Gagal load plugin:" << loader.errorString();
        return;
    }

    IPluginInterface *plugin = qobject_cast<IPluginInterface *>(pluginObj);
    if (!plugin) {
        qWarning() << "Plugin tidak sesuai interface.";
        return;
    }

    qDebug() << "Plugin loaded:" << plugin->pluginName();

    // Panggil fungsi plugin untuk menampilkan window
    plugin->showWindow();
}

void MainWindow::startAisRecord(){
    IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");

    if (dvr){
        QString logName = "/ais_log_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".nmea.log";
        QString logPath = QCoreApplication::applicationDirPath() + logName;
        dvr->startRecording(logPath);

        qDebug() << "AIS Recording starts..";
        qDebug() << "Log file created: "+logName;

        startAisRecAction->setEnabled(false);
        stopAisRecAction->setEnabled(true);
    }
}

void MainWindow::stopAisRecord(){
    IAisDvrPlugin* dvr = PluginManager::instance().getPlugin<IAisDvrPlugin>("IAisDvrPlugin");

    if (dvr){
        dvr->stopRecording();

        qDebug() << "AIS Recording stopped..";

        startAisRecAction->setEnabled(true);
        stopAisRecAction->setEnabled(false);
    }
}

void MainWindow::nmeaDecode(){
    //QString nmea = "!AIVDM,1,1,,B,17ldJpP0007aA>ctPKjrS@:f05A@,0*07";
    //QString nmea = "!AIVDM,1,1,,A,17ldGv0P007aCeUtPW4KIwvd0@=S,0*62";

    // qDebug() << "NMEA: " << nmea;
    // qDebug() << "MMSI: " << AisDecoder::decodeAisOption(nmea, "mmsi", "!AIVDM");
    // qDebug() << "Latitude: " << AisDecoder::decodeAisOption(nmea, "latitude", "!AIVDM");
    // qDebug() << "Longitude: " << AisDecoder::decodeAisOption(nmea, "longitude", "!AIVDM");
    // qDebug() << "SOG: " << AisDecoder::decodeAisOption(nmea, "sog", "!AIVDM");
    // qDebug() << "COG: " << AisDecoder::decodeAisOption(nmea, "cog", "!AIVDM");
    // qDebug() << "Nav Status: " << AisDecoder::decodeAisOption(nmea, "navStatus", "!AIVDM");
    // qDebug() << "Message Type: " << AisDecoder::decodeAisOption(nmea, "messageType", "!AIVDM");
    // qDebug() << "Pos Accuracy: " << AisDecoder::decodeAisOption(nmea, "posAccuracy", "!AIVDM");
    // qDebug() << "Heading: " << AisDecoder::decodeAisOption(nmea, "heading", "!AIVDM");

    /*
    // OWNSHIP
    double lat = -7.19806403;
    double lon = 112.8;
    double sog = 3.087;
    double cog = 97.74;

    // SHIP 1
    double latx = -7.19712733;
    double lonx = 112.86649704;
    double sogx = 2.159;
    double cogx = 198.04;

    // SHIP 2
    double laty = -7.19644609;
    double lony = 112.82684326;
    double sogy = 0.305;
    double cogy = 90;

    // SHIP 3
    double latz = -7.22556802;
    double lonz = 112.81911850;
    double sogz = 0.974;
    double cogz = 71.49;

    // SHIP 4
    double lata = -7.20198111;
    double lona = 112.80529976;
    double soga = 2.509;
    double coga = 107.45;

    for (int i = 0; i < 100; i ++){
        qDebug() << AIVDOEncoder::encodeAIVDO(0, lat, lon, sog, cog);
        //qDebug() << AIVDOEncoder::encodeAIVDM(366882412, latx, lonx, sogx, cogx);
        qDebug() << AIVDOEncoder::encodeAIVDM(366996240, laty, lony, sogy, cogy);
        qDebug() << AIVDOEncoder::encodeAIVDM(367159080, latz, lonz, sogz, cogz);
        //qDebug() << AIVDOEncoder::encodeAIVDM(366973590, lata, lona, soga, coga);

        lat = lat - 0.0000774897;
        lon = lon + 0.0006701202;

        latx = latx - 0.0001124025;
        lonx = lonx - 0.0003450394;

        laty = laty - 0;
        lony = lony - 0.0000274658;

        latz = latz + 0.0000544958;
        lonz = lonz + 0.00016222;

        lata = lata - 0.00012858;
        lona = lona + 0.0004085541;
    }
    */

    // QStringList *nmea = new QStringList();
    // nmea->append(AIVDOEncoder::encodeAIVDO(0, lat, lon, sog, cog));
    // nmea->append(AIVDOEncoder::encodeAIVDM(366996240, latx, lonx, sogx, cogx));

    // ecchart->ReadAISVariable(*nmea);

    QString string = "!AIVDM,1,1,,B,17lcMBwP00`4313spL<=hgwP2@Lk,0*52";

    AisDecoded decoded = AIVDOEncoder::decodeNMEALine(string);

    qDebug() << "Source:" << decoded.source;
    qDebug() << "Message Type:" << decoded.type;
    qDebug() << "MMSI:" << decoded.mmsi;
    qDebug() << "Latitude:" << decoded.data.latitude;
    qDebug() << "Longitude:" << decoded.data.longitude;
    qDebug() << "COG:" << decoded.data.cog;
    qDebug() << "SOG:" << decoded.data.sog;
    qDebug() << "Heading:" << decoded.data.heading;

    QString encoded;

    if (decoded.type == 1 || decoded.type == 2 || decoded.type == 3) {
        encoded = AIVDOEncoder::encodeAIVDM(
            decoded.type,
            decoded.mmsi,
            decoded.data.navStatus,
            decoded.data.rot,
            decoded.data.sog,
            decoded.data.posAcc ? 1 : 0,
            decoded.data.latitude,
            decoded.data.longitude,
            decoded.data.cog,
            decoded.data.heading,
            decoded.data.timestamp,
            decoded.data.maneuverIndicator,
            decoded.data.raim,
            decoded.data.radioStatus
            );
    }

    QStringList *nmea = new QStringList();
    nmea->append(encoded);

    ecchart->ReadAISVariable(*nmea);
}

void MainWindow::onNmeaReceived(const QString& line) {
    if (aisDvr && aisDvr->isRecording()) {
        aisDvr->recordRawNmea(line);
    }
}

void MainWindow::openSettingsDialog() {
    SettingsDialog dlg(this);
    dlg.loadSettings();
    if (dlg.exec() == QDialog::Accepted) {
        dlg.saveSettings();
        setDisplay();
        
        // Apply default guardzone filters to existing guardzones
        if (ecchart && ecchart->getGuardZoneManager()) {
            ecchart->getGuardZoneManager()->applyDefaultFiltersToExistingGuardZones();
        }
    }
}

void MainWindow::setDisplay(){
    int cs = EC_DAY_BRIGHT;
    if (SettingsManager::instance().data().displayMode == "Dusk")
        cs = EC_DUSK;
    if (SettingsManager::instance().data().displayMode == "Night")
        cs = EC_NIGHT;

    bool gm = ecchart->GetGreyMode();
    int  br = ecchart->GetBrightness();
    ecchart->SetColorScheme(cs, gm, br);
    DrawChart();
}

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent), ecchart(NULL)
{
  // set the registration mode similar to the mode
  // for which the Kernel development license has been registered
  // EcKernelRegisterSetMode(EC_REGISTER_CHECK_DONGLE_7); // for Sentinel dongle
  // EcKernelRegisterSetMode(EC_REGISTER_CHECK_DONGLE_8); // for Rockey dongle
  // EcKernelRegisterSetMode(EC_REGISTER_CHECK_NETCARD);  // for Network card

  // in case the registration dialog for manual input shall be suppressed
   EcKernelRegisterSetMode(EC_REGISTER_QUIET);

  // check the registration status of the Kernel
  int status;
  // if (!EcKernelRegisterGetStatus(&status))
  // {
  //   QString errString = QString("Cannot get registration status (%1)").arg(status);
  //   throw Exception(errString);
  // }
  if (!EcKernelRegisterGetStatus(&status))
  {
      //if mode was not set, default is hard disk. Check net card next
      EcKernelRegisterSetMode(EC_REGISTER_CHECK_NETCARD);
      if (!EcKernelRegisterGetStatus(&status))
      {
          // check Sentinel dongle
          EcKernelRegisterSetMode(EC_REGISTER_CHECK_DONGLE_7);
          if (!EcKernelRegisterGetStatus(&status))
          {
              // check Sentinel dongle
              EcKernelRegisterSetMode(EC_REGISTER_CHECK_DONGLE_8);
              if (!EcKernelRegisterGetStatus(&status))
              {
                  QString errString = QString("Cannot get registration status (%1)").arg(status);
                  throw Exception(errString);
              }
          }
      }
  }

  // check if the required modules are enabled
  if (!EcKernelRegisterTestModule(EC_MODULE_S57_IMPORT)) // for the import of S-57 and S-63 charts
    throw Exception("The S-57 Import Module is not enabled");

  // check if LIB_7CS exsists
  QString libStr = "";
  if(EcKernelGetEnv("LIB_7CS") != NULL)
  {
    libStr = QString(EcKernelGetEnv("LIB_7CS"));
    QDir tmpDir(libStr);
    if(!tmpDir.exists())
    {
      QString errString = "The LIB_7CS directory " + libStr + " does not exist";
      throw Exception(errString);
    }
  }
  else
    throw Exception("LIB_7CS is not set");

  // Get the Kernel version
  int subMinorVersion, type;
  QString version = QString(EcKernelGetVersionExt(&subMinorVersion, &type));

  // QString titleString = QString("showAIS  (" + version + ".%1, LIB_7CS = " + libStr + "), Qt %2.%3")
  //                       .arg(subMinorVersion)
  //                       .arg((QT_VERSION & 0xff0000) >> 16)
  //                       .arg((QT_VERSION & 0xff00) >> 8);
  QString titleString = QString("ECDIS")
                            .arg(subMinorVersion)
                            .arg((QT_VERSION & 0xff0000) >> 16)
                            .arg((QT_VERSION & 0xff00) >> 8);
  setWindowTitle(titleString);

  // Read the dictionaries
  UINT32 module = EC_MODULE_MAIN;
  dict = EcDictionaryReadModule(module, NULL);
  if (! dict) throw Exception("Cannot read dictionary.");

  // create the ecchart widget and pass the dictionary and value of LIB_7CS to it
  try
  {
    ecchart = new EcWidget(dict, &libStr, this);
    ecchart->setMainWindow(this);
  }
  catch (EcWidget::Exception & e)
  {
    throw Exception(e.GetMessages(), "Cannot create ECDIS Widget");
  }
  setCentralWidget(ecchart);

  connect(ecchart, SIGNAL(waypointCreated()), this, SLOT(onWaypointCreated()));
  connect(ecchart, SIGNAL(attachToShipStateChanged(bool)), this, SLOT(onAttachToShipStateChanged(bool)));


  // Define the DENC path
#ifdef _WIN32
  if (EcKernelGetEnv("APPDATA"))
    dencPath = QString(EcKernelGetEnv("APPDATA")) + "/SevenCs/EC2007/DENC";
#else
  if (EcKernelGetEnv("HOME"))
    dencPath = QString(EcKernelGetEnv("HOME")) + "/SevenCs/EC2007/DENC";
#endif

  QDir tmpPath(dencPath);
  if(tmpPath.mkpath(dencPath))
    {
    // Create and initialize the DENC structure
        if (ecchart->CreateDENC(dencPath, true) == false)
            throw Exception("Cannot create DENC Structure (check write access!)");
  }
  // if (!tmpPath.exists())
  // {
  //     tmpPath.mkpath(dencPath);
  // }
  // if (ecchart->CreateDENC(dencPath, true) == true)
  // {
  //     //Preinstall the IHO Certificate
  //     QString certificateFileName = QString(EcKernelGetEnv("EC2007DIR")) + "/data/Sample/S-63/IHO.CRT";
  //     qDebug() << certificateFileName;
  //     QFile certificateFile(certificateFileName);
  //     if (certificateFile.exists())
  //     {
  //         ecchart->ImportIHOCertificate(certificateFileName);
  //     }
  // }
  // else
  // {
  //     throw Exception("Cannot create DENC Structure (check write access!)");
  // }

  // Initialize the S-63 settings
  ecchart->InitS63();

  // Initialize the viewport settings
  ecchart->SetCenter(-7.18551, 112.78012);
  ecchart->SetScale(80000);
  ecchart->SetProjection(EcWidget::MercatorProjection);

  // Initialize the chart display settings which can be set by the user
  lookupTable = EC_LOOKUP_TRADITIONAL;
  displayCategory = EC_STANDARD;
  showLights = false;
  showText = true;
  showNationalText = false;
  showSoundings = false;
  showGrid = false;
  showAIS = true;
  trackShip = true;
  showDangerTarget = true;
  ecchart->SetLookupTable(lookupTable);
  ecchart->SetDisplayCategory(displayCategory);
  ecchart->ShowLights(showLights);
  ecchart->ShowText(showText);
  ecchart->ShowSoundings(showSoundings);
  ecchart->ShowGrid(showGrid);
  ecchart->ShowAIS(showAIS);
  ecchart->TrackShip(trackShip);
  ecchart->ShowDangerTarget(showDangerTarget);
  setDisplay();

  // Create the window for the pick report
  pickWindow = new PickWindow(this, dict, ecchart->GetDENC());

  // Create the window for the search lat lon
  searchWindow = new SearchWindow(tr("Enter Search Details"), this);

  // Iinitialize the AIS settings
  ecchart->InitAIS( dict );

  // Start subscribe MOOSDB
  // ecchart->startAISSubscribe();

  // Create the main user interface 
  connect(ecchart, SIGNAL(scale(int)), this, SLOT( onScale(int)));  
  connect(ecchart, SIGNAL(projection()), this, SLOT( onProjection()));  
  connect(ecchart, SIGNAL(mouseMove(EcCoordinate, EcCoordinate)), this, SLOT( onMouseMove(EcCoordinate, EcCoordinate)));  
  connect(ecchart, SIGNAL(mouseRightClick(QPoint)), this, SLOT( onMouseRightClick(QPoint)));

  // Status bar
  posEdit = new QLineEdit(statusBar());
  posEdit->setReadOnly(true);
  int wi = QFontMetrics(posEdit->font()).horizontalAdvance("12 34.56 N 123 45.678 E");
  posEdit->setFixedWidth(wi + 30);

  proEdit = new QLineEdit(statusBar());
  proEdit->setReadOnly(true);
  wi = QFontMetrics(proEdit->font()).horizontalAdvance("Cylindrical Equidistant");
  proEdit->setFixedWidth(wi + 10);

  sclEdit = new QLineEdit(statusBar());
  sclEdit->setReadOnly(true);
  wi = QFontMetrics(sclEdit->font()).horizontalAdvance("1:25000000");
  sclEdit->setFixedWidth(wi + 10);

  rngEdit = new QLineEdit(statusBar());
  rngEdit->setReadOnly(true);
  wi = QFontMetrics(rngEdit->font()).horizontalAdvance("2000");
  rngEdit->setFixedWidth(wi + 10);

  oriEdit = new QLineEdit(statusBar());
  oriEdit->setReadOnly(true);
  wi = QFontMetrics(oriEdit->font()).horizontalAdvance("2000");
  oriEdit->setFixedWidth(wi + 10);
  oriEdit->setText("0°");

  statusBar()->addPermanentWidget(new QLabel("Chart Rotation:", statusBar()));
  statusBar()->addPermanentWidget(oriEdit, 0);
  statusBar()->addPermanentWidget(new QLabel("Range:", statusBar()));
  statusBar()->addPermanentWidget(rngEdit, 0);
  statusBar()->addPermanentWidget(new QLabel("Scale:", statusBar()));
  statusBar()->addPermanentWidget(sclEdit, 0);
  statusBar()->addPermanentWidget(new QLabel("Projection:", statusBar()));
  statusBar()->addPermanentWidget(proEdit, 0);
  statusBar()->addPermanentWidget(new QLabel("Cursor:", statusBar()));
  statusBar()->addPermanentWidget(posEdit, 0);

  /*
  // PLEASE WAIT
  // File menu
  QMenu *fileMenu = menuBar()->addMenu("&File");

  fileMenu->addAction("E&xit", this, SLOT(close()))->setShortcut(tr("Ctrl+x", "File|Exit"));
  // fileMenu->addAction("Reload", this, SLOT(onReload()));
  */

  // DENC menu
  QMenu *importMenu = menuBar()->addMenu("&Import");

  dencActionGroup = new QActionGroup(this);

  QAction * action = dencActionGroup->addAction("Tree");
  connect(action, SIGNAL(triggered()), this, SLOT(onImportTree()));
  importMenu->addAction(action);

  action = dencActionGroup->addAction("S-57 Exchange Set");
  connect(action, SIGNAL(triggered()), this, SLOT(onImportS57()));
  importMenu->addAction(action);

  importMenu->addSeparator();

  action = dencActionGroup->addAction("IHO Certificate");
  connect(action, SIGNAL(triggered()), this, SLOT(onImportIHOCertificate()));
  importMenu->addAction(action);

  action = dencActionGroup->addAction("S-63 Permits");
  connect(action, SIGNAL(triggered()), this, SLOT(onImportS63Permits()));
  importMenu->addAction(action);

  action = dencActionGroup->addAction("S-63 Exchange Set");
  connect(action, SIGNAL(triggered()), this, SLOT(onImportS63()));
  importMenu->addAction(action);

  dencActionGroup->setEnabled(!dencPath.isEmpty());

  // Draw menu
  QMenu *drawMenu = menuBar()->addMenu("&Control");

  drawMenu->addAction("Zoom In", this, SLOT(onZoomIn()))->setShortcut(tr("PgUp", "Draw|Zoom In"));
  drawMenu->addAction("Zoom Out", this, SLOT(onZoomOut()))->setShortcut(tr("PgDown", "Draw|Zoom out"));

  drawMenu->addSeparator();

  drawMenu->addAction("Shift Left", this, SLOT(onLeft()))->setShortcut(tr("Left", "Draw|Left"));
  drawMenu->addAction("Shift Right", this, SLOT(onRight()))->setShortcut(tr("Right", "Draw|Right"));
  drawMenu->addAction("Shift Up", this, SLOT(onUp()))->setShortcut(tr("Up", "Draw|Up"));
  drawMenu->addAction("Shift Down", this, SLOT(onDown()))->setShortcut(tr("Down", "Draw|Down"));

  drawMenu->addSeparator();

  drawMenu->addAction("Rotate Clockwise", this, SLOT(onRotateCW()))->setShortcut(tr("+", "Draw|Rotate Clockwise"));
  drawMenu->addAction("Rotate AntiClockwise", this, SLOT(onRotateCCW()))->setShortcut(tr("-", "Draw|Rotate Anticlockwise"));

  drawMenu->addSeparator();

  QActionGroup *pActionGroup = new QActionGroup(this);
  autoProjectionAction = pActionGroup->addAction("Automatic");
  mercatorAction       = pActionGroup->addAction("Mercator");
  gnomonicAction       = pActionGroup->addAction("Gnomonic");
  stereographicAction  = pActionGroup->addAction("Stereographic");
  autoProjectionAction->setCheckable(true);
  mercatorAction->setCheckable(true);
  gnomonicAction->setCheckable(true);
  stereographicAction->setCheckable(true);
  mercatorAction->setChecked(true);
  connect(pActionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onProjection(QAction *)));

  // drawMenu->addAction(autoProjectionAction);
  // drawMenu->addAction(mercatorAction);
  // drawMenu->addAction(gnomonicAction);
  // drawMenu->addAction(stereographicAction);

  // View menu
  QMenu *viewMenu = menuBar()->addMenu("&Layers");

  QActionGroup *lActionGroup = new QActionGroup(this);
  simplifiedAction  = lActionGroup->addAction("Simplified");
  fullChartAction   = lActionGroup->addAction("Full Chart");
  simplifiedAction->setCheckable(true);
  fullChartAction->setCheckable(true);
  if(lookupTable == EC_LOOKUP_SIMPLIFIED)
    simplifiedAction->setChecked(true);
  else
    fullChartAction->setChecked(true);
  connect(lActionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onLookup(QAction *)));

  viewMenu->addAction(simplifiedAction);
  viewMenu->addAction(fullChartAction);

  viewMenu->addSeparator();

  QActionGroup *vActionGroup = new QActionGroup(this);
  baseAction = vActionGroup->addAction("Display Base");
  standardAction    = vActionGroup->addAction("Standard");
  otherAction       = vActionGroup->addAction("Detailed");
  baseAction->setCheckable(true);
  standardAction->setCheckable(true);
  otherAction->setCheckable(true);
  if(displayCategory == EC_OTHER)
    otherAction->setChecked(true);
  else if(displayCategory == EC_STANDARD)
    standardAction->setChecked(true);
  else
    baseAction->setChecked(true);

  connect(vActionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onDisplayCategory(QAction *)));

  viewMenu->addAction(baseAction);
  viewMenu->addAction(standardAction);
  viewMenu->addAction(otherAction);
  viewMenu->addSeparator();

  QAction *lightsAction = viewMenu->addAction("Lights");
  lightsAction->setCheckable(true);
  lightsAction->setChecked(showLights);
  connect(lightsAction, SIGNAL(toggled(bool)), this, SLOT(onLights(bool)));

  QAction *textAction = viewMenu->addAction("Text");
  textAction->setCheckable(true);
  textAction->setChecked(showText);
  connect(textAction, SIGNAL(toggled(bool)), this, SLOT(onText(bool)));

  QAction *soundingsAction = viewMenu->addAction("Soundings");
  soundingsAction->setCheckable(true);
  soundingsAction->setChecked(showSoundings);
  connect(soundingsAction, SIGNAL(toggled(bool)), this, SLOT(onSoundings(bool)));

  viewMenu->addSeparator();

  QAction *gridAction = viewMenu->addAction("Grid");
  gridAction->setCheckable(true);
  gridAction->setChecked(showGrid);
  connect(gridAction, SIGNAL(toggled(bool)), this, SLOT(onGrid(bool)));

  viewMenu->addSeparator();

  //QAction *aisAction = viewMenu->addAction("AIS targets");
  QAction *aisAction = viewMenu->addAction("Own ship");
  aisAction->setCheckable(true);
  aisAction->setChecked(showAIS);
  connect(aisAction, SIGNAL(toggled(bool)), this, SLOT(onAIS(bool)));

  QAction *trackAction = viewMenu->addAction("Track Ship");
  trackAction->setCheckable(true);
  trackAction->setChecked(trackShip);
  connect(trackAction, SIGNAL(toggled(bool)), this, SLOT(onTrack(bool)));

  viewMenu->addSeparator();

  QAction *trailAction = viewMenu->addAction("Clear Trail");
  connect(trailAction, &QAction::triggered, this, [=]() {
      ecchart->clearOwnShipTrail();
      update(); // misalnya untuk redraw
  });

  viewMenu->addSeparator();

  // QAction *searchAction = viewMenu->addAction("Search");
  // connect(searchAction, &QAction::triggered, this, &MainWindow::onSearch);

  // Color menu
  QMenu *colorMenu = menuBar()->addMenu("&Display");

  QActionGroup *cActionGroup = new QActionGroup(this);
  dayAction    = cActionGroup->addAction("Day");
  duskAction = cActionGroup->addAction("Dusk");
  nightAction = cActionGroup->addAction("Night");
  dayAction->setCheckable(true);
  duskAction->setCheckable(true);
  nightAction->setCheckable(true);
  dayAction->setChecked(true);
  connect(cActionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onColorScheme(QAction *)));

  colorMenu->addAction(dayAction);
  colorMenu->addAction(duskAction);
  colorMenu->addAction(nightAction);

  colorMenu->addSeparator();

  QAction *greyAction = colorMenu->addAction("Grey Mode");
  greyAction->setCheckable(true);
  connect(greyAction, SIGNAL(toggled(bool)), this, SLOT(onGreyMode(bool)));

  // MOOSDB
  QMenu *moosMenu = menuBar()->addMenu("&MOOSDB");
  moosMenu->addAction("Restart Connection", this, SLOT(subscribeMOOSDB()) );
  moosMenu->addAction("Stop Connection", this, SLOT(stopSubscribeMOOSDB()) );

  // AIS
  QMenu *aisMenu = menuBar()->addMenu("&AIS");
  aisMenu->addAction( tr( "Run AIS" ), this, SLOT( runAis() ) );
  aisMenu->addAction( tr( "Load AIS Logfile" ), this, SLOT( slotLoadAisFile() ) );
  aisMenu->addAction( tr( "Load AIS Variable" ), this, SLOT( slotLoadAisVariable() ) );
  aisMenu->addAction( tr( "Stop Load AIS Variable" ), this, SLOT( slotStopLoadAisVariable() ) );
  // aisMenu->addAction( tr( "Connect to AIS Server" ), this, SLOT( slotConnectToAisServer() ) );

  // SETTINGS MANAGER
  QMenu *settingMenu = menuBar()->addMenu("&Settings");
  settingMenu->addAction("Setting Manager", this, SLOT(openSettingsDialog()) );

  // SIDEBAR -- PLEASE WAIT
  createDockWindows();
  
  // Setup CPA/TCPA Panel after createDockWindows
  setupCPATCPAPanel();
  if (m_cpatcpaPanel) {
      ecchart->setCPAPanelToAIS(m_cpatcpaPanel);
  }
  
  createActions();

  // PERBAIKAN: Manual sync UI dengan state backend setelah actions dibuat
  // Gunakan QTimer untuk delay sync agar guardzone dari file sudah fully loaded
  QTimer::singleShot(500, [this]() {
      bool hasAttached = ecchart && ecchart->hasAttachedGuardZone();
      qDebug() << "[STARTUP-SYNC-DELAYED] Checking hasAttachedGuardZone:" << hasAttached;
      
      if (hasAttached) {
          // Jika ada guardzone attached, sync UI
          if (attachToShipAction) {
              attachToShipAction->blockSignals(true);
              attachToShipAction->setChecked(true);
              attachToShipAction->blockSignals(false);
              qDebug() << "[STARTUP-SYNC-DELAYED] Manual sync: found attached guardzone, set UI to checked";
          } else {
              qDebug() << "[STARTUP-SYNC-DELAYED] ERROR: attachToShipAction is null!";
          }
      } else {
          qDebug() << "[STARTUP-SYNC-DELAYED] No attached guardzone found, keeping UI unchecked";
      }
  });

  qDebug() << "[STARTUP] MainWindow startup completed, UI synchronized with backend state";

  // GuardZone
  // ========== INITIALIZE GUARDZONE PANEL POINTERS ==========
  guardZonePanel = nullptr;
  guardZoneDock = nullptr;
  // ========================================================

  // ========== INITIALIZE ALERT PANEL POINTERS ==========
  alertPanel = nullptr;
  alertDock = nullptr;
  // ===================================================

  
  // Waypoint
  QMenu *waypointMenu = menuBar()->addMenu("&Waypoint");

  waypointMenu->addAction("Create", this, SLOT(onCreateWaypoint()));
  waypointMenu->addAction("Remove", this, SLOT(onRemoveWaypoint()));
  waypointMenu->addAction("Move", this, SLOT(onMoveWaypoint()));
  waypointMenu->addAction("Edit", this, SLOT(onEditWaypoint()));

  waypointMenu->addSeparator();
  waypointMenu->addAction("Import...", this, SLOT(onImportWaypoints()));
  waypointMenu->addAction("Export...", this, SLOT(onExportWaypoints()));
  waypointMenu->addAction("Clear All", this, SLOT(onClearWaypoints()));

  // GuardZone menu
  QMenu *guardZoneMenu = menuBar()->addMenu("&GuardZone");

  QAction *enableGuardZoneAction = guardZoneMenu->addAction("Enable GuardZone");
  enableGuardZoneAction->setCheckable(true);
  enableGuardZoneAction->setChecked(true);  // PERBAIKAN: Default checked
  connect(enableGuardZoneAction, SIGNAL(toggled(bool)), this, SLOT(onEnableGuardZone(bool)));

  guardZoneMenu->addSeparator();

  guardZoneMenu->addAction("Create Circular GuardZone", this, SLOT(onCreateCircularGuardZone()));
  guardZoneMenu->addAction("Create Polygon GuardZone", this, SLOT(onCreatePolygonGuardZone()));

  guardZoneMenu->addSeparator();

  guardZoneMenu->addAction("Check for Threats", this, SLOT(onCheckGuardZone()));

  attachToShipAction = guardZoneMenu->addAction("Attach to Ship");
  attachToShipAction->setCheckable(true);
  attachToShipAction->setChecked(false);  // PERBAIKAN: Set default false, akan diupdate oleh signal dari EcWidget
  connect(attachToShipAction, SIGNAL(toggled(bool)), this, SLOT(onAttachGuardZoneToShip(bool)));

  guardZoneMenu->addSeparator();

  // ========== TAMBAHAN BARU: SUB MENU TEST GUARDZONE ==========
  QAction *testGuardZoneAction = guardZoneMenu->addAction("Test guardzone");
  testGuardZoneAction->setCheckable(true);
  connect(testGuardZoneAction, SIGNAL(toggled(bool)), this, SLOT(onTestGuardZone(bool)));
  // ===========================================================

  guardZoneMenu->addSeparator();

  QAction *autoCheckShipGuardianAction = guardZoneMenu->addAction("Auto-Check Ship Guardian");
  autoCheckShipGuardianAction->setCheckable(true);
  connect(autoCheckShipGuardianAction, SIGNAL(toggled(bool)), this, SLOT(onAutoCheckShipGuardian(bool)));

  guardZoneMenu->addAction("Check Ship Guardian Now", this, SLOT(onCheckShipGuardianNow()));

  guardZoneMenu->addSeparator();
  // Auto-check menu items
  QAction *autoCheckAction = guardZoneMenu->addAction("Enable Auto-Check");
  autoCheckAction->setCheckable(true);
  autoCheckAction->setChecked(true);  // PERBAIKAN: Default checked sesuai dengan backend setting
  connect(autoCheckAction, &QAction::toggled, this, &MainWindow::onToggleGuardZoneAutoCheck);

  QAction *configAutoCheckAction = guardZoneMenu->addAction("Configure Auto-Check...");
  connect(configAutoCheckAction, &QAction::triggered, this, &MainWindow::onConfigureGuardZoneAutoCheck);

  guardZoneMenu->addSeparator();
  QAction *realTimeStatusAction = guardZoneMenu->addAction("Show Real-Time Status");
  connect(realTimeStatusAction, &QAction::triggered, this, &MainWindow::onShowGuardZoneStatus);


  QMenu *simulationMenu = menuBar()->addMenu("&Simulation");

  simulationMenu->addAction("Start AIS Target Simulation", this, SLOT(onStartSimulation()));
  simulationMenu->addAction("Stop Simulation", this, SLOT(onStopSimulation()));

  simulationMenu->addSeparator();

  // QAction *autoCheckAction = simulationMenu->addAction("Auto-Check GuardZone");
  // autoCheckAction->setCheckable(true);
  // connect(autoCheckAction, SIGNAL(toggled(bool)), this, SLOT(onAutoCheckGuardZone(bool)));

  // DVR
  QMenu *dvrMenu = menuBar()->addMenu("&AIS DVR");

  startAisRecAction = dvrMenu->addAction("Start Record", this, SLOT(startAisRecord()) );
  stopAisRecAction = dvrMenu->addAction("Stop Record", this, SLOT(stopAisRecord()) );

  startAisRecAction->setEnabled(true);
  stopAisRecAction->setEnabled(false);

  // Debug Purpose
  // PLEASE WAIT!!

  /*
  QMenu *debugMenu = menuBar()->addMenu("&Debug");
  debugMenu->addAction("NMEA Decode", this, SLOT(nmeaDecode()));
  */

  ///====================== IGNORE =============================
  // CPA/TCPA menu
  QMenu *cpaMenu = menuBar()->addMenu("&CPA/TCPA");

  // Sub menu untuk CPA/TCPA
  QAction *cpaSettingsAction = cpaMenu->addAction("CPA/TCPA Settings");
  connect(cpaSettingsAction, SIGNAL(triggered()), this, SLOT(onCPASettings()));

  cpaMenu->addSeparator();

  QAction *showCPATargetsAction = cpaMenu->addAction("Show CPA/TCPA Monitor");
  showCPATargetsAction->setCheckable(true);
  showCPATargetsAction->setChecked(false);
  connect(showCPATargetsAction, SIGNAL(triggered(bool)), this, SLOT(onShowCPATargets(bool)));

  QAction *showTCPAInfoAction = cpaMenu->addAction("Show TCPA Info");
  showTCPAInfoAction->setCheckable(true);
  showTCPAInfoAction->setChecked(false);
  connect(showTCPAInfoAction, SIGNAL(triggered(bool)), this, SLOT(onShowTCPAInfo(bool)));

  cpaMenu->addSeparator();

  QAction *cpaTcpaAlarmsAction = cpaMenu->addAction("Enable CPA/TCPA Alarms");
  cpaTcpaAlarmsAction->setCheckable(true);
  cpaTcpaAlarmsAction->setChecked(true);
  connect(cpaTcpaAlarmsAction, SIGNAL(triggered(bool)), this, SLOT(onCPATCPAAlarms(bool)));



  // PLEASE WAIT!!

  /*
  // DISPLAY ORIENTATION
  QMenu *displayOrientationMenu = menuBar()->addMenu("&Chart Orientation");

  // Buat grup eksklusif
  QActionGroup *orientationGroup = new QActionGroup(this);
  orientationGroup->setExclusive(true); // Hanya satu yang bisa aktif

  // NORTH UP
  QAction *northUpAction = displayOrientationMenu->addAction("North-Up Mode");
  northUpAction->setCheckable(true);
  northUpAction->setChecked(true);
  orientationGroup->addAction(northUpAction);
  connect(northUpAction, SIGNAL(triggered(bool)), this, SLOT(onNorthUp(bool)));

  // HEAD UP
  QAction *headUpAction = displayOrientationMenu->addAction("Head-Up Mode");
  headUpAction->setCheckable(true);
  headUpAction->setChecked(false);
  orientationGroup->addAction(headUpAction);
  connect(headUpAction, SIGNAL(triggered(bool)), this, SLOT(onHeadUp(bool)));

  // COURSE UP
  QAction *courseUpAction = displayOrientationMenu->addAction("Course-Up Mode");
  courseUpAction->setCheckable(true);
  courseUpAction->setChecked(false);
  orientationGroup->addAction(courseUpAction);
  connect(courseUpAction, SIGNAL(triggered(bool)), this, SLOT(onCourseUp(bool)));

  // DISPLAY ORIENTATION
  QMenu *osCenteringMenu = menuBar()->addMenu("&Ownship Centering");

  // Buat grup eksklusif
  QActionGroup *centeringGroup = new QActionGroup(this);
  centeringGroup->setExclusive(true); // Hanya satu yang bisa aktif

  // HEAD UP
  QAction *centeringAction = osCenteringMenu->addAction("Centering Mode");
  centeringAction->setCheckable(true);
  centeringAction->setChecked(true);
  centeringGroup->addAction(centeringAction);
  connect(centeringAction, SIGNAL(triggered(bool)), this, SLOT(onCentered(bool)));

  // NORTH UP
  QAction *lookAheadAction = osCenteringMenu->addAction("Look-Ahead Mode");
  lookAheadAction->setCheckable(true);
  lookAheadAction->setChecked(false);
  centeringGroup->addAction(lookAheadAction);
  connect(lookAheadAction, SIGNAL(triggered(bool)), this, SLOT(onLookAhead(bool)));

  // COURSE UP
  QAction *manualAction = osCenteringMenu->addAction("Manual Offset Mode");
  manualAction->setCheckable(true);
  manualAction->setChecked(false);
  centeringGroup->addAction(manualAction);
  connect(manualAction, SIGNAL(triggered(bool)), this, SLOT(onManual(bool)));
  */

  // Load Plugin
  // loadPluginAis();

    // char *userpermit = nullptr;
    // unsigned char* hwid = (unsigned char*)"95C7-DAC8-182A-403D-257C-C";
    // //unsigned char* hwid = (unsigned char*)"6B4A-4473-2387-B940-5A7C-A";
    // unsigned char* mid = (unsigned char*)"BF";
    // unsigned char* mkey = (unsigned char*)"82115";

    // EcS63CreateUserPermit(hwid, mkey, mid, &userpermit);

    // qDebug() << userpermit;

  // PLEASE WAIT, TURN OFF FOR A MOMENT

  // setupGuardZonePanel();
  // setupAISTargetPanel();
  // setupObstacleDetectionPanel();

  //setupAlertPanel();
  //setupTestingMenu();

  // Initialize CPA/TCPA system
  m_cpaCalculator = new CPATCPACalculator(this);
  m_cpaUpdateTimer = new QTimer(this);
  connect(m_cpaUpdateTimer, SIGNAL(timeout()), this, SLOT(updateCPATCPAForAllTargets()));

  // Setup CPA/TCPA Panel - will be called after createDockWindows
  //setupCPATCPAPanel();
  //ecchart->setCPAPanelToAIS(m_cpatcpaPanel);

  // Connect to settings changes
  connect(&CPATCPASettings::instance(), &CPATCPASettings::settingsChanged, this, [this]() {
      int interval = CPATCPASettings::instance().getAlarmUpdateInterval() * 1000;
      m_cpaUpdateTimer->setInterval(interval);
      qDebug() << "CPA/TCPA update interval changed to:" << interval << "ms";
  });

  // Start timer with current settings
  int interval = CPATCPASettings::instance().getAlarmUpdateInterval() * 1000;
  m_cpaUpdateTimer->setInterval(interval);
  m_cpaUpdateTimer->start();

  qDebug() << "CPA/TCPA system initialized with update interval:" << interval << "ms";

  connect(m_cpatcpaDock, &QDockWidget::visibilityChanged, this, [=](bool visible) {
      if (!visible) {
          showCPATargetsAction->setChecked(false);
      }
      else {
          showCPATargetsAction->setChecked(true);
      }
  });



#ifdef _DEBUG
      // Testing menu hanya untuk debug build
      setupTestingMenu();
#endif

  // PERBAIKAN: Pastikan menu states sinkron dengan backend states saat startup
  if (ecchart) {
      // Inisialisasi GuardZone sesuai default checkbox (true)
      ecchart->enableGuardZone(true);
      
      // Inisialisasi Auto-Check sesuai default checkbox (true) - sudah diset di EcWidget constructor
      // ecchart->setGuardZoneAutoCheck(true); // Tidak perlu karena sudah default true di EcWidget
      
      qDebug() << "[STARTUP] Menu states synchronized with backend: GuardZone=true, AutoCheck=true";
  }
}

/*---------------------------------------------------------------------------*/

MainWindow::~MainWindow()
{
  if (dict)
  {
    EcDictionaryFree(dict);
    dict = NULL;
  }
  if (pickWindow)
    delete pickWindow;

  if (aisDvr && aisDvr->isRecording()) {
      aisDvr->stopRecording();
  }

  // ========== CLEANUP GUARDZONE PANEL ==========
  if (guardZonePanel) {
      delete guardZonePanel;
      guardZonePanel = nullptr;
  }

  if (guardZoneDock) {
      delete guardZoneDock;
      guardZoneDock = nullptr;
  }
  // ============================================

  // ========== CLEANUP ALERT PANEL ==========
  if (alertPanel) {
      delete alertPanel;
      alertPanel = nullptr;
  }

  if (alertDock) {
      delete alertDock;
      alertDock = nullptr;
  }
  // ========================================

  if (m_cpaUpdateTimer) {
      m_cpaUpdateTimer->stop();
  }
}

void MainWindow::onReload()
{
    DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onImportTree()
{
  QString dir = QFileDialog::getExistingDirectory(this, tr("Import Tree"),
    // QString(getenv("DATA_7CS"))+"/Sample/SENC",
    QString("C:/ECDIS/map"),
    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks); 

  if (!dir.isEmpty())
  {
    impPath = dir;
    // Import all unencrypted plain S-57 or SENC charts recursively
    ecchart->ImportTree(dir);
    // Apply any S-57 update files if exist
    ecchart->ApplyUpdate();
    DrawChart();
  }
}

/*---------------------------------------------------------------------------*/

void MainWindow::onImportS57()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Import S-57 Exchange Set"),
                                                    QString(getenv("DATA_7CS"))+"/Sample/S-57/ENC_ROOT",
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks | QFileDialog::DontUseNativeDialog);

    if (!dir.isEmpty())
    {
        impPath = dir;
        // Import an unencrypted S-57 Exchange Set
        QApplication::setOverrideCursor(Qt::WaitCursor);
        ecchart->ImportS57ExchangeSet(dir);
        // Apply any S-57 update files if exist
        ecchart->ApplyUpdate();
        QApplication::restoreOverrideCursor();
        DrawChart();
    }
}

/*---------------------------------------------------------------------------*/

void MainWindow::onImportIHOCertificate()
{
    QString name = QFileDialog::getOpenFileName(this, tr("Import S-63"),
                                                QString(getenv("DATA_7CS"))+"/Sample/S-63", "IHO Certificate (*.CRT)");

    if (name.size() > 0)
    {
        if(ecchart->ImportIHOCertificate(name))
            QMessageBox::information(this, "S-63 Import", "IHO Certificate sucessfully read");
        else
            QMessageBox::critical(this, "S-63 Import", "Reading IHO Certificate failed");
    }
}

/*---------------------------------------------------------------------------*/

void MainWindow::onImportS63Permits()
{
    QStringList names = QFileDialog::getOpenFileNames(this, tr("Import S-63"),
                                                      QString(getenv("DATA_7CS"))+"/Sample/S-63/Permits", "S-63 Permit File (*.PMT *.TXT)");

    if (names.size() > 0)
    {
        impPath = QFileInfo(names[0]).absolutePath();
        QStringListIterator iT(names);
        QApplication::setOverrideCursor(Qt::WaitCursor);
        while (iT.hasNext())
        {
            if(ecchart->ImportS63Permits(iT.next()))
                QMessageBox::information(this, "S-63 Import", "Permits sucessfully read");
            else
                QMessageBox::critical(this, "S-63 Import", "Reading permits failed");
        }
        QApplication::restoreOverrideCursor();
    }
    DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onImportS63()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Import S-63"),
                                                    QString(getenv("DATA_7CS"))+"/Sample/S-63/V01X01/ENC_ROOT",
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty())
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        impPath = dir;
        // Import an encrypted S-63 Exchange Set
        if(ecchart->ImportS63ExchangeSet(dir))
        {
            QMessageBox::information(this, "S-63 Import", "S-63 Exhange Set sucessfully imported");
            // Apply any S-57 update files if exist
            ecchart->ApplyUpdate();
            QApplication::restoreOverrideCursor();
            DrawChart();
        }
        else
        {
            QApplication::restoreOverrideCursor();
        }
    }
}

/*---------------------------------------------------------------------------*/

void MainWindow::onZoomIn()
{
  ecchart->SetScale(ecchart->GetScale()/2);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onZoomOut()
{
  ecchart->SetScale(ecchart->GetScale()*2);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onLeft()
{
  EcCoordinate lat, lon;
  ecchart->XyToLatLon(0, ecchart->height()/2, lat, lon);
  ecchart->SetCenter(lat, lon);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onRight()
{
  EcCoordinate lat, lon;
  ecchart->XyToLatLon(ecchart->width(), ecchart->height()/2, lat, lon);
  ecchart->SetCenter(lat, lon);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onUp()
{
  EcCoordinate lat, lon;
  ecchart->XyToLatLon(ecchart->width()/2, 0, lat, lon);
  ecchart->SetCenter(lat, lon);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onDown()
{
  EcCoordinate lat, lon;
  ecchart->XyToLatLon(ecchart->width()/2, ecchart->height(), lat, lon);
  ecchart->SetCenter(lat, lon);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onRotateCW()
{
  double hdg = ecchart->GetHeading();
  hdg += 10;
  if (hdg >= 360) hdg -= 360.0;
  ecchart->SetHeading(hdg);
  oriEdit->setText(QString("%1°").arg(hdg));

  DrawChart();
}

void MainWindow::oriEditSetText(const double &txt){
    oriEdit->setText(QString("%1°").arg(txt));
}

/*---------------------------------------------------------------------------*/

void MainWindow::onRotateCCW()
{
  double hdg = ecchart->GetHeading();
  hdg -= 10;
  if (hdg < 0) hdg += 360.0;
  ecchart->SetHeading(hdg);
  oriEdit->setText(QString("%1°").arg(hdg));

  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onProjection(QAction *a)
{
  EcWidget::ProjectionMode pr = EcWidget::MercatorProjection;
  if (a == autoProjectionAction)
    pr = EcWidget::AutoProjection;
  else if (a == mercatorAction)
    pr = EcWidget::MercatorProjection;
  else if (a == gnomonicAction)
    pr = EcWidget::GnomonicProjection;
  else if (a == stereographicAction)
    pr = EcWidget::StereographicProjection;

  ecchart->SetProjection(pr);

  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onDisplayCategory(QAction *a)
{
  int dc = EC_STANDARD;
  if (a == baseAction)
    dc = EC_DISPLAYBASE;
  else if (a == standardAction)
    dc = EC_STANDARD;
  else if (a == otherAction)
    dc = EC_OTHER;

  ecchart->SetDisplayCategory(dc);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onLookup(QAction *a)
{
  int lut = EC_LOOKUP_SIMPLIFIED;
  if (a == fullChartAction)
    lut = EC_LOOKUP_TRADITIONAL;

  ecchart->SetLookupTable(lut); 
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onLights(bool on)
{
  ecchart->ShowLights(on);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onText(bool on)
{
  ecchart->ShowText(on);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onSoundings(bool on)
{
  ecchart->ShowSoundings(on);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onGrid(bool on)
{
  ecchart->ShowGrid(on);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onAIS()
{
  // Simple toggle action for AIS
  ecchart->ShowAIS(true);
  DrawChart();
}

void MainWindow::onAIS(bool on)
{
  ecchart->ShowAIS(on);
  DrawChart();
}

void MainWindow::onTrack(bool on)
{
    ecchart->TrackShip(on);
    DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onSearch()
{
    searchWindow->show();
    // SearchWindow dialog(tr("Enter Search Details"), this);

    if (searchWindow->exec() == QDialog::Accepted) {
        qDebug() << "Search Ok";
        QList<EcFeature> pickedFeatureList;

        ecchart->GetPickedFeatures(pickedFeatureList);
        qDebug() << searchWindow->getCheckedRadioButtonValue();
        // ecchart->SetCenter(searchWindow.latitude(), searchWindow.longitude());

        pickWindow->fill(pickedFeatureList);
        pickWindow->show();
    }
}

/*---------------------------------------------------------------------------*/

void MainWindow::onColorScheme(QAction *a)
{
  int cs = EC_DAY_BRIGHT;
  if (a == duskAction)
    cs = EC_DUSK;
  if (a == nightAction)
    cs = EC_NIGHT;

  bool gm = ecchart->GetGreyMode();
  int  br = ecchart->GetBrightness();
  ecchart->SetColorScheme(cs, gm, br);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onGreyMode(bool on)
{
  int cs = ecchart->GetColorScheme();
  int br = ecchart->GetBrightness();
  ecchart->SetColorScheme(cs, on, br);
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onScale(int sc)
{
  int range = (int)ecchart->GetRange(sc);
  sclEdit->setText(QString("1:%1").arg(sc));
  rngEdit->setText(QString("%1").arg(range));
}

/*---------------------------------------------------------------------------*/

void MainWindow::onProjection()
{    
  QString str = ecchart->GetProjectionName();
  proEdit->setText(str);
}

/*---------------------------------------------------------------------------*/

void MainWindow::onMouseMove(EcCoordinate lat, EcCoordinate lon)
{
  char buf[64];
  EcOutPositionToString(buf, lat, lon, 2);
  posEdit->setText(buf);
}

/*---------------------------------------------------------------------------*/

void MainWindow::onMouseRightClick(const QPoint& pos)
{
    // KLIK TARGET AIS
    EcAISTargetInfo* target = ecchart->findAISTargetInfoAtPosition(pos);

    // GET FEATURE
	QList<EcFeature> pickedFeatureList;

	ecchart->GetPickedFeatures(pickedFeatureList);

    pickWindow->fill(pickedFeatureList);

    //pickWindow->fillJson(pickedFeatureList);

    if (!aisTemp->toPlainText().trimmed().isEmpty()){
        aisText->setHtml(aisTemp->toHtml());
    }
    else {
        aisText->setHtml("");
    }

    if (!ownShipTemp->toPlainText().trimmed().isEmpty()){
        ownShipText->setHtml(ownShipTemp->toHtml());
    }

    if (target) {
        QString mmsiStr = QString::number(target->mmsi);
        AISTargetData ais;

        ais.mmsi = mmsiStr;
        ais.lat = ((double)target->latitude / 10000) / 60;
        ais.lon = ((double)target->longitude / 10000) / 60;

        ecchart->setAISTrack(ais);
        ecchart->TrackTarget(mmsiStr);

        qDebug() << "Track Target MMSI:" << mmsiStr;
    }
    else {
        AISTargetData ais;

        ais.mmsi = "";
        ais.lat = 0;
        ais.lon = 0;

        ecchart->setAISTrack(ais);
        ecchart->TrackTarget("");

        qDebug() << "Track Ownship";
    }


    pickWindow->show();

    //changeText();
}

// void MainWindow::onMouseRightClick()
// {
//     MOOSShip moosShip;

//     QList<EcFeature> pickedFeatureList;

//     moosShip = serverThreadMOOSSubscribeMapInfo();

//     qDebug() << moosShip.lat;
//     qDebug() << moosShip.lon;

//     ecchart->GetPickedFeaturesSubs(pickedFeatureList, moosShip.lat, moosShip.lon);

//     startServerMOOSPublishMapInfo(pickWindow->fillJson(pickedFeatureList));

//     pickWindow->show();
// }

/*---------------------------------------------------------------------------*/

void MainWindow::DrawChart()
{
  QApplication::setOverrideCursor(Qt::WaitCursor);
  ecchart->Draw();
  QApplication::restoreOverrideCursor();
}

/*---------------------------------------------------------------------------*/

void MainWindow::closeEvent(QCloseEvent *e)
{
  if (ecchart)
  {
    delete ecchart;
    ecchart = NULL;
  }
  e->accept();
}

/*---------------------------------------------------------------------------*/

////////////////////////////////////////////////////   AIS   /////////////////////////////////////////////////////
void MainWindow::slotLoadAisFile()
{    
    // Read AIS logfile.
    ////////////////////
    QString strLogFile = QFileDialog::getOpenFileName( this, tr( "Open File" ), QString(getenv("DATA_7CS"))+"/Sample/AIS", tr( "AIS logfiles (*.dat *.log *.txt)" ) );
    QFileInfo fi( strLogFile );
    if( fi.exists() == false )
    {
        //QMessageBox::warning( this, tr( "File not found" ), tr( "AIS logfile doesn't exists." ) );
        return;
    }

    ecchart->ReadAISLogfile( strLogFile );
}

void MainWindow::runAis()
{
    // Read AIS logfile.
    ////////////////////
    QString strLogFile = SettingsManager::instance().data().aisLogFile;
    QFileInfo fi( strLogFile );
    if( fi.exists() == false )
    {
        QMessageBox::warning( this, tr( "File not found" ), tr( "AIS logfile doesn't exists." ) );
        return;
    }

    ecchart->ReadAISLogfile( strLogFile );
    //ecchart->ReadAISLogfileWDelay(strLogFile);

    ecchart->Draw();
}

void MainWindow::slotLoadAisVariable()
{
    // Read AIS logfile.
    ////////////////////
    QStringList nmeaData;
    // nmeaData << "$GPGGA,074026.569,0712.233,S,11248.823,E,1,12,1.0,0.0,M,0.0,M,,*7F"
    //          << "$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30"
    //          << "$GPRMC,074026.569,A,0712.233,S,11248.823,E,2327.7,288.4,210325,000.0,W*59"
    //          << "$GPGGA,074027.569,0712.029,S,11248.206,E,1,12,1.0,0.0,M,0.0,M,,*7A"
    //          << "$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30"
    //          << "$GPRMC,074027.569,A,0712.029,S,11248.206,E,1333.0,276.3,210325,000.0,W*5B"
    //          << "$GPGGA,074028.569,0711.988,S,11247.835,E,1,12,1.0,0.0,M,0.0,M,,*71"
    //          << "$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30";
    //nmeaData << aivdo;

    nmeaData << "!AIVDM,1,1,,A,4050KlAvUl1ss`43P5spKjo004;d,0*1B";
    nmeaData << "!AIVDM,1,1,,A,4050KlAvUl1ss`43P5spKjo004;d,0*1B";
    nmeaData << "!AIVDM,1,1,,A,37ldh0gP@:840mQspDrF<600010i,0*1E";
    nmeaData << "!AIVDM,1,1,,A,37ldh0gP@:840mQspDrF<600010i,0*1E";
    nmeaData << "!AIVDM,1,1,,B,17lcMBwP00`4319spL?=hgwn20SE,0*24";
    nmeaData << "!AIVDM,1,1,,B,17lcMBwP00`4319spL?=hgwn20SE,0*24";
    nmeaData << "!AIVDM,1,1,,A,17lcMBwP00`4319spL>uhgv224@0,0*51";
    nmeaData << "!AIVDM,1,1,,A,17lcMBwP00`4319spL>uhgv224@0,0*51";
    nmeaData << "!AIVDM,1,1,,A,17lcM@0P00846F5spGCcdwv60<2D,0*46";
    nmeaData << "!AIVDM,1,1,,A,17lcM@0P00846F5spGCcdwv60<2D,0*46";
    nmeaData << "!AIVDM,1,1,,B,37ldh0gP@:840mespDq5iEd60000,0*75";
    nmeaData << "!AIVDM,1,1,,B,37ldh0gP@:840mespDq5iEd60000,0*75";
    nmeaData << "!AIVDM,1,1,,B,B7lde2000:113Ovv3HMDKwQ73P06,0*34";
    nmeaData << "!AIVDM,1,1,,B,B7lde2000:113Ovv3HMDKwQ73P06,0*34";
    nmeaData << "!AIVDM,1,1,,B,17lcMBwP00`4319spL>ehgv62@2W,0*27";
    nmeaData << "!AIVDM,1,1,,B,17lcMBwP00`4319spL>ehgv62@2W,0*27";
    nmeaData << "!AIVDM,1,1,,B,17isD0@P00`44wAspFh:Twv:2D49,0*08";
    nmeaData << "!AIVDM,1,1,,B,17isD0@P00`44wAspFh:Twv:2D49,0*08";
    nmeaData << "!AIVDM,1,1,,B,17lciMPP00`41PWsp86W?wv:24@0,0*42";
    nmeaData << "!AIVDM,1,1,,B,17lciMPP00`41PWsp86W?wv:24@0,0*42";
    nmeaData << "!AIVDM,1,1,,A,37ldh0gP@9840mwspDomI5>>011P,0*43";
    nmeaData << "!AIVDM,1,1,,A,37ldh0gP@9840mwspDomI5>>011P,0*43";
    nmeaData << "!AIVDM,1,1,,A,17lcMBwP00`4319spL>Mhgv820S?,0*7B";
    nmeaData << "!AIVDM,1,1,,A,17lcMBwP00`4319spL>Mhgv820S?,0*7B";
    nmeaData << "!AIVDM,1,1,,B,17lunPh000`42jAspH7N4;2>2<2B,0*5E";
    nmeaData << "!AIVDM,1,1,,B,17lunPh000`42jAspH7N4;2>2<2B,0*5E";
    nmeaData << "!AIVDM,1,1,,A,17lulhH000842IQspGNio`B>085;,0*75";
    nmeaData << "!AIVDM,1,1,,A,17lulhH000842IQspGNio`B>085;,0*75";
    nmeaData << "!AIVDM,1,1,,B,17lcMBwP00`4317spL=uhgv@24@0,0*2D";
    nmeaData << "!AIVDM,1,1,,B,17lcMBwP00`4317spL=uhgv@24@0,0*2D";


    ecchart->ReadAISVariable( nmeaData );

    //ecchart->StartReadAISSubscribeSSH();
}


void MainWindow::slotStopLoadAisVariable()
{
    qDebug() << "Stop Selected";
    ecchart->StopReadAISVariable();
}

void MainWindow::slotConnectToAisServer()
{
    // Connect to AIS Server via tcp
    ecchart->ReadFromServer( QString( "192.168.50.230:5500" ) );
}
///////////////////////////////////////////////////////////////   END AIS   ///////////////////////////////////////////////////

////////////////////////////////////////////////////   MOOSDB   /////////////////////////////////////////////////////

void MainWindow::subscribeMOOSDB()
{
    // NEW
    ecchart->startAISSubscribe();
    qDebug() << "MOOSDB IP: " + SettingsManager::instance().data().moosIp;
}

void MainWindow::stopSubscribeMOOSDB()
{
    ecchart->stopAISConnection();
}

////////////////////////////////////////////////////   END MOOSDB   /////////////////////////////////////////////////////

//////////////////////////////////////////////////////    UNUSED - FOR LOGIC BACKUP ONLY     //////////////////////////////////////////////////////


// Waypoint menu handlers
void MainWindow::onCreateWaypoint()
{
    setWindowTitle("ShowRoute     Creating waypoint ...");

    if (ecchart) {
        ecchart->setActiveFunction(EcWidget::CREATE_WAYP);
    }

    statusBar()->showMessage("Click on the chart to create a waypoint");
}

void MainWindow::onRemoveWaypoint()
{
    qDebug() << "Remove Waypoint clicked";

    setWindowTitle("ECDIS AUV - Remove Waypoint Mode");
    statusBar()->showMessage("Click a waypoint to remove");

    if (ecchart) {
        ecchart->setActiveFunction(EcWidget::REMOVE_WAYP);
    }
}

void MainWindow::onMoveWaypoint()
{
    qDebug() << "Move Waypoint clicked";

    setWindowTitle("ECDIS AUV - Move Waypoint Mode");
    statusBar()->showMessage("Click a waypoint to move");

    if (ecchart) {
        ecchart->setActiveFunction(EcWidget::MOVE_WAYP);
    }
}


void MainWindow::onEditWaypoint()
{
    qDebug() << "Edit Waypoint clicked";

    setWindowTitle("ECDIS AUV - Edit Waypoint Mode");
    statusBar()->showMessage("Click a waypoint to edit");

    if (ecchart) {
        ecchart->setActiveFunction(EcWidget::EDIT_WAYP);
    }
}


void MainWindow::onWaypointCreated()
{
    setWindowTitle("ECDIS AUV");
    statusBar()->showMessage("Ready");
}

void MainWindow::onClearWaypoints()
{
    if (ecchart)
        ecchart->clearWaypoints();
}

void MainWindow::onExportWaypoints()
{
    if (!ecchart || ecchart->getWaypointCount() == 0)
    {
        QMessageBox::warning(this, tr("Export Waypoints"), tr("No waypoints to export."));
        return;
    }

    QString filename = QFileDialog::getSaveFileName(this, tr("Export Waypoints"),
                                                    QDir::homePath(), tr("JSON Files (*.json);;All Files (*)"));
    if (!filename.isEmpty())
    {
        if (!filename.endsWith(".json", Qt::CaseInsensitive))
            filename += ".json";

        ecchart->exportWaypointsToFile(filename);
    }
}

void MainWindow::onImportWaypoints()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Import Waypoints"),
                                                    QDir::homePath(), tr("JSON Files (*.json);;All Files (*)"));
    if (!filename.isEmpty())
    {
        ecchart->importWaypointsFromFile(filename);
    }
}

// GuardZone

void MainWindow::onEnableGuardZone(bool enabled)
{
    if (ecchart) {
        ecchart->enableGuardZone(enabled);
        ecchart->update(); // Tambahkan ini untuk memastikan perubahan terlihat
    }
}

void MainWindow::onCreateCircularGuardZone()
{
    if (ecchart) {
        ecchart->startCreateGuardZone(GUARD_ZONE_CIRCLE);
        statusBar()->showMessage("Click to set center of circular guard zone, then click again to set radius");
    }
}

void MainWindow::onCreatePolygonGuardZone()
{
    if (ecchart) {
        ecchart->startCreateGuardZone(GUARD_ZONE_POLYGON);
        statusBar()->showMessage("Click to add points to guard zone. Right-click when finished.");
    }
}

void MainWindow::onCheckGuardZone()
{
    if (!ecchart) {
        qDebug() << "[CHECK-GZ] EcChart not available";
        return;
    }

    qDebug() << "[CHECK-GZ] Manual GuardZone check initiated";

    // Debug AIS targets terlebih dahulu
    ecchart->debugAISTargets();

    // Jalankan check guardzone
    bool threatsDetected = ecchart->checkTargetsInGuardZone();

    if (threatsDetected) {
        statusBar()->showMessage("Threats detected in GuardZone!", 5000);
        qDebug() << "[CHECK-GZ] Threats detected";
    } else {
        statusBar()->showMessage("No threats detected in GuardZone", 3000);
        qDebug() << "[CHECK-GZ] No threats detected";
    }
}

void MainWindow::onAttachGuardZoneToShip(bool attached)
{
    qDebug() << "=== ATTACH TO SHIP CALLED ===" << attached;
    qDebug() << "[ATTACH-DEBUG] Function started successfully";

    // CRITICAL: Add comprehensive protection but restore full functionality
    try {
        if (!ecchart) {
            qDebug() << "[ATTACH-ERROR] ecchart is null!";
            statusBar()->showMessage(tr("Chart not initialized"), 3000);
            return;
        }
        
        qDebug() << "[ATTACH-DEBUG] ecchart is valid";
        
        // Debug current state with protection
        bool currentHasAttached = false;
        try {
            currentHasAttached = ecchart->hasAttachedGuardZone();
            qDebug() << "[ATTACH-DEBUG] hasAttachedGuardZone returned:" << currentHasAttached;
        } catch (...) {
            qDebug() << "[ATTACH-ERROR] Exception in hasAttachedGuardZone()";
            currentHasAttached = false; // Assume no attached guardzone
        }
        
        // PERBAIKAN: Jika user mengaktifkan attach tapi sudah ada guardzone attached
        if (attached && currentHasAttached) {
            qDebug() << "[DEBUG] Already has attached guardzone, skipping creation";
            statusBar()->showMessage(tr("Guardzone already attached to ship"), 3000);
            return;
        }
        
        // PERBAIKAN: Jika user menonaktifkan attach tapi tidak ada guardzone attached
        if (!attached && !currentHasAttached) {
            qDebug() << "[DEBUG] No attached guardzone found, nothing to detach";
            statusBar()->showMessage(tr("No guardzone to detach"), 3000);
            return;
        }
        
        qDebug() << "[ATTACH-DEBUG] About to call setRedDotAttachedToShip...";
        
        // CRITICAL: Protect this call but restore functionality
        try {
            ecchart->setRedDotAttachedToShip(attached);
            qDebug() << "[ATTACH-DEBUG] setRedDotAttachedToShip completed successfully";
        } catch (const std::exception& e) {
            qDebug() << "[ATTACH-ERROR] Exception in setRedDotAttachedToShip:" << e.what();
            statusBar()->showMessage(tr("Error creating attached guardzone"), 3000);
            return;
        } catch (...) {
            qDebug() << "[ATTACH-ERROR] Unknown exception in setRedDotAttachedToShip";
            statusBar()->showMessage(tr("Unknown error in attach function"), 3000);
            return;
        }

        if (attached) {
            statusBar()->showMessage(tr("Red dot tracker attached to ship"), 3000);
            qDebug() << "[ATTACH-DEBUG] Attached guardzone created successfully";
        } else {
            statusBar()->showMessage(tr("Red dot tracker detached from ship"), 3000);
            qDebug() << "[ATTACH-DEBUG] Attached guardzone removed successfully";
        }
        
        qDebug() << "[ATTACH-DEBUG] Function completed successfully";
        
    } catch (const std::exception& e) {
        qDebug() << "[ATTACH-ERROR] Exception in onAttachGuardZoneToShip:" << e.what();
        statusBar()->showMessage(tr("Critical error in attach function"), 5000);
    } catch (...) {
        qDebug() << "[ATTACH-ERROR] Unknown exception in onAttachGuardZoneToShip";
        statusBar()->showMessage(tr("Unknown critical error"), 5000);
    }
}

void MainWindow::onAttachToShipStateChanged(bool attached)
{
    qDebug() << "[UI-SYNC] onAttachToShipStateChanged called with:" << attached;
    
    // Update checkbox untuk sinkronisasi dengan backend state
    if (attachToShipAction) {
        attachToShipAction->blockSignals(true); // Block signal untuk prevent infinite loop
        attachToShipAction->setChecked(attached);
        attachToShipAction->blockSignals(false);
        qDebug() << "[UI-SYNC] Attach to ship checkbox updated to:" << attached;
    } else {
        qDebug() << "[UI-SYNC] WARNING: attachToShipAction is null!";
    }
}

// AIS Simulation
void MainWindow::onStartSimulation()
{
    if (ecchart) {
        ecchart->startAISTargetSimulation();
    }
}

void MainWindow::onStopSimulation()
{
    if (ecchart) {
        ecchart->stopAISTargetSimulation();
    }
}

void MainWindow::onAutoCheckGuardZone(bool checked)
{
    if (ecchart) {
        ecchart->setAutoCheckGuardZone(checked);
    }
}

// ========== SETUP GUARDZONE PANEL ==========
void MainWindow::setupGuardZonePanel()
{
    if (!ecchart) {
        qDebug() << "Cannot setup GuardZone panel: ecchart is null";
        return;
    }

    try {
        // Create GuardZone panel
        guardZonePanel = new GuardZonePanel(ecchart, ecchart->getGuardZoneManager(), this);

        // Create dock widget
        guardZoneDock = new QDockWidget(tr("GuardZone Manager"), this);
        guardZoneDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        guardZoneDock->setWidget(guardZonePanel);

        // Add to right dock area
        addDockWidget(Qt::RightDockWidgetArea, guardZoneDock);

        // Add to view menu
        QList<QAction*> actions = menuBar()->actions();
        for (QAction* action : actions) {
            if (action->menu() && action->menu()->title() == tr("&Sidebar")) {
                action->menu()->addAction(guardZoneDock->toggleViewAction());
                break;
            }
        }

        // Signal connections dengan error handling
        connect(ecchart, &EcWidget::guardZoneCreated,
                guardZonePanel, &GuardZonePanel::onGuardZoneCreated,
                Qt::QueuedConnection);

        connect(ecchart, &EcWidget::guardZoneDeleted,
                guardZonePanel, &GuardZonePanel::onGuardZoneDeleted,
                Qt::QueuedConnection);

        connect(ecchart, &EcWidget::guardZoneModified,
                guardZonePanel, &GuardZonePanel::onGuardZoneModified,
                Qt::QueuedConnection);

        connect(guardZonePanel, &GuardZonePanel::guardZoneSelected,
                this, &MainWindow::onGuardZoneSelected,
                Qt::QueuedConnection);

        connect(guardZonePanel, &GuardZonePanel::guardZoneEditRequested,
                this, &MainWindow::onGuardZoneEditRequested,
                Qt::QueuedConnection);

        connect(guardZonePanel, &GuardZonePanel::guardZoneVisibilityChanged,
                this, &MainWindow::onGuardZoneVisibilityChanged,
                Qt::QueuedConnection);

        // TAMBAHAN: Connect new signals
        connect(guardZonePanel, &GuardZonePanel::validationCompleted,
                [this](bool success) {
                    if (!success) {
                        statusBar()->showMessage(tr("GuardZone panel validation found issues"), 3000);
                    }
                });

        // Delayed initial refresh
        QTimer::singleShot(100, [this]() {
            try {
                if (guardZonePanel) {
                    guardZonePanel->refreshGuardZoneList();
                    qDebug() << "GuardZone panel initial refresh completed successfully";

                    // Validate after refresh - sekarang menggunakan public method
                    QTimer::singleShot(200, [this]() {
                        if (guardZonePanel) {
                            guardZonePanel->validatePanelState();  // Sekarang public
                        }
                    });
                }
            } catch (const std::exception& e) {
                qDebug() << "Error during panel initialization:" << e.what();
                if (guardZonePanel) {
                    guardZonePanel->recoverFromError();  // Sekarang public
                }
            }
        });

        qDebug() << "GuardZone panel setup completed successfully";

    } catch (const std::exception& e) {
        qDebug() << "Critical error setting up GuardZone panel:" << e.what();
        QMessageBox::critical(this, tr("Setup Error"),
                              tr("Failed to setup GuardZone panel: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "Unknown critical error setting up GuardZone panel";
        QMessageBox::critical(this, tr("Setup Error"),
                              tr("Unknown error occurred while setting up GuardZone panel"));
    }
}

// ========== SETUP AIS TARGET PANEL ==========
void MainWindow::setupAISTargetPanel()
{
    if (!ecchart) {
        qDebug() << "Cannot setup AIS Target panel: ecchart is null";
        return;
    }

    try {
        // Create AIS Target panel
        aisTargetPanel = new AISTargetPanel(ecchart, ecchart->getGuardZoneManager(), this);

        // Create dock widget
        aisTargetDock = new QDockWidget(tr("AIS Target Manager"), this);
        aisTargetDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
        aisTargetDock->setWidget(aisTargetPanel);

        // Add to right dock area
        addDockWidget(Qt::RightDockWidgetArea, aisTargetDock);

        // Add to view menu
        QList<QAction*> actions = menuBar()->actions();
        bool sidebarFound = false;
        for (QAction* action : actions) {
            if (action->menu() && action->menu()->title() == tr("&Sidebar")) {
                action->menu()->addAction(aisTargetDock->toggleViewAction());
                qDebug() << "AIS Target Panel added to Sidebar menu successfully";
                sidebarFound = true;
                break;
            }
        }
        if (!sidebarFound) {
            qDebug() << "[MAIN] Warning: Sidebar menu not found for AIS Target Panel";
        }

        // Signal connections
        if (ecchart->getGuardZoneManager()) {
            connect(ecchart->getGuardZoneManager(), &GuardZoneManager::guardZoneAlert,
                    aisTargetPanel, &AISTargetPanel::onGuardZoneAlert,
                    Qt::QueuedConnection);
        }
        
        // Connect signal dari EcWidget auto-check system
        connect(ecchart, &EcWidget::aisTargetDetected,
                aisTargetPanel, &AISTargetPanel::onGuardZoneAlert,
                Qt::QueuedConnection);


        // Connect from EcWidget auto-check signals if available
        connect(ecchart, &EcWidget::guardZoneTargetDetected,
                [this](int guardZoneId, int targetCount) {
                    if (aisTargetPanel) {
                        aisTargetPanel->refreshTargetList();
                    }
                });

        // Delayed initial refresh
        QTimer::singleShot(500, [this]() {
            if (aisTargetPanel) {
                aisTargetPanel->refreshTargetList();
                qDebug() << "AIS Target panel initial refresh completed";
            }
        });

        // ========== TABIFY WITH GUARDZONE AND CPA/TCPA PANELS ==========
        if (guardZoneDock && aisTargetDock) {
            // Create tabbed interface - GuardZone Manager as base, AIS Target Manager as tab
            tabifyDockWidget(guardZoneDock, aisTargetDock);
            
            // Add CPA/TCPA panel to the tabbed interface if available
            if (m_cpatcpaDock) {
                tabifyDockWidget(guardZoneDock, m_cpatcpaDock);
                qDebug() << "Added CPA/TCPA Manager to tabbed interface";
            }
            
            // Set GuardZone Manager as the default active tab
            guardZoneDock->raise();
            
            qDebug() << "Created tabbed dock interface: GuardZone Manager + AIS Target Manager + CPA/TCPA Manager";
        }
        // =================================================

        qDebug() << "AIS Target panel setup completed successfully";

    } catch (const std::exception& e) {
        qDebug() << "Critical error setting up AIS Target panel:" << e.what();
        QMessageBox::critical(this, tr("Setup Error"),
                              tr("Failed to setup AIS Target panel: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "Unknown critical error setting up AIS Target panel";
        QMessageBox::critical(this, tr("Setup Error"),
                              tr("Unknown error occurred while setting up AIS Target panel"));
    }
}

void MainWindow::setupObstacleDetectionPanel()
{
    qDebug() << "Setting up Obstacle Detection panel...";
    
    if (!ecchart) {
        qDebug() << "Cannot setup Obstacle Detection panel: ecchart not available";
        return;
    }

    try {
        // Create Obstacle Detection panel
        obstacleDetectionPanel = new ObstacleDetectionPanel(ecchart, ecchart->getGuardZoneManager(), this);

        // Create dock widget
        obstacleDetectionDock = new QDockWidget(tr("Obstacle Detection"), this);
        obstacleDetectionDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
        obstacleDetectionDock->setWidget(obstacleDetectionPanel);

        // Add to right dock area
        addDockWidget(Qt::RightDockWidgetArea, obstacleDetectionDock);

        // Add to Sidebar menu
        QList<QAction*> actions = menuBar()->actions();
        bool sidebarFound = false;
        for (QAction* action : actions) {
            if (action->menu() && action->menu()->title() == tr("&Sidebar")) {
                action->menu()->addAction(obstacleDetectionDock->toggleViewAction());
                qDebug() << "Obstacle Detection Panel added to Sidebar menu successfully";
                sidebarFound = true;
                break;
            }
        }
        if (!sidebarFound) {
            qDebug() << "[MAIN] Warning: Sidebar menu not found for Obstacle Detection Panel";
        }

        // Connect pick report obstacle detection signal
        bool connectionResult = connect(ecchart, &EcWidget::pickReportObstacleDetected,
                obstacleDetectionPanel, &ObstacleDetectionPanel::onPickReportObstacleDetected,
                Qt::QueuedConnection);
        qDebug() << "[OBSTACLE-MAIN-DEBUG] Signal connection result:" << connectionResult;
        
        // Connect dangerous obstacle alarm signals (synchronized with flashing)
        connect(ecchart, &EcWidget::dangerousObstacleDetected,
                obstacleDetectionPanel, &ObstacleDetectionPanel::startDangerousAlarm,
                Qt::QueuedConnection);
        connect(ecchart, &EcWidget::dangerousObstacleCleared,
                obstacleDetectionPanel, &ObstacleDetectionPanel::stopDangerousAlarm,
                Qt::QueuedConnection);
        qDebug() << "[ALARM-SYNC] Connected dangerous obstacle alarm signals with flashing";

        // ========== TABIFY WITH OTHER PANELS ==========
        if (aisTargetDock && obstacleDetectionDock) {
            // Add Obstacle Detection Panel to the tabbed interface
            tabifyDockWidget(aisTargetDock, obstacleDetectionDock);
            qDebug() << "Added Obstacle Detection Panel to tabbed interface";
        }
        // =================================================

        qDebug() << "Obstacle Detection panel setup completed successfully";

    } catch (const std::exception& e) {
        qDebug() << "Critical error setting up Obstacle Detection panel:" << e.what();
        QMessageBox::critical(this, tr("Setup Error"),
                              tr("Failed to setup Obstacle Detection panel: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "Unknown critical error setting up Obstacle Detection panel";
        QMessageBox::critical(this, tr("Setup Error"),
                              tr("Unknown error occurred while setting up Obstacle Detection panel"));
    }
}

// ========== GUARDZONE PANEL HANDLERS ==========

void MainWindow::onGuardZoneSelected(int guardZoneId)
{
    qDebug() << "GuardZone selected from panel:" << guardZoneId;

    // Center map on selected guardzone
    if (ecchart) {
        QList<GuardZone>& guardZones = ecchart->getGuardZones();

        for (const GuardZone& gz : guardZones) {
            if (gz.id == guardZoneId) {
                if (gz.shape == GUARD_ZONE_CIRCLE) {
                    ecchart->SetCenter(gz.centerLat, gz.centerLon);
                } else if (gz.shape == GUARD_ZONE_POLYGON && gz.latLons.size() >= 6) {
                    // Calculate center
                    double centerLat = 0, centerLon = 0;
                    int numPoints = gz.latLons.size() / 2;

                    for (int i = 0; i < gz.latLons.size(); i += 2) {
                        centerLat += gz.latLons[i];
                        centerLon += gz.latLons[i + 1];
                    }

                    centerLat /= numPoints;
                    centerLon /= numPoints;

                    ecchart->SetCenter(centerLat, centerLon);
                }

                ecchart->Draw();
                break;
            }
        }
    }
}

void MainWindow::onGuardZoneEditRequested(int guardZoneId)
{
    qDebug() << "Edit requested for GuardZone:" << guardZoneId;

    if (ecchart) {
        ecchart->startEditGuardZone(guardZoneId);
    }
}

void MainWindow::onGuardZoneVisibilityChanged(int guardZoneId, bool visible)
{
    qDebug() << "GuardZone" << guardZoneId << "visibility changed to:" << visible;

    // Update status bar or show notification
    QString status = visible ? tr("enabled") : tr("disabled");
    statusBar()->showMessage(tr("GuardZone %1 %2").arg(guardZoneId).arg(status), 2000);
}

void MainWindow::setupTestingMenu()
{
    // TAMBAHAN: Debug menu untuk testing
    QMenu *debugMenu = menuBar()->addMenu("&Debug");

    debugMenu->addAction("Validate GuardZone System", [this]() {
        if (ecchart) {
            bool isValid = ecchart->validateGuardZoneSystem();
            QString message = isValid ?
                                  tr("✓ GuardZone system validation passed") :
                                  tr("✗ GuardZone system validation failed - check debug output");

            QMessageBox::information(this, tr("System Validation"), message);
        }
    });

    debugMenu->addAction("Refresh GuardZone Panel", [this]() {
        if (guardZonePanel) {
            guardZonePanel->refreshGuardZoneList();
            statusBar()->showMessage(tr("GuardZone panel refreshed"), 2000);
        }
    });

    debugMenu->addAction("Force Save GuardZones", [this]() {
        if (ecchart) {
            ecchart->saveGuardZones();
            statusBar()->showMessage(tr("GuardZones saved"), 2000);
        }
    });

    debugMenu->addAction("Show System Statistics", [this]() {
        showSystemStatistics();
    });

    debugMenu->addSeparator();

    debugMenu->addAction("Create Test GuardZones", [this]() {
        createTestGuardZones();
    });

    debugMenu->addAction("Clear All GuardZones", [this]() {
        if (ecchart) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, tr("Clear All GuardZones"),
                tr("Are you sure you want to delete ALL GuardZones?\nThis action cannot be undone."),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                ecchart->getGuardZones().clear();
                ecchart->enableGuardZone(false);
                ecchart->saveGuardZones();
                ecchart->update();

                if (guardZonePanel) {
                    guardZonePanel->refreshGuardZoneList();
                }

                QMessageBox::information(this, tr("Cleared"), tr("All GuardZones have been deleted."));
            }
        }
    });

    // ========== ALERT SYSTEM TESTING ==========
    debugMenu->addSeparator();
    debugMenu->addAction("🔊 Test Low Priority Alert", [this]() {
        if (ecchart) {
            ecchart->triggerNavigationAlert("Test low priority navigation alert", 1);
        }
    });

    debugMenu->addAction("🟡 Test Medium Priority Alert", [this]() {
        if (ecchart) {
            ecchart->triggerNavigationAlert("Test medium priority navigation alert", 2);
        }
    });

    debugMenu->addAction("🟠 Test High Priority Alert", [this]() {
        if (ecchart) {
            ecchart->triggerNavigationAlert("Test high priority navigation alert", 3);
        }
    });

    debugMenu->addAction("🚨 Test Critical Alert", [this]() {
        if (ecchart) {
            ecchart->triggerNavigationAlert("CRITICAL: Test critical priority alert", 4);
        }
    });

    debugMenu->addSeparator();

    debugMenu->addAction("💧 Test Depth Alert", [this]() {
        if (ecchart) {
            // Simulate shallow water alert
            ecchart->triggerDepthAlert(3.5, 5.0, true); // current: 3.5m, threshold: 5.0m, shallow: true
        }
    });

    debugMenu->addAction("⚠️ Test GuardZone Alert", [this]() {
        if (ecchart) {
            ecchart->triggerGuardZoneAlert(1, "AIS target detected within GuardZone boundaries");
        }
    });

    debugMenu->addSeparator();

    debugMenu->addAction("📊 Show Alert Statistics", [this]() {
        if (alertPanel) {
            alertPanel->showAlertStatistics();
        } else {
            QMessageBox::information(this, "Alert Statistics", "Alert Panel not available");
        }
    });

    debugMenu->addAction("🔄 Refresh Alert Panel", [this]() {
        if (alertPanel) {
            alertPanel->refreshAlertList();
            statusBar()->showMessage("Alert panel refreshed", 2000);
        }
    });

    debugMenu->addAction("✅ Test Alert Acknowledge/Resolve", [this]() {
        testAlertWorkflow();
    });

    // ==========================================

    debugMenu->addAction("🔄 Force Refresh Alert Panel", [this]() {
        if (alertPanel) {
            qDebug() << "[MAIN] Forcing alert panel refresh...";
            alertPanel->refreshAlertList();

            // Also check AlertSystem directly
            if (ecchart) {
                AlertSystem* alertSystem = ecchart->getAlertSystem();
                if (alertSystem) {
                    QList<AlertData> alerts = alertSystem->getActiveAlerts();
                    qDebug() << "[MAIN] AlertSystem has" << alerts.size() << "active alerts";
                    for (const AlertData& alert : alerts) {
                        qDebug() << "[MAIN] Alert:" << alert.id << alert.title;
                    }
                }
            }
        }
    });

    debugMenu->addAction("📝 Create Test Alert Directly", [this]() {
        if (alertPanel && ecchart) {
            AlertSystem* alertSystem = ecchart->getAlertSystem();
            if (alertSystem) {
                // Create alert directly using AlertSystem
                int alertId = alertSystem->triggerAlert(
                    ALERT_NAVIGATION_WARNING,
                    PRIORITY_HIGH,
                    "Direct Test Alert",
                    "This alert was created directly via AlertSystem",
                    "Direct_Test",
                    -7.18551, 112.78012
                    );

                qDebug() << "[MAIN] Created direct test alert with ID:" << alertId;

                // Force refresh panel
                QTimer::singleShot(100, [this]() {
                    if (alertPanel) {
                        alertPanel->refreshAlertList();
                    }
                });
            }
        }
    });

    debugMenu->addAction("➕ Manual Add Alert to Panel", [this]() {
        if (!alertPanel) {
            QMessageBox::warning(this, "Error", "Alert Panel not available");
            return;
        }

        try {
            qDebug() << "[MAIN] Creating manual alert...";

            // PERBAIKAN: Gunakan AlertSystem untuk create alert, bukan manual
            if (ecchart) {
                AlertSystem* alertSystem = ecchart->getAlertSystem();
                if (alertSystem) {
                    qDebug() << "[MAIN] Using AlertSystem to create alert...";

                    int alertId = alertSystem->triggerAlert(
                        ALERT_USER_DEFINED,        // Safe enum value
                        PRIORITY_MEDIUM,           // Safe enum value
                        "Manual Panel Test",       // Simple string
                        "Testing panel display",   // Simple string
                        "Manual_Test",             // Simple string
                        0.0, 0.0                   // Safe coordinates
                        );

                    qDebug() << "[MAIN] AlertSystem created alert ID:" << alertId;

                    // Force refresh after small delay
                    QTimer::singleShot(200, [this]() {
                        if (alertPanel) {
                            alertPanel->refreshAlertList();
                        }
                    });

                    if (alertId > 0) {
                        QMessageBox::information(this, "Success",
                                                 QString("Alert created with ID: %1").arg(alertId));
                    } else {
                        QMessageBox::warning(this, "Failed", "Failed to create alert");
                    }

                } else {
                    QMessageBox::warning(this, "Error", "AlertSystem not available");
                }
            } else {
                QMessageBox::warning(this, "Error", "EcWidget not available");
            }

        } catch (const std::exception& e) {
            qDebug() << "[MAIN] Exception in manual add alert:" << e.what();
            QMessageBox::critical(this, "Crash Prevented",
                                  QString("Error: %1").arg(e.what()));
        } catch (...) {
            qDebug() << "[MAIN] Unknown exception in manual add alert";
            QMessageBox::critical(this, "Crash Prevented", "Unknown error occurred");
        }
    });

    debugMenu->addAction("🔍 Debug Alert System State", [this]() {
        QString debug;

        if (ecchart) {
            AlertSystem* alertSystem = ecchart->getAlertSystem();
            if (alertSystem) {
                QList<AlertData> alerts = alertSystem->getActiveAlerts();
                debug += QString("AlertSystem active alerts: %1\n").arg(alerts.size());

                for (const AlertData& alert : alerts) {
                    debug += QString("- ID %1: %2\n").arg(alert.id).arg(alert.title);
                }
            } else {
                debug += "AlertSystem: NULL\n";
            }
        } else {
            debug += "EcWidget: NULL\n";
        }

        if (alertPanel) {
            debug += QString("AlertPanel active count: %1\n").arg(alertPanel->getActiveAlertCount());
            debug += QString("AlertPanel total count: %1\n").arg(alertPanel->getTotalAlertCount());
        } else {
            debug += "AlertPanel: NULL\n";
        }

        QMessageBox::information(this, "Debug State", debug);
    });

    debugMenu->addAction("🛡️ Safe Panel Test", [this]() {
        if (alertPanel) {
            // Test basic panel functionality
            alertPanel->updateAlertCounts();
            alertPanel->refreshAlertList();

            QString status = QString("Panel is working!\nActive count: %1\nTotal count: %2")
                                 .arg(alertPanel->getActiveAlertCount())
                                 .arg(alertPanel->getTotalAlertCount());

            QMessageBox::information(this, "Safe Test", status);
        }
    });
}

// 2. TAMBAHAN: System statistics display
void MainWindow::showSystemStatistics()
{
    if (!ecchart) return;

    QList<GuardZone>& guardZones = ecchart->getGuardZones();

    QString stats;
    stats += tr("=== ECDIS GuardZone System Statistics ===\n\n");

    // Basic counts
    int total = guardZones.size();
    int active = std::count_if(guardZones.begin(), guardZones.end(),
                               [](const GuardZone& gz) { return gz.active; });
    int inactive = total - active;
    int circles = std::count_if(guardZones.begin(), guardZones.end(),
                                [](const GuardZone& gz) { return gz.shape == GUARD_ZONE_CIRCLE; });
    int polygons = total - circles;
    int attachedToShip = std::count_if(guardZones.begin(), guardZones.end(),
                                       [](const GuardZone& gz) { return gz.attachedToShip; });

    stats += tr("Total GuardZones: %1\n").arg(total);
    stats += tr("Active: %1 | Inactive: %2\n").arg(active).arg(inactive);
    stats += tr("Circles: %1 | Polygons: %2\n").arg(circles).arg(polygons);
    stats += tr("Attached to Ship: %1\n\n").arg(attachedToShip);

    // System state
    stats += tr("System State:\n");
    stats += tr("- GuardZone System Active: %1\n").arg(ecchart->isGuardZoneActive() ? "Yes" : "No");
    stats += tr("- Next GuardZone ID: %1\n").arg(ecchart->getNextGuardZoneId());
    // PERBAIKAN: Gunakan public method
    stats += tr("- Panel Items: %1\n\n").arg(guardZonePanel ? guardZonePanel->getGuardZoneListCount() : 0);

    // Performance info
    stats += tr("Memory Usage:\n");
    stats += tr("- GuardZone Objects: %1 x ~200 bytes = ~%2 KB\n")
                 .arg(total).arg((total * 200) / 1024.0, 0, 'f', 1);

    // File info
    QString filePath = ecchart->getGuardZoneFilePath();
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists()) {
        stats += tr("- Save File: %1 (%2 KB)\n")
        .arg(fileInfo.fileName())
            .arg(fileInfo.size() / 1024.0, 0, 'f', 1);
        stats += tr("- Last Modified: %1\n").arg(fileInfo.lastModified().toString());
    } else {
        stats += tr("- Save File: Not found\n");
    }

    // Validation
    bool isValid = ecchart->validateGuardZoneSystem();
    stats += tr("\nSystem Validation: %1\n").arg(isValid ? "✓ PASSED" : "✗ FAILED");

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("GuardZone System Statistics"));
    msgBox.setText(stats);
    msgBox.setTextFormat(Qt::PlainText);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

// 3. TAMBAHAN: Create test guardzones untuk testing
void MainWindow::createTestGuardZones()
{
    if (!ecchart) return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Create Test GuardZones"),
        tr("This will create 3 test GuardZones for testing purposes.\nContinue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (reply != QMessageBox::Yes) return;

    try {
        // Test Circle 1 - Active
        GuardZone testCircle1;
        testCircle1.id = ecchart->getNextGuardZoneId();
        testCircle1.name = "Test Circle 1 (Active)";
        testCircle1.shape = GUARD_ZONE_CIRCLE;
        testCircle1.active = true;
        testCircle1.attachedToShip = false;
        testCircle1.color = Qt::red;
        testCircle1.centerLat = -7.18551;
        testCircle1.centerLon = 112.78012;
        testCircle1.radius = 1.0;

        // Test Circle 2 - Inactive
        GuardZone testCircle2;
        testCircle2.id = ecchart->getNextGuardZoneId();
        testCircle2.name = "Test Circle 2 (Inactive)";
        testCircle2.shape = GUARD_ZONE_CIRCLE;
        testCircle2.active = false;
        testCircle2.attachedToShip = false;
        testCircle2.color = Qt::blue;
        testCircle2.centerLat = -7.19551;
        testCircle2.centerLon = 112.79012;
        testCircle2.radius = 0.5;

        // Test Polygon - Active, Attached to Ship
        GuardZone testPolygon;
        testPolygon.id = ecchart->getNextGuardZoneId();
        testPolygon.name = "Test Polygon (Ship Attached)";
        testPolygon.shape = GUARD_ZONE_POLYGON;
        testPolygon.active = true;
        testPolygon.attachedToShip = true;
        testPolygon.color = Qt::green;
        testPolygon.latLons = {
            -7.17551, 112.77012,  // Point 1
            -7.17551, 112.78012,  // Point 2
            -7.18551, 112.78012,  // Point 3
            -7.18551, 112.77012   // Point 4
        };

        // Add to system
        QList<GuardZone>& guardZones = ecchart->getGuardZones();
        guardZones.append(testCircle1);
        guardZones.append(testCircle2);
        guardZones.append(testPolygon);

        // Enable system and save
        ecchart->enableGuardZone(true);
        ecchart->saveGuardZones();
        ecchart->update();

        // Refresh panel
        if (guardZonePanel) {
            guardZonePanel->refreshGuardZoneList();
        }

        QMessageBox::information(this, tr("Test GuardZones Created"),
                                 tr("Successfully created 3 test GuardZones:\n"
                                    "- Circle 1 (Active)\n"
                                    "- Circle 2 (Inactive)\n"
                                    "- Polygon (Active, Ship Attached)"));

    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to create test GuardZones: %1").arg(e.what()));
    } catch (...) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Unknown error occurred while creating test GuardZones"));
    }
}

void MainWindow::setupAlertPanel()
{
    qDebug() << "[MAIN] Starting setupAlertPanel()";

    if (!ecchart) {
        qDebug() << "[MAIN] Cannot setup Alert panel: ecchart is null";
        return;
    }

    // PERBAIKAN: Cek AlertSystem lebih detail
    AlertSystem* alertSystem = ecchart->getAlertSystem();
    qDebug() << "[MAIN] EcWidget AlertSystem pointer:" << alertSystem;

    if (!alertSystem) {
        qDebug() << "[MAIN] AlertSystem is null! Attempting to reinitialize...";

        // Try to reinitialize AlertSystem
        QTimer::singleShot(100, [this]() {
            if (ecchart) {
                ecchart->initializeAlertSystem();

                // Retry setup after reinitialize
                QTimer::singleShot(500, [this]() {
                    AlertSystem* retrySystem = ecchart->getAlertSystem();
                    if (retrySystem && alertPanel) {
                        qDebug() << "[MAIN] Reconnecting AlertSystem to panel...";

                        // Update panel's AlertSystem pointer
                        alertPanel->setAlertSystem(retrySystem);

                        // Manual signal connections
                        connect(retrySystem, &AlertSystem::alertTriggered,
                                this, [this](const AlertData& alert) {
                                    qDebug() << "[MAIN] Forwarding alert to panel:" << alert.title;
                                    if (alertPanel) {
                                        alertPanel->onAlertTriggered(alert);
                                    }
                                }, Qt::QueuedConnection);

                        connect(retrySystem, &AlertSystem::systemStatusChanged,
                                this, [this](bool enabled) {
                                    qDebug() << "[MAIN] System status changed:" << enabled;
                                    if (alertPanel) {
                                        alertPanel->onAlertSystemStatusChanged(enabled);
                                    }
                                }, Qt::QueuedConnection);
                    }
                });
            }
        });
    }

    try {
        qDebug() << "[MAIN] Creating Alert panel...";

        // Create Alert panel (even with null AlertSystem for UI testing)
        alertPanel = new AlertPanel(ecchart, alertSystem, this);

        qDebug() << "[MAIN] Creating alert dock widget...";

        // Create dock widget
        alertDock = new QDockWidget(tr("Alert Manager"), this);
        alertDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        alertDock->setWidget(alertPanel);

        // PERBAIKAN: Force show dock
        alertDock->setVisible(true);
        alertDock->show();

        qDebug() << "[MAIN] Adding dock to main window...";

        // Add to left dock area (berbeda dengan GuardZone yang di kanan)
        addDockWidget(Qt::LeftDockWidgetArea, alertDock);

        qDebug() << "[MAIN] Alert dock added successfully";

        // Add to view menu
        QMenu* viewMenu = nullptr;
        QList<QAction*> actions = menuBar()->actions();
        for (QAction* action : actions) {
            if (action->menu() && action->menu()->title() == tr("&Sidebar")) {
                viewMenu = action->menu();
                break;
            }
        }

        if (viewMenu) {
            viewMenu->addAction(alertDock->toggleViewAction());
            qDebug() << "[MAIN] Alert panel added to view menu";
        } else {
            qDebug() << "[MAIN] Warning: Sidebar menu not found";
        }

        // Signal connections dengan error handling
        if (alertSystem) {
            qDebug() << "[MAIN] Setting up signal connections with AlertSystem...";

            bool conn1 = connect(ecchart, &EcWidget::alertTriggered,
                                 this, &MainWindow::onAlertTriggered,
                                 Qt::QueuedConnection);

            bool conn2 = connect(ecchart, &EcWidget::criticalAlertTriggered,
                                 this, &MainWindow::onCriticalAlertTriggered,
                                 Qt::QueuedConnection);

            bool conn3 = connect(ecchart, &EcWidget::alertSystemStatusChanged,
                                 this, &MainWindow::onAlertSystemStatusChanged,
                                 Qt::QueuedConnection);

            qDebug() << "[MAIN] EcWidget signal connections:" << conn1 << conn2 << conn3;

            // TAMBAHAN: Direct AlertSystem connections ke AlertPanel
            bool conn4 = connect(alertSystem, &AlertSystem::alertTriggered,
                                 alertPanel, &AlertPanel::onAlertTriggered,
                                 Qt::QueuedConnection);

            bool conn5 = connect(alertSystem, &AlertSystem::systemStatusChanged,
                                 alertPanel, &AlertPanel::onAlertSystemStatusChanged,
                                 Qt::QueuedConnection);

            qDebug() << "[MAIN] Direct AlertSystem→Panel connections:" << conn4 << conn5;

        } else {
            qDebug() << "[MAIN] AlertSystem is null - limited signal connections";

            // Still connect EcWidget signals if available
            connect(ecchart, &EcWidget::alertTriggered,
                    this, &MainWindow::onAlertTriggered,
                    Qt::QueuedConnection);

            connect(ecchart, &EcWidget::criticalAlertTriggered,
                    this, &MainWindow::onCriticalAlertTriggered,
                    Qt::QueuedConnection);

            connect(ecchart, &EcWidget::alertSystemStatusChanged,
                    this, &MainWindow::onAlertSystemStatusChanged,
                    Qt::QueuedConnection);
        }

        // AlertPanel specific connections
        if (alertPanel) {
            bool conn6 = connect(alertPanel, &AlertPanel::alertSelected,
                                 this, &MainWindow::onAlertSelected,
                                 Qt::QueuedConnection);

            bool conn7 = connect(alertPanel, &AlertPanel::alertDetailsRequested,
                                 [this](int alertId) {
                                     qDebug() << "Alert details requested for ID:" << alertId;
                                     statusBar()->showMessage(tr("Alert details: ID %1").arg(alertId), 3000);
                                 });

            bool conn8 = connect(alertPanel, &AlertPanel::errorOccurred,
                                 [this](const QString& error) {
                                     qWarning() << "Alert Panel Error:" << error;
                                     statusBar()->showMessage(tr("Alert Panel Error: %1").arg(error), 5000);
                                 });

            qDebug() << "[MAIN] AlertPanel signal connections:" << conn6 << conn7 << conn8;
        }

        // Delayed initial refresh
        QTimer::singleShot(200, [this]() {
            if (alertPanel) {
                qDebug() << "[MAIN] Performing delayed initial refresh...";
                alertPanel->refreshAlertList();
                qDebug() << "[MAIN] Alert panel initial refresh completed";

                // Validation after refresh
                QTimer::singleShot(300, [this]() {
                    if (alertPanel) {
                        bool valid = alertPanel->validatePanelState();
                        qDebug() << "[MAIN] Panel validation result:" << valid;

                        if (!valid) {
                            qWarning() << "[MAIN] Panel validation failed - attempting recovery";
                            alertPanel->recoverFromError();
                        }
                    }
                });
            }
        });

        qDebug() << "[MAIN] Alert panel setup completed successfully";

    } catch (const std::exception& e) {
        qDebug() << "[MAIN] Critical error setting up Alert panel:" << e.what();
        QMessageBox::critical(this, tr("Setup Error"),
                              tr("Failed to setup Alert panel: %1").arg(e.what()));

        // Cleanup on error
        if (alertPanel) {
            delete alertPanel;
            alertPanel = nullptr;
        }

        if (alertDock) {
            delete alertDock;
            alertDock = nullptr;
        }

    } catch (...) {
        qDebug() << "[MAIN] Unknown critical error setting up Alert panel";
        QMessageBox::critical(this, tr("Setup Error"),
                              tr("Unknown error occurred while setting up Alert panel"));

        // Cleanup on error
        if (alertPanel) {
            delete alertPanel;
            alertPanel = nullptr;
        }

        if (alertDock) {
            delete alertDock;
            alertDock = nullptr;
        }
    }
}

// Tambahkan alert handling methods:
void MainWindow::onAlertTriggered(const AlertData& alert)
{
    qDebug() << "MainWindow: Alert triggered -" << alert.title;

    // Update status bar
    QString statusMsg = QString("ALERT: %1 - %2").arg(alert.title, alert.message);
    statusBar()->showMessage(statusMsg, 10000); // Show for 10 seconds

    // Show alert dock if hidden and alert is high priority
    if (alert.priority >= PRIORITY_HIGH && alertDock && !alertDock->isVisible()) {
        alertDock->show();
        alertDock->raise();
    }

    // Flash application if critical
    if (alert.priority == PRIORITY_CRITICAL) {
        QApplication::alert(this, 3000); // Flash for 3 seconds
    }
}

void MainWindow::onCriticalAlertTriggered(const AlertData& alert)
{
    qWarning() << "MainWindow: CRITICAL ALERT -" << alert.title;

    // Force show alert panel
    if (alertDock) {
        alertDock->show();
        alertDock->raise();
        alertDock->activateWindow();
    }

    // Flash window more prominently
    QApplication::alert(this, 5000);

    // Update status bar with critical styling
    statusBar()->setStyleSheet("QStatusBar { background-color: red; color: white; font-weight: bold; }");
    statusBar()->showMessage(QString("🚨 CRITICAL ALERT: %1").arg(alert.title), 15000);

    // Reset status bar style after delay
    QTimer::singleShot(15000, [this]() {
        statusBar()->setStyleSheet("");
    });
}

void MainWindow::onAlertSelected(int alertId)
{
    qDebug() << "MainWindow: Alert selected -" << alertId;
    statusBar()->showMessage(tr("Alert %1 selected").arg(alertId), 2000);
}

void MainWindow::onAlertSystemStatusChanged(bool enabled)
{
    qDebug() << "MainWindow: Alert system status changed -" << enabled;

    QString status = enabled ? "enabled" : "disabled";
    statusBar()->showMessage(tr("Alert system %1").arg(status), 3000);
}

void MainWindow::testAlertWorkflow()
{
    if (!ecchart) {
        QMessageBox::warning(this, "Test", "EcWidget not available");
        return;
    }

    AlertSystem* alertSystem = ecchart->getAlertSystem();
    if (!alertSystem) {
        QMessageBox::warning(this, "Test", "Alert System not available");
        return;
    }

    // Create test alert
    int alertId = alertSystem->triggerAlert(
        ALERT_NAVIGATION_WARNING,
        PRIORITY_MEDIUM,
        "Test Workflow Alert",
        "This alert will be acknowledged and then resolved automatically",
        "Testing_System",
        0.0, 0.0
        );

    if (alertId > 0) {
        // Show message and auto-acknowledge after 3 seconds
        QMessageBox::information(this, "Test Workflow",
                                 QString("Created alert ID %1.\nWill auto-acknowledge in 3 seconds, then resolve in 6 seconds.").arg(alertId));

        // Auto acknowledge after 3 seconds
        QTimer::singleShot(3000, [this, alertSystem, alertId]() {
            bool success = alertSystem->acknowledgeAlert(alertId);
            if (success) {
                statusBar()->showMessage(QString("Alert %1 acknowledged").arg(alertId), 3000);

                // Auto resolve after additional 3 seconds
                QTimer::singleShot(3000, [this, alertSystem, alertId]() {
                    bool resolved = alertSystem->resolveAlert(alertId);
                    if (resolved) {
                        statusBar()->showMessage(QString("Alert %1 resolved").arg(alertId), 3000);
                    }
                });
            }
        });
    }
}

// Placeholder implementations untuk CPA/TCPA
void MainWindow::onCPASettings()
{
    qDebug() << "Opening CPA/TCPA Settings dialog...";

    CPATCPASettingsDialog dialog(this);

    // Load current settings
    CPATCPASettings& settings = CPATCPASettings::instance();
    dialog.setCPAThreshold(settings.getCPAThreshold());
    dialog.setTCPAThreshold(settings.getTCPAThreshold());
    dialog.setCPAAlarmEnabled(settings.isCPAAlarmEnabled());
    dialog.setTCPAAlarmEnabled(settings.isTCPAAlarmEnabled());
    dialog.setVisualAlarmEnabled(settings.isVisualAlarmEnabled());
    dialog.setAudioAlarmEnabled(settings.isAudioAlarmEnabled());
    dialog.setAlarmUpdateInterval(settings.getAlarmUpdateInterval());

    if (dialog.exec() == QDialog::Accepted) {
        // Save new settings
        settings.setCPAThreshold(dialog.getCPAThreshold());
        settings.setTCPAThreshold(dialog.getTCPAThreshold());
        settings.setCPAAlarmEnabled(dialog.isCPAAlarmEnabled());
        settings.setTCPAAlarmEnabled(dialog.isTCPAAlarmEnabled());
        settings.setVisualAlarmEnabled(dialog.isVisualAlarmEnabled());
        settings.setAudioAlarmEnabled(dialog.isAudioAlarmEnabled());
        settings.setAlarmUpdateInterval(dialog.getAlarmUpdateInterval());
        settings.saveSettings();

        qDebug() << "CPA/TCPA settings updated and saved";
    }
}

void MainWindow::onShowCPATargets(bool enabled)
{
    qDebug() << "Show CPA Targets:" << enabled;

    ecchart->ShowDangerTarget(enabled);

    if (m_cpatcpaDock) {
        m_cpatcpaDock->setVisible(enabled);
    }

    if (enabled) {
        addTextToBar("CPA Target display enabled");
    } else {
        addTextToBar("CPA Target display disabled");
    }
}

void MainWindow::onShowTCPAInfo(bool enabled)
{
    qDebug() << "Show TCPA Info:" << enabled;

    if (m_cpatcpaPanel && enabled) {
        m_cpatcpaPanel->refreshData();
        addTextToBar("TCPA Information display enabled");
    } else {
        addTextToBar("TCPA Information display disabled");
    }
}

void MainWindow::onCPATCPAAlarms(bool enabled)
{
    qDebug() << "CPA/TCPA Alarms:" << enabled;

    if (enabled) {
        addTextToBar("CPA/TCPA Alarms enabled");
        m_cpaUpdateTimer->start();
        if (m_cpatcpaDock) {
            m_cpatcpaDock->setVisible(true);
        }
    } else {
        addTextToBar("CPA/TCPA Alarms disabled");
        m_cpaUpdateTimer->stop();
    }
}

// ORIENTATION DISPLAY MODE
void MainWindow::onNorthUp(bool checked) {
    if (checked) {
        ecchart->displayOrientation = EcWidget::NorthUp;

        QSettings settings("config.ini", QSettings::IniFormat);
        settings.setValue("OwnShip/orientation", EcWidget::NorthUp);

        update();
        DrawChart();
        DrawChart();
    }
}

void MainWindow::onHeadUp(bool checked) {
    if (checked) {
        ecchart->displayOrientation = EcWidget::HeadUp;

        QSettings settings("config.ini", QSettings::IniFormat);
        settings.setValue("OwnShip/orientation", EcWidget::HeadUp);

        update();
        DrawChart();
        DrawChart();
    }
}

void MainWindow::onCourseUp(bool checked) {
    if (checked) {
        ecchart->displayOrientation = EcWidget::CourseUp;

        QSettings settings("config.ini", QSettings::IniFormat);
        settings.setValue("OwnShip/orientation", EcWidget::CourseUp);
        settings.setValue("OwnShip/course_heading", 0);

        update();
        DrawChart();
        DrawChart();
    }
}

// OWNSHIP CENTERING MODE
void MainWindow::onCentered(bool checked) {
    if (checked) {
        ecchart->osCentering = EcWidget::Centered;

        QSettings settings("config.ini", QSettings::IniFormat);
        settings.setValue("OwnShip/centering", EcWidget::Centered);

        update();
        DrawChart();
        DrawChart();
    }
}

void MainWindow::onLookAhead(bool checked) {
    if (checked) {
        ecchart->osCentering = EcWidget::LookAhead;

        QSettings settings("config.ini", QSettings::IniFormat);
        settings.setValue("OwnShip/centering", EcWidget::LookAhead);

        update();
        DrawChart();
        DrawChart();
    }
}

void MainWindow::onManual(bool checked) {
    if (checked) {
        ecchart->osCentering = EcWidget::Manual;

        QSettings settings("config.ini", QSettings::IniFormat);
        settings.setValue("OwnShip/centering", EcWidget::Manual);

        update();
        DrawChart();
        DrawChart();
    }
}

void MainWindow::updateCPATCPAForAllTargets()
{
    VesselState ownShip;
    ownShip.lat = 29.4037;
    ownShip.lon = -94.7497;
    ownShip.cog = 90.0;
    ownShip.sog = 5.0;
    ownShip.timestamp = QDateTime::currentDateTime();

    qDebug() << "Own ship position:" << ownShip.lat << ownShip.lon;

    // Update AIS targets list first
    if (ecchart && ecchart->hasAISData()) {
        ecchart->updateAISTargetsList();  // Refresh data

        QList<AISTargetData> targets = ecchart->getAISTargets();
        qDebug() << "AIS data available, target count:" << targets.size();

        if (targets.isEmpty()) {
            qDebug() << "No AIS targets found, using test target";
            processTestTarget(ownShip);
        } else {
            qDebug() << "Processing" << targets.size() << "AIS targets from file data";
            // Process real AIS targets
            for (const auto& target : targets) {
                processAISTarget(ownShip, target);
            }
        }
    } else {
        qDebug() << "No AIS data available, using test target";
        processTestTarget(ownShip);
    }

    checkCPATCPAAlarms();
}

void MainWindow::checkCPATCPAAlarms()
{
    CPATCPASettings& settings = CPATCPASettings::instance();

    if (!settings.isCPAAlarmEnabled() && !settings.isTCPAAlarmEnabled()) {
        return;
    }

    // Count dangerous targets
    int dangerousTargets = 0;

    /*
    for (const auto& target : aisTargets) { // Sesuaikan dengan struktur data Anda
        if (target.isDangerous && target.cpaCalculationValid) {
            dangerousTargets++;
        }
    }
    */

    // Update status bar atau log dengan informasi alarm
    if (dangerousTargets > 0) {
        QString alarmText = QString("CPA/TCPA ALARM: %1 dangerous target(s)").arg(dangerousTargets);

        if (settings.isVisualAlarmEnabled()) {
            // Update status bar atau tampilkan visual alarm
            statusBar()->showMessage(alarmText, 5000);
        }

        // Log alarm
        addTextToBar(alarmText);
        qDebug() << alarmText;
    }
}

void MainWindow::logCPATCPAInfo(const QString& mmsi, const CPATCPAResult& result)
{
    QString logText = QString("MMSI %1: CPA=%.2f NM, TCPA=%.1f min, Range=%.2f NM, Bearing=%.0f°")
                          .arg(mmsi)
                          .arg(result.cpa)
                          .arg(result.tcpa)
                          .arg(result.currentRange)
                          .arg(result.relativeBearing);

    addTextToBar(logText);
    qDebug() << "CPA/TCPA Info:" << logText;
}

void MainWindow::processTestTarget(const VesselState& ownShip)
{
    // Test target yang sudah berhasil
    VesselState testTarget;
    testTarget.lat = 29.41;
    testTarget.lon = -94.73;
    testTarget.cog = 270.0;
    testTarget.sog = 8.0;
    testTarget.timestamp = QDateTime::currentDateTime();

    // Calculate CPA/TCPA
    CPATCPAResult result = m_cpaCalculator->calculateCPATCPA(ownShip, testTarget);

    if (result.isValid) {
        QString logText = QString("TEST TARGET: CPA=%.2f NM, TCPA=%.1f min, Range=%.2f NM")
        .arg(result.cpa)
            .arg(result.tcpa)
            .arg(result.currentRange);

        addTextToBar(logText);

        // Check if dangerous
        CPATCPASettings& settings = CPATCPASettings::instance();
        bool isDangerous = false;

        if (settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) {
            isDangerous = true;
        }

        if (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()) {
            isDangerous = true;
        }

        if (isDangerous) {
            QString alarmText = "⚠️ CPA/TCPA ALARM: Test target is dangerous!";
            addTextToBar(alarmText);
            statusBar()->showMessage(alarmText, 5000);
        }
    }
}

void MainWindow::processAISTarget(const VesselState& ownShip, const AISTargetData& target)
{
    // Convert AISTargetData ke VesselState
    VesselState targetVessel;
    targetVessel.lat = target.lat;
    targetVessel.lon = target.lon;
    targetVessel.cog = target.cog;
    targetVessel.sog = target.sog;
    targetVessel.timestamp = target.lastUpdate;

    // Calculate CPA/TCPA
    CPATCPAResult result = m_cpaCalculator->calculateCPATCPA(ownShip, targetVessel);

    if (result.isValid) {
        QString logText = QString("AIS TARGET %1: CPA=%.2f NM, TCPA=%.1f min, Range=%.2f NM")
        .arg(target.mmsi)
            .arg(result.cpa)
            .arg(result.tcpa)
            .arg(result.currentRange);

        addTextToBar(logText);
        qDebug() << "CPA/TCPA Result for MMSI" << target.mmsi << ":" << logText;

        // Check if dangerous
        CPATCPASettings& settings = CPATCPASettings::instance();
        bool isDangerous = false;

        if (settings.isCPAAlarmEnabled() && result.cpa < settings.getCPAThreshold()) {
            isDangerous = true;
        }

        if (settings.isTCPAAlarmEnabled() && result.tcpa > 0 && result.tcpa < settings.getTCPAThreshold()) {
            isDangerous = true;
        }

        if (isDangerous) {
            QString alarmText = QString("⚠️ CPA/TCPA ALARM: AIS Target %1 is dangerous! CPA=%.2f NM, TCPA=%.1f min")
                                    .arg(target.mmsi)
                                    .arg(result.cpa)
                                    .arg(result.tcpa);
            addTextToBar(alarmText);
            statusBar()->showMessage(alarmText, 8000);
        }
    } else {
        qDebug() << "CPA/TCPA calculation invalid for MMSI" << target.mmsi;
    }
}

void MainWindow::setupCPATCPAPanel()
{
    // Create CPA/TCPA panel
    m_cpatcpaPanel = new CPATCPAPanel();
    m_cpatcpaPanel->setEcWidget(ecchart);

    // Create dock widget untuk panel
    m_cpatcpaDock = new QDockWidget(tr("CPA/TCPA Monitor"), this);
    m_cpatcpaDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_cpatcpaDock->setWidget(m_cpatcpaPanel);

    // Add dock to main window
    addDockWidget(Qt::RightDockWidgetArea, m_cpatcpaDock);

    // Add CPA/TCPA Monitor to Sidebar menu
    QMenu* sidebarMenu = nullptr;
    QList<QMenu*> menus = menuBar()->findChildren<QMenu*>();
    for (QMenu* menu : menus) {
        if (menu->title().contains("Sidebar") || menu->title().contains("&Sidebar")) {
            sidebarMenu = menu;
            break;
        }
    }

    if (sidebarMenu) {
        sidebarMenu->addAction(m_cpatcpaDock->toggleViewAction());
    }

    // Set initial visibility
    m_cpatcpaDock->setVisible(false);
}

void MainWindow::onEnableRedDotTracker(bool enabled)
{
    qDebug() << "onEnableRedDotTracker called with enabled =" << enabled;

    if (ecchart) {
        ecchart->setRedDotTrackerEnabled(enabled);

        if (enabled) {
            statusBar()->showMessage(tr("Red dot tracker enabled"), 3000);
        } else {
            statusBar()->showMessage(tr("Red dot tracker disabled"), 3000);
        }
    }
}

void MainWindow::onAttachRedDotToShip(bool attached)
{
    qDebug() << "onAttachRedDotToShip called with attached =" << attached;

    if (ecchart) {
        ecchart->setRedDotAttachedToShip(attached);

        // Auto-enable tracker when attaching
        if (attached) {
            ecchart->setRedDotTrackerEnabled(true);
            statusBar()->showMessage(tr("Red dot tracker attached to ship"), 3000);
        } else {
            statusBar()->showMessage(tr("Red dot tracker detached from ship"), 3000);
        }
    }
}

// test guardzone
void MainWindow::onTestGuardZone(bool enabled)
{
    qDebug() << "onTestGuardZone called with enabled =" << enabled;

    if (ecchart) {
        if (enabled) {
            // Aktifkan Test GuardZone
            ecchart->setTestGuardZoneEnabled(true);
            statusBar()->showMessage(tr("Test guardzone activated"), 3000);
        } else {
            // Nonaktifkan Test GuardZone
            ecchart->setTestGuardZoneEnabled(false);
            statusBar()->showMessage(tr("Test guardzone deactivated"), 3000);
        }

        // Update chart display
        ecchart->update();
    }
}

void MainWindow::onAutoCheckShipGuardian(bool enabled)
{
    qDebug() << "onAutoCheckShipGuardian called with enabled =" << enabled;

    if (ecchart) {
        ecchart->setShipGuardianAutoCheck(enabled);

        if (enabled) {
            statusBar()->showMessage(tr("Ship Guardian auto-check enabled (every 5 seconds)"), 3000);
        } else {
            statusBar()->showMessage(tr("Ship Guardian auto-check disabled"), 3000);
        }
    }
}

void MainWindow::onCheckShipGuardianNow()
{
    qDebug() << "onCheckShipGuardianNow called";

    if (ecchart) {
        if (ecchart->checkShipGuardianZone()) {
            statusBar()->showMessage(tr("Ship Guardian check completed - obstacles detected!"), 5000);
        } else {
            statusBar()->showMessage(tr("Ship Guardian check completed - no obstacles"), 3000);
        }
    }
}

void MainWindow::onToggleGuardZoneAutoCheck(bool enabled)
{
    if (!ecchart) return;

    ecchart->setGuardZoneAutoCheck(enabled);

    QString status = enabled ? "enabled" : "disabled";
    statusBar()->showMessage(tr("GuardZone auto-check %1").arg(status), 3000);

    qDebug() << "GuardZone auto-check toggled:" << enabled;
}

void MainWindow::onConfigureGuardZoneAutoCheck()
{
    if (!ecchart) return;

    bool ok;
    int currentInterval = ecchart->getGuardZoneCheckInterval() / 1000; // Convert to seconds

    int newInterval = QInputDialog::getInt(this,
                                           tr("Configure Auto-Check"),
                                           tr("Check interval (seconds):"),
                                           currentInterval, 1, 300, 1, &ok);

    if (ok) {
        ecchart->setGuardZoneCheckInterval(newInterval * 1000); // Convert to milliseconds
        statusBar()->showMessage(tr("Auto-check interval set to %1 seconds").arg(newInterval), 3000);

        qDebug() << "GuardZone auto-check interval configured:" << newInterval << "seconds";
    }
}

void MainWindow::onShowGuardZoneStatus()
{
    if (!ecchart) return;

    QStringList statusInfo;
    statusInfo << "=== GuardZone Real-Time Status ===";
    statusInfo << "";

    // System status
    statusInfo << QString("System Active: %1").arg(ecchart->isGuardZoneActive() ? "YES" : "NO");
    statusInfo << QString("Auto-Check: %1").arg(ecchart->isGuardZoneAutoCheckEnabled() ? "ENABLED" : "DISABLED");
    statusInfo << QString("Check Interval: %1 seconds").arg(ecchart->getGuardZoneCheckInterval() / 1000);
    statusInfo << "";

    // GuardZone list
    QList<GuardZone>& guardZones = ecchart->getGuardZones();
    statusInfo << QString("Total GuardZones: %1").arg(guardZones.size());

    for (const GuardZone& gz : guardZones) {
        QString shapeStr = (gz.shape == GUARD_ZONE_CIRCLE) ? "Circle" : "Polygon";
        QString attachStr = gz.attachedToShip ? " (Ship-attached)" : " (Static)";
        QString activeStr = gz.active ? "ACTIVE" : "inactive";

        statusInfo << QString("- %1: %2%3 [%4]").arg(gz.name).arg(shapeStr).arg(attachStr).arg(activeStr);

        if (gz.shape == GUARD_ZONE_CIRCLE) {
            statusInfo << QString("  Center: %1, %2  Radius: %3 NM")
            .arg(gz.centerLat, 0, 'f', 6)
                .arg(gz.centerLon, 0, 'f', 6)
                .arg(gz.radius, 0, 'f', 2);
        }
    }

    statusInfo << "";
    statusInfo << "Real-time monitoring will detect AIS targets";
    statusInfo << "entering/exiting active GuardZones automatically.";

    QMessageBox::information(this, tr("GuardZone Status"), statusInfo.join("\n"));
}
