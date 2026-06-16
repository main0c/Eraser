#include "TaskListWidget.h"
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QHeaderView>
#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QCheckBox>
#include <algorithm>
#include <QMutex>
#include <QApplication>
#include <QThread>
#include <QTimer>
#include <QSplitter>
#include <QGroupBox>
#include <QTextEdit>
#include <QIcon>
#include "Utils.h"

TaskListWidget::TaskListWidget(QWidget *parent)
    : QWidget(parent)
    , m_taskListGroup(nullptr)
    , m_treeWidget(nullptr)
    , m_refreshBtn(nullptr)
    , m_clearCompletedBtn(nullptr)
    , m_taskCountLabel(nullptr)
    , m_clientFilterCombo(nullptr)
    , m_filterCombo(nullptr)
    , m_searchEdit(nullptr)
    , m_taskManager(nullptr)
    , m_viewModeFlat(false)
    , m_detailWidget(nullptr)
    , m_detailTextBrowser(nullptr)
    , m_viewModeToggleBtn(nullptr)
    , m_currentSelectedTaskId("")
    , m_filterClientId("")
{
    setupUI();
}

void TaskListWidget::setTaskManager(ErasureTaskManager* manager)
{
    m_taskManager = manager;
    refreshTaskList();
}

void TaskListWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);

    QWidget* listContainer = new QWidget(mainSplitter);
    QVBoxLayout* listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(5, 5, 5, 5);

    m_taskListGroup = new QGroupBox("擦除任务列表", listContainer);
    QVBoxLayout* groupLayout = new QVBoxLayout(m_taskListGroup);

    QHBoxLayout* filterLayout = new QHBoxLayout();
    
    filterLayout->addWidget(new QLabel("客户端筛选:"));
    m_clientFilterCombo = new QComboBox(m_taskListGroup);
    m_clientFilterCombo->addItem("全部客户端", "");
    filterLayout->addWidget(m_clientFilterCombo);
    
    m_filterCombo = new QComboBox(m_taskListGroup);
    m_filterCombo->addItem("全部状态");
    m_filterCombo->addItem("等待中");
    m_filterCombo->addItem("进行中");
    m_filterCombo->addItem("已完成");
    m_filterCombo->addItem("失败");
    m_searchEdit = new QLineEdit(m_taskListGroup);
    m_searchEdit->setPlaceholderText("搜索任务ID、磁盘ID或客户端ID...");

    QPushButton* searchBtn = new QPushButton("搜索", m_taskListGroup);
    
    m_treeWidget = new QTreeWidget(m_taskListGroup);
    m_treeWidget->setStyleSheet("QTreeWidget::item { height: 30px; }");

    m_viewModeToggleBtn = new QPushButton(m_taskListGroup);
    m_viewModeToggleBtn->setFlat(true);
    m_viewModeToggleBtn->setCheckable(true);
    m_viewModeToggleBtn->setFixedSize(20, 20);
    m_viewModeToggleBtn->setIconSize(QSize(12, 12));
    
    if (m_viewModeFlat) {
        m_viewModeToggleBtn->setIcon(QIcon(":/res/list.png"));
        m_viewModeToggleBtn->setToolTip("切换为分组视图");
        m_treeWidget->setRootIsDecorated(false);
    } else {
        m_viewModeToggleBtn->setIcon(QIcon(":/res/tree.png"));
        m_viewModeToggleBtn->setToolTip("切换为平铺视图");
        m_treeWidget->setRootIsDecorated(true);
    }

    connect(m_viewModeToggleBtn, &QPushButton::clicked, this, [this]() {
        m_viewModeFlat = !m_viewModeFlat;
        if (m_viewModeFlat) {
            m_viewModeToggleBtn->setIcon(QIcon(":/res/list.png"));
            m_viewModeToggleBtn->setToolTip("切换为分组视图");
            m_treeWidget->setRootIsDecorated(false);
        } else {
            m_viewModeToggleBtn->setIcon(QIcon(":/res/tree.png"));
            m_viewModeToggleBtn->setToolTip("切换为平铺视图");
            m_treeWidget->setRootIsDecorated(true);
        }
        refreshTaskList();
    });

    filterLayout->addWidget(new QLabel("状态筛选:"));
    filterLayout->addWidget(m_filterCombo);
    filterLayout->addWidget(m_searchEdit);
    filterLayout->addWidget(searchBtn);
    filterLayout->addWidget(m_viewModeToggleBtn);

    m_refreshBtn = new QPushButton(m_taskListGroup);

    m_refreshBtn->setIcon(QIcon(":/res/refresh.png"));
    m_refreshBtn->setToolTip("刷新列表");
    m_refreshBtn->setFixedSize(20, 20); // 设置固定大小以适应图标
    m_refreshBtn->setIconSize(QSize(12, 12));
    m_refreshBtn->setFlat(true);

    filterLayout->addWidget(m_refreshBtn);

    groupLayout->addLayout(filterLayout);

    QStringList headers;
    headers << "类型"
            << "任务ID"
            << "磁盘型号"   // Index 2
            << "客户端ID"   // Index 3
            << "擦除方法"   // Index 4
            << "进度"       // Index 5
            << "状态"       // Index 6
            << "开始时间"   // Index 7
            << "结束时间"   // Index 8
            << "已用时间";  // Index 9
    
    m_treeWidget->setHeaderLabels(headers);
    int columnCount = headers.size(); // 10 columns

    for (int i = 0; i < columnCount; ++i) {
        m_treeWidget->header()->setSectionResizeMode(i, QHeaderView::Interactive);
    }

    m_treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeWidget->header()->setSectionsClickable(true);
    m_treeWidget->setSortingEnabled(true);
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &TaskListWidget::onTreeItemClicked);

    groupLayout->addWidget(m_treeWidget);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_clearCompletedBtn = new QPushButton("清除已完成", m_taskListGroup);
    m_taskCountLabel = new QLabel("任务数量: 0", m_taskListGroup);

    buttonLayout->addWidget(m_clearCompletedBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_taskCountLabel);

    groupLayout->addLayout(buttonLayout);
    listLayout->addWidget(m_taskListGroup);

    m_detailWidget = new QWidget(mainSplitter);

    QVBoxLayout* detailLayout = new QVBoxLayout(m_detailWidget);
    detailLayout->setContentsMargins(5, 5, 5, 5);

    QGroupBox* detailGroup = new QGroupBox("任务详情", m_detailWidget);
    QVBoxLayout* detailGroupLayout = new QVBoxLayout(detailGroup);

    m_detailTextBrowser = new QTextEdit(detailGroup);
    m_detailTextBrowser->setReadOnly(true);

    detailGroupLayout->addWidget(m_detailTextBrowser);
    detailLayout->addWidget(detailGroup);

    mainSplitter->addWidget(listContainer);
    mainSplitter->addWidget(m_detailWidget);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(mainSplitter);

    connect(m_clientFilterCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this]() {
        QString clientId = m_clientFilterCombo->currentData().toString();
        onClientFilterChanged(clientId);
    });
    
    connect(m_refreshBtn, &QPushButton::clicked, this, &TaskListWidget::refreshTaskList);
    connect(m_clearCompletedBtn, &QPushButton::clicked, this, &TaskListWidget::clearCompletedTasks);
    connect(searchBtn, &QPushButton::clicked, this, &TaskListWidget::refreshTaskList);
    connect(m_filterCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &TaskListWidget::refreshTaskList);
}

