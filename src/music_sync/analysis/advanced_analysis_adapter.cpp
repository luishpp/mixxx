#include "music_sync/analysis/advanced_analysis_adapter.h"

#include <algorithm>

#include "audio/frame.h"
#include "track/beats.h"
#include "track/track.h"
#include "waveform/waveform.h"

namespace mixxx::music_sync {

// 0.2.1: exit window is PICKED by headroom (the dip) but its confidence stays
// stability×length, so a usable groove no longer drops the pair to Crossfade.
const QString AdvancedAnalysisAdapter::kAnalyzerVersion = QStringLiteral("advanced-0.2.1");

EnergyCurves AdvancedAnalysisAdapter::computeCurves(
        const QVector<BandSample>& frames, int numBuckets) {
    EnergyCurves out;
    if (frames.isEmpty() || numBuckets <= 0) {
        return out;
    }
    out.energy.resize(numBuckets);
    out.bass.resize(numBuckets);
    QVector<double> rawEnergy(numBuckets, 0.0);

    const int frameCount = frames.size();
    for (int b = 0; b < numBuckets; ++b) {
        const int start = static_cast<int>(static_cast<qint64>(b) * frameCount / numBuckets);
        const int end = static_cast<int>(static_cast<qint64>(b + 1) * frameCount / numBuckets);
        double energySum = 0.0;
        double bassSum = 0.0;
        int n = 0;
        for (int i = start; i < end && i < frameCount; ++i) {
            energySum += frames[i].low + frames[i].mid + frames[i].high;
            bassSum += frames[i].low;
            ++n;
        }
        rawEnergy[b] = n > 0 ? energySum / n : 0.0;
        out.energy[b] = static_cast<float>(rawEnergy[b]);
        out.bass[b] = static_cast<float>(n > 0 ? bassSum / n : 0.0);
    }

    // Overall energy: mean raw energy scaled by the theoretical max (3 * 255).
    double energyMean = 0.0;
    for (double e : rawEnergy) {
        energyMean += e;
    }
    energyMean /= numBuckets;
    out.overallEnergy = std::clamp(energyMean / (3.0 * 255.0), 0.0, 1.0);

    // Per-track normalize both curves to 0..1 by their peak.
    const auto normalize = [](QVector<float>& curve) {
        float peak = 0.0f;
        for (float v : curve) {
            peak = std::max(peak, v);
        }
        if (peak > 0.0f) {
            for (float& v : curve) {
                v /= peak;
            }
        }
    };
    normalize(out.energy);
    normalize(out.bass);

    return out;
}

QVector<PhraseMarker> AdvancedAnalysisAdapter::computePhrases(
        double firstBeatMs,
        double bpm,
        std::int64_t durationMs,
        int beatsPerBar,
        int barsPerPhrase) {
    QVector<PhraseMarker> out;
    if (bpm <= 0.0 || durationMs <= 0 || beatsPerBar <= 0 || barsPerPhrase <= 0) {
        return out;
    }
    const double beatMs = 60000.0 / bpm;
    const double barMs = beatMs * beatsPerBar;
    const double phraseMs = barMs * barsPerPhrase;
    if (phraseMs <= 0.0) {
        return out;
    }
    for (double t = std::max(0.0, firstBeatMs); t < static_cast<double>(durationMs);
            t += phraseMs) {
        const double remaining = static_cast<double>(durationMs) - t;
        const int bars = std::min(barsPerPhrase, static_cast<int>(remaining / barMs));
        if (bars <= 0) {
            break;
        }
        out.append(PhraseMarker{static_cast<std::int64_t>(t), bars});
    }
    return out;
}

QVector<Section> AdvancedAnalysisAdapter::computeSections(
        const QVector<float>& energy, std::int64_t durationMs) {
    QVector<Section> out;
    const int n = energy.size();
    if (n == 0 || durationMs <= 0) {
        return out;
    }
    const auto classOf = [](float e) -> int {
        if (e < 0.4f) {
            return 0; // low
        }
        if (e < 0.7f) {
            return 1; // mid
        }
        return 2; // high
    };
    const double msPerPoint = static_cast<double>(durationMs) / n;
    const auto appendSection = [&](int start, int end, int cls) {
        double sum = 0.0;
        for (int i = start; i < end; ++i) {
            sum += energy[i];
        }
        Section section;
        section.startMs = static_cast<std::int64_t>(start * msPerPoint);
        section.endMs = static_cast<std::int64_t>(end * msPerPoint);
        section.energy = static_cast<float>(sum / std::max(1, end - start));
        const bool atStart = (start == 0);
        const bool atEnd = (end == n);
        if (cls == 2) {
            section.type = QStringLiteral("Drop");
        } else if (cls == 0 && atStart) {
            section.type = QStringLiteral("Intro");
        } else if (cls == 0 && atEnd) {
            section.type = QStringLiteral("Outro");
        } else if (cls == 0) {
            section.type = QStringLiteral("Breakdown");
        } else {
            section.type = QStringLiteral("Groove");
        }
        section.confidence = 0.5f;
        out.append(section);
    };

    int runStart = 0;
    int runClass = classOf(energy[0]);
    for (int i = 1; i < n; ++i) {
        const int c = classOf(energy[i]);
        if (c != runClass) {
            appendSection(runStart, i, runClass);
            runStart = i;
            runClass = c;
        }
    }
    appendSection(runStart, n, runClass);

    // A groove immediately before a Drop is really a Build.
    for (int i = 0; i + 1 < out.size(); ++i) {
        if (out[i].type == QStringLiteral("Groove") &&
                out[i + 1].type == QStringLiteral("Drop")) {
            out[i].type = QStringLiteral("Build");
        }
    }
    return out;
}

QVector<TransitionWindow> AdvancedAnalysisAdapter::computeTransitionWindows(
        const QVector<float>& energy,
        const QVector<PhraseMarker>& phrases,
        std::int64_t durationMs,
        bool entry) {
    QVector<TransitionWindow> out;
    const int n = energy.size();
    if (n == 0 || phrases.isEmpty() || durationMs <= 0) {
        return out;
    }
    const double entryMaxMs = 0.30 * durationMs;
    const double exitMinMs = 0.60 * durationMs;

    // Selection and confidence are different questions, and conflating them was
    // a bug: ranking by headroom also dragged the stored confidence down, so a
    // perfectly usable high-energy groove scored ~0.3 and chooseType's 0.5 gate
    // dropped the pair to a plain Crossfade. So: PICK by headroom (prefer the
    // dip), but STORE confidence as stability×length (is this a usable mixing
    // point at all — a stable window is, breakdown or not).
    struct Candidate {
        TransitionWindow window;
        double selectionScore = 0.0;
    };
    QVector<Candidate> candidates;
    for (int p = 0; p < phrases.size(); ++p) {
        const std::int64_t startMs = phrases[p].startMs;
        if (entry && static_cast<double>(startMs) > entryMaxMs) {
            continue;
        }
        if (!entry && static_cast<double>(startMs) < exitMinMs) {
            continue;
        }

        // Merge consecutive phrases until the window reaches the preferred
        // length; a 16-bar phrase grid then yields a 32-bar window.
        int bars = 0;
        int lastPhrase = p;
        for (int q = p; q < phrases.size() && bars < kPreferredWindowBars; ++q) {
            bars += phrases[q].bars;
            lastPhrase = q;
        }
        const std::int64_t endMs = (lastPhrase + 1 < phrases.size())
                ? phrases[lastPhrase + 1].startMs
                : durationMs;
        if (endMs <= startMs || bars < kMinWindowBars) {
            continue; // a tail stub is not a usable blend
        }

        int i0 = static_cast<int>(static_cast<double>(startMs) / durationMs * n);
        int i1 = static_cast<int>(static_cast<double>(endMs) / durationMs * n);
        i0 = std::clamp(i0, 0, n - 1);
        i1 = std::clamp(i1, i0 + 1, n);

        double sum = 0.0;
        for (int i = i0; i < i1; ++i) {
            sum += energy[i];
        }
        const double mean = sum / (i1 - i0);
        double variance = 0.0;
        for (int i = i0; i < i1; ++i) {
            const double d = energy[i] - mean;
            variance += d * d;
        }
        variance /= (i1 - i0);
        const float stability = static_cast<float>(1.0 - std::min(1.0, variance / 0.05));

        // A short window is inherently less useful and also looks artificially
        // stable (fewer samples), so weight it down instead of letting it win.
        const double lengthFactor =
                std::min(1.0, static_cast<double>(bars) / kPreferredWindowBars);

        // SELECTION: prefer the DIP. Two reference sets (see
        // music-sync-ai/reference-analysis) hand over on breakdowns — ~90-96% of
        // their energy dips coincide with the transition — because a breakdown is
        // where the outgoing track thins out, leaving room for the incoming one.
        // `energy` is normalized to the track's own peak, so headroom = how far
        // below that peak the window sits (a breakdown/outro scores high).
        const double headroom = std::clamp(1.0 - mean, 0.0, 1.0);
        const double selectionScore = lengthFactor *
                (kWindowHeadroomWeight * headroom +
                        (1.0 - kWindowHeadroomWeight) * stability);

        TransitionWindow window;
        window.kind = entry ? QStringLiteral("entry") : QStringLiteral("exit");
        window.startMs = startMs;
        window.endMs = endMs;
        window.bars = bars;
        window.energy = static_cast<float>(mean);
        window.energyStability = stability;
        window.instrumentalScore = 0.5f; // vocal analysis is a later phase
        // CONFIDENCE: usability, not preference. A stable, full-length window is
        // a usable mixing point whatever its energy, so this stays stability×
        // length — the value chooseType's gate was tuned against.
        window.confidence = static_cast<float>(stability * lengthFactor);
        candidates.append({window, selectionScore});
    }

    std::sort(candidates.begin(),
            candidates.end(),
            [](const Candidate& a, const Candidate& b) {
                if (a.selectionScore != b.selectionScore) {
                    return a.selectionScore > b.selectionScore;
                }
                return a.window.startMs < b.window.startMs; // deterministic
            });
    for (int i = 0; i < candidates.size() && i < kMaxTransitionWindows; ++i) {
        out.append(candidates[i].window);
    }
    return out;
}

void AdvancedAnalysisAdapter::compute(
        const TrackPointer& pTrack, std::int64_t durationMs, TrackFeatures* pFeatures) {
    if (!pTrack || pFeatures == nullptr) {
        return;
    }
    pFeatures->advancedAnalyzerVersion = kAnalyzerVersion;

    // Energy / bass from the coarse waveform summary (~1920 buckets, cheap).
    const ConstWaveformPointer pWaveform = pTrack->getWaveformSummary();
    if (!pWaveform.isNull() && pWaveform->getDataSize() >= 2) {
        const int frameCount = pWaveform->getDataSize() / 2; // stereo interleaved
        QVector<BandSample> frames;
        frames.reserve(frameCount);
        const WaveformData* data = pWaveform->data();
        for (int f = 0; f < frameCount; ++f) {
            const WaveformData& left = data[2 * f];
            const WaveformData& right = data[2 * f + 1];
            BandSample sample;
            sample.low = 0.5f * (left.filtered.low + right.filtered.low);
            sample.mid = 0.5f * (left.filtered.mid + right.filtered.mid);
            sample.high = 0.5f * (left.filtered.high + right.filtered.high);
            frames.append(sample);
        }
        const EnergyCurves curves = computeCurves(frames, kEnergyCurvePoints);
        pFeatures->energyCurve = curves.energy;
        pFeatures->bassCurve = curves.bass;
        pFeatures->overallEnergy = curves.overallEnergy;
    }

    // Phrases from the constant-tempo grid.
    const mixxx::BeatsPointer pBeats = pTrack->getBeats();
    const double bpm = pTrack->getBpm();
    if (pBeats && bpm > 0.0) {
        const double sampleRate = pTrack->getSampleRate().toDouble();
        const mixxx::audio::FramePos firstBeat = pBeats->firstBeat();
        double firstBeatMs = 0.0;
        if (firstBeat.isValid() && sampleRate > 0.0) {
            firstBeatMs = firstBeat.value() / sampleRate * 1000.0;
        }
        pFeatures->phrases =
                computePhrases(firstBeatMs, bpm, durationMs, kBeatsPerBar, kBarsPerPhrase);
    }

    // Sections + transition windows derived from the energy curve and phrases.
    pFeatures->sections = computeSections(pFeatures->energyCurve, durationMs);
    pFeatures->entryWindows = computeTransitionWindows(
            pFeatures->energyCurve, pFeatures->phrases, durationMs, /*entry=*/true);
    pFeatures->exitWindows = computeTransitionWindows(
            pFeatures->energyCurve, pFeatures->phrases, durationMs, /*entry=*/false);
}

} // namespace mixxx::music_sync
