QT += core gui qml quick quickcontrols2 network multimedia

CONFIG += c++17 release
TARGET = rack
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/discovery.h \
    src/game.h \
    src/gameclient.h \
    src/hostserver.h \
    src/omarchy.h \
    src/physics.h \
    src/protocol.h \
    src/sound.h \
    src/tableview.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/backend.cpp \
    src/discovery.cpp \
    src/game.cpp \
    src/gameclient.cpp \
    src/hostserver.cpp \
    src/main.cpp \
    src/omarchy.cpp \
    src/physics.cpp \
    src/protocol.cpp \
    src/sound.cpp \
    src/tableview.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
