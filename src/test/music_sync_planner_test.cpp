#include <gtest/gtest.h>

#include <QSet>
#include <QString>

#include "music_sync/domain/mix_intent.h"
#include "music_sync/domain/track_features.h"
#include "music_sync/planner/energy_curve.h"
#include "music_sync/planner/explanation_builder.h"
#include "music_sync/planner/harmonic_compatibility.h"
#include "music_sync/planner/energy_normalizer.h"
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

TEST(MusicSyncTransitionTest, CutWhenIncompatibleAndNoBreakdownToLandIn) {
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 145.0, QStringLiteral("3B"), 0.60);
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent);
    // No sections at all -> nothing to hide the clash behind, so a cut is honest.
    EXPECT_EQ(p.type, TransitionType::CutOnPhrase);
    EXPECT_LE(p.durationBars, 8);
    EXPECT_FALSE(p.beatSync); // never drag the incoming track to a foreign tempo
}

TrackFeatures withBreakdownNearExit(TrackFeatures f) {
    Section breakdown;
    breakdown.type = QStringLiteral("Breakdown");
    breakdown.startMs = 200000; // 67% of the 300 s fixture
    breakdown.endMs = 240000;
    breakdown.energy = 0.2f;
    f.sections.append(breakdown);
    return f;
}

TEST(MusicSyncTransitionTest, ClashingKeyBecomesABreakdownSwapNotACut) {
    // The Portal symptom: 3B -> 9B is a clash, but the tempo is fine and the
    // outgoing track has a breakdown to land in. Spec 16 calls for a "troca por
    // breakdown"; four hard cuts in the atmospheric opening is what we got
    // before, and it sounded wrong.
    const MixIntent intent;
    const TrackFeatures a =
            withBreakdownNearExit(makeTrackWithWindows(1, 125.0, QStringLiteral("3B"), 0.30));
    const TrackFeatures b = makeTrackWithWindows(2, 123.0, QStringLiteral("9B"), 0.30);
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent);

    EXPECT_EQ(p.type, TransitionType::BreakdownSwap);
    EXPECT_GT(p.durationBars, 8); // long and gentle, unlike a cut
    // The tempos agree (1.6%), so this one still beat-matches.
    EXPECT_TRUE(p.beatSync);
    EXPECT_FALSE(p.ramps.isEmpty());
}

TEST(MusicSyncTransitionTest, CutWithMatchingTempoStillBeatSyncs) {
    // Weightless (130) -> Gravity (127) in the real set: a key clash forces a cut,
    // but the 2.3% tempo gap means the cut should still lock and phase-align so it
    // lands on the beat. Keying sync off the type made this come in off-beat.
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 130.0, QStringLiteral("6B"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 127.0, QStringLiteral("10A"), 0.60);
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent);
    EXPECT_EQ(p.type, TransitionType::CutOnPhrase); // no sections to land in
    EXPECT_TRUE(p.beatSync);                        // tempos agree, so it locks
}

TEST(MusicSyncTransitionTest, BreakdownSwapForATempoGapDoesNotSync) {
    // Same landing zone, but chosen because the tempo moves: locking would drag
    // the incoming track to a foreign tempo.
    const MixIntent intent;
    const TrackFeatures a =
            withBreakdownNearExit(makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60));
    const TrackFeatures b = makeTrackWithWindows(2, 145.0, QStringLiteral("8A"), 0.60);
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent);
    EXPECT_EQ(p.type, TransitionType::BreakdownSwap);
    EXPECT_FALSE(p.beatSync);
}

TrackFeatures makeActTrack(std::int64_t id, int act, double bpm, const QString& camelot) {
    TrackFeatures f = makeTrack(id, bpm, camelot, 0.5);
    f.act = act;
    return f;
}

int actOf(const QVector<TrackFeatures>& tracks, std::int64_t id) {
    for (const TrackFeatures& f : tracks) {
        if (f.mixxxTrackId == id) {
            return f.act;
        }
    }
    return -1;
}

