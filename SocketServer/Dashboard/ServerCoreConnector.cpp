#include "ServerCoreConnector.h"
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

ServerCoreConnector::ServerCoreConnector(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_socket(nullptr)
    , m_connected(false)
    , m_messageTimer(nullptr)
    , m_connectionTimer(nullptr)
    , m_serverProcess(nullptr)
{
}

ServerCoreConnector::~ServerCoreConnector()
{
    stopServerCore();
    disconnectFromServer();
    cleanupZMQ();
}

bool ServerCoreConnector::connectToServer(const QString& serverAddress)
{
    if (m_connected) {
        return true;
    }

    m_serverAddress = serverAddress;
    initializeZMQ();

    std::string address = serverAddress.toStdString();
    if (zmq_connect(m_socket, address.c_str()) != 0) {
        emit connectionError(QString("Failed to connect to ") + serverAddress);
        cleanupZMQ();
        return false;
    }

    m_connected = true;

    m_messageTimer = new QTimer(this);
    connect(m_messageTimer, &QTimer::timeout, this, &ServerCoreConnector::processMessages);
    m_messageTimer->start(100);

    m_connectionTimer = new QTimer(this);
    connect(m_connectionTimer, &QTimer::timeout, this, &ServerCoreConnector::checkConnection);
    m_connectionTimer->start(RECONNECT_INTERVAL);

    emit connected();
    qDebug() << "Connected to ServerCore at" << serverAddress;
    return true;
}

void ServerCoreConnector::disconnectFromServer()
{
    if (!m_connected) {
        return;
    }

    m_connected = false;

    if (m_messageTimer) {
        m_messageTimer->stop();
        delete m_messageTimer;
        m_messageTimer = nullptr;
    }

    if (m_connectionTimer) {
        m_connectionTimer->stop();
        delete m_connectionTimer;
        m_connectionTimer = nullptr;
    }

    cleanupZMQ();
    emit disconnected();
    qDebug() << "Disconnected from ServerCore";
}

bool ServerCoreConnector::isConnected() const
{
    return m_connected;
}

void ServerCoreConnector::initializeZMQ()
{
    m_context = zmq_ctx_new();
    if (!m_context) {
        emit connectionError("Failed to create ZMQ context");
        return;
    }

    m_socket = zmq_socket(m_context, ZMQ_DEALER);
    if (!m_socket) {
        emit connectionError("Failed to create ZMQ socket");
        cleanupZMQ();
        return;
    }

    // 设置唯一的identity
    QString identity = QUuid::createUuid().toString();
    zmq_setsockopt(m_socket, ZMQ_IDENTITY, identity.toUtf8().data(), identity.toUtf8().size());

    int timeout = 100;
    zmq_setsockopt(m_socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
}

void ServerCoreConnector::cleanupZMQ()
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

void ServerCoreConnector::processMessages()
{
    if (!m_connected || !m_socket) {
        return;
    }

    while (true) {
        zmq_msg_t content;
        zmq_msg_init(&content);
        int size = zmq_msg_recv(&content, m_socket, ZMQ_DONTWAIT);
        
        if (size < 0) {
            zmq_msg_close(&content);
            break;
        }

        QByteArray data(static_cast<char*>(zmq_msg_data(&content)), size);
        Message message;
        if (message.ParseFromArray(data.data(), data.size())) {
            // 判断是通知还是查询响应
            if (message.type() >= MESSAGE_TYPE_NOTIFY_CLIENT_CONNECTED &&
                message.type() <= MESSAGE_TYPE_NOTIFY_TASK_COMPLETED) {
                handleNotification(message);
            } else if (message.type() >= MESSAGE_TYPE_RESPONSE_CLIENTS &&
                       message.type() <= MESSAGE_TYPE_RESPONSE_STATISTICS) {
                handleQueryResponse(message);
            }
        }

        zmq_msg_close(&content);
    }
}

void ServerCoreConnector::handleNotification(const Message& message)
{
    switch (message.type()) {
    case MESSAGE_TYPE_NOTIFY_CLIENT_CONNECTED: {
        const ClientInfo& clientInfo = message.client_info();
        QString serverClientId = QString::fromStdString(clientInfo.server_client_id());
        emit clientConnected(serverClientId, clientInfo);
        break;
    }
    case MESSAGE_TYPE_NOTIFY_CLIENT_DISCONNECTED: {
        const ClientInfo& clientInfo = message.client_info();
        QString serverClientId = QString::fromStdString(clientInfo.server_client_id());
        QString reason = QString::fromStdString(message.user_data());
        emit clientDisconnected(serverClientId, reason);
        break;
    }
    case MESSAGE_TYPE_NOTIFY_DISK_UPDATED: {
        const ClientInfo& clientInfo = message.client_info();
        QString serverClientId = QString::fromStdString(clientInfo.server_client_id());
        
        QList<DiskInfo> disks;
        for (const auto& disk : message.disk_list()) {
            disks.append(disk);
        }
        emit diskUpdated(serverClientId, disks);
        break;
    }
    case MESSAGE_TYPE_NOTIFY_TASK_CREATED: {
        // 从user_data中解析任务数据
        emit taskCreated(message.task_info());
        break;
    }
    case MESSAGE_TYPE_NOTIFY_TASK_PROGRESS: {
        emit taskProgressUpdated(message.progress_info());
        break;
    }
    case MESSAGE_TYPE_NOTIFY_TASK_COMPLETED: {
        emit taskCompleted(message.task_info());
        break;
    }
    default:
        break;
    }
}

void ServerCoreConnector::handleQueryResponse(const Message& message)
{
    const QueryResponse& response = message.query_response();
    
    switch (message.type()) {
    case MESSAGE_TYPE_RESPONSE_CLIENTS: {
        QList<ClientInfo> clients;
        for (const auto& client : response.client_list()) {
            clients.append(client);
        }
        emit clientsReceived(clients, response.online_clients(), response.offline_clients());
        break;
    }
    case MESSAGE_TYPE_RESPONSE_CLIENT_DETAIL: {
        emit clientDetailReceived(response.client_detail());
        break;
    }
    case MESSAGE_TYPE_RESPONSE_DISK_INFO: {
        QList<DiskInfo> disks;
        for (const auto& disk : response.disk_list()) {
            disks.append(disk);
        }
        // 需要从其他地方获取clientId，这里简化处理
        emit diskInfoReceived("", disks);
        break;
    }
    case MESSAGE_TYPE_RESPONSE_TASKS: {
        QList<ErasureTask> tasks;
        for (const auto& task : response.task_list()) {
            tasks.append(task);
        }
        emit tasksReceived(tasks);
        break;
    }
    case MESSAGE_TYPE_RESPONSE_TASK_DETAIL: {
        emit taskDetailReceived(response.task_detail());
        break;
    }
    case MESSAGE_TYPE_RESPONSE_STATISTICS: {
        emit statisticsReceived(
            response.total_clients(),
            response.online_clients(),
            response.offline_clients(),
            response.total_tasks(),
            response.active_tasks(),
            response.completed_tasks(),
            response.failed_tasks()
        );
        break;
    }
    default:
        break;
    }
}

void ServerCoreConnector::sendQuery(const QueryRequest::QueryType& queryType,
                                     const QString& filterClientId,
                                     const QString& filterDiskId,
                                     const QString& filterTaskId)
{
    if (!m_connected || !m_socket) {
        return;
    }

    Message message;
    message.set_type(MESSAGE_TYPE_QUERY_CLIENTS); // 默认类型，会根据queryType调整
    message.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    message.set_source(SOURCE_DASHBOARD);  // 明确标识来源

    QueryRequest* query = message.mutable_query_request();
    query->set_query_id(QUuid::createUuid().toString().toStdString());
    query->set_query_type(queryType);
    
    if (!filterClientId.isEmpty()) {
        query->set_filter_client_id(filterClientId.toStdString());
    }
    if (!filterDiskId.isEmpty()) {
        query->set_filter_disk_id(filterDiskId.toStdString());
    }
    if (!filterTaskId.isEmpty()) {
        query->set_filter_task_id(filterTaskId.toStdString());
    }

    // 根据queryType设置正确的消息类型
    switch (queryType) {
    case QueryRequest::QUERY_ALL_CLIENTS:
        message.set_type(MESSAGE_TYPE_QUERY_CLIENTS);
        break;
    case QueryRequest::QUERY_CLIENT_BY_ID:
        message.set_type(MESSAGE_TYPE_QUERY_CLIENT_DETAIL);
        break;
    case QueryRequest::QUERY_CLIENT_DISKS:
        message.set_type(MESSAGE_TYPE_QUERY_DISK_INFO);
        break;
    case QueryRequest::QUERY_ALL_TASKS:
    case QueryRequest::QUERY_TASKS_BY_CLIENT:
    case QueryRequest::QUERY_TASKS_BY_DISK:
    case QueryRequest::QUERY_ACTIVE_TASKS:
    case QueryRequest::QUERY_COMPLETED_TASKS:
        message.set_type(MESSAGE_TYPE_QUERY_TASKS);
        break;
    case QueryRequest::QUERY_TASK_BY_ID:
        message.set_type(MESSAGE_TYPE_QUERY_TASK_DETAIL);
        break;
    case QueryRequest::QUERY_STATISTICS:
        message.set_type(MESSAGE_TYPE_QUERY_STATISTICS);
        break;
    default:
        break;
    }

    std::string serialized;
    message.SerializeToString(&serialized);

    zmq_msg_t content;
    zmq_msg_init_size(&content, serialized.size());
    memcpy(zmq_msg_data(&content), serialized.data(), serialized.size());
    zmq_msg_send(&content, m_socket, 0);
    zmq_msg_close(&content);
}

void ServerCoreConnector::queryAllClients()
{
    sendQuery(QueryRequest::QUERY_ALL_CLIENTS);
}

void ServerCoreConnector::queryClientDetail(const QString& clientId)
{
    sendQuery(QueryRequest::QUERY_CLIENT_BY_ID, clientId);
}

void ServerCoreConnector::queryDiskInfo(const QString& clientId)
{
    sendQuery(QueryRequest::QUERY_CLIENT_DISKS, clientId);
}

void ServerCoreConnector::queryAllTasks()
{
    sendQuery(QueryRequest::QUERY_ALL_TASKS);
}

void ServerCoreConnector::queryTasksByClient(const QString& clientId)
{
    sendQuery(QueryRequest::QUERY_TASKS_BY_CLIENT, clientId);
}

void ServerCoreConnector::queryTaskDetail(const QString& taskId)
{
    sendQuery(QueryRequest::QUERY_TASK_BY_ID, "", "", taskId);
}

void ServerCoreConnector::queryStatistics()
{
    sendQuery(QueryRequest::QUERY_STATISTICS);
}

void ServerCoreConnector::sendErasureCommand(const QString& clientId, const ErasureTask& task)
{
    if (!m_connected || !m_socket) {
        return;
    }

    // 这里需要ServerCore支持命令转发功能
    // 简化处理：直接发送任务信息，实际项目中可能需要更复杂的命令协议
    Message message;
    message.set_type(MESSAGE_TYPE_ERASURE_START);
    message.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    *message.mutable_task_info() = task;

    std::string serialized;
    message.SerializeToString(&serialized);

    zmq_msg_t content;
    zmq_msg_init_size(&content, serialized.size());
    memcpy(zmq_msg_data(&content), serialized.data(), serialized.size());
    zmq_msg_send(&content, m_socket, 0);
    zmq_msg_close(&content);
}

void ServerCoreConnector::checkConnection()
{
    if (!m_connected) {
        // 尝试重连
        connectToServer(m_serverAddress);
    }
}

bool ServerCoreConnector::startServerCore(const QString& serverPath, const QStringList& arguments)
{
    if (isServerCoreRunning()) {
        qDebug() << "ServerCore is already running.";
        return true;
    }

    if (serverPath.isEmpty()) {
        emit connectionError("ServerCore path is empty.");
        return false;
    }

    m_serverProcess = new QProcess(this);
    connect(m_serverProcess, &QProcess::started, this, [this]() {
        qDebug() << "ServerCore process started.";
    });
    connect(m_serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        qDebug() << "ServerCore process finished with code" << exitCode << "status" << exitStatus;
        m_serverProcess = nullptr;
        // 可选：进程退出后尝试断开连接或重连
        disconnectFromServer();
    });
    connect(m_serverProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        qDebug() << "ServerCore process error:" << error;
        emit connectionError("Failed to start ServerCore process: " + m_serverProcess->errorString());
        m_serverProcess = nullptr;
    });

    m_serverProcess->start(serverPath, arguments);
    
    if (!m_serverProcess->waitForStarted(5000)) {
        emit connectionError("Timeout waiting for ServerCore to start.");
        delete m_serverProcess;
        m_serverProcess = nullptr;
        return false;
    }

    return true;
}
void ServerCoreConnector::stopServerCore()
{
    if (m_serverProcess) {
        if (m_serverProcess->state() != QProcess::NotRunning) {
            m_serverProcess->terminate();
            if (!m_serverProcess->waitForFinished(3000)) {
                m_serverProcess->kill();
                m_serverProcess->waitForFinished();
            }
        }
        delete m_serverProcess;
        m_serverProcess = nullptr;
        qDebug() << "ServerCore process stopped.";
    }
}

bool ServerCoreConnector::isServerCoreRunning() const
{
    // 如果通过本类启动了进程，直接检查
    if (m_serverProcess && m_serverProcess->state() != QProcess::NotRunning) {
        return true;
    }

    // 否则检查系统中是否存在名为 "ServerCore" 的进程
    return isProcessRunningByName("ServerCore");
}

bool ServerCoreConnector::isProcessRunningByName(const QString& processName) const
{
#ifdef Q_OS_WIN
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool found = false;
    if (Process32First(snapshot, &entry)) {
        do {
            QString name = QString::fromWCharArray(entry.szExeFile);
            if (name.contains(processName, Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
#else
    // Linux/macOS: 使用 pgrep 或者遍历 /proc (Linux)
    // 这里使用一个简单的 system 调用 pgrep 的方式，或者更稳健的方式是读取 /proc
    // 为了跨平台兼容性和避免依赖 shell 命令，这里仅做简单示意，实际生产环境建议优化
    // 使用 QProcess 执行 pgrep
    QProcess proc;
    proc.start("pgrep", QStringList() << "-f" << processName);
    proc.waitForFinished();
    return proc.exitCode() == 0 && !proc.readAllStandardOutput().isEmpty();
#endif
}
