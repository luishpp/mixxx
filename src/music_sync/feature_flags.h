#pragma once

// Music Sync DJ — feature flags.
//
// The whole module is compiled in only when the CMake option
// MUSIC_SYNC_ENABLED is ON, which defines MIXXX_MUSIC_SYNC_ENABLED globally
// (see the root CMakeLists.txt). Upstream files that must reference the module
// guard their additions with `#ifdef MIXXX_MUSIC_SYNC_ENABLED`.
//
// This header centralizes a compile-time constant for code that is always
// compiled but wants to branch on the flag without spraying #ifdefs.

namespace mixxx::music_sync {

#ifdef MIXXX_MUSIC_SYNC_ENABLED
constexpr bool kMusicSyncCompiledIn = true;
#else
constexpr bool kMusicSyncCompiledIn = false;
#endif

} // namespace mixxx::music_sync
