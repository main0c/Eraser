#include "ProgressDisplayWidget.h"


ProgressDisplayWidget::ProgressDisplayWidget(QWidget *parent)
    : QWidget(parent)
    , m_progressGroup(nullptr)
    , m_progressBar(nullptr)
    , m_progressLabel(nullptr)
    , m_speedLabel(nullptr)
    , m_timeRemainingLabel(nullptr)
    , m_statusLabel(nullptr)
{
    setupUI();
}

ProgressDisplayWidget::~ProgressDisplayWidget()
{
}

void ProgressDisplayWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    m_progressGroup = new QGroupBox("总体进度监控", this);
    QVBoxLayout* progressLayout = new QVBoxLayout(m_progressGroup);

    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat("%p%");
    progressLayout->addWidget(m_progressBar);

    // 详细信息网格布局
    QGridLayout* infoLayout = new QGridLayout();
    m_progressLabel = new QLabel("总体进度: 0%", this);
    m_speedLabel = new QLabel("总速度: 0 MB/s", this);
    m_timeRemainingLabel = new QLabel("剩余时间: --:--:--", this);
    m_statusLabel = new QLabel("状态: 就绪", this);
    
    infoLayout->addWidget(m_progressLabel, 0, 0);
    infoLayout->addWidget(m_speedLabel, 0, 1);
    infoLayout->addWidget(m_timeRemainingLabel, 1, 0);
    infoLayout->addWidget(m_statusLabel, 1, 1);
    
    progressLayout->addLayout(infoLayout);
    
    mainLayout->addWidget(m_progressGroup);
}

void ProgressDisplayWidget::setProgress(int percent)
{
    m_progressBar->setValue(percent);
    m_progressLabel->setText(QString("总体进度: %1%").arg(percent));
}

void ProgressDisplayWidget::setTotalSpeed(double speedMbPerSec)
{
    m_speedLabel->setText(QString("总速度: %1 MB/s").arg(speedMbPerSec, 0, 'f', 2));
}

void ProgressDisplayWidget::setTimeRemaining(int hours, int minutes, int seconds)
{
    if (hours >= 0 && minutes >= 0 && seconds >= 0) {
        m_timeRemainingLabel->setText(QString("剩余时间: %1:%2:%3")
                                      .arg(hours, 2, 10, QChar('0'))
                                      .arg(minutes, 2, 10, QChar('0'))
                                      .arg(seconds, 2, 10, QChar('0')));
    } else {
        m_timeRemainingLabel->setText("剩余时间: --:--:--");
    }
}

void ProgressDisplayWidget::setStatusText(const QString& status)
{
    m_statusLabel->setText(status);
}

void ProgressDisplayWidget::reset()
{
    m_progressBar->setValue(0);
    m_progressLabel->setText("总体进度: 0%");
    m_speedLabel->setText("总速度: 0 MB/s");
    m_timeRemainingLabel->setText("剩余时间: --:--:--");
    m_statusLabel->setText("状态: 就绪");
}
