#include "mainwindow.h"
#include <QCoreApplication>
#include <QMessageBox>
#include <QRegularExpression>
#include <QDebug>

#include "logger.h"
#include "eventreceiver.h"
#include "settings.h"
#include "ui_mainwindow.h"

constexpr uint32_t s_numberOfModules = 3U;
static std::array<bool, s_numberOfModules> s_errorNotified { false, false, false };
static std::array<bool, s_numberOfModules> s_moduleStopped { false, false, false };
constexpr uint32_t s_criticalModule = 3U;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_receiver(new EventReceiver(this)),
    m_watchdogTimer(new QTimer(this)),
    m_logger(new Logger(this)),
    m_pythonProcessManager (new PythonProcessManager(this)),
    m_ipAddress{"127.0.0.1"},
    m_localPort(1024),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_customPlot = new QCustomPlot(this);
    ui->plotWidget->layout()->addWidget(m_customPlot);

    m_graphModule1 = m_customPlot->addGraph();
    m_graphModule2 = m_customPlot->addGraph();
    m_graphModule3 = m_customPlot->addGraph();

    m_graphModule1->setPen(QPen(Qt::red));
    m_graphModule2->setPen(QPen(Qt::green));
    m_graphModule3->setPen(QPen(Qt::blue));

    m_customPlot->xAxis->setLabel("Time (s)");
    m_customPlot->yAxis->setLabel("Value");

    m_customPlot->xAxis->setRange(-10, 0);
    m_customPlot->yAxis->setRange(0, 100);

    m_plotUpdateTimer = new QTimer(this);
    m_plotUpdateTimer->setInterval(100);
    connect(m_plotUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePlot);
    m_plotUpdateTimer->start();

    ui->logTextEdit->setReadOnly(true);
    ui->startModulesButton->setEnabled(true);
    ui->stopApplicationButton->setEnabled(false);
    ui->stopModule1Button->setEnabled(false);
    ui->stopModule2Button->setEnabled(false);
    ui->stopModule3Button->setEnabled(false);

    connect(m_receiver, &EventReceiver::messageReceived, this, &MainWindow::handleMessage);
    connect(m_logger, &Logger::messageReady, this, &MainWindow::displayMessage);
    connect(ui->actionSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);

    m_watchdogTimer->setInterval(10000);
    connect(m_pythonProcessManager, &PythonProcessManager::triggerOutput,
            this, &MainWindow::handleTriggerOutput);
    connect(m_pythonProcessManager, &PythonProcessManager::triggerError,
            this, &MainWindow::handleTriggerError);
    connect(m_pythonProcessManager, &PythonProcessManager::triggerCrashed,
            this, &MainWindow::handleTriggerCrashed);
    connect(m_pythonProcessManager, &PythonProcessManager::triggerFinished,
            this, &MainWindow::handleTriggerFinished);
    connect(m_watchdogTimer, &QTimer::timeout, this, &MainWindow::handleWatchdogTimeout);

}

void MainWindow::onOpenSettings()
{
    Settings dlg(this);
    setSettingDialogValues(dlg);
    if (dlg.exec() == QDialog::Accepted)
    {
        onSettingsChanged(dlg);
    }
}

void  MainWindow::setSettingDialogValues(Settings &dlg)
{
    // all data processors have the same parameters
    double const lower = m_processorModule1.getLowerThreshold();
    double const upper = m_processorModule1.getUpperThreshold();
    int const windowSize = m_processorModule1.getWindowSize();
    double const plotWindowSec = m_processorModule1.getPlotTimeWindow();

    // Logger settings
    int const ringBufferSize = m_logger->getLoggerMaxSize();
    int const flushInterval = m_logger->getLoggerFlushInterval();
    dlg.loadSettings(m_ipAddress, m_localPort,lower, upper, plotWindowSec, windowSize, flushInterval, ringBufferSize);

}

