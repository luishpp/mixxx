#include "music_sync/preview/deck_adapter.h"

#include "control/controlproxy.h"
#include "engine/channels/enginechannel.h"
#include "mixer/playermanager.h"

namespace {
// EQ low band. The [ChannelN],filterLow aliases point at the same controls, but
// this effect-slot path is the one Mixxx itself drives.
QString eqLowGroup(const QString& channelGroup) {
    return QStringLiteral("[EqualizerRack1_%1_Effect1]").arg(channelGroup);
}
QString filterGroup(const QString& channelGroup) {
    return QStringLiteral("[QuickEffectRack1_%1]").arg(channelGroup);
}
} // anonymous namespace

namespace mixxx::music_sync {

DeckAdapter::DeckAdapter(int deckIndex, QObject* parent)
        : QObject(parent),
          m_deckIndex(deckIndex),
          m_group(PlayerManager::groupForDeck(deckIndex)) {
    m_pPlay = new ControlProxy(m_group, QStringLiteral("play"), this);
    m_pPlayPosition = new ControlProxy(m_group, QStringLiteral("playposition"), this);
    m_pTrackSamples = new ControlProxy(m_group, QStringLiteral("track_samples"), this);
    m_pBpm = new ControlProxy(m_group, QStringLiteral("bpm"), this);
    m_pSyncEnabled = new ControlProxy(m_group, QStringLiteral("sync_enabled"), this);
    m_pSyncPhase = new ControlProxy(m_group, QStringLiteral("beatsync_phase"), this);
    m_pQuantize = new ControlProxy(m_group, QStringLiteral("quantize"), this);
    m_pVolume = new ControlProxy(m_group, QStringLiteral("volume"), this);
    m_pOrientation = new ControlProxy(m_group, QStringLiteral("orientation"), this);
    m_pEqLow = new ControlProxy(eqLowGroup(m_group), QStringLiteral("parameter1"), this);
    m_pFilter = new ControlProxy(filterGroup(m_group), QStringLiteral("super1"), this);
}

bool DeckAdapter::valid() const {
    return m_pPlay && m_pPlay->valid() && m_pPlayPosition && m_pPlayPosition->valid() &&
            m_pVolume && m_pVolume->valid();
}

bool DeckAdapter::isLoaded() const {
    return m_pTrackSamples && m_pTrackSamples->get() > 0.0;
}

double DeckAdapter::position() const {
    return m_pPlayPosition ? m_pPlayPosition->get() : 0.0;
}

void DeckAdapter::seek(double pos01) {
    if (m_pPlayPosition) {
        m_pPlayPosition->set(pos01);
    }
}

double DeckAdapter::bpm() const {
    return m_pBpm ? m_pBpm->get() : 0.0;
}

bool DeckAdapter::isPlaying() const {
    return m_pPlay && m_pPlay->toBool();
}

void DeckAdapter::setPlaying(bool playing) {
    if (m_pPlay) {
        m_pPlay->set(playing ? 1.0 : 0.0);
    }
}

void DeckAdapter::setSync(bool enabled) {
    if (m_pSyncEnabled) {
        m_pSyncEnabled->set(enabled ? 1.0 : 0.0);
    }
}

void DeckAdapter::setQuantize(bool enabled) {
    if (m_pQuantize && m_pQuantize->valid()) {
        m_pQuantize->set(enabled ? 1.0 : 0.0);
    }
}

void DeckAdapter::syncPhase() {
    if (m_pSyncPhase && m_pSyncPhase->valid()) {
        m_pSyncPhase->set(1.0); // push button: firing it once performs the align
    }
}

void DeckAdapter::setVolume(double volume) {
    if (m_pVolume) {
        m_pVolume->set(volume);
    }
}

void DeckAdapter::setEqLow(double value) {
    if (m_pEqLow && m_pEqLow->valid()) {
        m_pEqLow->set(value);
    }
}

void DeckAdapter::setFilter(double value) {
    if (m_pFilter && m_pFilter->valid()) {
        m_pFilter->set(value);
    }
}

void DeckAdapter::setOrientation(int orientation) {
    if (m_pOrientation) {
        m_pOrientation->set(orientation);
    }
}

void DeckAdapter::handBackToUser() {
    setSync(false);
    setEqLow(1.0);
    setFilter(0.5);
    setOrientation(EngineChannel::CENTER);
    setVolume(1.0);
}

} // namespace mixxx::music_sync

#include "moc_deck_adapter.cpp"
