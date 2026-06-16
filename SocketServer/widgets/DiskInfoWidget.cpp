#include "DiskInfoWidget.h"
#include "Utils.h"
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#define k_use_process_UI 1
// 刷新磁盘表格
DiskInfoWidget::DiskInfoWidget(QWidget *parent)
    : QWidget(parent)
    , m_viewMode(TreeMode)
{
    // 初始化列可见性
    for (int i = 0; i < ColumnCount; ++i) {
        m_columnVisibility[i] = true;
    }

    // 列名称 -- 去掉磁盘ID列
    m_columnNames << "选择" << "型号" << "序列号" << "容量"
                  << "已用空间" << "文件系统" << "健康状态" << "挂载点"
                  << "擦除方法" << "进度" << "状态" << "开始时间"
                  << "预计完成" << "速度";

    setupUI();
    createActions();
}

DiskInfoWidget::~DiskInfoWidget()
{
}

void DiskInfoWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 创建水平分割器（左右布局）
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_mainGroup = new QGroupBox("磁盘列表与擦除进度", this);
    QVBoxLayout* groupLayout = new QVBoxLayout(m_mainGroup);

    // 顶部工具栏
    m_topLayout = new QHBoxLayout();

    // 过滤器
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("搜索磁盘...");
    m_filterEdit->setMaximumWidth(200);
    connect(m_filterEdit, SIGNAL(textChanged(QString)), this, SLOT(onFilterTextChanged(QString)));
    m_topLayout->addWidget(new QLabel("过滤:"));
    m_topLayout->addWidget(m_filterEdit);

    m_topLayout->addStretch();

    // 视图模式切换按钮
    m_viewModeToggleBtn = new QPushButton(this);
    m_viewModeToggleBtn->setIcon(QIcon(":/res/list.png")); // 默认显示列表图标（平铺模式）
    m_viewModeToggleBtn->setToolTip("切换为分组视图");
    m_viewModeToggleBtn->setFixedSize(20, 20); // 设置固定大小以适应图标
    m_viewModeToggleBtn->setIconSize(QSize(12, 12));
    m_viewModeToggleBtn->setFlat(true);

    connect(m_viewModeToggleBtn, SIGNAL(clicked(bool)), this, SLOT(onViewModeChanged()));
    m_topLayout->addWidget(m_viewModeToggleBtn);

    // 列配置按钮
    m_columnConfigBtn = new QPushButton(this);
    m_columnConfigBtn->setIcon(QIcon(":/res/filter.png"));
    m_columnConfigBtn->setToolTip("配置显示列");
    m_columnConfigBtn->setFixedSize(20, 20);
    m_columnConfigBtn->setIconSize(QSize(12, 12));
    m_columnConfigBtn->setFlat(true);

    connect(m_columnConfigBtn, SIGNAL(clicked()), this, SLOT(onColumnConfigClicked()));
    m_topLayout->addWidget(m_columnConfigBtn);

    m_refreshBtn = new QPushButton(this);
    m_refreshBtn->setIcon(QIcon(":/res/refresh.png"));
    m_refreshBtn->setToolTip("刷新列表");
    m_refreshBtn->setFixedSize(20, 20); // 设置固定大小以适应图标
    m_refreshBtn->setIconSize(QSize(12, 12));
    m_refreshBtn->setFlat(true);
    m_topLayout->addWidget(m_refreshBtn);
    connect(m_refreshBtn, &QPushButton::clicked, this, &DiskInfoWidget::refreshRequested);



    groupLayout->addLayout(m_topLayout);

    // 树形视图（统一使用，支持平铺和分组两种模式）
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setStyleSheet("QTreeWidget::item { height: 30px; }");
    m_treeWidget->setHeaderLabels(m_columnNames);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setSortingEnabled(true);
    m_treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeWidget->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &DiskInfoWidget::onHeaderCustomContextMenuRequested);
    connect(m_treeWidget, &QTreeWidget::itemChanged,
            this, &DiskInfoWidget::onDiskItemChanged);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &DiskInfoWidget::onDiskItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged,
            this, &DiskInfoWidget::onDiskSelectionChanged);

    // 设置列宽（去掉磁盘ID后，所有后面列向前移动，原本1,2,3...现在变成1,2,3...）
    m_treeWidget->setColumnWidth(0, 70);   // 选择
    m_treeWidget->setColumnWidth(1, 150);  // 型号
    m_treeWidget->setColumnWidth(2, 150);  // 序列号
    m_treeWidget->setColumnWidth(3, 100);  // 容量
    m_treeWidget->setColumnWidth(4, 80);   // 已用空间
    m_treeWidget->setColumnWidth(5, 80);   // 文件系统
    m_treeWidget->setColumnWidth(6, 60);   // 健康状态
    m_treeWidget->setColumnWidth(7, 100);  // 挂载点
    m_treeWidget->setColumnWidth(8, 80);   // 擦除方法
    m_treeWidget->setColumnWidth(9, 100);  // 进度
    m_treeWidget->setColumnWidth(10, 80);  // 状态
    m_treeWidget->setColumnWidth(11, 140); // 开始时间
    m_treeWidget->setColumnWidth(12, 140); // 预计完成
    m_treeWidget->setColumnWidth(13, 100); // 速度

    groupLayout->addWidget(m_treeWidget);