TEST(MusicSyncOptimizerTest, ActsConstrainTheOrder) {
    // Act 3 tracks are deliberately the most compatible with the act 1 opener,
    // so a purely score-driven optimizer would interleave them. The narrative
    // must win: every act 1 track comes before every act 2, and so on.
    QVector<TrackFeatures> tracks;
    tracks.append(makeActTrack(1, 1, 124.0, QStringLiteral("8A")));
    tracks.append(makeActTrack(2, 3, 124.0, QStringLiteral("8A")));
    tracks.append(makeActTrack(3, 2, 130.0, QStringLiteral("3B")));
    tracks.append(makeActTrack(4, 1, 124.5, QStringLiteral("9A")));
    tracks.append(makeActTrack(5, 3, 124.0, QStringLiteral("8A")));
    tracks.append(makeActTrack(6, 2, 129.0, QStringLiteral("3B")));

    SequenceOptimizer::Options options;
    options.numAlternatives = 3;
    const QVector<Arrangement> out = SequenceOptimizer::arrange(tracks, options);

    ASSERT_FALSE(out.isEmpty());
    const Arrangement& best = out.first();
    ASSERT_EQ(best.items.size(), tracks.size()); // nobody dropped
    int previousAct = 0;
    for (const ArrangementItem& item : best.items) {
        const int act = actOf(tracks, item.mixxxTrackId);
        EXPECT_GE(act, previousAct) << "acts must never go backwards";
        previousAct = act;
    }
}

TEST(MusicSyncOptimizerTest, TracksWithoutActGoLast) {
    QVector<TrackFeatures> tracks;
    tracks.append(makeActTrack(1, 0, 124.0, QStringLiteral("8A"))); // extra
    tracks.append(makeActTrack(2, 1, 124.0, QStringLiteral("8A")));
    tracks.append(makeActTrack(3, 2, 124.0, QStringLiteral("8A")));

    SequenceOptimizer::Options options;
    const QVector<Arrangement> out = SequenceOptimizer::arrange(tracks, options);
    ASSERT_FALSE(out.isEmpty());
    const QVector<ArrangementItem>& items = out.first().items;
    ASSERT_EQ(items.size(), 3);
    EXPECT_EQ(actOf(tracks, items.last().mixxxTrackId), 0);
}

TEST(MusicSyncOptimizerTest, NoActsBehavesLikeBefore) {
    // A library that was never prepped has no acts: one group, so the act path
    // must fall through to the plain global optimization.
    QVector<TrackFeatures> tracks;
    tracks.append(makeActTrack(1, 0, 124.0, QStringLiteral("8A")));
    tracks.append(makeActTrack(2, 0, 124.5, QStringLiteral("9A")));
    tracks.append(makeActTrack(3, 0, 125.0, QStringLiteral("8A")));

    SequenceOptimizer::Options options;
    const QVector<Arrangement> out = SequenceOptimizer::arrange(tracks, options);
    ASSERT_FALSE(out.isEmpty());
    // Every track routed exactly once, as the plain path has always done. (How
    // many distinct alternatives come out depends on route diversity — with
    // near-identical tracks 2-opt converges to one, which is not a defect.)
    EXPECT_EQ(out.first().items.size(), 3);
    QSet<std::int64_t> seen;
    for (const ArrangementItem& item : out.first().items) {
        seen.insert(item.mixxxTrackId);
    }
    EXPECT_EQ(seen.size(), 3);
}

TEST(MusicSyncOptimizerTest, AnchorHoldsItsPlaceInsideTheAct) {
    // Act 1 of the plan: the anchor is track 05 and must CLOSE the act, even
    // though the engine would rather move it (the others pair better with it
    // in the middle).
    QVector<TrackFeatures> tracks;
    const auto make = [](std::int64_t id, const QString& num, const QString& fn) {
        TrackFeatures f = makeTrack(id, 124.0, QStringLiteral("8A"), 0.5);
        f.act = 1;
        f.trackNumber = num;
        f.setFunction = fn;
        return f;
    };
    tracks.append(make(1, QStringLiteral("01"), QStringLiteral("INTRO")));
    tracks.append(make(2, QStringLiteral("02"), QStringLiteral("PONTE")));
    tracks.append(make(3, QStringLiteral("03"), QStringLiteral("GROOVE")));
    tracks.append(make(4, QStringLiteral("04"), QStringLiteral("EMOCIONAL")));
    tracks.append(make(5, QStringLiteral("05"), QStringLiteral("ÂNCORA")));
    // A second act, so the act path (not the plain one) runs.
    TrackFeatures second = makeTrack(6, 124.0, QStringLiteral("8A"), 0.5);
    second.act = 2;
    second.trackNumber = QStringLiteral("06");
    tracks.append(second);

    SequenceOptimizer::Options options;
    const QVector<Arrangement> out = SequenceOptimizer::arrange(tracks, options);
    ASSERT_FALSE(out.isEmpty());
    const QVector<ArrangementItem>& items = out.first().items;
    ASSERT_EQ(items.size(), 6);
    // Track 5 is the anchor at rank 5 of act 1 -> index 4, i.e. it closes act 1.
    EXPECT_EQ(items.at(4).mixxxTrackId, 5);
    EXPECT_TRUE(items.at(4).locked);
    EXPECT_FALSE(items.at(0).locked); // non-anchors stay free
}