void TaskListWidget::refreshTaskList()
{
    if (qApp->thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, "refreshTaskList", Qt::QueuedConnection);
        return;
    }
    if (!m_taskManager) {
        return;
    }
    
    QStringList clientIds = m_taskManager->getAllClientIds();
    
    m_clientFilterCombo->blockSignals(true);
    m_clientFilterCombo->clear();
    m_clientFilterCombo->addItem("全部客户端", "");
    for (const QString& clientId : clientIds) {
        m_clientFilterCombo->addItem(clientId, clientId);
    }
    
    if (!m_filterClientId.isEmpty() && clientIds.contains(m_filterClientId)) {
        m_clientFilterCombo->setCurrentText(m_filterClientId);
    }
    m_clientFilterCombo->blockSignals(false);
    
    updateTaskTree();
}

void TaskListWidget::onTaskCreated(const QByteArray& taskData)
{
    ErasureTask task;
    if (!task.ParseFromArray(taskData.constData(), taskData.size())) {
        qWarning() << "TaskListWidget: taskCreated 反序列化失败";
        return;
    }
    updateTaskEntry(task);
    updateTaskCountLabel();
}

void TaskListWidget::onTaskProgressUpdated(const QByteArray& taskData)
{ 
//TODO no ErasureTask is ErasureProgress
    ErasureTask taskInfo;
    if (!taskInfo.ParseFromArray(taskData.constData(), taskData.size())) {
        qWarning() << "TaskListWidget: taskProgressUpdated 反序列化失败";
        return;
    }
    updateTaskEntry(taskInfo);
    if (!m_currentSelectedTaskId.isEmpty() && m_currentSelectedTaskId == QString::fromStdString(taskInfo.task_id())) {
        updateDetailView(m_currentSelectedTaskId);
    }
}

