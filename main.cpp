// #include <QtGui>
#include <QtWidgets>
#include "mainwindow.h"
#include "logger.h"
#include "iplugininterface.h"
#include "IAisDvrPlugin.h"
#include "SettingsManager.h"
#include "PluginManager.h"
#include "aisdatabasemanager.h"
#include "appconfig.h"

// PERBAIKAN: Message filter untuk menghilangkan HANYA warning debug messages Qt
void messageFilter(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Filter out ONLY specific warning debug messages, not QPainter functionality
    if (type == QtWarningMsg && 
        (msg.contains("WARNING: QWidget::paintEngine") || 
         msg.contains("WARNING: QPainter::") ||
         msg.contains("QWidget::paintEngine"))) {
        return; // Ignore these specific warning messages only
    }
    
    // Let ALL other messages through (including debug, info, critical)
    QTextStream stream(stdout);
    stream << msg << Qt::endl;
}

int main( int argc, char ** argv )
{
  QApplication a( argc, argv );

  a.setWindowIcon(QIcon(":/icon/dummy_icon.png"));

  // PERBAIKAN: Install message filter
  qInstallMessageHandler(messageFilter);

  // LOAD CONFIG.INI
  SettingsManager::instance().load();

  // LOAD PLUGIN
  if (AppConfig::isBeta()){
      PluginManager::instance().loadPlugin("/AisDvrPlugin.dll", "IAisDvrPlugin");
  }

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

  mw->resize(800, 800);
  mw->showMaximized();

  a.connect( &a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()) );

  int result = a.exec();

  // CRITICAL: Proper cleanup order to prevent crash
  // 1. Clear Logger's textEdit pointer FIRST before destroying MainWindow
  Logger::cleanup();

  // 2. Then destroy MainWindow (which contains logText widget)
  delete mw;

  return result;
}
