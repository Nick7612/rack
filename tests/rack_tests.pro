QT += core gui qml quick quickcontrols2 network multimedia testlib

CONFIG += c++17 testcase
TARGET = rack_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/game.h \
    ../src/gameclient.h \
    ../src/hostserver.h \
    ../src/omarchy.h \
    ../src/physics.h \
    ../src/protocol.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    rack_tests.cpp \
    ../src/game.cpp \
    ../src/gameclient.cpp \
    ../src/hostserver.cpp \
    ../src/omarchy.cpp \
    ../src/physics.cpp \
    ../src/protocol.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp
