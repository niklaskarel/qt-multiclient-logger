#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QProcess>
#include "eventreceiver.h"
#include "pythonprocessmanager.h"
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
    // Python script handling slots
    void handleTriggerOutput(const QString &line);
    void handleTriggerError(const QString &error);
    void handleTriggerCrashed();
    void handleTriggerFinished(int exitCode);

private:
    void appendSystemMessage(QString const & msg);
    void startPythonProcess();
    void killPythonProcess();
    void stopModule(const int clientId, const bool logMessage);
    void setSettingDialogValues(Settings &dlg);
    void flushLoggerAfterAppStop( const QString & msg);
private:
    Ui::MainWindow *ui;
    // Processing incoming data from data points
    DataProcessor m_processorModule1;
    DataProcessor m_processorModule2;
    DataProcessor m_processorModule3;

    //
    QCustomPlot *m_customPlot;
    QCPGraph *m_graphModule1;
    QCPGraph *m_graphModule2;
    QCPGraph *m_graphModule3;

    // Message Handlers
    Logger *m_logger;
    EventReceiver *m_receiver;

    // timers for the poitns ploting and logging
    QTimer *m_watchdogTimer;
    QTimer *m_plotUpdateTimer;

    PythonProcessManager * m_pythonProcessManager;

    QString m_ipAddress;
    int m_localPort;
};
#endif // MAINWINDOW_H