#if !defined(ZMQ_SERVER)
    // 擦除控制组件
    m_erasureControlWidget = new ErasureControlWidget(this);
    groupLayout->addWidget(m_erasureControlWidget);
    
    // 连接擦除控制组件的信号，转发到 DiskInfoWidget 的信号
    connect(m_erasureControlWidget, &ErasureControlWidget::startErasureRequested,
            this, &DiskInfoWidget::startErasureRequested);
    connect(m_erasureControlWidget, &ErasureControlWidget::stopErasureRequested,
            this, &DiskInfoWidget::stopErasureRequested);
    connect(m_erasureControlWidget, &ErasureControlWidget::pauseErasureRequested,
            this, &DiskInfoWidget::pauseErasureRequested);
    connect(m_erasureControlWidget, &ErasureControlWidget::resumeErasureRequested,
            this, &DiskInfoWidget::resumeErasureRequested);
    connect(m_erasureControlWidget, &ErasureControlWidget::erasureMethodChanged,
            this, &DiskInfoWidget::erasureMethodChanged);
#endif
    // 将磁盘列表区域添加到分割器左侧
    m_splitter->addWidget(m_mainGroup);

    // === 右侧：详细信息区域 ===
    m_detailGroup = new QGroupBox("磁盘详细信息", this);
    // 关键修复：确保 GroupBox 的布局能够正确扩展其子控件
    QVBoxLayout* detailLayout = new QVBoxLayout(m_detailGroup);
    // 移除多余的边距，让 QTextEdit 填满整个区域
    detailLayout->setContentsMargins(2, 2, 2, 2);

    m_detailTextEdit = new QTextEdit(this);
    m_detailTextEdit->setReadOnly(true);
    m_detailTextEdit->setHtml("<p style='color: gray;'>请选择一个磁盘以查看详细信息</p>");
    // 关键修复：设置 QTextEdit 的大小策略为 Expanding，确保它能随父容器拉伸
    m_detailTextEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 将 QTextEdit 添加到布局中，stretch 因子设为 1 确保它占据所有可用空间
    detailLayout->addWidget(m_detailTextEdit, 1);

    // 将详细信息区域添加到分割器右侧
    m_splitter->addWidget(m_detailGroup);

    // 设置分割器初始大小比例（左:右 = 2:1）
    m_splitter->setSizes({600, 300});
    m_splitter->setStretchFactor(0, 2);  // 左侧可拉伸
    m_splitter->setStretchFactor(1, 1);  // 右侧可拉伸

    // 将分割器添加到主布局
    mainLayout->addWidget(m_splitter);
}

void DiskInfoWidget::createActions()
{
    m_showAllColumnsAction = new QAction("显示所有列", this);
    connect(m_showAllColumnsAction, SIGNAL(triggered()), this, SLOT(onShowAllColumns()));

    m_hideAllColumnsAction = new QAction("隐藏所有列", this);
    connect(m_hideAllColumnsAction, SIGNAL(triggered()), this, SLOT(onHideAllColumns()));
}

void DiskInfoWidget::onShowAllColumns()
{
    for (int i = 0; i < ColumnCount; ++i) {
        setColumnVisible(i, true);
    }
    updateDisplay();
}

