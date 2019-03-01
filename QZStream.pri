CONFIG(debug, debug|release) {
    QZSTREAM_CONFIG_DIR = Debug
} else {
    QZSTREAM_CONFIG_DIR = Release
}

QZSTREAM_LIB_DIR = $$PWD/build/$$QZSTREAM_CONFIG_DIR

unix:LIBS += -lz
win32:INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib

DEFINES += ZLIB_CONST
