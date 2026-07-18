#include "music_sync/analysis/override_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync");
} // anonymous namespace

namespace mixxx::music_sync {

OverrideRepository::OverrideRepository(QSqlDatabase database)
        : m_database(std::move(database)) {
}

QHash<PairKey, TransitionOverride> OverrideRepository::loadAll() const {
    QHash<PairKey, TransitionOverride> out;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
                "SELECT source_track_id, target_track_id, transition_type, bars, "
                "       source_exit_ms FROM MusicSyncTransitionOverrides"))) {
        kLogger.warning() << "Could not load transition overrides:" << query.lastError();
        return out;
    }
    while (query.next()) {
        PairKey key;
        key.sourceTrackId = query.value(0).toLongLong();
        key.targetTrackId = query.value(1).toLongLong();
        TransitionOverride override;
        // NULL means "the planner decides", which is different from a stored 0.
        if (!query.value(2).isNull()) {
            override.type = static_cast<TransitionType>(query.value(2).toInt());
        }
        if (!query.value(3).isNull()) {
            override.bars = query.value(3).toInt();
        }
        if (!query.value(4).isNull()) {
            override.sourceExitMs = query.value(4).toLongLong();
        }
        if (!override.isEmpty()) {
            out.insert(key, override);
        }
    }
    return out;
}

bool OverrideRepository::save(const PairKey& key, const TransitionOverride& override) {
    if (override.isEmpty()) {
        // Back to automatic: drop the row rather than store an empty decision.
        QSqlQuery del(m_database);
        del.prepare(QStringLiteral("DELETE FROM MusicSyncTransitionOverrides "
                                   "WHERE source_track_id = :source AND "
                                   "      target_track_id = :target"));
        del.bindValue(QStringLiteral(":source"), static_cast<qlonglong>(key.sourceTrackId));
        del.bindValue(QStringLiteral(":target"), static_cast<qlonglong>(key.targetTrackId));
        if (!del.exec()) {
            kLogger.warning() << "Could not clear override:" << del.lastError();
            return false;
        }
        return true;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "INSERT INTO MusicSyncTransitionOverrides ("
            "  source_track_id, target_track_id, transition_type, bars, source_exit_ms,"
            "  updated_at) "
            "VALUES (:source, :target, :type, :bars, :exit, datetime('now')) "
            "ON CONFLICT(source_track_id, target_track_id) DO UPDATE SET "
            "  transition_type = excluded.transition_type,"
            "  bars = excluded.bars, source_exit_ms = excluded.source_exit_ms,"
            "  updated_at = excluded.updated_at"));
    query.bindValue(QStringLiteral(":source"), static_cast<qlonglong>(key.sourceTrackId));
    query.bindValue(QStringLiteral(":target"), static_cast<qlonglong>(key.targetTrackId));
    query.bindValue(QStringLiteral(":type"),
            override.type ? QVariant(static_cast<int>(*override.type)) : QVariant());
    query.bindValue(QStringLiteral(":bars"),
            override.bars ? QVariant(*override.bars) : QVariant());
    query.bindValue(QStringLiteral(":exit"),
            override.sourceExitMs ? QVariant(static_cast<qlonglong>(*override.sourceExitMs))
                                  : QVariant());
    if (!query.exec()) {
        kLogger.warning() << "Could not save override:" << query.lastError();
        return false;
    }
    return true;
}

QHash<int, TransitionOverride> OverrideRepository::loadActRules() const {
    QHash<int, TransitionOverride> out;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
                "SELECT act, transition_type, bars FROM MusicSyncActOverrides"))) {
        kLogger.warning() << "Could not load act rules:" << query.lastError();
        return out;
    }
    while (query.next()) {
        TransitionOverride rule;
        if (!query.value(1).isNull()) {
            rule.type = static_cast<TransitionType>(query.value(1).toInt());
        }
        if (!query.value(2).isNull()) {
            rule.bars = query.value(2).toInt();
        }
        if (!rule.isEmpty()) {
            out.insert(query.value(0).toInt(), rule);
        }
    }
    return out;
}

bool OverrideRepository::saveActRule(int act, const TransitionOverride& rule) {
    if (rule.isEmpty()) {
        QSqlQuery del(m_database);
        del.prepare(QStringLiteral("DELETE FROM MusicSyncActOverrides WHERE act = :act"));
        del.bindValue(QStringLiteral(":act"), act);
        if (!del.exec()) {
            kLogger.warning() << "Could not clear act rule:" << del.lastError();
            return false;
        }
        return true;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "INSERT INTO MusicSyncActOverrides (act, transition_type, bars, updated_at) "
            "VALUES (:act, :type, :bars, datetime('now')) "
            "ON CONFLICT(act) DO UPDATE SET transition_type = excluded.transition_type,"
            "  bars = excluded.bars, updated_at = excluded.updated_at"));
    query.bindValue(QStringLiteral(":act"), act);
    query.bindValue(QStringLiteral(":type"),
            rule.type ? QVariant(static_cast<int>(*rule.type)) : QVariant());
    query.bindValue(QStringLiteral(":bars"), rule.bars ? QVariant(*rule.bars) : QVariant());
    if (!query.exec()) {
        kLogger.warning() << "Could not save act rule:" << query.lastError();
        return false;
    }
    return true;
}

TransitionOverride OverrideRepository::resolve(
        const QHash<PairKey, TransitionOverride>& pairs,
        const QHash<int, TransitionOverride>& acts,
        const PairKey& key,
        int act) {
    // The pair only states what it disagrees with its act about.
    return pairs.value(key).layeredOver(acts.value(act));
}

int OverrideRepository::clear() {
    QSqlQuery count(m_database);
    int before = 0;
    if (count.exec(QStringLiteral("SELECT COUNT(*) FROM MusicSyncTransitionOverrides")) &&
            count.next()) {
        before = count.value(0).toInt();
    }
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("DELETE FROM MusicSyncTransitionOverrides"))) {
        kLogger.warning() << "Could not clear overrides:" << query.lastError();
        return -1;
    }
    return before;
}

int OverrideRepository::clearActRules() {
    QSqlQuery count(m_database);
    int before = 0;
    if (count.exec(QStringLiteral("SELECT COUNT(*) FROM MusicSyncActOverrides")) &&
            count.next()) {
        before = count.value(0).toInt();
    }
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("DELETE FROM MusicSyncActOverrides"))) {
        kLogger.warning() << "Could not clear act rules:" << query.lastError();
        return -1;
    }
    return before;
}

int OverrideRepository::resetAll() {
    const int pairs = clear();
    const int acts = clearActRules();
    if (pairs < 0 || acts < 0) {
        return -1;
    }
    return pairs + acts;
}

} // namespace mixxx::music_sync
