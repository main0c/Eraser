#include "ErasureControlWidget.h"
#include <QMessageBox>


ErasureControlWidget::ErasureControlWidget(QWidget *parent)
    : QWidget(parent)
    , m_controlGroup(nullptr)
    , m_erasureMethodCombo(nullptr)
    , m_passCountSpin(nullptr)
    , m_verificationCheck(nullptr)
    , m_startBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_startStopContainer(nullptr)
    , m_pauseBtn(nullptr)
    , m_resumeBtn(nullptr)
    , m_pauseResumeContainer(nullptr)
{
    setupUI();
}

ErasureControlWidget::~ErasureControlWidget()
{
}

void ErasureControlWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    m_controlGroup = new QGroupBox("擦除控制", this);
    QVBoxLayout* erasureLayout = new QVBoxLayout(m_controlGroup);

    // 擦除方法和遍数设置
    QHBoxLayout* methodLayout = new QHBoxLayout();
    methodLayout->addWidget(new QLabel("擦除方法:"));
    m_erasureMethodCombo = new QComboBox(this);
    m_erasureMethodCombo->addItems({"Quick", "DoD 5220.22-M", "Gutmann", "Zero Fill"});
    methodLayout->addWidget(m_erasureMethodCombo);
    
    methodLayout->addWidget(new QLabel("擦除遍数:"));
    m_passCountSpin = new QSpinBox(this);
    m_passCountSpin->setRange(1, 35);
    m_passCountSpin->setValue(1);
    methodLayout->addWidget(m_passCountSpin);

    m_verificationCheck = new QCheckBox("启用擦除验证", this);
    m_verificationCheck->setChecked(true);
    methodLayout->addStretch();
    methodLayout->addWidget(m_verificationCheck);
    
    erasureLayout->addLayout(methodLayout);

    m_startStopContainer = new QWidget(this);
    // 控制按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QHBoxLayout* startStopLayout = new QHBoxLayout(m_startStopContainer);
    startStopLayout->setContentsMargins(0, 0, 0, 0);
    startStopLayout->setSpacing(6); 
    m_startBtn = new QPushButton("开始擦除", m_startStopContainer);
    m_stopBtn = new QPushButton("停止擦除", m_startStopContainer);
    startStopLayout->addWidget(m_startBtn);
    startStopLayout->addWidget(m_stopBtn);
    m_pauseResumeContainer = new QWidget(this);
    QHBoxLayout* pauseResumeLayout = new QHBoxLayout(m_pauseResumeContainer);
    pauseResumeLayout->setContentsMargins(0, 0, 0, 0);
    pauseResumeLayout->setSpacing(6);
    m_pauseBtn = new QPushButton("暂停擦除", m_pauseResumeContainer);
    m_resumeBtn = new QPushButton("恢复擦除", m_pauseResumeContainer);
    
    pauseResumeLayout->addWidget(m_pauseBtn);
    pauseResumeLayout->addWidget(m_resumeBtn);
    
    // 底部按钮栏
    m_generateBtn = new QPushButton("生成模拟磁盘", this);
    m_sendBtn = new QPushButton("发送磁盘信息", this);

    buttonLayout->addWidget(m_generateBtn);
    buttonLayout->addWidget(m_sendBtn);
    connect(m_generateBtn, &QPushButton::clicked, this, &ErasureControlWidget::generateDiskRequested);
    connect(m_sendBtn, &QPushButton::clicked, this, &ErasureControlWidget::sendDiskInfoRequested);

    buttonLayout->addStretch();

    buttonLayout->addWidget(m_startStopContainer);
    buttonLayout->addWidget(m_pauseResumeContainer);
    erasureLayout->addLayout(buttonLayout);
    
    mainLayout->addWidget(m_controlGroup);
    
    m_stopBtn->setEnabled(false);
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    // 连接信号
    connect(m_startBtn, &QPushButton::clicked, this, &ErasureControlWidget::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &ErasureControlWidget::onStopClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &ErasureControlWidget::onPauseClicked);
    connect(m_resumeBtn, &QPushButton::clicked, this, &ErasureControlWidget::onResumeClicked);
    connect(m_erasureMethodCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &ErasureControlWidget::onMethodChanged);
}

QString ErasureControlWidget::getErasureMethod() const
{
    return m_erasureMethodCombo->currentText();
}

int ErasureControlWidget::getPassCount() const
{
    return m_passCountSpin->value();
}

bool ErasureControlWidget::isVerificationEnabled() const
{
    return m_verificationCheck->isChecked();
}

void ErasureControlWidget::setControlsEnabled(bool enabled)
{
    m_erasureMethodCombo->setEnabled(enabled);
    m_passCountSpin->setEnabled(enabled);
    m_verificationCheck->setEnabled(enabled);
}

void ErasureControlWidget::setStartButtonEnabled(bool enabled)
{
    m_startBtn->setEnabled(enabled);
}

void ErasureControlWidget::setStopButtonEnabled(bool enabled)
{
    m_stopBtn->setEnabled(enabled);
}

void ErasureControlWidget::setPauseButtonEnabled(bool enabled)
{
    m_pauseBtn->setEnabled(enabled);
    m_resumeBtn->setEnabled(!enabled);
}

void ErasureControlWidget::setResumeButtonEnabled(bool enabled)
{
    m_pauseBtn->setVisible(!enabled);
    m_resumeBtn->setVisible(enabled);
}

void ErasureControlWidget::onStartClicked()
{
    emit startErasureRequested();
}

void ErasureControlWidget::onStopClicked()
{
    emit stopErasureRequested();
}

void ErasureControlWidget::onPauseClicked()
{
    emit pauseErasureRequested();
}

void ErasureControlWidget::onResumeClicked()
{
    emit resumeErasureRequested();
}

void ErasureControlWidget::onMethodChanged(int index)
{
    Q_UNUSED(index);
    QString method = m_erasureMethodCombo->currentText();
    int passCount = m_passCountSpin->value();
    emit erasureMethodChanged(method, passCount);
}
