unix|win32-g++ {
     LIBS += -lz
} else {
    win32 {
        QMAKE_LFLAGS += /NODEFAULTLIB:LIBCMT

        INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib

        LIBS += -lWs2_32
    }
}
