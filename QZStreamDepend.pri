include(QZStream.pri)

msvc:PRE_TARGETDEPS += $$QZSTREAM_BIN_DIR/QZStream.lib
else:PRE_TARGETDEPS += $$QZSTREAM_BIN_DIR/libQZStream.a

LIBS += -L$$QZSTREAM_BIN_DIR
LIBS += -lQZStream

INCLUDEPATH += $$PWD/lib

CONFIG(static, static) {
    DEFINES += CCZ_IMAGEFORMAT_STATIC
    LIBS += -lqcczimagecontainer
}