VERSION = 1.0.2

TARGET = qcczimagecontainer

TEMPLATE = lib
CONFIG += plugin

CONFIG += c++11 warn_off

DEFINES += QT_NO_DEPRECATED_WARNINGS

include(../QZStreamDepend.pri)

CONFIG(static, static) {
    DESTDIR = $$QZSTREAM_BIN_DIR
} else {
    DESTDIR = $$[QT_INSTALL_PLUGINS]/imageformats
    win32 {
        CONFIG(debug, debug|release) {
            TARGET_EXT = d.dll
        } else {
            TARGET_EXT = .dll
        }
    }
}

unix|win32-g++ {
    QMAKE_CXXFLAGS_WARN_OFF -= -w
    QMAKE_CXXFLAGS += -Wall
    QMAKE_CXXFLAGS += \
        -Wno-unknown-pragmas
}

msvc {
    QMAKE_CXXFLAGS_WARN_OFF -= -W0
    QMAKE_CXXFLAGS += -W3
    QMAKE_LFLAGS += /NODEFAULTLIB:LIBCMT
}

HEADERS += \
    QCCZImageContainerPlugin.h \
    QCCZImageContainerHandler.h

SOURCES += \
    QCCZImageContainerPlugin.cpp \
    QCCZImageContainerHandler.cpp


OTHER_FILES += ccz.json

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QCCZImageContainerPlugin

DISTFILES += CHANGELOG
