#include <gtest/gtest.h>

#include <QString>

#include "music_sync/domain/mix_intent.h"
#include "music_sync/domain/track_features.h"
#include "music_sync/planner/energy_curve.h"
#include "music_sync/planner/explanation_builder.h"
#include "music_sync/planner/harmonic_compatibility.h"
#include "music_sync/planner/pair_scorer.h"

namespace {

using namespace mixxx::music_sync;

TEST(MusicSyncHarmonicTest, CamelotParse) {
    EXPECT_TRUE(HarmonicCompatibility::parse(QStringLiteral("8A")).isValid());
    EXPECT_EQ(HarmonicCompatibility::parse(QStringLiteral("8A")).number, 8);
    EXPECT_EQ(HarmonicCompatibility::parse(QStringLiteral("8A")).letter, 'A');
    EXPECT_EQ(HarmonicCompatibility::parse(QStringLiteral("12B")).number, 12);
    EXPECT_FALSE(HarmonicCompatibility::parse(QString()).isValid());
    EXPECT_FALSE(HarmonicCompatibility::parse(QStringLiteral("13A")).isValid());
    EXPECT_FALSE(HarmonicCompatibility::parse(QStringLiteral("8C")).isValid());
    EXPECT_FALSE(HarmonicCompatibility::parse(QStringLiteral("A")).isValid());
}

TEST(MusicSyncHarmonicTest, CompatibilityRules) {
    EXPECT_DOUBLE_EQ(HarmonicCompatibility::score(QStringLiteral("8A"), QStringLiteral("8A")), 1.0);
    EXPECT_DOUBLE_EQ(HarmonicCompatibility::score(QStringLiteral("8A"), QStringLiteral("9A")), 0.9);
    EXPECT_DOUBLE_EQ(HarmonicCompatibility::score(QStringLiteral("8A"), QStringLiteral("7A")), 0.9);
    EXPECT_DOUBLE_EQ(HarmonicCompatibility::score(QStringLiteral("12A"), QStringLiteral("1A")), 0.9);
    EXPECT_DOUBLE_EQ(HarmonicCompatibility::score(QStringLiteral("8A"), QStringLiteral("8B")), 0.75);
    EXPECT_DOUBLE_EQ(HarmonicCompatibility::score(QStringLiteral("8A"), QStringLiteral("10A")), 0.5);
    EXPECT_DOUBLE_EQ(HarmonicCompatibility::score(QStringLiteral("8A"), QStringLiteral("3A")), 0.2);
    EXPECT_DOUBLE_EQ(HarmonicCompatibility::score(QStringLiteral("8A"), QStringLiteral("xx")), 0.3);
}

TEST(MusicSyncTempoTest, TempoCompatibility) {
    EXPECT_DOUBLE_EQ(PairScorer::tempoCompatibility(124.0, 124.0, 5.0), 1.0);
    EXPECT_GT(PairScorer::tempoCompatibility(124.0, 125.0, 5.0), 0.8);
    EXPECT_LT(PairScorer::tempoCompatibility(124.0, 140.0, 5.0), 0.5);
    EXPECT_DOUBLE_EQ(PairScorer::tempoCompatibility(0.0, 124.0, 5.0), 0.3);
    EXPECT_NEAR(PairScorer::tempoCompatibility(100.0, 105.0, 5.0), 0.7, 1e-9);
}

TrackFeatures makeTrack(
        std::int64_t id, double bpm, const QString& camelot, double energy, bool analyzed = true) {
    TrackFeatures f;
    f.mixxxTrackId = id;
    f.bpm = bpm;
    f.camelot = camelot;
    f.overallEnergy = energy;
    f.analyzed = analyzed;
    f.phrases.append(PhraseMarker{0, 16});
    return f;
}

TEST(MusicSyncPairScoreTest, StrongBeatsWeak) {
    const ScoringWeights weights;
    const TrackFeatures a = makeTrack(1, 124.0, QStringLiteral("8A"), 0.5);
    const TrackFeatures strong = makeTrack(2, 124.5, QStringLiteral("9A"), 0.55);
    const TrackFeatures weak = makeTrack(3, 145.0, QStringLiteral("3B"), 0.9);

    const PairScoreBreakdown strongScore = PairScorer::score(a, strong, weights, 5.0);
    const PairScoreBreakdown weakScore = PairScorer::score(a, weak, weights, 5.0);
    EXPECT_GT(strongScore.total, weakScore.total);
    EXPECT_GT(strongScore.total, 0.7);
    EXPECT_LT(weakScore.total, 0.6);
    EXPECT_FALSE(weakScore.penaltyReasons.isEmpty());
    EXPECT_GT(weakScore.tempoChangePercent, 5.0);
}

TEST(MusicSyncPairScoreTest, UnanalyzedPenalty) {
    const ScoringWeights weights;
    const TrackFeatures a = makeTrack(1, 124.0, QStringLiteral("8A"), 0.5, true);
    const TrackFeatures b = makeTrack(2, 124.0, QStringLiteral("8A"), 0.5, false);
    EXPECT_GT(PairScorer::score(a, b, weights, 5.0).penalties, 0.0);
}

TEST(MusicSyncEnergyCurveTest, PresetsAndSampling) {
    const QVector<EnergyPoint> late = EnergyCurve::forPreset(EnergyPreset::LatePeak);
    ASSERT_FALSE(late.isEmpty());
    EXPECT_NEAR(EnergyCurve::energyAt(late, 0.90), 1.0, 1e-6);
    EXPECT_LT(EnergyCurve::energyAt(late, 0.0), EnergyCurve::energyAt(late, 0.9));

    const QVector<EnergyPoint> flat = EnergyCurve::forPreset(EnergyPreset::Constant);
    EXPECT_NEAR(EnergyCurve::energyAt(flat, 0.2), EnergyCurve::energyAt(flat, 0.8), 1e-9);

    const QVector<EnergyPoint> up = EnergyCurve::forPreset(EnergyPreset::Ascending);
    EXPECT_GT(EnergyCurve::energyAt(up, 1.0), EnergyCurve::energyAt(up, 0.0));
}

TEST(MusicSyncExplanationTest, MentionsKeyFacts) {
    const ScoringWeights weights;
    const TrackFeatures a = makeTrack(1, 124.0, QStringLiteral("8A"), 0.5);
    const TrackFeatures b = makeTrack(2, 125.8, QStringLiteral("9A"), 0.6);
    const PairScoreBreakdown score = PairScorer::score(a, b, weights, 5.0);
    const QString text = ExplanationBuilder::forPair(a, b, score);
    EXPECT_TRUE(text.contains(QStringLiteral("8A -> 9A")));
    EXPECT_TRUE(text.contains(QStringLiteral("BPM")));
    EXPECT_TRUE(text.contains(QStringLiteral("energy")));
}

} // namespace
