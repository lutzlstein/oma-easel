QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = easel
TEMPLATE = app

HEADERS += \
    src/paths.h \
    src/themelibrary.h \
    src/screens.h \
    src/promptbuilder.h \
    src/generator.h \
    src/codexgenerator.h \
    src/library.h \
    src/backends.h \
    src/portalfilepicker.h \
    src/studio.h \
    src/theme.h \
    src/translator.h \
    src/cli.h

SOURCES += \
    src/main.cpp \
    src/paths.cpp \
    src/themelibrary.cpp \
    src/screens.cpp \
    src/promptbuilder.cpp \
    src/codexgenerator.cpp \
    src/library.cpp \
    src/backends.cpp \
    src/portalfilepicker.cpp \
    src/studio.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/cli.cpp

RESOURCES += src/resources.qrc
