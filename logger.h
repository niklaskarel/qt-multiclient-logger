#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTimer>
#include <deque>
#include "eventmessage.h"
#include "writer.h"
#include <memory>

class Logger : public QObject {
    Q_OBJECT
public:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();
    void addMessage(const EventMessage &msg);
    bool isEmpty() const;
    void startNewLogFile();
    void logManualStop(uint32_t moduleId);
    void setLoggerMaxSize(int const maxSize);
    void setLoggerFlushInterval(int const flushInterval);
    int getLoggerFlushInterval() const;
    int getLoggerMaxSize();
    void applyFlushInterval();
    void applyFlushIntervalAfterAppStopped();

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
    int m_maxSize;
    int m_flushInterval;
};

#endif // LOGGER_H
