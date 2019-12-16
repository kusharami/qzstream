TEMPLATE = subdirs
SUBDIRS = \
    tests \
    lib \
    ccz_imageformat_plugin

tests.file = tests/QZStreamTests.pro
lib.file = lib/QZStream.pro
ccz_imageformat_plugin.depends = lib

!emscripten {
    tests.depends = ccz_imageformat_plugin
}
