#pragma once

#include <QVector>

#include "music_sync/domain/mix_intent.h"

namespace mixxx::music_sync {

/// Energy-curve presets (RF-006) and piecewise-linear sampling. Positions and
/// energies are normalized to 0..1.
class EnergyCurve {
  public:
    static QVector<EnergyPoint> forPreset(EnergyPreset preset);

    /// Resolves the intent's curve (preset, or its custom points).
    static QVector<EnergyPoint> forIntent(const MixIntent& intent);

    /// Piecewise-linear interpolation of a curve at position (0..1).
    static double energyAt(const QVector<EnergyPoint>& curve, double position);
};

} // namespace mixxx::music_sync
