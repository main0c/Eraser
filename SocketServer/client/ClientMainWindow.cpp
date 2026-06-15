#include "ClientMainWindow.h"
#include "Utils.h"

#include <QApplication>
#include <QDateTime>
#include <QHeaderView>
#include <QFileDialog>
#include <QInputDialog>
#include <QStandardPaths>
#include <QDir>
#include <QScrollBar>
#include <QHostInfo>
#include <QSysInfo>
#include <QMessageBox>
#include <QDebug>
#include <QProgressBar>

// 构造函数
ClientMainWindow::ClientMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_tabWidget(nullptr)
    , m_connectionControlWidget(nullptr)
    , m_diskInfoGroup(nullptr)
    , m_diskInfoWidget(nullptr)
    , m_progressDisplayWidget(nullptr)
    , m_logWidget(nullptr)
    , m_statusLabel(nullptr)
    , m_menuBar(nullptr)
    , m_connectAction(nullptr)
    , m_disconnectAction(nullptr)
    , m_exitAction(nullptr)
    , m_clearLogAction(nullptr)
    , m_exportLogAction(nullptr)
    , m_aboutAction(nullptr)
    , m_isConnected(false)
    , m_isErasing(false)
    , m_isPaused(false)
    , m_progressTimer(nullptr)
    , m_connectionTimer(nullptr)
    , m_messageHandler(nullptr)
    , m_zmqClient(nullptr)
    , m_taskManager(nullptr)
    //    , m_defaultServer("tcp://192.168.1.55:5555")
    , m_defaultServer("tcp://localhost:5555")
{

    
    m_messageHandler = new MessageHandler(this);
    
    QString clientId = QString("GUI_CLIENT_%1").arg(QDateTime::currentMSecsSinceEpoch());


    m_zmqClient = new ZMQClient(clientId, m_defaultServer, this);
    connect(m_zmqClient, &ZMQClient::connected, this, &ClientMainWindow::onClientStarted);
    connect(m_zmqClient, &ZMQClient::resultOfClientRegister, this, &ClientMainWindow::onClientRegister);

    connect(m_zmqClient, &ZMQClient::disconnected, this, &ClientMainWindow::onClientStopped);
    connect(m_zmqClient, &ZMQClient::errorOccurred, this, &ClientMainWindow::onErrorOccurred);
    connect(m_zmqClient, &ZMQClient::serverResponseReceived, this, [this](const ServerResponse& response) {
        QString msg = QString::fromStdString(response.message());
        if (response.success()) {
            addLogEntry(QString("服务器响应: %1").arg(msg), "SUCCESS");
        } else {
            addLogEntry(QString("服务器响应失败: %1").arg(msg), "ERROR");
        }
    });
    
    m_taskManager = new ErasureTaskManager(this);
    connect(m_taskManager, &ErasureTaskManager::taskCreated, this, &ClientMainWindow::onTaskCreated);
    connect(m_taskManager, &ErasureTaskManager::taskProgressUpdated, this, &ClientMainWindow::onTaskProgressUpdated);
    connect(m_taskManager, &ErasureTaskManager::taskStatusChanged, this, &ClientMainWindow::onTaskStatusChanged);
    connect(m_taskManager, &ErasureTaskManager::taskCompleted, this, &ClientMainWindow::onTaskCompleted);
    
    // 设置任务存储回调，使用 CheckResultDB 进行数据库持久化
    m_taskManager->setTaskStorageCallback(
                [this](const ErasureTask& task) {
#if defined(DB_SUPPORT)
        // 保存任务到数据库
        if (CheckResultDB::GetInstance().InsertTask(task)) {
            qDebug() << "任务已保存到数据库:" << QString::fromStdString(task.task_id());
        } else {
            qWarning() << "保存任务到数据库失败:" << QString::fromStdString(task.task_id());
        }
#endif
    },
    [this](const QString& taskId) -> ErasureTask {
#if defined(DB_SUPPORT)
        // 从数据库加载任务
        ErasureTask task;
        if (CheckResultDB::GetInstance().SelectTask(taskId.toStdString(), task)) {
            qDebug() << "从数据库加载任务成功:" << taskId;
            return task;
        } else {
            qWarning() << "从数据库加载任务失败:" << taskId;
            return ErasureTask();
        }
#endif
    },
    [this](const QString& taskId) {
#if defined(DB_SUPPORT)
        // 从数据库删除任务
        if (CheckResultDB::GetInstance().DeleteTask(taskId.toStdString())) {
            qDebug() << "从数据库删除任务成功:" << taskId;
        } else {
            qWarning() << "从数据库删除任务失败:" << taskId;
        }
#endif
    }
    );
    
    m_progressTimer = new QTimer(this);
    connect(m_progressTimer, &QTimer::timeout, this, &ClientMainWindow::updateErasureProgress);
    
    m_connectionTimer = new QTimer(this);
    connect(m_connectionTimer, &QTimer::timeout, this, &ClientMainWindow::onConnectionStatusChanged);
    
    // 初始化客户端列表（单客户端模式）
    m_clientList.clear();
    ClientInfoWrap localClient;

    ClientInfo clientInfo = m_messageHandler->generateDemoClientInfo(clientId);
    localClient.clientInfo = clientInfo;
    localClient.diskList = QList<ClientDiskInfo>();
    m_clientList.append(localClient);

    generateSimulatedDisks(m_clientList);

    setupUI();
    setupMenuBar();
    setupStatusBar();


    // 使用新的多客户端接口
    m_diskInfoWidget->setClientList(m_clientList);
    
    updateConnectionStatus(false);
    
    addLogEntry("客户端界面已初始化");
    
#if defined(DB_SUPPORT)
    // 从数据库恢复未完成的任务
    restoreUnfinishedTasksFromDB();
#endif

    connectToServer();

    //TODO
    generateDiskInfo();
}

ClientMainWindow::~ClientMainWindow()
{
    if (m_isConnected) {
        disconnectFromServer();
    }
}

void ClientMainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_tabWidget = new QTabWidget(this);

    setupConnectionGroup();
    setupDiskInfoGroup();
    setupTaskListGroup();
    setupLogGroup();

    m_tabWidget->addTab(m_diskInfoGroup, "磁盘信息与擦除");
    m_tabWidget->addTab(m_taskListWidget, "任务列表");
    m_tabWidget->addTab(m_logWidget, "系统日志");

    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->addWidget(m_tabWidget);

    resize(1200, 800);
    setWindowTitle("Eraser 客户端");
}

