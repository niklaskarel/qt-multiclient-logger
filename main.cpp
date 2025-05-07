// event_monitor_qt - C++/Qt app that receives JSON messages over TCP and displays them with severity

// Files included:
// - main.cpp
// - mainwindow.h / mainwindow.cpp
// - eventreceiver.h / eventreceiver.cpp (TCP server logic)
// - message.h (simple struct for parsed messages)
// - resources: icons folder (optional)
// - Python script: trigger.py (sends events)

// You will still need to set up a .pro file or CMakeLists.txt, add a .ui file using Qt Designer,
// and hook up signals from GUI to logic (e.g., update labels, logs, etc.)

#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
