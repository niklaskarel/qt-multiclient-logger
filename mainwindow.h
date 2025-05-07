#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
#include <QTextEdit>
#include <QProcess>
#include "eventreceiver.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Logger;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

private slots:
    void startModules();
    void stopModule1();
    void stopModule2();
    void stopModule3();
    void stopApplication();
    void handleMessage(const EventMessage &msg);
    void displayMessage(const EventMessage &msg);
    void handleWatchdogTimeout();

private:
    void appendSystemMessage(QString const & msg);

private:
    Logger *m_logger;
    EventReceiver *m_receiver;
    QTimer *m_watchdogTimer;
    QTimer *m_triggerTimer;
    QPushButton *b_startModules;
    QPushButton *b_stopApplication;
    QPushButton *b_stopModule1;
    QPushButton *b_stopModule2;
    QPushButton *b_stopModule3;
    QTextEdit *t_textLog;
    QProcess *m_triggerProcess;
};
#endif // MAINWINDOW_H
