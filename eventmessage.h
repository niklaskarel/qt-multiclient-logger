#ifndef EVENTMESSAGE_H
#define EVENTMESSAGE_H

#include <QString>
#include <QDateTime>

struct EventMessage {
    uint32_t clientId;
    QString type; // INFO, WARNING, ERROR
    QString text;
    QDateTime timestamp;
};

#endif // EVENTMESSAGE_H
