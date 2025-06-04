// #include <QtGui>
#include <QtWidgets>

#include <QPluginLoader>
#include <QDir>

#include "mainwindow.h"
#include "pickwindow.h"
#include "searchwindow.h"
#include "iplugininterface.h"
#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "PluginManager.h"
#include "guardzone.h"

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

    //QMenu* viewMenu = menuBar()->addMenu(tr("&Sidebar"));
    dock = new QDockWidget(tr("NMEA Received"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    nmeaText = new QTextEdit(dock);
    nmeaText->setText("");
    nmeaText->setReadOnly(true); // Membuat teks tidak dapat diedit
    dock->setWidget(nmeaText);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());
    dock->hide();

    //QMenu* viewMenu = menuBar()->addMenu(tr("&Sidebar"));
    dock = new QDockWidget(tr("Ownship Info"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    ownShipText = new QTextEdit(dock);
    ownShipText->setText("");
    ownShipText->setReadOnly(true); // Membuat teks tidak dapat diedit
    dock->setWidget(ownShipText);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());

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
  }
  catch (EcWidget::Exception & e)
  {
    throw Exception(e.GetMessages(), "Cannot create ECDIS Widget");
  }
  setCentralWidget(ecchart);

  connect(ecchart, SIGNAL(waypointCreated()), this, SLOT(onWaypointCreated()));


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
  ecchart->SetLookupTable(lookupTable);
  ecchart->SetDisplayCategory(displayCategory);
  ecchart->ShowLights(showLights);
  ecchart->ShowText(showText);
  ecchart->ShowSoundings(showSoundings);
  ecchart->ShowGrid(showGrid);
  ecchart->ShowAIS(showAIS);
  setDisplay();

  // Create the window for the pick report
  pickWindow = new PickWindow(this, dict, ecchart->GetDENC());

  // Create the window for the search lat lon
  searchWindow = new SearchWindow(tr("Enter Search Details"), this);

  // Iinitialize the AIS settings
  ecchart->InitAIS( dict );

  // Start subscribe MOOSDB
  // ecchart->StartThreadSubscribeSSH(ecchart);

  // Create the main user interface 
  connect(ecchart, SIGNAL(scale(int)), this, SLOT( onScale(int)));  
  connect(ecchart, SIGNAL(projection()), this, SLOT( onProjection()));  
  connect(ecchart, SIGNAL(mouseMove(EcCoordinate, EcCoordinate)), this, SLOT( onMouseMove(EcCoordinate, EcCoordinate)));  
  connect(ecchart, SIGNAL(mouseRightClick()), this, SLOT( onMouseRightClick()));  

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

  statusBar()->addPermanentWidget(new QLabel("Range:", statusBar()));
  statusBar()->addPermanentWidget(rngEdit, 0);
  statusBar()->addPermanentWidget(new QLabel("Scale:", statusBar()));
  statusBar()->addPermanentWidget(sclEdit, 0);
  statusBar()->addPermanentWidget(new QLabel("Projection:", statusBar()));
  statusBar()->addPermanentWidget(proEdit, 0);
  statusBar()->addPermanentWidget(new QLabel("Cursor:", statusBar()));
  statusBar()->addPermanentWidget(posEdit, 0);

  // File menu
  QMenu *fileMenu = menuBar()->addMenu("&File");

  fileMenu->addAction("E&xit", this, SLOT(close()))->setShortcut(tr("Ctrl+x", "File|Exit"));
  // fileMenu->addAction("Reload", this, SLOT(onReload()));

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
  QMenu *drawMenu = menuBar()->addMenu("&Draw");

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
  QMenu *viewMenu = menuBar()->addMenu("&View");

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
  otherAction       = vActionGroup->addAction("Other");
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

  viewMenu->addSeparator();

  // QAction *searchAction = viewMenu->addAction("Search");
  // connect(searchAction, &QAction::triggered, this, &MainWindow::onSearch);

  // Color menu
  QMenu *colorMenu = menuBar()->addMenu("&Colour");

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

  // AIS
  QMenu *aisMenu = menuBar()->addMenu("&AIS");
  aisMenu->addAction( tr( "Run AIS" ), this, SLOT( runAis() ) );
  aisMenu->addAction( tr( "Load AIS Logfile" ), this, SLOT( slotLoadAisFile() ) );
  aisMenu->addAction( tr( "Load AIS Variable" ), this, SLOT( slotLoadAisVariable() ) );
  aisMenu->addAction( tr( "Stop Load AIS Variable" ), this, SLOT( slotStopLoadAisVariable() ) );
  aisMenu->addAction( tr( "Connect to AIS Server" ), this, SLOT( slotConnectToAisServer() ) );

  // MOOSDB
  QMenu *moosMenu = menuBar()->addMenu("&MOOSDB");
  moosMenu->addAction("Restart Connection", this, SLOT(subscribeMOOSDB()) );
  //moosMenu->addAction("Subscribe MAP_INFO", this, SLOT(subscribeMOOSDBMAP()) );
  moosMenu->addAction("Stop Connection", this, SLOT(stopSubscribeMOOSDB()) );
  //moosMenu->addAction("Start Publish", this, SLOT(publishMOOSDB()) );
  //moosMenu->addAction("Stop Publish", this, SLOT(stopPublishMOOSDB()) );

  // moosMenu->addAction("Target AIS", this, SLOT(jsonExample()) );

  // SETTINGS MANAGER
  QMenu *settingMenu = menuBar()->addMenu("&Settings");
  settingMenu->addAction("Setting Manager", this, SLOT(openSettingsDialog()) );

  // SIDEBAR
  createDockWindows();
  createActions();

  // GuardZone
  // ========== INITIALIZE GUARDZONE PANEL POINTERS ==========
  guardZonePanel = nullptr;
  guardZoneDock = nullptr;
  // ========================================================

  
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
  connect(enableGuardZoneAction, SIGNAL(toggled(bool)), this, SLOT(onEnableGuardZone(bool)));

  guardZoneMenu->addSeparator();

  guardZoneMenu->addAction("Create Circular GuardZone", this, SLOT(onCreateCircularGuardZone()));
  guardZoneMenu->addAction("Create Polygon GuardZone", this, SLOT(onCreatePolygonGuardZone()));

  guardZoneMenu->addSeparator();

  guardZoneMenu->addAction("Check for Threats", this, SLOT(onCheckGuardZone()));

  // Di constructor MainWindow
  QAction *attachToShipAction = guardZoneMenu->addAction("Attach to Ship");
  attachToShipAction->setCheckable(true);
  connect(attachToShipAction, SIGNAL(toggled(bool)), this, SLOT(onAttachGuardZoneToShip(bool)));

  QMenu *simulationMenu = menuBar()->addMenu("&Simulation");

  simulationMenu->addAction("Start AIS Target Simulation", this, SLOT(onStartSimulation()));
  simulationMenu->addAction("Stop Simulation", this, SLOT(onStopSimulation()));

  simulationMenu->addSeparator();

  QAction *autoCheckAction = simulationMenu->addAction("Auto-Check GuardZone");
  autoCheckAction->setCheckable(true);
  connect(autoCheckAction, SIGNAL(toggled(bool)), this, SLOT(onAutoCheckGuardZone(bool)));

  // DVR
  QMenu *dvrMenu = menuBar()->addMenu("&AIS DVR");

  startAisRecAction = dvrMenu->addAction("Start Record", this, SLOT(startAisRecord()) );
  stopAisRecAction = dvrMenu->addAction("Stop Record", this, SLOT(stopAisRecord()) );

  startAisRecAction->setEnabled(true);
  stopAisRecAction->setEnabled(false);

  // Load Plugin
  // loadPluginAis();

    // char *userpermit = nullptr;
    // unsigned char* hwid = (unsigned char*)"8BA3-E363-1982-4EDB-257C-C";
    // //unsigned char* hwid = (unsigned char*)"6B4A-4473-2387-B940-5A7C-A";
    // unsigned char* mid = (unsigned char*)"BF";
    // unsigned char* mkey = (unsigned char*)"82115";

    // EcS63CreateUserPermit(hwid, mkey, mid, &userpermit);

    // qDebug() << userpermit;
  setupGuardZonePanel();
  setupTestingMenu();

#ifdef _DEBUG
      // Testing menu hanya untuk debug build
      setupTestingMenu();
#endif
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
  DrawChart();
}