void TaskListWidget::onTaskStatusChanged(const QByteArray& taskData)
{
    ErasureTask task;
    if (!task.ParseFromArray(taskData.constData(), taskData.size())) {
        qWarning() << "TaskListWidget: taskStatusChanged 反序列化失败";
        return;
    }
    updateTaskEntry(task);
    updateTaskCountLabel();
    if (!m_currentSelectedTaskId.isEmpty() && m_currentSelectedTaskId == QString::fromStdString(task.task_id())) {
        updateDetailView(m_currentSelectedTaskId);
    }
}

void TaskListWidget::onTaskCompleted(const QByteArray& taskData)
{
    ErasureTask task;
    if (!task.ParseFromArray(taskData.constData(), taskData.size())) {
        qWarning() << "TaskListWidget: taskCompleted 反序列化失败";
        return;
    }
    updateTaskEntry(task);
    updateTaskCountLabel();
    if (!m_currentSelectedTaskId.isEmpty() && m_currentSelectedTaskId == QString::fromStdString(task.task_id())) {
        updateDetailView(m_currentSelectedTaskId);
    }
}

void TaskListWidget::onClientFilterChanged(const QString& clientId)
{
    m_filterClientId = clientId;
    updateTaskTree();
}

void TaskListWidget::clearCompletedTasks()
{
    if (!m_taskManager) {
        return;
    }

    int completedCount = m_taskManager->getCompletedTasks().size();
    if (completedCount == 0) {
        QMessageBox::information(this, "提示", "没有已完成的任务");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
                this, "确认",
                QString("确定要清除 %1 个已完成的任务吗？").arg(completedCount),
                QMessageBox::Yes | QMessageBox::No
                );

    if (reply == QMessageBox::Yes) {
        if (m_filterClientId.isEmpty()) {
            m_taskManager->clearCompletedTasks();
        } else {
            m_taskManager->clearCompletedTasksByClient(m_filterClientId);
        }
        refreshTaskList();
        clearDetailView();
    }
}

