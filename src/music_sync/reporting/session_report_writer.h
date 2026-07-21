#pragma once

#include <QString>

#include "music_sync/domain/session_report.h"

namespace mixxx::music_sync {

/// Serializes a SessionReport (spec RF-015). Pure and static — no filesystem, no
/// engine — so the exact bytes are testable; the controller writes the returned
/// strings to disk.
class SessionReportWriter {
  public:
    /// A human-readable tracklist with entry timestamps (tracklist.txt).
    static QString tracklistText(const SessionReport& report);

    /// The machine-readable session report (session-report.json).
    static QString jsonText(const SessionReport& report);

    /// mm:ss or h:mm:ss, for a millisecond offset from the recording start.
    static QString formatClock(std::int64_t ms);
};

} // namespace mixxx::music_sync
