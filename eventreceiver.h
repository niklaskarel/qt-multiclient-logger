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

signals:
    void messageReceived(const EventMessage &msg);
    void clientStopped(uint32_t clientId);

private slots:
    void onNewConnection();
    void onReadyRead(QTcpSocket* socket);

private:
    QTcpSocket *clientSocket = nullptr;
    QMap<uint32_t, QTcpSocket*> clientSockets;
};

#endif // EVENTRECEIVER_H
