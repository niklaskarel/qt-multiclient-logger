#ifndef EVENTRECEIVER_H
#define EVENTRECEIVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QMap>
#include <QPointer>
#include "eventmessage.h"


class EventReceiver : public QObject
{
    Q_OBJECT
public:
    explicit EventReceiver(QObject *parent = nullptr);
    ~EventReceiver();

    EventReceiver(const EventReceiver&) = delete;
    EventReceiver& operator=(const EventReceiver&) = delete;
    EventReceiver(EventReceiver&&) = delete;
    EventReceiver& operator=(EventReceiver&&) = delete;

    bool listen (const QHostAddress &address, const quint16 port);
    void close();
    bool isListening() const;
    void stopClient(uint32_t clientId);

signals:
    void messageReceived(const EventMessage &msg);

protected slots:
    void handleNewConnection();
    void onReadyRead(QPointer<QTcpSocket> socket);

private:
    std::unique_ptr<QTcpServer> m_server;
    QMap<uint32_t, QTcpSocket*> m_clients;
    QMap<QTcpSocket*, QByteArray> m_pendingBuffers;
};

#endif // EVENTRECEIVER_H
