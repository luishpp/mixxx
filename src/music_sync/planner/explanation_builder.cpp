#include "music_sync/planner/explanation_builder.h"

#include <cmath>

namespace mixxx::music_sync {

QString ExplanationBuilder::forPair(
        const TrackFeatures& from,
        const TrackFeatures& to,
        const PairScoreBreakdown& breakdown) {
    QString level;
    if (breakdown.total >= 0.75) {
        level = QStringLiteral("Strong");
    } else if (breakdown.total >= 0.5) {
        level = QStringLiteral("Moderate");
    } else {
        level = QStringLiteral("Weak");
    }

    const double energyDelta = to.overallEnergy - from.overallEnergy;
    QString energyDir;
    if (energyDelta > 0.08) {
        energyDir = QStringLiteral("rising energy");
    } else if (energyDelta < -0.08) {
        energyDir = QStringLiteral("falling energy");
    } else {
        energyDir = QStringLiteral("stable energy");
    }

    QString text = QStringLiteral("%1 match: %2 BPM difference, %3, %4.")
                           .arg(level,
                                   QString::number(std::abs(breakdown.bpmDelta), 'f', 1),
                                   breakdown.camelotMove,
                                   energyDir);
    if (!breakdown.penaltyReasons.isEmpty()) {
        text += QStringLiteral(" Caution: %1.")
                        .arg(breakdown.penaltyReasons.join(QStringLiteral(", ")));
    }
    return text;
}

} // namespace mixxx::music_sync
