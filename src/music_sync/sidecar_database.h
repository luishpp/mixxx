#pragma once

#include <QSqlDatabase>
#include <QString>

namespace mixxx::music_sync {

/// Owns the Music Sync sidecar SQLite database (`music-sync-dj.sqlite`), kept
/// separate from Mixxx's own library database so the module can be added or
/// dropped without touching the Mixxx schema.
///
/// The connection uses a dedicated connection name ("MUSIC_SYNC") and never
/// shares Mixxx's "MIXXX" connection. A failure here must be isolated: it
/// disables the module but never prevents Mixxx from running.
class SidecarDatabase {
  public:
    /// Latest schema version this build knows how to migrate to.
    static constexpr int kTargetSchemaVersion = 9;

    explicit SidecarDatabase(QString filePath);
    ~SidecarDatabase();

    SidecarDatabase(const SidecarDatabase&) = delete;
    SidecarDatabase& operator=(const SidecarDatabase&) = delete;

    /// Opens the database file, creating it if necessary. Returns false on
    /// failure (logged); the caller should then treat the module as disabled.
    bool open();

    /// Applies pending migrations up to kTargetSchemaVersion inside a
    /// transaction. Safe to call repeatedly (idempotent). Returns false on
    /// failure (the transaction is rolled back).
    bool applyMigrations();

    bool isOpen() const {
        return m_database.isOpen();
    }

    /// Current schema version recorded in MusicSyncSchemaMigrations, or 0 if
    /// the database has not been migrated yet.
    int schemaVersion() const;

    QString filePath() const {
        return m_filePath;
    }

    /// The underlying connection, for repositories that run their own queries.
    /// Must be used on the same (GUI) thread that opened it.
    QSqlDatabase database() const {
        return m_database;
    }

    /// Reads a value from the MusicSyncSettings key/value table.
    QString getSetting(const QString& key, const QString& defaultValue = QString()) const;

    /// Writes a value into the MusicSyncSettings key/value table (upsert).
    /// Returns false on failure (logged).
    bool setSetting(const QString& key, const QString& value);

  private:
    bool migrateToV1();
    bool migrateToV2();
    bool migrateToV3();
    bool migrateToV4();
    bool migrateToV5();
    bool migrateToV6();
    bool migrateToV7();
    bool migrateToV8();
    bool migrateToV9();

    const QString m_connectionName;
    QString m_filePath;
    QSqlDatabase m_database;
};

} // namespace mixxx::music_sync
