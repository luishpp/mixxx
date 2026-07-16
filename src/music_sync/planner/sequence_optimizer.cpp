#include "music_sync/planner/sequence_optimizer.h"

#include <QHash>
#include <QMap>
#include <QSet>
#include <algorithm>
#include <cmath>

#include "music_sync/planner/energy_curve.h"
#include "music_sync/planner/explanation_builder.h"

namespace mixxx::music_sync {

const QString SequenceOptimizer::kAlgorithmVersion = QStringLiteral("optimizer-0.1.0");

namespace {

using Matrix = QVector<QVector<double>>;

Matrix buildMatrix(const QVector<TrackFeatures>& tracks,
        const ScoringWeights& weights,
        double maxTempoPct) {
    const int n = tracks.size();
    Matrix m(n, QVector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i != j) {
                m[i][j] = PairScorer::score(tracks[i], tracks[j], weights, maxTempoPct).total;
            }
        }
    }
    return m;
}

double routeTotal(const QVector<int>& route, const Matrix& m) {
    if (route.size() < 2) {
        return route.isEmpty() ? 0.0 : 1.0;
    }
    double sum = 0.0;
    for (int i = 0; i + 1 < route.size(); ++i) {
        sum += m[route[i]][route[i + 1]];
    }
    return sum / (route.size() - 1);
}

/// Pins each anchor of `group` to the position the plan gave it: its rank when
/// the act's tracks are put in track-number order. Returns nothing when the
/// tracks carry no usable numbers, so an unprepped library is untouched.
QVector<LockedPosition> anchorLocks(const QVector<TrackFeatures>& group) {
    QVector<LockedPosition> locks;
    QVector<int> byPlan;
    for (int i = 0; i < group.size(); ++i) {
        if (group.at(i).planOrder() < 0.0) {
            return locks; // incomplete numbering: pinning would be a guess
        }
        byPlan.append(i);
    }
    std::sort(byPlan.begin(), byPlan.end(), [&group](int a, int b) {
        const double pa = group.at(a).planOrder();
        const double pb = group.at(b).planOrder();
        return pa != pb ? pa < pb : a < b;
    });
    for (int pos = 0; pos < byPlan.size(); ++pos) {
        const TrackFeatures& track = group.at(byPlan.at(pos));
        if (track.isAnchor()) {
            locks.append({pos, track.mixxxTrackId});
        }
    }
    return locks;
}

/// The act's tracks in the order the plan gave them (by track number). Empty if
/// any track lacks a number — then there is no plan order to honour.
QVector<TrackFeatures> inPlanOrder(const QVector<TrackFeatures>& group) {
    for (const TrackFeatures& track : group) {
        if (track.planOrder() < 0.0) {
            return {};
        }
    }
    QVector<TrackFeatures> ordered = group;
    std::stable_sort(ordered.begin(),
            ordered.end(),
            [](const TrackFeatures& a, const TrackFeatures& b) {
                return a.planOrder() < b.planOrder();
            });
    return ordered;
}

QVector<std::int64_t> idsOf(const QVector<TrackFeatures>& ordered) {
    QVector<std::int64_t> ids;
    ids.reserve(ordered.size());
    for (const TrackFeatures& track : ordered) {
        ids.append(track.mixxxTrackId);
    }
    return ids;
}

