include(QZStream.pri)

msvc:PRE_TARGETDEPS += $$QZSTREAM_LIB_DIR/QZStream.lib
else:PRE_TARGETDEPS += $$QZSTREAM_LIB_DIR/libQZStream.a

LIBS += -L$$QZSTREAM_LIB_DIR
LIBS += -lQZStream

INCLUDEPATH += $$PWD/lib
