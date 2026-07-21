#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>
#include <cstdint>

namespace mixxx::music_sync {

/// One track as it actually played in a recorded set (spec RF-015). Timestamps
/// are milliseconds from the moment recording started, so they line up with the
/// WAV — not wall-clock and not the track's own position.
struct SessionTrack {
    int position = 0;
    QString artist;
    QString title;
    std::int64_t entryMs = 0;    // when this track became the live one
    double effectiveBpm = 0.0;   // the tempo it played at (after any beatmatch)
    std::int64_t fileSize = 0;   // lightweight fingerprint (spec RF-015)
};

/// One handover as it happened, timestamped against the recording.
struct SessionTransition {
    int fromPosition = 0;
    std::int64_t atMs = 0;       // when the transition began
    QString type;                // transition type name
    int bars = 0;
    bool beatSync = false;
};

/// Everything the session report and tracklist need (spec RF-015). Pure data:
/// the executor fills it as the set runs and the writer serializes it, so both
/// are testable without an engine or the filesystem.
struct SessionReport {
    QString musicSyncVersion;
    QString mixxxBaseline;
    QString recordingPath;       // the WAV the set was recorded to
    QDateTime startedAt;
    std::int64_t durationMs = 0; // total recorded length so far
    bool completed = false;      // false = interrupted; the report is still valid
    QVector<SessionTrack> tracks;
    QVector<SessionTransition> transitions;
    QVector<QString> warnings;
};

} // namespace mixxx::music_sync
