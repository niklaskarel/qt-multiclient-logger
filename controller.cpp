#include "controller.h"
#include <QMetaObject>
#include "settings.h"
#include <QDebug>
#include <QRegularExpression>

namespace {
    constexpr uint32_t s_numberOfModules = 3U;
    constexpr uint32_t s_criticalModule = 3U;
    static std::array<bool, s_numberOfModules> s_moduleStopped { false, false, false };
}


Controller::Controller(QObject *parent)
    : QObject{parent},
    m_logger{std::make_unique<Logger>(this)},
    m_receiver{std::make_unique<EventReceiver>()},
    m_receiverThread{std::make_unique<QThread>(this)},
    m_pythonProcessManager{std::make_unique<PythonProcessManager>(this)},
    m_ipAddress{"127.0.0.1"},
    m_localPort(1024)
{
    m_receiver->moveToThread(m_receiverThread.get());

    connect(m_receiver.get(), &EventReceiver::messageReceived, this, &Controller::handleMessage, Qt::QueuedConnection);
    connect(m_receiverThread.get(), &QThread::finished, m_receiver.get(), &QObject::deleteLater);
    connect(m_receiver.get(), &QObject::destroyed, this, [this]() {
        m_receiver.release();
        m_receiver = nullptr;
    });

    connect(m_logger.get(), &Logger::messageReady, this, &Controller::displayMessage);
    connect(m_pythonProcessManager.get(), &PythonProcessManager::triggerOutput,
            this, &Controller::handleTriggerOutput);
    connect(m_pythonProcessManager.get(), &PythonProcessManager::triggerError,
            this, &Controller::handleTriggerError);
    connect(m_pythonProcessManager.get(), &PythonProcessManager::triggerCrashed,
            this, &Controller::handleTriggerCrashed);
    connect(m_pythonProcessManager.get(), &PythonProcessManager::triggerFinished,
            this, &Controller::handleTriggerFinished);

     m_receiverThread->start();
}

Controller::~Controller()
{
    if (m_receiver) {
        QMetaObject::invokeMethod(m_receiver.get(), "deleteLater", Qt::BlockingQueuedConnection);
        m_receiver.reset();
    }

    if (m_receiverThread) {
        m_receiverThread->quit();
        m_receiverThread->wait();
    }
}

bool Controller::startModules()
{
    bool isReceiverNew {false};
    if (!m_receiver)
    {
        m_receiver = std::make_unique<EventReceiver>();
        isReceiverNew = true;
        connect(m_receiver.get(), &EventReceiver::messageReceived, this, &Controller::handleMessage);
        connect(m_receiver.get(), &QObject::destroyed, this, [this]() {
            m_receiver.release();
            m_receiver = nullptr;
        });
    }

    if (m_receiverThread && !m_receiverThread->isRunning())
    {
        if (isReceiverNew)
        {
            isReceiverNew = false;
            m_receiver->moveToThread(m_receiverThread.get());
            connect(m_receiverThread.get(), &QThread::finished, m_receiver.get(), &QObject::deleteLater);
        }
        m_receiverThread->start();
    }

    if (!m_receiver->isListening()) {
        if (m_receiver->listen(QHostAddress{m_ipAddress}, m_localPort)) {
            m_logger->startNewLogFile();
            emit systemMessage(QString("Server started on port %1\n").arg(m_localPort));
            emit modulesStarted();
            s_moduleStopped = {false, false, false};
            QTimer::singleShot(1000, this, &Controller::startPythonProcess);
            return true;
        }
        else {
            return false;
        }
    }
    else {
        qWarning("This should never come!");
    }
    return true;
}

bool Controller::stopApplication()
{
    if (!m_receiver.get()) {
        return false;
    }

    if (m_receiver->isListening()) {
        shutdownReceiverSoft();
    }

    s_moduleStopped = {true, true, true};
    flushLoggerAfterAppStop("Application stopped by user. Flushing remaining messages...\n");
    return true;
}

void Controller::stopModule(const int clientId, const bool logMessage)
{
    if (m_receiver->isListening()) {
        m_receiver->stopClient(clientId);
        m_logger->logManualStop(clientId);
        s_moduleStopped[clientId -1] = true;
        if (s_moduleStopped[0] && s_moduleStopped[1] && s_moduleStopped[2]) {
            shutdownReceiverSoft();
            m_logger->applyFlushIntervalAfterAppStopped();
            emit moduleStopped(clientId, logMessage, true);
            flushLoggerAfterAppStop("The other modules are already stopped so the logger is stopping...\n");
            return;
        }
        emit  moduleStopped(clientId, logMessage, false);
    }
}

void Controller::startPythonProcess()
{
    m_pythonProcessManager->start(m_ipAddress, m_localPort);
}

void Controller::killPythonProcess()
{
    m_pythonProcessManager->stop();
}

void Controller::handleTriggerOutput(const QString &line)
{
    qDebug() << "[Python stdout]:" << line;
}

void Controller::handleTriggerError(const QString &error)
{
    qWarning() << "[Python stderr]:" << error;
}

void Controller::handleTriggerCrashed()
{
    qCritical() << "Python process crashed!";
}

void Controller::handleTriggerFinished(int exitCode)
{
    qDebug() << "Python process finished with exit code:" << exitCode;
}

