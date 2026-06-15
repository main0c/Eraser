# ---------------------------------------------------------
# Qt Protobuf 3.5.18 Common Settings
# ---------------------------------------------------------

CONFIG += c++11

# 1. Include Paths
INCLUDEPATH += $$PWD

# 2. Protocol Buffers Configuration

# Specify protoc executable path (Windows)
win32 {
    CONFIG(debug, debug|release) {
        PROTOBUF_ROOT = $$PWD/../3rd/protobuf_build/build_debug/install/x86-windows
    } else {
        PROTOBUF_ROOT = $$PWD/../3rd/protobuf_build/build_release/install/x86-windows/
    }
    PROTOC = $$PROTOBUF_ROOT/bin/protoc.exe
}

unix {
    # Linux specific protobuf settings
    PROTOBUF_ROOT = $$PWD/../3rd/protobuf_build/build_linux_release/install
    PROTOC = $$PROTOBUF_ROOT/bin/protoc
}

message(PROTOC Path: $$PROTOC)
message(PROTOBUF Root: $$PROTOBUF_ROOT)

# Specify .proto file(s)
PROTOS = $$PWD/messages.proto

# Custom compiler: generate .pb.h based on 
# https://vilimpoc.org/blog/2013/06/09/using-google-protocol-buffers-with-qmake/
protobuf_header.name = Google Protobuf Headers
protobuf_header.input = PROTOS
protobuf_header.output = ${QMAKE_FILE_IN_PATH}/${QMAKE_FILE_BASE}.pb.h
protobuf_header.commands = $$PROTOC --cpp_out=${QMAKE_FILE_IN_PATH}/ --proto_path=${QMAKE_FILE_IN_PATH} ${QMAKE_FILE_NAME}
protobuf_header.variable_out = HEADERS
QMAKE_EXTRA_COMPILERS += protobuf_header

# Custom compiler: generate .pb.cc
protobuf_impl.name = Google Protobuf Sources
protobuf_impl.input = PROTOS
protobuf_impl.output = ${QMAKE_FILE_IN_PATH}/${QMAKE_FILE_BASE}.pb.cc
protobuf_impl.depends = ${QMAKE_FILE_IN_PATH}/${QMAKE_FILE_BASE}.pb.h
protobuf_impl.commands = $$PROTOC --cpp_out=${QMAKE_FILE_IN_PATH}/ --proto_path=${QMAKE_FILE_IN_PATH} ${QMAKE_FILE_NAME}
protobuf_impl.variable_out = SOURCES
QMAKE_EXTRA_COMPILERS += protobuf_impl

DEFINES += QT_DEPRECATED_WARNINGS

# 3. Platform Dependencies & Third-party Libraries

win32 {
    # Protobuf Libraries
    CONFIG(debug, debug|release) {
        message("Configuring for Debug build")
        INCLUDEPATH += $$PROTOBUF_ROOT/include
        LIBS += -L$$PROTOBUF_ROOT/lib -llibprotobufd
    } else {
        message("Configuring for Release build")
        INCLUDEPATH += $$PROTOBUF_ROOT/include
        LIBS += -L$$PROTOBUF_ROOT/lib -llibprotobuf
    }

    INCLUDEPATH += $$PROTOBUF_ROOT/include
}

unix {
    # Linux/Unix Libraries
    INCLUDEPATH += $$PROTOBUF_ROOT/include
    LIBS += -L$$PROTOBUF_ROOT/lib -lprotobuf
}

# 4. Common Sources & Headers
SOURCES += $$PWD/MessageHandler.cpp \
           $$PWD/Utils.cpp \
           $$PWD/messages.pb.cc

HEADERS += $$PWD/MessageHandler.h \
           $$PWD/Utils.h \
           $$PWD/messages.pb.h

RESOURCES += \
    $${PWD}/common.qrc

!contains(DEFINES, CORE_SERVER) {
    include($${PWD}/task/task.pri)
}
# 5. Clean Rules
QMAKE_CLEAN += $$PWD/messages.pb.cc \
               $$PWD/messages.pb.h

DEFINES += DB_SUPPORT

# 设置子系统和 XP 工具集
win32-msvc2013 {
    QMAKE_LFLAGS_WINDOWS = /SUBSYSTEM:WINDOWS,5.01
    QMAKE_LFLAGS += /SUBSYSTEM:WINDOWS,5.01      # 强制使用 v120_xp 工具集

    QMAKE_CXXFLAGS += /D_USING_V110_SDK71_

    win32-msvc* {
        QMAKE_CXXFLAGS += /D_AFXDLL

        debug {
            QMAKE_CXXFLAGS += /MDd      # Debug 模式使用多线程调试库
        } else {
            QMAKE_CXXFLAGS += /MD       # Release 模式使用多线程库
        }
    }
}


DEFINES += QT_DEPRECATED_WARNINGS \
           ELPP_QT_LOGGING \
           ELPP_NO_DEFAULT_LOG_FILE \
           ELPP_THREAD_SAFE


include($$PWD/../comLib/comLib.pri)
include($$PWD/../comLib/SqlDatabase/SqlDatabase.pri)
