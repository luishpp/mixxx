#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

#include "music_sync/domain/track_features.h"
#include "track/track_decl.h"

namespace mixxx::music_sync {

/// One mono-folded waveform sample: filtered band magnitudes (0..255).
struct BandSample {
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;
};

/// Result of the energy/bass curve computation.
struct EnergyCurves {
    QVector<float> energy;      // per-track normalized 0..1
    QVector<float> bass;        // per-track normalized 0..1
    double overallEnergy = 0.0; // mean raw energy scaled to 0..1 (cross-track)
};

/// Computes advanced analysis (energy/bass curves, phrase markers) from data
/// Mixxx already produced — the waveform summary and the tempo/beat grid — so
/// it never re-decodes audio. The DSP core is exposed as pure static functions
/// for testing.
class AdvancedAnalysisAdapter {
  public:
    static const QString kAnalyzerVersion;

    static constexpr int kEnergyCurvePoints = 64;
    static constexpr int kBeatsPerBar = 4;
    static constexpr int kBarsPerPhrase = 16;
    static constexpr int kMaxTransitionWindows = 3;
    /// A window shorter than this is not a usable blend (a 2-bar "EQ blend" is
    /// really a cut), so such candidates are dropped.
    static constexpr int kMinWindowBars = 8;
    /// Windows merge consecutive phrases up to this length, so a 16-bar phrase
    /// grid can still yield the 32-bar transition the spec targets.
    static constexpr int kPreferredWindowBars = 32;

    /// Buckets per-frame band samples into `numBuckets` and returns per-track
    /// normalized energy (low+mid+high) and bass (low) curves plus an overall
    /// energy value comparable across tracks.
    static EnergyCurves computeCurves(const QVector<BandSample>& frames, int numBuckets);

    /// Analytic phrase boundaries from a constant-tempo grid (accurate for
    /// electronic music). Each phrase is `barsPerPhrase` bars; the last may be
    /// shorter.
    static QVector<PhraseMarker> computePhrases(
            double firstBeatMs,
            double bpm,
            std::int64_t durationMs,
            int beatsPerBar,
            int barsPerPhrase);

    /// Coarse structural sections from the normalized energy curve: Intro/Outro
    /// at the low-energy ends, Drop at high energy, Breakdown at a mid-track
    /// low-energy dip, Build before a Drop, Groove otherwise.
    static QVector<Section> computeSections(
            const QVector<float>& energyCurve, std::int64_t durationMs);

    /// Phrase-aligned candidate mixing windows. Each candidate starts on a
    /// phrase boundary and merges consecutive phrases up to kPreferredWindowBars;
    /// candidates shorter than kMinWindowBars are dropped. Ranked by energy
    /// stability weighted by length, so a short tail phrase (which looks
    /// artificially stable) cannot outrank a full-length window.
    /// `entry` restricts the start to the first 30% of the track; otherwise the
    /// start must be in the last 40%.
    static QVector<TransitionWindow> computeTransitionWindows(
            const QVector<float>& energyCurve,
            const QVector<PhraseMarker>& phrases,
            std::int64_t durationMs,
            bool entry);

    /// Fills the advanced fields of `pFeatures` (native fields already set) by
    /// reading pTrack's waveform summary and tempo. A no-op when data is absent.
    static void compute(
            const TrackPointer& pTrack, std::int64_t durationMs, TrackFeatures* pFeatures);
};

} // namespace mixxx::music_sync