// 菜单栏初始化
void ClientMainWindow::setupMenuBar()
{
    m_menuBar = menuBar();

    QMenu* connectionMenu = m_menuBar->addMenu("连接");
    m_connectAction = connectionMenu->addAction("连接服务器");
    m_disconnectAction = connectionMenu->addAction("断开连接");
    connectionMenu->addSeparator();
    m_exitAction = connectionMenu->addAction("退出");

    QMenu* toolsMenu = m_menuBar->addMenu("工具");
    m_clearLogAction = toolsMenu->addAction("清空日志");
    m_exportLogAction = toolsMenu->addAction("导出日志");
    toolsMenu->addSeparator();
    QAction* testMultiClientAction = toolsMenu->addAction("测试多客户端模式");
    connect(testMultiClientAction, &QAction::triggered, this, &ClientMainWindow::testMultiClientMode);

    QMenu* helpMenu = m_menuBar->addMenu("帮助");
    m_aboutAction = helpMenu->addAction("关于");

    connect(m_connectAction, &QAction::triggered, this, &ClientMainWindow::connectToServer);
    connect(m_disconnectAction, &QAction::triggered, this, &ClientMainWindow::disconnectFromServer);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    connect(m_clearLogAction, &QAction::triggered, this, &ClientMainWindow::clearLog);
    connect(m_exportLogAction, &QAction::triggered, this, &ClientMainWindow::exportLog);
    connect(m_aboutAction, &QAction::triggered, this, &ClientMainWindow::showAbout);
}

// 状态栏初始化
void ClientMainWindow::setupStatusBar()
{
    QStatusBar* statusBar = this->statusBar();

    m_connectionStatusIcon = new QLabel(this);
    m_connectionStatusIcon->setPixmap(QPixmap(":/icons/disconnected.png").scaled(16, 16));

    m_statusLabel = new QLabel("就绪", this);

    statusBar->addWidget(m_connectionStatusIcon);
    statusBar->addWidget(m_statusLabel);
    statusBar->addPermanentWidget(m_statusLabel);
}

// 连接面板初始化
void ClientMainWindow::setupConnectionGroup()
{
    // 创建连接控制组件
    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
    QString clientId = QString::fromStdString(clientInfo.client_id());
    m_connectionControlWidget = new ConnectionControlWidget(clientId, this);
    
    // 设置默认服务器地址
    m_connectionControlWidget->setServerAddress(m_defaultServer);
    
    // 连接信号
    connect(m_connectionControlWidget, &ConnectionControlWidget::connectRequested,
            this, &ClientMainWindow::connectToServer);
    connect(m_connectionControlWidget, &ConnectionControlWidget::disconnectRequested,
            this, &ClientMainWindow::disconnectFromServer);
}

// 磁盘区域初始化
void ClientMainWindow::setupDiskInfoGroup()
{
    // 创建新的磁盘信息 widget
    m_diskInfoGroup = new QGroupBox("", this);
    QVBoxLayout* mainLayout = new QVBoxLayout(m_diskInfoGroup);

    // 添加连接控制组件
    mainLayout->addWidget(m_connectionControlWidget);

    QGroupBox* diskTableGroup = new QGroupBox("", this);
    QVBoxLayout* tableLayout = new QVBoxLayout(diskTableGroup);
    m_diskInfoWidget = new DiskInfoWidget(this);
    
    tableLayout->addWidget(m_diskInfoWidget);
    mainLayout->addWidget(diskTableGroup, 1);

    // 进度显示组件
    m_progressDisplayWidget = new ProgressDisplayWidget(this);
    mainLayout->addWidget(m_progressDisplayWidget);
    mainLayout->addStretch();

    // 连接 DiskInfoWidget 的信号（包含擦除控制信号）
    connect(m_diskInfoWidget, &DiskInfoWidget::diskSelectionChanged,
            this, &ClientMainWindow::onDiskSelectionChanged);
    ErasureControlWidget* erasureCtrl = m_diskInfoWidget->getErasureControlWidget();

    connect(erasureCtrl, &ErasureControlWidget::generateDiskRequested,
            this, &ClientMainWindow::generateDiskInfo);
    connect(erasureCtrl, &ErasureControlWidget::sendDiskInfoRequested,
            this, &ClientMainWindow::sendDiskInfo);
    connect(m_diskInfoWidget, &DiskInfoWidget::refreshRequested,
            this, &ClientMainWindow::refreshDiskList);
    
    // 连接擦除控制信号
    connect(m_diskInfoWidget, &DiskInfoWidget::startErasureRequested,
            this, &ClientMainWindow::startErasure);
    connect(m_diskInfoWidget, &DiskInfoWidget::stopErasureRequested,
            this, &ClientMainWindow::stopErasure);
    connect(m_diskInfoWidget, &DiskInfoWidget::pauseErasureRequested,
            this, &ClientMainWindow::pauseErasure);
    connect(m_diskInfoWidget, &DiskInfoWidget::resumeErasureRequested,
            this, &ClientMainWindow::resumeErasure);
    connect(m_diskInfoWidget, &DiskInfoWidget::erasureMethodChanged,
            this, &ClientMainWindow::onErasureMethodChanged);
}

void ClientMainWindow::setupTaskListGroup()
{
    m_taskListWidget = new TaskListWidget(this);
    m_taskListWidget->setTaskManager(m_taskManager);
    
    connect(m_taskManager, &ErasureTaskManager::taskCreated,
            m_taskListWidget, &TaskListWidget::onTaskCreated);
    connect(m_taskManager, &ErasureTaskManager::taskProgressUpdated,
            m_taskListWidget, &TaskListWidget::onTaskProgressUpdated);
    connect(m_taskManager, &ErasureTaskManager::taskStatusChanged,
            m_taskListWidget, &TaskListWidget::onTaskStatusChanged);
    connect(m_taskManager, &ErasureTaskManager::taskCompleted,
            m_taskListWidget, &TaskListWidget::onTaskCompleted);
}

void ClientMainWindow::setupLogGroup()
{
    m_logWidget = new LogWidget(this);
}

// 生成模拟磁盘数据
void ClientMainWindow::generateSimulatedDisks(QList<ClientInfoWrap>& clientList)
{
    for (int i =0; i< clientList.size(); i++) {
        ClientInfoWrap& client =  clientList[i];
        client.diskList.clear();

        QStringList models = {
            "Samsung SSD 970 EVO 1TB",
            "Western Digital WD10EZEX 1TB",
            "Seagate Barracuda 2TB",
            "Crucial MX500 500GB",
            "Kingston A2000 1TB"
        };

        QStringList interfaces = {"SATA", "NVMe", "USB 3.0", "SAS"};
        QStringList fileSystems = {"NTFS", "exFAT", "ext4", "APFS"};
        QStringList mountPoints = {"C:\\", "D:\\", "E:\\", "/mnt/data", "/media/usb"};

        for (int i = 1; i <= 4; ++i) {
            DiskInfo disk;
            disk.set_disk_id(QString("DISK_%1").arg(i, 2, 10, QChar('0')).toStdString());
            disk.set_model(models[Utils::generateRandomInt(0, models.size() - 1)].toStdString());
            disk.set_serial_number(QString("SN%1%2").arg(i, 2, 10, QChar('0')).arg(Utils::generateRandomInt(1000, 9999)).toStdString());
            disk.set_interface_type(interfaces[Utils::generateRandomInt(0, interfaces.size() - 1)].toStdString());

            qint64 totalSizeGB = Utils::generateRandomInt64(500, 2000);
            disk.set_total_size(totalSizeGB * 1024LL * 1024 * 1024); // 500GB-2TB

            double usageRatio = 0.3 + Utils::generateRandomInt(0, 50) / 100.0; // 30-80% 使用率
            disk.set_used_size(static_cast<qint64>(disk.total_size() * usageRatio));
            disk.set_free_size(disk.total_size() - disk.used_size());

            disk.set_file_system(fileSystems[Utils::generateRandomInt(0, fileSystems.size() - 1)].toStdString());
            disk.set_is_system_disk(i == 1);
            disk.set_is_removable(Utils::generateRandomInt(0, 9) < 2);
            disk.set_mount_point(mountPoints[Utils::generateRandomInt(0, mountPoints.size() - 1)].toStdString());
            disk.set_health_status(60.0 + Utils::generateRandomInt(0, 40));

            ClientDiskInfo wrapper;
            wrapper.info = disk;
            wrapper.isSelected = false;
            wrapper.progressPercent = 0;
            wrapper.status = "空闲";
            client.diskList.append(wrapper);
        }
    }
}