TEST(MusicSyncOptimizerTest, PlanOrderAlwaysLeads) {
    // The real symptom: the set opened with Weightless (03) instead of The
    // Future Is Unknown (01), because the engine picks the opener by energy fit
    // and cannot know track 01 is the cinematic intro.
    const auto make = [](std::int64_t id, int act, const QString& num, double energy) {
        TrackFeatures f = makeTrack(id, 124.0, QStringLiteral("8A"), energy);
        f.act = act;
        f.trackNumber = num;
        f.energyCurve = QVector<float>(4, 0.5f);
        return f;
    };
    QVector<TrackFeatures> tracks;
    tracks.append(make(1, 1, QStringLiteral("01"), 0.28));
    tracks.append(make(2, 1, QStringLiteral("02"), 0.08));
    tracks.append(make(3, 1, QStringLiteral("03"), 0.64));
    tracks.append(make(4, 2, QStringLiteral("04"), 0.5));

    SequenceOptimizer::Options options;
    options.intent.energyPreset = EnergyPreset::Waves;
    const QVector<Arrangement> out = SequenceOptimizer::arrange(tracks, options);
    ASSERT_FALSE(out.isEmpty());
    const QVector<ArrangementItem>& items = out.first().items;
    ASSERT_EQ(items.size(), 4);
    EXPECT_EQ(items.at(0).mixxxTrackId, 1);
    EXPECT_EQ(items.at(1).mixxxTrackId, 2);
    EXPECT_EQ(items.at(2).mixxxTrackId, 3);
}

TEST(MusicSyncOptimizerTest, PlanOrderLeadsEvenWhenItScoresWorse) {
    // Act 1 of the real set runs 3B -> 9B -> 6B -> 10A -> 5A: poor by Camelot
    // adjacency, so a score comparison hands the lead to the engine and the set
    // gets musically worse. The plan was curated by narrative and ear, and the
    // keys come from Mixxx's detection anyway — so score must not decide.
    const auto make = [](std::int64_t id, int act, const QString& num, const QString& camelot) {
        TrackFeatures f = makeTrack(id, 124.0, camelot, 0.5);
        f.act = act;
        f.trackNumber = num;
        return f;
    };
    QVector<TrackFeatures> tracks;
    tracks.append(make(1, 1, QStringLiteral("01"), QStringLiteral("3B")));
    tracks.append(make(2, 1, QStringLiteral("02"), QStringLiteral("9B")));
    tracks.append(make(3, 1, QStringLiteral("03"), QStringLiteral("3B")));
    tracks.append(make(4, 2, QStringLiteral("04"), QStringLiteral("8A")));

    SequenceOptimizer::Options options;
    const QVector<Arrangement> out = SequenceOptimizer::arrange(tracks, options);
    ASSERT_FALSE(out.isEmpty());
    const QVector<ArrangementItem>& items = out.first().items;
    ASSERT_EQ(items.size(), 4);
    // Plan order, despite 3B -> 9B being a clash the engine would rather avoid.
    EXPECT_EQ(items.at(0).mixxxTrackId, 1);
    EXPECT_EQ(items.at(1).mixxxTrackId, 2);
    EXPECT_EQ(items.at(2).mixxxTrackId, 3);
    // The engine's own route is still offered, so it can be compared.
    EXPECT_GE(out.size(), 2);
}

