#include "logger.h"
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>

Logger::Logger(QObject *parent)
    : QObject(parent),
    m_maxSize(100),
    m_flushInterval(200)
{
    m_flushTimer.setInterval(m_flushInterval);
    connect(&m_flushTimer, &QTimer::timeout, this, &Logger::flushBuffer);
    m_flushTimer.start();
}

Logger::~Logger() {
    stop();
}

void Logger::addMessage(const EventMessage &msg) {
    qDebug() << "Added message to buffer:" << msg.clientId << msg.type << msg.text;
    if (m_buffer.size() >= m_maxSize){
        auto it = std::find_if(m_buffer.begin(), m_buffer.end(), [](const EventMessage &m) {
            return m.type == "INFO" || m.type == "WARNING";
        });
        if (it != m_buffer.end()) {
            m_buffer.erase(it);  // Drop first INFO/WARNING message, as they are lower prio
        } else if (msg.type == "CRITICAL"){
            // If the message is CRITICAL drop an ERROR, if it exists
            it = std::find_if(m_buffer.begin(), m_buffer.end(), [](const EventMessage &m) {
                return m.type == "ERROR";
            });
            if (it != m_buffer.end())
                m_buffer.erase(it);
        } else {
            // If no INFO/WARNING exists, and the message is ERROR dont drop anything
            return;
        }
    }
    m_buffer.push_back(msg);
}

void Logger::flushBuffer() {
    if (!m_buffer.empty()) {
        EventMessage const msg = m_buffer.front();
        emit messageReady(msg);
        m_buffer.pop_front();
        if (m_logWriter) {
            m_logWriter->enqueue(msg);
        }
    }
}

void Logger::clear() {
    m_buffer.clear();
}

bool Logger::isEmpty() const
{
    return m_buffer.empty();
}

void Logger::startNewLogFile() {
    QDir const logDir(QCoreApplication::applicationDirPath() + "/logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    QString const timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    m_logFilePath = logDir.filePath("logger_" + timestamp + ".txt");

    if (m_logWriter) {
        m_logWriter->finish();
        m_logWriter->wait();
        m_logWriter.reset();
    }
    m_logWriter = std::make_unique<Writer>(this);
    m_logWriter->setLogFilePath(m_logFilePath);
    m_logWriter->start();
}

void Logger::logManualStop(uint32_t const moduleId) {
    EventMessage msg;
    msg.clientId = moduleId;
    msg.timestamp = QDateTime::currentDateTime();
    msg.type = "INFO";
    msg.text = QString("Module %1 manually stopped by user.").arg(moduleId);
    if (m_logWriter) {
        m_logWriter->enqueue(msg);
    }
}

void Logger::stop()
{
    if (m_logWriter) {
        m_logWriter->finish();
        m_logWriter->wait();
        m_logWriter.reset();
    }
    clear();
}

void Logger::applyFlushInterval()
{
    m_flushTimer.stop();
    m_flushTimer.setInterval(m_flushInterval);
    m_flushTimer.start();
}
