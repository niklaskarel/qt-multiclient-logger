#ifndef PYTHONPROCESSMANAGER_H
#define PYTHONPROCESSMANAGER_H

#include <QObject>
#include <QProcess>


class PythonProcessManager : public QObject
{
    Q_OBJECT
public:
    explicit PythonProcessManager(QObject *parent = nullptr);
    ~PythonProcessManager();

    void start(const QString & ipAddress, const int localPort);
    void stop();
    bool isRunning() const;

signals:
    void triggerOutput(const QString &line);
    void triggerError(const QString &error);
    void triggerCrashed();
    void triggerFinished(int exitCode);

private slots:
    void handleReadyReadStandardOutput();
    void handleReadyReadStandardError();
    void handleErrorOccurred(QProcess::ProcessError error);
    void handleFinished(int exitCode, QProcess::ExitStatus status);
private:
    QProcess *m_process = nullptr;
signals:
};

#endif // PYTHONPROCESSMANAGER_H