/// Rebuilds an Arrangement from a final track order, recomputing every pair from
/// scratch. Used by the act path: concatenating per-act routes creates new pairs
/// at the act boundaries that no sub-arrangement ever scored.
Arrangement buildFromOrderedTracks(const QVector<TrackFeatures>& ordered,
        const ScoringWeights& weights,
        double maxTempoPct,
        const QVector<EnergyPoint>& curve,
        const QSet<std::int64_t>& lockedIds) {
    Arrangement arr;
    arr.algorithmVersion = SequenceOptimizer::kAlgorithmVersion;
    int tempoWarnings = 0;
    double pairSum = 0.0;
    for (int pos = 0; pos < ordered.size(); ++pos) {
        const TrackFeatures& track = ordered.at(pos);
        ArrangementItem item;
        item.mixxxTrackId = track.mixxxTrackId;
        item.position = pos;
        item.locked = lockedIds.contains(track.mixxxTrackId);
        if (pos > 0) {
            const TrackFeatures& prev = ordered.at(pos - 1);
            // Score each transition with the weights of the act it lands in, so
            // the reported number matches the one that drove the optimization.
            const PairScoreBreakdown bd = PairScorer::score(
                    prev, track, weightsForAct(track.act, weights), maxTempoPct);
            item.pairScoreFromPrevious = bd.total;
            item.explanationFromPrevious = ExplanationBuilder::forPair(prev, track, bd);
            pairSum += bd.total;
            if (bd.tempoChangePercent > maxTempoPct) {
                ++tempoWarnings;
            }
        }
        arr.totalDurationMs += track.durationMs;
        arr.items.append(item);
    }
    arr.totalScore = ordered.size() > 1 ? pairSum / (ordered.size() - 1)
                                        : (ordered.isEmpty() ? 0.0 : 1.0);

    if (ordered.size() < 2 || curve.isEmpty()) {
        arr.energyFitScore = 0.5;
    } else {
        double err = 0.0;
        for (int pos = 0; pos < ordered.size(); ++pos) {
            const double p = static_cast<double>(pos) / (ordered.size() - 1);
            err += std::abs(ordered.at(pos).overallEnergy - EnergyCurve::energyAt(curve, p));
        }
        arr.energyFitScore = std::clamp(1.0 - err / ordered.size(), 0.0, 1.0);
    }
    if (tempoWarnings > 0) {
        arr.warnings.append(QStringLiteral("%1 transition(s) exceed the tempo tolerance")
                                    .arg(tempoWarnings));
    }
    arr.explanation = QStringLiteral("%1 tracks, avg compatibility %2%, energy fit %3%")
                              .arg(ordered.size())
                              .arg(qRound(arr.totalScore * 100.0))
                              .arg(qRound(arr.energyFitScore * 100.0));
    return arr;
}

} // anonymous namespace

