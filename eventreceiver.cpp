#include "eventreceiver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

EventReceiver::EventReceiver(QObject *parent) : QTcpServer(parent) {
    connect(this, &QTcpServer::newConnection, this, &EventReceiver::onNewConnection);
}

void EventReceiver::stopClient(uint32_t clientId) {
    if (m_clientSockets.contains(clientId)) {
        QTcpSocket *socket = m_clientSockets[clientId];
        socket->disconnectFromHost();
        socket->deleteLater();
        m_clientSockets.remove(clientId);
    }
}

void EventReceiver::onReadyRead(QTcpSocket* socket) {
    if (!socket) return;

    bool ok = false;
    uint32_t parsedClientSocket = socket->property("clientId").toUInt(&ok);
    if (!ok || !m_clientSockets.contains(parsedClientSocket)) return;

    QByteArray data = socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    uint32_t const parsedClientId = static_cast<uint32_t>(obj["client"].toInt());
    QString const type = obj["type"].toString();
    QString const message = obj["message"].toString();
    QString const timestampStr = obj["timestamp"].toString();
    QDateTime const timestamp = QDateTime::fromString(timestampStr, "yyyy-MM-dd HH:mm:ss");
    bool isMsgCorrupted {false};

    EventMessage msg;
    msg.type = "WARNING";
    if (parsedClientId < 1 || parsedClientId > 3) {
        msg.clientId = 0;
        msg.text = QString("Invalid module number on the received message!!");
        msg.timestamp = QDateTime::currentDateTime();
        isMsgCorrupted = true;
    }
    else if (!QSet<QString>{"INFO", "WARNING", "ERROR", "CRITICAL"}.contains(type)) {
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

    emit messageReceived(msg);

    // if module 3 has a critical message stop the entire logger
    if ((msg.type == "CRITICAL") && (msg.clientId == 3)) {
        for (QTcpSocket* s : m_clientSockets) {
            if (s) {
                s->disconnect();
                s->disconnectFromHost();
                s->deleteLater();
            }
        }
        m_clientSockets.clear();
    }
    // for the other 2 modules just stop the respective module
    else if (msg.type == "CRITICAL") {
        stopClient(msg.clientId);
        emit clientStopped(msg.clientId);
    }
}

void EventReceiver::onNewConnection() {
    QTcpSocket *socket = this->nextPendingConnection();

    if (!socket->waitForReadyRead(500)) {
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    QByteArray initialData = socket->peek(1024);
    QJsonDocument doc = QJsonDocument::fromJson(initialData);
    if (!doc.isObject()) {
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    QJsonObject obj = doc.object();
    uint32_t clientId = static_cast<uint32_t>(obj["client"].toInt());

    if (clientId < 1 || clientId > 3) {
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    if (m_clientSockets.contains(clientId)) {
        QTcpSocket* oldSocket = m_clientSockets[clientId];
        oldSocket->disconnectFromHost();
        oldSocket->deleteLater();
        m_clientSockets.remove(clientId);
    }

    m_clientSockets[clientId] = socket;
    socket->setProperty("clientId", clientId);

    connect(socket, &QTcpSocket::readyRead, this, [=]() { onReadyRead(socket); });
    if (socket->bytesAvailable() > 0) {
        QTimer::singleShot(0, this, [=]() { onReadyRead(socket); });
    }
    connect(socket, &QTcpSocket::disconnected, this, [=]() {
        bool ok = false;
        uint32_t clientId = socket->property("clientId").toUInt(&ok);
        if (ok && m_clientSockets.contains(clientId)) {
            m_clientSockets.remove(clientId);
        }
        socket->deleteLater();
    });
}
