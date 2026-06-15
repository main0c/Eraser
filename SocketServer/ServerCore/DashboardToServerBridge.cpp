#include "DashboardToServerBridge.h"
#include <QDateTime>
#include <QDebug>
#include <QUuid>

DashboardToServerBridge::DashboardToServerBridge(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_socket(nullptr)
    , m_running(false)
    , m_clientManager(nullptr)
    , m_taskManager(nullptr)
    , m_messageTimer(nullptr)
{
}

DashboardToServerBridge::~DashboardToServerBridge()
{
    stop();
    cleanupZMQ();
}

bool DashboardToServerBridge::start(const QString& bindAddress)
{
    if (m_running) {
        return true;
    }

    m_bindAddress = bindAddress;
    initializeZMQ();

    m_running = true;

    m_messageTimer = new QTimer(this);
    connect(m_messageTimer, &QTimer::timeout, this, &DashboardToServerBridge::processMessages);
    m_messageTimer->start(100);

    qDebug() << "DashboardToServerBridge started on" << bindAddress;
    return true;
}

void DashboardToServerBridge::stop()
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

    cleanupZMQ();
    qDebug() << "DashboardToServerBridge stopped";
}

bool DashboardToServerBridge::isRunning() const
{
    return m_running;
}

void DashboardToServerBridge::setClientManager(ClientManager* manager)
{
    m_clientManager = manager;
}

void DashboardToServerBridge::setTaskManager(ErasureTaskManager* manager)
{
    m_taskManager = manager;
}

void DashboardToServerBridge::initializeZMQ()
{
    m_context = zmq_ctx_new();
    if (!m_context) {
        qCritical() << "Failed to create ZMQ context";
        return;
    }

    m_socket = zmq_socket(m_context, ZMQ_ROUTER);
    if (!m_socket) {
        qCritical() << "Failed to create ZMQ socket";
        cleanupZMQ();
        return;
    }

    int timeout = 100;
    zmq_setsockopt(m_socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    std::string address = m_bindAddress.toStdString();
    if (zmq_bind(m_socket, address.c_str()) != 0) {
        qCritical() << "Failed to bind to" << m_bindAddress << ":" << zmq_strerror(errno);
        cleanupZMQ();
        return;
    }

    qDebug() << "ZMQ Router socket bound to" << m_bindAddress;
}

void DashboardToServerBridge::cleanupZMQ()
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

void DashboardToServerBridge::processMessages()
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

        QString dashboardId = QString::fromUtf8(
            static_cast<char*>(zmq_msg_data(&identity)),
            zmq_msg_size(&identity)
        );

        // 记录已连接的Dashboard并更新活跃时间
        m_connectedDashboards[dashboardId] = QDateTime::currentMSecsSinceEpoch();
        cleanupStaleDashboards();

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

        QByteArray data(static_cast<char*>(zmq_msg_data(&content)), size);
        Message message;
        if (message.ParseFromArray(data.data(), data.size())) {
            if (message.type() == MESSAGE_TYPE_QUERY_CLIENTS ||
                message.type() == MESSAGE_TYPE_QUERY_CLIENT_DETAIL ||
                message.type() == MESSAGE_TYPE_QUERY_DISK_INFO ||
                message.type() == MESSAGE_TYPE_QUERY_TASKS ||
                message.type() == MESSAGE_TYPE_QUERY_TASK_DETAIL ||
                message.type() == MESSAGE_TYPE_QUERY_STATISTICS) {
                handleQueryRequest(message, dashboardId);
            }
        }

        zmq_msg_close(&identity);
        zmq_msg_close(&content);
    }
}

void DashboardToServerBridge::handleQueryRequest(const Message& message, const QString& dashboardId)
{
    const QueryRequest& query = message.query_request();
    QString requestId = QString::fromStdString(query.query_id());

    switch (query.query_type()) {
    case QueryRequest::QUERY_ALL_CLIENTS:
        handleQueryClients(query, requestId, dashboardId);
        break;
    case QueryRequest::QUERY_CLIENT_BY_ID:
        handleQueryClientDetail(query, requestId, dashboardId);
        break;
    case QueryRequest::QUERY_CLIENT_DISKS:
        handleQueryDiskInfo(query, requestId, dashboardId);
        break;
    case QueryRequest::QUERY_ALL_TASKS:
    case QueryRequest::QUERY_TASKS_BY_CLIENT:
    case QueryRequest::QUERY_TASKS_BY_DISK:
    case QueryRequest::QUERY_ACTIVE_TASKS:
    case QueryRequest::QUERY_COMPLETED_TASKS:
        handleQueryTasks(query, requestId, dashboardId);
        break;
    case QueryRequest::QUERY_TASK_BY_ID:
        handleQueryTaskDetail(query, requestId, dashboardId);
        break;
    case QueryRequest::QUERY_STATISTICS:
        handleQueryStatistics(query, requestId, dashboardId);
        break;
    default:
        qWarning() << "Unknown query type:" << query.query_type();
        break;
    }
}

