#pragma once

#include "music_sync/domain/preview_program.h"
#include "music_sync/domain/transition_plan.h"

namespace mixxx::music_sync {

/// Compiles a declarative TransitionPlan into a fully expanded PreviewProgram:
/// instantaneous actions become single writes; linear ramps are discretized at
/// a fixed beat resolution. Pure and deterministic — no engine access — so the
/// timeline can be unit tested and serialized before the executor consumes it.
class PreviewCompiler {
  public:
    /// Ramp discretization step, in beats (0.25 = a 1/16 note; ~117 ms at
    /// 128 BPM — smooth without flooding the control system).
    static constexpr double kDefaultStepBeats = 0.25;

    static PreviewProgram compile(
            const TransitionPlan& plan, double stepBeats = kDefaultStepBeats);
};

} // namespace mixxx::music_sync
