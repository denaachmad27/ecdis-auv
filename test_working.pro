QT += core
QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = test_working

SOURCES += test_working_encoder.cpp aivdoencoder.cpp

HEADERS += aivdoencoder.h

DEFINES += _WIN32