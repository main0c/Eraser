cmake_minimum_required(VERSION 3.5)
project(EraserServerQtZmqProto)

set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 5.6 REQUIRED COMPONENTS Core Widgets)
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/eraser.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

add_executable(eraser_server
    src/main.cpp
    src/mainwindow.cpp
    src/zmqserverworker.cpp
    src/mainwindow.h
    src/zmqserverworker.h
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_include_directories(eraser_server PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(eraser_server PRIVATE
    Qt5::Core
    Qt5::Widgets
    ${Protobuf_LIBRARIES}
    ${ZMQ_LIBRARIES}
)
