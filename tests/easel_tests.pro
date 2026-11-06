QT += core gui testlib dbus network
CONFIG += c++17 testcase
TARGET = easel_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/paths.h ../src/themelibrary.h ../src/screens.h ../src/promptbuilder.h \
           ../src/generator.h ../src/codexgenerator.h ../src/library.h ../src/backends.h ../src/settings.h ../src/keystore.h ../src/apigenerators.h \
           ../src/portalfilepicker.h ../src/previewsession.h ../src/studio.h ../src/theme.h ../src/translator.h
SOURCES += easel_tests.cpp ../src/paths.cpp ../src/themelibrary.cpp ../src/screens.cpp \
           ../src/promptbuilder.cpp ../src/codexgenerator.cpp ../src/library.cpp ../src/backends.cpp ../src/settings.cpp ../src/keystore.cpp ../src/apigenerators.cpp \
           ../src/portalfilepicker.cpp ../src/previewsession.cpp ../src/studio.cpp ../src/theme.cpp ../src/translator.cpp
