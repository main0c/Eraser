#ifndef ERASURECONTROLWIDGET_H
#define ERASURECONTROLWIDGET_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif


#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>

class ErasureControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ErasureControlWidget(QWidget *parent = nullptr);
    ~ErasureControlWidget();
    
    // 获取擦除配置
    QString getErasureMethod() const;
    int getPassCount() const;
    bool isVerificationEnabled() const;
    
    // 设置控件状态
    void setControlsEnabled(bool enabled);
    void setStartButtonEnabled(bool enabled);
    void setStopButtonEnabled(bool enabled);
    void setPauseButtonEnabled(bool enabled);
    void setResumeButtonEnabled(bool enabled);
    
signals:
    void startErasureRequested();
    void stopErasureRequested();
    void pauseErasureRequested();
    void resumeErasureRequested();
    void erasureMethodChanged(const QString& method, int passCount);
    void generateDiskRequested();
    void sendDiskInfoRequested();

private slots:
    void onStartClicked();
    void onStopClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onMethodChanged(int index);

private:
    void setupUI();
    
    // UI组件
    QGroupBox* m_controlGroup;
    QComboBox* m_erasureMethodCombo;
    QSpinBox* m_passCountSpin;
    QCheckBox* m_verificationCheck;
    QPushButton* m_generateBtn;
    QPushButton* m_sendBtn;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QWidget * m_startStopContainer;
    QPushButton* m_pauseBtn;
    QPushButton* m_resumeBtn;
    QWidget * m_pauseResumeContainer;
};

#endif // ERASURECONTROLWIDGET_H
