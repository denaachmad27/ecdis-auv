QT += core
QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = test_decoder

SOURCES += test_decoder_fix.cpp aivdoencoder.cpp

HEADERS += aivdoencoder.h

DEFINES += _WIN32