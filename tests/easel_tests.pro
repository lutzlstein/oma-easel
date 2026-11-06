QT += core gui testlib dbus
CONFIG += c++17 testcase
TARGET = easel_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/paths.h ../src/themelibrary.h ../src/screens.h ../src/promptbuilder.h \
           ../src/generator.h ../src/codexgenerator.h ../src/library.h ../src/backends.h \
           ../src/portalfilepicker.h ../src/studio.h ../src/theme.h ../src/translator.h
SOURCES += easel_tests.cpp ../src/paths.cpp ../src/themelibrary.cpp ../src/screens.cpp \
           ../src/promptbuilder.cpp ../src/codexgenerator.cpp ../src/library.cpp ../src/backends.cpp \
           ../src/portalfilepicker.cpp ../src/studio.cpp ../src/theme.cpp ../src/translator.cpp
