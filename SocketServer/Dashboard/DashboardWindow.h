#ifndef DASHBOARDWINDOW_H
#define DASHBOARDWINDOW_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QMainWindow>
#include <QProgressBar>
#include "ServerCoreConnector.h"
#include "ClientInfoWidget.h"
#include "DiskInfoWidget.h"
#include "TaskListWidget.h"
#include "LogWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DashboardWindow; }
QT_END_NAMESPACE

class DashboardWindow : public QMainWindow
{
    Q_OBJECT

public:
    DashboardWindow(QWidget *parent = nullptr);
    ~DashboardWindow();

public slots:
    void closeEvent(QCloseEvent *event);
private slots:
    void connectToServer();
    void disconnectFromServer();
    void clearLogs();
    void exportLogs();
    void refreshData();

    // ServerCore连接信号
    void onConnected();
    void onDisconnected();
    void onConnectionError(const QString& error);

    // 查询响应信号
    void onClientsReceived(const QList<ClientInfo>& clients, int onlineCount, int offlineCount);
    void onClientDetailReceived(const ClientInfo& client);
    void onDiskInfoReceived(const QString& clientId, const QList<DiskInfo>& disks);
    void onTasksReceived(const QList<ErasureTask>& tasks);
    void onTaskDetailReceived(const ErasureTask& task);
    void onStatisticsReceived(int totalClients, int onlineClients, int offlineClients,
                             int totalTasks, int activeTasks, int completedTasks, int failedTasks);

    // 通知信号
    void onClientConnected(const QString& serverClientId, const ClientInfo& clientInfo);
    void onClientDisconnected(const QString& serverClientId, const QString& reason);
    void onDiskUpdated(const QString& serverClientId, const QList<DiskInfo>& disks);
    void onTaskCreated(const ErasureTask& task);
    void onTaskProgressUpdated(const ErasureProgress& progress);
    void onTaskStatusChanged(const ErasureTask& task);
    void onTaskCompleted(const ErasureTask& task);

private:
    Ui::DashboardWindow *ui;
    void updateClientCount();
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void addLogEntry(const QString& message, const QString& type = "INFO");

    // UI组件
    QWidget* m_centralWidget;
    QSplitter* m_mainSplitter;
    QSplitter* m_rightSplitter;

    // 左侧面板 - 客户端列表
    QWidget* m_clientListGroup;
    QPushButton* m_connectBtn;
    QPushButton* m_disconnectBtn;
    QPushButton* m_refreshBtn;

    // 右侧面板 - 详细信息
    QTabWidget* m_detailTabs;
    ClientInfoWidget* m_clientInfoWidget;
    DiskInfoWidget* m_diskInfoWidget;
    TaskListWidget* m_taskListWidget;

    // 底部面板 - 日志
    LogWidget* m_logWidget;

    // 状态栏
    QLabel* m_statusLabel;
    QLabel* m_clientCountLabel;
    QLabel* m_serverStatusLabel;
    QProgressBar* m_progressBar;

    // 菜单和动作
    QMenuBar* m_menuBar;
    QAction* m_connectAction;
    QAction* m_disconnectAction;
    QAction* m_refreshAction;
    QAction* m_clearLogsAction;
    QAction* m_exportLogsAction;
    QAction* m_exitAction;

    // 通信组件
    ServerCoreConnector* m_connector;
    
    QTimer* m_refreshTimer;

    // 状态
    bool m_connected;
    
    // 数据缓存
    QList<ClientInfoWrap> m_cachedClients;
};
#endif // DASHBOARDWINDOW_H