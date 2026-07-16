#include <gtest/gtest.h>

#include <QString>

#include "music_sync/domain/preview_program.h"
#include "music_sync/domain/transition_plan.h"
#include "music_sync/planner/preview_compiler.h"

namespace mixxx::music_sync {
namespace {

TEST(MusicSyncPreviewTest, ExpandsRampToDiscreteWrites) {
    TransitionPlan plan;
    plan.durationBeats = 128.0;
    AutomationRamp ramp;
    ramp.control = QStringLiteral("crossfader");
    ramp.fromBeat = 0.0;
    ramp.toBeat = 128.0;
    ramp.startValue = -1.0;
    ramp.endValue = 1.0;
    plan.ramps.append(ramp);

    const PreviewProgram program = PreviewCompiler::compile(plan, 0.25);

    // ceil(128 / 0.25) = 512 steps -> 513 inclusive samples.
    EXPECT_EQ(program.writes.size(), 513);
    EXPECT_DOUBLE_EQ(program.writes.first().atBeat, 0.0);
    EXPECT_DOUBLE_EQ(program.writes.first().value, -1.0);
    EXPECT_DOUBLE_EQ(program.writes.last().atBeat, 128.0);
    EXPECT_DOUBLE_EQ(program.writes.last().value, 1.0);
    // Halfway sample sits at beat 64 with value 0.
    EXPECT_DOUBLE_EQ(program.writes.at(256).atBeat, 64.0);
    EXPECT_NEAR(program.writes.at(256).value, 0.0, 1e-9);
    for (const ControlWrite& write : program.writes) {
        EXPECT_EQ(write.control, QStringLiteral("crossfader"));
    }
}

TEST(MusicSyncPreviewTest, ActionsAndRampsMergedAndSorted) {
    TransitionPlan plan;
    plan.actions.append({QStringLiteral("targetPlay"), 0.0, 1.0});
    plan.actions.append({QStringLiteral("sourceLowEq"), 96.0, 0.0});
    AutomationRamp ramp;
    ramp.control = QStringLiteral("targetVolume");
    ramp.fromBeat = 0.0;
    ramp.toBeat = 96.0;
    ramp.startValue = 0.0;
    ramp.endValue = 1.0;
    plan.ramps.append(ramp);

    const PreviewProgram program = PreviewCompiler::compile(plan);

    ASSERT_FALSE(program.writes.isEmpty());
    for (int i = 1; i < program.writes.size(); ++i) {
        EXPECT_LE(program.writes.at(i - 1).atBeat, program.writes.at(i).atBeat);
    }
    EXPECT_DOUBLE_EQ(program.writes.first().atBeat, 0.0);
    bool sawLateAction = false;
    for (const ControlWrite& write : program.writes) {
        if (write.control == QStringLiteral("sourceLowEq")) {
            EXPECT_DOUBLE_EQ(write.atBeat, 96.0);
            sawLateAction = true;
        }
    }
    EXPECT_TRUE(sawLateAction);
}

TEST(MusicSyncPreviewTest, ZeroLengthRampEmitsEndpoint) {
    TransitionPlan plan;
    AutomationRamp ramp;
    ramp.control = QStringLiteral("crossfader");
    ramp.fromBeat = 10.0;
    ramp.toBeat = 10.0;
    ramp.startValue = 0.2;
    ramp.endValue = 0.9;
    plan.ramps.append(ramp);

    const PreviewProgram program = PreviewCompiler::compile(plan);

    ASSERT_EQ(program.writes.size(), 1);
    EXPECT_DOUBLE_EQ(program.writes.first().atBeat, 10.0);
    EXPECT_DOUBLE_EQ(program.writes.first().value, 0.9);
}

TEST(MusicSyncPreviewTest, BeatSyncOnlyForBlendingTransitions) {
    // A cut is chosen BECAUSE the tempos clash, so the executor must not drag
    // the incoming track to the outgoing one's tempo.
    const auto programFor = [](TransitionType type) {
        TransitionPlan plan;
        plan.type = type;
        return PreviewCompiler::compile(plan);
    };
    EXPECT_TRUE(programFor(TransitionType::Crossfade).needsBeatSync());
    EXPECT_TRUE(programFor(TransitionType::EqBlend).needsBeatSync());
    EXPECT_TRUE(programFor(TransitionType::BassSwap).needsBeatSync());
    EXPECT_TRUE(programFor(TransitionType::FilterTransition).needsBeatSync());
    EXPECT_FALSE(programFor(TransitionType::CutOnPhrase).needsBeatSync());
    EXPECT_FALSE(programFor(TransitionType::AutoDjFallback).needsBeatSync());
}

TEST(MusicSyncPreviewTest, CopiesTransitionType) {
    TransitionPlan plan;
    plan.type = TransitionType::BassSwap;
    EXPECT_EQ(PreviewCompiler::compile(plan).type, TransitionType::BassSwap);
}

TEST(MusicSyncPreviewTest, CopiesPlanMetadata) {
    TransitionPlan plan;
    plan.sourceTrackId = 7;
    plan.targetTrackId = 8;
    plan.sourceExitMs = 200000;
    plan.targetEntryMs = 0;
    plan.sourceRateRatio = 1.01;
    plan.targetRateRatio = 1.0;
    plan.targetBpm = 120.0;
    plan.durationBeats = 64.0;
    plan.durationMs = 32000;
    plan.explanation = QStringLiteral("test");

    const PreviewProgram program = PreviewCompiler::compile(plan);

    EXPECT_EQ(program.sourceTrackId, 7);
    EXPECT_EQ(program.targetTrackId, 8);
    EXPECT_EQ(program.sourceStartMs, 200000);
    EXPECT_EQ(program.targetStartMs, 0);
    EXPECT_DOUBLE_EQ(program.sourceRateRatio, 1.01);
    EXPECT_DOUBLE_EQ(program.targetRateRatio, 1.0);
    EXPECT_DOUBLE_EQ(program.targetBpm, 120.0);
    EXPECT_EQ(program.durationMs, 32000);
    EXPECT_EQ(program.explanation, QStringLiteral("test"));
    EXPECT_DOUBLE_EQ(program.beatToMs(2.0), 1000.0);
}

} // namespace
} // namespace mixxx::music_sync
