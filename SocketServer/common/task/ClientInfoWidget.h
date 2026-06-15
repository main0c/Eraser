#ifndef CLIENTINFOWIDGET_H
#define CLIENTINFOWIDGET_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTextEdit>
#include <QGroupBox>
#include <QHeaderView>
#include <QSplitter>
#include <QPushButton>
#include "ClientManager.h"

class ClientInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClientInfoWidget(QWidget *parent = nullptr);
    void setClientManager(ClientManager* manager);
    void setClientList(const QList<ClientInfoWrap>& clientList);

public slots:
    void updateClientInfo(const QString& clientId, const ClientInfoWrap& clientData);
    void clearClientInfo();
    void refreshClientList();
    void onClientStatusChanged(const QString& clientId, bool isOnline, const QString& reason);

private:
    void setupUI();
    void updateClientTable();
    void displayClientDetails(const QString& clientId);
    QString formatDuration(const QDateTime& start, const QDateTime& end = QDateTime());
    
    QSplitter* m_splitter;
    QGroupBox* m_clientListGroup;
    QTableWidget* m_clientTable;
    QGroupBox* m_detailGroup;
    QTextEdit* m_detailTextEdit;
    QPushButton* m_refreshBtn;

    ClientManager* m_clientManager;
    QString m_selectedClientId;
};

#endif // CLIENTINFOWIDGET_H
