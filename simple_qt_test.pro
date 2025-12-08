QT += core
QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = simple_qt_test

SOURCES += simple_qt_test.cpp aivdoencoder.cpp

HEADERS += aivdoencoder.h

DEFINES += _WIN32