#include "music_sync/sidecar_database.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync");

// A single, fixed connection name so the sidecar never collides with Mixxx's
// own "MIXXX" library connection.
const QString kConnectionName = QStringLiteral("MUSIC_SYNC");

bool execStatement(QSqlDatabase& db, const QString& sql) {
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        kLogger.warning() << "SQL failed:" << sql << "->" << query.lastError();
        return false;
    }
    return true;
}
} // anonymous namespace

namespace mixxx::music_sync {

SidecarDatabase::SidecarDatabase(QString filePath)
        : m_connectionName(kConnectionName),
          m_filePath(std::move(filePath)) {
}

SidecarDatabase::~SidecarDatabase() {
    if (m_database.isOpen()) {
        m_database.close();
    }
    // Drop our own reference before removing the connection, otherwise Qt warns
    // that the connection is still in use (same approach as Mixxx's DbConnection).
    const QString connectionName = m_connectionName;
    m_database = QSqlDatabase();
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::removeDatabase(connectionName);
    }
}

bool SidecarDatabase::open() {
    if (m_database.isOpen()) {
        return true;
    }
    if (QSqlDatabase::contains(m_connectionName)) {
        m_database = QSqlDatabase::database(m_connectionName);
    } else {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    }
    m_database.setDatabaseName(m_filePath);
    if (!m_database.open()) {
        kLogger.warning() << "Failed to open sidecar database at" << m_filePath
                          << "->" << m_database.lastError();
        return false;
    }
    kLogger.info() << "Opened sidecar database at" << m_filePath;
    return true;
}