TEST(MusicSyncOptimizerTest, EachActIsJudgedOnItsOwnSliceOfTheCurve) {
    // With an ascending curve, a late act must prefer its HIGH-energy track
    // first-to-last order. If every act were judged against the whole 0..1 curve
    // (the bug), the late act would be told to start low and build, like the
    // opener.
    const auto make = [](std::int64_t id, int act, double energy) {
        TrackFeatures f = makeTrack(id, 124.0, QStringLiteral("8A"), energy);
        f.act = act;
        f.energyCurve = QVector<float>(4, 0.5f);
        return f;
    };
    QVector<TrackFeatures> tracks;
    tracks.append(make(1, 1, 0.0)); // act 1: the quiet opener
    tracks.append(make(2, 1, 0.1));
    tracks.append(make(3, 7, 0.9)); // act 7: both loud, near the curve's top
    tracks.append(make(4, 7, 1.0));

    SequenceOptimizer::Options options;
    options.intent.energyPreset = EnergyPreset::Ascending;
    const QVector<Arrangement> out = SequenceOptimizer::arrange(tracks, options);
    ASSERT_FALSE(out.isEmpty());
    // Act 7 sits at the end of the curve, so its louder track closes the set.
    EXPECT_EQ(out.first().items.last().mixxxTrackId, 4);
}

TEST(MusicSyncOptimizerTest, NoAnchorLocksWithoutTrackNumbers) {
    // An unprepped act (no plan numbers) must not get guessed positions.
    QVector<TrackFeatures> tracks;
    for (int i = 0; i < 3; ++i) {
        TrackFeatures f = makeTrack(i + 1, 124.0, QStringLiteral("8A"), 0.5);
        f.act = 1;
        f.setFunction = QStringLiteral("ÂNCORA"); // anchor, but no number
        tracks.append(f);
    }
    TrackFeatures second = makeTrack(9, 124.0, QStringLiteral("8A"), 0.5);
    second.act = 2;
    tracks.append(second);

    SequenceOptimizer::Options options;
    const QVector<Arrangement> out = SequenceOptimizer::arrange(tracks, options);
    ASSERT_FALSE(out.isEmpty());
    for (const ArrangementItem& item : out.first().items) {
        EXPECT_FALSE(item.locked);
    }
}

TEST(MusicSyncPairScoreTest, HarmonyWeightFollowsTheAct) {
    const ScoringWeights base;
    // Portal / Melodic House / Melodic Techno / Final: harmony leads (spec 10.5).
    for (int act : {1, 2, 5, 7}) {
        const ScoringWeights w = weightsForAct(act, base);
        EXPECT_GT(w.harmonic, base.harmonic) << "act " << act;
    }
    // Nostalgia flashes and peak crossover: harmony deliberately looser.
    for (int act : {4, 6}) {
        const ScoringWeights w = weightsForAct(act, base);
        EXPECT_LT(w.harmonic, base.harmonic) << "act " << act;
    }
    // Groove and unknown keep the spec defaults.
    EXPECT_DOUBLE_EQ(weightsForAct(3, base).harmonic, base.harmonic);
    EXPECT_DOUBLE_EQ(weightsForAct(0, base).harmonic, base.harmonic);

    // Every profile still sums to 1.0, so scores stay comparable across acts.
    for (int act = 0; act <= 7; ++act) {
        const ScoringWeights w = weightsForAct(act, base);
        const double sum = w.harmonic + w.tempo + w.phrase + w.energy +
                w.vocalSafety + w.transitionWindow + w.style;
        EXPECT_NEAR(sum, 1.0, 1e-9) << "act " << act;
    }
}

TEST(MusicSyncPairScoreTest, HarmonyPriorityActPunishesAClashingKey) {
    // The real symptom: in the Portal, a distant key still scored ~72% and won.
    const TrackFeatures a = makeTrack(1, 125.0, QStringLiteral("3B"), 0.3);
    const TrackFeatures b = makeTrack(2, 127.0, QStringLiteral("10A"), 0.3);
    const ScoringWeights base;
    const double defaultScore = PairScorer::score(a, b, base, 5.0).total;
    const double portalScore = PairScorer::score(a, b, weightsForAct(1, base), 5.0).total;
    EXPECT_LT(portalScore, defaultScore);
}

TEST(MusicSyncTransitionTest, OverrideTypeWinsOverThePlanner) {
    // RF-010: the DJ pins the type. The planner would pick a blend here.
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 124.5, QStringLiteral("9A"), 0.62);
    EXPECT_NE(TransitionPlanner::plan(a, b, intent).type, TransitionType::CutOnPhrase);

    TransitionOverride override;
    override.type = TransitionType::CutOnPhrase;
    EXPECT_EQ(TransitionPlanner::plan(a, b, intent, override).type,
            TransitionType::CutOnPhrase);
}

