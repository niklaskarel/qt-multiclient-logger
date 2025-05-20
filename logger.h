#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTimer>
#include <deque>
#include "eventmessage.h"
#include "writer.h"
#include <memory>

constexpr int LOGGER_MAX_SIZE = 100;
constexpr int LOGGER_FLUSH_INTERVAL_MS = 200;

class Logger : public QObject {
    Q_OBJECT
public:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();
    void addMessage(const EventMessage &msg);
    bool isEmpty() const;
    void startNewLogFile();
    void logManualStop(uint32_t moduleId);
    void stop();

signals:
    void messageReady(const EventMessage &msg);

public slots:
    void flushBuffer();

private:
    void clear();

private:
    std::deque<EventMessage> m_buffer;
    QTimer m_flushTimer;
    QString m_logFilePath;
    std::unique_ptr<Writer> m_logWriter;
};

#endif // LOGGER_H
