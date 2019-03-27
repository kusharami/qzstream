#-------------------------------------------------
#
# Project created by QtCreator 2017-05-22T12:21:21
#
#-------------------------------------------------

QT       += testlib

QT       += gui

TARGET = QZStreamTests
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += \
    Tests.h

SOURCES += \
    main.cpp \
    Tests.cpp

INCLUDEPATH += ../lib

include(../QZStreamDepend.pri)
