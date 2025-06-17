#ifndef EVENTRECEIVER_H
#define EVENTRECEIVER_H

#include <QTcpServer>
#include <QHostAddress>
#include <QMap>
#include "eventmessage.h"

class QTcpSocket;

class EventReceiver : public QTcpServer
{
    Q_OBJECT
public:
    explicit EventReceiver(QObject *parent = nullptr);
    void close();
    int getLocalPort() const;
    void setLocalPort(int port);
    void stopClient(uint32_t clientId);

signals:
    void messageReceived(const EventMessage &msg);
    void clientStopped(int clientId);

protected slots:
    void handleNewConnection();
    void onReadyRead(QTcpSocket *socket);

private:
    int m_localPort;
    QMap<uint32_t, QTcpSocket*> m_clients;
    QMap<QTcpSocket*, QByteArray> m_pendingBuffers;
};

#endif // EVENTRECEIVER_H
