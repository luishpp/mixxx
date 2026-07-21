#pragma once

#include <QString>
#include <QVector>
#include <cstdint>
#include <optional>

namespace mixxx::music_sync {

/// Transition strategies (spec 19). Beyond these are later phases.
enum class TransitionType {
    Crossfade,
    EqBlend,
    BassSwap,
    FilterTransition,
    /// Spec 16 "troca por breakdown": the answer when the key clashes, the tempo
    /// moves or the genre turns. The incoming track arrives across a low-energy
    /// section, where there is little tonal content to clash — so the swap can
    /// stay long and smooth instead of becoming a cut.
    BreakdownSwap,
    CutOnPhrase,
    AutoDjFallback,
};

QString transitionTypeName(TransitionType type);

/// The DJ's explicit choice for one pair, overriding what the planner would pick
/// (spec RF-010: "alterar duração e tipo"). An unset field means "you decide".
///
/// Keyed by the PAIR of track ids, never by position: regenerating the sequence
/// reorders positions, and an override pinned to a position would silently land
/// on a different pair. This is what DJ.Studio's transition lock protects.
struct TransitionOverride {
    std::optional<TransitionType> type;
    std::optional<int> bars;
    /// Where the outgoing track hands over, in ms. This is also how a track's
    /// play time is shortened: in a linear set each track is the source of
    /// exactly one transition, so its handover point IS that transition's exit.
    /// Spec 9 needs this — "flashes nostálgicos: faixas para 90 segundos a 3
    /// minutos" is unreachable while the exit is whatever the analysis found.
    std::optional<std::int64_t> sourceExitMs;
    /// Where the INCOMING track comes in, in ms. Automatic entry prefers a
    /// low-energy point (the intro), which on a track with a long intro drops
    /// the energy right at the handover. Setting this skips past the intro so
    /// the incoming groove lands while the outgoing one is still driving.
    std::optional<std::int64_t> targetEntryMs;
    /// Force (true) or forbid (false) beat-sync for this pair, overriding the
    /// tempo-gap rule. Set true to beatmatch across a gap the planner would
    /// otherwise refuse (e.g. pull a 140 BPM track down to a 128 body); the
    /// executor tempo-locks it and keylocks the big stretch so the pitch holds.
    std::optional<bool> forceBeatSync;

    bool isEmpty() const {
        return !type.has_value() && !bars.has_value() && !sourceExitMs.has_value() &&
                !targetEntryMs.has_value() && !forceBeatSync.has_value();
    }

    /// The fields this override actually decides, layered over `base`. Used to
    /// resolve pair-over-act: a pair says only what it disagrees about.
    TransitionOverride layeredOver(const TransitionOverride& base) const {
        TransitionOverride out = base;
        if (type) {
            out.type = type;
        }
        if (bars) {
            out.bars = bars;
        }
        if (sourceExitMs) {
            out.sourceExitMs = sourceExitMs;
        }
        if (targetEntryMs) {
            out.targetEntryMs = targetEntryMs;
        }
        if (forceBeatSync) {
            out.forceBeatSync = forceBeatSync;
        }
        return out;
    }
};

/// A control set to a value at a specific beat (instantaneous).
struct ControlAction {
    QString control; // e.g. "targetPlay", "targetLowEq"
    double atBeat = 0.0;
    double value = 0.0;
};

/// A control ramped linearly from startValue to endValue over [fromBeat, toBeat].
struct AutomationRamp {
    QString control; // e.g. "crossfader", "targetVolume"
    double fromBeat = 0.0;
    double toBeat = 0.0;
    double startValue = 0.0;
    double endValue = 0.0;
};

/// A declarative transition plan for a pair (spec 12.7 / 20). The planner
/// produces it; the executor (a later phase) consumes it. No audio engine here —
/// this is pure, serializable data.
struct TransitionPlan {
    TransitionType type = TransitionType::Crossfade;
    std::int64_t sourceTrackId = -1;
    std::int64_t targetTrackId = -1;

    // Positions in the source/target tracks (ms).
    std::int64_t sourceExitMs = 0;
    std::int64_t targetEntryMs = 0;

    // Transition length.
    int durationBars = 0;
    double durationBeats = 0.0;
    std::int64_t durationMs = 0;

    // Tempo handling (beat sync).
    double targetBpm = 0.0;
    double sourceRateRatio = 1.0;
    double targetRateRatio = 1.0;
    /// Whether the decks should be tempo-locked. Decided here, not derived from
    /// the type: only the planner knows whether the tempos actually agree. A
    /// breakdown swap over a 1.6% tempo gap wants sync; a cut never does.
    bool beatSync = true;

    // Automation, beat-relative to the start of the transition.
    QVector<ControlAction> actions;
    QVector<AutomationRamp> ramps;

    double score = 0.0;
    double confidence = 0.0;
    QString explanation;
    QVector<QString> warnings;
};

} // namespace mixxx::music_sync
