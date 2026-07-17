#pragma once

#include <QHash>
#include <QVector>
#include <cstdint>

#include "music_sync/domain/arrangement.h"
#include "music_sync/domain/mix_intent.h"
#include "music_sync/domain/set_program.h"
#include "music_sync/analysis/override_repository.h"
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

    /// `overrides` carries the DJ's per-pair choices (RF-010); pairs absent from
    /// it are planned automatically.
    static SetProgram compile(const Arrangement& arrangement,
            const QHash<std::int64_t, TrackFeatures>& byId,
            const MixIntent& intent,
            const QHash<PairKey, TransitionOverride>& overrides =
                    QHash<PairKey, TransitionOverride>(),
            const QHash<int, TransitionOverride>& actRules =
                    QHash<int, TransitionOverride>());

    /// The arrangement restricted to acts [fromAct, toAct]; 0 on either bound
    /// means "unbounded", so (0, 0) returns it untouched.
    ///
    /// Scoping happens BEFORE compiling, never by slicing a compiled program:
    /// decks alternate by position, so a slice starting at an odd position
    /// would put the opener on the wrong deck, and the transition leaving the
    /// last kept track would hand over to a track that is not there. Filtering
    /// first lets the compiler assign decks and plan handovers for the excerpt
    /// as if it were the whole set — which, for a rehearsal, it is (spec 19,
    /// Ensaio 3: practise Act 1 into Act 2, Act 2 into Act 3...).
    static Arrangement scopeToActs(const Arrangement& arrangement,
            const QHash<std::int64_t, TrackFeatures>& byId,
            int fromAct,
            int toAct);
};

} // namespace mixxx::music_sync
