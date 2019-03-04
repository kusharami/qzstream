VERSION = 1.0.0

TARGET = qcczimagecontainer

TEMPLATE = lib
CONFIG += plugin

CONFIG += c++11 warn_off

unix|win32-g++ {
    QMAKE_CXXFLAGS_WARN_OFF -= -w
    QMAKE_CXXFLAGS += -Wall
    QMAKE_CXXFLAGS += \
        -Wno-unknown-pragmas
}

win32-msvc* {
    QMAKE_CXXFLAGS_WARN_OFF -= -W0
    QMAKE_CXXFLAGS += -W3
    QMAKE_LFLAGS += /NODEFAULTLIB:LIBCMT
}

DESTDIR = $$[QT_INSTALL_PLUGINS]/imageformats

HEADERS += \
    QCCZImageContainerPlugin.h \
    QCCZImageContainerHandler.h

SOURCES += \
    QCCZImageContainerPlugin.cpp \
    QCCZImageContainerHandler.cpp

include(../QZStream.pri)

win32 {
    CONFIG(debug, debug|release) {
        TARGET_EXT = d.dll
    } else {
        TARGET_EXT = .dll
    }
}

CONFIG(debug, debug|release) {
    CONFIG_DIR = Debug
} else {
    CONFIG_DIR = Release
}

BUILD_LIBS_DIR = $$_PRO_FILE_PWD_/../build/$$CONFIG_DIR

win32-msvc*:PRE_TARGETDEPS += \    
    $$BUILD_LIBS_DIR/QZStream.lib

linux|macx:PRE_TARGETDEPS += \
    $$BUILD_LIBS_DIR/libQZStream.a

LIBS += -L$$BUILD_LIBS_DIR
LIBS += -lQZStream

OTHER_FILES += ccz.json

INCLUDEPATH += ../lib

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QCCZImageContainerPlugin

DISTFILES += CHANGELOG