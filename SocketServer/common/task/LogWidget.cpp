#include "LogWidget.h"
#include <QMessageBox>

LogWidget::LogWidget(QWidget *parent)
    : QWidget(parent)
    , m_logGroup(nullptr)
    , m_logTextEdit(nullptr)
    , m_clearBtn(nullptr)
    , m_exportBtn(nullptr)
{
    setupUI();
}

LogWidget::~LogWidget()
{
}

void LogWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    m_logGroup = new QGroupBox("系统日志", this);
    QVBoxLayout* layout = new QVBoxLayout(m_logGroup);

    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    layout->addWidget(m_logTextEdit);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_clearBtn = new QPushButton("清空日志", this);
    m_exportBtn = new QPushButton("导出日志", this);
    buttonLayout->addWidget(m_clearBtn);
    buttonLayout->addWidget(m_exportBtn);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    
    mainLayout->addWidget(m_logGroup);
    
    // 连接信号
    connect(m_clearBtn, &QPushButton::clicked, this, &LogWidget::onClearClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &LogWidget::onExportClicked);
}

void LogWidget::addLogEntry(const QString& message, const QString& type)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString logEntry = QString("[%1] [%2] %3").arg(timestamp, type, message);

    m_logTextEdit->append(logEntry);

    // 自动滚动到底部
    QScrollBar* scrollBar = m_logTextEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void LogWidget::clearLog()
{
    m_logTextEdit->clear();
    addLogEntry("日志已清空");
}

void LogWidget::exportLog()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出日志",
                                                    QString("client_log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
                                                    "文本文件 (*.txt)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << m_logTextEdit->toPlainText();
            addLogEntry(QString("日志已导出到: %1").arg(fileName), "SUCCESS");
        }
    }
}

QString LogWidget::getLogContent() const
{
    return m_logTextEdit->toPlainText();
}

void LogWidget::onClearClicked()
{
    clearLog();
}

void LogWidget::onExportClicked()
{
    exportLog();
}
