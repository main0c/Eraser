#include "ConnectionControlWidget.h"

ConnectionControlWidget::ConnectionControlWidget(const QString& clientId, QWidget *parent)
    : QWidget(parent)
    , m_connectionGroup(nullptr)
    , m_serverAddressEdit(nullptr)
    , m_connectionStatusLabel(nullptr)
    , m_connectBtn(nullptr)
    , m_disconnectBtn(nullptr)
    , m_clientIdLabel(nullptr)
{
    setupUI();
    if (!clientId.isEmpty()) {
        setClientId(clientId);
    }
}

ConnectionControlWidget::~ConnectionControlWidget()
{
}

void ConnectionControlWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    m_connectionGroup = new QGroupBox("连接控制", this);
    QVBoxLayout* layout = new QVBoxLayout(m_connectionGroup);

    // 客户端ID显示
    m_clientIdLabel = new QLabel("客户端ID: ", this);
    m_clientIdLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_clientIdLabel);

    // 服务器地址输入
    QHBoxLayout* addressLayout = new QHBoxLayout();
    QLabel* serverAddrLabel = new QLabel("服务器地址:", this);
    m_serverAddressEdit = new QLineEdit("tcp://localhost:5555", this);
    m_serverAddressEdit->setPlaceholderText("例如: tcp://192.168.1.100:5555");
    addressLayout->addWidget(serverAddrLabel);
    addressLayout->addWidget(m_serverAddressEdit);
    layout->addLayout(addressLayout);

    // 状态和按钮布局
    QHBoxLayout* statusButtonLayout = new QHBoxLayout();

    m_connectionStatusLabel = new QLabel("状态: 未连接", this);
    m_connectionStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    statusButtonLayout->addWidget(m_connectionStatusLabel);
    statusButtonLayout->addStretch();

    m_connectBtn = new QPushButton("连接服务器", this);
    m_disconnectBtn = new QPushButton("断开连接", this);
    m_disconnectBtn->setEnabled(false);
    
    statusButtonLayout->addWidget(m_connectBtn);
    statusButtonLayout->addWidget(m_disconnectBtn);

    layout->addLayout(statusButtonLayout);
    layout->addStretch();

    mainLayout->addWidget(m_connectionGroup);
    
    // 连接信号
    connect(m_connectBtn, &QPushButton::clicked, this, &ConnectionControlWidget::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &ConnectionControlWidget::onDisconnectClicked);
}

QString ConnectionControlWidget::getServerAddress() const
{
    return m_serverAddressEdit->text().trimmed();
}

void ConnectionControlWidget::setServerAddress(const QString& address)
{
    m_serverAddressEdit->setText(address);
}

void ConnectionControlWidget::setConnected(bool connected)
{
    updateStatusDisplay(connected);
}

void ConnectionControlWidget::setClientId(const QString& clientId)
{
    m_clientIdLabel->setText(QString("客户端ID: %1").arg(clientId));
}

void ConnectionControlWidget::updateStatusDisplay(bool connected)
{
    if (connected) {
        m_connectionStatusLabel->setText("状态: 已连接");
        m_connectionStatusLabel->setStyleSheet("color: green; font-weight: bold;");
        m_connectBtn->setEnabled(false);
        m_disconnectBtn->setEnabled(true);
        m_serverAddressEdit->setEnabled(false);
    } else {
        m_connectionStatusLabel->setText("状态: 未连接");
        m_connectionStatusLabel->setStyleSheet("color: red; font-weight: bold;");
        m_connectBtn->setEnabled(true);
        m_disconnectBtn->setEnabled(false);
        m_serverAddressEdit->setEnabled(true);
    }
}

void ConnectionControlWidget::onConnectClicked()
{
    QString serverAddress = getServerAddress();
    if (serverAddress.isEmpty()) {
        return;
    }
    emit connectRequested(serverAddress);
}

void ConnectionControlWidget::onDisconnectClicked()
{
    emit disconnectRequested();
}
