INCLUDEPATH += $$PWD

# Source files
SOURCES += \
    $$PWD/TaskListWidget.cpp \
    $$PWD/DiskInfoWidget.cpp \
    $$PWD/LogWidget.cpp

# Header files
HEADERS += \
    $$PWD/TaskListWidget.h \
    $$PWD/DiskInfoWidget.h \
    $$PWD/LogWidget.h

# Conditional compilation for ZMQ_SERVER or DASHBOARD_APP
contains(DEFINES, ZMQ_SERVER) | contains(DEFINES, DASHBOARD_APP) {
    SOURCES += $$PWD/ClientInfoWidget.cpp
    HEADERS += $$PWD/ClientInfoWidget.h
}

# Conditional compilation: Include ErasureControlWidget if ZMQ_SERVER is NOT defined
!contains(DEFINES, ZMQ_SERVER) {
    SOURCES += $$PWD/ErasureControlWidget.cpp
    HEADERS += $$PWD/ErasureControlWidget.h
}
