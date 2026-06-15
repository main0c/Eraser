#include "ServerCoreApplication.h"
#include <QDateTime>
#include <QDebug>

ServerCoreApplication::ServerCoreApplication(QObject *parent)
    : QObject(parent)
    , m_clientManager(nullptr)
    , m_taskManager(nullptr)
    , m_clientBridge(nullptr)
    , m_dashboardBridge(nullptr)
    , m_cleanupTimer(nullptr)
    , m_initialized(false)
{
}

ServerCoreApplication::~ServerCoreApplication()
{
    stop();
}

bool ServerCoreApplication::initialize()
{
    if (m_initialized) {
        return true;
    }

    qDebug() << "Initializing ServerCore...";

    // 创建业务管理器
    m_clientManager = new ClientManager(this);
    if (!m_clientManager) {
        qCritical() << "Failed to create ClientManager";
        return false;
    }

    m_taskManager = new ErasureTaskManager(this);
    if (!m_taskManager) {
        qCritical() << "Failed to create ErasureTaskManager";
        delete m_clientManager;
        m_clientManager = nullptr;
        return false;
    }

    // 创建通信桥接器
    m_clientBridge = new ClientToServerBridge(this);
    if (!m_clientBridge) {
        qCritical() << "Failed to create ClientToServerBridge";
        delete m_taskManager;
        delete m_clientManager;
        m_taskManager = nullptr;
        m_clientManager = nullptr;
        return false;
    }
    m_clientBridge->setClientManager(m_clientManager);
    m_clientBridge->setTaskManager(m_taskManager);

    m_dashboardBridge = new DashboardToServerBridge(this);
    if (!m_dashboardBridge) {
        qCritical() << "Failed to create DashboardToServerBridge";
        delete m_clientBridge;
        delete m_taskManager;
        delete m_clientManager;
        m_clientBridge = nullptr;
        m_taskManager = nullptr;
        m_clientManager = nullptr;
        return false;
    }
    m_dashboardBridge->setClientManager(m_clientManager);
    m_dashboardBridge->setTaskManager(m_taskManager);

    // 连接信号槽
    connect(m_clientManager, &ClientManager::clientRegistered,
            this, &ServerCoreApplication::onClientConnected);
    connect(m_clientManager, &ClientManager::clientUnregistered,
            this, &ServerCoreApplication::onClientDisconnected);
    connect(m_clientManager, &ClientManager::clientDisksUpdated,
            this, &ServerCoreApplication::onDiskInfoReceived);

    connect(m_taskManager, &ErasureTaskManager::taskCreated,
            this, &ServerCoreApplication::onTaskCreated);
    connect(m_taskManager, &ErasureTaskManager::taskProgressUpdated,
            this, &ServerCoreApplication::onTaskProgressUpdated);
    connect(m_taskManager, &ErasureTaskManager::taskStatusChanged,
            this, &ServerCoreApplication::onTaskStatusChanged);
    connect(m_taskManager, &ErasureTaskManager::taskCompleted,
            this, &ServerCoreApplication::onTaskCompleted);

    // 设置定时清理
    m_cleanupTimer = new QTimer(this);
    if (!m_cleanupTimer) {
        qCritical() << "Failed to create cleanup timer";
        // Cleanup other resources if necessary, though QTimer failure is rare
        return false;
    }
    connect(m_cleanupTimer, &QTimer::timeout, [this]() {
        if (m_clientManager) {
            m_clientManager->cleanupOldRecords(7);
        }
    });
    m_cleanupTimer->start(3600000); // 每小时清理一次

    m_initialized = true;
    qDebug() << "ServerCore initialized successfully";
    return true;
}

void ServerCoreApplication::start()
{
    if (!m_initialized) {
        qWarning() << "ServerCore not initialized!";
        return;
    }

    if (!m_clientBridge || !m_dashboardBridge) {
        qCritical() << "Bridges are not initialized!";
        return;
    }

    qDebug() << "Starting ServerCore...";

    // 启动客户端通信服务（监听客户端连接）
    if (!m_clientBridge->start("tcp://*:5555")) {
        qCritical() << "Failed to start client bridge";
        return;
    }

    // 启动Dashboard通信服务（监听Dashboard连接）
    if (!m_dashboardBridge->start("tcp://*:5556")) {
        qCritical() << "Failed to start dashboard bridge";
        // Attempt to stop the already started client bridge to maintain consistency
        m_clientBridge->stop();
        return;
    }

    qDebug() << "ServerCore started successfully";
    qDebug() << "Client endpoint: tcp://*:5555";
    qDebug() << "Dashboard endpoint: tcp://*:5556";
}