void TaskListWidget::updateTaskTree()
{
    // 只允许主线程刷新UI
    if (qApp->thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, "updateTaskTree", Qt::QueuedConnection);
        return;
    }

    bool wasSortingEnabled = m_treeWidget->isSortingEnabled();
    m_treeWidget->setSortingEnabled(false);
    m_treeWidget->clear();

    QList<ErasureTask> allTasks = m_taskManager ? m_taskManager->getAllTasks() : QList<ErasureTask>();

    // 获取筛选条件
    QString filterText = m_searchEdit ? m_searchEdit->text().toLower() : "";
    int statusFilterIndex = m_filterCombo ? m_filterCombo->currentIndex() : 0;

    // 过滤任务
    QList<ErasureTask> filteredTasks;
    for (const ErasureTask& task : allTasks) {
        // 文本筛选
        if (!filterText.isEmpty()) {
            QString taskId = QString::fromStdString(task.task_id()).toLower();
            QString diskId = QString::fromStdString(task.disk_id()).toLower();
            QString clientId = QString::fromStdString(task.server_client_id()).toLower();
            QString diskModel = QString::fromStdString(task.disk_info().model()).toLower();
            QString erasureMethod = QString::fromStdString(task.erasure_method()).toLower();
            
            if (!taskId.contains(filterText) &&
                    !diskId.contains(filterText) &&
                    !clientId.contains(filterText) &&
                    !diskModel.contains(filterText) &&
                    !erasureMethod.contains(filterText)) {
                continue;
            }
        }
        // 状态筛选
        if (statusFilterIndex > 0) {
            TaskStatus requiredStatus;
            switch (statusFilterIndex) {
            case 1: requiredStatus = TaskStatus::PENDING; break;
            case 2: requiredStatus = TaskStatus::IN_PROGRESS; break; // 包含 STARTED 和 IN_PROGRESS
            case 3: requiredStatus = TaskStatus::COMPLETED; break;
            case 4: requiredStatus = TaskStatus::FAILED; break;
            default: continue;
            }

            if (statusFilterIndex == 2) {
                if (task.status() != TaskStatus::STARTED &&
                        task.status() != TaskStatus::IN_PROGRESS) {
                    continue;
                }
            } else if (task.status() != requiredStatus) {
                continue;
            }
        }
        filteredTasks.append(task);
    }



    if (m_viewModeFlat) {
        for (const ErasureTask& task : filteredTasks) {
            TaskTreeWidgetItem* item = new TaskTreeWidgetItem(m_treeWidget, TaskTreeWidgetItem::TypeTask);
            setupTaskItem(item, task);
        }
    } else {
        // 三级结构：客户端 -> 磁盘 -> 任务
        // 1. 按客户端ID分组
        QMap<QString, QList<ErasureTask>> clientTasksMap;
        for (const ErasureTask& task : filteredTasks) {
            clientTasksMap[QString::fromStdString(task.server_client_id())].append(task);
        }

        QStringList clientIds = clientTasksMap.keys();
        // 可选：对客户端组进行初始排序，以便展开时看起来有序
        std::sort(clientIds.begin(), clientIds.end());

        for (const QString& clientId : clientIds) {
            const QList<ErasureTask>& cTasks = clientTasksMap[clientId];
            if (cTasks.isEmpty()) continue;

            // 创建客户端节点 (Level 1)
            TaskTreeWidgetItem* clientItem = new TaskTreeWidgetItem(m_treeWidget, TaskTreeWidgetItem::TypeClient);
            clientItem->clientId = clientId;
            clientItem->setText(0, "💻 客户端");
            clientItem->setText(1, clientId); // Task ID column shows Client ID for group
            clientItem->setText(2, "-");      // Disk Model
            clientItem->setText(3, clientId); // Client ID
            clientItem->setText(4, "-");
            clientItem->setText(5, "-");
            clientItem->setText(6, "客户端组");
            clientItem->setText(7, "-");
            clientItem->setText(8, "-");
            clientItem->setText(9, "-");
            clientItem->setExpanded(true);
            
            QFont font;
            font.setBold(true);
            clientItem->setFont(0, font);
            clientItem->setFont(1, font);

            // 2. 在该客户端下，按磁盘ID分组
            QMap<QString, QList<ErasureTask>> diskTasksMap;
            for (const ErasureTask& task : cTasks) {
                diskTasksMap[QString::fromStdString(task.disk_id())].append(task);
            }

            QStringList diskIds = diskTasksMap.keys();
            // 可选：对磁盘组进行初始排序
            std::sort(diskIds.begin(), diskIds.end());

            for (const QString& diskId : diskIds) {
                const QList<ErasureTask>& dTasks = diskTasksMap[diskId];
                if (dTasks.isEmpty()) continue;

                // 创建磁盘节点 (Level 2)
                TaskTreeWidgetItem* diskItem = new TaskTreeWidgetItem(clientItem, TaskTreeWidgetItem::TypeDisk);
                diskItem->diskId = diskId;
                diskItem->setText(0, "💾 磁盘");
                diskItem->setText(1, diskId); // Task ID column shows Disk ID for group
                diskItem->setText(2, QString::fromStdString(dTasks.first().disk_info().model())); // Disk Model (新索引2)
                diskItem->setText(3, clientId);                 // Client ID (新索引3)
                diskItem->setText(4, "-");
                diskItem->setText(5, "-");
                diskItem->setText(6, "磁盘组");
                diskItem->setText(7, "-");
                diskItem->setText(8, "-");
                diskItem->setText(9, "-");
                diskItem->setExpanded(true);

                // 3. 添加任务节点 (Level 3)
                for (const ErasureTask& task : dTasks) {
                    TaskTreeWidgetItem* taskItem = new TaskTreeWidgetItem(diskItem, TaskTreeWidgetItem::TypeTask);
                    setupTaskItem(taskItem, task);
                }
            }
        }
    }

    // 恢复排序状态，触发一次重新排序以应用当前的排序指示器（如果有）
    m_treeWidget->setSortingEnabled(wasSortingEnabled);
    if (wasSortingEnabled) {
        // 如果之前有排序列，强制排序一次以确保持久化
        int sortCol = m_treeWidget->header()->sortIndicatorSection();
        Qt::SortOrder sortOrder = m_treeWidget->header()->sortIndicatorOrder();
        m_treeWidget->sortByColumn(sortCol, sortOrder);
    }

    int totalCount = allTasks.size();
    int activeCount = m_taskManager ? m_taskManager->getActiveTasks().size() : 0;
    m_taskCountLabel->setText(QString("任务数量: %1 (进行中: %2)").arg(totalCount).arg(activeCount));

    QTimer::singleShot(0, this, [this]() {
        if (m_treeWidget) {
            for (int i = 0; i < m_treeWidget->columnCount(); ++i) {
                m_treeWidget->resizeColumnToContents(i);
            }
        }
    });
}

