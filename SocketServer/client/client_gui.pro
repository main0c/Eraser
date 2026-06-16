# Eraser GUI Client Project

QT      += core gui widgets network
CONFIG  += c++11
TARGET  = SocketEraserClient
TEMPLATE = app

VERSION = 1.0.0
DEFINES += APP_VERSION="$$VERSION"

# Output directories
CONFIG(debug, debug|release) {
    DESTDIR = ../bin_client_debug
} else {
    DESTDIR = ../bin_client
}
DEFINES += ZMQ_CLIENT

# Use common Protobuf configuration
include($$PWD/../common/common.pri)
include($$PWD/../manager/manager.pri)
include($${PWD}/../widgets/widgets.pri)

unix {
    ZMQ_ROOT = $$PWD/../3rd/libzmq_build/build_linux_release
    INCLUDEPATH += $$ZMQ_ROOT/include
    LIBS += -L$$ZMQ_ROOT/lib -lzmq
 

    # C++11 flags for Unix
    QMAKE_CXXFLAGS += -std=c++11

    # Install target on Unix
    target.path = /usr/local/bin
    INSTALLS += target
}

# Windows dependency configuration

win32 {
    # MSVC C++11 flag
    msvc* {
        QMAKE_CXXFLAGS += /std:c++11
    }

    CONFIG(debug, debug|release) {
        ZMQ_ROOT = $$PWD/../3rd/libzmq_build/build_debug/install
    } else {
        ZMQ_ROOT = $$PWD/../3rd/libzmq_build/build_release/install
    }

    INCLUDEPATH += $$ZMQ_ROOT/include
    LIBS += -L$$ZMQ_ROOT/lib

    CONFIG(debug, debug|release) {
        message("Configuring for Debug build")
        LIBS += -llibzmq-v140-mt-gd-4_3_4
    } else {
        message("Configuring for Release build")
        LIBS += -llibzmq-v140-mt-4_3_4
    }

    ZERO_BIN_DIR = $$shell_path($$ZMQ_ROOT/bin)

    CONFIG(debug, debug|release) {
        COPY_DLLS = libzmq-v140-mt-gd-4_3_4.dll
    } else {
        COPY_DLLS = libzmq-v140-mt-4_3_4.dll
    }

    LOCAL_BIN_DIR = $$shell_path($$DESTDIR)

    for (dll, COPY_DLLS) {
        DLL_SRC = $$shell_path($$ZERO_BIN_DIR/$${dll})
        DLL_DST = $$shell_path($$LOCAL_BIN_DIR/$${dll})

        message([DLL_DST] $$DLL_DST)
        message([DLL_SRC] $$DLL_SRC)

        system(copy /Y "$$DLL_SRC" "$$DLL_DST")
    }
}

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

# Clean rules
QMAKE_CLEAN += \
    $${DESTDIR}/* \
    $$PWD/../build/client_gui/*