void ClientMainWindow::updateProgressDisplay()
{
    QList<ErasureTask> activeTasks = m_taskManager->getActiveTasks();
    
    // 1. 通过新的 DiskInfoWidget 更新磁盘列表中的进度
    for (const auto& task : activeTasks) {
        // 通知 widget 更新该磁盘的进度信息
        m_diskInfoWidget->updateDiskProgress(task);
    }

    // 2. 计算总体进度用于底部状态栏和进度条
    if (activeTasks.isEmpty()) {
        m_progressDisplayWidget->reset();
        return;
    }

    // 计算总体进度、速度和剩余时间
    double totalProgress = 0.0;
    qint64 totalErasedBytes = 0;
    qint64 totalBytes = 0;
    qint64 minRemainingMs = LLONG_MAX;
    bool hasActiveProgress = false;
    
    int pausedCount = 0;

    for (int i = 0; i < activeTasks.size(); ++i) {
        const ErasureTask& task = activeTasks[i];
        double progressPercent = task.progress_percent() / 100.0;
        totalProgress += progressPercent;
        totalErasedBytes += task.erased_bytes();
        totalBytes += task.total_bytes();
        
        if (task.status() == TaskStatus::PAUSED) {
            pausedCount++;
        } else if (task.status() == TaskStatus::IN_PROGRESS ||
                   task.status() == TaskStatus::STARTED) {
            hasActiveProgress = true;
            
            if (progressPercent > 0 && progressPercent < 100) {
                QDateTime startTime = QDateTime::fromMSecsSinceEpoch(task.start_time());
                qint64 elapsedMs = startTime.msecsTo(QDateTime::currentDateTime());
                if (elapsedMs > 0) {
                    qint64 estimatedTotalMs = elapsedMs * 100.0 / progressPercent;
                    qint64 remainingMs = estimatedTotalMs - elapsedMs;
                    if (remainingMs > 0) {
                        if (remainingMs > minRemainingMs && minRemainingMs != LLONG_MAX) {
                            // keep max
                        } else if (minRemainingMs == LLONG_MAX) {
                            minRemainingMs = remainingMs;
                        } else {
                            if (remainingMs > minRemainingMs) minRemainingMs = remainingMs;
                        }
                    }
                }
            }
        }
    }

    int activeCount = activeTasks.size();
    double avgProgress = totalProgress / activeCount;
    
    // 更新进度显示组件
    m_progressDisplayWidget->setProgress(static_cast<int>(avgProgress));
    
    if (hasActiveProgress) {
        double totalSpeedMBps = 0.0;
        int speedCalcCount = 0;
        for (int i = 0; i < activeTasks.size(); ++i) {
            const ErasureTask& task = activeTasks[i];
            double progressPercent = task.progress_percent() / 100.0;
            if (progressPercent > 0) {
                QDateTime startTime = QDateTime::fromMSecsSinceEpoch(task.start_time());
                qint64 elapsedMs = startTime.msecsTo(QDateTime::currentDateTime());
                if (elapsedMs > 0) {
                    double speedMBps = (task.erased_bytes() / 1024.0 / 1024.0) / (elapsedMs / 1000.0);
                    totalSpeedMBps += speedMBps;
                    speedCalcCount++;
                }
            }
        }
        
        if (speedCalcCount > 0) {
            m_progressDisplayWidget->setTotalSpeed(totalSpeedMBps);
        } else {
            m_progressDisplayWidget->setTotalSpeed(0.0);
        }
    } else {
        m_progressDisplayWidget->setTotalSpeed(0.0);
    }
    
    // 计算剩余时间
    if (hasActiveProgress && minRemainingMs != LLONG_MAX && minRemainingMs > 0) {
        int remainingSeconds = minRemainingMs / 1000;
        int hours = remainingSeconds / 3600;
        int minutes = (remainingSeconds % 3600) / 60;
        int seconds = remainingSeconds % 60;
        m_progressDisplayWidget->setTimeRemaining(hours, minutes, seconds);
    } else {
        m_progressDisplayWidget->setTimeRemaining(-1, -1, -1);
    }
    
    QString statusText;
    if (m_isPaused) {
        statusText = QString("状态: 已暂停 (%1个任务)").arg(activeCount);
    } else {
        int runningCount = activeCount - pausedCount;
        statusText = QString("状态: %1个任务运行中").arg(runningCount);
    }
    m_progressDisplayWidget->setStatusText(statusText);
}

void ClientMainWindow::addLogEntry(const QString& message, const QString& type)
{
    if (m_logWidget) {
        m_logWidget->addLogEntry(message, type);
    }
    
    // 同时在状态栏显示消息
    m_statusLabel->setText(message);
}

void ClientMainWindow::connectToServer()
{
    if (m_isConnected) {
        addLogEntry("已经连接到服务器", "WARNING");
        return;
    }

    QString serverAddress = m_connectionControlWidget->getServerAddress();
    if (serverAddress.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入服务器地址");
        return;
    }
    if (!m_zmqClient) {
        addLogEntry("ZMQ客户端未初始化", "ERROR");
        return;
    }

    m_zmqClient->setServerAddress(serverAddress);

    if (m_zmqClient->isConnected()) {
        m_zmqClient->disconnectFromServer();
    }

    addLogEntry(QString("正在连接到服务器: %1").arg(serverAddress));
    
    // 更新连接控制组件状态为连接中
    m_connectionControlWidget->setConnected(false); // 先设置为未连接，显示中间状态
    // 注意：这里可以扩展 ConnectionControlWidget 添加一个 "connecting" 状态

    bool success = m_zmqClient->connectToServer();

    if (success) {
        addLogEntry("ZMQ连接已建立，等待服务器响应...", "INFO");
    } else {
        addLogEntry("连接服务器失败，请检查服务器是否运行", "ERROR");
        updateConnectionStatus(false);
    }
}
void ClientMainWindow::disconnectFromServer()
{
    if (!m_isConnected) {
        addLogEntry("未连接到服务器", "WARNING");
        return;
    }

    addLogEntry("正在断开服务器连接...");

    // 停止所有擦除任务
    if (m_isErasing) {
        stopErasure();
    }

    m_connectionTimer->stop();

    // 断开 ZMQ 连接（会自动停止心跳）
    if (m_zmqClient) {
        m_zmqClient->disconnectFromServer();
    }

    updateConnectionStatus(false);
    addLogEntry("已断开服务器连接", "INFO");
}

