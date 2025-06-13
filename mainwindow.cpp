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
    m_triggerProcess (new QProcess(this)),
    m_ipAddress{"127.0.0.1"},
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_customPlot = new QCustomPlot(this);
    ui->plotWidget->layout()->addWidget(m_customPlot);

    m_graph1 = m_customPlot->addGraph();
    m_graph2 = m_customPlot->addGraph();
    m_graph3 = m_customPlot->addGraph();

    m_graph1->setPen(QPen(Qt::red));
    m_graph2->setPen(QPen(Qt::green));
    m_graph3->setPen(QPen(Qt::blue));

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
    ui->stopApplicationButton->setEnabled(true);
    ui->stopModule1Button->setEnabled(false);
    ui->stopModule2Button->setEnabled(false);
    ui->stopModule3Button->setEnabled(false);

    connect(m_receiver, &EventReceiver::messageReceived, this, &MainWindow::handleMessage);
    connect(m_logger, &Logger::messageReady, this, &MainWindow::displayMessage);
    connect(ui->actionSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);

    m_watchdogTimer->setInterval(10000);
    connect(m_watchdogTimer, &QTimer::timeout, this, &MainWindow::handleWatchdogTimeout);

    connect(m_triggerProcess, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        qDebug() << "Trigger process error:" << error << m_triggerProcess->errorString();
    });

    connect(m_triggerProcess, &QProcess::readyReadStandardError, this, [=]() {
        qDebug() << "Trigger script stderr:" << m_triggerProcess->readAllStandardError();
    });

    connect(m_triggerProcess, &QProcess::readyReadStandardOutput, this, [=]() {
        qDebug() << "Trigger script stdout:" << m_triggerProcess->readAllStandardOutput();
    });
}

void MainWindow::onOpenSettings()
{
    Settings dlg(this);
    dlg.exec();
    onSettingsChanged(dlg);
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
            delete m_receiver;
            m_receiver = nullptr;
            ui->stopModule1Button->setEnabled(false);
            ui->stopModule2Button->setEnabled(false);
            ui->stopModule3Button->setEnabled(false);
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
            if (msg.clientId == 1){
                on_stopModule1Button_clicked();
                appendSystemMessage("CRITICAL message received from module 1. Module 1 auto-stopped.\n");
            }
            else {
                on_stopModule2Button_clicked();
                appendSystemMessage("CRITICAL message received from module 2. Module 2 auto-stopped.\n");
            }
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
                m_processor1.addSample(value, msg.timestamp);
            } else if (msg.clientId == 2) {
                m_processor2.addSample(value, msg.timestamp);
            } else if (msg.clientId == 3) {
                m_processor3.addSample(value, msg.timestamp);
            }
        }
    }
}

void MainWindow::displayMessage(const EventMessage &msg) {
    qDebug() << "DISPLAY:" << msg.type << msg.text;
    m_watchdogTimer->start();
    QTextCursor cursor = ui->logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
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

    cursor.insertText(QString("[%1] Message from client%2 %3: %4\n").arg(msg.timestamp.toString(), QString::number(msg.clientId), msg.type, msg.text), format);
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
        int const localPort = m_receiver->getLocalPort();
        if (m_receiver->listen(QHostAddress{m_ipAddress}, localPort)) {
            m_logger->startNewLogFile();
            appendSystemMessage(QString("Server started on port %1\n").arg(localPort));
            ui->startModulesButton->setEnabled(false);
            s_errorNotified = {false, false, false};
            m_watchdogTimer->start();

            QTimer::singleShot(1000, this, &MainWindow::startPythonProcess);

        } else {
            QMessageBox::critical(this, "Error", QString("Failed to start TCP server at port %1.\n").arg(localPort));
        }
    }
    else {
        qWarning("This should never come!");
    }
}


