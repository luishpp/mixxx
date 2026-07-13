#include "music_sync/planner/energy_curve.h"

#include <algorithm>

namespace mixxx::music_sync {

QVector<EnergyPoint> EnergyCurve::forPreset(EnergyPreset preset) {
    switch (preset) {
    case EnergyPreset::Ascending:
        return {{0.0, 0.25}, {1.0, 1.0}};
    case EnergyPreset::CenterPeak:
        return {{0.0, 0.30}, {0.5, 1.0}, {1.0, 0.40}};
    case EnergyPreset::LatePeak:
        return {{0.0, 0.25}, {0.35, 0.50}, {0.70, 0.78}, {0.90, 1.00}, {1.0, 0.65}};
    case EnergyPreset::Waves:
        return {{0.0, 0.30}, {0.25, 0.80}, {0.5, 0.45}, {0.75, 0.90}, {1.0, 0.50}};
    case EnergyPreset::Constant:
        return {{0.0, 0.60}, {1.0, 0.60}};
    case EnergyPreset::Custom:
        return {}; // caller supplies customEnergyPoints
    }
    return {};
}

QVector<EnergyPoint> EnergyCurve::forIntent(const MixIntent& intent) {
    if (intent.energyPreset == EnergyPreset::Custom) {
        return intent.customEnergyPoints;
    }
    return forPreset(intent.energyPreset);
}

double EnergyCurve::energyAt(const QVector<EnergyPoint>& curve, double position) {
    if (curve.isEmpty()) {
        return 0.0;
    }
    const double p = std::clamp(position, 0.0, 1.0);
    if (p <= curve.first().position) {
        return curve.first().energy;
    }
    if (p >= curve.last().position) {
        return curve.last().energy;
    }
    for (int i = 1; i < curve.size(); ++i) {
        if (p <= curve[i].position) {
            const EnergyPoint& a = curve[i - 1];
            const EnergyPoint& b = curve[i];
            const double span = b.position - a.position;
            if (span <= 0.0) {
                return b.energy;
            }
            const double t = (p - a.position) / span;
            return a.energy + t * (b.energy - a.energy);
        }
    }
    return curve.last().energy;
}

} // namespace mixxx::music_sync
