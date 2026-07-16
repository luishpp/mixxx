#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

#include "music_sync/domain/transition_plan.h"

namespace mixxx::music_sync {

/// One scheduled control write, beat-relative to the start of the transition.
/// `control` is an ABSTRACT name ("targetVolume", "sourceLowEq", "crossfader",
/// "targetPlay", ...); the engine-facing executor resolves it to a concrete
/// Mixxx ConfigKey (group,key) for the actual source/target decks. Keeping the
/// program abstract makes it pure and serializable (spec 28.14) without baking
/// in deck group names.
struct ControlWrite {
    double atBeat = 0.0;
    QString control;
    double value = 0.0;
};

/// A fully expanded, deterministic preview timeline compiled from a
/// TransitionPlan (spec 20 / RF-010). The ramps are already discretized into
/// ControlWrites, so the executor only has to fire writes as playback crosses
/// each beat — no interpolation or optimization on the audio path.
struct PreviewProgram {
    /// The strategy this program came from. The executor needs it: a
    /// CutOnPhrase exists *because* the tempos are incompatible, so beat-syncing
    /// the target would defeat it (spec 16, "troca por breakdown").
    TransitionType type = TransitionType::Crossfade;

    std::int64_t sourceTrackId = -1;
    std::int64_t targetTrackId = -1;

    // Where to cue each deck before the transition (track ms).
    std::int64_t sourceStartMs = 0;
    std::int64_t targetStartMs = 0;

    // Tempo handling (beat sync to the target BPM).
    double targetBpm = 0.0;
    double sourceRateRatio = 1.0;
    double targetRateRatio = 1.0;

    // Transition length.
    double durationBeats = 0.0;
    std::int64_t durationMs = 0;

    // Sorted by atBeat (stable).
    QVector<ControlWrite> writes;

    QString explanation;

    /// Beat offset -> milliseconds at the target BPM.
    double beatToMs(double beat) const {
        return targetBpm > 0.0 ? beat * 60000.0 / targetBpm : 0.0;
    }

    /// Whether the two decks should be tempo-locked. Blends overlap both tracks,
    /// so they must share a tempo; a cut or the Auto DJ fallback hands over
    /// instead, and each track keeps its own.
    bool needsBeatSync() const {
        return type != TransitionType::CutOnPhrase &&
                type != TransitionType::AutoDjFallback;
    }
};

} // namespace mixxx::music_sync
