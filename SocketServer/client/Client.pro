# Eraser GUI Client Project

QT      += core gui widgets network
CONFIG  += c++11
TARGET  = EraserClient
TEMPLATE = app

VERSION = 1.0.0
DEFINES += APP_VERSION="$$VERSION"

# Output directories
CONFIG(debug, debug|release) {
    DESTDIR = ../bin_debug
} else {
    DESTDIR = ../bin
}
DEFINES += ZMQ_CLIENT

# Use common Protobuf configuration
include($$PWD/../common/common.pri)
include($$PWD/../common/manager/manager.pri)
include($$PWD/../common/widgets/widgets.pri)


SOURCES += \
    $$PWD/main_gui.cpp \
    $$PWD/ZMQClient.cpp \
    $$PWD/ProgressDisplayWidget.cpp \
    $$PWD/ConnectionControlWidget.cpp \
    $$PWD/ClientMainWindow.cpp

HEADERS += \
    $$PWD/ZMQClient.h \
    $$PWD/ProgressDisplayWidget.h \
    $$PWD/ConnectionControlWidget.h \
    $$PWD/ClientMainWindow.h

# Configuration-specific settings
CONFIG(debug, debug|release) {
    TARGET = $$join(TARGET,,,_debug)
    DEFINES += DEBUG_MODE
    CONFIG += console
}

CONFIG(release, debug|release) {
    DEFINES += RELEASE_MODE
}

QMAKE_CLEAN += \
    $${DESTDIR}/*
