#ifndef DISKINFOWIDGET_H
#define DISKINFOWIDGET_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTableWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QHeaderView>
#include <QProgressBar>
#include <QMenu>
#include <QAction>
#include <QTextEdit>
#include <QSplitter>
#include "../common/messages.pb.h"
#if !defined(ZMQ_SERVER)

#include "ErasureControlWidget.h"
#endif
#include "MessageHandler.h"
using namespace socket_eraser;

class DiskInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiskInfoWidget(QWidget *parent = nullptr);
    ~DiskInfoWidget();
        
    // 数据管理 - 支持多客户端分组模式（新接口）
    void setClientList(const QList<ClientInfoWrap>& clientList);
    QList<ClientInfoWrap> getClientList() const { return m_clientList; }
    
    // 擦除控制接口
#if !defined(ZMQ_SERVER)
    ErasureControlWidget* getErasureControlWidget() const { return m_erasureControlWidget; }
    void setErasureControlsEnabled(bool enabled);
#endif

    QList<ClientDiskInfo> getSelectedDisks() const;
    QStringList getSelectedDiskIds() const;  // 获取选中磁盘的 ID 列表
    void restorePreviousSelection();  // 恢复之前的选择状态
    void updateDiskProgress(const ErasureTask& task);  // 重载版本，从 task 中提取 diskId // will depress
    void updateDiskProgress(const ErasureProgress& progress);

    // 显示磁盘详细信息
    void showDiskDetail(const QString& diskId);
    
    // 视图模式
    enum ViewMode {
        FlatMode,    // 平铺视图（直接显示所有磁盘，无分组）
        TreeMode     // 分组树形视图（按客户端分组显示磁盘）
    };
    void setViewMode(ViewMode mode);
    ViewMode getViewMode() const { return m_viewMode; }
    
    // 列可见性控制
    void setColumnVisible(int column, bool visible);
    bool isColumnVisible(int column) const;
    void showColumnConfigMenu(const QPoint& pos);
    
signals:
    void diskSelectionChanged();
    void diskDoubleClicked(const QString& diskId);
    void refreshRequested();
    
    // 擦除控制信号（转发 ErasureControlWidget 的信号）
    void startErasureRequested();
    void stopErasureRequested();
    void pauseErasureRequested();
    void resumeErasureRequested();
    void erasureMethodChanged(const QString& method, int passCount);

private slots:
    void onFilterTextChanged(const QString& text);
    void onViewModeChanged();
    void onDiskItemChanged(QTreeWidgetItem* item, int column);
    void onDiskItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onDiskSelectionChanged();  // 磁盘选择变化时更新详情
    void onHeaderCustomContextMenuRequested(const QPoint& pos);
    void onShowAllColumns();
    void onHideAllColumns();
    void onColumnConfigClicked();
private:
    void setupUI();
    void createActions();
    void updateDisplay();
    void fillTreeItem(QTreeWidgetItem* item, const ClientDiskInfo& disk);
    
    // 快速更新进度的辅助方法（直接更新 UI，不重建）
    void updateTreeItemProgress(QTreeWidgetItem* item, const ClientDiskInfo& disk);
    
    // 生成磁盘详细信息 HTML
    QString generateDiskDetailHtml(const ClientDiskInfo& disk) const;
    
    QString getStatusText(const ClientDiskInfo& disk) const;
    QColor getStatusColor(const QString& status) const;
    
    // UI组件
    QSplitter* m_splitter;  // 左右分割器
    QGroupBox* m_mainGroup;
    QHBoxLayout* m_topLayout;
    QLineEdit* m_filterEdit;
    QPushButton* m_viewModeToggleBtn;
    QPushButton* m_columnConfigBtn;

    QPushButton* m_refreshBtn;
    
    QTreeWidget* m_treeWidget;  // 统一使用 TreeWidget，支持平铺和分组两种模式
    
    // 详细信息显示
    QGroupBox* m_detailGroup;
    QTextEdit* m_detailTextEdit;
#if !defined(ZMQ_SERVER)

    // 擦除控制组件
    ErasureControlWidget* m_erasureControlWidget;
#endif
    // 数据
    QList<ClientDiskInfo> m_diskList;          // 单客户端模式下的磁盘列表（向后兼容）
    QList<ClientInfoWrap> m_clientList;        // 多客户端模式下的客户端列表
    ViewMode m_viewMode;
    // 列配置
    static const int ColumnCount = 14;
    bool m_columnVisibility[ColumnCount];
    QStringList m_columnNames;
    
    // Actions for context menu
    QAction* m_showAllColumnsAction;
    QAction* m_hideAllColumnsAction;
};

#endif // DISKINFOWIDGET_H
