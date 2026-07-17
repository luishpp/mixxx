#include "music_sync/planner/set_compiler.h"

#include "music_sync/planner/preview_compiler.h"
#include "music_sync/planner/transition_planner.h"

namespace mixxx::music_sync {

Arrangement SetCompiler::scopeToActs(const Arrangement& arrangement,
        const QHash<std::int64_t, TrackFeatures>& byId,
        int fromAct,
        int toAct) {
    if (fromAct <= 0 && toAct <= 0) {
        return arrangement;
    }
    Arrangement scoped;
    scoped.algorithmVersion = arrangement.algorithmVersion;
    for (const ArrangementItem& item : arrangement.items) {
        const auto it = byId.constFind(item.mixxxTrackId);
        if (it == byId.constEnd()) {
            continue;
        }
        const int act = it.value().act;
        if (fromAct > 0 && act < fromAct) {
            continue;
        }
        if (toAct > 0 && act > toAct) {
            continue;
        }
        scoped.items.append(item);
    }
    // compile() renumbers positions and re-plans every pair, so the excerpt
    // needs no further fixing up.
    return scoped;
}

SetProgram SetCompiler::compile(const Arrangement& arrangement,
        const QHash<std::int64_t, TrackFeatures>& byId,
        const MixIntent& intent) {
    SetProgram set;
    if (arrangement.items.isEmpty()) {
        return set;
    }

    QVector<TrackFeatures> ordered;
    ordered.reserve(arrangement.items.size());
    for (const ArrangementItem& item : arrangement.items) {
        const auto it = byId.constFind(item.mixxxTrackId);
        if (it == byId.constEnd()) {
            set.warnings.append(
                    QStringLiteral("Track %1 is not in the library; set truncated")
                            .arg(item.mixxxTrackId));
            break; // a gap would silently reorder the narrative
        }
        ordered.append(it.value());
    }
    if (ordered.isEmpty()) {
        return set;
    }

    // Plan every transition first: each one decides where the outgoing track
    // hands over and where the incoming one comes in.
    for (int i = 0; i + 1 < ordered.size(); ++i) {
        const TransitionPlan plan =
                TransitionPlanner::plan(ordered.at(i), ordered.at(i + 1), intent);
        SetTransition transition;
        transition.fromPosition = i;
        transition.program = PreviewCompiler::compile(plan);
        set.transitions.append(transition);
        for (const QString& warning : plan.warnings) {
            set.warnings.append(QStringLiteral("%1 -> %2: %3")
                                        .arg(i + 1)
                                        .arg(i + 2)
                                        .arg(warning));
        }
    }

    for (int i = 0; i < ordered.size(); ++i) {
        const TrackFeatures& track = ordered.at(i);
        SetItem item;
        item.mixxxTrackId = track.mixxxTrackId;
        item.position = i;
        // Alternate decks: while one plays, the next loads and cues on the other.
        item.deckIndex = i % kDeckCount;
        item.durationMs = track.durationMs;
        item.artist = track.artist;
        item.title = track.title;
        item.bpm = track.bpm;
        item.act = track.act;

        // Comes in where the previous transition puts it; the opener starts at
        // its own entry window.
        if (i == 0) {
            item.startMs = track.entryWindows.isEmpty()
                    ? 0
                    : track.entryWindows.first().startMs;
        } else {
            item.startMs = set.transitions.at(i - 1).program.targetStartMs;
        }
        // Hands over where the next transition takes it; the last track has no
        // successor, so it simply runs out.
        item.exitMs = (i < set.transitions.size())
                ? set.transitions.at(i).program.sourceStartMs
                : track.durationMs;

        if (item.exitMs <= item.startMs) {
            set.warnings.append(
                    QStringLiteral("%1. %2: exit lands at or before the entry; "
                                   "the track gets no play time")
                            .arg(i + 1)
                            .arg(track.title.isEmpty() ? track.artist : track.title));
        }
        set.items.append(item);
    }

    const std::int64_t seconds = set.estimatedDurationMs() / 1000;
    set.explanation = QStringLiteral("%1 tracks, %2 transition(s), about %3:%4")
                              .arg(set.items.size())
                              .arg(set.transitions.size())
                              .arg(seconds / 60)
                              .arg(seconds % 60, 2, 10, QChar('0'));
    return set;
}

} // namespace mixxx::music_sync
