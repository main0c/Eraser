#include "ZMQClient.h"
#include <QDebug>
#include <QDateTime>
#include "Utils.h"
#include <QUuid>

ZMQClient::ZMQClient(const QString& clientId, const QString& serverAddress, QObject *parent)
    : QObject(parent)
    , m_clientId(clientId)
    , m_serverAddress(serverAddress)
    , m_context(nullptr)
    , m_socket(nullptr)
    , m_connected(false)
    , m_messageHandler(nullptr)
    , m_heartbeatTimer(nullptr)
    , m_currentRequestId(0)
{
    m_messageHandler = new MessageHandler(this);
}

ZMQClient::~ZMQClient()
{
    disconnectFromServer();
    delete m_messageHandler;
}

bool ZMQClient::initializeZMQ()
{
    try {
        m_context = zmq_ctx_new();
        if (!m_context) {
            emit errorOccurred("创建ZMQ context失败");
            return false;
        }
        
        m_socket = zmq_socket(m_context, ZMQ_REQ);
        if (!m_socket) {
            emit errorOccurred("创建ZMQ socket失败");
            cleanupZMQ();
            return false;
        }
        
        int timeout = SOCKET_TIMEOUT;
        zmq_setsockopt(m_socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        zmq_setsockopt(m_socket, ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
        
        int rc = zmq_connect(m_socket, m_serverAddress.toStdString().c_str());
        if (rc != 0) {
            emit errorOccurred(QString("连接服务器失败: %1").arg(zmq_strerror(zmq_errno())));
            cleanupZMQ();
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        emit errorOccurred(QString("初始化ZMQ异常: %1").arg(e.what()));
        cleanupZMQ();
        return false;
    }
}

void ZMQClient::cleanupZMQ()
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

bool ZMQClient::connectToServer()
{
    if (m_connected) {
        return true;
    }
    
    if (initializeZMQ()) {
        m_connected = true;
        emit connected();
        emit clientStarted();
        logMessage("已连接到服务器");
        return true;
    }
    
    return false;
}

void ZMQClient::disconnectFromServer()
{
    if (!m_connected) {
        return;
    }
    
    stopHeartbeat();
    
    ClientInfo clientInfo;
    clientInfo.set_client_id(m_clientId.toStdString());
    sendClientDisconnect(clientInfo);
    
    m_connected = false;
    cleanupZMQ();
    
    emit disconnected();
    emit clientStopped();
    logMessage("已断开服务器连接");
}

bool ZMQClient::isConnected() const
{
    return m_connected;
}

void ZMQClient::setServerAddress(QString serverAddress)
{
    m_serverAddress = serverAddress;
}

bool ZMQClient::sendClientRegister(const ClientInfo& clientInfo)
{
    if (!m_connected) {
        logMessage("未连接，无法发送注册消息");
        return false;
    }
    
    Message message = m_messageHandler->createClientRegisterMessage(clientInfo);
    // 生成新的请求ID
    m_currentRequestId = generateRequestId();
    message.set_request_id(m_currentRequestId);
    
    bool result = sendMessage(message);
    
    if (result) {
        emit messageSent("CLIENT_REGISTER");
        logMessage("客户端注册消息已发送");
    }
    
    return result;
}

bool ZMQClient::sendDiskInfo(const ClientInfo& clientInfo, const QList<DiskInfo>& diskList)
{
    if (!m_connected) {
        logMessage("未连接，无法发送磁盘信息");
        return false;
    }
    
    Message message = m_messageHandler->createDiskInfoMessage(diskList);
    m_currentRequestId = generateRequestId();
    message.set_request_id(m_currentRequestId);
    message.mutable_client_info()->CopyFrom(clientInfo);
    qDebug() << QString::fromStdString(clientInfo.server_client_id());
    bool result = sendMessage(message);
    
    if (result) {
        emit messageSent("DISK_INFO");
        logMessage(QString("磁盘信息已发送 (%1 个磁盘)").arg(diskList.size()));
    }
    
    return result;
}

bool ZMQClient::sendErasureTask(const ClientInfo& clientInfo, const ErasureTask& taskInfo)
{
    if (!m_connected) {
        logMessage("未连接，无法发送擦除任务信息");
        return false;
    }
    
    Message message = m_messageHandler->createErasureTaskMessage(clientInfo, taskInfo);
    m_currentRequestId = generateRequestId();
    message.set_request_id(m_currentRequestId);
    
    bool result = sendMessage(message);
    
    if (result) {
        emit messageSent("ERASURE_TASK");
        logMessage(QString("擦除任务信息已发送: %1, 状态: %2")
                   .arg(QString::fromStdString(taskInfo.disk_id()))
                   .arg(Utils::getStatusString(taskInfo.status())));
    }
    
    return result;
}

bool ZMQClient::sendErasureProgress(const ClientInfo& clientInfo, const ErasureProgress& progressInfo)
{
    if (!m_connected) {
        logMessage("未连接，无法发送擦除进度");
        return false;
    }
    
    Message message = m_messageHandler->createErasureProgressMessage(clientInfo, progressInfo);
    m_currentRequestId = generateRequestId();
    message.set_request_id(m_currentRequestId);
    
    bool result = sendMessage(message);
    
    if (result) {
        emit messageSent("ERASURE_PROGRESS");
        logMessage(QString("擦除进度已发送: %1%").arg(progressInfo.progress_percent() / 100.0));
    }
    
    return result;
}

bool ZMQClient::sendErasureComplete(const ClientInfo& clientInfo, const ErasureTask& erasureInfo)
{
    if (!m_connected) {
        logMessage("未连接，无法发送擦除完成消息");
        return false;
    }
    
    Message message = m_messageHandler->createErasureCompleteMessage(clientInfo, erasureInfo);
    m_currentRequestId = generateRequestId();
    message.set_request_id(m_currentRequestId);
    
    bool result = sendMessage(message);
    
    if (result) {
        emit messageSent("ERASURE_COMPLETE");
        logMessage(QString("擦除完成消息已发送: %1").arg(Utils::getStatusString(erasureInfo.status())));
    }
    
    return result;
}

bool ZMQClient::sendHeartbeat(const ClientInfo& clientInfo)
{
    if (!m_connected) {
        return false;
    }
    
    Message message = m_messageHandler->createHeartbeatMessage(clientInfo);
    // 心跳通常不需要严格的请求ID匹配，但为了统一协议也可以设置
    m_currentRequestId = generateRequestId();
    message.set_request_id(m_currentRequestId);
    
    bool result = sendMessage(message);
    
    if (result) {
        emit messageSent("HEARTBEAT");
    }
    
    return result;
}

bool ZMQClient::sendClientDisconnect(const ClientInfo& clientInfo)
{
    if (!m_connected) {
        return false;
    }
    
    Message message = m_messageHandler->createClientDisconnectMessage(clientInfo);
    m_currentRequestId = generateRequestId();
    message.set_request_id(m_currentRequestId);
    
    bool result = sendMessage(message);
    
    if (result) {
        emit messageSent("CLIENT_DISCONNECT");
        logMessage("客户端断开消息已发送");
    }
    
    return result;
}

void ZMQClient::startHeartbeat(int intervalMs)
{
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        delete m_heartbeatTimer;
    }
    
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ZMQClient::sendHeartbeatMessage);
    m_heartbeatTimer->start(intervalMs);
    
    logMessage(QString("心跳已启动 (间隔: %1ms)").arg(intervalMs));
}

void ZMQClient::stopHeartbeat()
{
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        delete m_heartbeatTimer;
        m_heartbeatTimer = nullptr;
        logMessage("心跳已停止");
    }
}

