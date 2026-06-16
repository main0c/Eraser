# ---------------------------------------------------------
# Qt Protobuf 3.5.18 Common Settings
# ---------------------------------------------------------

CONFIG += c++11

# 1. Include Paths
INCLUDEPATH += $$PWD

# 2. Protocol Buffers Configuration

# 4. Common Sources & Headers
SOURCES += $$PWD/ClientManager.cpp \
           $$PWD/ErasureTaskManager.cpp \
           $$PWD/TaskWorkerThread.cpp

HEADERS += $$PWD/ClientManager.h \
           $$PWD/ErasureTaskManager.h \
           $$PWD/TaskWorkerThread.h
