TEMPLATE = app

QT += network
QT += widgets
QT += winextras
QT += core gui
QT += sql
QT += multimedia
QT += svg
QT += concurrent

# win32:KERNELPATH = ../../
win32:KERNELPATH = C:/EC2007/5.22.69.3

TARGET +=
DEPENDPATH += .
DEPENDPATH += ..
INCLUDEPATH += .
INCLUDEPATH += ..
win32:INCLUDEPATH += $${KERNELPATH}/include
unix:INCLUDEPATH += /usr/include/EC2007/5.22/kernel

win32:LIBS += $${KERNELPATH}/lib/eckernel-5.22-dynr.lib $${KERNELPATH}/lib/s63lib-5.22-dynr.lib -lgdi32 -luser32 -lshell32
win32:DEFINES += _WINNT_SOURCE 
win32{
	!contains(QMAKE_HOST.arch, x86_64) {
		DEFINES += _USE_32BIT_TIME_T
	}
}
win32:CONFIG += embed_manifest_exe

unix:LIBS += -leckernel-5.22-dynr -lX11
unix:DEFINES += _LINUX_SOURCE

# Input
HEADERS += mainwindow.h ecwidget.h pickwindow.h ais.h \
    chartmanagerpanel.h \
    AISSubscriber.h \
    IAisDvrPlugin.h \
    IndexerWorker.h \
    PluginManager.h \
    SettingsData.h \
    SettingsDialog.h \
    SettingsManager.h \
    aisdatabasemanager.h \
    aisdecoder.h \
    aistooltip.h \
    aivdoencoder.h \
    alertmanager.h \
    appconfig.h \
    compasswidget.h \
    cpatcpacalculator.h \
    cpatcpapanel.h \
    cpatcpasettings.h \
    cpatcpasettingsdialog.h \
    editwaypointdialog.h \
    guardzone.h \
    guardzonemanager.h \
    guardzonepanel.h \
    iplugininterface.h \
    logger.h \
    guardzonecheckdialog.h \
    nmeadecoder.h \
    searchwindow.h \
    alertsystem.h \
    alertpanel.h \
    aistargetpanel.h \
    obstacledetectionpanel.h \
    routepanel.h \
    routedetaildialog.h \
    waypointdialog.h \
    routeformdialog.h \
    aoi.h \
    aoipanel.h \
    poi.h \
    poipanel.h \
    routequickformdialog.h \
    routedeviationdetector.h \
    eblvrm.h \
    aitargettracker.h \
    autoroutedialog.h \
    autorouteplanner.h \
    autoroutestartdialog.h \
    tidemanager.h \
    tidepanel.h \
    tidalcurvewidget.h \
    tidalcurvewidget_simple.h \
    testpanel.h \
    src/currentvisualisation.h \
        
SOURCES += main.cpp mainwindow.cpp ecwidget.cpp pickwindow.cpp ais.cpp \
    chartmanagerpanel.cpp \
    AISSubscriber.cpp \
    IndexerWorker.cpp \
    PluginManager.cpp \
    SettingsDialog.cpp \
    SettingsManager.cpp \
    aisdatabasemanager.cpp \
    aisdecoder.cpp \
    aistooltip.cpp \
    aivdoencoder.cpp \
    appconfig.cpp \
    compasswidget.cpp \
    cpatcpacalculator.cpp \
    cpatcpapanel.cpp \
    cpatcpasettings.cpp \
    cpatcpasettingsdialog.cpp \
    editwaypointdialog.cpp \
    guardzonemanager.cpp \
    guardzonepanel.cpp \
    logger.cpp \
    nmeadecoder.cpp \
    searchwindow.cpp \
    alertsystem.cpp \
    alertpanel.cpp \
    aistargetpanel.cpp \
    obstacledetectionpanel.cpp \
    routepanel.cpp \
    routedetaildialog.cpp \
    waypointdialog.cpp \
    routeformdialog.cpp \
    aoipanel.cpp \
    poipanel.cpp \
    routequickformdialog.cpp \
    routedeviationdetector.cpp \
    eblvrm.cpp \
    aitargettracker.cpp \
    autoroutedialog.cpp \
    autorouteplanner.cpp \
    autoroutestartdialog.cpp \
    tidemanager.cpp \
    tidepanel.cpp \
    tidalcurvewidget.cpp \
    tidalcurvewidget_simple.cpp \
    testpanel.cpp \
    src/currentvisualisation.cpp \
        
RESOURCES += \
    resources.qrc

DISTFILES += \
    icon/icon.ico

SUBDIRS += \

CONFIG += plugin

LIBS += -ldwmapi

win32:RC_ICONS += icon.ico

