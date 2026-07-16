#include "music_sync/preview/preview_executor.h"

#include <QTimer>
#include <algorithm>
#include <cmath>

#include "control/controlproxy.h"
#include "engine/channels/enginechannel.h"
#include "mixer/playermanager.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync.preview");

constexpr int kTickMs = 25;
constexpr int kLoadPollMs = 50;
constexpr int kMaxLoadPolls = 120; // ~6 s before giving up on a load
constexpr double kCrossfaderUserTolerance = 0.02;

// EQ low band (canonical effect-slot control): 0 kill .. 1 unity .. 4 boost.
QString eqLowGroup(const QString& channelGroup) {
    return QStringLiteral("[EqualizerRack1_%1_Effect1]").arg(channelGroup);
}

// QuickEffect (filter) super knob: 0..1, 0.5 neutral.
QString filterGroup(const QString& channelGroup) {
    return QStringLiteral("[QuickEffectRack1_%1]").arg(channelGroup);
}
} // namespace

namespace mixxx::music_sync {

PreviewExecutor::PreviewExecutor(std::shared_ptr<PlayerManager> pPlayerManager,
        int sourceDeckIndex,
        int targetDeckIndex,
        QObject* parent)
        : QObject(parent),
          m_pPlayerManager(std::move(pPlayerManager)),
          m_sourceDeckIndex(sourceDeckIndex),
          m_targetDeckIndex(targetDeckIndex),
          m_sourceGroup(PlayerManager::groupForDeck(sourceDeckIndex)),
          m_targetGroup(PlayerManager::groupForDeck(targetDeckIndex)) {
    const auto makeDeck = [this](const QString& group) {
        DeckControls d;
        d.play = new ControlProxy(group, QStringLiteral("play"), this);
        d.playPosition = new ControlProxy(group, QStringLiteral("playposition"), this);
        d.trackSamples = new ControlProxy(group, QStringLiteral("track_samples"), this);
        d.bpm = new ControlProxy(group, QStringLiteral("bpm"), this);
        d.syncEnabled = new ControlProxy(group, QStringLiteral("sync_enabled"), this);
        d.volume = new ControlProxy(group, QStringLiteral("volume"), this);
        d.orientation = new ControlProxy(group, QStringLiteral("orientation"), this);
        d.eqLow = new ControlProxy(
                eqLowGroup(group), QStringLiteral("parameter1"), this);
        d.filter = new ControlProxy(filterGroup(group), QStringLiteral("super1"), this);
        return d;
    };
    m_source = makeDeck(m_sourceGroup);
    m_target = makeDeck(m_targetGroup);
    m_pCrossfader = new ControlProxy(
            QStringLiteral("[Master]"), QStringLiteral("crossfader"), this);
    m_pCrossfader->connectValueChanged(
            this, &PreviewExecutor::onCrossfaderChanged, Qt::AutoConnection);

    m_pTimer = new QTimer(this);
    m_pTimer->setInterval(kTickMs);
    connect(m_pTimer, &QTimer::timeout, this, &PreviewExecutor::onTick);

    m_pLoadTimer = new QTimer(this);
    m_pLoadTimer->setInterval(kLoadPollMs);
    connect(m_pLoadTimer, &QTimer::timeout, this, &PreviewExecutor::pollLoaded);
}

PreviewExecutor::~PreviewExecutor() = default;

bool PreviewExecutor::controlsAvailable() const {
    return m_pCrossfader && m_pCrossfader->valid() && m_source.play &&
            m_source.play->valid() && m_target.play && m_target.play->valid() &&
            m_source.playPosition && m_source.playPosition->valid() &&
            m_source.volume && m_source.volume->valid() && m_target.volume &&
            m_target.volume->valid();
}

void PreviewExecutor::setState(State state, const QString& message) {
    m_state = state;
    if (!message.isEmpty()) {
        kLogger.info() << "Preview state" << static_cast<int>(state) << message;
    }
    emit stateChanged(static_cast<int>(state), message);
}

void PreviewExecutor::preview(const PreviewProgram& program,
        TrackPointer pSource,
        TrackPointer pTarget,
        double sourceDurationMs,
        double targetDurationMs) {
    if (!m_pPlayerManager || !controlsAvailable()) {
        setState(State::Failed, QStringLiteral("Deck controls unavailable"));
        return;
    }
    if (!pSource || !pTarget) {
        setState(State::Failed, QStringLiteral("Missing source/target track"));
        return;
    }
    m_pTimer->stop();
    m_pLoadTimer->stop();

    m_program = program;
    m_sourceDurationMs = sourceDurationMs > 0.0 ? sourceDurationMs : 1.0;
    m_targetDurationMs = targetDurationMs > 0.0 ? targetDurationMs : 1.0;
    m_nextWrite = 0;
    m_loadPolls = 0;

    m_pPlayerManager->slotLoadTrackToPlayer(pSource, m_sourceGroup, false);
    m_pPlayerManager->slotLoadTrackToPlayer(pTarget, m_targetGroup, false);
    setState(State::Loading, QStringLiteral("Loading pair into decks"));
    m_pLoadTimer->start();
}

void PreviewExecutor::pollLoaded() {
    ++m_loadPolls;
    const bool sourceReady = m_source.trackSamples && m_source.trackSamples->get() > 0.0;
    const bool targetReady = m_target.trackSamples && m_target.trackSamples->get() > 0.0;
    if (sourceReady && targetReady) {
        m_pLoadTimer->stop();
        cueDecks();
        begin();
        return;
    }
    if (m_loadPolls >= kMaxLoadPolls) {
        m_pLoadTimer->stop();
        setState(State::Failed, QStringLiteral("Timed out loading tracks"));
    }
}

void PreviewExecutor::cueDecks() {
    m_source.play->set(0.0);
    m_target.play->set(0.0);

    m_source.orientation->set(EngineChannel::LEFT);
    m_target.orientation->set(EngineChannel::RIGHT);

    const double sourcePos =
            std::clamp(m_program.sourceStartMs / m_sourceDurationMs, 0.0, 1.0);
    const double targetPos =
            std::clamp(m_program.targetStartMs / m_targetDurationMs, 0.0, 1.0);
    m_source.playPosition->set(sourcePos);
    m_target.playPosition->set(targetPos);

    m_source.volume->set(1.0);
    m_target.volume->set(0.0);
    m_source.eqLow->set(1.0);
    m_target.eqLow->set(0.0); // pre-kill B bass; the program restores it mid-swap
    if (m_source.filter->valid()) {
        m_source.filter->set(0.5);
    }
    if (m_target.filter->valid()) {
        m_target.filter->set(0.5);
    }
    m_lastCrossfaderSet = -1.0;
    m_pCrossfader->set(-1.0); // fully on the source deck
}

void PreviewExecutor::begin() {
    m_nextWrite = 0;
    m_startPos01 = m_source.playPosition->get();
    m_refBpm = m_source.bpm && m_source.bpm->get() > 0.0 ? m_source.bpm->get() : 128.0;

    // Only tempo-lock when the transition actually overlaps both tracks. A cut
    // (or the Auto DJ fallback) is chosen precisely BECAUSE the tempos clash;
    // syncing there would drag the incoming track to a foreign tempo — e.g. a
    // 140 BPM track pulled to 112 is a 20% stretch — which is the opposite of
    // what a cut is for (spec 16 / 19.6).
    const bool beatSync = m_program.needsBeatSync();
    m_target.syncEnabled->set(beatSync ? 1.0 : 0.0);
    m_source.play->set(1.0);
    m_target.play->set(1.0);

    kLogger.info() << "Preview begin:"
                   << "type=" << transitionTypeName(m_program.type)
                   << "startPos01=" << m_startPos01
                   << "refBpm=" << m_refBpm
                   << "sourceDurationMs=" << m_sourceDurationMs
                   << "durationBeats=" << m_program.durationBeats
                   << "durationBars=" << (m_program.durationBeats / 4.0)
                   << "writes=" << m_program.writes.size()
                   << "expectedSeconds="
                   << (m_refBpm > 0.0 ? m_program.durationBeats * 60.0 / m_refBpm : -1.0);

    setState(State::Transitioning,
            beatSync ? QStringLiteral("Running transition (beat-synced)")
                     : QStringLiteral("Running transition (no sync: each track keeps its tempo)"));
    m_pTimer->start();
}

double PreviewExecutor::elapsedBeats(double pos01,
        double startPos01,
        double sourceDurationMs,
        double refBpm) {
    if (sourceDurationMs <= 0.0 || refBpm <= 0.0) {
        return 0.0;
    }
    const double elapsedMs = (pos01 - startPos01) * sourceDurationMs;
    if (elapsedMs <= 0.0) {
        return 0.0;
    }
    return elapsedMs * refBpm / 60000.0;
}

void PreviewExecutor::onTick() {
    if (m_state != State::Transitioning) {
        return;
    }
    const double beats = elapsedBeats(m_source.playPosition->get(),
            m_startPos01,
            m_sourceDurationMs,
            m_refBpm);

    while (m_nextWrite < m_program.writes.size() &&
            m_program.writes.at(m_nextWrite).atBeat <= beats) {
        applyWrite(m_program.writes.at(m_nextWrite));
        ++m_nextWrite;
    }
    if (beats >= m_program.durationBeats) {
        kLogger.debug() << "Preview finishing at beat" << beats << "of"
                        << m_program.durationBeats;
        finish();
    }
}

void PreviewExecutor::finish() {
    // Flush any remaining writes so controls land on their final values.
    while (m_nextWrite < m_program.writes.size()) {
        applyWrite(m_program.writes.at(m_nextWrite));
        ++m_nextWrite;
    }
    m_pTimer->stop();
    m_source.play->set(0.0); // the outgoing deck is done
    handControlsToUser();
    m_lastCrossfaderSet = 0.0;
    m_pCrossfader->set(0.0);
    setState(State::Completed, QStringLiteral("Transition complete"));
}

void PreviewExecutor::handControlsToUser() {
    m_source.syncEnabled->set(0.0);
    m_target.syncEnabled->set(0.0);
    m_source.eqLow->set(1.0);
    m_target.eqLow->set(1.0);
    if (m_source.filter->valid()) {
        m_source.filter->set(0.5);
    }
    if (m_target.filter->valid()) {
        m_target.filter->set(0.5);
    }
    m_source.orientation->set(EngineChannel::CENTER);
    m_target.orientation->set(EngineChannel::CENTER);
    m_source.volume->set(1.0);
    m_target.volume->set(1.0);
}

void PreviewExecutor::applyWrite(const ControlWrite& write) {
    if (write.control == QStringLiteral("targetPlay")) {
        m_target.play->set(write.value >= 0.5 ? 1.0 : 0.0);
        return;
    }
    ControlProxy* pProxy = resolve(write.control);
    if (!pProxy || !pProxy->valid()) {
        return;
    }
    if (write.control == QStringLiteral("crossfader")) {
        m_lastCrossfaderSet = write.value;
    }
    pProxy->set(write.value);
}

ControlProxy* PreviewExecutor::resolve(const QString& control) {
    if (control == QStringLiteral("crossfader")) {
        return m_pCrossfader;
    }
    if (control == QStringLiteral("targetVolume")) {
        return m_target.volume;
    }
    if (control == QStringLiteral("sourceVolume")) {
        return m_source.volume;
    }
    if (control == QStringLiteral("targetLowEq")) {
        return m_target.eqLow;
    }
    if (control == QStringLiteral("sourceLowEq")) {
        return m_source.eqLow;
    }
    if (control == QStringLiteral("sourceFilter")) {
        return m_source.filter;
    }
    if (control == QStringLiteral("targetFilter")) {
        return m_target.filter;
    }
    return nullptr;
}

void PreviewExecutor::onCrossfaderChanged(double value) {
    if (m_state != State::Transitioning) {
        return;
    }
    if (std::abs(value - m_lastCrossfaderSet) < kCrossfaderUserTolerance) {
        return; // our own automation write
    }
    // The user grabbed the crossfader: stop fighting them (spec 21.4 / RF-013).
    m_pTimer->stop();
    handControlsToUser();
    setState(State::ManualOverride, QStringLiteral("Manual override: crossfader moved"));
}

void PreviewExecutor::repeat() {
    if (!controlsAvailable() ||
            (m_state != State::Completed && m_state != State::Cancelled &&
                    m_state != State::ManualOverride)) {
        return;
    }
    cueDecks();
    begin();
}

void PreviewExecutor::cancel() {
    m_pTimer->stop();
    m_pLoadTimer->stop();
    if (m_state == State::Transitioning) {
        handControlsToUser();
    }
    setState(State::Cancelled, QStringLiteral("Preview cancelled"));
}

} // namespace mixxx::music_sync

#include "moc_preview_executor.cpp"
