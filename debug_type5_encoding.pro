QT += core
QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = debug_type5_encoding

SOURCES += \
    debug_type5_encoding.cpp \
    aivdoencoder.cpp

HEADERS += \
    aivdoencoder.h

DEFINES += _WIN32