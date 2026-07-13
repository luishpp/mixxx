#pragma once

#include <QString>

#include "music_sync/domain/track_features.h"
#include "music_sync/planner/pair_scorer.h"

namespace mixxx::music_sync {

/// Turns a PairScoreBreakdown into a short human-readable explanation, derived
/// from the same numbers used to rank the transition (spec RF-008).
class ExplanationBuilder {
  public:
    static QString forPair(
            const TrackFeatures& from,
            const TrackFeatures& to,
            const PairScoreBreakdown& breakdown);
};

} // namespace mixxx::music_sync
