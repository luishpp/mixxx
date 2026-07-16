#include <gtest/gtest.h>

#include <QHash>
#include <QString>

#include "music_sync/domain/arrangement.h"
#include "music_sync/domain/set_program.h"
#include "music_sync/planner/set_compiler.h"
#include "music_sync/preview/set_executor.h"

namespace mixxx::music_sync {
namespace {

TrackFeatures makeSetTrack(std::int64_t id, double bpm, const QString& camelot) {
    TrackFeatures f;
    f.mixxxTrackId = id;
    f.artist = QStringLiteral("Artist %1").arg(id);
    f.title = QStringLiteral("Title %1").arg(id);
    f.bpm = bpm;
    f.camelot = camelot;
    f.durationMs = 300000; // 5:00
    f.analyzed = true;
    f.overallEnergy = 0.6;
    f.energyCurve = QVector<float>(8, 0.6f);
    TransitionWindow entry;
    entry.kind = QStringLiteral("entry");
    entry.startMs = 8000;
    entry.endMs = 40000;
    entry.bars = 32;
    entry.confidence = 0.9f;
    f.entryWindows.append(entry);
    TransitionWindow exit;
    exit.kind = QStringLiteral("exit");
    exit.startMs = 210000; // 70%
    exit.endMs = 270000;
    exit.bars = 32;
    exit.confidence = 0.9f;
    f.exitWindows.append(exit);
    return f;
}

struct Fixture {
    Arrangement arrangement;
    QHash<std::int64_t, TrackFeatures> byId;
};

Fixture makeFixture(int count) {
    Fixture fx;
    for (int i = 0; i < count; ++i) {
        const TrackFeatures track =
                makeSetTrack(i + 1, 124.0 + i * 0.5, QStringLiteral("8A"));
        fx.byId.insert(track.mixxxTrackId, track);
        ArrangementItem item;
        item.mixxxTrackId = track.mixxxTrackId;
        item.position = i;
        fx.arrangement.items.append(item);
    }
    return fx;
}

TEST(MusicSyncSetTest, DecksAlternateSoTheNextTrackCanPreload) {
    // The whole point of two decks: while one plays, the next is already
    // loaded and cued on the other.
    const Fixture fx = makeFixture(5);
    const SetProgram set = SetCompiler::compile(fx.arrangement, fx.byId, MixIntent());

    ASSERT_EQ(set.items.size(), 5);
    for (int i = 0; i < set.items.size(); ++i) {
        EXPECT_EQ(set.items.at(i).deckIndex, i % 2) << "position " << i;
        if (i > 0) {
            EXPECT_NE(set.items.at(i).deckIndex, set.items.at(i - 1).deckIndex);
        }
    }
}

TEST(MusicSyncSetTest, OneTransitionPerConsecutivePair) {
    const Fixture fx = makeFixture(5);
    const SetProgram set = SetCompiler::compile(fx.arrangement, fx.byId, MixIntent());

    ASSERT_EQ(set.transitions.size(), 4); // 5 tracks -> 4 handovers
    for (int i = 0; i < set.transitions.size(); ++i) {
        const SetTransition& transition = set.transitions.at(i);
        EXPECT_EQ(transition.fromPosition, i);
        EXPECT_EQ(transition.program.sourceTrackId, set.items.at(i).mixxxTrackId);
        EXPECT_EQ(transition.program.targetTrackId, set.items.at(i + 1).mixxxTrackId);
        EXPECT_GT(transition.program.durationBeats, 0.0);
        EXPECT_FALSE(transition.program.writes.isEmpty());
    }
}

TEST(MusicSyncSetTest, EntryAndExitComeFromTheTransitions) {
    // A track must come in exactly where the previous transition puts it, and
    // hand over exactly where the next one takes it — otherwise the executor
    // and the automation would disagree about the timeline.
    const Fixture fx = makeFixture(3);
    const SetProgram set = SetCompiler::compile(fx.arrangement, fx.byId, MixIntent());

    ASSERT_EQ(set.items.size(), 3);
    // The opener starts at its own entry window.
    EXPECT_EQ(set.items.at(0).startMs, 8000);
    for (int i = 1; i < set.items.size(); ++i) {
        EXPECT_EQ(set.items.at(i).startMs, set.transitions.at(i - 1).program.targetStartMs);
    }
    for (int i = 0; i + 1 < set.items.size(); ++i) {
        EXPECT_EQ(set.items.at(i).exitMs, set.transitions.at(i).program.sourceStartMs);
    }
    // The last track has no successor: it runs to its end.
    EXPECT_EQ(set.items.last().exitMs, set.items.last().durationMs);
    EXPECT_GT(set.items.first().playSpanMs(), 0);
}

TEST(MusicSyncSetTest, SingleTrackHasNoTransitions) {
    const Fixture fx = makeFixture(1);
    const SetProgram set = SetCompiler::compile(fx.arrangement, fx.byId, MixIntent());
    ASSERT_EQ(set.items.size(), 1);
    EXPECT_TRUE(set.transitions.isEmpty());
    EXPECT_EQ(set.items.first().exitMs, set.items.first().durationMs);
}

TEST(MusicSyncSetTest, EmptyArrangementCompilesToNothing) {
    const SetProgram set =
            SetCompiler::compile(Arrangement(), QHash<std::int64_t, TrackFeatures>(), MixIntent());
    EXPECT_TRUE(set.items.isEmpty());
    EXPECT_TRUE(set.transitions.isEmpty());
}

TEST(MusicSyncSetTest, MissingTrackTruncatesAndWarns) {
    // Skipping a missing track would silently reorder the narrative, so the set
    // stops there and says so.
    Fixture fx = makeFixture(4);
    fx.byId.remove(3); // the third track vanished from the library
    const SetProgram set = SetCompiler::compile(fx.arrangement, fx.byId, MixIntent());

    EXPECT_EQ(set.items.size(), 2); // stops before the gap
    EXPECT_EQ(set.transitions.size(), 1);
    EXPECT_FALSE(set.warnings.isEmpty());
}

TEST(MusicSyncSetTest, EstimatedDurationAddsUpThePlaySpans) {
    const Fixture fx = makeFixture(3);
    const SetProgram set = SetCompiler::compile(fx.arrangement, fx.byId, MixIntent());
    std::int64_t expected = 0;
    for (const SetItem& item : set.items) {
        expected += item.playSpanMs();
    }
    EXPECT_EQ(set.estimatedDurationMs(), expected);
    EXPECT_GT(set.estimatedDurationMs(), 0);
    EXPECT_FALSE(set.explanation.isEmpty());
}

TEST(MusicSyncSetTest, ReachedExitDecidesTheHandover) {
    // The decision the whole set hinges on: 5:00 track handing over at 3:30.
    constexpr std::int64_t kDuration = 300000;
    constexpr std::int64_t kExit = 210000; // 70%

    EXPECT_FALSE(SetExecutor::reachedExit(0.0, kExit, kDuration));
    EXPECT_FALSE(SetExecutor::reachedExit(0.69, kExit, kDuration));
    EXPECT_TRUE(SetExecutor::reachedExit(0.70, kExit, kDuration));
    EXPECT_TRUE(SetExecutor::reachedExit(0.95, kExit, kDuration));
}

TEST(MusicSyncSetTest, ReachedExitNeverFiresOnMissingData) {
    // Without a duration or an exit there is no handover point, and guessing one
    // would cut a track off mid-play.
    EXPECT_FALSE(SetExecutor::reachedExit(0.9, 210000, 0));
    EXPECT_FALSE(SetExecutor::reachedExit(0.9, 0, 300000));
}

} // namespace
} // namespace mixxx::music_sync
