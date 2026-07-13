#pragma once

#include <QString>

namespace mixxx::music_sync {

/// Parsed Camelot (Lancelot) wheel code, e.g. "8A" -> {8, 'A'}.
struct CamelotCode {
    int number = 0;  // 1..12, 0 if invalid
    char letter = 0; // 'A' or 'B', 0 if invalid

    bool isValid() const {
        return number >= 1 && number <= 12 && (letter == 'A' || letter == 'B');
    }
};

/// Explicit, tested harmonic compatibility on the Camelot wheel (spec 17). Key
/// values come from Mixxx (KeyUtils Lancelot notation); incompatibility is never
/// blocking, only lower-scoring.
class HarmonicCompatibility {
  public:
    static CamelotCode parse(const QString& camelot);

    /// Compatibility 0..1 between two Camelot codes. Unknown keys score a
    /// neutral-low 0.3.
    static double score(const QString& camelotA, const QString& camelotB);

    /// A human-readable move label, e.g. "8A -> 9A".
    static QString moveLabel(const QString& camelotA, const QString& camelotB);
};

} // namespace mixxx::music_sync
