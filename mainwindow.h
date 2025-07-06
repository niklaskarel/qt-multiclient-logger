#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QProcess>
#include "eventreceiver.h"
#include "pythonprocessmanager.h"
#include "dataprocessor.h"
#include "qcustomplot.h"
#include "controller.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Logger;
class Settings;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

private slots:
    void modulesStarted();
    void handleMessage(const uint32_t clientId, const Controller::MessageType msgType);
    void displayMessage(const EventMessage &msg);
    void handleWatchdogTimeout();
    void on_startModulesButton_clicked();
    void on_stopApplicationButton_clicked();
    void on_clearButton_clicked();
    void on_stopModule1Button_clicked();
    void on_stopModule2Button_clicked();
    void on_stopModule3Button_clicked();
    void onOpenSettings();
    void updatePlot();
    void stopModule(const int clientId, const bool logMessage, const bool stopApplication);
    void appendSystemMessage(QString const & msg);
    void loggerFlushedAfterStop();

private:
    Ui::MainWindow *ui;
    std::unique_ptr<Controller> m_controller;
    // Processing incoming data from data points
    DataProcessor m_processorModule1;
    DataProcessor m_processorModule2;
    DataProcessor m_processorModule3;

    //
    QCustomPlot *m_customPlot;
    QCPGraph *m_graphModule1;
    QCPGraph *m_graphModule2;
    QCPGraph *m_graphModule3;

    // timers for the poitns ploting and logging
    QTimer *m_watchdogTimer;
    QTimer *m_plotUpdateTimer;
};
#endif // MAINWINDOW_H