void ClientMainWindow::sendClientRegister()
{
    if (!m_isConnected || !m_zmqClient) {
        addLogEntry("未连接到服务器，无法发送注册信息", "ERROR");
        return;
    }

    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
    clientInfo.set_hostname(QHostInfo::localHostName().toStdString());
    clientInfo.set_ip_address(Utils::getLocalIPString().toStdString());
    
    clientInfo.set_os_type(QSysInfo::productType().toStdString());
    clientInfo.set_os_version(QSysInfo::productVersion().toStdString());
    clientInfo.set_timestamp(QDateTime::currentMSecsSinceEpoch());

#if defined(DB_SUPPORT)
    // 保存客户端信息到数据库
    if (CheckResultDB::GetInstance().InsertClientInfo(clientInfo)) {
        qDebug() << "客户端信息已保存到数据库:" << QString::fromStdString(clientInfo.client_id());
    } else {
        qWarning() << "保存客户端信息到数据库失败";
    }
#endif
    addLogEntry("正在发送客户端注册信息...");

    if (m_zmqClient->sendClientRegister(clientInfo)) {
        addLogEntry("客户端注册信息已发送，等待服务器确认", "INFO");
    } else {
        addLogEntry("客户端注册信息发送失败", "ERROR");
        m_isConnected = false;
        updateConnectionStatus(false);
    }
}

void ClientMainWindow::generateDiskInfo()
{
    generateSimulatedDisks(m_clientList);

    m_diskInfoWidget->setClientList(m_clientList);
    
    // 统计生成的磁盘总数
    int totalDiskCount = 0;
    for (const auto& client : m_clientList) {
        totalDiskCount += client.diskList.size();
    }
    
    addLogEntry(QString("重新生成了 %1 个模拟磁盘").arg(totalDiskCount));
}

void ClientMainWindow::sendDiskInfo()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "错误", "请先连接到服务器");
        return;
    }
    for (int i =0; i< m_clientList.size(); i++) {
        ClientInfoWrap& client =  m_clientList[i];
        qDebug() << QString::fromStdString(client.clientInfo.server_client_id());

        if (client.diskList.isEmpty()) {
            QMessageBox::warning(this, "错误", "没有可发送的磁盘信息");
            return;
        }

        addLogEntry("正在发送磁盘信息...");

        // 将 ClientDiskInfo 列表转换为 protobuf DiskInfo 列表
        QList<DiskInfo> diskList;
        for (int i = 0; i < client.diskList.size(); ++i) {
            DiskInfo info= client.diskList[i].info;
            info.set_server_client_id(client.clientInfo.server_client_id());
            diskList.append(info);
        }

#if defined(DB_SUPPORT)
        // 批量保存磁盘信息到数据库
        // 这里将 QList<DiskInfo> 转换为 std::vector<DiskInfo>
        std::vector<DiskInfo> diskVector(diskList.begin(), diskList.end());

        if (CheckResultDB::GetInstance().BatchInsertDiskInfos(diskVector)) {
            qDebug() << "已保存" << diskVector.size() << "个磁盘信息到数据库";
        } else {
            qWarning() << "保存磁盘信息到数据库失败";
        }
#endif

        if (m_zmqClient->sendDiskInfo(client.clientInfo, diskList)) {
            addLogEntry(QString("已发送 %1 个磁盘的信息").arg(diskList.size()), "SUCCESS");
        } else {
            addLogEntry("发送磁盘信息失败", "ERROR");
        }
    }

}

void ClientMainWindow::startErasure()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "错误", "请先连接到服务器");
        return;
    }
    
    if (m_selectedDiskIds.isEmpty()) {
        QMessageBox::warning(this, "错误", "请先选择要擦除的磁盘");
        return;
    }
    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
    QStringList disksToErase;
    for (int i = 0; i < m_selectedDiskIds.size(); ++i) {
        const QString& diskId = m_selectedDiskIds[i];
        if (m_taskManager->hasActiveTaskForDisk(QString::fromStdString(clientInfo.client_id()), diskId)) {
            addLogEntry(QString("磁盘 %1 已有活动任务，跳过").arg(diskId), "WARNING");
            continue;
        }
        disksToErase.append(diskId);
    }
    
    if (disksToErase.isEmpty()) {
        QMessageBox::warning(this, "错误", "所选磁盘都已有活动任务");
        return;
    }
    
    // 从 DiskInfoWidget 中获取擦除控制组件
    ErasureControlWidget* erasureCtrl = m_diskInfoWidget->getErasureControlWidget();
    QString method = erasureCtrl->getErasureMethod();
    int passCount = erasureCtrl->getPassCount();
    bool verificationEnabled = erasureCtrl->isVerificationEnabled();
    
    int createdCount = 0;
    for (int i = 0; i < disksToErase.size(); ++i) {
        const QString& diskId = disksToErase[i];
        DiskInfo diskInfo;
        bool found = false;
        for (int i =0; i< m_clientList.size(); i++) {
            ClientInfoWrap client =  m_clientList[i];
            for (int j = 0; j < client.diskList.size(); ++j) {
                const ClientDiskInfo& wrapper = client.diskList[j];
                if (QString::fromStdString(wrapper.info.disk_id()) == diskId) {
                    diskInfo = wrapper.info;
                    found = true;
                    break;
                }
            }

            if (!found) {
                addLogEntry(QString("未找到磁盘信息: %1").arg(diskId), "ERROR");
                continue;
            }

            QString taskId = "";
            ErasureTask newTask = m_taskManager->createTask(
                        taskId, // taskId 由管理器生成，传空
                        QString::fromStdString(clientInfo.client_id()),
                        diskId,
                        QString::fromStdString(diskInfo.model()),
                        method,
                        passCount,
                        diskInfo.total_size(),
                        verificationEnabled
                        );

            // 检查任务是否创建成功 (通常通过检查 task_id 是否为空来判断)
            if (!newTask.task_id().empty()) {
                createdCount++;
                addLogEntry(QString("已创建擦除任务: %1 - %2 (%3遍)")
                            .arg(diskId)
                            .arg(QString::fromStdString(diskInfo.model()))
                            .arg(passCount), "SUCCESS");

                sendErasureStart(newTask);
            } else {
                addLogEntry(QString("创建任务失败: %1").arg(diskId), "ERROR");
            }
        }
    }
    
    if (createdCount > 0) {
        m_isErasing = true;
        m_isPaused = false;
        updateErasureControlStates();
        
        if (!m_progressTimer->isActive()) {
            m_progressTimer->start(PROGRESS_UPDATE_INTERVAL);
        }
        
        addLogEntry(QString("成功创建 %1 个擦除任务").arg(createdCount), "SUCCESS");
    }
}

