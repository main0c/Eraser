#include "DashboardWindow.h"
#include "ui_DashboardWindow.h"
#include <QDateTime>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QCloseEvent>

DashboardWindow::DashboardWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mainSplitter(nullptr)
    , m_rightSplitter(nullptr)
    , m_clientListGroup(nullptr)
    , m_connectBtn(nullptr)
    , m_disconnectBtn(nullptr)
    , m_refreshBtn(nullptr)
    , m_detailTabs(nullptr)
    , m_clientInfoWidget(nullptr)
    , m_diskInfoWidget(nullptr)
    , m_taskListWidget(nullptr)
    , m_logWidget(nullptr)
    , m_statusLabel(nullptr)
    , m_clientCountLabel(nullptr)
    , m_serverStatusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_menuBar(nullptr)
    , m_connectAction(nullptr)
    , m_disconnectAction(nullptr)
    , m_refreshAction(nullptr)
    , m_clearLogsAction(nullptr)
    , m_exportLogsAction(nullptr)
    , m_exitAction(nullptr)
    , m_connector(nullptr)
    , m_refreshTimer(nullptr)
    , m_connected(false)
    , ui(new Ui::DashboardWindow)
{
    ui->setupUi(this);
    
    m_connector = new ServerCoreConnector(this);
    
    setupUI();
    setupMenuBar();
    setupStatusBar();
    
    // 连接信号槽
    connect(m_connector, &ServerCoreConnector::connected,
            this, &DashboardWindow::onConnected);
    connect(m_connector, &ServerCoreConnector::disconnected,
            this, &DashboardWindow::onDisconnected);
    connect(m_connector, &ServerCoreConnector::connectionError,
            this, &DashboardWindow::onConnectionError);
    
    connect(m_connector, &ServerCoreConnector::clientsReceived,
            this, &DashboardWindow::onClientsReceived);
    connect(m_connector, &ServerCoreConnector::diskInfoReceived,
            this, &DashboardWindow::onDiskInfoReceived);
    connect(m_connector, &ServerCoreConnector::tasksReceived,
            this, &DashboardWindow::onTasksReceived);
    connect(m_connector, &ServerCoreConnector::statisticsReceived,
            this, &DashboardWindow::onStatisticsReceived);
    
    connect(m_connector, &ServerCoreConnector::clientConnected,
            this, &DashboardWindow::onClientConnected);
    connect(m_connector, &ServerCoreConnector::clientDisconnected,
            this, &DashboardWindow::onClientDisconnected);
    connect(m_connector, &ServerCoreConnector::diskUpdated,
            this, &DashboardWindow::onDiskUpdated);
    connect(m_connector, &ServerCoreConnector::taskCreated,
            this, &DashboardWindow::onTaskCreated);
    connect(m_connector, &ServerCoreConnector::taskProgressUpdated,
            this, &DashboardWindow::onTaskProgressUpdated);
    connect(m_connector, &ServerCoreConnector::taskStatusChanged,
            this, &DashboardWindow::onTaskStatusChanged);
    connect(m_connector, &ServerCoreConnector::taskCompleted,
            this, &DashboardWindow::onTaskCompleted);
    
    // 自动刷新定时器
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &DashboardWindow::refreshData);
    m_refreshTimer->start(30000); // 每30秒刷新一次
    
    addLogEntry("Dashboard initialized", "INFO");

    // 启动时尝试自动连接
    QTimer::singleShot(500, this, [this]() {
        if (!m_connected) {
            addLogEntry("正在尝试自动连接ServerCore...", "INFO");
            connectToServer();
        }
    });
}

DashboardWindow::~DashboardWindow()
{
    if (m_connected) {
        m_connector->disconnectFromServer();
    }
    delete ui;
}

void DashboardWindow::closeEvent(QCloseEvent *event)
{
    if (m_connected) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("退出确认");
        msgBox.setText("ServerCore 仍在运行。您希望如何处理？");
        
        QPushButton* bgButton = msgBox.addButton("后台运行", QMessageBox::AcceptRole);
        QPushButton* stopButton = msgBox.addButton("停止 ServerCore", QMessageBox::DestructiveRole);
        QPushButton* cancelButton = msgBox.addButton("取消", QMessageBox::RejectRole);
        
        msgBox.setDefaultButton(cancelButton);
        msgBox.exec();

        if (msgBox.clickedButton() == stopButton) {
            // 这里假设 connector 有停止服务器的方法，如果没有，可能需要调用其他接口
            // m_connector->stopServer(); 
            addLogEntry("请求停止 ServerCore...", "WARNING");
            // 注意：实际停止逻辑取决于 ServerCoreConnector 的具体实现
            // 如果无法直接停止，可能只是断开连接
            m_connector->disconnectFromServer();
            event->accept();
        } else if (msgBox.clickedButton() == bgButton) {
            // 后台运行：只断开 UI 连接，不关闭服务器进程（如果服务器是独立进程）
            // 或者仅仅隐藏窗口
            addLogEntry("Dashboard 关闭，ServerCore 继续在后台运行", "INFO");
            m_connector->disconnectFromServer();
            event->accept();
        } else {
            // 取消退出
            event->ignore();
        }
    } else {
        event->accept();
    }
}

