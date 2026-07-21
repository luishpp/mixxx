#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>
#include <cstdint>

class QProcess;

namespace mixxx::music_sync {

/// One JSON-Lines event from the optional analysis worker (spec 16.2).
struct WorkerEvent {
    enum class Type { Progress, Track, TrackError, Done, Fatal, Unknown };
    Type type = Type::Unknown;
    int done = 0;
    int total = 0;
    std::int64_t trackId = -1;
    double overallEnergy = 0.0;
    QVector<float> energyCurve;
    double vocalDensity = 0.0;
    QString message;
};

/// Drives the optional Python analysis worker over QProcess (spec 16). It runs
/// entirely off the audio thread and never touches decks, recording or the
/// Mixxx database — it only asks the worker to read files and stream back richer
/// analysis. If Python or the script is missing the module falls back to its C++
/// heuristics (spec rule 15), so callers must handle failed().
class WorkerClient : public QObject {
    Q_OBJECT
  public:
    explicit WorkerClient(QObject* parent = nullptr);
    ~WorkerClient() override;

    /// Whether the worker can run: the script exists and `python` answers. Cheap
    /// enough to call before offering the action; does a one-shot `--version`.
    static bool isAvailable(const QString& pythonExe, const QString& scriptPath);

    /// Parses one JSON-Lines event. Pure and static so the protocol is testable
    /// without spawning a process. An unparseable line yields Type::Unknown.
    static WorkerEvent parseLine(const QByteArray& line);

    /// Starts the worker on `requestPath` (a JSON file the caller wrote). Streams
    /// progress()/trackAnalyzed() and ends with finished() or failed(). One run
    /// at a time; a second call while running is ignored.
    void analyze(const QString& pythonExe,
            const QString& scriptPath,
            const QString& requestPath);

    bool isRunning() const;

  signals:
    void progress(int done, int total);
    void trackAnalyzed(std::int64_t trackId,
            double overallEnergy,
            const QVector<float>& energyCurve,
            double vocalDensity);
    void finished(int analyzed);
    void failed(const QString& message);

  private:
    void drainStdout();

    QProcess* m_pProcess = nullptr;
    QByteArray m_buffer;
    int m_analyzed = 0;
    QString m_fatal;
};

} // namespace mixxx::music_sync