void ServerCoreApplication::stop()
{
    qDebug() << "Stopping ServerCore...";

    if (m_clientBridge) {
        m_clientBridge->stop();
    }

    if (m_dashboardBridge) {
        m_dashboardBridge->stop();
    }

    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
    }

    m_initialized = false;
    qDebug() << "ServerCore stopped";
}

void ServerCoreApplication::onClientConnected(const QString& serverClientId, const ClientInfo& clientInfo)
{
    if (!m_dashboardBridge) {
        qWarning() << "Dashboard bridge is null, cannot notify client connected";
        return;
    }
    qDebug() << "Client connected:" << serverClientId << "(" << QString::fromStdString(clientInfo.hostname()) << ")";
    
    // 通知所有已连接的Dashboard
    m_dashboardBridge->notifyClientConnected(serverClientId, clientInfo);
}

void ServerCoreApplication::onClientDisconnected(const QString& serverClientId, const QString& reason)
{
    if (!m_dashboardBridge) {
        qWarning() << "Dashboard bridge is null, cannot notify client disconnected";
        return;
    }
    qDebug() << "Client disconnected:" << serverClientId << "Reason:" << reason;
    
    // 通知所有已连接的Dashboard
    m_dashboardBridge->notifyClientDisconnected(serverClientId, reason);
}

void ServerCoreApplication::onDiskInfoReceived(const QString& serverClientId, int diskCount)
{
    if (!m_clientManager || !m_dashboardBridge) {
        qWarning() << "Manager or Bridge is null, cannot process disk info";
        return;
    }
    qDebug() << "Disk info received from client:" << serverClientId << "Disk count:" << diskCount;
    
    // 通知Dashboard
    auto clientData = m_clientManager->getClientData(serverClientId);
    m_dashboardBridge->notifyDiskUpdated(serverClientId, clientData.diskList);
}

void ServerCoreApplication::onErasureProgressUpdated(const QString& clientId, const ErasureProgress& progress)
{
    if (!m_dashboardBridge) {
        qWarning() << "Dashboard bridge is null, cannot notify task progress";
        return;
    }
    // 通知Dashboard任务进度更新
    m_dashboardBridge->notifyTaskProgress(clientId, progress);
}

void ServerCoreApplication::onErasureCompleted(const QString& clientId, const QString& diskId, bool success)
{
    qDebug() << "Erasure completed for client:" << clientId << "disk:" << diskId 
             << "success:" << success;
}

void ServerCoreApplication::onTaskCreated(const QByteArray& taskData)
{
    if (!m_dashboardBridge) {
        qWarning() << "Dashboard bridge is null, cannot notify task created";
        return;
    }
    // 通知Dashboard新任务创建
    m_dashboardBridge->notifyTaskCreated(taskData);
}

void ServerCoreApplication::onTaskProgressUpdated(const QByteArray& taskData)
{
    if (!m_dashboardBridge) {
        qWarning() << "Dashboard bridge is null, cannot notify task progress update";
        return;
    }
    // 通知Dashboard任务进度更新
    m_dashboardBridge->notifyTaskProgressUpdate(taskData);
}

void ServerCoreApplication::onTaskStatusChanged(const QByteArray& taskData)
{
    if (!m_dashboardBridge) {
        qWarning() << "Dashboard bridge is null, cannot notify task status changed";
        return;
    }
    // 通知Dashboard任务状态变化
    m_dashboardBridge->notifyTaskStatusChanged(taskData);
}

void ServerCoreApplication::onTaskCompleted(const QByteArray& taskData)
{
    if (!m_dashboardBridge) {
        qWarning() << "Dashboard bridge is null, cannot notify task completed";
        return;
    }
    // 通知Dashboard任务完成
    m_dashboardBridge->notifyTaskCompleted(taskData);
}