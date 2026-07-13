#include "music_sync/analysis/analysis_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync");

QVariant toVariant(const std::optional<std::int64_t>& value) {
    return value ? QVariant(static_cast<qlonglong>(*value)) : QVariant();
}

std::optional<std::int64_t> readOptionalMs(const QSqlQuery& query, int index) {
    if (index < 0) {
        return std::nullopt;
    }
    const QVariant v = query.value(index);
    if (v.isNull()) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(v.toLongLong());
}

mixxx::music_sync::TrackFeatures readRow(const QSqlQuery& query) {
    // QSqlQuery::value() only takes an int index; resolve names via the record.
    const QSqlRecord record = query.record();
    const auto idx = [&record](const char* name) {
        return record.indexOf(QLatin1String(name));
    };
    mixxx::music_sync::TrackFeatures f;
    f.mixxxTrackId = query.value(idx("mixxx_track_id")).toLongLong();
    f.location = query.value(idx("location")).toString();
    f.fileSize = query.value(idx("file_size")).toLongLong();
    f.title = query.value(idx("title")).toString();
    f.artist = query.value(idx("artist")).toString();
    f.album = query.value(idx("album")).toString();
    f.genre = query.value(idx("genre")).toString();
    f.durationMs = query.value(idx("duration_ms")).toLongLong();
    f.sampleRate = query.value(idx("sample_rate")).toInt();
    f.channels = query.value(idx("channels")).toInt();
    f.bitrateKbps = query.value(idx("bitrate_kbps")).toInt();
    f.bpm = query.value(idx("bpm")).toDouble();
    f.keyChromatic = query.value(idx("key_chromatic")).toInt();
    f.keyText = query.value(idx("key_text")).toString();
    f.camelot = query.value(idx("camelot")).toString();
    f.replaygainRatio = query.value(idx("replaygain_ratio")).toDouble();
    f.hasBeatgrid = query.value(idx("has_beatgrid")).toInt() != 0;
    f.introStartMs = readOptionalMs(query, idx("intro_start_ms"));
    f.introEndMs = readOptionalMs(query, idx("intro_end_ms"));
    f.outroStartMs = readOptionalMs(query, idx("outro_start_ms"));
    f.outroEndMs = readOptionalMs(query, idx("outro_end_ms"));
    f.analyzed = query.value(idx("analyzed")).toInt() != 0;
    f.analyzerVersion = query.value(idx("analyzer_version")).toString();
    f.snapshotAt = query.value(idx("snapshot_at")).toString();
    return f;
}

const QString kSelectColumns = QStringLiteral(
        "mixxx_track_id, location, file_size, title, artist, album, genre, "
        "duration_ms, sample_rate, channels, bitrate_kbps, bpm, key_chromatic, "
        "key_text, camelot, replaygain_ratio, has_beatgrid, intro_start_ms, "
        "intro_end_ms, outro_start_ms, outro_end_ms, analyzed, analyzer_version, "
        "snapshot_at");
} // anonymous namespace