TEST(MusicSyncTransitionTest, OverrideBarsBeatsTheHeuristicCaps) {
    // A cut is normally capped at 8 bars — that cap is the planner's taste, and
    // an explicit request overrules taste (only physics may object).
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 145.0, QStringLiteral("3B"), 0.60);
    EXPECT_LE(TransitionPlanner::plan(a, b, intent).durationBars, 8);

    TransitionOverride override;
    override.type = TransitionType::CutOnPhrase;
    override.bars = 24;
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent, override);
    EXPECT_EQ(p.durationBars, 24);
}

TEST(MusicSyncTransitionTest, OverrideBeyondTheWindowIsAllowedButWarned) {
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 124.5, QStringLiteral("9A"), 0.62);
    TransitionOverride override;
    override.bars = 24; // the fixture's exit window is 16 bars
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent, override);
    EXPECT_EQ(p.durationBars, 24); // honoured
    bool warned = false;
    for (const QString& w : p.warnings) {
        if (w.contains(QStringLiteral("exit window"))) {
            warned = true;
        }
    }
    EXPECT_TRUE(warned); // but told
}

TEST(MusicSyncTransitionTest, OverrideCannotOutlastTheTrack) {
    // Physics nobody overrules: the fixture exits at 200 s of a 300 s track, so
    // roughly 51 bars remain at 124 BPM. Asking for 200 would run the source out
    // mid-handover.
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 124.5, QStringLiteral("9A"), 0.62);
    TransitionOverride override;
    override.bars = 200;
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent, override);
    EXPECT_LT(p.durationBars, 200);
    EXPECT_GT(p.durationBars, 0);
    bool warned = false;
    for (const QString& w : p.warnings) {
        if (w.contains(QStringLiteral("trimmed"))) {
            warned = true;
        }
    }
    EXPECT_TRUE(warned);
}

TEST(MusicSyncTransitionTest, OverrideEntryPointSkipsTheIntro) {
    // A long intro drops the energy right at the handover; the DJ points the
    // incoming track past it. The default entry is the low-energy window at 0.
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 124.5, QStringLiteral("9A"), 0.62);
    EXPECT_EQ(TransitionPlanner::plan(a, b, intent).targetEntryMs, 0);

    TransitionOverride override;
    override.targetEntryMs = 48000;
    EXPECT_EQ(TransitionPlanner::plan(a, b, intent, override).targetEntryMs, 48000);
}

TEST(MusicSyncTransitionTest, OverrideEntryCannotSeekOffTheEnd) {
    // Physics again: an entry past the track length would seek the deck off the
    // end. Clamp to the track (the fixture is 300 s long).
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 124.5, QStringLiteral("9A"), 0.62);
    TransitionOverride override;
    override.targetEntryMs = 999999999;
    EXPECT_EQ(TransitionPlanner::plan(a, b, intent, override).targetEntryMs, 300000);
}

TEST(MusicSyncTransitionTest, EmptyOverrideChangesNothing) {
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 124.5, QStringLiteral("9A"), 0.62);
    const TransitionPlan planned = TransitionPlanner::plan(a, b, intent);
    const TransitionPlan defaulted =
            TransitionPlanner::plan(a, b, intent, TransitionOverride());
    EXPECT_TRUE(TransitionOverride().isEmpty());
    EXPECT_EQ(defaulted.type, planned.type);
    EXPECT_EQ(defaulted.durationBars, planned.durationBars);
}

// Where the incoming track's volume starts to come up, in beats. Small = it is
// audible early (a real blend); near the end = it stays silent until a quick swap.
double targetVolumeStartBeat(const TransitionPlan& p) {
    double start = -1.0;
    for (const AutomationRamp& r : p.ramps) {
        if (r.control == QStringLiteral("targetVolume")) {
            start = (start < 0.0) ? r.fromBeat : std::min(start, r.fromBeat);
        }
    }
    return start;
}

