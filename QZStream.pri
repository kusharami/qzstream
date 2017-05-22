unix|win32-g++ {
    QMAKE_CXXFLAGS_WARN_OFF -= -w
    QMAKE_CXXFLAGS += -Wall

    LIBS += -lz
}

win32 {
    QMAKE_CXXFLAGS_WARN_OFF -= -W0
    QMAKE_CXXFLAGS += -W3

    INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib

    LIBS += -lWs2_32
}

