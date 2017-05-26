TEMPLATE = subdirs
SUBDIRS = \
    tests \
    lib 

lib.file = lib/QZStream.pro
tests.depends = lib