void MainWindow::appendSystemMessage(QString const & msg)
{
    QTextCursor cursor = ui->logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    format.setForeground(Qt::black);
    format.setFontWeight(QFont::Normal);

    cursor.insertText(msg, format);
    ui->logTextEdit->setTextCursor(cursor);
}

void MainWindow::handleMessage(const EventMessage &msg) {
    m_watchdogTimer->start();
    m_logger->addMessage(msg);

    if (msg.type == "CRITICAL") {
        if (msg.clientId == s_criticalModule) {
            m_receiver->close();
            m_receiver->deleteLater();
            m_receiver = nullptr;
            ui->stopModule1Button->setEnabled(false);
            ui->stopModule2Button->setEnabled(false);
            ui->stopModule3Button->setEnabled(false);
            ui->stopApplicationButton->setEnabled(false);
            // reset these for the next time the user click start
            s_moduleStopped[0] = false;
            s_moduleStopped[1] = false;
            s_moduleStopped[2] = false;
            appendSystemMessage("CRITICAL message received from module 3. Logger auto-stopped.\n");
            QTimer *reenableStartTimer = new QTimer(this);
            reenableStartTimer->setInterval(m_logger->getLoggerFlushInterval());
            connect(reenableStartTimer, &QTimer::timeout, this, [this, reenableStartTimer]() {
                if (m_logger->isEmpty()) {
                    ui->startModulesButton->setEnabled(true);
                    reenableStartTimer->stop();
                    reenableStartTimer->deleteLater();
                }
            });
            reenableStartTimer->start();
            killPythonProcess();
        }
        else{
            stopModule(msg.clientId, false);
        }
    } else if (msg.type == "ERROR") {
        switch (msg.clientId){
        case 1: {
            if (!s_moduleStopped[0]) {
                ui->stopModule1Button->setEnabled(true);
            }
            break;
        }
        case 2: {
            if (!s_moduleStopped[1]) {
                ui->stopModule2Button->setEnabled(true);
            }
            break;
        }
        case 3: {
            if (!s_moduleStopped[2]) {
                ui->stopModule3Button->setEnabled(true);
                if (!s_moduleStopped[0]) {
                    ui->stopModule1Button->setEnabled(true);
                }
                if (!s_moduleStopped[1]) {
                    ui->stopModule2Button->setEnabled(true);
                }
            }
            break;
        }
        default:
            break;
        }
        if (!s_errorNotified[msg.clientId - 1]) {
            s_errorNotified[msg.clientId - 1] = true;
            QMetaObject::invokeMethod(this, [this, msg]() {
                QMessageBox::information(this, "Error Received", QString("An error occurred for module %1. You may now stop the logger manually.\n" ).arg(msg.clientId));
            }, Qt::QueuedConnection);
        }
    }
    else if (msg.type == "DATA") {
        qDebug() << "Received DATA:" << msg.text << "from client" << msg.clientId;
        QRegularExpression regex("value:(\\d+\\.\\d+)");
        QRegularExpressionMatch match = regex.match(msg.text);
        if (match.hasMatch()) {
            double value = match.captured(1).toDouble();
            if (msg.clientId == 1) {
                m_processorModule1.addSample(value, msg.timestamp);
            } else if (msg.clientId == 2) {
                m_processorModule2.addSample(value, msg.timestamp);
            } else if (msg.clientId == 3) {
                m_processorModule3.addSample(value, msg.timestamp);
            }
        }
    }
}

