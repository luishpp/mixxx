#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

namespace mixxx::music_sync {

/// Shape of the energy journey over the set (RF-006).
enum class EnergyPreset {
    Ascending,
    CenterPeak,
    LatePeak,
    Waves,
    Constant,
    Custom,
};

/// One point of a normalized energy curve: position and energy both in 0..1.
struct EnergyPoint {
    double position = 0.0;
    double energy = 0.0;
};

/// Tempo tolerance presets (percent), spec 18.4.
namespace tempo_tolerance {
constexpr double kConservative = 3.0;
constexpr double kBalanced = 5.0;
constexpr double kFlexible = 8.0;
} // namespace tempo_tolerance

/// The user's declarative intent for a set (subset relevant to sequencing).
struct MixIntent {
    EnergyPreset energyPreset = EnergyPreset::LatePeak;
    QVector<EnergyPoint> customEnergyPoints; // used when energyPreset == Custom
    QStringList genreJourney;
    double startBpm = 0.0;
    double endBpm = 0.0;
    double maxTempoChangePercent = tempo_tolerance::kBalanced;
    double peakPosition = 0.85;
    int preferredTransitionBars = 32;
    bool avoidVocalOverlap = true;
    std::int64_t targetDurationMs = 0;
};

} // namespace mixxx::music_sync
