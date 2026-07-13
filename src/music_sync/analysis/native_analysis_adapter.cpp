#include "music_sync/analysis/native_analysis_adapter.h"

#include <QFileInfo>
#include <cmath>
#include <optional>

#include "audio/frame.h"
#include "proto/keys.pb.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/cueinfo.h"
#include "track/keys.h"
#include "track/keyutils.h"
#include "track/replaygain.h"
#include "track/track.h"
#include "track/trackid.h"

namespace {

/// Converts a Mixxx frame position to milliseconds, or nullopt when either the
/// position or the sample rate is not usable.
std::optional<std::int64_t> framePosToMs(mixxx::audio::FramePos pos, double sampleRate) {
    if (!pos.isValid() || sampleRate <= 0.0) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(std::llround(pos.value() / sampleRate * 1000.0));
}

} // anonymous namespace

namespace mixxx::music_sync {

const QString NativeAnalysisAdapter::kAnalyzerVersion = QStringLiteral("native-0.1.0");

bool NativeAnalysisAdapter::needsAnalysis(const TrackPointer& pTrack) {
    if (!pTrack) {
        return false;
    }
    const bool noBeats = (pTrack->getBeats() == nullptr) || pTrack->getBpm() <= 0.0;
    const bool noKey = (pTrack->getKey() == mixxx::track::io::key::INVALID);
    return noBeats || noKey;
}

TrackFeatures NativeAnalysisAdapter::extract(const TrackPointer& pTrack) {
    TrackFeatures f;
    if (!pTrack) {
        return f;
    }

    const TrackId id = pTrack->getId();
    f.mixxxTrackId = id.isValid() ? static_cast<std::int64_t>(id.toVariant().toLongLong()) : -1;
    f.location = pTrack->getLocation();
    f.fileSize = f.location.isEmpty() ? 0 : QFileInfo(f.location).size();

    f.title = pTrack->getTitle();
    f.artist = pTrack->getArtist();
    f.album = pTrack->getAlbum();
    f.genre = pTrack->getGenre();

    const double sampleRate = pTrack->getSampleRate().toDouble();
    f.durationMs = static_cast<std::int64_t>(std::llround(pTrack->getDuration() * 1000.0));
    f.sampleRate = static_cast<int>(pTrack->getSampleRate().value());
    f.channels = pTrack->getChannels();
    f.bitrateKbps = pTrack->getBitrate();

    f.bpm = pTrack->getBpm();

    const mixxx::track::io::key::ChromaticKey key = pTrack->getKey();
    f.keyChromatic = static_cast<int>(key);
    f.keyText = KeyUtils::keyToString(key, KeyUtils::KeyNotation::Traditional);
    f.camelot = KeyUtils::keyToString(key, KeyUtils::KeyNotation::Lancelot);

    const mixxx::ReplayGain replayGain = pTrack->getReplayGain();
    f.replaygainRatio = replayGain.hasRatio() ? replayGain.getRatio() : 0.0;

    const mixxx::BeatsPointer pBeats = pTrack->getBeats();
    f.hasBeatgrid = static_cast<bool>(pBeats);

    if (const CuePointer pIntro = pTrack->findCueByType(mixxx::CueType::Intro)) {
        const Cue::StartAndEndPositions se = pIntro->getStartAndEndPosition();
        f.introStartMs = framePosToMs(se.startPosition, sampleRate);
        f.introEndMs = framePosToMs(se.endPosition, sampleRate);
    }
    if (const CuePointer pOutro = pTrack->findCueByType(mixxx::CueType::Outro)) {
        const Cue::StartAndEndPositions se = pOutro->getStartAndEndPosition();
        f.outroStartMs = framePosToMs(se.startPosition, sampleRate);
        f.outroEndMs = framePosToMs(se.endPosition, sampleRate);
    }

    const bool hasKey = (key != mixxx::track::io::key::INVALID);
    f.analyzed = f.hasBeatgrid && f.bpm > 0.0 && hasKey;
    f.analyzerVersion = kAnalyzerVersion;

    return f;
}

} // namespace mixxx::music_sync
