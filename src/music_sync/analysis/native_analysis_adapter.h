#pragma once

#include <QString>

#include "music_sync/domain/track_features.h"
#include "track/track_decl.h"

namespace mixxx::music_sync {

/// Reads a track's NATIVE analysis data (the values Mixxx already computed:
/// BPM, key, beatgrid, ReplayGain, intro/outro cues, stream info) into a
/// TrackFeatures snapshot. This never triggers analysis or touches the audio
/// thread; it only reads already-available data off a loaded Track.
class NativeAnalysisAdapter {
  public:
    /// Version stamp for the native snapshot format.
    static const QString kAnalyzerVersion;

    /// Extracts native features from a loaded track. Returns an invalid
    /// TrackFeatures (mixxxTrackId < 0) when pTrack is null.
    static TrackFeatures extract(const TrackPointer& pTrack);

    /// Whether Mixxx still needs to analyze this track, mirroring Mixxx's own
    /// gate (no beatgrid / invalid BPM, or no global key).
    static bool needsAnalysis(const TrackPointer& pTrack);
};

} // namespace mixxx::music_sync
