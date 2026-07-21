#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "music_sync/domain/session_report.h"
#include "music_sync/reporting/session_report_writer.h"

namespace mixxx::music_sync {
namespace {

SessionReport makeReport(bool completed) {
    SessionReport r;
    r.musicSyncVersion = QStringLiteral("music-sync 0.8");
    r.mixxxBaseline = QStringLiteral("2.5.6");
    r.recordingPath = QStringLiteral("C:/rec/set.wav");
    r.durationMs = 3661000; // 1:01:01
    r.completed = completed;

    SessionTrack a;
    a.position = 0;
    a.artist = QStringLiteral("Artist A");
    a.title = QStringLiteral("Opener");
    a.entryMs = 0;
    a.effectiveBpm = 128.0;
    a.fileSize = 5000000;
    r.tracks.append(a);

    SessionTrack b;
    b.position = 1;
    b.artist = QStringLiteral("Artist B");
    b.title = QStringLiteral("Second");
    b.entryMs = 184000; // 3:04
    b.effectiveBpm = 125.0;
    b.fileSize = 6000000;
    r.tracks.append(b);

    SessionTransition t;
    t.fromPosition = 0;
    t.atMs = 180000;
    t.type = QStringLiteral("EQ Blend");
    t.bars = 32;
    t.beatSync = true;
    r.transitions.append(t);

    if (!completed) {
        r.warnings.append(QStringLiteral("set interrupted at track 2"));
    }
    return r;
}

TEST(MusicSyncReportTest, FormatClockHandlesMinutesAndHours) {
    EXPECT_EQ(SessionReportWriter::formatClock(0), QStringLiteral("0:00"));
    EXPECT_EQ(SessionReportWriter::formatClock(184000), QStringLiteral("3:04"));
    EXPECT_EQ(SessionReportWriter::formatClock(3661000), QStringLiteral("1:01:01"));
    EXPECT_EQ(SessionReportWriter::formatClock(-5), QStringLiteral("0:00"));
}

TEST(MusicSyncReportTest, TracklistShowsEveryTrackWithItsEntryTime) {
    const QString txt = SessionReportWriter::tracklistText(makeReport(true));
    EXPECT_TRUE(txt.contains(QStringLiteral("Artist A - Opener")));
    EXPECT_TRUE(txt.contains(QStringLiteral("Artist B - Second")));
    EXPECT_TRUE(txt.contains(QStringLiteral("3:04"))); // second track's entry
    EXPECT_TRUE(txt.contains(QStringLiteral("128.0 BPM")));
    // The transition that brought the second track in is noted on its line.
    EXPECT_TRUE(txt.contains(QStringLiteral("EQ Blend")));
    EXPECT_TRUE(txt.contains(QStringLiteral("beatmatched")));
    EXPECT_TRUE(txt.contains(QStringLiteral("Completed set")));
    EXPECT_TRUE(txt.contains(QStringLiteral("C:/rec/set.wav")));
}

TEST(MusicSyncReportTest, InterruptedSetIsMarkedAndKeepsWarnings) {
    const QString txt = SessionReportWriter::tracklistText(makeReport(false));
    EXPECT_TRUE(txt.contains(QStringLiteral("Interrupted set")));
    EXPECT_TRUE(txt.contains(QStringLiteral("set interrupted at track 2")));
}

TEST(MusicSyncReportTest, JsonIsValidAndCarriesTheRequiredFields) {
    const QString json = SessionReportWriter::jsonText(makeReport(true));
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    ASSERT_EQ(err.error, QJsonParseError::NoError) << err.errorString().toStdString();
    ASSERT_TRUE(doc.isObject());
    const QJsonObject root = doc.object();

    EXPECT_EQ(root[QStringLiteral("mixxxBaseline")].toString(), QStringLiteral("2.5.6"));
    EXPECT_EQ(root[QStringLiteral("recordingPath")].toString(), QStringLiteral("C:/rec/set.wav"));
    EXPECT_TRUE(root[QStringLiteral("completed")].toBool());

    const QJsonArray tracks = root[QStringLiteral("tracks")].toArray();
    ASSERT_EQ(tracks.size(), 2);
    EXPECT_EQ(tracks.at(1).toObject()[QStringLiteral("title")].toString(), QStringLiteral("Second"));
    EXPECT_EQ(tracks.at(1).toObject()[QStringLiteral("entryMs")].toDouble(), 184000.0);

    const QJsonArray transitions = root[QStringLiteral("transitions")].toArray();
    ASSERT_EQ(transitions.size(), 1);
    EXPECT_EQ(transitions.at(0).toObject()[QStringLiteral("type")].toString(),
            QStringLiteral("EQ Blend"));
    EXPECT_TRUE(transitions.at(0).toObject()[QStringLiteral("beatSync")].toBool());
}

} // namespace
} // namespace mixxx::music_sync
