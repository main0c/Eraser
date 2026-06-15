#ifndef CLIENTMAINWINDOW_H
#define CLIENTMAINWINDOW_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QProgressBar>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QSplitter>
#include <QHeaderView>
#include <QTimer>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QDateTime>
#include <QTreeWidget>
#include "../common/MessageHandler.h"
#include "../common/messages.pb.h"
#include "ZMQClient.h"
#include "ErasureTaskManager.h"
#include "TaskListWidget.h"
#include "DiskInfoWidget.h"
#include "ProgressDisplayWidget.h"
#include "ConnectionControlWidget.h"
#include "LogWidget.h"
#if defined(DB_SUPPORT)
#include "../comLib/SqlDatabase/checkresultdb.h"
#endif

class ClientMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ClientMainWindow(QWidget *parent = nullptr);
    ~ClientMainWindow();

private slots:
    void connectToServer();
    void disconnectFromServer();
    void generateDiskInfo();
    void sendDiskInfo();
    void startErasure();
    void stopErasure();
    void pauseErasure();
    void resumeErasure();
    void updateErasureProgress();
    void onDiskSelectionChanged();
    void onDiskCheckStateChanged(QTreeWidgetItem *item, int column);
    void onErasureMethodChanged(const QString& method, int pass);
    void onConnectionStatusChanged();
    void clearLog();
    void exportLog();
    void refreshDiskList();
    void showAbout();
    void onTaskCreated(const QByteArray& taskData);
    void onTaskProgressUpdated(const QByteArray& taskData);
    void onTaskStatusChanged(const QByteArray& taskData);
    void onTaskCompleted(const QByteArray& taskData);
    void onClientRegister(const QString& server_clinent_id);
    // 测试多客户端功能
    void testMultiClientMode();

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnectionGroup();
    void setupDiskInfoGroup();
    void setupErasureControlGroup();
    void setupProgressGroup();
    void setupLogGroup();
    void setupTaskListGroup();
    
    void generateSimulatedDisks(QList<ClientInfoWrap>& clientList);
    void updateConnectionStatus(bool connected);
    void updateProgressDisplay();
    void updateTaskListDisplay();
    void addLogEntry(const QString& message, const QString& type = "INFO");
    void sendErasureStart(const ErasureTask &taskInfo);

    // 消息发送方法
    void sendClientRegister();
    void sendErasureProgress(const QString& taskId, const QString& diskId, double progress);
    void sendErasureComplete(const QString& taskId, const QString& diskId, bool success);
    void updateErasureControlStates();
#if defined(DB_SUPPORT)
    void restoreUnfinishedTasksFromDB();
#endif
    // ZMQ客户端回调
    void onClientStarted();
    void onClientStopped();
    void onErrorOccurred(const QString& error);

    // UI组件
    QWidget* m_centralWidget;
    QSplitter* m_mainSplitter;
    QTabWidget* m_tabWidget;
    // 连接控制组件
    ConnectionControlWidget* m_connectionControlWidget;
        
    // 磁盘信息组 - 使用新的 DiskInfoWidget（包含擦除控制）
    QGroupBox* m_diskInfoGroup;
    DiskInfoWidget* m_diskInfoWidget;
    
    // 进度显示组件
    ProgressDisplayWidget* m_progressDisplayWidget;
    
    TaskListWidget* m_taskListWidget;
    
    // 日志组件
    LogWidget* m_logWidget;
    
    // 状态栏
    QLabel* m_connectionStatusIcon;
    QLabel* m_statusLabel;  // 用于在状态栏显示消息
    
    // 菜单和动作
    QMenuBar* m_menuBar;
    QAction* m_connectAction;
    QAction* m_disconnectAction;
    QAction* m_exitAction;
    QAction* m_clearLogAction;
    QAction* m_exportLogAction;
    QAction* m_aboutAction;
    
    // 数据
    QList<ClientInfoWrap> m_clientList;        // 多客户端模式下的客户端列表
    QHash<QString, ErasureTask> m_erasureTasks;
    QStringList m_selectedDiskIds;
    
    // 状态
    bool m_isConnected;
    bool m_isErasing;
    bool m_isPaused;
    
    // 定时器
    QTimer* m_progressTimer;
    QTimer* m_connectionTimer;
    
    // 消息处理
    MessageHandler* m_messageHandler;

    // ZMQ客户端
    ZMQClient* m_zmqClient;
    ErasureTaskManager* m_taskManager;
    QString m_defaultServer;
    
    static const int HEARTBEAT_INTERVAL = 10000; // 10秒
    static const int PROGRESS_UPDATE_INTERVAL = 2000; // 2秒
    static const int CONNECTION_CHECK_INTERVAL = 5000; // 5秒
};

#endif // CLIENTMAINWINDOW_H
