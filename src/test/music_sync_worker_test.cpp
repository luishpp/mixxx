#include <gtest/gtest.h>

#include "music_sync/analysis/worker_client.h"

namespace mixxx::music_sync {
namespace {

TEST(MusicSyncWorkerTest, ParsesProgress) {
    const WorkerEvent ev =
            WorkerClient::parseLine(R"({"event":"progress","done":3,"total":37})");
    EXPECT_EQ(ev.type, WorkerEvent::Type::Progress);
    EXPECT_EQ(ev.done, 3);
    EXPECT_EQ(ev.total, 37);
}

TEST(MusicSyncWorkerTest, ParsesTrackWithCurveAndVocal) {
    const WorkerEvent ev = WorkerClient::parseLine(
            R"({"event":"track","id":4167,"overallEnergy":366.28,)"
            R"("energyCurve":[0.1,0.5,0.9],"vocalDensity":0.42})");
    EXPECT_EQ(ev.type, WorkerEvent::Type::Track);
    EXPECT_EQ(ev.trackId, 4167);
    EXPECT_DOUBLE_EQ(ev.overallEnergy, 366.28);
    EXPECT_DOUBLE_EQ(ev.vocalDensity, 0.42);
    ASSERT_EQ(ev.energyCurve.size(), 3);
    EXPECT_FLOAT_EQ(ev.energyCurve.at(2), 0.9f);
}

TEST(MusicSyncWorkerTest, ParsesTrackErrorAndDoneAndFatal) {
    EXPECT_EQ(WorkerClient::parseLine(R"({"event":"trackError","id":9,"message":"bad file"})").type,
            WorkerEvent::Type::TrackError);
    EXPECT_EQ(WorkerClient::parseLine(R"({"event":"done","total":37})").type,
            WorkerEvent::Type::Done);
    const WorkerEvent fatal =
            WorkerClient::parseLine(R"({"event":"fatal","message":"missing deps"})");
    EXPECT_EQ(fatal.type, WorkerEvent::Type::Fatal);
    EXPECT_EQ(fatal.message, QStringLiteral("missing deps"));
}

TEST(MusicSyncWorkerTest, GarbageAndBlankLinesAreUnknownNotACrash) {
    EXPECT_EQ(WorkerClient::parseLine("not json at all").type, WorkerEvent::Type::Unknown);
    EXPECT_EQ(WorkerClient::parseLine("").type, WorkerEvent::Type::Unknown);
    EXPECT_EQ(WorkerClient::parseLine("   ").type, WorkerEvent::Type::Unknown);
    // A valid JSON object without a known event is Unknown, not a wrong type.
    EXPECT_EQ(WorkerClient::parseLine(R"({"hello":"world"})").type, WorkerEvent::Type::Unknown);
}

} // namespace
} // namespace mixxx::music_sync
