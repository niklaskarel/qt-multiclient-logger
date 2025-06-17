#include "pythonprocessmanager.h"
#include <QCoreApplication>
#include <QDebug>

namespace {
    constexpr auto s_scriptFileName = "/message_trigger.py";
}

PythonProcessManager::PythonProcessManager(QObject *parent)
    : QObject{parent},
    m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &PythonProcessManager::handleReadyReadStandardOutput);

    connect(m_process, &QProcess::readyReadStandardError,
            this, &PythonProcessManager::handleReadyReadStandardError);

    connect(m_process, &QProcess::errorOccurred,
            this, &PythonProcessManager::handleErrorOccurred);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PythonProcessManager::handleFinished);
}


PythonProcessManager::~PythonProcessManager()
{
    stop();
}

void PythonProcessManager::start(const QString & ipAddress, const int localPort)
{
    if (isRunning())
        return;

    const QString scriptPath = QCoreApplication::applicationDirPath() + s_scriptFileName;
    m_process->setProgram("python");
    QStringList arguments;
    arguments << "--ip" << ipAddress << "--port" << QString::number(localPort);
    m_process->setArguments(QStringList{scriptPath} + arguments);
    m_process->start();
}

void PythonProcessManager::stop()
{
    if (isRunning()) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}

bool PythonProcessManager::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void PythonProcessManager::handleReadyReadStandardOutput()
{
    const QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    emit triggerOutput(output);
}

void PythonProcessManager::handleReadyReadStandardError()
{
    const QString errorOutput = QString::fromUtf8(m_process->readAllStandardError());
    emit triggerError(errorOutput);
}

void PythonProcessManager::handleErrorOccurred(QProcess::ProcessError error)
{
    if (error == QProcess::Crashed) {
        emit triggerCrashed();
    } else {
        emit triggerError(QString("Trigger process error: %1").arg(static_cast<int>(error)));
    }
}

void PythonProcessManager::handleFinished(int exitCode, QProcess::ExitStatus status)
{
    if (status == QProcess::CrashExit) {
        emit triggerCrashed();
    } else {
        emit triggerFinished(exitCode);
    }
}
