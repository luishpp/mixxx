#include "music_sync/analysis/analysis_repository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
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

QString curveToJson(const QVector<float>& curve) {
    QJsonArray array;
    for (float value : curve) {
        array.append(value);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<float> curveFromJson(const QString& json) {
    QVector<float> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isArray()) {
        const QJsonArray array = doc.array();
        out.reserve(array.size());
        for (const QJsonValue& value : array) {
            out.append(static_cast<float>(value.toDouble()));
        }
    }
    return out;
}

QString phrasesToJson(const QVector<mixxx::music_sync::PhraseMarker>& phrases) {
    QJsonArray array;
    for (const mixxx::music_sync::PhraseMarker& phrase : phrases) {
        QJsonObject obj;
        obj.insert(QStringLiteral("startMs"), static_cast<double>(phrase.startMs));
        obj.insert(QStringLiteral("bars"), phrase.bars);
        array.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<mixxx::music_sync::PhraseMarker> phrasesFromJson(const QString& json) {
    QVector<mixxx::music_sync::PhraseMarker> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isArray()) {
        const QJsonArray array = doc.array();
        for (const QJsonValue& value : array) {
            const QJsonObject obj = value.toObject();
            mixxx::music_sync::PhraseMarker phrase;
            phrase.startMs =
                    static_cast<std::int64_t>(obj.value(QStringLiteral("startMs")).toDouble());
            phrase.bars = obj.value(QStringLiteral("bars")).toInt();
            out.append(phrase);
        }
    }
    return out;
}

QString sectionsToJson(const QVector<mixxx::music_sync::Section>& sections) {
    QJsonArray array;
    for (const mixxx::music_sync::Section& s : sections) {
        QJsonObject obj;
        obj.insert(QStringLiteral("type"), s.type);
        obj.insert(QStringLiteral("startMs"), static_cast<double>(s.startMs));
        obj.insert(QStringLiteral("endMs"), static_cast<double>(s.endMs));
        obj.insert(QStringLiteral("energy"), s.energy);
        obj.insert(QStringLiteral("confidence"), s.confidence);
        array.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<mixxx::music_sync::Section> sectionsFromJson(const QString& json) {
    QVector<mixxx::music_sync::Section> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isArray()) {
        for (const QJsonValue& value : doc.array()) {
            const QJsonObject obj = value.toObject();
            mixxx::music_sync::Section s;
            s.type = obj.value(QStringLiteral("type")).toString();
            s.startMs = static_cast<std::int64_t>(obj.value(QStringLiteral("startMs")).toDouble());
            s.endMs = static_cast<std::int64_t>(obj.value(QStringLiteral("endMs")).toDouble());
            s.energy = static_cast<float>(obj.value(QStringLiteral("energy")).toDouble());
            s.confidence = static_cast<float>(obj.value(QStringLiteral("confidence")).toDouble());
            out.append(s);
        }
    }
    return out;
}

QString windowsToJson(const QVector<mixxx::music_sync::TransitionWindow>& windows) {
    QJsonArray array;
    for (const mixxx::music_sync::TransitionWindow& w : windows) {
        QJsonObject obj;
        obj.insert(QStringLiteral("kind"), w.kind);
        obj.insert(QStringLiteral("startMs"), static_cast<double>(w.startMs));
        obj.insert(QStringLiteral("endMs"), static_cast<double>(w.endMs));
        obj.insert(QStringLiteral("bars"), w.bars);
        obj.insert(QStringLiteral("energy"), w.energy);
        obj.insert(QStringLiteral("energyStability"), w.energyStability);
        obj.insert(QStringLiteral("instrumentalScore"), w.instrumentalScore);
        obj.insert(QStringLiteral("confidence"), w.confidence);
        array.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<mixxx::music_sync::TransitionWindow> windowsFromJson(const QString& json) {
    QVector<mixxx::music_sync::TransitionWindow> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isArray()) {
        for (const QJsonValue& value : doc.array()) {
            const QJsonObject obj = value.toObject();
            mixxx::music_sync::TransitionWindow w;
            w.kind = obj.value(QStringLiteral("kind")).toString();
            w.startMs = static_cast<std::int64_t>(obj.value(QStringLiteral("startMs")).toDouble());
            w.endMs = static_cast<std::int64_t>(obj.value(QStringLiteral("endMs")).toDouble());
            w.bars = obj.value(QStringLiteral("bars")).toInt();
            w.energy = static_cast<float>(obj.value(QStringLiteral("energy")).toDouble());
            w.energyStability =
                    static_cast<float>(obj.value(QStringLiteral("energyStability")).toDouble());
            w.instrumentalScore =
                    static_cast<float>(obj.value(QStringLiteral("instrumentalScore")).toDouble());
            w.confidence = static_cast<float>(obj.value(QStringLiteral("confidence")).toDouble());
            out.append(w);
        }
    }
    return out;
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
    f.overallEnergy = query.value(idx("overall_energy")).toDouble();
    f.energyCurve = curveFromJson(query.value(idx("energy_curve")).toString());
    f.bassCurve = curveFromJson(query.value(idx("bass_curve")).toString());
    f.phrases = phrasesFromJson(query.value(idx("phrase_markers")).toString());
    f.advancedAnalyzerVersion = query.value(idx("advanced_analyzer_version")).toString();
    f.sections = sectionsFromJson(query.value(idx("sections")).toString());
    f.entryWindows = windowsFromJson(query.value(idx("entry_windows")).toString());
    f.exitWindows = windowsFromJson(query.value(idx("exit_windows")).toString());
    f.snapshotAt = query.value(idx("snapshot_at")).toString();
    return f;
}

const QString kSelectColumns = QStringLiteral(
        "mixxx_track_id, location, file_size, title, artist, album, genre, "
        "duration_ms, sample_rate, channels, bitrate_kbps, bpm, key_chromatic, "
        "key_text, camelot, replaygain_ratio, has_beatgrid, intro_start_ms, "
        "intro_end_ms, outro_start_ms, outro_end_ms, analyzed, analyzer_version, "
        "overall_energy, energy_curve, bass_curve, phrase_markers, "
        "advanced_analyzer_version, sections, entry_windows, exit_windows, "
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
            "  overall_energy, energy_curve, bass_curve, phrase_markers,"
            "  advanced_analyzer_version, sections, entry_windows, exit_windows,"
            "  snapshot_at) "
            "VALUES ("
            "  :id, :location, :file_size, :title, :artist, :album, :genre,"
            "  :duration_ms, :sample_rate, :channels, :bitrate_kbps, :bpm, :key_chromatic,"
            "  :key_text, :camelot, :replaygain_ratio, :has_beatgrid, :intro_start_ms,"
            "  :intro_end_ms, :outro_start_ms, :outro_end_ms, :analyzed, :analyzer_version,"
            "  :overall_energy, :energy_curve, :bass_curve, :phrase_markers,"
            "  :advanced_analyzer_version, :sections, :entry_windows, :exit_windows,"
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
            "  analyzer_version=excluded.analyzer_version,"
            "  overall_energy=excluded.overall_energy, energy_curve=excluded.energy_curve,"
            "  bass_curve=excluded.bass_curve, phrase_markers=excluded.phrase_markers,"
            "  advanced_analyzer_version=excluded.advanced_analyzer_version,"
            "  sections=excluded.sections, entry_windows=excluded.entry_windows,"
            "  exit_windows=excluded.exit_windows,"
            "  snapshot_at=excluded.snapshot_at"));

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
    query.bindValue(QStringLiteral(":overall_energy"), f.overallEnergy);
    query.bindValue(QStringLiteral(":energy_curve"), curveToJson(f.energyCurve));
    query.bindValue(QStringLiteral(":bass_curve"), curveToJson(f.bassCurve));
    query.bindValue(QStringLiteral(":phrase_markers"), phrasesToJson(f.phrases));
    query.bindValue(QStringLiteral(":advanced_analyzer_version"), f.advancedAnalyzerVersion);
    query.bindValue(QStringLiteral(":sections"), sectionsToJson(f.sections));
    query.bindValue(QStringLiteral(":entry_windows"), windowsToJson(f.entryWindows));
    query.bindValue(QStringLiteral(":exit_windows"), windowsToJson(f.exitWindows));

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
