#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

#include "music_sync/domain/set_program.h"
#include "track/track_decl.h"

class ControlProxy;
class PlayerManager;
class QTimer;

namespace mixxx::music_sync {

class DeckAdapter;

/// Runs a whole SetProgram on the real decks (spec 21): plays each track to its
/// handover point, fires the pre-compiled transition, then prepares the next
/// track on the deck that just freed up.
///
/// Everything is planned by SetCompiler beforehand — this only reads playback
/// position and writes controls, all on the GUI thread (spec 21.3 / RNF-002).
class SetExecutor : public QObject {
    Q_OBJECT
  public:
    /// Spec 21.1. Kept as a flat enum so the panel can show it verbatim.
    enum class State {
        Idle,
        Preparing,     // loading and cueing the first pair
        Playing,       // the live track runs toward its handover point
        Transitioning, // a transition's automation is running
        PreparingNext, // loading the following track on the freed deck
        Paused,
        ManualOverride,
        Completed,
        Cancelled,
        Failed,
    };
    Q_ENUM(State)

    explicit SetExecutor(std::shared_ptr<PlayerManager> pPlayerManager,
            QObject* parent = nullptr);
    ~SetExecutor() override;

    State state() const {
        return m_state;
    }
    int currentPosition() const {
        return m_current;
    }
    bool controlsAvailable() const;

    /// Whether the live track has reached its handover point. Pure and static:
    /// this is the decision the whole set hinges on, so it is testable without
    /// an engine.
    static bool reachedExit(double pos01, std::int64_t exitMs, std::int64_t durationMs);

    /// Whether a running transition should be finalized: the blend finished, OR a
    /// safety net tripped — the source reached its end, or the wall clock ran well
    /// past the expected length. Pure and static so the "a stuck transition never
    /// freezes the set" guarantee is testable without an engine.
    static bool transitionComplete(double beats,
            double durationBeats,
            double sourcePos01,
            std::int64_t elapsedMs,
            double expectedMs);

    /// Starts the set. `resolve` supplies the TrackPointer for a track id;
    /// tracks are loaded lazily, one deck ahead, never all at once.
    void start(const SetProgram& program,
            const QVector<TrackPointer>& tracksByPosition);

    void pause();
    void resume();
    void skip();   // hand over to the next track immediately
    void cancel(); // stop automating, leave the audio playing

  signals:
    void stateChanged(int state, const QString& message);
    void positionChanged(int position, const QString& what);
    /// The track at `position` just became the live one, playing at `bpm` (spec
    /// RF-015: its entry timestamp and effective tempo). Fires for the opener and
    /// after every completed handover.
    void trackLive(int position, double bpm);
    /// The handover out of `fromPosition` just started (its transition timestamp).
    void transitionBegan(int fromPosition);

  private slots:
    void onTick();
    void onCrossfaderChanged(double value);

  private:
    void setState(State state, const QString& message = QString());
    DeckAdapter* deckFor(int position);
    void prepare(int position);   // load + cue the track at `position`
    void beginTransition();
    void finishTransition();
    void applyWrite(const ControlWrite& write, int sourceDeck, int targetDeck);
    void handBackAllDecks();

    std::shared_ptr<PlayerManager> m_pPlayerManager;
    QVector<DeckAdapter*> m_decks;
    ControlProxy* m_pCrossfader = nullptr;
    QTimer* m_pTimer = nullptr;

    State m_state = State::Idle;
    SetProgram m_program;
    QVector<TrackPointer> m_tracks;

    int m_current = 0;       // position of the live track
    int m_nextWrite = 0;     // index into the running transition's writes
    double m_startPos01 = 0.0;
    double m_refBpm = 0.0;
    int m_preparedUpTo = -1;    // highest position already loaded/cued
    int m_cuedSeekedUpTo = -1;  // highest position pre-seeked to its entry point
    double m_lastCrossfaderSet = 0.0;
    QElapsedTimer m_transitionClock; // wall-clock since the current transition began
};

} // namespace mixxx::music_sync
