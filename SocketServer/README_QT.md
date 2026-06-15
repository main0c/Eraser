# Socket Eraser Server (Qt .pro 版本)

局域网多客户端磁盘擦除管理系统

基于 Qt5.6+、C++11、libzmq 和 Protocol Buffers 的磁盘擦除服务器应用程序，使用 Qt `.pro` 项目文件进行构建管理。

## 系统组件

| 可执行文件 | 描述 | 角色 |
| :--- | :--- | :--- |
| `ServerCore.exe` | 核心服务进程 | 常驻后台，处理 ZMQ 通信、任务调度与数据存储 |
| `Dashboard.exe` | 管理界面 | Qt Widgets GUI，用于监控状态、下发任务 |
| `ClientAgent.exe` | 客户端代理 | 运行在被控 PC 上，接收指令并协调擦除操作 |
| `EraseEngine.exe` | 擦除引擎 | 执行具体的磁盘扇区写入/擦除逻辑（当前为 Demo 模拟） |

## 模块架构

### Server 端 (PC1)
ServerCore
│
├── Dashboard (Qt UI)       # 用户交互界面
├── ZmqServer               # ZeroMQ 服务端 socket 管理
├── TaskManager             # 任务队列与状态机
├── DeviceManager           # 在线客户端设备管理
├── ClientManager           # 客户端连接会话管理
├── Repository              # 数据持久化层 (SQLite/JSON)
└── Scheduler               # 定时任务调度器

### Agent 端 (局域网客户端 PC)
ClientAgent
│
├── ZmqClient               # ZeroMQ 客户端 socket 管理
├── DiskScanner             # 磁盘信息扫描与上报
├── SmartCollector          # SMART 信息收集
└── Watchdog                # 进程守护与健康检查

## 架构图

graph TD
    subgraph Server_Side [Server Side (PC1)]
        UI[Dashboard<br/>Qt Widgets UI]
        Core[ServerCore<br/>Main Process]
        ZMQ_S[ZMQ Server<br/>TCP:5555]
        DB[(Database<br/>SQLite)]
        
        UI <--> Core
        Core <--> ZMQ_S
        Core <--> DB
    end

    subgraph Network [Network (ZMQ + Protobuf)]
        direction LR
        ZMQ_S ===|Protobuf Binary| ZMQ_C1
        ZMQ_S ===|Protobuf Binary| ZMQ_C2
        ZMQ_S ===|Protobuf Binary| ZMQ_C3
    end

    subgraph Client_Side [Client Side (LAN PCs)]
        Agent1[ClientAgent 1]
        Agent2[ClientAgent 2]
        Agent3[ClientAgent 3]
        
        ZMQ_C1((ZMQ Client)) --- Agent1
        ZMQ_C2((ZMQ Client)) --- Agent2
        ZMQ_C3((ZMQ Client)) --- Agent3
    end

*(注：如果无法渲染 Mermaid，请参考以下文本架构图)*

                 ┌─────────────┐
                 │  Dashboard  │
                 │  (Qt UI)    │
                 └──────┬──────┘
                        │ IPC / Direct Call
                        ▼
        ┌──────────────────────────────────┐
        │         ServerCore               │
        │                                  │
        │  ┌──────────┐  ┌──────────────┐  │
        │  │ ZmqServer│  │ TaskManager  │  │
        │  └────┬─────┘  └──────────────┘  │
        │       │                           │
        │  ┌────┴─────┐  ┌──────────────┐  │
        │  │ClientMgr │  │ Repository   │  │
        │  └──────────┘  └──────────────┘  │
        └──────────┬───────────────────────┘
                   │
                   │ ZMQ Socket (Protobuf)
                   │
    ┌──────────────┼──────────────┐
    ▼              ▼              ▼
 Client1        Client2        Client3
(Agent)        (Agent)        (Agent)

## 项目结构

