#include <gtest/gtest.h>

#include <QSet>
#include <QString>

#include "music_sync/domain/mix_intent.h"
#include "music_sync/domain/track_features.h"
#include "music_sync/planner/energy_curve.h"
#include "music_sync/planner/explanation_builder.h"
#include "music_sync/planner/harmonic_compatibility.h"
#include "music_sync/planner/pair_scorer.h"
#include "music_sync/planner/sequence_optimizer.h"
#include "music_sync/planner/transition_planner.h"

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

TEST(MusicSyncOptimizerTest, ProducesAlternativesCoveringAllTracks) {
    QVector<TrackFeatures> tracks;
    tracks.append(makeTrack(1, 122.0, QStringLiteral("8A"), 0.30));
    tracks.append(makeTrack(2, 123.0, QStringLiteral("9A"), 0.40));
    tracks.append(makeTrack(3, 124.0, QStringLiteral("10A"), 0.55));
    tracks.append(makeTrack(4, 125.0, QStringLiteral("11A"), 0.70));
    tracks.append(makeTrack(5, 126.0, QStringLiteral("12A"), 0.85));
    tracks.append(makeTrack(6, 127.0, QStringLiteral("1A"), 1.00));

    SequenceOptimizer::Options options;
    options.intent.energyPreset = EnergyPreset::Ascending;
    options.numAlternatives = 3;

    const QVector<Arrangement> arrangements = SequenceOptimizer::arrange(tracks, options);
    ASSERT_GE(arrangements.size(), 1);
    EXPECT_LE(arrangements.size(), 3);
    for (const Arrangement& arr : arrangements) {
        ASSERT_EQ(arr.items.size(), tracks.size());
        QSet<std::int64_t> ids;
        for (const ArrangementItem& item : arr.items) {
            ids.insert(item.mixxxTrackId);
        }
        EXPECT_EQ(ids.size(), tracks.size()); // each track exactly once
    }
    for (int i = 1; i < arrangements.size(); ++i) {
        EXPECT_LE(arrangements[i].totalScore, arrangements[0].totalScore + 1e-9); // best first
    }
}

TEST(MusicSyncOptimizerTest, RespectsLockedPositions) {
    QVector<TrackFeatures> tracks;
    for (int i = 0; i < 6; ++i) {
        tracks.append(makeTrack(i + 1, 124.0 + i, QStringLiteral("8A"), 0.4 + 0.1 * i));
    }
    SequenceOptimizer::Options options;
    options.locks.append({0, 3}); // track id 3 pinned to position 0
    options.locks.append({5, 6}); // track id 6 pinned to position 5

    const QVector<Arrangement> arrangements = SequenceOptimizer::arrange(tracks, options);
    ASSERT_GE(arrangements.size(), 1);
    for (const Arrangement& arr : arrangements) {
        ASSERT_EQ(arr.items.size(), 6);
        EXPECT_EQ(arr.items.first().mixxxTrackId, 3);
        EXPECT_TRUE(arr.items.first().locked);
        EXPECT_EQ(arr.items.last().mixxxTrackId, 6);
        EXPECT_TRUE(arr.items.last().locked);
    }
}

TEST(MusicSyncOptimizerTest, Deterministic) {
    QVector<TrackFeatures> tracks;
    tracks.append(makeTrack(1, 122.0, QStringLiteral("8A"), 0.30));
    tracks.append(makeTrack(2, 128.0, QStringLiteral("3B"), 0.90));
    tracks.append(makeTrack(3, 124.0, QStringLiteral("9A"), 0.55));
    tracks.append(makeTrack(4, 125.0, QStringLiteral("10A"), 0.70));

    SequenceOptimizer::Options options;
    const QVector<Arrangement> a = SequenceOptimizer::arrange(tracks, options);
    const QVector<Arrangement> b = SequenceOptimizer::arrange(tracks, options);
    ASSERT_FALSE(a.isEmpty());
    ASSERT_EQ(a.size(), b.size());
    ASSERT_EQ(a.first().items.size(), b.first().items.size());
    for (int i = 0; i < a.first().items.size(); ++i) {
        EXPECT_EQ(a.first().items[i].mixxxTrackId, b.first().items[i].mixxxTrackId);
    }
    EXPECT_DOUBLE_EQ(a.first().totalScore, b.first().totalScore);
}

TrackFeatures makeTrackWithWindows(
        std::int64_t id, double bpm, const QString& camelot, double energy) {
    TrackFeatures f = makeTrack(id, bpm, camelot, energy);
    f.durationMs = 300000;
    TransitionWindow exit;
    exit.kind = QStringLiteral("exit");
    exit.startMs = 200000;
    exit.endMs = 232000;
    exit.bars = 16;
    exit.confidence = 0.9f;
    exit.energyStability = 0.9f;
    f.exitWindows.append(exit);
    TransitionWindow entry;
    entry.kind = QStringLiteral("entry");
    entry.startMs = 0;
    entry.endMs = 32000;
    entry.bars = 16;
    entry.confidence = 0.9f;
    entry.energyStability = 0.9f;
    f.entryWindows.append(entry);
    return f;
}

TEST(MusicSyncTransitionTest, PlanValidForGoodPair) {
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 124.5, QStringLiteral("9A"), 0.62);
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent);
    EXPECT_EQ(p.sourceTrackId, 1);
    EXPECT_EQ(p.targetTrackId, 2);
    EXPECT_GT(p.durationBars, 0);
    EXPECT_LE(p.durationBars, 16); // capped by the exit window bars
    EXPECT_DOUBLE_EQ(p.targetBpm, 124.5);
    EXPECT_NEAR(p.sourceRateRatio, 124.5 / 124.0, 1e-9);
    EXPECT_DOUBLE_EQ(p.targetRateRatio, 1.0);
    EXPECT_FALSE(p.ramps.isEmpty());
    EXPECT_FALSE(p.actions.isEmpty());
    EXPECT_TRUE(p.type == TransitionType::EqBlend || p.type == TransitionType::BassSwap);
    EXPECT_GT(p.confidence, 0.5);
    EXPECT_EQ(p.sourceExitMs, 200000);
    EXPECT_EQ(p.targetEntryMs, 0);
}

TEST(MusicSyncTransitionTest, FallbackWhenNoWindows) {
    const MixIntent intent;
    const TrackFeatures a = makeTrack(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrack(2, 124.5, QStringLiteral("9A"), 0.60);
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent);
    EXPECT_EQ(p.type, TransitionType::AutoDjFallback);
    EXPECT_FALSE(p.warnings.isEmpty());
    EXPECT_LE(p.durationBars, 16);
}

TEST(MusicSyncTransitionTest, CutWhenIncompatible) {
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 145.0, QStringLiteral("3B"), 0.60);
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent);
    EXPECT_EQ(p.type, TransitionType::CutOnPhrase);
    EXPECT_LE(p.durationBars, 8); // a cut is short
}

} // namespace
