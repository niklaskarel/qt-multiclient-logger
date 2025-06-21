#include "eventreceiver.h"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDebug>
#include <QTimer>
#include <QDateTime>

EventReceiver::EventReceiver(QObject *parent)
    : QObject(parent),
    m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &EventReceiver::handleNewConnection);
}

bool EventReceiver::listen (const QHostAddress &address, const uint16_t port)
{
      return m_server->listen(address, port);
}

void EventReceiver::close()
{
    for (auto &socket : m_clients) {
        if (socket) {
            try {
                socket->disconnect();
                socket->disconnectFromHost();
                socket->deleteLater();
            }
            catch (const std::exception &e) {
                qDebug() << "Exception while closing the sockets message:" << e.what();
            } catch (...) {
                qDebug() << "Unknown error while closing the sockets.";
            }
        }
    }
    m_clients.clear();    
    if (m_server->isListening()){
        m_server->close();
    }
}

void EventReceiver::handleNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *clientSocket = m_server->nextPendingConnection();
        if (!clientSocket) continue;

        connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
            onReadyRead(clientSocket);
        });

        connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket]() {
            const int clientKey = m_clients.key(clientSocket, -1);
            if (clientKey != -1) {
                m_clients.remove(clientKey);
            }
             m_pendingBuffers.remove(clientSocket);
            clientSocket->deleteLater();
        });
    }
}

bool EventReceiver::isListening() const
{
    return m_server->isListening();
}

void EventReceiver::onReadyRead(QTcpSocket* socket)
{
    if (!socket) {
        qDebug() << "onReadyRead called but sender() was null!";
        return;
    }

    try {
        QByteArray data = socket->readAll();
        if (data.isEmpty()) return;
        if (!m_pendingBuffers.contains(socket))
            m_pendingBuffers[socket] = QByteArray();
        m_pendingBuffers[socket].append(data);
        while (true)
        {
            int newlineIndex = m_pendingBuffers[socket].indexOf('\n');
            if (newlineIndex == -1) break;

            QByteArray line = m_pendingBuffers[socket].left(newlineIndex).trimmed();
            m_pendingBuffers[socket].remove(0, newlineIndex + 1);

            if (line.isEmpty()) continue;

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                qWarning() << "Invalid JSON:" << parseError.errorString() << "Data:" << line;
                continue;
            }

            QJsonObject obj = doc.object();
            const uint32_t parsedClientId = static_cast<uint32_t>(obj["client"].toInt());
            const QString type = obj["type"].toString();
            const QString message = obj["message"].toString();
            const QString timestampStr = obj["timestamp"].toString();
            const QDateTime timestamp = QDateTime::fromString(timestampStr, "yyyy-MM-dd HH:mm:ss");
            bool isMsgCorrupted {false};

            EventMessage msg;
            msg.type = "WARNING";
            if (parsedClientId < 1 || parsedClientId > 3) {
                msg.clientId = 0;
                msg.text = QString("Invalid module number on the received message!!");
                msg.timestamp = QDateTime::currentDateTime();
                isMsgCorrupted = true;
            }
            else if (!QSet<QString>{"INFO", "WARNING", "ERROR", "CRITICAL", "DATA"}.contains(type)) {
                msg.clientId = parsedClientId;
                msg.text = QString("Corrupted message type received from client!Invalid message type!");
                msg.timestamp = QDateTime::currentDateTime();
                isMsgCorrupted = true;
            }
            else if (!timestamp.isValid()) {
                msg.clientId = parsedClientId;
                msg.text = QString("Corrupted message type received from client!Invalid timestamp!");
                msg.timestamp = QDateTime::currentDateTime();
                isMsgCorrupted = true;
            }

            if (isMsgCorrupted) {
                emit messageReceived(msg);
                return;
            }

            // Quality check of the message

            msg.clientId = parsedClientId;
            msg.type = type;
            msg.text = message;
            msg.timestamp = timestamp;

            m_clients[msg.clientId] = socket;
            emit messageReceived(msg);


            // if module 3 has a critical message stop the entire logger
            if ((msg.type == "CRITICAL")) {
                if (msg.clientId == 3)
                {
                    for (QTcpSocket* s : m_clients) {
                        if (s) {
                            connect(s, &QTcpSocket::disconnected, s, &QTcpSocket::deleteLater);
                            s->disconnectFromHost();

                            // delete the socket abdurtply if danglisng
                            QTimer::singleShot(3000, s, [s]() {
                                if (s->state() != QAbstractSocket::UnconnectedState) {
                                    s->abort();
                                    s->deleteLater();
                                }
                            });
                        }
                    }
                    // Defer the map cleanup
                    QTimer::singleShot(1000, this, [this]() {
                        m_clients.clear();
                    });
                }
                // for the other 2 modules just stop the respective module
                else {
                    stopClient(msg.clientId);
                }
            }
        }
    }
    catch (const std::exception &e) {
        qDebug() << "Exception while handling CRITICAL message:" << e.what();
    } catch (...) {
        qDebug() << "Unknown error while handling CRITICAL message.";
    }

}

void EventReceiver::stopClient(uint32_t clientId)
{
    if (m_clients.contains(clientId)) {
        QTcpSocket *socket = m_clients[clientId];
        socket->disconnectFromHost();
        socket->deleteLater();
        m_clients.remove(clientId);
    }
}
