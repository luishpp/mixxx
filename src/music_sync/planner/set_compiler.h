#pragma once

#include <QHash>
#include <QVector>
#include <cstdint>

#include "music_sync/domain/arrangement.h"
#include "music_sync/domain/mix_intent.h"
#include "music_sync/domain/set_program.h"
#include "music_sync/domain/track_features.h"

namespace mixxx::music_sync {

/// Turns an approved arrangement into a SetProgram: assigns decks, plans and
/// compiles every transition, and works out where each track comes in and hands
/// over (spec RF-011 / 21.2).
///
/// Pure and deterministic — no engine, no I/O. Everything the executor needs is
/// computed here, so the executor never plans while audio is playing (spec
/// 21.3 / RNF-002).
class SetCompiler {
  public:
    /// How many decks the set rotates between. Two is what the plan and the
    /// preview assume; a third would need the executor to track more state.
    static constexpr int kDeckCount = 2;

    static SetProgram compile(const Arrangement& arrangement,
            const QHash<std::int64_t, TrackFeatures>& byId,
            const MixIntent& intent);
};

} // namespace mixxx::music_sync
