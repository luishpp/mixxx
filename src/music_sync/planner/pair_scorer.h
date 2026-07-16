#pragma once

#include <QString>
#include <QVector>

#include "music_sync/domain/track_features.h"

namespace mixxx::music_sync {

/// Configurable, versioned scoring weights (spec 18.2). Components are 0..1 and
/// the weights should sum to ~1.0.
struct ScoringWeights {
    double harmonic = 0.24;
    double tempo = 0.20;
    double phrase = 0.17;
    double energy = 0.17;
    double vocalSafety = 0.10;
    double transitionWindow = 0.07;
    double style = 0.05;
    QString version = QStringLiteral("pairscore-0.1.0");
};

/// Weights for a given act (spec 10.5: "priorizar harmonia" in the Portal,
/// Melodic House, Melodic Techno and the emotional finale; harmony is
/// "menos rígida" in the nostalgia flashes and the peak crossover). Act 3 and
/// unknown acts keep the spec 18.2 defaults.
///
/// The remaining weights are rescaled so the total stays 1.0, keeping scores
/// comparable across acts; the returned version records the profile, since the
/// spec requires weights to be versioned.
ScoringWeights weightsForAct(int act, const ScoringWeights& base = ScoringWeights());

/// Decomposable score of a transition from track A to track B, so a suggestion
/// can be explained from the same numbers used to rank it (spec RNF-008).
struct PairScoreBreakdown {
    double harmonic = 0.0;
    double tempo = 0.0;
    double phrase = 0.0;
    double energy = 0.0;
    double vocalSafety = 0.0;
    double transitionWindow = 0.0;
    double style = 0.0;
    double penalties = 0.0;
    double total = 0.0;
    QVector<QString> penaltyReasons;
    QString camelotMove;
    double bpmDelta = 0.0;
    double tempoChangePercent = 0.0;
};

/// Scores a candidate transition A -> B. Deterministic and side-effect free.
class PairScorer {
  public:
    static PairScoreBreakdown score(
            const TrackFeatures& from,
            const TrackFeatures& to,
            const ScoringWeights& weights,
            double maxTempoChangePercent);

    // Pure component helpers (each returns 0..1).
    static double tempoCompatibility(double bpmA, double bpmB, double maxChangePercent);
    static double energyContinuity(double fromEnergy, double toEnergy);
    static double phraseCompatibility(const TrackFeatures& from, const TrackFeatures& to);
    static double transitionWindowQuality(const TrackFeatures& from, const TrackFeatures& to);
    static double styleContinuity(const TrackFeatures& from, const TrackFeatures& to);
};

} // namespace mixxx::music_sync
