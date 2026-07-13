#pragma once

#include <QString>

#include "music_sync/domain/mix_intent.h"
#include "music_sync/domain/track_features.h"
#include "music_sync/domain/transition_plan.h"

namespace mixxx::music_sync {

/// Produces a declarative transition plan for a pair A -> B (spec 19/20). Pure
/// and deterministic; it never touches the audio engine — the executor (a later
/// phase) consumes the plan.
class TransitionPlanner {
  public:
    static const QString kPlannerVersion;

    /// Picks a transition strategy from the pair's features.
    static TransitionType chooseType(
            const TrackFeatures& from, const TrackFeatures& to, double maxTempoChangePercent);

    /// Builds the full plan (positions, tempo/rate, automation, score,
    /// explanation, warnings).
    static TransitionPlan plan(
            const TrackFeatures& from, const TrackFeatures& to, const MixIntent& intent);
};

} // namespace mixxx::music_sync
