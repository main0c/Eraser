#include "ClientToServerBridge.h"
#include <QDateTime>
#include <QDebug>
#include <QByteArray>

ClientToServerBridge::ClientToServerBridge(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_socket(nullptr)
    , m_running(false)
    , m_clientManager(nullptr)
    , m_taskManager(nullptr)
    , m_messageTimer(nullptr)
    , m_heartbeatTimer(nullptr)
{
}

ClientToServerBridge::~ClientToServerBridge()
{
    stop();
    cleanupZMQ();
}

bool ClientToServerBridge::start(const QString& bindAddress)
{
    if (m_running) {
        return true;
    }

    m_bindAddress = bindAddress;
    initializeZMQ();

    m_running = true;

    // 启动消息处理定时器
    m_messageTimer = new QTimer(this);
    connect(m_messageTimer, &QTimer::timeout, this, &ClientToServerBridge::processMessages);
    m_messageTimer->start(100); // 每100ms检查一次消息

    // 启动心跳检查定时器
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ClientToServerBridge::checkClientHeartbeats);
    m_heartbeatTimer->start(HEARTBEAT_INTERVAL);

    emit bridgeStarted();
    qDebug() << "ClientToServerBridge started on" << bindAddress;
    return true;
}

void ClientToServerBridge::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_messageTimer) {
        m_messageTimer->stop();
        delete m_messageTimer;
        m_messageTimer = nullptr;
    }

    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        delete m_heartbeatTimer;
        m_heartbeatTimer = nullptr;
    }

    cleanupZMQ();
    emit bridgeStopped();
    qDebug() << "ClientToServerBridge stopped";
}

bool ClientToServerBridge::isRunning() const
{
    return m_running;
}

void ClientToServerBridge::setClientManager(ClientManager* manager)
{
    m_clientManager = manager;
}

void ClientToServerBridge::setTaskManager(ErasureTaskManager* manager)
{
    m_taskManager = manager;
}