void ClientMainWindow::stopErasure()
{
    if (!m_isErasing) {
        addLogEntry("没有正在进行的擦除任务", "WARNING");
        return;
    }

    addLogEntry("停止所有擦除任务", "WARNING");

    QList<ErasureTask> activeTasks = m_taskManager->getActiveTasks();
    int stoppedCount = 0;
    
    for (int i = 0; i < activeTasks.size(); ++i) {
        const ErasureTask& task = activeTasks[i];
        QString taskId = QString::fromStdString(task.task_id());
        QString diskId = QString::fromStdString(task.disk_id());
        QString erasureMethod = QString::fromStdString(task.erasure_method());
        if (m_taskManager->stopTask(taskId)) {
            sendErasureComplete(taskId, diskId, false);
            addLogEntry(QString("任务已停止: %1 (%2)").arg(diskId, erasureMethod), "WARNING");
            stoppedCount++;
        }
    }

    m_isErasing = false;
    m_isPaused = false;
    m_progressTimer->stop();

    updateErasureControlStates();
    updateProgressDisplay();
    
    addLogEntry(QString("已停止 %1 个任务").arg(stoppedCount), "INFO");
}

void ClientMainWindow::pauseErasure()
{
    if (!m_isErasing || m_isPaused) {
        return;
    }

    addLogEntry("暂停所有擦除任务");

    QList<ErasureTask> activeTasks = m_taskManager->getActiveTasks();
    int pausedCount = 0;
    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
    for (int i = 0; i < activeTasks.size(); ++i) {
        const ErasureTask& task = activeTasks[i];
        QString taskId = QString::fromStdString(task.task_id());
        QString diskId = QString::fromStdString(task.disk_id());
        if (task.status() != TaskStatus::PAUSED) {
            if (m_taskManager->pauseTask(taskId)) {
                // 获取更新后的任务信息并发送
                ErasureTask updatedTask = m_taskManager->getTask(taskId);
                if (m_isConnected && m_zmqClient) {
                    m_zmqClient->sendErasureTask(clientInfo, updatedTask);
                }
                pausedCount++;
            }
        }
    }

    m_isPaused = true;
    m_progressTimer->stop();

    updateErasureControlStates();
    addLogEntry(QString("已暂停 %1 个任务").arg(pausedCount), "INFO");
}

void ClientMainWindow::resumeErasure()
{
    if (!m_isErasing || !m_isPaused) {
        return;
    }

    addLogEntry("恢复所有擦除任务");

    QList<ErasureTask> activeTasks = m_taskManager->getActiveTasks();
    int resumedCount = 0;
    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
    for (int i = 0; i < activeTasks.size(); ++i) {
        const ErasureTask& task = activeTasks[i];
        QString taskId = QString::fromStdString(task.task_id());
        QString diskId = QString::fromStdString(task.disk_id());
        if (task.status() == TaskStatus::PAUSED) {
            if (m_taskManager->resumeTask(taskId)) {
                // 获取更新后的任务信息并发送
                ErasureTask updatedTask = m_taskManager->getTask(taskId);
                if (m_isConnected && m_zmqClient) {
                    m_zmqClient->sendErasureTask(clientInfo, updatedTask);
                }
                resumedCount++;
            }
        }
    }

    m_isPaused = false;
    m_progressTimer->start(PROGRESS_UPDATE_INTERVAL);

    updateErasureControlStates();
    addLogEntry(QString("已恢复 %1 个任务").arg(resumedCount), "INFO");
}

void ClientMainWindow::updateErasureProgress()
{
    if (!m_isErasing || !m_taskManager) {
        return;
    }

    QList<ErasureTask> activeTasks = m_taskManager->getActiveTasks();
    
    if (activeTasks.isEmpty()) {
        m_isErasing = false;
        m_progressTimer->stop();
        updateErasureControlStates();
        addLogEntry("所有任务已完成或停止", "INFO");
        return;
    }

    bool allCompleted = true;
    
    for (int i = 0; i < activeTasks.size(); ++i) {
        ErasureTask& task = activeTasks[i];
        QString taskId = QString::fromStdString(task.task_id());
        QString diskId = QString::fromStdString(task.disk_id());
        
        if (task.status() == TaskStatus::PAUSED) {
            allCompleted = false;
            continue;
        }
        
        if (task.status() != TaskStatus::IN_PROGRESS &&
                task.status() != TaskStatus::STARTED) {
            continue;
        }
        
        double increment = Utils::generateRandomInt(2, 8);
        int currentProgress = task.progress_percent();
        int newProgress = qMin(10000, currentProgress + static_cast<int>(increment * 100));
        qint64 newErasedBytes = static_cast<qint64>(task.total_bytes() * newProgress / 10000.0);
        
        m_taskManager->updateTaskProgress(taskId, newProgress / 100.0, newErasedBytes);
        
        sendErasureProgress(taskId, diskId, newProgress / 100.0);
        
        if (newProgress >= 10000) {
            newProgress = 10000;
            newErasedBytes = task.total_bytes();
            
            bool success = m_taskManager->completeTask(taskId, true, task.verification_enabled());
            if (success) {
                sendErasureComplete(QString::fromStdString(task.task_id()), QString::fromStdString(task.disk_id()), true);
                addLogEntry(QString("✓ 擦除任务完成: %1 - %2").arg(QString::fromStdString(task.disk_id()), QString::fromStdString(task.erasure_method())), "SUCCESS");
            }
        } else {
            allCompleted = false;
        }
    }
    
    if (allCompleted) {
        m_isErasing = false;
        m_progressTimer->stop();
        updateErasureControlStates();
        addLogEntry("🎉 所有擦除任务已完成", "SUCCESS");
    }
    
    updateProgressDisplay();
}
void ClientMainWindow::sendErasureStart(const ErasureTask &taskInfo)
{
    if (!m_isConnected || !m_zmqClient) {
        addLogEntry("未连接服务器，无法发送擦除开始消息", "ERROR");
        return;
    }
    
    // 发送任务信息（使用 ErasureTask）
    QString diskId = QString::fromStdString(taskInfo.disk_id());
    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
    if (m_zmqClient->sendErasureTask(clientInfo, taskInfo)) {
        qDebug() << "擦除任务消息已发送:" << diskId;
    } else {
        addLogEntry(QString("发送擦除任务消息失败: %1").arg(diskId), "ERROR");
    }
}


void ClientMainWindow::sendErasureProgress(const QString& taskId, const QString& diskId, double progress)
{
    if (!m_isConnected || !m_zmqClient) {
        return;
    }

    ErasureTask task = m_taskManager->getTask(taskId);
    if (task.task_id().empty()) {
        qDebug() << "任务不存在:" << taskId;
        return;
    }
    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
    // 创建 ErasureProgress 消息
    ErasureProgress progressInfo;
    progressInfo.set_task_id(taskId.toStdString());
    progressInfo.set_server_client_id(clientInfo.server_client_id());

    progressInfo.set_disk_id(diskId.toStdString());
    progressInfo.set_current_pass(1);  // 当前遍数（简化处理）
    progressInfo.set_total_passes(task.pass_count());
    progressInfo.set_current_step(0);  // 当前步骤
    progressInfo.set_total_steps_in_pass(1);  // 总步骤数
    progressInfo.set_step_description("擦除中");
    progressInfo.set_bytes_processed(task.erased_bytes());
    progressInfo.set_total_bytes(task.total_bytes());
    progressInfo.set_progress_percent(static_cast<int32_t>(progress * 100.0));  // 转换为 0-10000
    progressInfo.set_erased_bytes(task.erased_bytes());
    progressInfo.set_speed_mb_per_s(task.speed_mb_per_s());
    progressInfo.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    // 发送进度信息（使用 ErasureProgress）
    if (!m_zmqClient->sendErasureProgress(clientInfo, progressInfo)) {
        addLogEntry(QString("发送进度失败: %1").arg(diskId), "ERROR");
    }
}


