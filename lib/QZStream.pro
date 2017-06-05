VERSION = 1.0.2

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

CONFIG(debug, debug|release) {
    CONFIG_DIR = Debug
} else {
    CONFIG_DIR = Release
}

DESTDIR = $$_PRO_FILE_PWD_/../build/$$CONFIG_DIR

include(../QZStream.pri)