void ClientToServerBridge::initializeZMQ()
{
    m_context = zmq_ctx_new();
    if (!m_context) {
        emit bridgeError("Failed to create ZMQ context");
        return;
    }

    m_socket = zmq_socket(m_context, ZMQ_ROUTER);
    if (!m_socket) {
        emit bridgeError("Failed to create ZMQ socket");
        cleanupZMQ();
        return;
    }

    // 设置socket选项
    int timeout = 100;
    zmq_setsockopt(m_socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    std::string address = m_bindAddress.toStdString();
    if (zmq_bind(m_socket, address.c_str()) != 0) {
        emit bridgeError(QString("Failed to bind to ") + m_bindAddress + ": " + zmq_strerror(errno));
        cleanupZMQ();
        return;
    }

    qDebug() << "ZMQ Router socket bound to" << m_bindAddress;
}

void ClientToServerBridge::cleanupZMQ()
{
    if (m_socket) {
        zmq_close(m_socket);
        m_socket = nullptr;
    }

    if (m_context) {
        zmq_ctx_destroy(m_context);
        m_context = nullptr;
    }
}

void ClientToServerBridge::processMessages()
{
    if (!m_running || !m_socket) {
        return;
    }

    while (true) {
        zmq_msg_t identity;
        zmq_msg_init(&identity);
        
        if (zmq_msg_recv(&identity, m_socket, ZMQ_DONTWAIT) < 0) {
            zmq_msg_close(&identity);
            break;
        }

        // 接收空分隔符
        zmq_msg_t delimiter;
        zmq_msg_init(&delimiter);
        zmq_msg_recv(&delimiter, m_socket, 0);
        zmq_msg_close(&delimiter);

        // 接收消息内容
        zmq_msg_t content;
        zmq_msg_init(&content);
        int size = zmq_msg_recv(&content, m_socket, 0);
        
        if (size < 0) {
            zmq_msg_close(&identity);
            zmq_msg_close(&content);
            break;
        }

        // 解析消息
        QByteArray data(static_cast<char*>(zmq_msg_data(&content)), size);
        Message message;
        if (message.ParseFromArray(data.data(), data.size())) {
            handleMessage(message);
        } else {
            qWarning() << "Failed to parse message";
        }

        zmq_msg_close(&identity);
        zmq_msg_close(&content);
    }
}

bool ClientToServerBridge::handleMessage(const Message& message)
{
    switch (message.type()) {
    case MESSAGE_TYPE_CLIENT_REGISTER:
        return handleClientRegister(message);
    case MESSAGE_TYPE_DISK_INFO:
        handleDiskInfo(message);
        return true;
    case MESSAGE_TYPE_ERASURE_START:
        handleErasureStart(message);
        return true;
    case MESSAGE_TYPE_ERASURE_PROGRESS:
        handleErasureProgress(message);
        return true;
    case MESSAGE_TYPE_ERASURE_COMPLETE:
        handleErasureComplete(message);
        return true;
    case MESSAGE_TYPE_HEARTBEAT:
        handleHeartbeat(message);
        return true;
    case MESSAGE_TYPE_CLIENT_DISCONNECT:
        handleClientDisconnect(message);
        return true;
    default:
        qWarning() << "Unknown message type:" << message.type();
        return false;
    }
}

bool ClientToServerBridge::handleClientRegister(const Message& message)
{
    if (!m_clientManager) {
        return false;
    }

    const ClientInfo& clientInfo = message.client_info();
    QString serverClientId = m_clientManager->registerClient(clientInfo);

    // 发送注册响应
    ServerResponse response;
    response.set_type(ServerResponse::RESPONSE_TYPE_REGISTER);
    response.set_success(true);
    response.set_message("Registration successful");
    response.set_server_client_id(serverClientId.toStdString());
    response.set_request_id(message.request_id());

    sendResponse(QString::fromStdString(clientInfo.client_id()), response);
    return true;
}

void ClientToServerBridge::handleDiskInfo(const Message& message)
{
    if (!m_clientManager) {
        return;
    }

    const ClientInfo& clientInfo = message.client_info();
    const google::protobuf::RepeatedPtrField<DiskInfo>& diskList = message.disk_list();

    std::vector<DiskInfo> disks;
    for (const auto& disk : diskList) {
        disks.push_back(disk);
    }

    // 转换clientInfo为可修改副本
    ClientInfo mutableClientInfo = clientInfo;
    m_clientManager->updateClientDisks(mutableClientInfo, disks);
}

void ClientToServerBridge::handleErasureStart(const Message& message)
{
    if (!m_taskManager) {
        return;
    }

    const ErasureTask& taskInfo = message.task_info();
    
    QString taskId;
    m_taskManager->createTask(
        taskId,
        QString::fromStdString(taskInfo.server_client_id()),
        QString::fromStdString(taskInfo.disk_id()),
        QString::fromStdString(taskInfo.disk_info().model()),
        QString::fromStdString(taskInfo.erasure_method()),
        taskInfo.pass_count(),
        taskInfo.total_bytes(),
        taskInfo.verification_enabled()
    );
}

void ClientToServerBridge::handleErasureProgress(const Message& message)
{
    if (!m_taskManager) {
        return;
    }

    const ErasureProgress& progress = message.progress_info();
    QString taskId = QString::fromStdString(progress.task_id());
    
    m_taskManager->updateTaskProgress(taskId, progress);
}

void ClientToServerBridge::handleErasureComplete(const Message& message)
{
    if (!m_taskManager) {
        return;
    }

    const ErasureTask& taskInfo = message.task_info();
    QString taskId = QString::fromStdString(taskInfo.task_id());
    
    m_taskManager->completeTask(
        taskId,
        taskInfo.status() == COMPLETED,
        taskInfo.verification_passed()
    );
}

void ClientToServerBridge::handleHeartbeat(const Message& message)
{
    if (!m_clientManager) {
        return;
    }

    const ClientInfo& clientInfo = message.client_info();
    QString serverClientId = QString::fromStdString(clientInfo.server_client_id());
    
    m_clientManager->updateHeartbeat(serverClientId);
}

void ClientToServerBridge::handleClientDisconnect(const Message& message)
{
    if (!m_clientManager) {
        return;
    }

    const ClientInfo& clientInfo = message.client_info();
    QString serverClientId = QString::fromStdString(clientInfo.server_client_id());
    
    m_clientManager->unregisterClient(serverClientId, "Client disconnected");
}

void ClientToServerBridge::sendResponse(const QString& clientId, const ServerResponse& response)
{
    if (!m_socket) {
        return;
    }

    // 序列化响应
    std::string serialized;
    response.SerializeToString(&serialized);

    // 发送响应（Router socket需要指定目标identity）
    zmq_msg_t identity;
    zmq_msg_init_size(&identity, clientId.toUtf8().size());
    memcpy(zmq_msg_data(&identity), clientId.toUtf8().data(), clientId.toUtf8().size());
    zmq_msg_send(&identity, m_socket, ZMQ_SNDMORE);
    zmq_msg_close(&identity);

    // 空分隔符
    zmq_msg_t delimiter;
    zmq_msg_init_size(&delimiter, 0);
    zmq_msg_send(&delimiter, m_socket, ZMQ_SNDMORE);
    zmq_msg_close(&delimiter);

    // 消息内容
    zmq_msg_t content;
    zmq_msg_init_size(&content, serialized.size());
    memcpy(zmq_msg_data(&content), serialized.data(), serialized.size());
    zmq_msg_send(&content, m_socket, 0);
    zmq_msg_close(&content);
}

void ClientToServerBridge::checkClientHeartbeats()
{
    if (!m_clientManager) {
        return;
    }

    QStringList timeoutClients = m_clientManager->checkHeartbeatTimeout(CLIENT_TIMEOUT);
    for (const QString& clientId : timeoutClients) {
        m_clientManager->markClientOffline(clientId, "Heartbeat timeout");
    }
}