void ClientMainWindow::sendErasureComplete(const QString& taskId, const QString& diskId, bool success)
{
    if (!m_isConnected || !m_zmqClient) {
        return;
    }

    ErasureTask task = m_taskManager->getTask(taskId);
    if (task.task_id().empty()) {
        qDebug() << "任务不存在:" << taskId;
        return;
    }

    // 更新任务状态
    task.set_status(success ? TaskStatus::COMPLETED
                            : TaskStatus::FAILED);
    task.set_progress_percent(success ? 10000 : task.progress_percent());
    task.set_erased_bytes(success ? task.total_bytes() : task.erased_bytes());
    task.set_estimated_end_time(QDateTime::currentMSecsSinceEpoch());
    
    // 发送任务完成信息（使用 ErasureTask）
    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
    if (m_zmqClient->sendErasureComplete(clientInfo, task)) {
        addLogEntry(QString("擦除完成消息已发送: %1 (%2)").arg(diskId, success ? "成功" : "失败"),
                    success ? "SUCCESS" : "ERROR");
    } else {
        addLogEntry(QString("发送擦除完成消息失败: %1").arg(diskId), "ERROR");
    }
}

void ClientMainWindow::onDiskCheckStateChanged(QTreeWidgetItem *item, int column)
{
    if (!item || column != 0) {
        return;
    }
    for (int i =0; i< m_clientList.size(); i++) {
        ClientInfoWrap client =  m_clientList[i];
        if (m_isErasing) {
            // 恢复之前的状态
            QString diskId = item->data(0, Qt::UserRole).toString();
            bool wasSelected = false;
            for (int i = 0; i < client.diskList.size(); ++i) {
                if (QString::fromStdString(client.diskList[i].info.disk_id()) == diskId) {
                    wasSelected = client.diskList[i].isSelected;
                    break;
                }
            }
            item->setCheckState(0, wasSelected ? Qt::Checked : Qt::Unchecked);
            QMessageBox::information(this, "提示", "擦除进行中，无法更改磁盘选择");
            return;
        }

        QString diskId = item->data(0, Qt::UserRole).toString();
        bool isChecked = (item->checkState(0) == Qt::Checked);

        // 更新 client.diskList 中的状态
        for (int i = 0; i < client.diskList.size(); ++i) {
            if (QString::fromStdString(client.diskList[i].info.disk_id()) == diskId) {
                client.diskList[i].isSelected = isChecked;
                break;
            }
        }

        // 更新 m_selectedDiskIds 列表
        m_selectedDiskIds.clear();
        for (int i = 0; i < client.diskList.size(); ++i) {
            if (client.diskList[i].isSelected) {
                m_selectedDiskIds.append(QString::fromStdString(client.diskList[i].info.disk_id()));
            }
        }

        if (isChecked) {
            addLogEntry(QString("选中磁盘: %1").arg(diskId));
        } else {
            addLogEntry(QString("取消选中磁盘: %1").arg(diskId));
        }
        updateErasureControlStates();
    }
}

void ClientMainWindow::onDiskSelectionChanged()
{
    // 如果正在擦除，禁止更改选择
    if (m_isErasing) {
        // 恢复之前的选择状态（通过 widget）
        m_diskInfoWidget->restorePreviousSelection();
        return;
    }

    // 获取选中的磁盘 ID 列表
    QStringList selectedDiskIds = m_diskInfoWidget->getSelectedDiskIds();
    
    // 更新选中状态
    for (int i =0; i< m_clientList.size(); i++) {
        ClientInfoWrap client =  m_clientList[i];
        for (int i = 0; i < client.diskList.size(); ++i) {
            QString diskId = QString::fromStdString(client.diskList[i].info.disk_id());
            bool wasSelected = client.diskList[i].isSelected;
            bool isSelected = selectedDiskIds.contains(diskId);

            if (wasSelected != isSelected) {
                client.diskList[i].isSelected = isSelected;
                if (isSelected) {
                    m_selectedDiskIds.append(diskId);
                    addLogEntry(QString("选中磁盘: %1").arg(diskId));
                } else {
                    m_selectedDiskIds.removeAll(diskId);
                    addLogEntry(QString("取消选中磁盘: %1").arg(diskId));
                }
            }
        }
    }
    
    updateErasureControlStates();
}

// 擦除方法变更
void ClientMainWindow::onErasureMethodChanged(const QString& method, int pass)
{

    addLogEntry(QString("擦除方法已更改: %1 (%2 遍)").arg(method).arg(pass));
}

void ClientMainWindow::onConnectionStatusChanged()
{
    if (!m_isConnected || !m_zmqClient) {
        return;
    }

    if (!m_zmqClient->isConnected()) {
        addLogEntry("检测到连接断开", "WARNING");
        m_isConnected = false;
        updateConnectionStatus(false);
        
        if (m_zmqClient) {
            m_zmqClient->stopHeartbeat();
        }
        m_connectionTimer->stop();
    } else {
        qDebug() << "连接状态检查: 正常";
    }
}

void ClientMainWindow::updateConnectionStatus(bool connected)
{
    m_isConnected = connected;
    
    // 使用连接控制组件更新状态
    if (m_connectionControlWidget) {
        m_connectionControlWidget->setConnected(connected);
    }
    
    m_connectionStatusIcon->setPixmap(QPixmap(connected ? ":/icons/connected.png" : ":/icons/disconnected.png").scaled(16, 16));
    updateErasureControlStates();
}

void ClientMainWindow::updateErasureControlStates()
{
    // 1. 获取当前状态标志
    bool hasSelection = !m_selectedDiskIds.isEmpty(); // 是否有选中的磁盘
    bool hasActiveTasks = !m_taskManager->getActiveTasks().isEmpty(); // 是否有正在运行的任务

    // 2. 计算按钮的可用状态
    // 开始按钮：已连接 + 有选中磁盘 + 没有正在运行的任务
    bool canStart = m_isConnected && hasSelection && !hasActiveTasks;

    // 停止按钮：正在擦除中 + 有活动任务
    bool canStop = m_isErasing && hasActiveTasks;

    // 暂停按钮：正在擦除中 + 未暂停 + 有活动任务
    bool canPause = m_isErasing && !m_isPaused && hasActiveTasks;

    // 恢复按钮：正在擦除中 + 已暂停 + 有活动任务
    bool canResume = m_isErasing && m_isPaused && hasActiveTasks;

    // 3. 获取擦除控制组件指针
    ErasureControlWidget* erasureCtrl = m_diskInfoWidget->getErasureControlWidget();

    // 设置“开始”按钮是否可点击
    erasureCtrl->setStartButtonEnabled(canStart);
    erasureCtrl->setStopButtonEnabled(canStop);
    erasureCtrl->setPauseButtonEnabled(canPause);
    erasureCtrl->setResumeButtonEnabled(canResume);
    erasureCtrl->setControlsEnabled(canStart || m_isErasing); // 修正：如果在擦除中，也应该允许操作（如停止/暂停），所以不应完全禁用
}
void ClientMainWindow::clearLog()
{
    if (m_logWidget) {
        m_logWidget->clearLog();
    }
}