/*---------------------------------------------------------------------------*/

void MainWindow::onRotateCCW()
{
  double hdg = ecchart->GetHeading();
  hdg -= 10;
  if (hdg < 0) hdg += 360.0;
  ecchart->SetHeading(hdg);
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

void MainWindow::onAIS(bool on)
{
  ecchart->ShowAIS(on);
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

void MainWindow::onMouseRightClick()
{
	QList<EcFeature> pickedFeatureList;

	ecchart->GetPickedFeatures(pickedFeatureList);

    pickWindow->fill(pickedFeatureList);

    if (!aisTemp->toPlainText().trimmed().isEmpty()){
        aisText->setHtml(aisTemp->toHtml());
    }

    if (!ownShipTemp->toPlainText().trimmed().isEmpty()){
        ownShipText->setHtml(ownShipTemp->toHtml());
    }


    //pickWindow->show();

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

    nmeaData << "$GPGGA,111000,2924.2304,N,09444.9053,W,2,8,1.5,-18,M,,M,,*70"
             << "$GPHDT,226.7,T*34"
             << "$GPROT,0.0,A*31"
             << "$GPVTG,198.7,T,195.4,M,0.1,N,0.1,K*40"
             << "$GPGGA,111001,2924.2304,N,09444.9053,W,2,8,1.5,-18,M,,M,,*71"
             << "!AIVDO,1,1,,,15Mw0k0001q>Ac6@lk@Gio6005H`,0*37"
             << "!AIVDO,1,1,,,15Mw0k0001q>Ac6@lk@7ho6205H`,0*44"
             << "$GPHDT,226.8,T*3B"
             << "$GPROT,0.0,A*31"
             << "$GPVTG,199.6,T,196.3,M,0.1,N,0.1,K*44"
             << "$GPGGA,111002,2924.2304,N,09444.9054,W,2,8,1.5,-17,M,,M,,*7A"
             << "$GPHDT,226.8,T*3B"
             << "$GPROT,0.0,A*31"
             << "$GPVTG,198.8,T,195.5,M,0.1,N,0.1,K*4E"
             << "$GPGGA,111003,2924.2304,N,09444.9054,W,2,8,1.5,-17,M,,M,,*7B"
             << "!AIVDO,1,1,,,15Mw0k0001q>Ac6@lk@7k76405H`,0*19"
             << "$GPHDT,226.8,T*3B"
             << "$GPROT,0.0,A*31"
             << "$GPVTG,201.4,T,198.1,M,0.1,N,0.1,K*48";

    nmeaData << aivdo;


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
    // QStringList nmeaData;
    // nmeaData << aivdo;

    // STOP THREAD
    //ecchart -> stopAISSubscribeThread();
    //ecchart->Draw();

    // START AGAIN
    ecchart->StartThreadSubscribeSSH(ecchart);
    qDebug() << "MOOSDB IP: " + SettingsManager::instance().data().moosIp;

    //serverThreadMOOSSubscribeSSH();
}

void MainWindow::stopSubscribeMOOSDB()
{
    ecchart -> stopAISSubscribeThread();
    ecchart->Draw();
}

////////////////////////////////////////////////////   END MOOSDB   /////////////////////////////////////////////////////

//////////////////////////////////////////////////////    UNUSED - FOR LOGIC BACKUP ONLY     //////////////////////////////////////////////////////

void MainWindow::subscribeMOOSDBMAP()
{
    ecchart->startAISSubscribeThreadMAP(ecchart);
}

void MainWindow::subscribeMOOSDBMapInfo()
{
    // Server subscribe
    MOOSShip moosShip;

    QList<EcFeature> pickedFeatureList;

    moosShip = serverThreadMOOSSubscribeMapInfo();

    qDebug() << moosShip.lat;
    qDebug() << moosShip.lon;

    // Arahkan ke titik tersebut
    ecchart->SetCenter(moosShip.lat, moosShip.lon);
    ecchart->SetScale(80000);

    DrawChart();

    // Server publish
    ecchart->GetPickedFeaturesSubs(pickedFeatureList, moosShip.lat, moosShip.lon);

    startServerMOOSPublishMapInfo(pickWindow->fillJson(pickedFeatureList));

    pickWindow->show();
}

void MainWindow::publishMOOSDB()
{
    startServerMOOSPublish();
}

void MainWindow::stopPublishMOOSDB()
{
    stopServerMOOSPublish();
}


void MainWindow::jsonExample()
{
    ecchart->jsonExample();
}

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
    qDebug() << "onCheckGuardZone called";
    if (ecchart) {
        qDebug() << "ecchart exists, calling checkGuardZone()";
        ecchart->checkGuardZone();
    } else {
        qDebug() << "ecchart is NULL";
    }
}

void MainWindow::onAttachGuardZoneToShip(bool attached)
{
    if (ecchart) {
        ecchart->setGuardZoneAttachedToShip(attached);
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
