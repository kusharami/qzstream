unix|win32-g++ {
     LIBS += -lz
} else {
    win32 {
        INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib
    }
}

DEFINES += ZLIB_CONST
