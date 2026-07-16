#include "music_sync/planner/energy_normalizer.h"

#include <algorithm>

namespace mixxx::music_sync {

void normalizeLibraryEnergy(QVector<TrackFeatures>* pTracks) {
    if (!pTracks) {
        return;
    }
    QVector<int> ranked;
    ranked.reserve(pTracks->size());
    for (int i = 0; i < pTracks->size(); ++i) {
        if (!pTracks->at(i).energyCurve.isEmpty()) {
            ranked.append(i);
        }
    }
    if (ranked.isEmpty()) {
        return;
    }
    if (ranked.size() == 1) {
        // A single reference point carries no relative information.
        (*pTracks)[ranked.first()].overallEnergy = 0.5;
        return;
    }

    std::sort(ranked.begin(), ranked.end(), [pTracks](int a, int b) {
        const TrackFeatures& fa = pTracks->at(a);
        const TrackFeatures& fb = pTracks->at(b);
        if (fa.overallEnergy != fb.overallEnergy) {
            return fa.overallEnergy < fb.overallEnergy;
        }
        return fa.mixxxTrackId < fb.mixxxTrackId; // deterministic tie-break
    });

    const double last = static_cast<double>(ranked.size() - 1);
    for (int r = 0; r < ranked.size(); ++r) {
        (*pTracks)[ranked.at(r)].overallEnergy = static_cast<double>(r) / last;
    }
}

} // namespace mixxx::music_sync
