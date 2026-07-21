#include <gtest/gtest.h>

#include <QString>
#include <QTemporaryDir>

#include "music_sync/analysis/analysis_repository.h"
#include "music_sync/analysis/override_repository.h"
#include "music_sync/domain/track_features.h"
#include "music_sync/sidecar_database.h"

namespace {

using mixxx::music_sync::AnalysisRepository;
using mixxx::music_sync::OverrideRepository;
using mixxx::music_sync::PairKey;
using mixxx::music_sync::TransitionOverride;
using mixxx::music_sync::TransitionType;
using mixxx::music_sync::SidecarDatabase;
using mixxx::music_sync::TrackFeatures;

TrackFeatures makeFeatures(std::int64_t id) {
    TrackFeatures f;
    f.mixxxTrackId = id;
    f.location = QStringLiteral("C:/music/track.mp3");
    f.fileSize = 12345;
    f.title = QStringLiteral("Weightless");
    f.artist = QStringLiteral("Nosi");
    f.album = QStringLiteral("Album");
    f.genre = QStringLiteral("Melodic House");
    f.durationMs = 421350;
    f.sampleRate = 44100;
    f.channels = 2;
    f.bitrateKbps = 320;
    f.bpm = 124.2;
    f.keyChromatic = 22; // A minor
    f.keyText = QStringLiteral("Am");
    f.camelot = QStringLiteral("8A");
    f.replaygainRatio = 0.5;
    f.hasBeatgrid = true;
    f.introStartMs = 0;
    f.introEndMs = 32000;
    // outro cues intentionally left unset (nullopt)
    f.analyzed = true;
    f.analyzerVersion = QStringLiteral("native-0.1.0");
    return f;
}

TEST(MusicSyncAnalysisRepositoryTest, UpsertLoadRoundTrip) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    ASSERT_EQ(db.schemaVersion(), SidecarDatabase::kTargetSchemaVersion);

    AnalysisRepository repo(db.database());
    EXPECT_EQ(repo.count(), 0);

    const TrackFeatures original = makeFeatures(42);
    ASSERT_TRUE(repo.upsert(original));
    EXPECT_EQ(repo.count(), 1);

    const auto loaded = repo.loadByTrackId(42);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->mixxxTrackId, 42);
    EXPECT_EQ(loaded->artist, QStringLiteral("Nosi"));
    EXPECT_EQ(loaded->title, QStringLiteral("Weightless"));
    EXPECT_DOUBLE_EQ(loaded->bpm, 124.2);
    EXPECT_EQ(loaded->camelot, QStringLiteral("8A"));
    EXPECT_EQ(loaded->keyChromatic, 22);
    EXPECT_EQ(loaded->durationMs, 421350);
    EXPECT_TRUE(loaded->hasBeatgrid);
    EXPECT_TRUE(loaded->analyzed);
    ASSERT_TRUE(loaded->introStartMs.has_value());
    EXPECT_EQ(*loaded->introStartMs, 0);
    ASSERT_TRUE(loaded->introEndMs.has_value());
    EXPECT_EQ(*loaded->introEndMs, 32000);
    EXPECT_FALSE(loaded->outroStartMs.has_value());
    EXPECT_FALSE(loaded->outroEndMs.has_value());

    // Upserting the same id updates in place (no duplicate row).
    TrackFeatures updated = original;
    updated.bpm = 126.0;
    updated.camelot = QStringLiteral("9A");
    updated.analyzed = false;
    ASSERT_TRUE(repo.upsert(updated));
    EXPECT_EQ(repo.count(), 1);
    const auto reloaded = repo.loadByTrackId(42);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_DOUBLE_EQ(reloaded->bpm, 126.0);
    EXPECT_EQ(reloaded->camelot, QStringLiteral("9A"));
    EXPECT_FALSE(reloaded->analyzed);

    // A second distinct track is stored and returned by loadAll().
    ASSERT_TRUE(repo.upsert(makeFeatures(7)));
    EXPECT_EQ(repo.count(), 2);
    EXPECT_EQ(repo.loadAll().size(), 2);

    // Unknown id yields nullopt.
    EXPECT_FALSE(repo.loadByTrackId(9999).has_value());
}