void DashboardWindow::setupUI()
{
    setWindowTitle("Eraser Dashboard");
    resize(1400, 900);
    
    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);
    
    // 左侧面板 - 客户端列表和控制
    m_clientListGroup = new QWidget(m_mainSplitter);
    QVBoxLayout* leftLayout = new QVBoxLayout(m_clientListGroup);
    
    m_connectBtn = new QPushButton("连接ServerCore", m_clientListGroup);
    m_disconnectBtn = new QPushButton("断开连接", m_clientListGroup);
    m_refreshBtn = new QPushButton("刷新数据", m_clientListGroup);
    
    m_disconnectBtn->setEnabled(false);
    
    leftLayout->addWidget(m_connectBtn);
    leftLayout->addWidget(m_disconnectBtn);
    leftLayout->addWidget(m_refreshBtn);
    leftLayout->addStretch();
    
    connect(m_connectBtn, &QPushButton::clicked, this, &DashboardWindow::connectToServer);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &DashboardWindow::disconnectFromServer);
    connect(m_refreshBtn, &QPushButton::clicked, this, &DashboardWindow::refreshData);
    
    // 右侧面板 - 详细信息
    m_rightSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);
    
    m_detailTabs = new QTabWidget(m_rightSplitter);
    
    m_clientInfoWidget = new ClientInfoWidget(m_detailTabs);
    m_diskInfoWidget = new DiskInfoWidget(m_detailTabs);
    m_taskListWidget = new TaskListWidget(m_detailTabs);
    
    m_detailTabs->addTab(m_clientInfoWidget, "客户端信息");
    m_detailTabs->addTab(m_diskInfoWidget, "磁盘信息");
    m_detailTabs->addTab(m_taskListWidget, "任务列表");
    
    m_logWidget = new LogWidget(m_rightSplitter);
    
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 3);
}

void DashboardWindow::setupMenuBar()
{
    m_menuBar = menuBar();
    
    QMenu* fileMenu = m_menuBar->addMenu("文件");
    QMenu* viewMenu = m_menuBar->addMenu("视图");
    QMenu* helpMenu = m_menuBar->addMenu("帮助");
    
    m_connectAction = new QAction("连接ServerCore", this);
    m_disconnectAction = new QAction("断开连接", this);
    m_refreshAction = new QAction("刷新数据", this);
    m_clearLogsAction = new QAction("清除日志", this);
    m_exportLogsAction = new QAction("导出日志", this);
    m_exitAction = new QAction("退出", this);
    
    m_disconnectAction->setEnabled(false);
    
    fileMenu->addAction(m_connectAction);
    fileMenu->addAction(m_disconnectAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);
    
    viewMenu->addAction(m_refreshAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_clearLogsAction);
    viewMenu->addAction(m_exportLogsAction);
    
    connect(m_connectAction, &QAction::triggered, this, &DashboardWindow::connectToServer);
    connect(m_disconnectAction, &QAction::triggered, this, &DashboardWindow::disconnectFromServer);
    connect(m_refreshAction, &QAction::triggered, this, &DashboardWindow::refreshData);
    connect(m_clearLogsAction, &QAction::triggered, this, &DashboardWindow::clearLogs);
    connect(m_exportLogsAction, &QAction::triggered, this, &DashboardWindow::exportLogs);
    connect(m_exitAction, &QAction::triggered, this, &QApplication::quit);
}

void DashboardWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("未连接", statusBar());
    m_clientCountLabel = new QLabel("客户端: 0", statusBar());
    m_serverStatusLabel = new QLabel("ServerCore: 未连接", statusBar());
    m_progressBar = new QProgressBar(statusBar());
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addWidget(m_clientCountLabel);
    statusBar()->addWidget(m_serverStatusLabel);
    statusBar()->addWidget(m_progressBar);
}

void DashboardWindow::addLogEntry(const QString& message, const QString& type)
{
    if (m_logWidget) {
        m_logWidget->addLogEntry(message, type);
    }
}

