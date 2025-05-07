#include "writer.h"
#include <QDateTime>

Writer::Writer(QObject *parent)
    : QThread(parent) {}

Writer::~Writer() {
    QMutexLocker locker(&m_mutex);
    m_running = false;
    m_wait.wakeAll();
    locker.unlock();
    wait();
}

void Writer::setLogFilePath(const QString &path) {
    QMutexLocker locker(&m_mutex);
    m_logFilePath = path;
}

void Writer::enqueue(const EventMessage &msg) {
    QMutexLocker locker(&m_mutex);
    m_queue.enqueue(msg);
    m_wait.wakeOne();
}

void Writer::run() {
    while (true) {
        QMutexLocker locker(&m_mutex);
        if (!m_running && m_queue.isEmpty()) break;

        if (m_queue.isEmpty()) {
            m_wait.wait(&m_mutex);
            continue;
        }

        EventMessage const msg = m_queue.dequeue();
        QString const path = m_logFilePath;
        locker.unlock();

        QFile file(path);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << msg.timestamp.toString("yyyy-MM-dd HH:mm:ss")
                << " Module " << msg.clientId
                << " [" << msg.type << "]: "
                << msg.text << "\n";
        }
    }
}