TEST(MusicSyncAnalysisRepositoryTest, RemoveMissingPrunesOrphansOnly) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    AnalysisRepository repo(db.database());

    ASSERT_TRUE(repo.upsert(makeFeatures(1)));
    ASSERT_TRUE(repo.upsert(makeFeatures(2)));
    ASSERT_TRUE(repo.upsert(makeFeatures(3)));
    ASSERT_EQ(repo.count(), 3);

    // Track 2 is gone from the library: only its snapshot is dropped.
    EXPECT_EQ(repo.removeMissing(QVector<std::int64_t>({1, 3})), 1);
    EXPECT_EQ(repo.count(), 2);
    EXPECT_TRUE(repo.loadByTrackId(1).has_value());
    EXPECT_FALSE(repo.loadByTrackId(2).has_value());
    EXPECT_TRUE(repo.loadByTrackId(3).has_value());

    // Idempotent: nothing left to prune.
    EXPECT_EQ(repo.removeMissing(QVector<std::int64_t>({1, 3})), 0);
    EXPECT_EQ(repo.count(), 2);

    // Ids the sidecar has never seen do not resurrect or remove anything.
    EXPECT_EQ(repo.removeMissing(QVector<std::int64_t>({1, 3, 42})), 0);
    EXPECT_EQ(repo.count(), 2);
}

TEST(MusicSyncAnalysisRepositoryTest, RemoveMissingWithEmptyLiveSetDropsEverything) {
    // Documents the sharp edge the caller must respect: an empty live set means
    // "the library is empty", so it prunes all. The controller therefore skips
    // pruning when the library query fails, instead of passing an empty list.
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    AnalysisRepository repo(db.database());

    ASSERT_TRUE(repo.upsert(makeFeatures(1)));
    ASSERT_TRUE(repo.upsert(makeFeatures(2)));
    EXPECT_EQ(repo.removeMissing(QVector<std::int64_t>()), 2);
    EXPECT_EQ(repo.count(), 0);
}

TEST(MusicSyncAnalysisRepositoryTest, ClearRemovesAllSnapshots) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    AnalysisRepository repo(db.database());

    ASSERT_TRUE(repo.upsert(makeFeatures(1)));
    ASSERT_TRUE(repo.upsert(makeFeatures(2)));
    ASSERT_EQ(repo.count(), 2);

    EXPECT_EQ(repo.clear(), 2);
    EXPECT_EQ(repo.count(), 0);
    EXPECT_TRUE(repo.loadAll().isEmpty());

    // Clearing an empty sidecar is a no-op, not an error.
    EXPECT_EQ(repo.clear(), 0);
    EXPECT_EQ(repo.count(), 0);
}

TEST(MusicSyncOverrideRepositoryTest, RoundTripsAPairChoice) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    OverrideRepository repo(db.database());

    PairKey key;
    key.sourceTrackId = 10;
    key.targetTrackId = 11;
    TransitionOverride override;
    override.type = TransitionType::BassSwap;
    override.bars = 64;
    ASSERT_TRUE(repo.save(key, override));

    const auto loaded = repo.loadAll();
    ASSERT_TRUE(loaded.contains(key));
    EXPECT_EQ(*loaded.value(key).type, TransitionType::BassSwap);
    EXPECT_EQ(*loaded.value(key).bars, 64);
}

TEST(MusicSyncOverrideRepositoryTest, RoundTripsExitAndEntryPoints) {
    // The two handover points are per-pair, independent, and must survive a
    // reload: the exit (where this track leaves) and the entry (where the next
    // one comes in, skipping a long intro).
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    OverrideRepository repo(db.database());

    PairKey key{20, 21};
    TransitionOverride override;
    override.sourceExitMs = 185000;
    override.targetEntryMs = 48000;
    ASSERT_TRUE(repo.save(key, override));

    const auto loaded = repo.loadAll();
    ASSERT_TRUE(loaded.contains(key));
    ASSERT_TRUE(loaded.value(key).sourceExitMs.has_value());
    ASSERT_TRUE(loaded.value(key).targetEntryMs.has_value());
    EXPECT_EQ(*loaded.value(key).sourceExitMs, 185000);
    EXPECT_EQ(*loaded.value(key).targetEntryMs, 48000);
    // Entry alone is not enough to keep type/bars from being automatic.
    EXPECT_FALSE(loaded.value(key).type.has_value());
    EXPECT_FALSE(loaded.value(key).bars.has_value());
}