void DashboardWindow::connectToServer()
{
    if (m_connected) {
        return;
    }
    
    addLogEntry("正在连接ServerCore...", "INFO");
    
    if (m_connector->connectToServer("tcp://localhost:5556")) {
        addLogEntry("连接请求已发送", "INFO");
    } else {
        addLogEntry("连接失败", "ERROR");
    }
}

void DashboardWindow::disconnectFromServer()
{
    if (!m_connected) {
        return;
    }
    
    addLogEntry("正在断开连接...", "INFO");
    m_connector->disconnectFromServer();
}

void DashboardWindow::refreshData()
{
    if (!m_connected) {
        return;
    }
    
    addLogEntry("刷新数据...", "INFO");
    
    m_connector->queryAllClients();
    m_connector->queryAllTasks();
    m_connector->queryStatistics();
}

void DashboardWindow::clearLogs()
{
    if (m_logWidget) {
        m_logWidget->clearLog();
        addLogEntry("日志已清除", "INFO");
    }
}

void DashboardWindow::exportLogs()
{
    if (!m_logWidget) {
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, "导出日志", "", "文本文件 (*.txt)");
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_logWidget->getLogContent();
        file.close();
        addLogEntry("日志已导出到: " + fileName, "INFO");
    }
}

void DashboardWindow::updateClientCount()
{
    int onlineCount = 0;
    for (const auto& client : m_cachedClients) {
        if (client.isOnline) {
            onlineCount++;
        }
    }
    
    m_clientCountLabel->setText(QString("客户端: %1 (在线: %2)").arg(m_cachedClients.size()).arg(onlineCount));
}

void DashboardWindow::onConnected()
{
    m_connected = true;
    
    m_connectBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(true);
    m_connectAction->setEnabled(false);
    m_disconnectAction->setEnabled(true);
    
    m_statusLabel->setText("已连接");
    m_serverStatusLabel->setText("ServerCore: 已连接");
    
    addLogEntry("成功连接到ServerCore", "SUCCESS");
    
    // 连接后立即查询数据
    refreshData();
}

void DashboardWindow::onDisconnected()
{
    m_connected = false;
    
    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_connectAction->setEnabled(true);
    m_disconnectAction->setEnabled(false);
    
    m_statusLabel->setText("未连接");
    m_serverStatusLabel->setText("ServerCore: 未连接");
    m_clientCountLabel->setText("客户端: 0");
    
    addLogEntry("已断开与ServerCore的连接", "INFO");
    
    // 清空缓存
    m_cachedClients.clear();
    if (m_clientInfoWidget) {
        m_clientInfoWidget->setClientList(m_cachedClients);
    }
    if (m_diskInfoWidget) {
        m_diskInfoWidget->setClientList(m_cachedClients);
    }
}

void DashboardWindow::onConnectionError(const QString& error)
{
    addLogEntry("连接错误: " + error, "ERROR");
    // 避免在自动重连时频繁弹出警告框，可根据需要调整
    // QMessageBox::warning(this, "连接错误", error);
}

void DashboardWindow::onClientsReceived(const QList<ClientInfo>& clients, int onlineCount, int offlineCount)
{
    addLogEntry(QString("收到客户端列表: %1个客户端 (在线: %2, 离线: %3)")
                .arg(clients.size()).arg(onlineCount).arg(offlineCount), "INFO");
    
    // 转换为ClientInfoWrap格式
    m_cachedClients.clear();
    for (const auto& client : clients) {
        ClientInfoWrap wrap(client);
        // 这里需要根据实际情况设置在线状态
        wrap.isOnline = true; // 简化处理
        wrap.connectTime = QDateTime::currentDateTime();
        wrap.lastHeartbeat = QDateTime::currentDateTime();
        m_cachedClients.append(wrap);
    }
    
    if (m_clientInfoWidget) {
        m_clientInfoWidget->setClientList(m_cachedClients);
    }
    if (m_diskInfoWidget) {
        m_diskInfoWidget->setClientList(m_cachedClients);
    }
    
    updateClientCount();
}

void DashboardWindow::onClientDetailReceived(const ClientInfo& client)
{
    addLogEntry("收到客户端详情: " + QString::fromStdString(client.hostname()), "INFO");
}

void DashboardWindow::onDiskInfoReceived(const QString& clientId, const QList<DiskInfo>& disks)
{
    addLogEntry(QString("收到磁盘信息: 客户端=%1, 磁盘数=%2").arg(clientId).arg(disks.size()), "INFO");
    
    // 更新对应客户端的磁盘信息
    for (auto& client : m_cachedClients) {
        if (client.clientInfo.server_client_id() == clientId.toStdString()) {
            client.diskList.clear();
            for (const auto& disk : disks) {
                ClientDiskInfo diskInfo(disk);
                client.diskList.append(diskInfo);
            }
            break;
        }
    }
    
    if (m_diskInfoWidget) {
        m_diskInfoWidget->setClientList(m_cachedClients);
    }
}