SocketEraser/
├── server/                  # 服务端源码
│   ├── server.pro           # Qt 项目文件
│   ├── src/
│   │   ├── main.cpp
│   │   ├── zmqserver/       # ZMQ 服务端实现
│   │   ├── dashboard/       # Qt UI 界面
│   │   ├── manager/         # 任务/设备/客户端管理
│   │   └── repository/      # 数据存储
│   └── include/
├── client/                  # 客户端源码
│   ├── client.pro           # Qt 项目文件
│   ├── src/
│   │   ├── main.cpp
│   │   ├── zmqclient/       # ZMQ 客户端实现
│   │   ├── scanner/         # 磁盘扫描
│   │   └── engine/          # 擦除引擎 (Demo)
│   └── include/
├── proto/                   # Protocol Buffers 定义
│   └── messages.proto
├── scripts/                 # 构建脚本
│   ├── build_qt.sh
│   └── build_qt.bat
├── docs/                    # 文档
└── README_QT.md

## 依赖项

- **Qt**: 5.6 或更高版本 (模块: `core`, `widgets`, `network`)
- **Protocol Buffers**: `libprotobuf`, `protoc` 编译器
- **ZeroMQ**: `libzmq` (4.x 推荐)
- **构建工具**: `qmake`, `make` (Linux/macOS) 或 `nmake/jom` (Windows)
- **编译器**: 支持 C++11 标准的编译器 (GCC 4.8+, Clang 3.3+, MSVC 2015+)

## 快速开始

### 1. 环境准备

确保已安装 Qt 开发环境、ZeroMQ 和 Protocol Buffers。

**Ubuntu/Debian:**
sudo apt-get install qt5-default qt5-qmake libprotobuf-dev protobuf-compiler libzmq3-dev build-essential

**macOS (Homebrew):**
brew install qt5 protobuf zeromq
export PATH=/usr/local/opt/qt5/bin:$PATH

