include(QZStream.pri)

win32-msvc*:PRE_TARGETDEPS += $$QZSTREAM_LIB_DIR/QZStream.lib
linux|macx:PRE_TARGETDEPS += $$QZSTREAM_LIB_DIR/libQZStream.a

LIBS += -L$$QZSTREAM_LIB_DIR
LIBS += -lQZStream