void DashboardWindow::onTasksReceived(const QList<ErasureTask>& tasks)
{
    addLogEntry(QString("收到任务列表: %1个任务").arg(tasks.size()), "INFO");
    
    // 更新任务列表显示
    // 这里需要将任务数据传递给TaskListWidget
}

void DashboardWindow::onTaskDetailReceived(const ErasureTask& task)
{
    addLogEntry("收到任务详情: " + QString::fromStdString(task.task_id()), "INFO");
}

void DashboardWindow::onStatisticsReceived(int totalClients, int onlineClients, int offlineClients,
                                           int totalTasks, int activeTasks, int completedTasks, int failedTasks)
{
    addLogEntry(QString("统计信息 - 客户端: %1 (在线: %2, 离线: %3), 任务: %4 (活跃: %5, 完成: %6, 失败: %7)")
                .arg(totalClients).arg(onlineClients).arg(offlineClients)
                .arg(totalTasks).arg(activeTasks).arg(completedTasks).arg(failedTasks), "INFO");
}

void DashboardWindow::onClientConnected(const QString& serverClientId, const ClientInfo& clientInfo)
{
    addLogEntry(QString("客户端连接: %1 (%2)").arg(serverClientId).arg(QString::fromStdString(clientInfo.hostname())), "SUCCESS");
    
    // 添加新客户端到缓存
    ClientInfoWrap wrap(clientInfo);
    wrap.isOnline = true;
    wrap.connectTime = QDateTime::currentDateTime();
    wrap.lastHeartbeat = QDateTime::currentDateTime();
    m_cachedClients.append(wrap);
    
    if (m_clientInfoWidget) {
        m_clientInfoWidget->setClientList(m_cachedClients);
    }
    if (m_diskInfoWidget) {
        m_diskInfoWidget->setClientList(m_cachedClients);
    }
    
    updateClientCount();
}

void DashboardWindow::onClientDisconnected(const QString& serverClientId, const QString& reason)
{
    addLogEntry(QString("客户端断开: %1, 原因: %2").arg(serverClientId).arg(reason), "WARNING");
    
    // 更新客户端状态
    for (auto& client : m_cachedClients) {
        if (client.clientInfo.server_client_id() == serverClientId.toStdString()) {
            client.isOnline = false;
            client.disconnectTime = QDateTime::currentDateTime();
            client.disconnectReason = reason;
            break;
        }
    }
    
    if (m_clientInfoWidget) {
        m_clientInfoWidget->setClientList(m_cachedClients);
    }
    if (m_diskInfoWidget) {
        m_diskInfoWidget->setClientList(m_cachedClients);
    }
    
    updateClientCount();
}

void DashboardWindow::onDiskUpdated(const QString& serverClientId, const QList<DiskInfo>& disks)
{
    addLogEntry(QString("磁盘信息更新: 客户端=%1, 磁盘数=%2").arg(serverClientId).arg(disks.size()), "INFO");
    
    // 更新对应客户端的磁盘信息
    for (auto& client : m_cachedClients) {
        if (client.clientInfo.server_client_id() == serverClientId.toStdString()) {
            client.diskList.clear();
            for (const auto& disk : disks) {
                ClientDiskInfo diskInfo(disk);
                client.diskList.append(diskInfo);
            }
            break;
        }
    }
    
    if (m_diskInfoWidget) {
        m_diskInfoWidget->setClientList(m_cachedClients);
    }
}

void DashboardWindow::onTaskCreated(const ErasureTask& task)
{
    addLogEntry(QString("任务创建: %1").arg(QString::fromStdString(task.task_id())), "INFO");
}

void DashboardWindow::onTaskProgressUpdated(const ErasureProgress& progress)
{
    // 更新任务进度显示
    QString taskId = QString::fromStdString(progress.task_id());
    int percent = progress.progress_percent() / 100; // 转换为0-100
    
    // 可以在这里更新UI进度条
}

void DashboardWindow::onTaskStatusChanged(const ErasureTask& task)
{
    addLogEntry(QString("任务状态变化: %1, 状态=%2")
                .arg(QString::fromStdString(task.task_id()))
                .arg(task.status()), "INFO");
}

void DashboardWindow::onTaskCompleted(const ErasureTask& task)
{
    bool success = (task.status() == COMPLETED);
    addLogEntry(QString("任务完成: %1, 成功=%2")
                .arg(QString::fromStdString(task.task_id()))
                .arg(success ? "是" : "否"), success ? "SUCCESS" : "ERROR");
}
