#include "music_sync/planner/preview_compiler.h"

#include <algorithm>
#include <cmath>

namespace mixxx::music_sync {

PreviewProgram PreviewCompiler::compile(const TransitionPlan& plan, double stepBeats) {
    PreviewProgram program;
    program.type = plan.type;
    program.sourceTrackId = plan.sourceTrackId;
    program.targetTrackId = plan.targetTrackId;
    program.sourceStartMs = plan.sourceExitMs;
    program.targetStartMs = plan.targetEntryMs;
    program.targetBpm = plan.targetBpm;
    program.sourceRateRatio = plan.sourceRateRatio;
    program.targetRateRatio = plan.targetRateRatio;
    program.durationBeats = plan.durationBeats;
    program.durationMs = plan.durationMs;
    program.explanation = plan.explanation;

    if (stepBeats <= 0.0) {
        stepBeats = kDefaultStepBeats;
    }

    // Instantaneous actions -> one write each.
    for (const ControlAction& action : plan.actions) {
        program.writes.append({action.atBeat, action.control, action.value});
    }

    // Linear ramps -> discrete writes at fixed beat resolution, endpoints
    // always included so the control lands exactly on its final value.
    for (const AutomationRamp& ramp : plan.ramps) {
        const double span = ramp.toBeat - ramp.fromBeat;
        if (span <= 0.0) {
            program.writes.append({ramp.fromBeat, ramp.control, ramp.endValue});
            continue;
        }
        const int steps = std::max(1, static_cast<int>(std::ceil(span / stepBeats)));
        for (int i = 0; i <= steps; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(steps);
            const double beat = ramp.fromBeat + t * span;
            const double value = ramp.startValue + t * (ramp.endValue - ramp.startValue);
            program.writes.append({beat, ramp.control, value});
        }
    }

    std::stable_sort(program.writes.begin(),
            program.writes.end(),
            [](const ControlWrite& a, const ControlWrite& b) { return a.atBeat < b.atBeat; });

    return program;
}

} // namespace mixxx::music_sync
