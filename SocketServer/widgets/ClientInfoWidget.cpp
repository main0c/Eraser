#include "ClientInfoWidget.h"
#include <QDateTime>
#include <QHeaderView>
#include <QDebug>

ClientInfoWidget::ClientInfoWidget(QWidget *parent)
    : QWidget(parent)
    , m_splitter(nullptr)
    , m_clientListGroup(nullptr)
    , m_clientTable(nullptr)
    , m_detailGroup(nullptr)
    , m_detailTextEdit(nullptr)
    , m_refreshBtn(nullptr)
    , m_clientManager(nullptr)
{
    setupUI();
}

void ClientInfoWidget::setClientManager(ClientManager* manager)
{
    m_clientManager = manager;
    refreshClientList();
}

void ClientInfoWidget::setupUI()
{
    m_splitter = new QSplitter(Qt::Vertical, this);

    m_clientListGroup = new QGroupBox("客户端列表", this);
    QVBoxLayout* clientListLayout = new QVBoxLayout(m_clientListGroup);

    m_clientTable = new QTableWidget(0, 5, this);
    m_clientTable->setHorizontalHeaderLabels({"客户端ID", "主机名", "IP地址", "连接时间", "状态"});
    m_clientTable->horizontalHeader()->setStretchLastSection(true);

    m_clientTable->horizontalHeader()->setSectionsMovable(true);
    m_clientTable->horizontalHeader()->setSectionsClickable(true);
    m_clientTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    m_clientTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_clientTable->setAlternatingRowColors(true);
    m_clientTable->setSortingEnabled(false);
    m_clientTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    clientListLayout->addWidget(m_clientTable);
    m_refreshBtn = new QPushButton("刷新客户端列表", this);

    clientListLayout->addWidget(m_refreshBtn);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ClientInfoWidget::refreshClientList);

    m_detailGroup = new QGroupBox("客户端详细信息", this);
    QVBoxLayout* detailLayout = new QVBoxLayout(m_detailGroup);

    m_detailTextEdit = new QTextEdit(this);
    m_detailTextEdit->setReadOnly(true);
    m_detailTextEdit->setFontFamily("Consolas");
    m_detailTextEdit->setFontPointSize(10);

    detailLayout->addWidget(m_detailTextEdit);

    m_splitter->addWidget(m_clientListGroup);
    m_splitter->addWidget(m_detailGroup);
    m_splitter->setSizes({400, 400});

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_splitter);

    connect(m_clientTable, &QTableWidget::cellClicked, this, [this](int row, int /*column*/) {
        if (row >= 0 && row < m_clientTable->rowCount()) {
            QTableWidgetItem* item = m_clientTable->item(row, 0);
            if (item) {
                QString clientId = item->text();
                displayClientDetails(clientId);
            }
        }
    });
}
void ClientInfoWidget:: setClientList(const QList<ClientInfoWrap>& clientList)
{
//todo
}
void ClientInfoWidget::updateClientInfo(const QString& clientId, const ClientInfoWrap& clientData)
{
    Q_UNUSED(clientId);
    Q_UNUSED(clientData);
    refreshClientList();
}

void ClientInfoWidget::clearClientInfo()
{
    m_clientTable->setRowCount(0);
    m_detailTextEdit->clear();
    m_selectedClientId.clear();
}

void ClientInfoWidget::refreshClientList()
{
    m_clientTable->setSortingEnabled(false);

    if (!m_clientManager) {
        qDebug() << "ClientManager 未设置";
        return;
    }

    updateClientTable();

    if (m_clientTable->rowCount() > 0 && m_selectedClientId.isEmpty()) {
        m_clientTable->selectRow(0);
        QTableWidgetItem* item = m_clientTable->item(0, 0);
        if (item) {
            displayClientDetails(item->text());
        }
    }
    m_clientTable->setSortingEnabled(true);//解决排序空白问题

}

void ClientInfoWidget::onClientStatusChanged(const QString& clientId, bool isOnline, const QString& reason)
{
    Q_UNUSED(clientId);
    Q_UNUSED(isOnline);
    Q_UNUSED(reason);
    refreshClientList();
}

// 修正后的updateClientTable，改为通过 clientInfo 取值
void ClientInfoWidget::updateClientTable()
{
    if (!m_clientManager) {
        return;
    }

    m_clientTable->setRowCount(0);

    QStringList clientIds = m_clientManager->getAllClientIds();

    for (const QString& clientId : clientIds) {
        ClientInfoWrap clientData = m_clientManager->getClientData(clientId);
        const ClientInfo& info = clientData.clientInfo;

        int row = m_clientTable->rowCount();
        m_clientTable->insertRow(row);

        m_clientTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(info.client_id())));
        m_clientTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(info.hostname())));
        m_clientTable->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(info.ip_address())));

        QString connectTimeStr = clientData.connectTime.isValid()
                ? clientData.connectTime.toString("yyyy-MM-dd hh:mm:ss")
                : "未知";
        m_clientTable->setItem(row, 3, new QTableWidgetItem(connectTimeStr));

        QString statusText;
        QColor statusColor;
        if (clientData.isOnline) {
            statusText = "在线";
            statusColor = Qt::green;
        } else {
            QString timeStr = clientData.disconnectTime.isValid()
                    ? clientData.disconnectTime.toString("hh:mm:ss")
                    : "未知";
            QString reasonStr = clientData.disconnectReason.isEmpty()
                    ? ""
                    : QString(" - %1").arg(clientData.disconnectReason);
            statusText = QString("离线 (%1)%2").arg(timeStr, reasonStr);
            statusColor = Qt::red;
        }

        QTableWidgetItem* statusItem = new QTableWidgetItem(statusText);
        statusItem->setForeground(statusColor);
        m_clientTable->setItem(row, 4, statusItem);

        if (clientId == m_selectedClientId) {
            m_clientTable->selectRow(row);
        }
    }
}