TEST(MusicSyncTransitionTest, UnsyncedTransitionKeepsTheOverlapTight) {
    // A tempo gap the decks can't lock (124 -> 140 ≈ 13%). Blending two unaligned
    // beats "sambas", so the incoming must stay silent until a quick swap at the
    // phrase end — never two beats loud at once.
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 140.0, QStringLiteral("9A"), 0.62);
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent);
    EXPECT_FALSE(p.beatSync);
    const double start = targetVolumeStartBeat(p);
    ASSERT_GE(start, 0.0);
    EXPECT_GE(start, p.durationBeats - 2.0); // silent until the last ~half-bar
}

TEST(MusicSyncTransitionTest, SyncedTransitionStillBlendsAcrossTheSpan) {
    // Tempos lock (124 -> 124.5): a real blend, incoming audible from the start.
    const MixIntent intent;
    const TrackFeatures a = makeTrackWithWindows(1, 124.0, QStringLiteral("8A"), 0.60);
    const TrackFeatures b = makeTrackWithWindows(2, 124.5, QStringLiteral("9A"), 0.62);
    const TransitionPlan p = TransitionPlanner::plan(a, b, intent);
    EXPECT_TRUE(p.beatSync);
    const double start = targetVolumeStartBeat(p);
    ASSERT_GE(start, 0.0);
    EXPECT_LT(start, p.durationBeats - 2.0); // comes in early — an actual crossfade
}

TrackFeatures makeEnergyTrack(std::int64_t id, double energy) {
    TrackFeatures f;
    f.mixxxTrackId = id;
    f.overallEnergy = energy;
    f.energyCurve = QVector<float>(4, 0.5f);
    return f;
}

TEST(MusicSyncEnergyNormalizerTest, SpreadsToFullRangeByRank) {
    QVector<TrackFeatures> tracks;
    tracks.append(makeEnergyTrack(1, 0.30));
    tracks.append(makeEnergyTrack(2, 0.20));
    tracks.append(makeEnergyTrack(3, 0.25));
    normalizeLibraryEnergy(&tracks);
    EXPECT_DOUBLE_EQ(tracks.at(1).overallEnergy, 0.0); // lowest raw (0.20)
    EXPECT_DOUBLE_EQ(tracks.at(2).overallEnergy, 0.5); // middle (0.25)
    EXPECT_DOUBLE_EQ(tracks.at(0).overallEnergy, 1.0); // highest (0.30)
}

TEST(MusicSyncEnergyNormalizerTest, IgnoresTracksWithoutEnergyCurve) {
    QVector<TrackFeatures> tracks;
    tracks.append(makeEnergyTrack(1, 0.20));
    tracks.append(makeEnergyTrack(2, 0.30));
    TrackFeatures noCurve; // never waveform-analyzed
    noCurve.mixxxTrackId = 3;
    noCurve.overallEnergy = 0.99;
    tracks.append(noCurve);
    normalizeLibraryEnergy(&tracks);
    EXPECT_DOUBLE_EQ(tracks.at(0).overallEnergy, 0.0);
    EXPECT_DOUBLE_EQ(tracks.at(1).overallEnergy, 1.0);
    EXPECT_DOUBLE_EQ(tracks.at(2).overallEnergy, 0.99); // untouched, not ranked
}

TEST(MusicSyncEnergyNormalizerTest, SingleTrackGetsMidpoint) {
    QVector<TrackFeatures> tracks;
    tracks.append(makeEnergyTrack(1, 0.22));
    normalizeLibraryEnergy(&tracks);
    EXPECT_DOUBLE_EQ(tracks.first().overallEnergy, 0.5);
}

TEST(MusicSyncEnergyNormalizerTest, UnlocksHighEnergyGate) {
    // The real-world complaint: a whole library sits at ~0.15..0.33 raw, so the
    // absolute "> 0.45" dancey gate (Bass Swap) could never fire.
    QVector<TrackFeatures> tracks;
    for (int i = 0; i < 10; ++i) {
        tracks.append(makeEnergyTrack(i + 1, 0.15 + 0.02 * i));
    }
    for (const TrackFeatures& f : tracks) {
        ASSERT_LT(f.overallEnergy, 0.45); // nothing qualifies beforehand
    }
    normalizeLibraryEnergy(&tracks);
    int dancey = 0;
    for (const TrackFeatures& f : tracks) {
        if (f.overallEnergy > 0.45) {
            ++dancey;
        }
    }
    EXPECT_GT(dancey, 0);
}

} // namespace
