#include "music_sync/reporting/session_report_writer.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace mixxx::music_sync {

QString SessionReportWriter::formatClock(std::int64_t ms) {
    if (ms < 0) {
        ms = 0;
    }
    const std::int64_t total = ms / 1000;
    const std::int64_t h = total / 3600;
    const std::int64_t m = (total % 3600) / 60;
    const std::int64_t s = total % 60;
    if (h > 0) {
        return QStringLiteral("%1:%2:%3")
                .arg(h)
                .arg(m, 2, 10, QChar('0'))
                .arg(s, 2, 10, QChar('0'));
    }
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

QString SessionReportWriter::tracklistText(const SessionReport& report) {
    QString out;
    QTextStream ts(&out);

    ts << "Music Sync DJ — Tracklist\n";
    ts << (report.completed ? "Completed set" : "Interrupted set (partial)") << "  ·  "
       << "duration " << formatClock(report.durationMs) << "  ·  " << report.musicSyncVersion
       << " / Mixxx " << report.mixxxBaseline << "\n";
    if (report.startedAt.isValid()) {
        ts << "Started: " << report.startedAt.toString(Qt::ISODate) << "\n";
    }
    if (!report.recordingPath.isEmpty()) {
        ts << "Recording: " << report.recordingPath << "\n";
    }
    ts << "\n";

    // The transition that brought each track in, keyed by the track it lands on.
    QHash<int, const SessionTransition*> incoming;
    for (const SessionTransition& t : report.transitions) {
        incoming.insert(t.fromPosition + 1, &t);
    }

    for (const SessionTrack& track : report.tracks) {
        const QString label = track.title.isEmpty()
                ? track.artist
                : (track.artist.isEmpty() ? track.title
                                          : track.artist + QStringLiteral(" - ") + track.title);
        ts << QStringLiteral("%1  %2  %3")
                        .arg(track.position + 1, 2, 10, QChar('0'))
                        .arg(formatClock(track.entryMs), -8)
                        .arg(label);
        if (track.effectiveBpm > 0.0) {
            ts << QStringLiteral("  [%1 BPM]").arg(track.effectiveBpm, 0, 'f', 1);
        }
        const auto it = incoming.constFind(track.position);
        if (it != incoming.constEnd()) {
            ts << QStringLiteral("  ← %1, %2 bars%3")
                            .arg((*it)->type)
                            .arg((*it)->bars)
                            .arg((*it)->beatSync ? QStringLiteral(", beatmatched") : QString());
        }
        ts << "\n";
    }

    if (!report.warnings.isEmpty()) {
        ts << "\nWarnings:\n";
        for (const QString& w : report.warnings) {
            ts << "  ! " << w << "\n";
        }
    }
    return out;
}

QString SessionReportWriter::jsonText(const SessionReport& report) {
    QJsonObject root;
    root[QStringLiteral("musicSyncVersion")] = report.musicSyncVersion;
    root[QStringLiteral("mixxxBaseline")] = report.mixxxBaseline;
    root[QStringLiteral("recordingPath")] = report.recordingPath;
    root[QStringLiteral("startedAt")] =
            report.startedAt.isValid() ? report.startedAt.toString(Qt::ISODate) : QString();
    root[QStringLiteral("durationMs")] = static_cast<double>(report.durationMs);
    root[QStringLiteral("completed")] = report.completed;

    QJsonArray tracks;
    for (const SessionTrack& t : report.tracks) {
        QJsonObject o;
        o[QStringLiteral("position")] = t.position;
        o[QStringLiteral("artist")] = t.artist;
        o[QStringLiteral("title")] = t.title;
        o[QStringLiteral("entryMs")] = static_cast<double>(t.entryMs);
        o[QStringLiteral("effectiveBpm")] = t.effectiveBpm;
        o[QStringLiteral("fileSize")] = static_cast<double>(t.fileSize);
        tracks.append(o);
    }
    root[QStringLiteral("tracks")] = tracks;

    QJsonArray transitions;
    for (const SessionTransition& t : report.transitions) {
        QJsonObject o;
        o[QStringLiteral("fromPosition")] = t.fromPosition;
        o[QStringLiteral("atMs")] = static_cast<double>(t.atMs);
        o[QStringLiteral("type")] = t.type;
        o[QStringLiteral("bars")] = t.bars;
        o[QStringLiteral("beatSync")] = t.beatSync;
        transitions.append(o);
    }
    root[QStringLiteral("transitions")] = transitions;

    QJsonArray warnings;
    for (const QString& w : report.warnings) {
        warnings.append(w);
    }
    root[QStringLiteral("warnings")] = warnings;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace mixxx::music_sync
