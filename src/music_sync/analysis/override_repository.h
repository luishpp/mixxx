#pragma once

#include <QHash>
#include <QSqlDatabase>
#include <cstdint>

#include "music_sync/domain/transition_plan.h"

namespace mixxx::music_sync {

/// Identifies a transition by the two tracks it joins.
///
/// Never by position: regenerating the sequence reorders positions, so an
/// override pinned to "pair 7" would silently reappear on a different pair. The
/// pair is what the DJ actually made a decision about.
struct PairKey {
    std::int64_t sourceTrackId = -1;
    std::int64_t targetTrackId = -1;

    bool operator==(const PairKey& other) const {
        return sourceTrackId == other.sourceTrackId &&
                targetTrackId == other.targetTrackId;
    }
};

inline uint qHash(const PairKey& key, uint seed = 0) {
    return ::qHash(static_cast<qulonglong>(key.sourceTrackId), seed) ^
            ::qHash(static_cast<qulonglong>(key.targetTrackId), seed << 1);
}

/// Stores the DJ's per-pair transition choices in the sidecar (RF-010),
/// surviving both a restart and a regenerated sequence.
class OverrideRepository {
  public:
    explicit OverrideRepository(QSqlDatabase database);

    /// Every stored override, ready to look up while planning a sequence.
    QHash<PairKey, TransitionOverride> loadAll() const;

    /// Saves the DJ's choice for one pair. An override with nothing set removes
    /// the row instead — "let the planner decide" is the absence of a choice,
    /// not a choice to store.
    bool save(const PairKey& key, const TransitionOverride& override);

    /// Forgets every choice. Returns how many were removed, or -1 on failure.
    int clear();

  private:
    QSqlDatabase m_database;
};

} // namespace mixxx::music_sync