void DiskInfoWidget::onHideAllColumns()
{
    for (int i = 0; i < ColumnCount; ++i) {
        if (i != 0) { // 只保留选择列
            setColumnVisible(i, false);
        }
    }
    updateDisplay();
}

void DiskInfoWidget::onColumnConfigClicked()
{
    showColumnConfigMenu(QCursor::pos());
}

void DiskInfoWidget::setClientList(const QList<ClientInfoWrap>& clientList)
{
    // 多客户端模式：设置客户端列表
    m_clientList = clientList;

    // 同步到 m_diskList（用于向后兼容）
    m_diskList.clear();
    for (int i = 0; i < m_clientList.size(); ++i) {
        const ClientInfoWrap& clientWrap = m_clientList.at(i);
        m_diskList.append(clientWrap.diskList);
    }

    updateDisplay();
}

QList<ClientDiskInfo> DiskInfoWidget::getSelectedDisks() const
{
    QList<ClientDiskInfo> selected;
    foreach (const ClientDiskInfo& disk, m_diskList) {
        if (disk.isSelected) {
            selected.append(disk);
        }
    }
    return selected;
}

QStringList DiskInfoWidget::getSelectedDiskIds() const
{
    QStringList ids;
    foreach (const ClientDiskInfo& disk, m_diskList) {
        if (disk.isSelected) {
            ids.append(QString::fromStdString(disk.info.disk_id()));
        }
    }
    return ids;
}

void DiskInfoWidget::restorePreviousSelection()
{
    // 恢复之前的选择状态（基于 m_diskList 中的 isSelected 标志）
    updateDisplay();
}


void DiskInfoWidget::updateDiskProgress(const ErasureTask& task)// will depresh
{
    QString diskId = QString::fromStdString(task.disk_id());

    // 查找对应的磁盘信息
    for (int i = 0; i < m_diskList.size(); ++i) {
        if (QString::fromStdString(m_diskList[i].info.disk_id()) == diskId) {
            ClientDiskInfo& disk = m_diskList[i];

            // 更新数据模型
            disk.erasureMethod = QString::fromStdString(task.erasure_method());
            disk.progressPercent = task.progress_percent();
            disk.status = Utils::getStatusString(task.status());
            disk.startTime = task.start_time();
            disk.estimatedEndTime = task.estimated_end_time();
            disk.speedMbPerSec = task.speed_mb_per_s();

            // 直接更新 UI（避免重建整个列表）
            if (disk.treeItem) {
                updateTreeItemProgress(disk.treeItem, disk);
            }

            break;
        }
    }
}
void DiskInfoWidget::updateDiskProgress(const ErasureProgress& progress)
{
    //TODO
//    const QString& clientId,
}
void DiskInfoWidget::setViewMode(ViewMode mode)
{
    // 如果切换视图模式，清除指针缓存
    if (m_viewMode != mode) {
        for (int i = 0; i < m_diskList.size(); ++i) {
            m_diskList[i].treeItem = nullptr;
        }
    }

    m_viewMode = mode;

    // 当前两种模式都使用 TreeWidget，只是显示方式不同
    // TreeMode: 平铺显示（无分组）
    // GroupedMode: 分组显示（未来扩展）
    m_treeWidget->show();

    updateDisplay();
}

void DiskInfoWidget::setColumnVisible(int column, bool visible)
{
    if (column >= 0 && column < ColumnCount) {
        m_columnVisibility[column] = visible;

        m_treeWidget->setColumnHidden(column, !visible);
    }
}


bool DiskInfoWidget::isColumnVisible(int column) const
{
    if (column >= 0 && column < ColumnCount) {
        return m_columnVisibility[column];
    }
    return false;
}

