# Dashboard Project - Pure UI frontend

QT += core gui network widgets

CONFIG += c++11
TARGET = Dashboard
TEMPLATE = app

VERSION = 1.0.0
DEFINES += APP_VERSION="$$VERSION"

# Output directories
CONFIG(debug, debug|release) {
    DESTDIR = ../bin_dashboard_debug
} else {
    DESTDIR = ../bin_dashboard
}

DEFINES += DASHBOARD_APP

# Use common Protobuf configuration
include($$PWD/../common/common.pri)
include($$PWD/../manager/manager.pri)
include($${PWD}/../widgets/widgets.pri)
unix {
    LIBS += -lzmq
    LIBS += -lprotobuf
    INCLUDEPATH += /usr/local/include

    QMAKE_CXXFLAGS += -std=c++11

    target.path = /usr/local/bin
    INSTALLS += target
}

win32 {
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
    $${PWD}/main.cpp \
    $${PWD}/DashboardWindow.cpp \
    $${PWD}/ServerCoreConnector.cpp

HEADERS += \
    $${PWD}/DashboardWindow.h \
    $${PWD}/ServerCoreConnector.h

FORMS += \
    $${PWD}/DashboardWindow.ui

CONFIG(debug, debug|release) {
    TARGET = $$join(TARGET,,,_debug)
    DEFINES += DEBUG_MODE
}

CONFIG(release, debug|release) {
    DEFINES += RELEASE_MODE
}

QMAKE_CLEAN += \
    $${DESTDIR}/* \
    $$PWD/../build/dashboard/*