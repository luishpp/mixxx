#include <gtest/gtest.h>

#include <QVector>
#include <algorithm>

#include "music_sync/analysis/advanced_analysis_adapter.h"
#include "music_sync/domain/track_features.h"

namespace {

using mixxx::music_sync::AdvancedAnalysisAdapter;
using mixxx::music_sync::BandSample;
using mixxx::music_sync::EnergyCurves;

TEST(MusicSyncAdvancedTest, ComputeCurvesBucketsAndNormalizes) {
    // 8 frames: bass constant, mid ramps up -> energy ramps up.
    QVector<BandSample> frames;
    for (int i = 0; i < 8; ++i) {
        BandSample sample;
        sample.low = 100.0f;                      // constant bass
        sample.mid = static_cast<float>(i) * 10.0f; // ramp
        sample.high = 0.0f;
        frames.append(sample);
    }

    const EnergyCurves curves = AdvancedAnalysisAdapter::computeCurves(frames, 4);
    ASSERT_EQ(curves.energy.size(), 4);
    ASSERT_EQ(curves.bass.size(), 4);

    // Energy is non-decreasing across buckets (the input ramps).
    for (int i = 1; i < curves.energy.size(); ++i) {
        EXPECT_GE(curves.energy[i], curves.energy[i - 1]);
    }
    // Per-track normalization: the peak bucket is 1.0.
    float peak = 0.0f;
    for (float v : curves.energy) {
        peak = std::max(peak, v);
    }
    EXPECT_FLOAT_EQ(peak, 1.0f);
    // Bass is constant -> every bucket normalizes to 1.0.
    for (float v : curves.bass) {
        EXPECT_FLOAT_EQ(v, 1.0f);
    }
    // Overall energy is a comparable 0..1 value.
    EXPECT_GE(curves.overallEnergy, 0.0);
    EXPECT_LE(curves.overallEnergy, 1.0);
}

TEST(MusicSyncAdvancedTest, ComputeCurvesEmptyInput) {
    const EnergyCurves curves = AdvancedAnalysisAdapter::computeCurves({}, 4);
    EXPECT_TRUE(curves.energy.isEmpty());
    EXPECT_TRUE(curves.bass.isEmpty());
    EXPECT_DOUBLE_EQ(curves.overallEnergy, 0.0);
}

TEST(MusicSyncAdvancedTest, ComputePhrasesConstantTempo) {
    // 120 BPM -> 500 ms/beat; 4 beats/bar -> 2 s/bar; 16 bars -> 32 s/phrase.
    // 100 s track, first beat at 0 -> phrase starts at 0, 32000, 64000, 96000.
    const QVector<mixxx::music_sync::PhraseMarker> phrases =
            AdvancedAnalysisAdapter::computePhrases(0.0, 120.0, 100000, 4, 16);
    ASSERT_EQ(phrases.size(), 4);
    EXPECT_EQ(phrases[0].startMs, 0);
    EXPECT_EQ(phrases[1].startMs, 32000);
    EXPECT_EQ(phrases[2].startMs, 64000);
    EXPECT_EQ(phrases[3].startMs, 96000);
    EXPECT_EQ(phrases[0].bars, 16);
    // Last phrase is partial: 100 - 96 = 4 s left -> 2 bars.
    EXPECT_EQ(phrases[3].bars, 2);
}

TEST(MusicSyncAdvancedTest, ComputePhrasesGuards) {
    EXPECT_TRUE(AdvancedAnalysisAdapter::computePhrases(0.0, 0.0, 100000, 4, 16).isEmpty());
    EXPECT_TRUE(AdvancedAnalysisAdapter::computePhrases(0.0, 120.0, 0, 4, 16).isEmpty());
    EXPECT_TRUE(AdvancedAnalysisAdapter::computePhrases(0.0, 120.0, 100000, 0, 16).isEmpty());
}

} // namespace
