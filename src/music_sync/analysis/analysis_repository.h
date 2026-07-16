#pragma once

#include <QSqlDatabase>
#include <QVector>
#include <cstdint>
#include <optional>

#include "music_sync/domain/track_features.h"

namespace mixxx::music_sync {

/// Persists TrackFeatures snapshots in the sidecar's MusicSyncTrackFeatures
/// table (created by sidecar migration v2). Keyed by the Mixxx track id.
class AnalysisRepository {
  public:
    explicit AnalysisRepository(QSqlDatabase database);

    /// Inserts or updates the snapshot for features.mixxxTrackId. Returns false
    /// on failure (logged).
    bool upsert(const TrackFeatures& features);

    /// All snapshots, ordered by artist then title.
    QVector<TrackFeatures> loadAll() const;

    std::optional<TrackFeatures> loadByTrackId(std::int64_t mixxxTrackId) const;

    /// Drops snapshots whose track is no longer in the library, so the sidecar
    /// mirrors it instead of accumulating orphans forever. Returns how many were
    /// removed, or -1 on failure.
    ///
    /// `liveTrackIds` must be the COMPLETE set of live library ids — passing a
    /// partial set (e.g. a limited batch) would delete valid snapshots. An empty
    /// list therefore means "the library is empty" and removes everything; the
    /// caller must not pass an empty list when the library query merely failed.
    int removeMissing(const QVector<std::int64_t>& liveTrackIds);

    /// Removes every snapshot. Returns how many were removed, or -1 on failure.
    int clear();

    int count() const;

  private:
    QSqlDatabase m_database;
};

} // namespace mixxx::music_sync
