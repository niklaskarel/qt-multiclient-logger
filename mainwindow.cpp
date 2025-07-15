#include "mainwindow.h"
#include <QCoreApplication>
#include <QMessageBox>
#include <QRegularExpression>
#include <QDebug>

#include "settings.h"
#include "ui_mainwindow.h"

constexpr uint32_t s_numberOfModules = 3U;
static std::array<bool, s_numberOfModules> s_errorNotified { false, false, false };
static std::array<bool, s_numberOfModules> s_moduleStopped { false, false, false };
constexpr uint32_t s_criticalModule = 3U;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_controller{std::make_unique<Controller>(this)},
    m_watchdogTimer(new QTimer(this)),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Stretch priorities for the layout
    ui->plotLayout->setStretch(0, 1); // plotStackedModeWidget
    ui->plotLayout->setStretch(1, 0); // plotModeWidget

    // Safety: Reinforce size policies
    ui->plotStackedModeWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->plotModeWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->plotModeWidget->setMinimumHeight(50);

    if (!ui->page2DPlot->layout()) {
        auto layout = new QVBoxLayout(ui->page2DPlot);
        layout->setContentsMargins(0, 0, 0, 0);
        ui->page2DPlot->setLayout(layout);
    }

    m_customPlot = new QCustomPlot(ui->page2DPlot);
    ui->page2DPlot->layout()->addWidget(m_customPlot);

    if (!ui->page3DPlot->layout()) {
        auto* layout = new QVBoxLayout(ui->page3DPlot);
        layout->setContentsMargins(0, 0, 0, 0);
        ui->page3DPlot->setLayout(layout);
    }

    m_glPlot = new OpenGL3DPlot(ui->page3DPlot);
    ui->page3DPlot->layout()->addWidget(m_glPlot);

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

    m_plot2DUpdateTimer = new QTimer(this);
    m_plot2DUpdateTimer->setInterval(100);
    connect(m_plot2DUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePlot2D);
    m_plot2DUpdateTimer->start();

    m_plot3DUpdateTimer = new QTimer(this);
    m_plot3DUpdateTimer->setInterval(100);
    connect(m_plot3DUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePlot3D);
    m_plot3DUpdateTimer->start();

    ui->logTextEdit->setReadOnly(true);
    ui->startModulesButton->setEnabled(true);
    ui->stopApplicationButton->setEnabled(false);
    ui->stopModule1Button->setEnabled(false);
    ui->stopModule2Button->setEnabled(false);
    ui->stopModule3Button->setEnabled(false);

    connect(m_controller.get(), &Controller::modulesStarted, this, &MainWindow::modulesStarted);
    connect(m_controller.get(), &Controller::newMessage, this, &MainWindow::handleMessage, Qt::QueuedConnection);
    connect(m_controller.get(), &Controller::displayMessage, this, &MainWindow::displayMessage);
    connect(ui->actionSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);
    connect(m_controller.get(), &Controller::moduleStopped, this, &MainWindow::stopModule);
    connect(m_controller.get(), &Controller::systemMessage, this, &MainWindow::appendSystemMessage);
    connect(m_controller.get(), &Controller::loggerFlushedAfterStop, this, &MainWindow::loggerFlushedAfterStop);

    m_watchdogTimer->setInterval(10000);
    connect(m_watchdogTimer, &QTimer::timeout, this, &MainWindow::handleWatchdogTimeout);

    ui->rB2DPlot->setChecked(true);
    ui->plotStackedModeWidget->setCurrentIndex(0);
}

void MainWindow::onOpenSettings()
{
    Settings dlg(this);
    m_controller->setSettingsOnDialog(dlg);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_controller->applySettings(dlg);
    }
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

