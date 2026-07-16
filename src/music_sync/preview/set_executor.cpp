#include "music_sync/preview/set_executor.h"

#include <QTimer>
#include <algorithm>
#include <cmath>

#include "control/controlproxy.h"
#include "engine/channels/enginechannel.h"
#include "mixer/playermanager.h"
#include "music_sync/planner/set_compiler.h"
#include "music_sync/preview/deck_adapter.h"
#include "music_sync/preview/preview_executor.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync.set");
constexpr int kTickMs = 25;
constexpr double kCrossfaderUserTolerance = 0.02;
} // anonymous namespace

namespace mixxx::music_sync {

SetExecutor::SetExecutor(std::shared_ptr<PlayerManager> pPlayerManager, QObject* parent)
        : QObject(parent), m_pPlayerManager(std::move(pPlayerManager)) {
    for (int i = 0; i < SetCompiler::kDeckCount; ++i) {
        m_decks.append(new DeckAdapter(i, this));
    }
    m_pCrossfader = new ControlProxy(
            QStringLiteral("[Master]"), QStringLiteral("crossfader"), this);
    m_pCrossfader->connectValueChanged(
            this, &SetExecutor::onCrossfaderChanged, Qt::AutoConnection);
    m_pTimer = new QTimer(this);
    m_pTimer->setInterval(kTickMs);
    connect(m_pTimer, &QTimer::timeout, this, &SetExecutor::onTick);
}

SetExecutor::~SetExecutor() = default;

bool SetExecutor::controlsAvailable() const {
    if (!m_pCrossfader || !m_pCrossfader->valid()) {
        return false;
    }
    for (const DeckAdapter* deck : m_decks) {
        if (!deck->valid()) {
            return false;
        }
    }
    return true;
}

bool SetExecutor::reachedExit(double pos01, std::int64_t exitMs, std::int64_t durationMs) {
    if (durationMs <= 0 || exitMs <= 0) {
        return false;
    }
    return pos01 * static_cast<double>(durationMs) >= static_cast<double>(exitMs);
}

void SetExecutor::setState(State state, const QString& message) {
    m_state = state;
    if (!message.isEmpty()) {
        kLogger.info() << "Set state" << static_cast<int>(state) << message;
    }
    emit stateChanged(static_cast<int>(state), message);
}

DeckAdapter* SetExecutor::deckFor(int position) {
    if (position < 0 || position >= m_program.items.size()) {
        return nullptr;
    }
    return m_decks.at(m_program.items.at(position).deckIndex % m_decks.size());
}

void SetExecutor::prepare(int position) {
    if (position < 0 || position >= m_program.items.size() ||
            position >= m_tracks.size() || position <= m_preparedUpTo) {
        return;
    }
    DeckAdapter* pDeck = deckFor(position);
    const TrackPointer pTrack = m_tracks.at(position);
    if (!pDeck || !pTrack) {
        return;
    }
    const SetItem& item = m_program.items.at(position);
    m_pPlayerManager->slotLoadTrackToPlayer(pTrack, pDeck->group(), false);
    pDeck->setOrientation(EngineChannel::CENTER);
    pDeck->setVolume(position == 0 ? 1.0 : 0.0);
    // Everything after the opener comes in with its bass out of the way; the
    // transition's automation brings it back.
    pDeck->setEqLow(position == 0 ? 1.0 : 0.0);
    m_preparedUpTo = position;
    kLogger.info() << "Prepared position" << position << "on" << pDeck->group()
                   << item.artist << "-" << item.title;
    emit positionChanged(position, QStringLiteral("%1 - %2").arg(item.artist, item.title));
}

void SetExecutor::start(const SetProgram& program, const QVector<TrackPointer>& tracksByPosition) {
    if (!m_pPlayerManager || !controlsAvailable()) {
        setState(State::Failed, QStringLiteral("Deck controls unavailable"));
        return;
    }
    if (program.items.isEmpty() || tracksByPosition.size() < program.items.size()) {
        setState(State::Failed, QStringLiteral("Nothing to run"));
        return;
    }
    m_pTimer->stop();
    m_program = program;
    m_tracks = tracksByPosition;
    m_current = 0;
    m_nextWrite = 0;
    m_preparedUpTo = -1;

    setState(State::Preparing, QStringLiteral("Loading the first tracks"));
    prepare(0);
    prepare(1); // one deck ahead, always

    m_lastCrossfaderSet = -1.0;
    m_pCrossfader->set(-1.0);
    m_decks.at(m_program.items.at(0).deckIndex)->setOrientation(EngineChannel::LEFT);
    if (m_program.items.size() > 1) {
        m_decks.at(m_program.items.at(1).deckIndex)->setOrientation(EngineChannel::RIGHT);
    }
    m_pTimer->start();
    setState(State::Playing, QStringLiteral("Running the set"));
}

void SetExecutor::onTick() {
    if (m_state != State::Playing && m_state != State::Transitioning) {
        return;
    }
    DeckAdapter* pLive = deckFor(m_current);
    if (!pLive) {
        return;
    }
    const SetItem& item = m_program.items.at(m_current);

    if (m_state == State::Playing) {
        // Seek and start the live track once its deck is actually loaded.
        if (!pLive->isPlaying()) {
            if (!pLive->isLoaded()) {
                return; // still decoding
            }
            if (item.durationMs > 0) {
                pLive->seek(std::clamp(
                        static_cast<double>(item.startMs) / item.durationMs, 0.0, 1.0));
            }
            pLive->setVolume(1.0);
            pLive->setEqLow(1.0);
            pLive->setPlaying(true);
            return;
        }
        if (m_current >= m_program.transitions.size()) {
            // The last track: nothing follows, so let it play out.
            if (pLive->position() >= 0.999) {
                m_pTimer->stop();
                handBackAllDecks();
                setState(State::Completed, QStringLiteral("Set complete"));
            }
            return;
        }
        if (reachedExit(pLive->position(), item.exitMs, item.durationMs)) {
            beginTransition();
        }
        return;
    }

    // --- Transitioning ---
    const PreviewProgram& program = m_program.transitions.at(m_current).program;
    const double beats = PreviewExecutor::elapsedBeats(
            pLive->position(), m_startPos01, static_cast<double>(item.durationMs), m_refBpm);
    const int sourceDeck = m_program.items.at(m_current).deckIndex;
    const int targetDeck = m_program.items.at(m_current + 1).deckIndex;
    while (m_nextWrite < program.writes.size() &&
            program.writes.at(m_nextWrite).atBeat <= beats) {
        applyWrite(program.writes.at(m_nextWrite), sourceDeck, targetDeck);
        ++m_nextWrite;
    }
    if (beats >= program.durationBeats) {
        finishTransition();
    }
}

void SetExecutor::beginTransition() {
    DeckAdapter* pLive = deckFor(m_current);
    DeckAdapter* pNext = deckFor(m_current + 1);
    if (!pLive || !pNext) {
        return;
    }
    const PreviewProgram& program = m_program.transitions.at(m_current).program;
    const SetItem& next = m_program.items.at(m_current + 1);

    if (!pNext->isLoaded()) {
        kLogger.warning() << "Next deck not loaded yet; holding the handover";
        return; // spec 21.4: a track that is not loaded pauses the handover
    }
    if (next.durationMs > 0) {
        pNext->seek(std::clamp(
                static_cast<double>(next.startMs) / next.durationMs, 0.0, 1.0));
    }
    pNext->setSync(program.needsBeatSync());
    pNext->setPlaying(true);

    m_nextWrite = 0;
    m_startPos01 = pLive->position();
    m_refBpm = pLive->bpm() > 0.0 ? pLive->bpm() : 128.0;
    kLogger.info() << "Transition" << (m_current + 1) << "->" << (m_current + 2)
                   << transitionTypeName(program.type) << "beats=" << program.durationBeats
                   << "sync=" << program.needsBeatSync();
    setState(State::Transitioning,
            QStringLiteral("Transition %1 -> %2 (%3)")
                    .arg(m_current + 1)
                    .arg(m_current + 2)
                    .arg(transitionTypeName(program.type)));
}

void SetExecutor::finishTransition() {
    const PreviewProgram& program = m_program.transitions.at(m_current).program;
    const int sourceDeck = m_program.items.at(m_current).deckIndex;
    const int targetDeck = m_program.items.at(m_current + 1).deckIndex;
    while (m_nextWrite < program.writes.size()) {
        applyWrite(program.writes.at(m_nextWrite), sourceDeck, targetDeck);
        ++m_nextWrite;
    }
    DeckAdapter* pOld = deckFor(m_current);
    if (pOld) {
        pOld->setPlaying(false);
        pOld->handBackToUser();
    }

    // The deck that just came in is now the live one, and it owns the left side
    // of the crossfader so the next handover runs the same way.
    ++m_current;
    DeckAdapter* pLive = deckFor(m_current);
    if (pLive) {
        pLive->setSync(false);
        pLive->setVolume(1.0);
        pLive->setEqLow(1.0);
        pLive->setOrientation(EngineChannel::LEFT);
    }
    m_lastCrossfaderSet = -1.0;
    m_pCrossfader->set(-1.0);

    if (m_current + 1 < m_program.items.size()) {
        // Free deck: load the track after next and point it right.
        prepare(m_current + 1);
        DeckAdapter* pNext = deckFor(m_current + 1);
        if (pNext) {
            pNext->setOrientation(EngineChannel::RIGHT);
        }
    }
    const SetItem& item = m_program.items.at(m_current);
    emit positionChanged(m_current, QStringLiteral("%1 - %2").arg(item.artist, item.title));
    setState(State::Playing,
            QStringLiteral("Playing %1/%2: %3")
                    .arg(m_current + 1)
                    .arg(m_program.items.size())
                    .arg(item.title));
}

void SetExecutor::applyWrite(const ControlWrite& write, int sourceDeck, int targetDeck) {
    DeckAdapter* pSource = m_decks.at(sourceDeck % m_decks.size());
    DeckAdapter* pTarget = m_decks.at(targetDeck % m_decks.size());
    const QString& control = write.control;
    if (control == QStringLiteral("targetPlay")) {
        pTarget->setPlaying(write.value >= 0.5);
    } else if (control == QStringLiteral("targetVolume")) {
        pTarget->setVolume(write.value);
    } else if (control == QStringLiteral("sourceVolume")) {
        pSource->setVolume(write.value);
    } else if (control == QStringLiteral("targetLowEq")) {
        pTarget->setEqLow(write.value);
    } else if (control == QStringLiteral("sourceLowEq")) {
        pSource->setEqLow(write.value);
    } else if (control == QStringLiteral("sourceFilter")) {
        pSource->setFilter(write.value);
    } else if (control == QStringLiteral("targetFilter")) {
        pTarget->setFilter(write.value);
    } else if (control == QStringLiteral("crossfader")) {
        m_lastCrossfaderSet = write.value;
        m_pCrossfader->set(write.value);
    }
}

void SetExecutor::handBackAllDecks() {
    for (DeckAdapter* deck : m_decks) {
        deck->handBackToUser();
    }
}

void SetExecutor::pause() {
    if (m_state != State::Playing && m_state != State::Transitioning) {
        return;
    }
    m_pTimer->stop();
    for (DeckAdapter* deck : m_decks) {
        deck->setPlaying(false);
    }
    setState(State::Paused, QStringLiteral("Paused"));
}

void SetExecutor::resume() {
    if (m_state != State::Paused) {
        return;
    }
    DeckAdapter* pLive = deckFor(m_current);
    if (pLive) {
        pLive->setPlaying(true);
    }
    // A transition that was mid-flight resumes from the position it reached;
    // the writes already applied stay applied.
    m_pTimer->start();
    setState(State::Playing, QStringLiteral("Resumed"));
}

void SetExecutor::skip() {
    if (m_state != State::Playing || m_current >= m_program.transitions.size()) {
        return;
    }
    kLogger.info() << "Skipping to position" << (m_current + 2);
    beginTransition();
}

void SetExecutor::cancel() {
    m_pTimer->stop();
    // Spec 21.4: stop automating without cutting the audio — the decks keep
    // playing and the user takes over.
    handBackAllDecks();
    setState(State::Cancelled, QStringLiteral("Set cancelled; decks are yours"));
}

void SetExecutor::onCrossfaderChanged(double value) {
    if (m_state != State::Playing && m_state != State::Transitioning) {
        return;
    }
    if (std::abs(value - m_lastCrossfaderSet) < kCrossfaderUserTolerance) {
        return; // our own automation write
    }
    m_pTimer->stop();
    handBackAllDecks();
    setState(State::ManualOverride,
            QStringLiteral("Manual override: crossfader moved; decks are yours"));
}

} // namespace mixxx::music_sync

#include "moc_set_executor.cpp"
