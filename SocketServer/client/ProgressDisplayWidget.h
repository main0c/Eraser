#ifndef PROGRESSDISPLAYWIDGET_H
#define PROGRESSDISPLAYWIDGET_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif



#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QProgressBar>
#include <QLabel>

class ProgressDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressDisplayWidget(QWidget *parent = nullptr);
    ~ProgressDisplayWidget();
    
    // 更新进度显示
    void setProgress(int percent);
    void setTotalSpeed(double speedMbPerSec);
    void setTimeRemaining(int hours, int minutes, int seconds);
    void setStatusText(const QString& status);
    
    // 重置显示
    void reset();

private:
    void setupUI();
    
    // UI组件
    QGroupBox* m_progressGroup;
    QProgressBar* m_progressBar;
    QLabel* m_progressLabel;
    QLabel* m_speedLabel;
    QLabel* m_timeRemainingLabel;
    QLabel* m_statusLabel;
};

#endif // PROGRESSDISPLAYWIDGET_H