void ClientMainWindow::exportLog()
{
    if (m_logWidget) {
        m_logWidget->exportLog();
    }
}

void ClientMainWindow::refreshDiskList()
{
    // 更新客户端列表
    m_diskInfoWidget->setClientList(m_clientList);
    addLogEntry("磁盘列表已刷新");
}

void ClientMainWindow::showAbout()
{
    QMessageBox::about(this, "关于",
                       "Eraser 客户端 v1.0\n\n"
                       "基于 Qt5.6、libzmq 和 Protocol Buffers 的磁盘擦除客户端\n\n"
                       "功能特性:\n"
                       "• 服务器连接管理\n"
                       "• 磁盘信息模拟和发送\n"
                       "• 磁盘擦除控制\n"
                       "• 实时进度监控\n"
                       "• 日志记录和导出\n"
                       "• 多客户端分组展示");
}

// 测试多客户端模式
void ClientMainWindow::testMultiClientMode()
{
    addLogEntry("正在测试多客户端模式...", "INFO");
    
    // 创建多个模拟客户端
    m_clientList.clear();

    for (int i = 1; i <= 3; ++i) {
        ClientInfoWrap clientWrap;
        
        // 设置客户端信息
        clientWrap.clientInfo.set_client_id(QString("CLIENT_%1").arg(i).toStdString());
        clientWrap.clientInfo.set_hostname(QString("工作站-%1").arg(i).toStdString());
        clientWrap.clientInfo.set_ip_address(QString("192.168.1.%1").arg(100 + i).toStdString());
        clientWrap.clientInfo.set_os_type("Windows");
        clientWrap.clientInfo.set_os_version("Windows 10");
        
        // 为每个客户端生成 2-4 个磁盘
        int diskCount = 2 + (i % 3);
        for (int j = 1; j <= diskCount; ++j) {
            DiskInfo disk;
            disk.set_disk_id(QString("DISK_%1_%2").arg(i).arg(j, 2, 10, QChar('0')).toStdString());
            disk.set_model(QString("Samsung SSD %1TB").arg(j).toStdString());
            disk.set_serial_number(QString("SN%1%2").arg(i).arg(j).toStdString());
            disk.set_interface_type("NVMe");
            disk.set_total_size(static_cast<qint64>(j) * 500LL * 1024 * 1024 * 1024);
            disk.set_used_size(disk.total_size() * 0.6);
            disk.set_free_size(disk.total_size() - disk.used_size());
            disk.set_file_system("NTFS");
            disk.set_is_system_disk(j == 1);
            disk.set_is_removable(false);
            disk.set_mount_point(QString("%1:").arg(QChar('C' + j - 1)).toStdString());
            disk.set_health_status(85.0 + i * 2);
            
            ClientDiskInfo wrapper;
            wrapper.info = disk;
            wrapper.isSelected = false;
            wrapper.progressPercent = 0;
            wrapper.status = "空闲";
            
            clientWrap.diskList.append(wrapper);
        }
        
        m_clientList.append(clientWrap);
    }
    
    // 更新显示
    m_diskInfoWidget->setClientList(m_clientList);
    int count = 0;
    for (const auto& clientWrap : m_clientList) {
        count +=clientWrap.diskList.size();
    }
    
    addLogEntry(QString("已切换到多客户端模式：共 %1 个客户端，%2 个磁盘")
                .arg(m_clientList.size())
                .arg(count), "SUCCESS");
}

void ClientMainWindow::onClientStarted()
{
    m_isConnected = true;
    updateConnectionStatus(true);

    sendClientRegister();

    if (m_zmqClient) {
        m_zmqClient->startHeartbeat(HEARTBEAT_INTERVAL);
        addLogEntry(QString("心跳已启动 (间隔: %1ms)").arg(HEARTBEAT_INTERVAL), "INFO");
    }

    m_connectionTimer->start(CONNECTION_CHECK_INTERVAL);

    addLogEntry("已成功连接到服务器并注册客户端", "SUCCESS");
}
void ClientMainWindow::onClientRegister(const QString& server_clinent_id)
{
    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
   clientInfo.set_server_client_id(server_clinent_id.toStdString());
    generateDiskInfo();//可能需要注册成功后再去获取磁盘信息
//    sendDiskInfo();
}
void ClientMainWindow::onClientStopped()
{
    m_isConnected = false;
    updateConnectionStatus(false);

    if (m_isErasing) {
        m_isErasing = false;
        m_isPaused = false;
        m_progressTimer->stop();
        updateErasureControlStates();
        addLogEntry("连接断开，擦除任务已停止", "WARNING");
    }

    addLogEntry("已断开与服务器的连接", "INFO");
}

void ClientMainWindow::onErrorOccurred(const QString& error)
{
    addLogEntry(QString("ZMQ客户端错误: %1").arg(error), "ERROR");
    QMessageBox::critical(this, "错误", error);
}

void ClientMainWindow::onTaskCreated(const QByteArray& taskData)
{
    // 反序列化获取任务信息
    ErasureTask taskInfo;
    if (!taskInfo.ParseFromArray(taskData.constData(), taskData.size())) {
        qWarning() << "反序列化任务信息失败";
        return;
    }
    
    QString taskId = QString::fromStdString(taskInfo.task_id());
    QString diskId = QString::fromStdString(taskInfo.disk_id());
    
    addLogEntry(QString("任务已创建: ID=%1, 磁盘=%2").arg(taskId, diskId), "INFO");
    
    ErasureTask task;
    task.set_task_id(taskId.toStdString());
    task.set_disk_id(diskId.toStdString());
    task.mutable_disk_info()->set_model(taskInfo.disk_info().model());
    task.set_erasure_method(taskInfo.erasure_method());
    task.set_pass_count(taskInfo.pass_count());
    task.set_total_bytes(taskInfo.total_bytes());
    task.set_status(taskInfo.status());
    task.set_progress_percent(taskInfo.progress_percent());
    task.set_start_time(taskInfo.start_time());
    
    m_erasureTasks[diskId] = task;
    
    // 更新UI状态
    m_isErasing = true;
    m_isPaused = false;
    updateErasureControlStates();
    m_progressTimer->start(PROGRESS_UPDATE_INTERVAL);
    updateProgressDisplay();
}

