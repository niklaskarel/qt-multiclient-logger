#include "eventreceiver.h"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QThread>
#include <concepts>

template<class F>
concept QtZeroArgInvocable =
    std::invocable<F&> &&
    std::same_as<void, std::invoke_result_t<F&>> &&
    std::is_move_constructible_v<std::decay_t<F>>;

template<class Obj, class F>
    requires std::derived_from<Obj, QObject> && QtZeroArgInvocable<F>
void invokeOnObjectThread(Obj * obj, F&& func)
{
    if (QThread::currentThread() == obj->thread()) {
        func();
    } else {
        QMetaObject::invokeMethod(obj,[fn = std::forward<F>(func)]() mutable {
            fn();
        }, Qt::QueuedConnection);
    }
}

EventReceiver::EventReceiver(QObject *parent)
    : QObject(parent)
{
}

EventReceiver::~EventReceiver()
{
    qDebug() << "~EventReceiver current=" << QThread::currentThread()
    << " affinity=" << this->thread();
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, &EventReceiver::close, Qt::BlockingQueuedConnection);
    } else {
       close();
    }
}

bool EventReceiver::listen(const QHostAddress &address, const uint16_t port)
{
    if (!m_server)
    {
        bool success {false};
        QMetaObject::invokeMethod(this, [this, address, port, &success] {
            m_server = std::make_unique<QTcpServer>(this); // Now constructed in the correct thread
            connect(m_server.get(), &QTcpServer::newConnection, this, &EventReceiver::handleNewConnection);
            success = m_server->listen(address, port);
        }, Qt::BlockingQueuedConnection);
        return success;
    }

    return m_server->listen(address, port);
}

void EventReceiver::close()
{
    qDebug() << "Receiver thread is quitting...";
    invokeOnObjectThread(this, [this] {
        closeImpl();
    });
}

void EventReceiver::closeImpl()
{
    if (!m_clients.empty())
    {
        const QList<uint32_t> ids = m_clients.keys();
        for (uint32_t id : ids)
            closeSocketOnce(id);

        m_clients.clear();
        m_pendingBuffers.clear();
    }

    if (m_server)
    {
        if (m_server->isListening())
        {
            m_server->close();
            m_server.reset();
        }
    }
}

void EventReceiver::closeSocketOnce(const uint32_t clientId)
{
    try {
        auto it = m_clients.find(clientId);
        if (it == m_clients.end())
            return;

        QTcpSocket* socket = it.value();
        if (!socket)
            return;

        // One-shot guard
        if (m_closing.contains(socket))
            return;

        m_closing.insert(socket);

        socket->disconnect(this);

        socket->disconnectFromHost();
        if (socket->state() != QAbstractSocket::UnconnectedState)
            socket->close();

        m_pendingBuffers.remove(socket);
        const int key = m_clients.key(socket, -1);
        if (key != -1)
            m_clients.remove(key);

        // Defer deletion on owning thread (idempotent)
        socket->deleteLater();
    }
    catch (const std::exception &e) {
        qDebug() << "Exception while closing the sockets message:" << e.what();
    } catch (...) {
        qDebug() << "Unknown error while closing the sockets.";
    }
}

void EventReceiver::handleNewConnection()
{    
    while (m_server->hasPendingConnections())
    {
        QTcpSocket *clientSocket = m_server->nextPendingConnection();
        if (!clientSocket) continue;

        connect(clientSocket, &QTcpSocket::readyRead, this, [this]() {
            QPointer<QTcpSocket> sock = qobject_cast<QTcpSocket*>(sender());
            onReadyRead(sock);
        });

        connect(clientSocket, &QTcpSocket::disconnected, this, [this]() {
            QPointer<QTcpSocket> sock = qobject_cast<QTcpSocket*>(sender());
            if (!sock)
                return;

            if (m_closing.contains(sock.data()))
                return;

            if (!m_clients.values().contains(sock.data()))
                return;

            const int clientKey = m_clients.key(sock, -1);
            // for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it)
            //     qDebug() << it.key() << reinterpret_cast<void*>(it.value());
            if (clientKey != -1)
            {
                m_clients.remove(clientKey);
            }
            // for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it)
            //     qDebug() << it.key() << reinterpret_cast<void*>(it.value());
            m_pendingBuffers.remove(sock.data());
            sock->deleteLater();
        });
    }
}

bool EventReceiver::isListening() const
{
    return m_server && m_server->isListening();
}

void EventReceiver::onReadyRead(QPointer<QTcpSocket> socket)
{
    if (!socket) {
        qDebug() << "onReadyRead called but sender() was null!";
        return;
    }

    try
    {
        QByteArray data = socket->readAll();
        if (data.isEmpty())
            return;

        if (!m_pendingBuffers.contains(socket))
        {
            m_pendingBuffers[socket] = QByteArray();
        }
        m_pendingBuffers[socket].append(data.data());
        while (true)
        {
            int newlineIndex = m_pendingBuffers[socket].indexOf('\n');
            if (newlineIndex == -1)
                break;

            QByteArray line = m_pendingBuffers[socket].left(newlineIndex).trimmed();
            m_pendingBuffers[socket].remove(0, newlineIndex + 1);

            if (line.isEmpty())
                continue;

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            {
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
            if (parsedClientId < 1 || parsedClientId > 3)
            {
                msg.clientId = 0;
                msg.text = QString("Invalid module number on the received message!!");
                msg.timestamp = QDateTime::currentDateTime();
                isMsgCorrupted = true;
            }
            else if (!QSet<QString>{"INFO", "WARNING", "ERROR", "CRITICAL", "DATA"}.contains(type))
            {
                msg.clientId = parsedClientId;
                msg.text = QString("Corrupted message type received from client!Invalid message type!");
                msg.timestamp = QDateTime::currentDateTime();
                isMsgCorrupted = true;
            }
            else if (!timestamp.isValid())
            {
                msg.clientId = parsedClientId;
                msg.text = QString("Corrupted message type received from client!Invalid timestamp!");
                msg.timestamp = QDateTime::currentDateTime();
                isMsgCorrupted = true;
            }

            if (isMsgCorrupted)
            {
                emit messageReceived(msg);
                return;
            }

            // Quality check of the message

            msg.clientId = parsedClientId;
            msg.type = type;
            msg.text = message;
            msg.timestamp = timestamp;

            m_clients[msg.clientId] = socket.data();

            // if module 3 has a critical message stop the entire logger
            if ((msg.type == "CRITICAL"))
            {
                if (msg.clientId == 3)
                {
                    QTimer::singleShot(0, this, [this]{
                        close();
                    });
                }
                // for the other 2 modules just stop the respective module
                else
                {
                    stopClient(msg.clientId);
                }
            }
            emit messageReceived(msg);
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
    qDebug() << "stopClient called for" << clientId << "in thread";
    invokeOnObjectThread(this, [this, clientId] {
        closeSocketOnce(clientId);
    });
}
