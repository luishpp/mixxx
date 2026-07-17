#include "music_sync/preview/preview_executor.h"

#include <QTimer>
#include <algorithm>
#include <cmath>

#include "control/controlproxy.h"
#include "engine/channels/enginechannel.h"
#include "mixer/playermanager.h"
#include "music_sync/preview/deck_adapter.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync.preview");

constexpr int kTickMs = 25;
constexpr int kLoadPollMs = 50;
constexpr int kMaxLoadPolls = 120; // ~6 s before giving up on a load
constexpr double kCrossfaderUserTolerance = 0.02;

} // namespace

namespace mixxx::music_sync {

PreviewExecutor::PreviewExecutor(std::shared_ptr<PlayerManager> pPlayerManager,
        int sourceDeckIndex,
        int targetDeckIndex,
        QObject* parent)
        : QObject(parent), m_pPlayerManager(std::move(pPlayerManager)) {
    m_pSource = new DeckAdapter(sourceDeckIndex, this);
    m_pTarget = new DeckAdapter(targetDeckIndex, this);
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
    return m_pCrossfader && m_pCrossfader->valid() && m_pSource && m_pSource->valid() &&
            m_pTarget && m_pTarget->valid();
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

    m_pPlayerManager->slotLoadTrackToPlayer(pSource, m_pSource->group(), false);
    m_pPlayerManager->slotLoadTrackToPlayer(pTarget, m_pTarget->group(), false);
    setState(State::Loading, QStringLiteral("Loading pair into decks"));
    m_pLoadTimer->start();
}

void PreviewExecutor::pollLoaded() {
    ++m_loadPolls;
    const bool sourceReady = m_pSource->isLoaded();
    const bool targetReady = m_pTarget->isLoaded();
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
    m_pSource->setPlaying(false);
    m_pTarget->setPlaying(false);

    m_pSource->setOrientation(EngineChannel::LEFT);
    m_pTarget->setOrientation(EngineChannel::RIGHT);

    const double sourcePos =
            std::clamp(m_program.sourceStartMs / m_sourceDurationMs, 0.0, 1.0);
    const double targetPos =
            std::clamp(m_program.targetStartMs / m_targetDurationMs, 0.0, 1.0);
    // Quantize BEFORE seeking: the planned entry is a millisecond, and landing
    // between beats is how a tempo-locked deck still comes in off-beat.
    m_pSource->setQuantize(true);
    m_pTarget->setQuantize(true);
    m_pSource->seek(sourcePos);
    m_pTarget->seek(targetPos);

    m_pSource->setVolume(1.0);
    m_pTarget->setVolume(0.0);
    m_pSource->setEqLow(1.0);
    m_pTarget->setEqLow(0.0); // pre-kill B bass; the program restores it mid-swap
    m_pSource->setFilter(0.5);
    m_pTarget->setFilter(0.5);
    m_lastCrossfaderSet = -1.0;
    m_pCrossfader->set(-1.0); // fully on the source deck
}

void PreviewExecutor::begin() {
    m_nextWrite = 0;
    m_startPos01 = m_pSource->position();
    m_refBpm = m_pSource->bpm() > 0.0 ? m_pSource->bpm() : 128.0;

    // Only tempo-lock when the transition actually overlaps both tracks. A cut
    // (or the Auto DJ fallback) is chosen precisely BECAUSE the tempos clash;
    // syncing there would drag the incoming track to a foreign tempo — e.g. a
    // 140 BPM track pulled to 112 is a 20% stretch — which is the opposite of
    // what a cut is for (spec 16 / 19.6).
    const bool beatSync = m_program.needsBeatSync();
    m_pTarget->setSync(beatSync);
    m_pSource->setPlaying(true);
    m_pTarget->setPlaying(true);
    if (beatSync) {
        // Tempo lock equalises the BPM; this is what puts the downbeats on top
        // of each other.
        m_pTarget->syncPhase();
    }

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
    const double beats = elapsedBeats(m_pSource->position(),
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
    m_pSource->setPlaying(false); // the outgoing deck is done
    handControlsToUser();
    m_lastCrossfaderSet = 0.0;
    m_pCrossfader->set(0.0);
    setState(State::Completed, QStringLiteral("Transition complete"));
}

void PreviewExecutor::handControlsToUser() {
    m_pSource->setSync(false);
    m_pTarget->setSync(false);
    m_pSource->setEqLow(1.0);
    m_pTarget->setEqLow(1.0);
    m_pSource->setFilter(0.5);
    m_pTarget->setFilter(0.5);
    m_pSource->setOrientation(EngineChannel::CENTER);
    m_pTarget->setOrientation(EngineChannel::CENTER);
    m_pSource->setVolume(1.0);
    m_pTarget->setVolume(1.0);
}

void PreviewExecutor::applyWrite(const ControlWrite& write) {
    const QString& control = write.control;
    if (control == QStringLiteral("targetPlay")) {
        m_pTarget->setPlaying(write.value >= 0.5);
    } else if (control == QStringLiteral("targetVolume")) {
        m_pTarget->setVolume(write.value);
    } else if (control == QStringLiteral("sourceVolume")) {
        m_pSource->setVolume(write.value);
    } else if (control == QStringLiteral("targetLowEq")) {
        m_pTarget->setEqLow(write.value);
    } else if (control == QStringLiteral("sourceLowEq")) {
        m_pSource->setEqLow(write.value);
    } else if (control == QStringLiteral("sourceFilter")) {
        m_pSource->setFilter(write.value);
    } else if (control == QStringLiteral("targetFilter")) {
        m_pTarget->setFilter(write.value);
    } else if (control == QStringLiteral("crossfader")) {
        m_lastCrossfaderSet = write.value;
        m_pCrossfader->set(write.value);
    }
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