void DiskInfoWidget::showColumnConfigMenu(const QPoint& pos)
{
    QMenu menu(this);

    // 添加列选择项
    for (int i = 0; i < ColumnCount; ++i) {
        QAction* action = new QAction(m_columnNames[i], &menu);
        action->setCheckable(true);
        action->setChecked(m_columnVisibility[i]);

        connect(action, &QAction::toggled, this, [this, i](bool checked) {
            setColumnVisible(i, checked);
        });

        menu.addAction(action);
    }

    menu.addSeparator();
    menu.addAction(m_showAllColumnsAction);
    menu.addAction(m_hideAllColumnsAction);

    menu.exec(pos);
}
void DiskInfoWidget::updateDisplay()
{
    m_treeWidget->blockSignals(true);
    m_treeWidget->clear();

    QString filterText = m_filterEdit->text().toLower();

    // 重置所有 treeItem 指针
    for (int i = 0; i < m_diskList.size(); ++i) {
        m_diskList[i].treeItem = nullptr;
    }

    if (m_viewMode == ViewMode::FlatMode) {
        // 平铺模式：直接显示所有磁盘
        foreach (const ClientDiskInfo& disk, m_diskList) {
            // 应用过滤器
            if (!filterText.isEmpty()) {
                QString diskId = QString::fromStdString(disk.info.disk_id()).toLower();
                QString model = QString::fromStdString(disk.info.model()).toLower();
                QString serial = QString::fromStdString(disk.info.serial_number()).toLower();

                if (!diskId.contains(filterText) &&
                        !model.contains(filterText) &&
                        !serial.contains(filterText)) {
                    continue;
                }
            }

            QTreeWidgetItem* item = new QTreeWidgetItem(m_treeWidget);
            fillTreeItem(item, disk);

            // 保存 item 指针到数据模型中
            for (int i = 0; i < m_diskList.size(); ++i) {
                if (QString::fromStdString(m_diskList[i].info.disk_id()) ==
                        QString::fromStdString(disk.info.disk_id())) {
                    m_diskList[i].treeItem = item;
                    break;
                }
            }
        }
    } else {
        // 分组模式：按客户端分组显示磁盘
        // 如果有多个客户端，使用 m_clientList；否则从 m_diskList 创建一个默认客户端
        QList<ClientInfoWrap> clientsToDisplay;

        if (!m_clientList.isEmpty()) {
            clientsToDisplay = m_clientList;
        } else {
            // 向后兼容：从 m_diskList 创建默认客户端
            ClientInfoWrap defaultClient;
            defaultClient.clientInfo.set_client_id("local_client");
            defaultClient.clientInfo.set_hostname("本地客户端");
            defaultClient.diskList = m_diskList;
            clientsToDisplay.append(defaultClient);
        }

        // 按客户端分组创建树节点
        foreach (const ClientInfoWrap& clientWrap, clientsToDisplay) {
            const QString clientId = QString::fromStdString(clientWrap.clientInfo.client_id());
            const QString hostname = QString::fromStdString(clientWrap.clientInfo.hostname());
            const QList<ClientDiskInfo>& disks = clientWrap.diskList;

            if (disks.isEmpty()) {
                continue;
            }

            // 创建客户端分组节点
            QTreeWidgetItem* clientItem = new QTreeWidgetItem(m_treeWidget);
            clientItem->setText(1, QString("💻 %1 (%2)").arg(hostname).arg(disks.size()));
            clientItem->setText(2, "客户端");
            clientItem->setFlags(clientItem->flags() & ~Qt::ItemIsSelectable); // 不可选中

            // 设置分组节点的样式
            QFont font = clientItem->font(1);
            font.setBold(true);
            clientItem->setFont(1, font);
            clientItem->setBackground(1, QColor("#E8F4F8"));

            // 展开分组
            clientItem->setExpanded(true);

            // 添加该客户端下的所有磁盘
            foreach (const ClientDiskInfo& disk, disks) {
                // 应用过滤器
                if (!filterText.isEmpty()) {
                    QString diskId = QString::fromStdString(disk.info.disk_id()).toLower();
                    QString model = QString::fromStdString(disk.info.model()).toLower();
                    QString serial = QString::fromStdString(disk.info.serial_number()).toLower();

                    if (!diskId.contains(filterText) &&
                            !model.contains(filterText) &&
                            !serial.contains(filterText)) {
                        continue;
                    }
                }

                QTreeWidgetItem* item = new QTreeWidgetItem(clientItem);
                fillTreeItem(item, disk);

                // 保存 item 指针到数据模型中
                for (int i = 0; i < m_diskList.size(); ++i) {
                    if (QString::fromStdString(m_diskList[i].info.disk_id()) ==
                            QString::fromStdString(disk.info.disk_id())) {
                        m_diskList[i].treeItem = item;
                        break;
                    }
                }
            }
        }
    }
    m_treeWidget->blockSignals(false);
}


