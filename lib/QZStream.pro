VERSION = 1.0.0

QT -= gui

TARGET = QZStream

TEMPLATE = lib
CONFIG += staticlib

HEADERS += \
    QZStream.h \
    QCCZStream.h

SOURCES += \
    QZStream.cpp \
    QCCZStream.cpp

include(../QZStream.pri)

unix|win32-g++ {
    QMAKE_CXXFLAGS_WARN_OFF -= -w
    QMAKE_CXXFLAGS += -Wall
}

win32 {
    QMAKE_CXXFLAGS_WARN_OFF -= -W0
    QMAKE_CXXFLAGS += -W3
}


