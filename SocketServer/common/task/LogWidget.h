#ifndef LOGWIDGET_H
#define LOGWIDGET_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif


#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QScrollBar>

class LogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogWidget(QWidget *parent = nullptr);
    ~LogWidget();
    
    // 添加日志条目
    void addLogEntry(const QString& message, const QString& type = "INFO");
    
    // 清空日志
    void clearLog();
    
    // 导出日志
    void exportLog();
    
    // 获取日志内容
    QString getLogContent() const;

private slots:
    void onClearClicked();
    void onExportClicked();

private:
    void setupUI();
    
    // UI组件
    QGroupBox* m_logGroup;
    QTextEdit* m_logTextEdit;
    QPushButton* m_clearBtn;
    QPushButton* m_exportBtn;
};

#endif // LOGWIDGET_H