void DiskInfoWidget::fillTreeItem(QTreeWidgetItem* item, const ClientDiskInfo& disk)
{
    // 0: 选择 (Checkbox)
    item->setCheckState(0, disk.isSelected ? Qt::Checked : Qt::Unchecked);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    // 1: 型号
    item->setText(1, QString::fromStdString(disk.info.model()));

    // 2: 序列号
    item->setText(2, QString::fromStdString(disk.info.serial_number()));

    // 3: 容量
    item->setText(3, Utils::formatBytes(disk.info.total_size()));

    // 4: 已用空间
    item->setText(4, Utils::formatBytes(disk.info.used_size()));

    // 5: 文件系统
    item->setText(5, QString::fromStdString(disk.info.file_system()));

    // 6: 健康状态
    QString healthText = QString("%1%").arg(disk.info.health_status(), 0, 'f', 1);
    item->setText(6, healthText);
    QColor healthColor = Utils::getHealthStatusColor(disk.info.health_status());
    item->setForeground(6, healthColor);

    // 7: 挂载点
    item->setText(7, QString::fromStdString(disk.info.mount_point()));

    // 8: 擦除方法
    item->setText(8, disk.erasureMethod.isEmpty() ? "-" : disk.erasureMethod);

    // 9: 进度 - 使用进度条控件
#if k_use_process_UI
    QWidget* progressContainer = new QWidget(m_treeWidget);
    QHBoxLayout* layout = new QHBoxLayout(progressContainer);
    layout->setContentsMargins(2, 2, 2, 2);

    QProgressBar* progressBar = new QProgressBar(progressContainer);
    progressBar->setRange(0, 10000);
    progressBar->setValue(disk.progressPercent);
    progressBar->setFormat("%p%");
    progressBar->setFixedHeight(14);
    layout->addWidget(progressBar);
    progressContainer->setLayout(layout);

    m_treeWidget->setItemWidget(item, 9, progressContainer);
#else
    item->setText(9, QString("%1%").arg(disk.progressPercent / 100.0, 0, 'f', 1));
#endif

    // 10: 状态
    QString statusText = getStatusText(disk);
    item->setText(10, statusText);
    item->setForeground(10, getStatusColor(statusText));

    // 11: 开始时间
    if (disk.startTime > 0) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(disk.startTime);
        item->setText(11, dt.toString("hh:mm:ss"));
    } else {
        item->setText(11, "-");
    }

    // 12: 预计完成时间
    if (disk.estimatedEndTime > 0) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(disk.estimatedEndTime);
        item->setText(12, dt.toString("hh:mm:ss"));
    } else {
        item->setText(12, "-");
    }

    // 13: 速度
    if (disk.speedMbPerSec > 0) {
        item->setText(13, QString("%1 MB/s").arg(disk.speedMbPerSec, 0, 'f', 2));
    } else {
        item->setText(13, "0 MB/s");
    }

    // 存储磁盘ID到 UserRole
    item->setData(0, Qt::UserRole, QString::fromStdString(disk.info.disk_id()));
}

QString DiskInfoWidget::getStatusText(const ClientDiskInfo& disk) const
{
    if (!disk.status.isEmpty()) {
        return disk.status;
    }
    return "空闲";
}

QColor DiskInfoWidget::getStatusColor(const QString& status) const
{
    if (status == "进行中" || status == "IN_PROGRESS") {
        return QColor(0, 128, 255); // 蓝色
    } else if (status == "已完成" || status == "COMPLETED") {
        return QColor(0, 192, 0); // 绿色
    } else if (status == "失败" || status == "FAILED") {
        return QColor(255, 0, 0); // 红色
    } else if (status == "暂停" || status == "PAUSED") {
        return QColor(255, 165, 0); // 橙色
    } else if (status == "停止" || status == "STOPPED") {
        return QColor(128, 128, 128); // 灰色
    }
    return QColor(0, 0, 0); // 黑色（空闲）
}