void ClientInfoWidget::displayClientDetails(const QString& clientId)
{
    if (!m_clientManager) {
        return;
    }

    m_selectedClientId = clientId;

    ClientInfoWrap data = m_clientManager->getClientData(clientId);
    const ClientInfo& info = data.clientInfo;

    if (QString::fromStdString(info.client_id()).isEmpty()) {
        m_detailTextEdit->clear();
        return;
    }

    QString onlineStatus = data.isOnline ? "在线" : "离线";
    QString onlineColor = data.isOnline ? "green" : "red";

    QString durationStr;
    if (data.isOnline) {
        durationStr = formatDuration(data.connectTime);
    } else if (data.disconnectTime.isValid()) {
        durationStr = formatDuration(data.connectTime, data.disconnectTime);
    } else {
        durationStr = "未知";
    }

    QString lastHeartbeatStr = data.lastHeartbeat.isValid()
            ? data.lastHeartbeat.toString("yyyy-MM-dd hh:mm:ss")
            : "未知";

    QString disconnectTimeStr = data.disconnectTime.isValid()
            ? data.disconnectTime.toString("yyyy-MM-dd hh:mm:ss")
            : "N/A";

    QString disconnectReasonStr = data.disconnectReason.isEmpty()
            ? "N/A"
            : data.disconnectReason;
    QString detailsTemplate =
            "<style>"
            "  .title { font-size: 16px; font-weight: bold; color: #2c3e50; margin-bottom: 10px; }"
            "  .section { font-size: 14px; font-weight: bold; color: #34495e; margin-top: 15px; margin-bottom: 5px; }"
            "  .label { font-weight: bold; color: #7f8c8d; }"
            "  .value { color: #2c3e50; }"
            "  .online { color: green; font-weight: bold; }"
            "  .offline { color: red; font-weight: bold; }"
            "</style>"
            "<div class='title'>💻 客户端详细信息</div>"
            "<hr>"
            "<div class='section'>基本信息</div>"
            "<table>"
            "<tr><td class='label'>客户端ID:</td><td class='value'>%1</td></tr>"
            "<tr><td class='label'>主机名:</td><td class='value'>%2</td></tr>"
            "<tr><td class='label'>IP地址:</td><td class='value'>%3</td></tr>"
            "<tr><td class='label'>操作系统:</td><td class='value'>%4</td></tr>"
            "<tr><td class='label'>系统版本:</td><td class='value'>%5</td></tr>"
            "</table>"
            "<div class='section'>连接信息</div>"
            "<table>"
            "<tr><td class='label'>连接时间:</td><td class='value'>%6</td></tr>"
            "<tr><td class='label'>在线时长:</td><td class='value'>%7</td></tr>"
            "<tr><td class='label'>最后心跳:</td><td class='value'>%8</td></tr>"
            "<tr><td class='label'>状态:</td><td><span class='%9'>%10</span></td></tr>"
            "</table>"
            "<div class='section'>断开信息</div>"
            "<table>"
            "<tr><td class='label'>断开时间:</td><td class='value'>%11</td></tr>"
            "<tr><td class='label'>断开原因:</td><td class='value'>%12</td></tr>"
            "</table>";

    QString details = detailsTemplate.arg(
                QString::fromStdString(info.client_id())
                ).arg(
                QString::fromStdString(info.hostname())
                ).arg(
                QString::fromStdString(info.ip_address())
                ).arg(
                QString::fromStdString(info.os_type())
                ).arg(
                QString::fromStdString(info.os_version())
                ).arg(
                data.connectTime.toString("yyyy-MM-dd hh:mm:ss")
                ).arg(
                durationStr
                ).arg(
                lastHeartbeatStr
                ).arg(
                data.isOnline ? "online" : "offline"
                ).arg(
                onlineStatus
                ).arg(
                disconnectTimeStr
                ).arg(
                disconnectReasonStr
                );

    m_detailTextEdit->setHtml(details);
}

QString ClientInfoWidget::formatDuration(const QDateTime& start, const QDateTime& end)
{
    qint64 seconds = start.secsTo(end.isValid() ? end : QDateTime::currentDateTime());

    int days = seconds / 86400;
    int hours = (seconds % 86400) / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    if (days > 0) {
        return QString("%1天 %2:%3:%4").arg(days).arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    } else if (hours > 0) {
        return QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0'));
    }
}
