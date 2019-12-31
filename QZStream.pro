TEMPLATE = subdirs
SUBDIRS = \
    lib \
    ccz_imageformat_plugin

lib.file = lib/QZStream.pro
ccz_imageformat_plugin.depends = lib

!emscripten {
    SUBDIRS += tests
    tests.file = tests/QZStreamTests.pro
    tests.depends = ccz_imageformat_plugin
}
