#include "music_sync/analysis/advanced_analysis_adapter.h"

#include <algorithm>

#include "audio/frame.h"
#include "track/beats.h"
#include "track/track.h"
#include "waveform/waveform.h"

namespace mixxx::music_sync {

const QString AdvancedAnalysisAdapter::kAnalyzerVersion = QStringLiteral("advanced-0.1.0");

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
}

} // namespace mixxx::music_sync
