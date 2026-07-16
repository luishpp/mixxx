#include "music_sync/planner/pair_scorer.h"

#include <algorithm>
#include <cmath>

#include "music_sync/planner/harmonic_compatibility.h"

namespace mixxx::music_sync {

namespace {
// How much of the score harmony takes in each profile. The spec's default is
// 0.24; the Portal/Melodic/Final acts lean on harmony, the flash and peak acts
// deliberately let a clashing key through when the energy is right.
constexpr double kHarmonicPriority = 0.34;
constexpr double kHarmonicRelaxed = 0.14;
} // anonymous namespace

ScoringWeights weightsForAct(int act, const ScoringWeights& base) {
    double harmonic = base.harmonic;
    switch (act) {
    case 1: // Portal
    case 2: // Melodic House
    case 5: // Melodic Techno
    case 7: // Final emocional
        harmonic = kHarmonicPriority;
        break;
    case 4: // flashes nostálgicos
    case 6: // peak crossover / trance
        harmonic = kHarmonicRelaxed;
        break;
    default:
        return base; // act 3 (groove) and unknown: spec 18.2 defaults
    }

    const double othersBase = base.tempo + base.phrase + base.energy +
            base.vocalSafety + base.transitionWindow + base.style;
    if (othersBase <= 0.0) {
        return base;
    }
    // Rescale the rest so the weights still sum to 1.0 and keep their relative
    // proportions — only harmony's share moves.
    const double scale = (1.0 - harmonic) / othersBase;
    ScoringWeights w = base;
    w.harmonic = harmonic;
    w.tempo *= scale;
    w.phrase *= scale;
    w.energy *= scale;
    w.vocalSafety *= scale;
    w.transitionWindow *= scale;
    w.style *= scale;
    w.version = QStringLiteral("%1/harmony%2")
                        .arg(base.version)
                        .arg(qRound(harmonic * 100.0));
    return w;
}

double PairScorer::tempoCompatibility(double bpmA, double bpmB, double maxChangePercent) {
    if (bpmA <= 0.0 || bpmB <= 0.0) {
        return 0.3; // unknown tempo -> neutral-low
    }
    if (maxChangePercent <= 0.0) {
        maxChangePercent = 5.0;
    }
    const double pct = std::abs(bpmB - bpmA) / bpmA * 100.0;
    if (pct <= maxChangePercent) {
        // 1.0 at 0%, down to 0.7 at the tolerance edge.
        return 1.0 - 0.3 * (pct / maxChangePercent);
    }
    const double over = pct - maxChangePercent;
    return std::max(0.0, 0.7 - 0.1 * over);
}

double PairScorer::energyContinuity(double fromEnergy, double toEnergy) {
    const double jump = std::abs(toEnergy - fromEnergy);
    return std::clamp(1.0 - jump, 0.0, 1.0);
}

double PairScorer::phraseCompatibility(const TrackFeatures& from, const TrackFeatures& to) {
    const bool fromHas = !from.phrases.isEmpty();
    const bool toHas = !to.phrases.isEmpty();
    if (fromHas && toHas) {
        return 1.0;
    }
    if (!fromHas && !toHas) {
        return 0.4;
    }
    return 0.6;
}

double PairScorer::transitionWindowQuality(const TrackFeatures& from, const TrackFeatures& to) {
    const double exitQuality = from.exitWindows.isEmpty()
            ? 0.3
            : static_cast<double>(from.exitWindows.first().confidence);
    const double entryQuality = to.entryWindows.isEmpty()
            ? 0.3
            : static_cast<double>(to.entryWindows.first().confidence);
    return 0.5 * (exitQuality + entryQuality);
}

double PairScorer::styleContinuity(const TrackFeatures& from, const TrackFeatures& to) {
    if (from.genre.isEmpty() || to.genre.isEmpty()) {
        return 0.5;
    }
    return from.genre.compare(to.genre, Qt::CaseInsensitive) == 0 ? 1.0 : 0.5;
}

PairScoreBreakdown PairScorer::score(
        const TrackFeatures& from,
        const TrackFeatures& to,
        const ScoringWeights& weights,
        double maxTempoChangePercent) {
    PairScoreBreakdown b;
    b.harmonic = HarmonicCompatibility::score(from.camelot, to.camelot);
    b.tempo = tempoCompatibility(from.bpm, to.bpm, maxTempoChangePercent);
    b.phrase = phraseCompatibility(from, to);
    b.energy = energyContinuity(from.overallEnergy, to.overallEnergy);
    b.vocalSafety = 0.5; // vocal density not analyzed yet (later phase)
    b.transitionWindow = transitionWindowQuality(from, to);
    b.style = styleContinuity(from, to);
    b.camelotMove = HarmonicCompatibility::moveLabel(from.camelot, to.camelot);
    b.bpmDelta = (from.bpm > 0.0 && to.bpm > 0.0) ? (to.bpm - from.bpm) : 0.0;
    b.tempoChangePercent =
            (from.bpm > 0.0) ? std::abs(to.bpm - from.bpm) / from.bpm * 100.0 : 0.0;

    const double weighted = weights.harmonic * b.harmonic + weights.tempo * b.tempo +
            weights.phrase * b.phrase + weights.energy * b.energy +
            weights.vocalSafety * b.vocalSafety +
            weights.transitionWindow * b.transitionWindow + weights.style * b.style;

    // Pair-level penalties; sequence-level penalties live in the optimizer.
    double penalties = 0.0;
    if (from.bpm > 0.0 && to.bpm > 0.0 && b.tempoChangePercent > maxTempoChangePercent) {
        penalties += 0.15;
        b.penaltyReasons.append(QStringLiteral("tempo change above tolerance"));
    }
    if (!from.analyzed || !to.analyzed) {
        penalties += 0.20;
        b.penaltyReasons.append(QStringLiteral("track not fully analyzed"));
    }
    b.penalties = penalties;
    b.total = std::clamp(weighted - penalties, 0.0, 1.0);
    return b;
}

} // namespace mixxx::music_sync
