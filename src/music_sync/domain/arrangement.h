#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

namespace mixxx::music_sync {

/// One track placed in an arrangement, with the score of its transition from
/// the previous item and whether the user pinned it to this position.
struct ArrangementItem {
    std::int64_t mixxxTrackId = -1;
    int position = 0;
    bool locked = false;
    double pairScoreFromPrevious = 0.0;
    QString explanationFromPrevious;
};

/// A candidate ordering of tracks with its aggregate scores and explanation.
struct Arrangement {
    QVector<ArrangementItem> items;
    double totalScore = 0.0;
    double energyFitScore = 0.0;
    std::int64_t totalDurationMs = 0;
    QVector<QString> warnings;
    QString explanation;
    QString algorithmVersion;
};

} // namespace mixxx::music_sync