TaskTreeWidgetItem* TaskListWidget::findTaskItem(const QString& taskId, QTreeWidgetItem* parent) const
{
    QTreeWidgetItem* root = parent ? parent : m_treeWidget->invisibleRootItem();
    QTreeWidgetItemIterator it(root);
    while (*it) {
        TaskTreeWidgetItem* taskItem = dynamic_cast<TaskTreeWidgetItem*>(*it);
        if (taskItem && taskItem->type() == TaskTreeWidgetItem::TypeTask && taskItem->taskId == taskId) {
            return taskItem;
        }
        ++it;
    }
    return nullptr;
}
void TaskListWidget::updateTaskEntry(const ErasureTask& task)
{
    if (!qApp || qApp->thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, "updateTaskEntry", Qt::QueuedConnection,
                                  Q_ARG(ErasureTask, task));
        return;
    }

    QString taskId = QString::fromStdString(task.task_id());
    TaskTreeWidgetItem* item = findTaskItem(taskId);
    if (!item) {
        refreshTaskList();
        return;
    }

    setupTaskItem(item, task);
}

void TaskListWidget::updateTaskCountLabel()
{
    if (!m_taskManager) return;
    int totalCount = m_taskManager->getAllTasks().size();
    int activeCount = m_taskManager->getActiveTasks().size();
    m_taskCountLabel->setText(QString("任务数量: %1 (进行中: %2)").arg(totalCount).arg(activeCount));
}

void TaskListWidget::addClientNode(const QString& clientId)
{
    TaskTreeWidgetItem* clientNode = new TaskTreeWidgetItem(m_treeWidget, TaskTreeWidgetItem::TypeClient);
    clientNode->clientId = clientId;
    clientNode->setText(0, "💻 客户端");
    clientNode->setText(1, clientId);
    clientNode->setText(2, ""); // Disk Model
    clientNode->setText(3, clientId); // Client ID
    clientNode->setText(4, "");
    clientNode->setText(5, "");
    clientNode->setText(6, "");
    clientNode->setText(7, "");
    clientNode->setText(8, "");
    clientNode->setText(9, "");
    clientNode->setExpanded(true);
    
    QFont font;
    font.setBold(true);
    clientNode->setFont(0, font);
    clientNode->setFont(1, font);
    
    QStringList diskIds = m_taskManager->getDiskIdsByClient(clientId);
    for (const QString& diskId : diskIds) {
        addDiskNode(clientNode, diskId);
    }
}

void TaskListWidget::addDiskNode(QTreeWidgetItem* clientNode, const QString& diskId)
{
    TaskTreeWidgetItem* diskNode = new TaskTreeWidgetItem(clientNode, TaskTreeWidgetItem::TypeDisk);
    diskNode->diskId = diskId;
    diskNode->setText(0, "💾 磁盘");
    diskNode->setText(1, diskId);
    
    QList<ErasureTask> tasks = m_taskManager->getTasksByDisk(clientNode->text(1), diskId);
    if (!tasks.isEmpty()) {
        diskNode->setText(2, QString::fromStdString(tasks.first().disk_info().model())); // Index 2 is Disk Model (Updated)
    } else {
        diskNode->setText(2, "");
    }
    
    diskNode->setText(3, clientNode->text(1)); // Client ID (Updated to Index 3)
    diskNode->setText(4, "");
    diskNode->setText(5, "");
    diskNode->setText(6, "");
    diskNode->setText(7, "");
    diskNode->setText(8, "");
    diskNode->setText(9, "");
    diskNode->setExpanded(true);
    
    for (const ErasureTask& task : tasks) {
        addTaskNode(diskNode, task);
    }
}