void DashboardToServerBridge::handleQueryClients(const QueryRequest& query, const QString& requestId, const QString& dashboardId)
{
    if (!m_clientManager) {
        return;
    }

    Message response;
    response.set_type(MESSAGE_TYPE_RESPONSE_CLIENTS);
    response.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    QueryResponse* queryResp = response.mutable_query_response();
    queryResp->set_query_id(requestId.toStdString());
    queryResp->set_success(true);

    auto clients = m_clientManager->getAllClients();
    queryResp->set_total_clients(clients.size());
    queryResp->set_online_clients(m_clientManager->getOnlineClientCount());
    queryResp->set_offline_clients(m_clientManager->getOfflineClientCount());

    for (const auto& clientWrap : clients) {
        ClientInfo* info = queryResp->add_client_list();
        *info = clientWrap.clientInfo;
    }

    // 只发送给发起查询的Dashboard
    sendResponse(dashboardId, response);
}

void DashboardToServerBridge::handleQueryClientDetail(const QueryRequest& query, const QString& requestId, const QString& dashboardId)
{
    if (!m_clientManager) {
        return;
    }

    QString clientId = QString::fromStdString(query.filter_client_id());
    
    Message response;
    response.set_type(MESSAGE_TYPE_RESPONSE_CLIENT_DETAIL);
    response.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    QueryResponse* queryResp = response.mutable_query_response();
    queryResp->set_query_id(requestId.toStdString());

    if (m_clientManager->hasClient(clientId)) {
        auto clientData = m_clientManager->getClientData(clientId);
        *queryResp->mutable_client_detail() = clientData.clientInfo;
        queryResp->set_success(true);
    } else {
        queryResp->set_success(false);
        queryResp->set_error_message("Client not found");
    }

    sendResponse(dashboardId, response);
}

void DashboardToServerBridge::handleQueryDiskInfo(const QueryRequest& query, const QString& requestId, const QString& dashboardId)
{
    if (!m_clientManager) {
        return;
    }

    QString clientId = QString::fromStdString(query.filter_client_id());
    
    Message response;
    response.set_type(MESSAGE_TYPE_RESPONSE_DISK_INFO);
    response.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    QueryResponse* queryResp = response.mutable_query_response();
    queryResp->set_query_id(requestId.toStdString());

    auto disks = m_clientManager->getClientDisks(clientId);
    for (const auto& disk : disks) {
        DiskInfo* info = queryResp->add_disk_list();
        *info = disk.info;
    }

    queryResp->set_success(true);
    sendResponse(dashboardId, response);
}

void DashboardToServerBridge::handleQueryTasks(const QueryRequest& query, const QString& requestId, const QString& dashboardId)
{
    if (!m_taskManager) {
        return;
    }

    QList<ErasureTask> tasks;
    
    switch (query.query_type()) {
    case QueryRequest::QUERY_ALL_TASKS:
        tasks = m_taskManager->getAllTasks();
        break;
    case QueryRequest::QUERY_TASKS_BY_CLIENT: {
        QString clientId = QString::fromStdString(query.filter_client_id());
        tasks = m_taskManager->getTasksByClient(clientId);
        break;
    }
    case QueryRequest::QUERY_TASKS_BY_DISK: {
        QString clientId = QString::fromStdString(query.filter_client_id());
        QString diskId = QString::fromStdString(query.filter_disk_id());
        tasks = m_taskManager->getTasksByDisk(clientId, diskId);
        break;
    }
    case QueryRequest::QUERY_ACTIVE_TASKS:
        tasks = m_taskManager->getActiveTasks();
        break;
    case QueryRequest::QUERY_COMPLETED_TASKS:
        tasks = m_taskManager->getCompletedTasks();
        break;
    default:
        break;
    }

    Message response;
    response.set_type(MESSAGE_TYPE_RESPONSE_TASKS);
    response.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    QueryResponse* queryResp = response.mutable_query_response();
    queryResp->set_query_id(requestId.toStdString());
    queryResp->set_success(true);
    queryResp->set_total_count(tasks.size());

    for (const auto& task : tasks) {
        ErasureTask* t = queryResp->add_task_list();
        *t = task;
    }

    sendResponse(dashboardId, response);
}