TEST(MusicSyncOverrideRepositoryTest, RoundTripsForceBeatSync) {
    // The tempo-lock veto is tri-state: unset (planner decides), true (force on),
    // false (force off). All three must survive a reload distinctly.
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    OverrideRepository repo(db.database());

    PairKey forceOn{1, 2};
    TransitionOverride on;
    on.forceBeatSync = true;
    ASSERT_TRUE(repo.save(forceOn, on));
    PairKey forceOff{3, 4};
    TransitionOverride off;
    off.forceBeatSync = false;
    ASSERT_TRUE(repo.save(forceOff, off));

    const auto loaded = repo.loadAll();
    ASSERT_TRUE(loaded.value(forceOn).forceBeatSync.has_value());
    EXPECT_TRUE(*loaded.value(forceOn).forceBeatSync);
    ASSERT_TRUE(loaded.value(forceOff).forceBeatSync.has_value());
    EXPECT_FALSE(*loaded.value(forceOff).forceBeatSync);
    // A pair never touched stays unset (automatic).
    EXPECT_FALSE(loaded.value(PairKey{5, 6}).forceBeatSync.has_value());
}

TEST(MusicSyncOverrideRepositoryTest, EitherFieldAloneIsAValidChoice) {
    // "Bass Swap, you pick the length" and "however you like, but 64 bars" are
    // both real answers, so each field is independently optional.
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    OverrideRepository repo(db.database());

    PairKey typeOnly{1, 2};
    TransitionOverride a;
    a.type = TransitionType::CutOnPhrase;
    ASSERT_TRUE(repo.save(typeOnly, a));

    PairKey barsOnly{3, 4};
    TransitionOverride b;
    b.bars = 16;
    ASSERT_TRUE(repo.save(barsOnly, b));

    const auto loaded = repo.loadAll();
    EXPECT_TRUE(loaded.value(typeOnly).type.has_value());
    EXPECT_FALSE(loaded.value(typeOnly).bars.has_value());
    EXPECT_FALSE(loaded.value(barsOnly).type.has_value());
    EXPECT_TRUE(loaded.value(barsOnly).bars.has_value());
}

TEST(MusicSyncOverrideRepositoryTest, BackToAutomaticRemovesTheChoice) {
    // "Automatic" is the absence of a decision, not a decision to store.
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    OverrideRepository repo(db.database());

    PairKey key{5, 6};
    TransitionOverride override;
    override.bars = 32;
    ASSERT_TRUE(repo.save(key, override));
    ASSERT_TRUE(repo.loadAll().contains(key));

    ASSERT_TRUE(repo.save(key, TransitionOverride())); // back to automatic
    EXPECT_FALSE(repo.loadAll().contains(key));
}

TEST(MusicSyncOverrideRepositoryTest, ChoicesFollowThePairNotThePosition) {
    // The whole reason for keying by pair: reordering the set must not move a
    // decision onto a different pair.
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    OverrideRepository repo(db.database());

    PairKey forward{1, 2};
    PairKey reversed{2, 1}; // the same tracks, the other way round: not the same
    TransitionOverride override;
    override.bars = 64;
    ASSERT_TRUE(repo.save(forward, override));

    const auto loaded = repo.loadAll();
    EXPECT_TRUE(loaded.contains(forward));
    EXPECT_FALSE(loaded.contains(reversed));
}

TEST(MusicSyncOverrideRepositoryTest, ResetAllForgetsPairsAndActRules) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SidecarDatabase db(tempDir.filePath(QStringLiteral("music-sync-dj.sqlite")));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.applyMigrations());
    OverrideRepository repo(db.database());

    TransitionOverride pairChoice;
    pairChoice.type = TransitionType::BassSwap;
    ASSERT_TRUE(repo.save(PairKey{1, 2}, pairChoice));
    TransitionOverride actRule;
    actRule.bars = 16;
    ASSERT_TRUE(repo.saveActRule(1, actRule));
    ASSERT_FALSE(repo.loadAll().isEmpty());
    ASSERT_FALSE(repo.loadActRules().isEmpty());

    EXPECT_EQ(repo.resetAll(), 2); // one pair + one act
    EXPECT_TRUE(repo.loadAll().isEmpty());
    EXPECT_TRUE(repo.loadActRules().isEmpty());

    EXPECT_EQ(repo.resetAll(), 0); // idempotent
}

} // namespace
