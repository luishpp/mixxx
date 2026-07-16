#pragma once

#include <QVector>

#include "music_sync/domain/track_features.h"

namespace mixxx::music_sync {

/// Rewrites `overallEnergy` in place to a library-relative percentile rank in
/// 0..1 (lowest-energy track -> 0, highest -> 1).
///
/// Why: the raw value is scaled by the theoretical waveform maximum (all three
/// bands saturated at once), which real music never reaches — a whole library
/// lands in a narrow ~0.15..0.35 band. That makes absolute thresholds
/// meaningless (nothing ever counts as "high energy") and makes the energy-curve
/// presets, which target 0..1, unreachable by construction.
///
/// A set's energy journey is inherently *relative* — what matters is that one
/// track is stronger than another — so ranking is the honest normalization. It
/// is deterministic (ties broken by track id) and O(n log n).
///
/// Tracks without an energy curve (never waveform-analyzed) are left untouched
/// and excluded from the ranking, so they cannot skew it.
void normalizeLibraryEnergy(QVector<TrackFeatures>* pTracks);

} // namespace mixxx::music_sync
