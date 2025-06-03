// #include <QtGui>
#include <QtWidgets>
#include "mainwindow.h"
#include "logger.h"
#include "iplugininterface.h"
#include "IAisDvrPlugin.h"
#include "SettingsManager.h"
#include "PluginManager.h"

int main( int argc, char ** argv ) 
{
  QApplication a( argc, argv );

  // LOAD CONFIG.INI
  SettingsManager::instance().load();

  // LOAD PLUGIN
  PluginManager::instance().loadPlugin("/AisDvrPlugin.dll", "IAisDvrPlugin");

  MainWindow * mw;
  try
  {
    mw = new MainWindow();
  }
  catch (MainWindow::Exception & e)
  {
    QString msg;
    QStringListIterator iT(e.GetMessages());
    while (iT.hasNext())
      msg += iT.next() + "\n";

    QMessageBox::critical(NULL, "Error", msg);
    return 1;
  }

  Logger logger(mw->logText); // Logger diarahkan ke QTextEdit

  qDebug() << "ECDIS Started";
  //qWarning() << "Tes peringatan";

  mw->resize(960, 600);
  mw->showMaximized();

  a.connect( &a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()) );

  return a.exec();
}