**Windows:**
建议使用 [vcpkg](https://github.com/microsoft/vcpkg) 或手动安装 Qt 和库文件，并确保环境变量配置正确。

### 2. 生成 Protobuf 代码

在项目根目录执行：
protoc --cpp_out=./server/include --cpp_out=./client/include proto/messages.proto
*(注意：根据实际项目结构调整输出路径，通常建议生成到统一的 include 目录或各自源码目录)*

### 3. 构建项目

#### 使用构建脚本 (推荐)

**Linux/macOS:**
chmod +x scripts/build_qt.sh
./scripts/build_qt.sh

**Windows:**
scripts\build_qt.bat

#### 手动构建

**构建服务器 (Server):**
mkdir build_server
cd build_server
qmake ../server/server.pro CONFIG+=release
make -j4  # Linux/macOS
# nmake   # Windows

**构建客户端 (Client):**
mkdir build_client
cd build_client
qmake ../client/client.pro CONFIG+=release
make -j4  # Linux/macOS
# nmake   # Windows

## 运行程序

### 启动服务器

cd build_server
# Linux/macOS
./bin/SocketEraserServer 
# Windows
bin\SocketEraserServer.exe

### 运行客户端 Agent

**单个客户端:**
cd build_client
./bin/ClientAgent --id AGENT_001 --server tcp://192.168.1.100:5555

**模拟多个客户端 (测试用):**
# 启动 5 个模拟客户端，每个间隔 2 秒
./bin/ClientAgent --count 5 --server tcp://192.168.1.100:5555 --delay 2

## 命令行参数

### ClientAgent 参数

| 参数 | 简写 | 描述 | 默认值 |
| :--- | :--- | :--- | :--- |
| `--id` | `-i` | 指定客户端唯一 ID | 自动生成 UUID |
| `--server` | `-s` | 服务器地址 (ZMQ Endpoint) | `tcp://localhost:5555` |
| `--count` | `-c` | 启动模拟客户端数量 | `1` |
| `--delay` | `-d` | 每个客户端启动间隔 (秒) | `0` |
| `--help` | `-h` | 显示帮助信息 | - |
| `--version` | `-v` | 显示版本信息 | - |

## 项目配置详解

### server.pro 关键配置

QT += core widgets network

CONFIG += c++11
CONFIG += release

TARGET = SocketEraserServer
TEMPLATE = app

# 源文件
SOURCES += \
    src/main.cpp \
    src/zmqserver/zmqserver.cpp \
    src/dashboard/mainwindow.cpp \
    # ... 其他源文件

HEADERS += \
    src/zmqserver/zmqserver.h \
    src/dashboard/mainwindow.h \
    # ... 其他头文件

# Protocol Buffers 集成示例 (需配合自定义 makefile 或 pre-build step)
# 这里假设 pb.cc 和 pb.h 已经生成并在 include 路径中
INCLUDEPATH += ./include

# 链接库
LIBS += -lzmq -lprotobuf

# Windows 特定设置
win32 {
    LIBS += -L$$PWD/lib -lzmq -lprotobuf
    DEFINES += WIN32_LEAN_AND_MEAN
}

### client.pro 关键配置

QT += core network

CONFIG += c++11 console
CONFIG -= app_bundle

TARGET = ClientAgent
TEMPLATE = app

SOURCES += src/main.cpp \
           src/zmqclient/zmqclient.cpp \
           # ...

HEADERS += src/zmqclient/zmqclient.h \
           # ...

INCLUDEPATH += ./include

LIBS += -lzmq -lprotobuf

## 开发说明

### 修改消息协议

1. 编辑 `proto/messages.proto`。
2. 重新生成 C++ 代码：
   protoc --cpp_out=./server/include --cpp_out=./client/include proto/messages.proto
3. 重新编译服务器和客户端项目。

### 调试模式

在构建时添加 `debug` 配置：
qmake ../server/server.pro CONFIG+=debug
make
使用 GDB (Linux/macOS) 或 Visual Studio Debugger (Windows) 附加到进程进行调试。

### 清理构建

# 清理服务器构建
rm -rf build_server
# 清理客户端构建
rm -rf build_client
# 清理生成的 Protobuf 文件 (如果需要强制重新生成)
rm -f server/include/messages.pb.* client/include/messages.pb.*

## 故障排除

### 常见问题

1. **`qmake: command not found`**
   - 确保 Qt 的 `bin` 目录已添加到系统 `PATH` 环境变量中。
   - Linux: `export PATH=/opt/Qt/5.6/gcc_64/bin:$PATH` (路径依安装位置而定)。

2. **`protoc: command not found`**
   - 安装 Protocol Buffers 编译器。
   - Ubuntu: `sudo apt-get install protobuf-compiler`
   - Windows: 下载 `protoc.exe` 并将其所在目录加入 `PATH`。

3. **链接错误 (`undefined reference to zmq_*` 或 `google::protobuf::*`)**
   - 确认库文件路径正确。
   - Linux: 检查是否安装了 `-dev` 包 (`libzmq3-dev`, `libprotobuf-dev`)。
   - Windows: 确保 `.lib` 文件路径在 `.pro` 文件的 `LIBS` 中正确配置，且区分 Debug/Release 版本。

4. **运行时找不到 DLL/so**
   - Windows: 将 `zmq.dll`, `libprotobuf.dll` 复制到可执行文件同级目录，或添加到系统 `PATH`。
   - Linux: 运行 `ldd ./SocketEraserServer` 检查缺失库，或使用 `export LD_LIBRARY_PATH=/path/to/libs:$LD_LIBRARY_PATH`。

### 平台特定注意事项

- **Windows**: 如果使用 MSVC，确保 Qt 版本与编译器版本匹配 (例如 Qt 5.6 MSVC2015)。
- **Linux**: 某些发行版可能需要手动指定 Qt5 的 mkspecs，例如 `qmake -spec linux-g++-64`。
- **macOS**: 如果遇到 Qt 插件加载问题，尝试使用 `macdeployqt` 工具打包应用。

## IDE 集成

### Qt Creator
1. 打开 `server/server.pro` 或 `client/client.pro`。
2. Qt Creator 会自动识别项目结构。
3. 在 "Projects" 面板中配置构建套件 (Kit) 和构建步骤。
4. 直接点击运行按钮即可调试。

### Visual Studio (Windows)
1. 安装 **Qt Visual Studio Tools** 扩展。
2. 使用扩展导入 `.pro` 文件，或直接打开包含 `.vcxproj` 的项目（如果已转换）。
3. 配置包含目录和库目录以指向 ZeroMQ 和 Protobuf。

### CLion
1. CLion 原生支持 CMake，对 `.pro` 支持有限。
2. 建议编写 `CMakeLists.txt` 替代 `.pro`，或使用 CLion 的 "Import Project from Sources" 并手动配置构建工具为 `qmake`。

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 联系方式

如有问题、Bug 报告或功能建议，请提交 GitHub Issue 或 Pull Request。
