#include "music_sync/analysis/worker_client.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync.worker");
} // anonymous namespace

namespace mixxx::music_sync {

WorkerClient::WorkerClient(QObject* parent) : QObject(parent) {}

WorkerClient::~WorkerClient() {
    if (m_pProcess && m_pProcess->state() != QProcess::NotRunning) {
        m_pProcess->kill();
        m_pProcess->waitForFinished(1000);
    }
}

bool WorkerClient::isAvailable(const QString& pythonExe, const QString& scriptPath) {
    if (!QFileInfo::exists(scriptPath)) {
        return false;
    }
    QProcess probe;
    probe.start(pythonExe, {QStringLiteral("--version")});
    if (!probe.waitForStarted(2000)) {
        return false;
    }
    probe.waitForFinished(2000);
    return probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0;
}

WorkerEvent WorkerClient::parseLine(const QByteArray& line) {
    WorkerEvent ev;
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return ev;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return ev;
    }
    const QJsonObject o = doc.object();
    const QString event = o.value(QStringLiteral("event")).toString();
    if (event == QLatin1String("progress")) {
        ev.type = WorkerEvent::Type::Progress;
        ev.done = o.value(QStringLiteral("done")).toInt();
        ev.total = o.value(QStringLiteral("total")).toInt();
    } else if (event == QLatin1String("track")) {
        ev.type = WorkerEvent::Type::Track;
        ev.trackId = static_cast<std::int64_t>(o.value(QStringLiteral("id")).toDouble());
        ev.overallEnergy = o.value(QStringLiteral("overallEnergy")).toDouble();
        ev.vocalDensity = o.value(QStringLiteral("vocalDensity")).toDouble();
        for (const QJsonValue& v : o.value(QStringLiteral("energyCurve")).toArray()) {
            ev.energyCurve.append(static_cast<float>(v.toDouble()));
        }
    } else if (event == QLatin1String("trackError")) {
        ev.type = WorkerEvent::Type::TrackError;
        ev.trackId = static_cast<std::int64_t>(o.value(QStringLiteral("id")).toDouble());
        ev.message = o.value(QStringLiteral("message")).toString();
    } else if (event == QLatin1String("done")) {
        ev.type = WorkerEvent::Type::Done;
        ev.total = o.value(QStringLiteral("total")).toInt();
    } else if (event == QLatin1String("fatal")) {
        ev.type = WorkerEvent::Type::Fatal;
        ev.message = o.value(QStringLiteral("message")).toString();
    }
    return ev;
}

bool WorkerClient::isRunning() const {
    return m_pProcess && m_pProcess->state() != QProcess::NotRunning;
}

void WorkerClient::analyze(const QString& pythonExe,
        const QString& scriptPath,
        const QString& requestPath) {
    if (isRunning()) {
        return; // one run at a time
    }
    m_buffer.clear();
    m_analyzed = 0;
    m_fatal.clear();

    if (!m_pProcess) {
        m_pProcess = new QProcess(this);
        connect(m_pProcess,
                &QProcess::readyReadStandardOutput,
                this,
                &WorkerClient::drainStdout);
        connect(m_pProcess,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,
                [this](int exitCode, QProcess::ExitStatus status) {
                    drainStdout(); // flush any trailing line
                    if (!m_fatal.isEmpty()) {
                        emit failed(m_fatal);
                    } else if (status != QProcess::NormalExit || exitCode != 0) {
                        emit failed(tr("The analysis worker exited with code %1.")
                                            .arg(exitCode));
                    } else {
                        emit finished(m_analyzed);
                    }
                });
        connect(m_pProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            if (m_fatal.isEmpty()) {
                m_fatal = tr("Could not start the analysis worker (is Python installed?).");
            }
        });
    }
    kLogger.info() << "Starting worker" << scriptPath << "on" << requestPath;
    m_pProcess->start(pythonExe, {scriptPath, requestPath});
}

void WorkerClient::drainStdout() {
    if (!m_pProcess) {
        return;
    }
    m_buffer += m_pProcess->readAllStandardOutput();
    int nl = -1;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        const WorkerEvent ev = parseLine(line);
        switch (ev.type) {
        case WorkerEvent::Type::Progress:
            emit progress(ev.done, ev.total);
            break;
        case WorkerEvent::Type::Track:
            ++m_analyzed;
            emit trackAnalyzed(ev.trackId, ev.overallEnergy, ev.energyCurve, ev.vocalDensity);
            break;
        case WorkerEvent::Type::TrackError:
            kLogger.warning() << "Worker could not analyze track" << ev.trackId << ev.message;
            break;
        case WorkerEvent::Type::Fatal:
            m_fatal = ev.message;
            break;
        case WorkerEvent::Type::Done:
        case WorkerEvent::Type::Unknown:
            break;
        }
    }
}

} // namespace mixxx::music_sync

#include "moc_worker_client.cpp"