void ClientMainWindow::onTaskProgressUpdated(const QByteArray& taskData)
{
    // 反序列化获取任务信息
    ErasureTask taskInfo;
    if (!taskInfo.ParseFromArray(taskData.constData(), taskData.size())) {
        qWarning() << "反序列化任务进度失败";
        return;
    }
    
    QString taskId = QString::fromStdString(taskInfo.task_id());
    QString diskId = QString::fromStdString(taskInfo.disk_id());

    if (diskId.isEmpty() || !m_erasureTasks.contains(diskId)) {
        return;
    }

    ErasureTask& task = m_erasureTasks[diskId];
    task.set_progress_percent(taskInfo.progress_percent());
    task.set_erased_bytes(taskInfo.erased_bytes());
    task.set_speed_mb_per_s(taskInfo.speed_mb_per_s());

    // 更新表格显示
    updateProgressDisplay();
    
    // 记录关键进度点
    int progressPercent = taskInfo.progress_percent() / 100;
    if (progressPercent % 10 == 0 && progressPercent > 0 && progressPercent < 100) {
        addLogEntry(QString("任务 %1 进度: %2%").arg(taskId).arg(taskInfo.progress_percent() / 100.0, 0, 'f', 1), "INFO");
    }
}

void ClientMainWindow::onTaskStatusChanged(const QByteArray& taskData)
{
    // 反序列化获取任务信息
    ErasureTask taskInfo;
    if (!taskInfo.ParseFromArray(taskData.constData(), taskData.size())) {
        qWarning() << "反序列化任务状态失败";
        return;
    }
    
    QString taskId = QString::fromStdString(taskInfo.task_id());
    QString diskId = QString::fromStdString(taskInfo.disk_id());
    TaskStatus status = taskInfo.status();

    if (diskId.isEmpty() || !m_erasureTasks.contains(diskId)) {
        return;
    }

    ErasureTask& task = m_erasureTasks[diskId];
    task.set_status(status);
    task.set_error_message(taskInfo.error_message());

    QString statusStr = Utils::getStatusString(status);

    addLogEntry(QString("任务 %1 (磁盘 %2) 状态变更: %3").arg(taskId, diskId, statusStr), "INFO");

    // 根据状态更新UI控件
    if (status == TaskStatus::PAUSED) {
        m_isPaused = true;
        m_progressTimer->stop();
    } else if (status == TaskStatus::IN_PROGRESS) {
        m_isPaused = false;
        if (m_isErasing) {
            m_progressTimer->start(PROGRESS_UPDATE_INTERVAL);
        }
    } else if (status == TaskStatus::STOPPED ||
               status == TaskStatus::COMPLETED ||
               status == TaskStatus::FAILED) {
        // 检查是否所有任务都结束
        bool allFinished = true;
        for (int i = 0; i < m_selectedDiskIds.size(); ++i) {
            const QString& dId = m_selectedDiskIds[i];
            if (m_erasureTasks.contains(dId)) {
                TaskStatus s = m_erasureTasks[dId].status();
                if (s != TaskStatus::COMPLETED &&
                        s != TaskStatus::FAILED &&
                        s != TaskStatus::STOPPED) {
                    allFinished = false;
                    break;
                }
            }
        }
        
        if (allFinished) {
            m_isErasing = false;
            m_isPaused = false;
            m_progressTimer->stop();
            updateErasureControlStates();
        }
    }
    
    updateErasureControlStates();
    updateProgressDisplay();
}

void ClientMainWindow::onTaskCompleted(const QByteArray& taskData)
{
    // 反序列化获取任务信息
    ErasureTask taskInfo;
    if (!taskInfo.ParseFromArray(taskData.constData(), taskData.size())) {
        qWarning() << "反序列化任务完成信息失败";
        return;
    }
    
    QString taskId = QString::fromStdString(taskInfo.task_id());
    QString diskId = QString::fromStdString(taskInfo.disk_id());
    bool success = (taskInfo.status() == TaskStatus::COMPLETED);

    if (diskId.isEmpty() || !m_erasureTasks.contains(diskId)) {
        return;
    }

    ErasureTask& task = m_erasureTasks[diskId];
    task.set_progress_percent(10000);
    task.set_erased_bytes(task.total_bytes());
    task.set_status(success ? TaskStatus::COMPLETED : TaskStatus::FAILED);
    task.set_verification_passed(taskInfo.verification_passed());
    task.set_error_message(taskInfo.error_message());

    if (success) {
        addLogEntry(QString("任务 %1 (磁盘 %2) 已成功完成").arg(taskId, diskId), "SUCCESS");
    } else {
        addLogEntry(QString("任务 %1 (磁盘 %2) 执行失败: %3").arg(taskId, diskId, "task.errorMessage"), "ERROR");
    }
#if defined(DB_SUPPORT)
    // 更新数据库中的任务状态
    if (CheckResultDB::GetInstance().UpdateTask(task)) {
        qDebug() << "任务完成状态已更新到数据库:" << taskId;
    } else {
        qWarning() << "更新任务完成状态到数据库失败:" << taskId;
    }
#endif
    // 检查是否所有选中的任务都已完成
    bool allFinished = true;
    for (int i = 0; i < m_selectedDiskIds.size(); ++i) {
        const QString& dId = m_selectedDiskIds[i];
        if (m_erasureTasks.contains(dId)) {
            TaskStatus s = m_erasureTasks[dId].status();
            if (s != TaskStatus::COMPLETED &&
                    s != TaskStatus::FAILED &&
                    s != TaskStatus::STOPPED) {
                allFinished = false;
                break;
            }
        }
    }

    if (allFinished) {
        m_isErasing = false;
        m_isPaused = false;
        m_progressTimer->stop();
        updateErasureControlStates();
    }

    updateProgressDisplay();
}
#if defined(DB_SUPPORT)
void ClientMainWindow::restoreUnfinishedTasksFromDB()
{
    addLogEntry("正在从数据库恢复未完成的任务...");
    ClientInfoWrap &localClient = m_clientList.first();
    ClientInfo &clientInfo = localClient.clientInfo;
    // 获取当前客户端 ID
    QString clientId = QString::fromStdString(clientInfo.client_id());
    
    // 查询该客户端的所有任务
    vector<ErasureTask> allTasks = CheckResultDB::GetInstance().SelectTasksByClient(clientId.toStdString());
    
    int restoredCount = 0;
    for (const auto& task : allTasks) {
        TaskStatus status = task.status();
        
        // 只恢夏未完成的任务（进行中、暂停状态）
        if (status == TaskStatus::IN_PROGRESS ||
                status == TaskStatus::STARTED ||
                status == TaskStatus::PAUSED) {
            
            QString taskId = QString::fromStdString(task.task_id());
            QString diskId = QString::fromStdString(task.disk_id());
            
            // 将任务添加到任务管理器
            m_taskManager->updateTaskInfo(taskId, task);
            
            // 更新本地任务缓存
            m_erasureTasks[diskId] = task;
            
            restoredCount++;
            addLogEntry(QString("恢夏任务: %1 (磁盘: %2, 状态: %3)")
                        .arg(taskId, diskId, Utils::getStatusString(status)), "INFO");
        }
    }
    
    if (restoredCount > 0) {
        addLogEntry(QString("成功恢夏 %1 个未完成的任务").arg(restoredCount), "SUCCESS");
        
        // 更新任务列表显示
        //        updateTaskListDisplay();
    } else {
        addLogEntry("没有找到需要恢夏的未完成任务", "INFO");
    }
}
#endif
