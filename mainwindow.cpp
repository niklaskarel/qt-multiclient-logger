#include "mainwindow.h"
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include "logger.h"
#include "eventreceiver.h"

constexpr uint32_t s_localPort {5050U};
constexpr uint32_t s_numberOfModules = 3U;
static std::array<bool, s_numberOfModules> s_errorNotified { false, false, false };
static std::array<bool, s_numberOfModules> s_moduleStopped { false, false, false };
constexpr uint32_t s_criticalModule = 3U;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_receiver(new EventReceiver(this)), m_watchdogTimer(new QTimer(this)),
    m_logger(new Logger(this)), m_triggerProcess (new QProcess(this)) {
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout();
    t_textLog = new QTextEdit();
    t_textLog->setReadOnly(true);
    b_startModules = new QPushButton("Start Modules");
    b_startModules->setEnabled(true);
    b_stopApplication = new QPushButton("Stop Application");
    b_stopApplication->setEnabled(true);
    QHBoxLayout *stopButtonLayout = new QHBoxLayout();
    b_stopModule1 = new QPushButton("Stop Module 1");
    b_stopModule1->setEnabled(false);
    b_stopModule2 = new QPushButton("Stop Module 2");
    b_stopModule2->setEnabled(false);
    b_stopModule3 = new QPushButton("Stop Module 3");
    b_stopModule3->setEnabled(false);
    stopButtonLayout->addWidget(b_stopModule1);
    stopButtonLayout->addWidget(b_stopModule2);
    stopButtonLayout->addWidget(b_stopModule3);

    layout->addWidget(t_textLog);
    layout->addLayout(stopButtonLayout);
    layout->addWidget(b_startModules);
    layout->addWidget(b_stopApplication);
    central->setLayout(layout);
    setCentralWidget(central);

    connect(b_startModules, &QPushButton::clicked, this, &MainWindow::startModules);
    connect(b_stopModule1, &QPushButton::clicked, this, &MainWindow::stopModule1);
    connect(b_stopModule2, &QPushButton::clicked, this, &MainWindow::stopModule2);
    connect(b_stopModule3, &QPushButton::clicked, this, &MainWindow::stopModule3);
    connect(b_stopApplication, &QPushButton::clicked, this, &MainWindow::stopApplication);
    connect(m_receiver, &EventReceiver::messageReceived, this, &MainWindow::handleMessage);
    connect(m_logger, &Logger::messageReady, this, &MainWindow::displayMessage);

    m_watchdogTimer->setInterval(10000);
    connect(m_watchdogTimer, &QTimer::timeout, this, &MainWindow::handleWatchdogTimeout);

    m_triggerTimer = new QTimer(this);
    m_triggerTimer->setSingleShot(true);
    connect(m_triggerTimer, &QTimer::timeout, this, [this]() {
        QString scriptPath = QCoreApplication::applicationDirPath() + "/message_trigger.py";
        if (m_triggerProcess->state() == QProcess::NotRunning) {
            m_triggerProcess->start("python", QStringList() << scriptPath);
        }
    });

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

void MainWindow::startModules() {
    if (!m_receiver->isListening()) {
        if (m_receiver->listen(QHostAddress::LocalHost, s_localPort)) {
            m_logger->startNewLogFile();
            appendSystemMessage(QString("Server started on port %1\n").arg(s_localPort));
            b_startModules->setEnabled(false);
            s_errorNotified = {false, false, false};
            m_watchdogTimer->start();

            m_triggerTimer->start(1000);

        } else {
            QMessageBox::critical(this, "Error", QString("Failed to start TCP server at port %1.\n").arg(s_localPort));
        }
    }
    else {
        qWarning("This should never come!");
    }
}

void MainWindow::stopModule1() {
    if (m_receiver->isListening()) {
        m_receiver->stopClient(1);
        m_logger->logManualStop(1);
        appendSystemMessage("Module 1 manually stopped via Stop Module 1 button.\n");
        b_stopModule1->setEnabled(false);
        s_moduleStopped[0] = true;
        if (s_moduleStopped[1] && s_moduleStopped[2]) {
            appendSystemMessage("The other modules are already stopped so the logger is stopping...\n");
            m_logger->stop();
            b_startModules->setEnabled(true);

            if (m_triggerProcess && m_triggerProcess->state() != QProcess::NotRunning) {
                m_triggerProcess->kill();
                m_triggerProcess->waitForFinished();
            }
        }
    }
}

void MainWindow::stopModule2() {
    if (m_receiver->isListening()) {
        m_receiver->stopClient(2);
        m_logger->logManualStop(2);
        appendSystemMessage("Module 2 manually stopped via Stop Module 2 button.\n");
        b_stopModule2->setEnabled(false);
        s_moduleStopped[1] = true;
        if (s_moduleStopped[0] && s_moduleStopped[2]) {
            appendSystemMessage("The other modules are already stopped so the logger is stopping...\n");
            m_logger->stop();
            b_startModules->setEnabled(true);

            if (m_triggerProcess && m_triggerProcess->state() != QProcess::NotRunning) {
                m_triggerProcess->kill();
                m_triggerProcess->waitForFinished();
            }
        }
    }
}

void MainWindow::stopModule3() {
    if (m_receiver->isListening()) {
        m_receiver->stopClient(3);
        m_logger->logManualStop(3);
        appendSystemMessage("Module 3 manually stopped via Stop Module 3 button.\n");
        b_stopModule3->setEnabled(false);
        s_moduleStopped[2] = true;
        if (s_moduleStopped[0] && s_moduleStopped[1]) {
            appendSystemMessage("The other modules are already stopped so the logger is stopping...\n");
            m_logger->stop();
            b_startModules->setEnabled(true);

            if (m_triggerProcess && m_triggerProcess->state() != QProcess::NotRunning) {
                m_triggerProcess->kill();
                m_triggerProcess->waitForFinished();
            }
        }
    }
}

void MainWindow::appendSystemMessage(QString const & msg)
{
    QTextCursor cursor = t_textLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    format.setForeground(Qt::black);
    format.setFontWeight(QFont::Normal);

    cursor.insertText(msg, format);
    t_textLog->setTextCursor(cursor);
}

void MainWindow::stopApplication() {
    b_startModules->setEnabled(false);
    b_stopModule1->setEnabled(false);
    b_stopApplication->setEnabled(false);
    if (m_receiver->isListening()) {
        m_receiver->close();
    }
    appendSystemMessage("Logger stopped by user. Flushing remaining messages...\n");
    QTimer *flushAndExitTimer = new QTimer(this);
    flushAndExitTimer->setInterval(LOGGER_FLUSH_INTERVAL_MS);
    connect(flushAndExitTimer, &QTimer::timeout, this, [this, flushAndExitTimer]() {
        if (!m_logger->isEmpty()) {
            m_logger->flushBuffer();  // flush one message
        } else {
            appendSystemMessage("Application closing...\n");
            QTimer::singleShot(1000, this, &QWidget::close);
            flushAndExitTimer->stop();
            flushAndExitTimer->deleteLater();
        }
    });
    flushAndExitTimer->start();

    if (m_triggerProcess && m_triggerProcess->state() != QProcess::NotRunning) {
        m_triggerProcess->kill();
        m_triggerProcess->waitForFinished();
    }
}

void MainWindow::handleMessage(const EventMessage &msg) {
    m_watchdogTimer->start();
    m_logger->addMessage(msg);

    if (msg.type == "CRITICAL") {
        if (msg.clientId == s_criticalModule) {
            m_receiver->close();
            b_stopModule1->setEnabled(false);
            b_stopModule2->setEnabled(false);
            b_stopModule3->setEnabled(false);
            // reset these for the next time the user click start
            s_moduleStopped[0] = false;
            s_moduleStopped[1] = false;
            s_moduleStopped[2] = false;
            appendSystemMessage("CRITICAL message received from module 3. Logger auto-stopped.\n");
            QTimer *reenableStartTimer = new QTimer(this);
            reenableStartTimer->setInterval(LOGGER_FLUSH_INTERVAL_MS);
            connect(reenableStartTimer, &QTimer::timeout, this, [this, reenableStartTimer]() {
                if (m_logger->isEmpty()) {
                    b_startModules->setEnabled(true);
                    reenableStartTimer->stop();
                    reenableStartTimer->deleteLater();
                }
            });
            reenableStartTimer->start();
        }
        else{
            if (msg.clientId == 1){
                stopModule1();
                appendSystemMessage("CRITICAL message received from module 1. Module 1 auto-stopped.\n");
            }
            else {
                stopModule2();
                appendSystemMessage("CRITICAL message received from module 1. Module 1 auto-stopped.\n");
            }
        }
    } else if (msg.type == "ERROR") {
        switch (msg.clientId){
        case 1: {
            if (!s_moduleStopped[0]) {
                b_stopModule1->setEnabled(true);
            }
            break;
        }
        case 2: {
            if (!s_moduleStopped[1]) {
                b_stopModule2->setEnabled(true);
            }
            break;
        }
        case 3: {
            if (!s_moduleStopped[2]) {
                b_stopModule3->setEnabled(true);
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
}

void MainWindow::displayMessage(const EventMessage &msg) {
    qDebug() << "DISPLAY:" << msg.type << msg.text;
    m_watchdogTimer->start();
    QTextCursor cursor = t_textLog->textCursor();
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
    t_textLog->setTextCursor(cursor);
    format.setForeground(Qt::black);
    format.setFontWeight(QFont::Normal);
    cursor.insertText(QString(""), format);
    t_textLog->setTextCursor(cursor);
}

void MainWindow::handleWatchdogTimeout() {
    appendSystemMessage("No message received in the last 10 seconds.\n");
}