namespace mixxx::music_sync {

AnalysisRepository::AnalysisRepository(QSqlDatabase database)
        : m_database(std::move(database)) {
}

bool AnalysisRepository::upsert(const TrackFeatures& f) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "INSERT INTO MusicSyncTrackFeatures ("
            "  mixxx_track_id, location, file_size, title, artist, album, genre,"
            "  duration_ms, sample_rate, channels, bitrate_kbps, bpm, key_chromatic,"
            "  key_text, camelot, replaygain_ratio, has_beatgrid, intro_start_ms,"
            "  intro_end_ms, outro_start_ms, outro_end_ms, analyzed, analyzer_version,"
            "  snapshot_at) "
            "VALUES ("
            "  :id, :location, :file_size, :title, :artist, :album, :genre,"
            "  :duration_ms, :sample_rate, :channels, :bitrate_kbps, :bpm, :key_chromatic,"
            "  :key_text, :camelot, :replaygain_ratio, :has_beatgrid, :intro_start_ms,"
            "  :intro_end_ms, :outro_start_ms, :outro_end_ms, :analyzed, :analyzer_version,"
            "  datetime('now')) "
            "ON CONFLICT(mixxx_track_id) DO UPDATE SET "
            "  location=excluded.location, file_size=excluded.file_size, title=excluded.title,"
            "  artist=excluded.artist, album=excluded.album, genre=excluded.genre,"
            "  duration_ms=excluded.duration_ms, sample_rate=excluded.sample_rate,"
            "  channels=excluded.channels, bitrate_kbps=excluded.bitrate_kbps, bpm=excluded.bpm,"
            "  key_chromatic=excluded.key_chromatic, key_text=excluded.key_text,"
            "  camelot=excluded.camelot, replaygain_ratio=excluded.replaygain_ratio,"
            "  has_beatgrid=excluded.has_beatgrid, intro_start_ms=excluded.intro_start_ms,"
            "  intro_end_ms=excluded.intro_end_ms, outro_start_ms=excluded.outro_start_ms,"
            "  outro_end_ms=excluded.outro_end_ms, analyzed=excluded.analyzed,"
            "  analyzer_version=excluded.analyzer_version, snapshot_at=excluded.snapshot_at"));

    query.bindValue(QStringLiteral(":id"), static_cast<qlonglong>(f.mixxxTrackId));
    query.bindValue(QStringLiteral(":location"), f.location);
    query.bindValue(QStringLiteral(":file_size"), static_cast<qlonglong>(f.fileSize));
    query.bindValue(QStringLiteral(":title"), f.title);
    query.bindValue(QStringLiteral(":artist"), f.artist);
    query.bindValue(QStringLiteral(":album"), f.album);
    query.bindValue(QStringLiteral(":genre"), f.genre);
    query.bindValue(QStringLiteral(":duration_ms"), static_cast<qlonglong>(f.durationMs));
    query.bindValue(QStringLiteral(":sample_rate"), f.sampleRate);
    query.bindValue(QStringLiteral(":channels"), f.channels);
    query.bindValue(QStringLiteral(":bitrate_kbps"), f.bitrateKbps);
    query.bindValue(QStringLiteral(":bpm"), f.bpm);
    query.bindValue(QStringLiteral(":key_chromatic"), f.keyChromatic);
    query.bindValue(QStringLiteral(":key_text"), f.keyText);
    query.bindValue(QStringLiteral(":camelot"), f.camelot);
    query.bindValue(QStringLiteral(":replaygain_ratio"), f.replaygainRatio);
    query.bindValue(QStringLiteral(":has_beatgrid"), f.hasBeatgrid ? 1 : 0);
    query.bindValue(QStringLiteral(":intro_start_ms"), toVariant(f.introStartMs));
    query.bindValue(QStringLiteral(":intro_end_ms"), toVariant(f.introEndMs));
    query.bindValue(QStringLiteral(":outro_start_ms"), toVariant(f.outroStartMs));
    query.bindValue(QStringLiteral(":outro_end_ms"), toVariant(f.outroEndMs));
    query.bindValue(QStringLiteral(":analyzed"), f.analyzed ? 1 : 0);
    query.bindValue(QStringLiteral(":analyzer_version"), f.analyzerVersion);

    if (!query.exec()) {
        kLogger.warning() << "Could not upsert track features for id" << f.mixxxTrackId
                          << "->" << query.lastError();
        return false;
    }
    return true;
}

QVector<TrackFeatures> AnalysisRepository::loadAll() const {
    QVector<TrackFeatures> result;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT %1 FROM MusicSyncTrackFeatures "
                                   "ORDER BY artist COLLATE NOCASE, title COLLATE NOCASE")
                            .arg(kSelectColumns))) {
        kLogger.warning() << "Could not load track features:" << query.lastError();
        return result;
    }
    while (query.next()) {
        result.append(readRow(query));
    }
    return result;
}

std::optional<TrackFeatures> AnalysisRepository::loadByTrackId(std::int64_t mixxxTrackId) const {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT %1 FROM MusicSyncTrackFeatures "
                                 "WHERE mixxx_track_id = :id")
                          .arg(kSelectColumns));
    query.bindValue(QStringLiteral(":id"), static_cast<qlonglong>(mixxxTrackId));
    if (query.exec() && query.next()) {
        return readRow(query);
    }
    return std::nullopt;
}

int AnalysisRepository::count() const {
    QSqlQuery query(m_database);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM MusicSyncTrackFeatures")) &&
            query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

} // namespace mixxx::music_sync
