#ifndef CONNECTIONCONTROLWIDGET_H
#define CONNECTIONCONTROLWIDGET_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif


#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class ConnectionControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionControlWidget(const QString& clientId = "", QWidget *parent = nullptr);
    ~ConnectionControlWidget();
    
    // 获取连接信息
    QString getServerAddress() const;
    void setServerAddress(const QString& address);
    
    // 设置连接状态
    void setConnected(bool connected);
    void setClientId(const QString& clientId);
    
signals:
    void connectRequested(const QString& serverAddress);
    void disconnectRequested();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();

private:
    void setupUI();
    void updateStatusDisplay(bool connected);
    
    // UI组件
    QGroupBox* m_connectionGroup;
    QLineEdit* m_serverAddressEdit;
    QLabel* m_connectionStatusLabel;
    QPushButton* m_connectBtn;
    QPushButton* m_disconnectBtn;
    QLabel* m_clientIdLabel;
};

#endif // CONNECTIONCONTROLWIDGET_H
