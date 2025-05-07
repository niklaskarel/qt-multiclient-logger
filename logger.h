#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTimer>
#include <deque>
#include "eventmessage.h"
#include "writer.h"

constexpr int LOGGER_MAX_SIZE = 100;
constexpr int LOGGER_FLUSH_INTERVAL_MS = 200;

class Logger : public QObject {
    Q_OBJECT
public:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();
    void addMessage(const EventMessage &msg);
    void clear();
    bool isEmpty() const;
    void startNewLogFile();
    void logManualStop(uint32_t moduleId);

signals:
    void messageReady(const EventMessage &msg);

public slots:
    void flushBuffer();

private:
    void writeToFile(const EventMessage &msg);

private:
    std::deque<EventMessage> m_buffer;
    const int MAX_SIZE = 100;
    QTimer m_flushTimer;
    QString m_logFilePath;
    Writer* m_logWriter = nullptr;
};

#endif // LOGGER_H