void DashboardToServerBridge::handleQueryTaskDetail(const QueryRequest& query, const QString& requestId, const QString& dashboardId)
{
    if (!m_taskManager) {
        return;
    }

    QString taskId = QString::fromStdString(query.filter_task_id());
    
    Message response;
    response.set_type(MESSAGE_TYPE_RESPONSE_TASK_DETAIL);
    response.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    QueryResponse* queryResp = response.mutable_query_response();
    queryResp->set_query_id(requestId.toStdString());

    if (m_taskManager->hasTask(taskId)) {
        auto task = m_taskManager->getTask(taskId);
        *queryResp->mutable_task_detail() = task;
        queryResp->set_success(true);
    } else {
        queryResp->set_success(false);
        queryResp->set_error_message("Task not found");
    }

    sendResponse(dashboardId, response);
}

void DashboardToServerBridge::handleQueryStatistics(const QueryRequest& query, const QString& requestId, const QString& dashboardId)
{
    if (!m_clientManager || !m_taskManager) {
        return;
    }

    Message response;
    response.set_type(MESSAGE_TYPE_RESPONSE_STATISTICS);
    response.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    QueryResponse* queryResp = response.mutable_query_response();
    queryResp->set_query_id(requestId.toStdString());
    queryResp->set_success(true);

    queryResp->set_total_clients(m_clientManager->getClientCount());
    queryResp->set_online_clients(m_clientManager->getOnlineClientCount());
    queryResp->set_offline_clients(m_clientManager->getOfflineClientCount());

    auto allTasks = m_taskManager->getAllTasks();
    queryResp->set_total_tasks(allTasks.size());

    int activeCount = 0;
    int completedCount = 0;
    int failedCount = 0;

    for (const auto& task : allTasks) {
        switch (task.status()) {
        case STARTED:
        case IN_PROGRESS:
        case PENDING:
            activeCount++;
            break;
        case COMPLETED:
            completedCount++;
            break;
        case FAILED:
            failedCount++;
            break;
        default:
            break;
        }
    }

    queryResp->set_active_tasks(activeCount);
    queryResp->set_completed_tasks(completedCount);
    queryResp->set_failed_tasks(failedCount);

    sendResponse(dashboardId, response);
}

void DashboardToServerBridge::cleanupStaleDashboards()
{
    const quint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList staleIds;

    for (auto it = m_connectedDashboards.begin(); it != m_connectedDashboards.end(); ++it) {
        if (now - it.value() > DASHBOARD_TIMEOUT_MS) {
            staleIds.append(it.key());
        }
    }

    for (const QString& dashboardId : staleIds) {
        m_connectedDashboards.remove(dashboardId);
    }
}

void DashboardToServerBridge::sendResponse(const QString& dashboardId, const Message& response)
{
    if (!m_socket) {
        return;
    }

    std::string serialized;
    response.SerializeToString(&serialized);

    if (dashboardId.isEmpty()) {
        broadcastToAllDashboards(response);
        return;
    }

    zmq_msg_t identity;
    zmq_msg_init_size(&identity, dashboardId.toUtf8().size());
    memcpy(zmq_msg_data(&identity), dashboardId.toUtf8().data(), dashboardId.toUtf8().size());
    zmq_msg_send(&identity, m_socket, ZMQ_SNDMORE);
    zmq_msg_close(&identity);

    zmq_msg_t delimiter;
    zmq_msg_init_size(&delimiter, 0);
    zmq_msg_send(&delimiter, m_socket, ZMQ_SNDMORE);
    zmq_msg_close(&delimiter);

    zmq_msg_t content;
    zmq_msg_init_size(&content, serialized.size());
    memcpy(zmq_msg_data(&content), serialized.data(), serialized.size());
    zmq_msg_send(&content, m_socket, 0);
    zmq_msg_close(&content);
}

