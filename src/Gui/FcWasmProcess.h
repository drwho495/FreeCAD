// SPDX-License-Identifier: LGPL-2.1-or-later
//
// WebAssembly build only. Qt for wasm disables QProcess (QT_CONFIG(process)
// is 0), so no QProcess class is declared. Many Gui features reference
// QProcess unconditionally (graphviz dependency-graph export, Qt Assistant
// help, the update/network retriever, run-external dialog, recovery restart,
// version migrator, …). In a browser there are no child processes, so this
// header provides an API-compatible, always-failing stand-in:
//   * a properly moc'd QObject named FcWasmProcess (so its metaobject and
//     signals are generated the normal way), and
//   * an alias  using QProcess = FcWasmProcess;
// The header is force-included across the Gui target, so every translation
// unit and generated moc file that mentions QProcess compiles. start()/
// startDetached()/execute() do nothing, the process is never Running, waits
// return immediately and reads are empty; the affected features degrade to
// "unavailable" at run time.
#pragma once

#if defined(__EMSCRIPTEN__)

#include <QIODevice>
#include <QByteArray>
#include <QString>
#include <QStringList>

// Qt for wasm still declares a useless stub `class QProcess` (deleted ctor,
// only splitCommand) in the same header as QProcessEnvironment. Rename that
// stub aside so we can keep QProcessEnvironment and bind the QProcess name to
// our working stand-in below.
#define QProcess QtDisabledQProcess
#include <QtCore/qprocess.h>
#undef QProcess

// Inherits QIODevice (as the real QProcess does) so QTextStream(process),
// write(ptr, len), canReadLine(), readAll() etc. all resolve.
class FcWasmProcess: public QIODevice
{
    Q_OBJECT
public:
    enum ProcessState { NotRunning, Starting, Running };
    Q_ENUM(ProcessState)
    enum ExitStatus { NormalExit, CrashExit };
    Q_ENUM(ExitStatus)
    enum ProcessError { FailedToStart, Crashed, Timedout, WriteError, ReadError, UnknownError };
    Q_ENUM(ProcessError)
    enum ProcessChannel { StandardOutput, StandardError };
    Q_ENUM(ProcessChannel)
    enum ProcessChannelMode { SeparateChannels, MergedChannels, ForwardedChannels };
    Q_ENUM(ProcessChannelMode)

    explicit FcWasmProcess(QObject* parent = nullptr): QIODevice(parent) {}

    // QIODevice pure virtuals: this device never has data.
    qint64 readData(char*, qint64) override { return -1; }
    qint64 writeData(const char*, qint64) override { return -1; }

    void start(const QString& = {}, const QStringList& = {}) { fail(); }
    void start(const QString&, const QStringList&, int) { fail(); }
    void setProgram(const QString&) {}
    void setArguments(const QStringList&) {}
    void setWorkingDirectory(const QString&) {}
    void setReadChannel(ProcessChannel) {}
    void setProcessChannelMode(ProcessChannelMode) {}
    void setEnvironment(const QStringList&) {}
    QStringList environment() const { return {}; }
    void setProcessEnvironment(const QProcessEnvironment&) {}

    bool waitForStarted(int = 30000) { return false; }
    bool waitForFinished(int = 30000) { return false; }
    bool waitForReadyRead(int = 30000) { return false; }
    ProcessState state() const { return NotRunning; }
    int exitCode() const { return -1; }
    ExitStatus exitStatus() const { return CrashExit; }
    ProcessError error() const { return FailedToStart; }
    qint64 processId() const { return 0; }

    // readAll(), write(const QByteArray&), write(const char*, qint64) and
    // canReadLine() come from QIODevice.
    QByteArray readAllStandardOutput() { return {}; }
    QByteArray readAllStandardError() { return {}; }
    void closeWriteChannel() {}

    void kill() {}
    void terminate() {}
    void close() {}

    static QStringList systemEnvironment() { return {}; }
    static bool startDetached(const QString& = {}, const QStringList& = {},
                              const QString& = {}, qint64* = nullptr) { return false; }
    static int execute(const QString& = {}, const QStringList& = {}) { return -1; }

Q_SIGNALS:
    void started();
    void finished(int exitCode, FcWasmProcess::ExitStatus exitStatus);
    void errorOccurred(FcWasmProcess::ProcessError error);
    void readyReadStandardOutput();
    void readyReadStandardError();
    void readyRead();

private:
    void fail()
    {
        Q_EMIT errorOccurred(FailedToStart);
        Q_EMIT finished(-1, CrashExit);
    }
};

using QProcess = FcWasmProcess;

#endif  // __EMSCRIPTEN__
