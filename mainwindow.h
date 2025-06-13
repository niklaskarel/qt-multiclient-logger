#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QProcess>
#include "eventreceiver.h"
#include "dataprocessor.h"
#include "qcustomplot.h"


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
    void handleMessage(const EventMessage &msg);
    void displayMessage(const EventMessage &msg);
    void handleWatchdogTimeout();
    void on_startModulesButton_clicked();
    void on_stopApplicationButton_clicked();
    void on_clearButton_clicked();
    void on_stopModule1Button_clicked();
    void on_stopModule2Button_clicked();
    void on_stopModule3Button_clicked();
    void onOpenSettings();
    void onSettingsChanged(const Settings& settings);
    void updatePlot();

private:
    void appendSystemMessage(QString const & msg);
    void startPythonProcess();
    void killPythonProcess();

private:
    Ui::MainWindow *ui;
    DataProcessor m_processor1;
    DataProcessor m_processor2;
    DataProcessor m_processor3;

    QCustomPlot *m_customPlot;
    QCPGraph *m_graph1;
    QCPGraph *m_graph2;
    QCPGraph *m_graph3;

    // Message Handlers
    Logger *m_logger;
    EventReceiver *m_receiver;
    QTimer *m_watchdogTimer;
    QTimer *m_plotUpdateTimer;

    QProcess *m_triggerProcess;

    QString m_ipAddress;
};
#endif // MAINWINDOW_H