void TaskListWidget::addTaskNode(QTreeWidgetItem* diskNode, const ErasureTask& task)
{
    TaskTreeWidgetItem* taskNode = new TaskTreeWidgetItem(diskNode, TaskTreeWidgetItem::TypeTask);
    taskNode->clientId = QString::fromStdString(task.server_client_id());
    setupTaskItem(taskNode, task);
}

void TaskListWidget::setupTaskItem(TaskTreeWidgetItem* item, const ErasureTask& task) {
    item->taskId = QString::fromStdString(task.task_id());
    item->diskId = QString::fromStdString(task.disk_id());
    item->taskData = task;
    
    // 0: 类型
    item->setText(0, "🔧 任务");
    
    // 1: 任务ID
    item->setText(1, item->taskId);
    
    // 2: 磁盘型号 (New Index)
    item->setText(2, QString::fromStdString(task.disk_info().model()));
    
    // 3: 客户端ID (New Index)
    item->setText(3, QString::fromStdString(task.server_client_id()));
    
    // 4: 擦除方法
    item->setText(4, QString::fromStdString(task.erasure_method()));

    // 5: 进度
    QString progressText = QString("%1%").arg(task.progress_percent()/100.0, 0, 'f', 1);
    item->setText(5, progressText);
    item->setData(5, Qt::UserRole, task.progress_percent());

    // 6: 状态
    QString statusText = Utils::getStatusString(task.status());
    QColor statusColor = Utils::getTaskStatusColor(task.status());
    item->setText(6, statusText);
    item->setForeground(6, statusColor);
    item->setData(6, Qt::UserRole, static_cast<int>(task.status()));

    // 7: 开始时间
    QString startTimeStr = "-";
    QDateTime startTime;
    uint64_t startTimestamp = task.start_time();
    startTime = QDateTime::fromTime_t(static_cast<uint>(startTimestamp));
    startTimeStr = startTime.toString("yyyy-MM-dd hh:mm:ss");

    item->setText(7, startTimeStr);
    item->setData(7, Qt::UserRole, startTime);

    // 8: 结束时间
    QString endTimeStr = "-";
    QDateTime endTime;
    uint64_t endTimestamp = task.estimated_end_time();
    endTime = QDateTime::fromTime_t(static_cast<uint>(endTimestamp));
    endTimeStr = endTime.toString("yyyy-MM-dd hh:mm:ss");

    item->setText(8, endTimeStr);
    item->setData(8, Qt::UserRole, endTime);

    // 9: 已用时间
    QString durationText;
    qint64 durationSeconds = 0;
    
    TaskStatus status = static_cast<TaskStatus>(task.status());
    if (status == TaskStatus::COMPLETED ||
            status == TaskStatus::FAILED ||
            status == TaskStatus::STOPPED) {
        durationText = Utils::formatDuration(startTime, endTime);
        if (!startTime.isNull() && !endTime.isNull()) {
            durationSeconds = startTime.secsTo(endTime);
        }

    } else {
        durationText = Utils::formatDuration(startTime);
        if (!startTime.isNull()) {
            durationSeconds = startTime.secsTo(QDateTime::currentDateTime());
        }

    }
    item->setText(9, durationText);
    item->setData(9, Qt::UserRole, durationSeconds);
}

void TaskListWidget::onTreeItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) {
        clearDetailView();
        return;
    }

    TaskTreeWidgetItem* taskItem = dynamic_cast<TaskTreeWidgetItem*>(item);
    if (!taskItem) {
        clearDetailView();
        return;
    }

    if (taskItem->type() == TaskTreeWidgetItem::TypeTask) {
        m_currentSelectedTaskId = taskItem->taskId;
        updateDetailView(m_currentSelectedTaskId);
    } else {
        m_currentSelectedTaskId.clear();
        clearDetailView();
    }
}

