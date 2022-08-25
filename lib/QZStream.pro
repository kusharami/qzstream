VERSION = 2.0.2

QT -= gui

TARGET = QZStream

TEMPLATE = lib
CONFIG += staticlib

CONFIG += c++11 warn_off

unix|win32-g++ {
    QMAKE_CXXFLAGS_WARN_OFF -= -w
    QMAKE_CXXFLAGS += -Wall
} else {
    win32 {
        QMAKE_CXXFLAGS_WARN_OFF -= -W0
        QMAKE_CXXFLAGS += -W3
    }
}

HEADERS += \
    QZStream.h \
    QCCZStream.h

SOURCES += \
    QZStream.cpp \
    QCCZStream.cpp

include(../QZStream.pri)
DESTDIR = $$QZSTREAM_BIN_DIR

DISTFILES += CHANGELOG