// 生成磁盘详细信息的 HTML 内容
QString DiskInfoWidget::generateDiskDetailHtml(const ClientDiskInfo& disk) const
{
    QString html;
    html += "<html><body style='font-family: Microsoft YaHei, Arial, sans-serif; font-size: 13px; color: #333; line-height: 1.6;'>";

    // 标题
    html += "<div style='margin-bottom: 20px; border-bottom: 2px solid #3498db; padding-bottom: 10px;'>";
    html += QString("<h2 style='margin: 0; color: #2c3e50; font-size: 18px;'>📀 %1</h2>")
                .arg(QString::fromStdString(disk.info.model()).toHtmlEscaped());
    html += QString("<div style='color: #7f8c8d; font-size: 12px; margin-top: 5px;'>ID: %1</div>")
                .arg(QString::fromStdString(disk.info.disk_id()).toHtmlEscaped());
    html += "</div>";

    // addSection 用普通函数代替 lambda，兼容旧编译器问题
    struct HtmlTools {
        static void addSection(QString& html, const QString& title, const QString& content) {
            html += QString("<div style='margin-bottom: 15px; border: 1px solid #e0e0e0; border-radius: 4px; overflow: hidden;'>");
            html += QString("<div style='background-color: #f8f9fa; padding: 8px 12px; font-weight: bold; color: #2c3e50; border-bottom: 1px solid #e0e0e0;'>%1</div>").arg(title.toHtmlEscaped());
            html += "<div style='padding: 10px 12px;'>";
            html += content;
            html += "</div></div>";
        }
        static void addItem(QString& html, const QString& label, const QString& value, bool isHtmlValue = false) {
            html += QString("<div style='display: flex; margin-bottom: 6px;'>");
            html += QString("<span style='width: 100px; color: #7f8c8d; flex-shrink: 0;'>%1:</span>").arg(label.toHtmlEscaped());
            if (isHtmlValue) {
                html += QString("<span style='flex: 1; font-weight: 500;'>%1</span>").arg(value);
            } else {
                html += QString("<span style='flex: 1; font-weight: 500;'>%1</span>").arg(value.toHtmlEscaped());
            }
            html += "</div>";
        }
    };

    // 1. 基本信息
    QString baseContent;
    HtmlTools::addItem(baseContent, "型号", QString::fromStdString(disk.info.model()));
    HtmlTools::addItem(baseContent, "序列号", QString::fromStdString(disk.info.serial_number()));
    HtmlTools::addItem(baseContent, "接口类型", QString::fromStdString(disk.info.interface_type()));
    HtmlTools::addItem(baseContent, "挂载点", QString::fromStdString(disk.info.mount_point()));
    HtmlTools::addItem(baseContent, "文件系统", QString::fromStdString(disk.info.file_system()));
    HtmlTools::addItem(baseContent, "系统盘", disk.info.is_system_disk() ? "是" : "否");
    HtmlTools::addItem(baseContent, "可移动", disk.info.is_removable() ? "是" : "否");
    HtmlTools::addSection(html, "📋 基本属性", baseContent);

    // 2. 容量信息
    QString storageContent;
    HtmlTools::addItem(storageContent, "总容量", Utils::formatBytes(disk.info.total_size()));
    HtmlTools::addItem(storageContent, "已用空间", Utils::formatBytes(disk.info.used_size()));
    HtmlTools::addItem(storageContent, "可用空间", Utils::formatBytes(disk.info.free_size()));

    double usagePercent = 0;
    if (disk.info.total_size() > 0) {
        usagePercent = (double)disk.info.used_size() / disk.info.total_size() * 100;
    }

    // 兼容 C++11，直接用三元表达式代替
    QString barColor;
    if (usagePercent > 90) {
        barColor = "#e74c3c";
    } else if (usagePercent > 70) {
        barColor = "#f39c12";
    } else {
        barColor = "#2ecc71";
    }
    storageContent += QString("<div style='margin-top: 8px;'>");
    storageContent += QString("<div style='display: flex; justify-content: space-between; font-size: 12px; margin-bottom: 2px;'>");
    storageContent += "<span>使用率</span>";
    storageContent += QString("<span>%1%</span>").arg(usagePercent, 0, 'f', 1);
    storageContent += "</div>";
    storageContent += QString("<div style='background-color: #ecf0f1; height: 8px; border-radius: 4px; overflow: hidden;'>");
    storageContent += QString("<div style='width: %1%; background-color: %2; height: 100%;'></div>")
                        .arg(usagePercent, 0, 'f', 1).arg(barColor);
    storageContent += "</div></div>";
    HtmlTools::addSection(html, "💾 存储容量", storageContent);

    // 3. 健康状态
    QString healthContent;
    QString healthStatus;
    QColor healthColor = Utils::getHealthStatusColor(disk.info.health_status());
    if (disk.info.health_status() >= 90) {
        healthStatus = "优秀";
    } else if (disk.info.health_status() >= 70) {
        healthStatus = "良好";
    } else if (disk.info.health_status() >= 50) {
        healthStatus = "一般";
    } else {
        healthStatus = "较差";
    }
    QString healthHtml = QString("<span style='color: %1; font-weight: bold; font-size: 14px;'>%2% (%3)</span>")
               .arg(healthColor.name())
               .arg(disk.info.health_status(), 0, 'f', 1)
               .arg(healthStatus);
    HtmlTools::addItem(healthContent, "健康度", healthHtml, true);
    HtmlTools::addSection(html, "❤️ 健康状态", healthContent);

    // 4. 擦除任务信息（如果有）
    if (!disk.status.isEmpty() && disk.status != "空闲") {
        QString taskContent;
        HtmlTools::addItem(taskContent, "擦除方法", disk.erasureMethod.isEmpty() ? "-" : disk.erasureMethod);

        double progressPercent = disk.progressPercent / 100.0;
        QString progressHtml = QString("<span style='color: #3498db; font-weight: bold;'>%1%</span>").arg(progressPercent, 0, 'f', 1);
        HtmlTools::addItem(taskContent, "进度", progressHtml, true);

        QColor statusColor = getStatusColor(disk.status);
        QString statusHtml = QString("<span style='color: %1; font-weight: bold;'>%2</span>")
                .arg(statusColor.name())
                .arg(disk.status.toHtmlEscaped());
        HtmlTools::addItem(taskContent, "状态", statusHtml, true);

        if (disk.startTime > 0) {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(disk.startTime);
            HtmlTools::addItem(taskContent, "开始时间", dt.toString("yyyy-MM-dd hh:mm:ss"));
        }

        if (disk.estimatedEndTime > 0) {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(disk.estimatedEndTime);
            HtmlTools::addItem(taskContent, "预计完成", dt.toString("yyyy-MM-dd hh:mm:ss"));
        }

        if (disk.speedMbPerSec > 0) {
            HtmlTools::addItem(taskContent, "当前速度", QString("%1 MB/s").arg(disk.speedMbPerSec, 0, 'f', 2));
        }

        // 计算剩余时间
        if (progressPercent > 0 && progressPercent < 100 && disk.speedMbPerSec > 0) {
            qint64 remainingBytes = disk.info.total_size() - disk.info.used_size();
            if (remainingBytes < 0) remainingBytes = 0;

            double remainingSeconds = (remainingBytes / 1024.0 / 1024.0) / disk.speedMbPerSec;
            int hours = static_cast<int>(remainingSeconds) / 3600;
            int minutes = (static_cast<int>(remainingSeconds) % 3600) / 60;
            int seconds = static_cast<int>(remainingSeconds) % 60;
            HtmlTools::addItem(taskContent, "预计剩余", QString("%1小时 %2分 %3秒").arg(hours).arg(minutes).arg(seconds));
        }

        HtmlTools::addSection(html, "🔧 擦除任务", taskContent);
    }

    // 5. SMART 信息
    std::string si = disk.info.smart_info();
    if (!si.empty()) {
        QString smartContent;
        QString smartText = QString::fromStdString(si);
        QString smartHtml = smartText.toHtmlEscaped().replace("\n", "<br>");
        smartContent += QString("<div style='background-color: #f8f9fa; padding: 8px; border-radius: 4px; font-family: Consolas, Monaco, monospace; font-size: 12px; white-space: pre-wrap; word-break: break-all;'>%1</div>").arg(smartHtml);
        HtmlTools::addSection(html, "💡 SMART 信息", smartContent);
    }

    html += "</body></html>";

    return html;
}

