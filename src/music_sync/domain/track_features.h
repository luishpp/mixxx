#pragma once

#include <QString>
#include <QVector>
#include <cstdint>
#include <optional>

namespace mixxx::music_sync {

/// A phrase boundary: musical phrases are groups of bars (typically 8/16/32).
struct PhraseMarker {
    std::int64_t startMs = 0;
    int bars = 0;
};

/// A structural section of the track (Intro/Groove/Build/Drop/Breakdown/Outro).
struct Section {
    QString type;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    float energy = 0.0f;
    float confidence = 0.0f;
};

/// A phrase-aligned candidate mixing window (an entry or an exit region).
struct TransitionWindow {
    QString kind; // "entry" | "exit"
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    int bars = 0;
    float energy = 0.0f;
    float energyStability = 0.0f;
    float instrumentalScore = 0.0f;
    float confidence = 0.0f;
};

/// Snapshot of a single track's native (Mixxx-provided) analysis data plus a
/// few derived fields (Camelot, analyzed flag). BPM/key/beats remain owned by
/// Mixxx; this is a cached, explainable view stored in the sidecar.
struct TrackFeatures {
    // Identity / correlation (see spec 11.3).
    std::int64_t mixxxTrackId = -1;
    QString location;
    std::int64_t fileSize = 0;

    // Metadata.
    QString title;
    QString artist;
    QString album;
    QString genre;

    // Stream info.
    std::int64_t durationMs = 0;
    int sampleRate = 0;
    int channels = 0;
    int bitrateKbps = 0;

    // Rhythm / tonality (native).
    double bpm = 0.0;
    int keyChromatic = 0; // mixxx::track::io::key::ChromaticKey as int; 0 == INVALID
    QString keyText;      // traditional key label
    QString camelot;      // Lancelot/Camelot notation via Mixxx KeyUtils

    // Loudness (native ReplayGain, linear ratio; 0 == not set).
    double replaygainRatio = 0.0;

    // Structure hints from Mixxx.
    bool hasBeatgrid = false;
    std::optional<std::int64_t> introStartMs;
    std::optional<std::int64_t> introEndMs;
    std::optional<std::int64_t> outroStartMs;
    std::optional<std::int64_t> outroEndMs;

    // Our derived analysis status (matches how Mixxx gates re-analysis).
    bool analyzed = false;

    // --- Set plan (parsed from the track comment written by the set prep, e.g.
    // "ATO 5 | ÂNCORA | MELODIC TECHNO | 126 BPM | ESCURIDÃO") ---
    /// 1..7 = the act this track belongs to in the narrative; 0 = unknown/extra.
    /// Drives the hybrid curation: the order follows the acts, the engine
    /// optimizes inside each one.
    int act = 0;
    /// ÂNCORA / PONTE / FLASH / CODA / ... — its role in the set (spec 9).
    QString setFunction;
    /// The track number from the set plan ("05", "33.1"). Gives each track its
    /// canonical place inside its act, which is what pins an anchor.
    QString trackNumber;

    /// The plan's track number as a sortable value; -1 when absent. "33.1"
    /// sorts between 33 and 34, which is exactly what the optional extras want.
    double planOrder() const {
        bool ok = false;
        const double value = trackNumber.toDouble(&ok);
        return ok ? value : -1.0;
    }

    /// Anchors hold their position; the engine arranges the rest around them
    /// (spec 9 — hybrid curation).
    bool isAnchor() const {
        return setFunction.compare(QStringLiteral("ÂNCORA"), Qt::CaseInsensitive) == 0 ||
                setFunction.compare(QStringLiteral("ANCORA"), Qt::CaseInsensitive) == 0;
    }

    // --- Fase 3: advanced analysis, derived from the Mixxx waveform + tempo ---
    double overallEnergy = 0.0;    // mean energy 0..1, comparable across tracks
    QVector<float> energyCurve;    // per-track normalized energy (0..1)
    // --- Fase 9: refined by the optional Python worker (0 = not measured) ---
    double vocalDensity = 0.0;     // share of energy in the voice band (0..1)
    QVector<float> bassCurve;      // per-track normalized bass presence (0..1)
    QVector<PhraseMarker> phrases; // heuristic phrase boundaries
    QVector<Section> sections;     // heuristic structural sections
    QVector<TransitionWindow> entryWindows;
    QVector<TransitionWindow> exitWindows;
    QString advancedAnalyzerVersion;

    // Provenance.
    QString analyzerVersion;
    QString snapshotAt; // set by the sidecar on write

    bool isValid() const {
        return mixxxTrackId >= 0;
    }
};

} // namespace mixxx::music_sync