void MainWindow::handleMessage(const uint32_t clientId, const Controller::MessageType msgType) {
    m_watchdogTimer->start();

    if (msgType == Controller::MessageType::CRITICAL) {
        ui->stopModule1Button->setEnabled(false);
        ui->stopModule2Button->setEnabled(false);
        ui->stopModule3Button->setEnabled(false);
        ui->stopApplicationButton->setEnabled(false);
        s_moduleStopped = {true, true, true};
    } else if (msgType ==  Controller::MessageType::ERROR) {
        switch (clientId){
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
        if (!s_errorNotified[clientId - 1]) {            
            if (clientId == s_criticalModule) {
                s_errorNotified = {true, true, true};
            }
            else {
               s_errorNotified[clientId - 1] = true;     
            }
            QMetaObject::invokeMethod(this, [this, clientId]() {
                QMessageBox::information(this, "Error Received", QString("An error occurred for module %1. You may now stop the logger manually.\n" ).arg(clientId));
            }, Qt::QueuedConnection);
        }
    }
}

void MainWindow::displayMessage(const EventMessage &msg) {
    QMetaObject::invokeMethod(this, [this, msg]() {
        qDebug() << "DISPLAY:" << msg.clientId << msg.type << msg.text;
        m_watchdogTimer->start();
        QTextCursor cursor = ui->logTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);

        QTextCharFormat format;
        if ((msg.clientId == 1 && s_moduleStopped[0]) ||
            (msg.clientId == 2 && s_moduleStopped[1]) ||
            (msg.clientId == 3 && s_moduleStopped[2]))
        {
            if (msg.type != "CRITICAL") {
                format.setForeground(Qt::gray);
                cursor.insertText(QString("[%1] Message from client%2 %3: %4 (%5)\n").
                                  arg(msg.timestamp.toString(), QString::number(msg.clientId), msg.type, msg.text, "message was already in the logger queue, the communication has already stopped"), format);
            }
            else {
                // most of the time the CRITICAL message for each module from flushing the buffer
                // will come after the s_moduleStopped[X] was flagged so the coloring of the message
                // should be always the same for better visibility reasons
                format.setForeground(Qt::magenta);
                format.setFontWeight(QFont::Bold);
                cursor.insertText(QString("[%1] Message from client%2 %3: %4\n").
                                  arg(msg.timestamp.toString(), QString::number(msg.clientId), msg.type, msg.text), format);
            }

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
    }, Qt::QueuedConnection);
}

void MainWindow::handleWatchdogTimeout() {
    appendSystemMessage("No message received in the last 10 seconds.\n");
}

void MainWindow::modulesStarted()
{
    ui->startModulesButton->setEnabled(false);
    ui->stopApplicationButton->setEnabled(true);
    s_errorNotified = {false, false, false};
    s_moduleStopped = {false, false, false};
    m_watchdogTimer->start();
    ui->actionSettings->setEnabled(false);
}

void MainWindow::on_startModulesButton_clicked()
{
    // set maximum points for openGL
    if (m_glPlot){
        const int maxPoints = m_controller->estimateMaxOpenGLPointsPerModule();
        m_glPlot->setMaxPoints(maxPoints);
    }

    if (!m_controller->startModules())
    {
        QMessageBox::critical(this, "Error", QString("Failed to start TCP server at port %1.\n").arg(m_controller->getLocalPort()));
    }
}


void MainWindow::on_stopApplicationButton_clicked()
{
    if (!m_controller->stopApplication()) {
        // this code should never reached!
        qDebug() << "tried to stop the application after the modules are stopped";
        ui->stopApplicationButton->setEnabled(false);
        ui->stopModule1Button->setEnabled(false);
        ui->stopModule2Button->setEnabled(false);
        ui->stopModule3Button->setEnabled(false);
        ui->startModulesButton->setEnabled(true);
        ui->actionSettings->setEnabled(true);
        return;
    }

    ui->stopModule1Button->setEnabled(false);
    ui->stopModule2Button->setEnabled(false);
    ui->stopModule3Button->setEnabled(false);
    ui->stopApplicationButton->setEnabled(false);
    ui->startModulesButton->setEnabled(true);
    ui->actionSettings->setEnabled(true);
    s_moduleStopped = {true, true, true};
}


void MainWindow::on_clearButton_clicked()
{
    ui->logTextEdit->clear();
}


void MainWindow::on_stopModule1Button_clicked()
{
    m_controller->stopModule(1, true);
}


void MainWindow::on_stopModule2Button_clicked()
{
    m_controller->stopModule(2, true);
}

void MainWindow::on_stopModule3Button_clicked()
{
    m_controller->stopModule(3, true);
}

void MainWindow::updatePlot2D()
{
    QDateTime currentTime = QDateTime::currentDateTime();

    QVector<QPointF> data1 = m_controller->getProcessedCurve2D(0, currentTime);
    QVector<QPointF> data2 = m_controller->getProcessedCurve2D(1, currentTime);
    QVector<QPointF> data3 = m_controller->getProcessedCurve2D(2, currentTime);

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

    double windowSec = m_controller->getPlotTimeWindow();
    m_customPlot->xAxis->setRange(-windowSec, 0);
    m_customPlot->yAxis->setRange(0,100);
    m_customPlot->replot();
}

void MainWindow::updatePlot3D()
{
    QDateTime currentTime = QDateTime::currentDateTime();

    for (int moduleId = 0; moduleId < 3; ++moduleId) {
        QVector<QVector3D> qvec = m_controller->getProcessedCurve3D(moduleId, currentTime);
        std::vector<QVector3D> stdvec(qvec.begin(), qvec.end());
        m_glPlot->setPoints(moduleId, stdvec);
    }
}

void MainWindow::stopModule(const int clientId, const bool logMessage, const bool stopApplication)
{
    s_moduleStopped[clientId -1] = true;
    if(logMessage){
        appendSystemMessage(QString("Module %1 manually stopped via Stop Module %1 button.\n").arg(clientId));
    }
    else {
        appendSystemMessage(QString("CRITICAL message received from module %1. Module %1 auto-stopped.\n").arg(clientId));
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
    if (stopApplication)
    {
        ui->stopApplicationButton->setEnabled(false);
    }
}

void MainWindow::loggerFlushedAfterStop()
{
    ui->startModulesButton->setEnabled(true);
    ui->actionSettings->setEnabled(true);
}

void MainWindow::on_rB2DPlot_toggled(bool checked)
{
    if (checked) {
       ui->plotStackedModeWidget->setCurrentIndex(0);
    }
}


void MainWindow::on_rB3DPlot_toggled(bool checked)
{
    if (checked) {
        ui->plotStackedModeWidget->setCurrentIndex(1);
    }
}