void Controller::handleMessage(const EventMessage & msg)
{
    m_logger->addMessage(msg);

    if (msg.type == "CRITICAL") {
        if (msg.clientId == s_criticalModule) {
            shutdownReceiverHard();
            s_moduleStopped = {true, true, true};
            flushLoggerAfterAppStop("CRITICAL message received from module 3. Logger auto-stopped.\n");
            emit newMessage(msg.clientId, MessageType::CRITICAL);
        }
        else{
            stopModule(msg.clientId, false);
        }
    } else if (msg.type == "ERROR") {
        emit newMessage(msg.clientId, MessageType::ERROR);
    } else if  (msg.type == "DATA") {
        qDebug() << "Received DATA:" << msg.text << "from client" << msg.clientId;
        QRegularExpression regex("X:(\\d+\\.\\d+),\\s*Y:(\\d+\\.\\d+)");
        QRegularExpressionMatch match = regex.match(msg.text);
        if (match.hasMatch()) {
            double valueX = match.captured(1).toDouble();
            double valueY = match.captured(2).toDouble();
            m_processors[msg.clientId - 1].addSample(valueX, valueY,  msg.timestamp);
        }
    }
}

void Controller::applySettings(const Settings & settings)
{
    double const lower = settings.getLowerThreshold();
    double const upper = settings.getUpperThreshold();
    int const windowSize = settings.getNumSamplesToAvg();
    double const plotWindowSec = settings.getPlotTime();

    m_processors[0].setThresholds(lower, upper);
    m_processors[1].setThresholds(lower, upper);
    m_processors[2].setThresholds(lower, upper);

    m_processors[0].setWindowSize(windowSize);
    m_processors[1].setWindowSize(windowSize);
    m_processors[2].setWindowSize(windowSize);

    m_processors[0].setPlotTimeWindowSec(plotWindowSec);
    m_processors[1].setPlotTimeWindowSec(plotWindowSec);
    m_processors[2].setPlotTimeWindowSec(plotWindowSec);

    // TCP connection settings
    int const localPort = settings.getTcpPort();
    QString const ipAddress = settings.getIpAddress();

    m_localPort = localPort;
    m_ipAddress = ipAddress;

    // Logger settings
    int const maxSize = settings.getRingBufferSize();
    int const flushInterval = settings.getFlushInterval();

    m_logger->setLoggerMaxSize(maxSize);
    m_logger->setLoggerFlushInterval(flushInterval);
    m_logger->applyFlushInterval();
}

void Controller::setSettingsOnDialog(Settings & settings)
{
    // all data processors have the same parameters
    double const lower =   m_processors[0].getLowerThreshold();
    double const upper =   m_processors[0].getUpperThreshold();
    int const windowSize =   m_processors[0].getWindowSize();
    double const plotWindowSec =   m_processors[0].getPlotTimeWindow();

    // Logger settings
    int const ringBufferSize = m_logger->getLoggerMaxSize();
    int const flushInterval = m_logger->getLoggerFlushInterval();
    settings.loadSettings(m_ipAddress, m_localPort,lower, upper, plotWindowSec, windowSize, flushInterval, ringBufferSize);
}

void Controller::shutdownReceiverSoft()
{
    if (m_receiver)
    {
        QMetaObject::invokeMethod(m_receiver.get(), &EventReceiver::close, Qt::BlockingQueuedConnection);
    }
}

void Controller::shutdownReceiverHard()
{
    if (m_receiver && m_receiverThread) {
        if (m_receiver) {
            QMetaObject::invokeMethod(m_receiver.get(), "deleteLater", Qt::QueuedConnection);
        }
    }

    if (m_receiverThread && m_receiverThread->isRunning()) {
        m_receiverThread->quit();
        m_receiverThread->wait();
    }
}

void Controller::flushLoggerAfterAppStop(const QString & msg)
{
    emit systemMessage(msg);
    QTimer *flushAndExitTimer = new QTimer(this);
    flushAndExitTimer->setInterval(m_logger->getLoggerFlushInterval());
    m_logger->applyFlushIntervalAfterAppStopped();
    connect(flushAndExitTimer, &QTimer::timeout, this, [this, flushAndExitTimer]() {
        if (!m_logger->isEmpty()) {
            m_logger->flushBuffer();  // flush one message
        } else {
            emit systemMessage("Application stopped.\n");
            emit loggerFlushedAfterStop();
            m_logger->applyFlushInterval();
            flushAndExitTimer->stop();
            flushAndExitTimer->deleteLater();
        }
    });
    flushAndExitTimer->start();
    killPythonProcess();
}

QVector<QPointF> Controller::getProcessedCurve2D(const uint32_t index, QDateTime currentTime)
{
    if (index > 2) {
        return {};
    }
    return m_processors[index].getProcessedCurve(currentTime);
}

QVector<QVector3D> Controller::getProcessedCurve3D(const uint32_t index, QDateTime currentTime)
{
    if (index > 2) {
        return {};
    }
    return m_processors[index].getProcessedCurve3D(currentTime);
}

int Controller::getWindowSize() const
{
    return m_processors[0].getWindowSize();
}

double Controller::getPlotTimeWindow() const
{
    return m_processors[0].getPlotTimeWindow();
}

int Controller::getLocalPort() const
{
    return m_localPort;
}

int Controller::estimateMaxOpenGLPointsPerModule() const
{
    const double plotTimeSec = getPlotTimeWindow();
    const int windowSize = getWindowSize();
    const int minIntervalMs = 50;                           // fastest possible arrival from python script (20 Hz)

    if (windowSize <= 0)
        return 100;  // fallback safety

    const double points = (plotTimeSec * 1000.0) / (minIntervalMs * windowSize);
    return std::max(10, static_cast<int>(points));          // enforce min bound
}


