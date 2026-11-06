QT += core gui

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
    src/cli.h

SOURCES += \
    src/main.cpp \
    src/paths.cpp \
    src/themelibrary.cpp \
    src/screens.cpp \
    src/promptbuilder.cpp \
    src/codexgenerator.cpp \
    src/library.cpp \
    src/cli.cpp
