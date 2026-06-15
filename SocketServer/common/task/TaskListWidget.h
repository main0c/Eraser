#ifndef TASKLISTWIDGET_H
#define TASKLISTWIDGET_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>
#include <QDateTime>
#include <QFormLayout>
#include "ErasureTaskManager.h"
#include <QTextEdit>

class TaskTreeWidgetItem : public QTreeWidgetItem {
public:
    enum ItemType {
        TypeClient = QTreeWidgetItem::UserType,
        TypeDisk = QTreeWidgetItem::UserType + 1,
        TypeTask = QTreeWidgetItem::UserType + 2
    };

    explicit TaskTreeWidgetItem(QTreeWidget *parent, ItemType type)
        : QTreeWidgetItem(parent, type), m_type(type) {}

    explicit TaskTreeWidgetItem(QTreeWidgetItem *parent, ItemType type)
        : QTreeWidgetItem(parent, type), m_type(type) {}

    ItemType type() const { return m_type; }

    QString clientId;
    QString taskId;
    QString diskId;
    ErasureTask taskData; // 仅当 type == TypeTask 时有效
    QString diskModel;    // 仅当 type == TypeDisk 时有效，或者从第一个子任务获取

private:
    ItemType m_type;
};


class TaskListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TaskListWidget(QWidget *parent = nullptr);
    void setTaskManager(ErasureTaskManager* manager);

    void resizeEvent(QResizeEvent *event);
public slots:
    void refreshTaskList();
    void onTaskCreated(const QByteArray& taskData);
    void onTaskProgressUpdated(const QByteArray& taskData);
    void onTaskStatusChanged(const QByteArray& taskData);
    void onTaskCompleted(const QByteArray& taskData);
    void onClientFilterChanged(const QString& clientId);
    void clearCompletedTasks();
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
protected slots:
private:
    void setupUI();
    void updateTaskTree();
    void addClientNode(const QString& clientId);
    void addDiskNode(QTreeWidgetItem* clientNode, const QString& diskId);
    void addTaskNode(QTreeWidgetItem* diskNode, const ErasureTask& task);
    void setupTaskItem(TaskTreeWidgetItem *item, const ErasureTask &task);
    void showTaskDetails(const QString &taskId);
    
    QGroupBox* m_taskListGroup;
    QTreeWidget* m_treeWidget;
    QPushButton* m_refreshBtn;
    QPushButton* m_clearCompletedBtn;
    QLabel* m_taskCountLabel;
    QComboBox* m_clientFilterCombo;
    QComboBox* m_filterCombo;
    QLineEdit* m_searchEdit;
    ErasureTaskManager* m_taskManager;
    bool m_viewModeFlat;
    QWidget *m_detailWidget;
    QFormLayout *m_detailFormLayout;
    QTextEdit* m_detailTextBrowser;
    QPushButton* m_viewModeToggleBtn;
    QString m_currentSelectedTaskId;
    QString m_filterClientId;


    void clearDetailView();
    void updateDetailView(const QString &taskId);
};

#endif // TASKLISTWIDGET_H
