#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

namespace mixxx::music_sync {

/// Transition strategies (spec 19). Beyond these are later phases.
enum class TransitionType {
    Crossfade,
    EqBlend,
    BassSwap,
    FilterTransition,
    CutOnPhrase,
    AutoDjFallback,
};

QString transitionTypeName(TransitionType type);

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

    // Automation, beat-relative to the start of the transition.
    QVector<ControlAction> actions;
    QVector<AutomationRamp> ramps;

    double score = 0.0;
    double confidence = 0.0;
    QString explanation;
    QVector<QString> warnings;
};

} // namespace mixxx::music_sync