QVector<Arrangement> SequenceOptimizer::arrange(
        const QVector<TrackFeatures>& tracks, const Options& options) {
    QVector<Arrangement> result;
    const int n = tracks.size();
    if (n == 0) {
        return result;
    }

    const double maxTempoPct = options.intent.maxTempoChangePercent > 0.0
            ? options.intent.maxTempoChangePercent
            : tempo_tolerance::kBalanced;

    // --- Hybrid curation: the narrative (acts) constrains the order; the engine
    // only optimizes inside each act. Without this the optimizer maximizes pair
    // scores globally and happily puts an Act 6 trance track after an Act 3
    // groove — musically smooth, narratively wrong.
    if (options.respectActs) {
        QMap<int, QVector<TrackFeatures>> byAct; // QMap iterates keys ascending
        for (const TrackFeatures& track : tracks) {
            byAct[track.act].append(track);
        }
        QVector<int> actOrder;
        for (auto it = byAct.cbegin(); it != byAct.cend(); ++it) {
            if (it.key() > 0) {
                actOrder.append(it.key());
            }
        }
        if (byAct.contains(0)) {
            actOrder.append(0); // unknown/extra tracks close the set
        }
        // One group = nothing to constrain; fall through (also ends the recursion).
        if (actOrder.size() > 1) {
            QHash<std::int64_t, TrackFeatures> byId;
            for (const TrackFeatures& track : tracks) {
                byId.insert(track.mixxxTrackId, track);
            }
            // Candidate orders per act, best first.
            QVector<QVector<QVector<TrackFeatures>>> perAct;
            QSet<std::int64_t> lockedIds;
            int placed = 0; // how many tracks the earlier acts already occupy
            for (int act : actOrder) {
                const QVector<TrackFeatures>& group = byAct.value(act);
                Options sub = options;
                sub.respectActs = false;
                // Each act only spans its own slice of the journey: act 5 is
                // judged against the curve around 70-85%, not against 0..1.
                if (n > 1) {
                    sub.curveFrom = static_cast<double>(placed) / (n - 1);
                    sub.curveTo = static_cast<double>(placed + group.size() - 1) / (n - 1);
                }
                placed += group.size();
                // Harmony matters more in some acts than others (spec 10.5).
                sub.weights = weightsForAct(act, options.weights);
                // Hybrid curation (spec 9): the act's anchors hold their
                // canonical place — the one the plan gave them, i.e. their rank
                // by track number inside the act — and the engine arranges the
                // rest around them. Without this, nothing stops the optimizer
                // from burying the anchor that defines the act mid-way.
                sub.locks = anchorLocks(group);
                for (const LockedPosition& lock : sub.locks) {
                    lockedIds.insert(lock.mixxxTrackId);
                }

                QVector<QVector<TrackFeatures>> orders;
                for (const Arrangement& alternative : arrange(group, sub)) {
                    QVector<TrackFeatures> order;
                    for (const ArrangementItem& item : alternative.items) {
                        order.append(byId.value(item.mixxxTrackId));
                    }
                    orders.append(order);
                }

                // The plan's order always leads; the engine's routes follow as
                // alternatives.
                //
                // Comparing them by score was tried and is the wrong instrument:
                // the score measures exactly what the plan does not optimize for.
                // Act 1's own order runs 3B -> 9B -> 6B -> 10A -> 5A, which is
                // poor by Camelot adjacency, so the engine "wins" while producing
                // a musically worse set — spec 8 curated it by narrative and ear
                // ("introdução cinematográfica", "deixar a faixa respirar"), and
                // the keys themselves come from Mixxx's detection, which errs.
                // So the engine informs, it does not decide.
                const QVector<TrackFeatures> baseline = inPlanOrder(group);
                if (!baseline.isEmpty()) {
                    // Never offer the same route twice.
                    const QVector<std::int64_t> baseIds = idsOf(baseline);
                    for (int i = orders.size() - 1; i >= 0; --i) {
                        if (idsOf(orders.at(i)) == baseIds) {
                            orders.removeAt(i);
                        }
                    }
                    orders.prepend(baseline);
                }
                perAct.append(orders);
            }

            const QVector<EnergyPoint> actCurve = EnergyCurve::forIntent(options.intent);
            for (int k = 0; k < options.numAlternatives; ++k) {
                QVector<TrackFeatures> ordered;
                bool fresh = false; // did any act actually offer a k-th variant?
                for (const QVector<QVector<TrackFeatures>>& orders : perAct) {
                    if (orders.isEmpty()) {
                        continue;
                    }
                    const int pick = std::min(k, static_cast<int>(orders.size()) - 1);
                    if (pick == k) {
                        fresh = true;
                    }
                    ordered.append(orders.at(pick));
                }
                if (ordered.isEmpty() || (k > 0 && !fresh)) {
                    break; // no act has anything new left: stop inventing duplicates
                }
                result.append(buildFromOrderedTracks(
                        ordered, options.weights, maxTempoPct, actCurve, lockedIds));
            }
            return result;
        }
    }

    const Matrix m = buildMatrix(tracks, options.weights, maxTempoPct);
    const QVector<EnergyPoint> curve = EnergyCurve::forIntent(options.intent);

    QHash<std::int64_t, int> idToIndex;
    for (int i = 0; i < n; ++i) {
        idToIndex.insert(tracks[i].mixxxTrackId, i);
    }

    QHash<int, int> lockedPosToIdx;
    for (const LockedPosition& lock : options.locks) {
        if (lock.position < 0 || lock.position >= n) {
            continue;
        }
        const auto it = idToIndex.constFind(lock.mixxxTrackId);
        if (it != idToIndex.constEnd()) {
            lockedPosToIdx.insert(lock.position, it.value());
        }
    }

    // Where this group sits on the global curve — the act path narrows it.
    const auto curveAtLocal = [&](int pos, int size) {
        const double local = size > 1 ? static_cast<double>(pos) / (size - 1) : 0.0;
        return options.curveFrom + local * (options.curveTo - options.curveFrom);
    };
    const double openerTarget = curve.isEmpty()
            ? 0.5
            : EnergyCurve::energyAt(curve, options.curveFrom);
    const auto openerScore = [&](int idx) {
        return 1.0 - std::abs(tracks[idx].overallEnergy - openerTarget);
    };

    // Greedy build with a variable choice for the first unlocked "opener".
    const auto greedy = [&](int seedRank) {
        QVector<int> route(n, -1);
        QVector<bool> used(n, false);
        for (auto it = lockedPosToIdx.cbegin(); it != lockedPosToIdx.cend(); ++it) {
            route[it.key()] = it.value();
            used[it.value()] = true;
        }
        int prev = -1;
        bool firstUnlocked = true;
        for (int pos = 0; pos < n; ++pos) {
            if (route[pos] != -1) {
                prev = route[pos];
                continue;
            }
            QVector<int> cands;
            for (int k = 0; k < n; ++k) {
                if (!used[k]) {
                    cands.append(k);
                }
            }
            if (cands.isEmpty()) {
                break;
            }
            int pick = cands.first();
            if (prev == -1) {
                std::sort(cands.begin(), cands.end(), [&](int a, int b) {
                    const double sa = openerScore(a);
                    const double sb = openerScore(b);
                    return sa != sb ? sa > sb : a < b;
                });
                const int rank =
                        firstUnlocked ? std::min(seedRank, static_cast<int>(cands.size()) - 1) : 0;
                pick = cands[rank];
            } else {
                double best = m[prev][pick];
                for (int c : cands) {
                    if (m[prev][c] > best) {
                        best = m[prev][c];
                        pick = c;
                    }
                }
            }
            route[pos] = pick;
            used[pick] = true;
            prev = pick;
            firstUnlocked = false;
        }
        return route;
    };

    // 2-opt improvement, never moving a locked position.
    const auto twoOpt = [&](QVector<int> route) {
        for (int pass = 0; pass < options.twoOptPasses; ++pass) {
            bool improved = false;
            for (int i = 0; i + 1 < n; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    bool anyLocked = false;
                    for (int p = i; p <= j; ++p) {
                        if (lockedPosToIdx.contains(p)) {
                            anyLocked = true;
                            break;
                        }
                    }
                    if (anyLocked) {
                        continue;
                    }
                    QVector<int> candidate = route;
                    std::reverse(candidate.begin() + i, candidate.begin() + j + 1);
                    if (routeTotal(candidate, m) > routeTotal(route, m) + 1e-9) {
                        route = candidate;
                        improved = true;
                    }
                }
            }
            if (!improved) {
                break;
            }
        }
        return route;
    };

    const auto energyFit = [&](const QVector<int>& route) -> double {
        if (route.size() < 2 || curve.isEmpty()) {
            return 0.5;
        }
        double err = 0.0;
        int count = 0;
        for (int pos = 0; pos < route.size(); ++pos) {
            if (route[pos] < 0) {
                continue;
            }
            const double p = curveAtLocal(pos, route.size());
            err += std::abs(tracks[route[pos]].overallEnergy - EnergyCurve::energyAt(curve, p));
            ++count;
        }
        return count > 0 ? std::clamp(1.0 - err / count, 0.0, 1.0) : 0.5;
    };

    // Generate distinct routes by varying the opener; 2-opt each.
    QVector<QVector<int>> routes;
    const int tries = std::min(n, std::max(options.numAlternatives + 3, 6));
    for (int seed = 0; seed < tries; ++seed) {
        const QVector<int> route = twoOpt(greedy(seed));
        if (route.contains(-1)) {
            continue;
        }
        if (!routes.contains(route)) {
            routes.append(route);
        }
    }

    struct Scored {
        QVector<int> route;
        double total = 0.0;
        double energyFit = 0.0;
    };
    QVector<Scored> scored;
    for (const QVector<int>& route : routes) {
        scored.append({route, routeTotal(route, m), energyFit(route)});
    }
    std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        return a.total != b.total ? a.total > b.total : a.energyFit > b.energyFit;
    });

    const int take = std::min(options.numAlternatives, static_cast<int>(scored.size()));
    for (int s = 0; s < take; ++s) {
        const QVector<int>& route = scored[s].route;
        Arrangement arr;
        arr.algorithmVersion = kAlgorithmVersion;
        arr.totalScore = scored[s].total;
        arr.energyFitScore = scored[s].energyFit;
        int tempoWarnings = 0;
        for (int pos = 0; pos < route.size(); ++pos) {
            const TrackFeatures& track = tracks[route[pos]];
            ArrangementItem item;
            item.mixxxTrackId = track.mixxxTrackId;
            item.position = pos;
            item.locked = lockedPosToIdx.contains(pos) && lockedPosToIdx.value(pos) == route[pos];
            if (pos > 0) {
                const TrackFeatures& prev = tracks[route[pos - 1]];
                const PairScoreBreakdown bd =
                        PairScorer::score(prev, track, options.weights, maxTempoPct);
                item.pairScoreFromPrevious = bd.total;
                item.explanationFromPrevious = ExplanationBuilder::forPair(prev, track, bd);
                if (bd.tempoChangePercent > maxTempoPct) {
                    ++tempoWarnings;
                }
            }
            arr.totalDurationMs += track.durationMs;
            arr.items.append(item);
        }
        if (tempoWarnings > 0) {
            arr.warnings.append(
                    QStringLiteral("%1 transition(s) exceed the tempo tolerance")
                            .arg(tempoWarnings));
        }
        arr.explanation =
                QStringLiteral("%1 tracks, avg compatibility %2%, energy fit %3%")
                        .arg(route.size())
                        .arg(qRound(arr.totalScore * 100.0))
                        .arg(qRound(arr.energyFitScore * 100.0));
        result.append(arr);
    }
    return result;
}

} // namespace mixxx::music_sync
