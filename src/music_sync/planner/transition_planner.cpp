#include "music_sync/planner/transition_planner.h"

#include <algorithm>
#include <cmath>

#include "music_sync/planner/harmonic_compatibility.h"
#include "music_sync/planner/pair_scorer.h"

namespace mixxx::music_sync {

QString transitionTypeName(TransitionType type) {
    switch (type) {
    case TransitionType::Crossfade:
        return QStringLiteral("Crossfade");
    case TransitionType::EqBlend:
        return QStringLiteral("EQ Blend");
    case TransitionType::BassSwap:
        return QStringLiteral("Bass Swap");
    case TransitionType::FilterTransition:
        return QStringLiteral("Filter");
    case TransitionType::BreakdownSwap:
        return QStringLiteral("Breakdown swap");
    case TransitionType::CutOnPhrase:
        return QStringLiteral("Cut on phrase");
    case TransitionType::AutoDjFallback:
        return QStringLiteral("Auto DJ fallback");
    }
    return QStringLiteral("Crossfade");
}

const QString TransitionPlanner::kPlannerVersion = QStringLiteral("transition-0.2.0");

namespace {
/// A low-energy stretch (Breakdown or Outro) in the last 40% of the track: the
/// landing zone a breakdown swap needs. Without one there is nothing to hide the
/// clash behind, and a cut is the honest choice.
bool hasBreakdownNearExit(const TrackFeatures& from) {
    if (from.durationMs <= 0) {
        return false;
    }
    const double exitRegionMs = 0.60 * static_cast<double>(from.durationMs);
    for (const Section& section : from.sections) {
        if (static_cast<double>(section.endMs) < exitRegionMs) {
            continue;
        }
        if (section.type == QStringLiteral("Breakdown") ||
                section.type == QStringLiteral("Outro")) {
            return true;
        }
    }
    return false;
}
} // anonymous namespace

TransitionType TransitionPlanner::chooseType(
        const TrackFeatures& from, const TrackFeatures& to, double maxTempoChangePercent) {
    const bool bothAnalyzed = from.analyzed && to.analyzed;
    const bool hasWindows = !from.exitWindows.isEmpty() && !to.entryWindows.isEmpty();
    if (!bothAnalyzed || !hasWindows) {
        return TransitionType::AutoDjFallback;
    }
    const double keyCompat = HarmonicCompatibility::score(from.camelot, to.camelot);
    const double tempoPct =
            (from.bpm > 0.0) ? std::abs(to.bpm - from.bpm) / from.bpm * 100.0 : 100.0;
    if (tempoPct > maxTempoChangePercent * 1.6 || keyCompat < 0.4) {
        // Spec 16: a clashing key, a tempo move or a genre turn calls for a
        // "troca por breakdown", not a cut — bring the new track across a
        // low-energy stretch, where there is little tonal content to clash. Only
        // fall back to cutting when the outgoing track offers no such landing.
        return hasBreakdownNearExit(from) ? TransitionType::BreakdownSwap
                                          : TransitionType::CutOnPhrase;
    }
    const double windowConf = 0.5 *
            (static_cast<double>(from.exitWindows.first().confidence) +
                    static_cast<double>(to.entryWindows.first().confidence));
    const bool dancey = from.overallEnergy > 0.45 && to.overallEnergy > 0.45;
    if (dancey && keyCompat >= 0.5 && windowConf >= 0.5) {
        return TransitionType::BassSwap;
    }
    if (windowConf >= 0.5) {
        return TransitionType::EqBlend;
    }
    return TransitionType::Crossfade;
}

namespace {

void buildAutomation(TransitionPlan& plan) {
    const double d = plan.durationBeats;
    if (d <= 0.0) {
        return;
    }
    plan.actions.append({QStringLiteral("targetPlay"), 0.0, 1.0});

    switch (plan.type) {
    case TransitionType::EqBlend:
    case TransitionType::BassSwap: {
        plan.actions.append({QStringLiteral("targetLowEq"), 0.0, 0.0}); // kill target bass
        plan.ramps.append({QStringLiteral("targetVolume"), 0.0, d * 0.25, 0.0, 1.0});
        plan.ramps.append({QStringLiteral("crossfader"), 0.0, d, -1.0, 1.0});
        const double swapAt = (plan.type == TransitionType::BassSwap) ? d * 0.5 : d * 0.6;
        plan.actions.append({QStringLiteral("sourceLowEq"), swapAt, 0.0}); // pull source bass
        plan.actions.append({QStringLiteral("targetLowEq"), swapAt, 1.0}); // bring target bass
        plan.ramps.append({QStringLiteral("sourceVolume"), d * 0.75, d, 1.0, 0.0});
        break;
    }
    case TransitionType::BreakdownSwap: {
        // Long and gentle: the outgoing track is thinning out anyway, so pull
        // its bass early and sweep it away with the filter while the incoming
        // one grows underneath. Nothing here happens abruptly — that is the
        // whole point of choosing this over a cut.
        plan.actions.append({QStringLiteral("targetLowEq"), 0.0, 0.0});
        plan.actions.append({QStringLiteral("sourceLowEq"), d * 0.25, 0.0});
        plan.ramps.append({QStringLiteral("targetVolume"), 0.0, d * 0.6, 0.0, 1.0});
        plan.ramps.append({QStringLiteral("sourceFilter"), d * 0.25, d, 0.5, 1.0});
        plan.ramps.append({QStringLiteral("targetLowEq"), d * 0.5, d * 0.8, 0.0, 1.0});
        plan.ramps.append({QStringLiteral("crossfader"), 0.0, d, -1.0, 1.0});
        plan.ramps.append({QStringLiteral("sourceVolume"), d * 0.7, d, 1.0, 0.0});
        break;
    }
    case TransitionType::FilterTransition: {
        plan.ramps.append({QStringLiteral("targetVolume"), 0.0, d * 0.3, 0.0, 1.0});
        plan.ramps.append({QStringLiteral("sourceFilter"), 0.0, d, 0.0, 1.0}); // sweep source out
        plan.ramps.append({QStringLiteral("crossfader"), 0.0, d, -1.0, 1.0});
        break;
    }
    case TransitionType::CutOnPhrase: {
        // Hold both until the phrase boundary, then swap quickly.
        plan.ramps.append({QStringLiteral("targetVolume"), std::max(0.0, d - 1.0), d, 0.0, 1.0});
        plan.ramps.append({QStringLiteral("sourceVolume"), std::max(0.0, d - 1.0), d, 1.0, 0.0});
        plan.actions.append({QStringLiteral("crossfader"), d, 1.0});
        break;
    }
    case TransitionType::Crossfade:
    case TransitionType::AutoDjFallback:
    default: {
        plan.ramps.append({QStringLiteral("targetVolume"), 0.0, d, 0.0, 1.0});
        plan.ramps.append({QStringLiteral("sourceVolume"), 0.0, d, 1.0, 0.0});
        plan.ramps.append({QStringLiteral("crossfader"), 0.0, d, -1.0, 1.0});
        break;
    }
    }
}

} // anonymous namespace

TransitionPlan TransitionPlanner::plan(const TrackFeatures& from,
        const TrackFeatures& to,
        const MixIntent& intent,
        const TransitionOverride& override) {
    TransitionPlan plan;
    plan.sourceTrackId = from.mixxxTrackId;
    plan.targetTrackId = to.mixxxTrackId;

    const double maxTempoPct = intent.maxTempoChangePercent > 0.0
            ? intent.maxTempoChangePercent
            : tempo_tolerance::kBalanced;
    // The DJ's choice wins; otherwise the planner picks.
    plan.type = override.type ? *override.type : chooseType(from, to, maxTempoPct);
    QVector<QString> overrideWarnings;

    // The DJ's handover point wins; otherwise the best exit window, or a
    // guess a minute from the end when there is no window at all.
    plan.sourceExitMs = override.sourceExitMs
            ? *override.sourceExitMs
            : (!from.exitWindows.isEmpty()
                              ? from.exitWindows.first().startMs
                              : std::max<std::int64_t>(0, from.durationMs - 60000));
    plan.sourceExitMs = std::clamp<std::int64_t>(plan.sourceExitMs, 0, from.durationMs);
    plan.targetEntryMs = !to.entryWindows.isEmpty() ? to.entryWindows.first().startMs : 0;

    int bars;
    if (override.bars && *override.bars > 0) {
        // Asked for explicitly: only physics gets to argue, not the heuristics.
        // The window and the per-type caps are the planner's taste, and the DJ
        // has just overruled it.
        bars = *override.bars;
        const int windowBars =
                from.exitWindows.isEmpty() ? 0 : from.exitWindows.first().bars;
        if (windowBars > 0 && bars > windowBars) {
            overrideWarnings.append(
                    QStringLiteral("%1 bars runs past the %2-bar exit window")
                            .arg(bars)
                            .arg(windowBars));
        }
    } else {
        bars = intent.preferredTransitionBars > 0 ? intent.preferredTransitionBars : 32;
        if (!from.exitWindows.isEmpty() && from.exitWindows.first().bars > 0) {
            bars = std::min(bars, from.exitWindows.first().bars);
        }
        if (plan.type == TransitionType::CutOnPhrase) {
            bars = std::min(bars, 8);
        }
        if (plan.type == TransitionType::AutoDjFallback) {
            bars = std::min(bars, 16);
        }
    }

    // Physics, which nobody overrules: the transition cannot outlast the track
    // it is leaving, or the source runs out mid-handover.
    const double barMs = from.bpm > 0.0 ? 4.0 * 60000.0 / from.bpm : 2000.0;
    const int availableBars =
            static_cast<int>((from.durationMs - plan.sourceExitMs) / barMs);
    if (availableBars > 0 && bars > availableBars) {
        overrideWarnings.append(QStringLiteral("%1 bars does not fit before the track "
                                               "ends; trimmed to %2")
                                        .arg(bars)
                                        .arg(availableBars));
        bars = availableBars;
    }
    plan.durationBars = std::max(bars, 1);
    plan.durationBeats = plan.durationBars * 4.0;

    plan.targetBpm = to.bpm > 0.0 ? to.bpm : from.bpm;
    plan.sourceRateRatio =
            (from.bpm > 0.0 && plan.targetBpm > 0.0) ? plan.targetBpm / from.bpm : 1.0;
    plan.targetRateRatio =
            (to.bpm > 0.0 && plan.targetBpm > 0.0) ? plan.targetBpm / to.bpm : 1.0;

    const double beatMs = plan.targetBpm > 0.0 ? 60000.0 / plan.targetBpm : 500.0;
    plan.durationMs = static_cast<std::int64_t>(plan.durationBeats * beatMs);

    // Beat-sync is about whether the TEMPOS match, not the transition type. A
    // cut chosen for a key clash between two ~like-tempo tracks should still lock
    // and phase-align, so the brief swap lands on the beat; a cut across a real
    // tempo gap must not, since syncing would drag the incoming track. Keying
    // this off the type made every cut abrupt even when the tempos agreed
    // (Weightless 130 -> Gravity 127 came in off-beat despite a 2.3% gap).
    const double tempoPct =
            (from.bpm > 0.0) ? std::abs(to.bpm - from.bpm) / from.bpm * 100.0 : 100.0;
    plan.beatSync = tempoPct <= maxTempoPct * 1.6;

    buildAutomation(plan);

    const PairScoreBreakdown breakdown =
            PairScorer::score(from, to, ScoringWeights{}, maxTempoPct);
    plan.score = breakdown.total;
    double confidence = breakdown.total;
    if (!from.exitWindows.isEmpty() && !to.entryWindows.isEmpty()) {
        const double windowConf = 0.5 *
                (static_cast<double>(from.exitWindows.first().confidence) +
                        static_cast<double>(to.entryWindows.first().confidence));
        confidence = 0.5 * confidence + 0.5 * windowConf;
    }
    plan.confidence = confidence;
    plan.explanation = QStringLiteral("%1 over %2 bars, %3 -> %4 BPM.")
                               .arg(transitionTypeName(plan.type))
                               .arg(plan.durationBars)
                               .arg(QString::number(from.bpm, 'f', 1))
                               .arg(QString::number(plan.targetBpm, 'f', 1));
    plan.warnings = breakdown.penaltyReasons;
    plan.warnings.append(overrideWarnings); // assigned above, so append after
    if (plan.type == TransitionType::AutoDjFallback) {
        plan.warnings.append(QStringLiteral("low confidence: falling back to Auto DJ style"));
    }
    return plan;
}

} // namespace mixxx::music_sync