void TaskListWidget::updateDetailView(const QString& taskId)
{
    if (!m_taskManager || !m_detailTextBrowser) return;

    ErasureTask task;
    bool found = false;
    for (const ErasureTask& t : m_taskManager->getAllTasks()) {
        if (QString::fromStdString(t.task_id()) == taskId) {
            task = t;
            found = true;
            break;
        }
    }

    if (!found) {
        clearDetailView();
        return;
    }

    QString statusText = Utils::getStatusString(task.status());
    QColor statusColor = Utils::getTaskStatusColor(task.status());

    QString statusClass = "status-default";
    TaskStatus status = static_cast<TaskStatus>(task.status());
    if (status == TaskStatus::PENDING) statusClass = "status-pending";
    else if (status == TaskStatus::STARTED ||
             status == TaskStatus::IN_PROGRESS) statusClass = "status-inprogress";
    else if (status == TaskStatus::COMPLETED) statusClass = "status-completed";
    else if (status == TaskStatus::FAILED) statusClass = "status-failed";
    else if (status == TaskStatus::STOPPED) statusClass = "status-stopped";

    QString startTimeStr = "N/A";
    QString endTimeStr = "N/A";
    QDateTime startTime, endTime;
    
    uint64_t startTimestamp = task.start_time();

    startTime = QDateTime::fromTime_t(static_cast<uint>(startTimestamp));
    startTimeStr = startTime.toString("yyyy-MM-dd hh:mm:ss");


    uint64_t endTimestamp = task.estimated_end_time();
    endTime = QDateTime::fromTime_t(static_cast<uint>(endTimestamp));
    endTimeStr = endTime.toString("yyyy-MM-dd hh:mm:ss");


    QString durationText;
    if (status == TaskStatus::COMPLETED ||
            status == TaskStatus::FAILED ||
            status == TaskStatus::STOPPED) {
        durationText = Utils::formatDuration(startTime, endTime);

    } else {
        durationText = Utils::formatDuration(startTime);

    }

    QString html = QString(
                "<html><body>"
                "<table>"
                "<tr><th>客户端ID</th><td>%1</td></tr>"
                "<tr><th>任务ID</th><td>%2</td></tr>"
                "<tr><th>磁盘ID</th><td>%3</td></tr>"
                "<tr><th>磁盘型号</th><td>%4</td></tr>"
                "<tr><th>擦除方法</th><td>%5</td></tr>"
                "<tr><th>状态</th><td><span class='%6' style='color:%7'>%8</span></td></tr>"
                "<tr><th>进度</th><td>%9%</td></tr>"
                "<tr><th>开始时间</th><td>%10</td></tr>"
                "<tr><th>结束时间</th><td>%11</td></tr>"
                "<tr><th>持续时间</th><td>%12</td></tr>"
                "</table>"
                "</body></html>"
                ).arg(QString::fromStdString(task.server_client_id()))
            .arg(QString::fromStdString(task.task_id()))
            .arg(QString::fromStdString(task.disk_id()))
            .arg(QString::fromStdString(task.disk_info().model()))
            .arg(QString::fromStdString(task.erasure_method()))
            .arg(statusClass)
            .arg(statusColor.name())
            .arg(statusText)
            .arg(QString::number(task.progress_percent(), 'f', 1))
            .arg(startTimeStr)
            .arg(endTimeStr)
            .arg(durationText);

    m_detailTextBrowser->setHtml(html);
    m_currentSelectedTaskId = taskId;
}

void TaskListWidget::clearDetailView()
{
    if (m_detailTextBrowser) {
        m_detailTextBrowser->clear();
        m_detailTextBrowser->setHtml("<html><body><p style='color:gray; text-align:center; margin-top:20px;'>请选择一个任务查看详情</p></body></html>");
    }
    m_currentSelectedTaskId.clear();
}

void TaskListWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_treeWidget) {
        QTimer::singleShot(0, this, [this]() {
            if (m_treeWidget) {
                for (int i = 0; i < m_treeWidget->columnCount(); ++i) {
                    m_treeWidget->resizeColumnToContents(i);
                }
            }
        });
    }
}
