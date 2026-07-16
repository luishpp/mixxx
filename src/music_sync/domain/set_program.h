#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

#include "music_sync/domain/preview_program.h"

namespace mixxx::music_sync {

/// One track's slot in the set: which deck plays it, where it comes in and
/// where it hands over (spec 21.2).
struct SetItem {
    std::int64_t mixxxTrackId = -1;
    int position = 0;
    /// Deck index (0-based). Consecutive tracks alternate decks: while one
    /// plays, the next is loaded and cued on the other.
    int deckIndex = 0;
    /// Where this track starts playing — its planned entry.
    std::int64_t startMs = 0;
    /// Where the transition to the next track begins. Equal to durationMs for
    /// the last track: nothing follows it.
    std::int64_t exitMs = 0;
    std::int64_t durationMs = 0;

    QString artist;
    QString title;
    double bpm = 0.0;

    /// Milliseconds this track is the live deck before handing over.
    std::int64_t playSpanMs() const {
        return exitMs > startMs ? exitMs - startMs : 0;
    }
};

/// The compiled transition from item `fromPosition` to the next one.
struct SetTransition {
    int fromPosition = 0;
    PreviewProgram program;
};

/// A whole set, compiled: the queue plus every transition between consecutive
/// tracks, ready for the executor to drive without any further planning (spec
/// 21.3 — automations are pre-computed, never derived while playing).
struct SetProgram {
    QVector<SetItem> items;
    /// One per consecutive pair, so size() == items.size() - 1 (or 0).
    QVector<SetTransition> transitions;
    QString explanation;
    QVector<QString> warnings;

    /// Rough wall-clock length: every track's play span plus the final track's
    /// remainder. Real length shifts slightly with tempo changes.
    std::int64_t estimatedDurationMs() const {
        std::int64_t total = 0;
        for (const SetItem& item : items) {
            total += item.playSpanMs();
        }
        return total;
    }
};

} // namespace mixxx::music_sync