// 显示磁盘详细信息
void DiskInfoWidget::showDiskDetail(const QString& diskId)
{
    for (const auto& disk : m_diskList) {
        if (QString::fromStdString(disk.info.disk_id()) == diskId) {
            QString html = generateDiskDetailHtml(disk);
            m_detailTextEdit->setHtml(html);
            return;
        }
    }

    // 未找到磁盘，显示提示
    m_detailTextEdit->setHtml("<p style='color: gray;'>未找到该磁盘的信息</p>");
}

void DiskInfoWidget::onDiskSelectionChanged()
{
    // 获取当前选中的磁盘
    QString selectedDiskId;

    QList<QTreeWidgetItem*> selectedItems = m_treeWidget->selectedItems();
    if (!selectedItems.isEmpty()) {
        selectedDiskId = selectedItems.first()->data(0, Qt::UserRole).toString();
    }

    if (!selectedDiskId.isEmpty()) {
        showDiskDetail(selectedDiskId);
    } else {
        m_detailTextEdit->setHtml("<p style='color: gray;'>请选择一个磁盘以查看详细信息</p>");
    }
}


// Slots implementation

void DiskInfoWidget::onFilterTextChanged(const QString& text)
{
    Q_UNUSED(text);
    updateDisplay();
}

void DiskInfoWidget::onViewModeChanged()
{
    if (m_viewMode == TreeMode) {
        setViewMode(FlatMode);
        m_viewModeToggleBtn->setIcon(QIcon(":/res/tree.png"));
        m_viewModeToggleBtn->setToolTip("切换为平铺视图");
    } else {
        setViewMode(TreeMode);
        m_viewModeToggleBtn->setIcon(QIcon(":/res/list.png"));
        m_viewModeToggleBtn->setToolTip("切换为分组视图");
    }
}
void DiskInfoWidget::onDiskItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == 0) { // 选择列
        QString diskId = item->data(0, Qt::UserRole).toString();

        // 更新数据模型
        for (int i = 0; i < m_diskList.size(); ++i) {
            if (QString::fromStdString(m_diskList[i].info.disk_id()) == diskId) {
                m_diskList[i].isSelected = (item->checkState(0) == Qt::Checked);
                break;
            }
        }

        emit diskSelectionChanged();
    }
}

