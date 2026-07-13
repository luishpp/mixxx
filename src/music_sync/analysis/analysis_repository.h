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

    int count() const;

  private:
    QSqlDatabase m_database;
};

} // namespace mixxx::music_sync
