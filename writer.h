#ifndef WRITER_H
#define WRITER_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include "eventmessage.h"

class Writer : public QThread {
    Q_OBJECT

public:
    explicit Writer(QObject *parent = nullptr);
    ~Writer();

    void setLogFilePath(const QString &path);
    void enqueue(const EventMessage &msg);
    void finish();

protected:
    void run() override;

private:
    QString m_logFilePath;
    QQueue<EventMessage> m_queue;
    QMutex m_mutex;
    QWaitCondition m_wait;
    bool m_running = true;
};

#endif // WRITER_H
