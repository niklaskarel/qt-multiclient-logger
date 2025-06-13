#ifndef EVENTRECEIVER_H
#define EVENTRECEIVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
#include <QMap>
#include "eventmessage.h"

class EventReceiver : public QTcpServer {
    Q_OBJECT
public:
    explicit EventReceiver(QObject *parent = nullptr);
    void stopClient(uint32_t clientId);
    void setLocalPort(const int port) { m_localPort = port; }
    int getLocalPort() const { return m_localPort; }

signals:
    void messageReceived(const EventMessage &msg);
    void clientStopped(uint32_t clientId);

private slots:
    void onNewConnection();
    void onReadyRead(QTcpSocket* socket);

private:
    QMap<uint32_t, QTcpSocket*> m_clientSockets;
    int m_localPort;
};

#endif // EVENTRECEIVER_H