void MainWindow::displayMessage(const EventMessage &msg) {
    qDebug() << "DISPLAY:" << msg.clientId << msg.type << msg.text;
    m_watchdogTimer->start();
    QTextCursor cursor = ui->logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
    if ((msg.clientId == 1 && s_moduleStopped[0]) ||
        (msg.clientId == 2 && s_moduleStopped[1]) ||
        (msg.clientId == 3 && s_moduleStopped[2]))
    {
        format.setForeground(Qt::gray);
        cursor.insertText(QString("[%1] Message from client%2 %3: %4 (%5)\n").
                          arg(msg.timestamp.toString(), QString::number(msg.clientId), msg.type, msg.text, "message was already in the logger queue, the communication has already stopped"), format);
    }
    else
    {
        if (msg.type == "INFO") {
            format.setForeground(Qt::darkGreen);
        } else if (msg.type == "WARNING") {
            format.setForeground(Qt::darkYellow);
        } else if (msg.type == "ERROR") {
            format.setForeground(Qt::red);
        } else if (msg.type == "CRITICAL") {
            format.setForeground(Qt::magenta);
            format.setFontWeight(QFont::Bold);
        }
        cursor.insertText(QString("[%1] Message from client%2 %3: %4\n").
                          arg(msg.timestamp.toString(), QString::number(msg.clientId), msg.type, msg.text), format);
    }
    ui->logTextEdit->setTextCursor(cursor);
    format.setForeground(Qt::black);
    format.setFontWeight(QFont::Normal);
    cursor.insertText(QString(""), format);
    ui->logTextEdit->setTextCursor(cursor);
}

void MainWindow::handleWatchdogTimeout() {
    appendSystemMessage("No message received in the last 10 seconds.\n");
}

void MainWindow::on_startModulesButton_clicked()
{
    if (m_receiver == nullptr) {
        m_receiver = new EventReceiver(this);
        connect(m_receiver, &EventReceiver::messageReceived, this, &MainWindow::handleMessage);
    }

    if (!m_receiver->isListening()) {
        if (m_receiver->listen(QHostAddress{m_ipAddress}, m_localPort)) {
            m_logger->startNewLogFile();
            appendSystemMessage(QString("Server started on port %1\n").arg(m_localPort));
            ui->startModulesButton->setEnabled(false);
            ui->stopApplicationButton->setEnabled(true);
            s_errorNotified = {false, false, false};
            m_watchdogTimer->start();

            QTimer::singleShot(1000, this, &MainWindow::startPythonProcess);

        } else {
            QMessageBox::critical(this, "Error", QString("Failed to start TCP server at port %1.\n").arg(m_localPort));
        }
    }
    else {
        qWarning("This should never come!");
    }
}


void MainWindow::on_stopApplicationButton_clicked()
{
    if (m_receiver == nullptr) {
        // this code should never reached!
        qDebug() << "tried to stop the application after the modules are stopped";
        ui->stopApplicationButton->setEnabled(false);
        ui->stopModule1Button->setEnabled(false);
        ui->stopModule2Button->setEnabled(false);
        ui->stopModule3Button->setEnabled(false);
        ui->startModulesButton->setEnabled(true);
        return;
    }

    if (m_receiver->isListening()) {
        m_receiver->close();
    }

    killPythonProcess();
    ui->startModulesButton->setEnabled(true);
    ui->stopModule1Button->setEnabled(false);
    ui->stopModule2Button->setEnabled(false);
    ui->stopModule3Button->setEnabled(false);
    ui->stopApplicationButton->setEnabled(false);
    s_moduleStopped = {true, true, true};

    appendSystemMessage("Application stopped by user. Flushing remaining messages...\n");
    QTimer *flushAndExitTimer = new QTimer(this);
    flushAndExitTimer->setInterval(m_logger->getLoggerFlushInterval());
    connect(flushAndExitTimer, &QTimer::timeout, this, [this, flushAndExitTimer]() {
        if (!m_logger->isEmpty()) {
            m_logger->flushBuffer();  // flush one message
        } else {
            appendSystemMessage("Application stopped.\n");
            flushAndExitTimer->stop();
            flushAndExitTimer->deleteLater();
        }
    });
    flushAndExitTimer->start();
}


void MainWindow::on_clearButton_clicked()
{
    ui->logTextEdit->clear();
}


void MainWindow::on_stopModule1Button_clicked()
{
    stopModule(1, true);
}


void MainWindow::on_stopModule2Button_clicked()
{
    stopModule(2, true);
}

void MainWindow::on_stopModule3Button_clicked()
{
    stopModule(3, true);
}

