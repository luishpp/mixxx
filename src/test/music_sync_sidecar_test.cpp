#include <gtest/gtest.h>

#include <QString>
#include <QTemporaryDir>

#include "music_sync/sidecar_database.h"

namespace {

using mixxx::music_sync::SidecarDatabase;

TEST(MusicSyncSidecarDatabaseTest, MigratesAndPersistsSettings) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("music-sync-dj.sqlite"));

    {
        SidecarDatabase db(dbPath);
        ASSERT_TRUE(db.open());
        ASSERT_TRUE(db.applyMigrations());
        EXPECT_EQ(db.schemaVersion(), SidecarDatabase::kTargetSchemaVersion);

        // Migrations are idempotent.
        ASSERT_TRUE(db.applyMigrations());
        EXPECT_EQ(db.schemaVersion(), SidecarDatabase::kTargetSchemaVersion);

        // Settings round-trip.
        EXPECT_EQ(db.getSetting(QStringLiteral("module_enabled"), QStringLiteral("0")),
                QStringLiteral("0"));
        EXPECT_TRUE(db.setSetting(QStringLiteral("module_enabled"), QStringLiteral("1")));
        EXPECT_EQ(db.getSetting(QStringLiteral("module_enabled"), QStringLiteral("0")),
                QStringLiteral("1"));
        // Upsert overwrites.
        EXPECT_TRUE(db.setSetting(QStringLiteral("module_enabled"), QStringLiteral("0")));
        EXPECT_EQ(db.getSetting(QStringLiteral("module_enabled"), QStringLiteral("1")),
                QStringLiteral("0"));
    }

    // Persistence survives reopening the same file.
    {
        SidecarDatabase db(dbPath);
        ASSERT_TRUE(db.open());
        EXPECT_EQ(db.schemaVersion(), SidecarDatabase::kTargetSchemaVersion);
        EXPECT_EQ(db.getSetting(QStringLiteral("module_enabled"), QStringLiteral("1")),
                QStringLiteral("0"));
    }
}

} // namespace
