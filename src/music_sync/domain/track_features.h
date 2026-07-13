#pragma once

#include <QString>
#include <cstdint>
#include <optional>

namespace mixxx::music_sync {

/// Snapshot of a single track's native (Mixxx-provided) analysis data plus a
/// few derived fields (Camelot, analyzed flag). BPM/key/beats remain owned by
/// Mixxx; this is a cached, explainable view stored in the sidecar.
struct TrackFeatures {
    // Identity / correlation (see spec 11.3).
    std::int64_t mixxxTrackId = -1;
    QString location;
    std::int64_t fileSize = 0;

    // Metadata.
    QString title;
    QString artist;
    QString album;
    QString genre;

    // Stream info.
    std::int64_t durationMs = 0;
    int sampleRate = 0;
    int channels = 0;
    int bitrateKbps = 0;

    // Rhythm / tonality (native).
    double bpm = 0.0;
    int keyChromatic = 0; // mixxx::track::io::key::ChromaticKey as int; 0 == INVALID
    QString keyText;      // traditional key label
    QString camelot;      // Lancelot/Camelot notation via Mixxx KeyUtils

    // Loudness (native ReplayGain, linear ratio; 0 == not set).
    double replaygainRatio = 0.0;

    // Structure hints from Mixxx.
    bool hasBeatgrid = false;
    std::optional<std::int64_t> introStartMs;
    std::optional<std::int64_t> introEndMs;
    std::optional<std::int64_t> outroStartMs;
    std::optional<std::int64_t> outroEndMs;

    // Our derived analysis status (matches how Mixxx gates re-analysis).
    bool analyzed = false;

    // Provenance.
    QString analyzerVersion;
    QString snapshotAt; // set by the sidecar on write

    bool isValid() const {
        return mixxxTrackId >= 0;
    }
};

} // namespace mixxx::music_sync
