#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <memory>

#include "eventreceiver.h"
#include "dataprocessor.h"
#include "logger.h"
#include "pythonprocessmanager.h"
#include "eventmessage.h"

class Settings;

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(QObject *parent = nullptr);
    ~Controller();

    bool startModules();
    bool stopApplication();
    void stopModule(const int clientId,  const bool logMessage);
    void applySettings(double lower, double upper, int avgWindow, double plotWindowSec, int flushInterval, int bufferSize);
    void startPythonProcess();
    void killPythonProcess();
    void applySettings(const Settings & settings);
    void setSettingsOnDialog(Settings & settings);

    //Data Processor setters and getters
    QVector<QPointF> getProcessedCurve2D(const uint32_t index,QDateTime currentTime);
    QVector<QVector3D> getProcessedCurve3D(const uint32_t index,QDateTime currentTime);
    double getPlotTimeWindow() const;
    int getWindowSize() const;

    int estimateMaxOpenGLPointsPerModule() const;

    //Event Receiver setters and getters
    int getLocalPort() const;

    enum class MessageType
    {
        INFO = 1,
        ERROR = 2,
        CRITICAL = 3
    };

    Q_ENUM(MessageType);

signals:
    void modulesStarted();
    void newMessage(uint32_t clientId, MessageType msgType);
    void logOutput(const EventMessage &msg);
    void moduleStopped(const int clientId, const bool logMessage, const bool stopApplication);
    void displayMessage(const EventMessage &msg);
    void systemMessage(const QString & msg);
    void loggerFlushedAfterStop();

private slots:
    void handleMessage(const EventMessage &msg);    
    // Python script handling slots
    void handleTriggerOutput(const QString &line);
    void handleTriggerError(const QString &error);
    void handleTriggerCrashed();
    void handleTriggerFinished(int exitCode);

private:
    void shutdownReceiverSoft();
    void shutdownReceiverHard();
    void flushLoggerAfterAppStop(const QString & msg);


private:
    std::unique_ptr<EventReceiver> m_receiver;
    std::unique_ptr<QThread> m_receiverThread;
    std::unique_ptr<Logger> m_logger;
    std::unique_ptr<PythonProcessManager> m_pythonProcessManager;

    std::array<DataProcessor, 3> m_processors;
    QString m_ipAddress;
    int m_localPort = 0;

signals:
};

#endif // CONTROLLER_H