void DashboardToServerBridge::broadcastToAllDashboards(const Message& message)
{
    if (!m_socket) {
        return;
    }

    std::string serialized;
    message.SerializeToString(&serialized);

    for (auto it = m_connectedDashboards.begin(); it != m_connectedDashboards.end(); ++it) {
        QString dashboardId = it.key();

        zmq_msg_t identity;
        zmq_msg_init_size(&identity, dashboardId.toUtf8().size());
        memcpy(zmq_msg_data(&identity), dashboardId.toUtf8().data(), dashboardId.toUtf8().size());
        zmq_msg_send(&identity, m_socket, ZMQ_SNDMORE);
        zmq_msg_close(&identity);

        zmq_msg_t delimiter;
        zmq_msg_init_size(&delimiter, 0);
        zmq_msg_send(&delimiter, m_socket, ZMQ_SNDMORE);
        zmq_msg_close(&delimiter);

        zmq_msg_t content;
        zmq_msg_init_size(&content, serialized.size());
        memcpy(zmq_msg_data(&content), serialized.data(), serialized.size());
        zmq_msg_send(&content, m_socket, 0);
        zmq_msg_close(&content);
    }
}

void DashboardToServerBridge::notifyClientConnected(const QString& serverClientId, const ClientInfo& clientInfo)
{
    Message notification;
    notification.set_type(MESSAGE_TYPE_NOTIFY_CLIENT_CONNECTED);
    notification.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    *notification.mutable_client_info() = clientInfo;

    broadcastToAllDashboards(notification);
}

void DashboardToServerBridge::notifyClientDisconnected(const QString& serverClientId, const QString& reason)
{
    Message notification;
    notification.set_type(MESSAGE_TYPE_NOTIFY_CLIENT_DISCONNECTED);
    notification.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    notification.set_user_data(reason.toStdString());

    ClientInfo clientInfo;
    clientInfo.set_server_client_id(serverClientId.toStdString());
    *notification.mutable_client_info() = clientInfo;

    broadcastToAllDashboards(notification);
}

void DashboardToServerBridge::notifyDiskUpdated(const QString& serverClientId, const QList<ClientDiskInfo>& diskList)
{
    Message notification;
    notification.set_type(MESSAGE_TYPE_NOTIFY_DISK_UPDATED);
    notification.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    ClientInfo clientInfo;
    clientInfo.set_server_client_id(serverClientId.toStdString());
    *notification.mutable_client_info() = clientInfo;

    for (const auto& disk : diskList) {
        DiskInfo* info = notification.add_disk_list();
        *info = disk.info;
    }

    broadcastToAllDashboards(notification);
}

void DashboardToServerBridge::notifyTaskCreated(const QByteArray& taskData)
{
    Message notification;
    notification.set_type(MESSAGE_TYPE_NOTIFY_TASK_CREATED);
    notification.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    notification.set_user_data(std::string(taskData.data(), taskData.size()));

    broadcastToAllDashboards(notification);
}

void DashboardToServerBridge::notifyTaskProgressUpdate(const QByteArray& taskData)
{
    Message notification;
    notification.set_type(MESSAGE_TYPE_NOTIFY_TASK_PROGRESS);
    notification.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    notification.set_user_data(std::string(taskData.data(), taskData.size()));

    broadcastToAllDashboards(notification);
}

void DashboardToServerBridge::notifyTaskStatusChanged(const QByteArray& taskData)
{
    Message notification;
    notification.set_type(MESSAGE_TYPE_NOTIFY_TASK_STATUS_CHANGED);
    notification.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    notification.set_user_data(std::string(taskData.data(), taskData.size()));

    broadcastToAllDashboards(notification);
}

void DashboardToServerBridge::notifyTaskCompleted(const QByteArray& taskData)
{
    Message notification;
    notification.set_type(MESSAGE_TYPE_NOTIFY_TASK_COMPLETED);
    notification.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    notification.set_user_data(std::string(taskData.data(), taskData.size()));

    broadcastToAllDashboards(notification);
}

void DashboardToServerBridge::notifyTaskProgress(const QString& clientId, const ErasureProgress& progress)
{
    Message notification;
    notification.set_type(MESSAGE_TYPE_NOTIFY_TASK_PROGRESS);
    notification.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    *notification.mutable_progress_info() = progress;

    broadcastToAllDashboards(notification);
}