void MainWindow::on_stopApplicationButton_clicked()
{
    if (m_receiver->isListening()) {
        m_receiver->close();
    }

    killPythonProcess();
    ui->startModulesButton->setEnabled(true);
    ui->stopModule1Button->setEnabled(false);
    ui->stopModule2Button->setEnabled(false);
    ui->stopModule3Button->setEnabled(false);
    ui->stopApplicationButton->setEnabled(false);

    appendSystemMessage("Application stopped by user. Flushing remaining messages...\n");
    QTimer *flushAndExitTimer = new QTimer(this);
    flushAndExitTimer->setInterval(m_logger->getLoggerFlushInterval());
    connect(flushAndExitTimer, &QTimer::timeout, this, [this, flushAndExitTimer]() {
        if (!m_logger->isEmpty()) {
            m_logger->flushBuffer();  // flush one message
        } else {
            appendSystemMessage("Application stopped.\n");
            QTimer::singleShot(1000, this, &QWidget::close);
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
    if (m_receiver->isListening()) {
        m_receiver->stopClient(1);
        m_logger->logManualStop(1);
        appendSystemMessage("Module 1 manually stopped via Stop Module 1 button.\n");
        ui->stopModule1Button->setEnabled(false);
        s_moduleStopped[0] = true;
        if (s_moduleStopped[1] && s_moduleStopped[2]) {
            appendSystemMessage("The other modules are already stopped so the logger is stopping...\n");
            m_logger->stop();
            ui->startModulesButton->setEnabled(true);

            killPythonProcess();
        }
    }
}


void MainWindow::on_stopModule2Button_clicked()
{
    if (m_receiver->isListening()) {
        m_receiver->stopClient(2);
        m_logger->logManualStop(2);
        appendSystemMessage("Module 2 manually stopped via Stop Module 2 button.\n");
        ui->stopModule2Button->setEnabled(false);
        s_moduleStopped[1] = true;
        if (s_moduleStopped[0] && s_moduleStopped[2]) {
            appendSystemMessage("The other modules are already stopped so the logger is stopping...\n");
            m_logger->stop();
            ui->startModulesButton->setEnabled(true);

            killPythonProcess();
        }
    }
}

void MainWindow::on_stopModule3Button_clicked()
{
    if (m_receiver->isListening()) {
        m_receiver->stopClient(3);
        m_logger->logManualStop(3);
        appendSystemMessage("Module 3 manually stopped via Stop Module 3 button.\n");
        ui->stopModule3Button->setEnabled(false);
        s_moduleStopped[2] = true;
        if (s_moduleStopped[0] && s_moduleStopped[1]) {
            appendSystemMessage("The other modules are already stopped so the logger is stopping...\n");
            m_logger->stop();
            ui->startModulesButton->setEnabled(true);

            killPythonProcess();        }
    }
}

void MainWindow::onSettingsChanged(const Settings & settings)
{
    // Data processor settings
    double const lower = settings.getLowerThreshold();
    double const upper = settings.getUpperThreshold();
    int const windowSize = settings.getNumSamplesToAvg();
    double const plotWindowSec = settings.getPlotTime();

    m_processor1.setThresholds(lower, upper);
    m_processor2.setThresholds(lower, upper);
    m_processor3.setThresholds(lower, upper);

    m_processor1.setWindowSize(windowSize);
    m_processor2.setWindowSize(windowSize);
    m_processor3.setWindowSize(windowSize);

    m_processor1.setPlotTimeWindowSec(plotWindowSec);
    m_processor2.setPlotTimeWindowSec(plotWindowSec);
    m_processor3.setPlotTimeWindowSec(plotWindowSec);

    // TCP connection settings
    int const localPort = settings.getTcpPort();
    QString const ipAddress = settings.getIpAddress();

    m_receiver->setLocalPort(localPort);
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

    QVector<QPointF> data1 = m_processor1.getProcessedCurve(currentTime);
    QVector<QPointF> data2 = m_processor2.getProcessedCurve(currentTime);
    QVector<QPointF> data3 = m_processor3.getProcessedCurve(currentTime);

    m_graph1->setData(QVector<double>(), QVector<double>());
    m_graph2->setData(QVector<double>(), QVector<double>());
    m_graph3->setData(QVector<double>(), QVector<double>());

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

    m_graph1->setData(x1, y1);
    m_graph2->setData(x2, y2);
    m_graph3->setData(x3, y3);

    double windowSec = m_processor1.getPlotTimeWindowSec();
    m_customPlot->xAxis->setRange(-windowSec, 0);
    m_customPlot->yAxis->setRange(0,100);
    m_customPlot->replot();
}

void MainWindow::startPythonProcess()
{
    QString portStr = QString::number(m_receiver->getLocalPort());

    QStringList arguments;
    arguments << "--ip" << m_ipAddress << "--port" << portStr;

    QString scriptPath = QCoreApplication::applicationDirPath() + "/message_trigger.py";
    if (m_triggerProcess->state() == QProcess::NotRunning) {
        m_triggerProcess->start("python", QStringList() << scriptPath << arguments);
    }
}

void MainWindow::killPythonProcess()
{
    if (m_triggerProcess && m_triggerProcess->state() != QProcess::NotRunning) {
        m_triggerProcess->kill();
        m_triggerProcess->waitForFinished();
    }
}