void ZMQClient::sendHeartbeatMessage()
{
    if (!m_connected) {
        return;
    }
    
    ClientInfo clientInfo;
    clientInfo.set_client_id(m_clientId.toStdString());
    clientInfo.set_server_client_id(m_server_clientId.toStdString());

    sendHeartbeat(clientInfo);
}

bool ZMQClient::sendMessage(const Message& message)
{
    if (!m_connected || !m_socket) {
        return false;
    }
    
    try {
        QByteArray data = Utils::serializeMessage(message);
        if (data.isEmpty()) {
            return false;
        }
        qDebug() << QString("[ZMQClient %1] 发送数据大小: %2 bytes").arg(m_clientId).arg(data.size());
        
        int rc = zmq_send(m_socket, data.constData(), data.size(), 0);
        if (rc == -1) {
            logMessage(QString("发送消息失败: %1").arg(zmq_strerror(zmq_errno())));
            return false;
        }

        ServerResponse response;
        if (receiveResponse(response)) {
            // 纠错: 处理服务器返回的 client_id (如果存在)
            if (response.type() == ServerResponse_ResponseType_RESPONSE_TYPE_REGISTER) {
               QString newClientId = QString::fromStdString(response.server_client_id());
                if (!newClientId.isEmpty()) {
                    logMessage(QString("服务器分配的新客户端ID: %1").arg(newClientId));
                    m_server_clientId = newClientId;
                    emit resultOfClientRegister(m_server_clientId);
                } else {
                    logMessage(QString("ERROR!!!!!!!!!!!注册标识，%1 服务端未分配ID").arg(m_clientId));
                }
            }

            // 纠错: 验证请求ID是否匹配
            if (response.request_id() != 0 && response.request_id() != m_currentRequestId) {
                logMessage(QString("警告: 响应请求ID (%1) 与发送请求ID (%2) 不匹配")
                           .arg(response.request_id()).arg(m_currentRequestId));
            }

            emit serverResponseReceived(response);
        }
        return true;
    } catch (const std::exception& e) {
        logMessage(QString("发送消息异常: %1").arg(e.what()));
        return false;
    }
}

bool ZMQClient::receiveResponse(ServerResponse& response)
{
    if (!m_socket) {
        return false;
    }
    
    try {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        
        int rc = zmq_msg_recv(&msg, m_socket, 0);
        if (rc == -1) {
            if (zmq_errno() == EAGAIN) {
                // 超时，但不认为是错误
                zmq_msg_close(&msg);
                return false;
            } else {
                logMessage(QString("接收响应失败: %1").arg(zmq_strerror(zmq_errno())));
                zmq_msg_close(&msg);
                return false;
            }
        }
        
        QByteArray responseData(static_cast<char*>(zmq_msg_data(&msg)), zmq_msg_size(&msg));
        zmq_msg_close(&msg);
        
        response = Utils::deserializeResponse(responseData);

        logMessage(QString("收到服务器响应 [ReqID:%1]: %2 - %3")
                   .arg(response.request_id())
                   .arg(QString::fromStdString(response.message()))
                   .arg(response.success() ? "成功" : "失败"));
        
        return true;
    } catch (const std::exception& e) {
        logMessage(QString("接收响应异常: %1").arg(e.what()));
        return false;
    }
}

void ZMQClient::logMessage(const QString& message)
{
    qDebug() << QString("[ZMQClient %1] %2").arg(m_clientId, message);
}

int64_t ZMQClient::generateRequestId()
{
    // 使用简单的时间戳+随机数或者自增ID作为请求ID
    // 这里使用 QDateTime 当前毫秒时间戳作为基础，保证唯一性
    return QDateTime::currentMSecsSinceEpoch();
}