int SidecarDatabase::schemaVersion() const {
    if (!m_database.isOpen()) {
        return 0;
    }
    QSqlQuery query(m_database);
    // Missing table simply means "not migrated yet".
    if (!query.exec(QStringLiteral(
                "SELECT MAX(version) FROM MusicSyncSchemaMigrations"))) {
        return 0;
    }
    if (query.first() && !query.value(0).isNull()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool SidecarDatabase::applyMigrations() {
    if (!m_database.isOpen()) {
        kLogger.warning() << "applyMigrations called on a closed database";
        return false;
    }

    // The migrations bookkeeping table must exist before we can read the version.
    if (!execStatement(m_database,
                QStringLiteral("CREATE TABLE IF NOT EXISTS MusicSyncSchemaMigrations ("
                               "  version INTEGER PRIMARY KEY,"
                               "  applied_at TEXT NOT NULL DEFAULT (datetime('now'))"
                               ")"))) {
        return false;
    }

    const int current = schemaVersion();
    if (current >= kTargetSchemaVersion) {
        return true;
    }

    if (!m_database.transaction()) {
        kLogger.warning() << "Could not begin migration transaction:"
                          << m_database.lastError();
        return false;
    }

    for (int version = current + 1; version <= kTargetSchemaVersion; ++version) {
        bool ok = false;
        switch (version) {
        case 1:
            ok = migrateToV1();
            break;
        case 2:
            ok = migrateToV2();
            break;
        case 3:
            ok = migrateToV3();
            break;
        case 4:
            ok = migrateToV4();
            break;
        case 5:
            ok = migrateToV5();
            break;
        case 6:
            ok = migrateToV6();
            break;
        case 7:
            ok = migrateToV7();
            break;
        default:
            kLogger.warning() << "No migration defined for version" << version;
            ok = false;
            break;
        }

        if (!ok) {
            m_database.rollback();
            kLogger.warning() << "Migration to version" << version
                              << "failed; rolled back";
            return false;
        }

        QSqlQuery stamp(m_database);
        stamp.prepare(QStringLiteral(
                "INSERT INTO MusicSyncSchemaMigrations (version) VALUES (:version)"));
        stamp.bindValue(QStringLiteral(":version"), version);
        if (!stamp.exec()) {
            m_database.rollback();
            kLogger.warning() << "Could not record migration version" << version
                              << "->" << stamp.lastError();
            return false;
        }
    }

    if (!m_database.commit()) {
        kLogger.warning() << "Could not commit migrations:" << m_database.lastError();
        m_database.rollback();
        return false;
    }

    kLogger.info() << "Sidecar schema migrated to version" << kTargetSchemaVersion;
    return true;
}

bool SidecarDatabase::migrateToV1() {
    // Minimal key/value store for module settings. Feature tables (track
    // features, projects, arrangements, etc.) are added in later phases.
    return execStatement(m_database,
            QStringLiteral("CREATE TABLE IF NOT EXISTS MusicSyncSettings ("
                           "  key TEXT PRIMARY KEY,"
                           "  value TEXT"
                           ")"));
}

bool SidecarDatabase::migrateToV2() {
    // Snapshot of each track's native (Mixxx-provided) analysis data. Keyed by
    // the Mixxx library track id; BPM/key/beats remain owned by Mixxx, this is
    // only a cached snapshot plus our derived fields (Camelot, analyzed flag).
    return execStatement(m_database,
            QStringLiteral("CREATE TABLE IF NOT EXISTS MusicSyncTrackFeatures ("
                           "  mixxx_track_id   INTEGER PRIMARY KEY,"
                           "  location         TEXT,"
                           "  file_size        INTEGER,"
                           "  title            TEXT,"
                           "  artist           TEXT,"
                           "  album            TEXT,"
                           "  genre            TEXT,"
                           "  duration_ms      INTEGER,"
                           "  sample_rate      INTEGER,"
                           "  channels         INTEGER,"
                           "  bitrate_kbps     INTEGER,"
                           "  bpm              REAL,"
                           "  key_chromatic    INTEGER,"
                           "  key_text         TEXT,"
                           "  camelot          TEXT,"
                           "  replaygain_ratio REAL,"
                           "  has_beatgrid     INTEGER,"
                           "  intro_start_ms   INTEGER,"
                           "  intro_end_ms     INTEGER,"
                           "  outro_start_ms   INTEGER,"
                           "  outro_end_ms     INTEGER,"
                           "  analyzed         INTEGER,"
                           "  analyzer_version TEXT,"
                           "  snapshot_at      TEXT NOT NULL DEFAULT (datetime('now'))"
                           ")"));
}

bool SidecarDatabase::migrateToV3() {
    // Advanced analysis fields (energy/bass curves + phrase markers), stored as
    // JSON alongside the native snapshot. Added as columns to preserve v2 rows.
    for (const QString& statement : {
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN overall_energy REAL"),
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN energy_curve TEXT"),
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN bass_curve TEXT"),
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN phrase_markers TEXT"),
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN advanced_analyzer_version TEXT"),
         }) {
        if (!execStatement(m_database, statement)) {
            return false;
        }
    }
    return true;
}

bool SidecarDatabase::migrateToV4() {
    // Structural sections and candidate transition windows, stored as JSON.
    for (const QString& statement : {
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN sections TEXT"),
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN entry_windows TEXT"),
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN exit_windows TEXT"),
         }) {
        if (!execStatement(m_database, statement)) {
            return false;
        }
    }
    return true;
}

bool SidecarDatabase::migrateToV7() {
    // The DJ's per-pair transition choices (RF-010). Keyed by the PAIR, never by
    // position: regenerating the sequence reorders positions, and an override
    // pinned to a position would silently land on a different pair.
    return execStatement(m_database,
            QStringLiteral("CREATE TABLE IF NOT EXISTS MusicSyncTransitionOverrides ("
                           "  source_track_id INTEGER NOT NULL,"
                           "  target_track_id INTEGER NOT NULL,"
                           "  transition_type INTEGER,"   // NULL = planner decides
                           "  bars INTEGER,"              // NULL = planner decides
                           "  updated_at TEXT,"
                           "  PRIMARY KEY (source_track_id, target_track_id))"));
}

bool SidecarDatabase::migrateToV6() {
    // The plan's track number ("05", "33.1"): the track's canonical place inside
    // its act, which is what pins an anchor.
    return execStatement(m_database,
            QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                           "ADD COLUMN track_number TEXT"));
}

bool SidecarDatabase::migrateToV5() {
    // The track's place in the set narrative, parsed from its comment tag.
    for (const QString& statement : {
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN act INTEGER DEFAULT 0"),
                 QStringLiteral("ALTER TABLE MusicSyncTrackFeatures "
                                "ADD COLUMN set_function TEXT"),
         }) {
        if (!execStatement(m_database, statement)) {
            return false;
        }
    }
    return true;
}

QString SidecarDatabase::getSetting(const QString& key, const QString& defaultValue) const {
    if (!m_database.isOpen()) {
        return defaultValue;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT value FROM MusicSyncSettings WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), key);
    if (query.exec() && query.first()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

bool SidecarDatabase::setSetting(const QString& key, const QString& value) {
    if (!m_database.isOpen()) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "INSERT INTO MusicSyncSettings (key, value) VALUES (:key, :value) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.bindValue(QStringLiteral(":key"), key);
    query.bindValue(QStringLiteral(":value"), value);
    if (!query.exec()) {
        kLogger.warning() << "Could not save setting" << key << "->" << query.lastError();
        return false;
    }
    return true;
}

} // namespace mixxx::music_sync
