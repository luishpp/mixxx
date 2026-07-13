#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

#include "music_sync/domain/arrangement.h"
#include "music_sync/domain/mix_intent.h"
#include "music_sync/domain/track_features.h"
#include "music_sync/planner/pair_scorer.h"

namespace mixxx::music_sync {

/// Pins a track to a fixed position in the arrangement (hybrid curation: the
/// act anchors are locked, the rest is optimized).
struct LockedPosition {
    int position = 0;
    std::int64_t mixxxTrackId = -1;
};

/// Deterministic, rule-based sequence optimizer (spec 18.1): a greedy route with
/// look-ahead improved by 2-opt, respecting locked positions, producing several
/// ranked alternatives.
class SequenceOptimizer {
  public:
    static const QString kAlgorithmVersion;

    struct Options {
        MixIntent intent;
        ScoringWeights weights;
        QVector<LockedPosition> locks;
        int numAlternatives = 3;
        int twoOptPasses = 2;
    };

    /// Returns ranked candidate arrangements (best first). May return fewer than
    /// numAlternatives when there are not enough distinct routes. Deterministic:
    /// same input -> same output.
    static QVector<Arrangement> arrange(
            const QVector<TrackFeatures>& tracks, const Options& options);
};

} // namespace mixxx::music_sync
