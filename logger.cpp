#include "logger.h"
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>

Logger::Logger(QObject *parent)
    : QObject(parent),
    m_maxSize(500),
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
            return m.type == "DATA";
        });
        if (it != m_buffer.end()) {
            m_buffer.erase(it);  // Drop first DATA message, as they are lower prio
        } else if (msg.type == "WARNING") {
            auto it = std::find_if(m_buffer.begin(), m_buffer.end(), [](const EventMessage &m) {
                return m.type == "INFO" || m.type == "INFO";
            });
            if (it != m_buffer.end())
                m_buffer.erase(it); // Drop first INFO message if there is a warning
        } else if (msg.type == "ERROR") {
                auto it = std::find_if(m_buffer.begin(), m_buffer.end(), [](const EventMessage &m) {
                    return m.type == "INFO" || m.type == "WARNING";
                });
                if (it != m_buffer.end())
                    m_buffer.erase(it); // Drop first INFO or WARNING message if there is a error
        } else if (msg.type == "CRITICAL"){
            // If the message is CRITICAL drop an ERROR, if it exists
            it = std::find_if(m_buffer.begin(), m_buffer.end(), [](const EventMessage &m) {
                return m.type == "INFO" || m.type == "WARNING" || m.type == "ERROR";
            });
            if (it != m_buffer.end()){
                m_buffer.erase(it); // Drop any message that is not critical to display critical
            } else {
            // If no other DATA/INFO/WARNING/ERROR message exists, and the message is CRITICAL dont drop anything
            // but also dont add it to the queue because its full
                return;
            }
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
    qDebug() << "Creating new log file:" << m_logFilePath;
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

void Logger::setLoggerMaxSize(int const maxSize)
{
    m_maxSize = maxSize;
}

void Logger::setLoggerFlushInterval(int const flushInterval)
{
    m_flushInterval = flushInterval;
}

int Logger::getLoggerFlushInterval() const
{
    return m_flushInterval;
}

int Logger::getLoggerMaxSize()
{
    return m_maxSize;
}