void MainWindow::onSettingsChanged(const Settings & settings)
{
    // Data processor settings
    double const lower = settings.getLowerThreshold();
    double const upper = settings.getUpperThreshold();
    int const windowSize = settings.getNumSamplesToAvg();
    double const plotWindowSec = settings.getPlotTime();

    m_processorModule1.setThresholds(lower, upper);
    m_processorModule2.setThresholds(lower, upper);
    m_processorModule3.setThresholds(lower, upper);

    m_processorModule1.setWindowSize(windowSize);
    m_processorModule2.setWindowSize(windowSize);
    m_processorModule3.setWindowSize(windowSize);

    m_processorModule1.setPlotTimeWindowSec(plotWindowSec);
    m_processorModule2.setPlotTimeWindowSec(plotWindowSec);
    m_processorModule3.setPlotTimeWindowSec(plotWindowSec);

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

void MainWindow::updatePlot()
{
    QDateTime currentTime = QDateTime::currentDateTime();

    QVector<QPointF> data1 = m_processorModule1.getProcessedCurve(currentTime);
    QVector<QPointF> data2 = m_processorModule2.getProcessedCurve(currentTime);
    QVector<QPointF> data3 = m_processorModule3.getProcessedCurve(currentTime);

    m_graphModule1->setData(QVector<double>(), QVector<double>());
    m_graphModule2->setData(QVector<double>(), QVector<double>());
    m_graphModule3->setData(QVector<double>(), QVector<double>());

    QVector<double> x1, y1, x2, y2, x3, y3;

    for (const auto &point : data1) {
        x1.append(point.x());
        y1.append(point.y());
    }
    for (const auto &point : data2) {
        x2.append(point.x());
        y2.append(point.y());
    }
    for (const auto &point : data3) {
        x3.append(point.x());
        y3.append(point.y());
    }

    m_graphModule1->setData(x1, y1);
    m_graphModule2->setData(x2, y2);
    m_graphModule3->setData(x3, y3);

    double windowSec = m_processorModule1.getPlotTimeWindowSec();
    m_customPlot->xAxis->setRange(-windowSec, 0);
    m_customPlot->yAxis->setRange(0,100);
    m_customPlot->replot();
}

void MainWindow::startPythonProcess()
{
    m_pythonProcessManager->start(m_ipAddress, m_localPort);
}

void MainWindow::killPythonProcess()
{
    m_pythonProcessManager->stop();
}

void MainWindow::handleTriggerOutput(const QString &line)
{
    qDebug() << "[Python stdout]:" << line;
}

void MainWindow::handleTriggerError(const QString &error)
{
     qWarning() << "[Python stderr]:" << error;
}

void MainWindow::handleTriggerCrashed()
{
    qCritical() << "Python process crashed!";
}

void MainWindow::handleTriggerFinished(int exitCode)
{
     qDebug() << "Python process finished with exit code:" << exitCode;
}

void MainWindow::stopModule(const int clientId, const bool logMessage)
{
    if (m_receiver->isListening()) {
        m_receiver->stopClient(clientId);
        m_logger->logManualStop(clientId);
        if(logMessage){
            appendSystemMessage(QString("Module %1 manually stopped via Stop Module %1 button.\n").arg(clientId));
        }
        else {
            appendSystemMessage(QString("CRITICAL message received from module 1. Module 1 auto-stopped.\n").arg(clientId));
        }
        switch (clientId){
        case 1:
            ui->stopModule1Button->setEnabled(false);
            break;
        case 2:
            ui->stopModule2Button->setEnabled(false);
            break;
        case 3:
            ui->stopModule3Button->setEnabled(false);
            break;
        default:
            break;
        }
        s_moduleStopped[clientId -1] = true;
        if (s_moduleStopped[0] && s_moduleStopped[1] && s_moduleStopped[2]) {
            appendSystemMessage("The other modules are already stopped so the logger is stopping...\n");
            m_logger->stop();
            ui->startModulesButton->setEnabled(true);
            ui->stopApplicationButton->setEnabled(false);
            killPythonProcess();        }
    }
}
