TEMPLATE = app

QT += network
QT += widgets
QT += winextras

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
    IAisDvrPlugin.h \
    PluginManager.h \
    SettingsData.h \
    SettingsDialog.h \
    SettingsManager.h \
    aisdecoder.h \
    aivdoencoder.h \
    alertmanager.h \
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
    moosdb.h \
    nmeadecoder.h \
    searchwindow.h \
    alertsystem.h \
    alertpanel.h \

SOURCES += main.cpp mainwindow.cpp ecwidget.cpp pickwindow.cpp ais.cpp \
    PluginManager.cpp \
    SettingsDialog.cpp \
    SettingsManager.cpp \
    aisdecoder.cpp \
    aivdoencoder.cpp \
    cpatcpacalculator.cpp \
    cpatcpapanel.cpp \
    cpatcpasettings.cpp \
    cpatcpasettingsdialog.cpp \
    editwaypointdialog.cpp \
    guardzonemanager.cpp \
    guardzonepanel.cpp \
    logger.cpp \
    moosdb.cpp \
    nmeadecoder.cpp \
    searchwindow.cpp \
    alertsystem.cpp \
    alertpanel.cpp \

RESOURCES += \
    resources.qrc

DISTFILES += \

SUBDIRS += \

CONFIG += plugin