void DiskInfoWidget::onDiskItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    QString diskId = item->data(0, Qt::UserRole).toString();
    emit diskDoubleClicked(diskId);
}

void DiskInfoWidget::onHeaderCustomContextMenuRequested(const QPoint& pos)
{
    showColumnConfigMenu(QCursor::pos());
}

// 快速更新树形视图中的进度信息（不重建整个列表）
void DiskInfoWidget::updateTreeItemProgress(QTreeWidgetItem* item, const ClientDiskInfo& disk)
{
    if (!item) return;

    // 8: 擦除方法
    item->setText(8, disk.erasureMethod.isEmpty() ? "-" : disk.erasureMethod);

    // 9: 进度 - 更新进度条
#if k_use_process_UI
    QWidget* containerWidget = m_treeWidget->itemWidget(item, 9);
    QProgressBar* progressBar = nullptr;
    if (containerWidget) {
        progressBar = containerWidget->findChild<QProgressBar*>();
    }
    if (progressBar) {
        progressBar->setValue(disk.progressPercent);
        double progressPercent = disk.progressPercent / 100.0;
        progressBar->setFormat(QString("%1%").arg(progressPercent, 0, 'f', 1));
    } else {
        item->setText(9, QString("%1%").arg(disk.progressPercent / 100.0, 0, 'f', 1));
    }
#else
    item->setText(9, QString("%1%").arg(disk.progressPercent / 100.0, 0, 'f', 1));
#endif

    // 10: 状态
    QString statusText = getStatusText(disk);
    item->setText(10, statusText);
    item->setForeground(10, getStatusColor(statusText));

    // 11: 开始时间
    if (disk.startTime > 0) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(disk.startTime);
        item->setText(11, dt.toString("hh:mm:ss"));
    } else {
        item->setText(11, "-");
    }

    // 12: 预计完成时间
    if (disk.estimatedEndTime > 0) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(disk.estimatedEndTime);
        item->setText(12, dt.toString("hh:mm:ss"));
    } else {
        item->setText(12, "-");
    }

    // 13: 速度
    if (disk.speedMbPerSec > 0) {
        item->setText(13, QString("%1 MB/s").arg(disk.speedMbPerSec, 0, 'f', 2));
    } else {
        item->setText(13, "0 MB/s");
    }
}

#if !defined(ZMQ_SERVER)
void DiskInfoWidget::setErasureControlsEnabled(bool enabled)
{
    if (m_erasureControlWidget) {
        m_erasureControlWidget->setControlsEnabled(enabled);
    }
}
#endif
