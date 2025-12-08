QT += core sql
QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = test_correct_type5

# Include project files
SOURCES += \
    test_correct_type5.cpp \
    aivdoencoder.cpp \
    aisdatabasemanager.cpp

HEADERS += \
    aivdoencoder.h \
    aisdatabasemanager.h

# Include database driver
LIBS += -lQt5Sql

# Define for Windows
DEFINES += _WIN32