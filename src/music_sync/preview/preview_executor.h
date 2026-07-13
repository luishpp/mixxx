#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include "music_sync/domain/preview_program.h"
#include "track/track_decl.h"

class ControlProxy;
class PlayerManager;
class QTimer;

namespace mixxx::music_sync {

/// Drives two real Mixxx decks through a compiled PreviewProgram (Fase 6 /
/// RF-010). Everything runs on the GUI thread and talks to the engine only via
/// ControlProxy (thread-safe, non-blocking) — no I/O, no locks, nothing heavy
/// on the audio path. The timeline is advanced from the source deck's
/// playback position (spec 21.3: musical events follow playback state, not a
/// wall clock); a QTimer only samples that position.
class PreviewExecutor : public QObject {
    Q_OBJECT
  public:
    enum class State {
        Idle,
        Loading,
        Transitioning,
        Completed,
        Cancelled,
        ManualOverride,
        Failed,
    };
    Q_ENUM(State)

    explicit PreviewExecutor(std::shared_ptr<PlayerManager> pPlayerManager,
            int sourceDeckIndex = 0,
            int targetDeckIndex = 1,
            QObject* parent = nullptr);
    ~PreviewExecutor() override;

    State state() const {
        return m_state;
    }

    bool controlsAvailable() const;

    /// Loads the pair into the two decks and, once both are ready, cues them and
    /// runs the transition. Track durations (ms) are used to map the source
    /// playback position to elapsed beats.
    void preview(const PreviewProgram& program,
            TrackPointer pSource,
            TrackPointer pTarget,
            double sourceDurationMs,
            double targetDurationMs);

    /// Re-cues the loaded pair and runs the transition again.
    void repeat();

    /// Stops the automation and hands the decks back to the user without cutting
    /// the audio.
    void cancel();

  signals:
    void stateChanged(int state, const QString& message);

  private slots:
    void pollLoaded();
    void onTick();
    void onCrossfaderChanged(double value);

  private:
    struct DeckControls {
        ControlProxy* play = nullptr;
        ControlProxy* playPosition = nullptr;
        ControlProxy* trackSamples = nullptr;
        ControlProxy* bpm = nullptr;
        ControlProxy* syncEnabled = nullptr;
        ControlProxy* volume = nullptr;
        ControlProxy* orientation = nullptr;
        ControlProxy* eqLow = nullptr;
        ControlProxy* filter = nullptr;
    };

    void setState(State state, const QString& message = QString());
    void cueDecks();
    void begin();
    void finish();
    void handControlsToUser();
    void applyWrite(const ControlWrite& write);
    ControlProxy* resolve(const QString& control);

    std::shared_ptr<PlayerManager> m_pPlayerManager;
    int m_sourceDeckIndex;
    int m_targetDeckIndex;
    QString m_sourceGroup;
    QString m_targetGroup;

    DeckControls m_source;
    DeckControls m_target;
    ControlProxy* m_pCrossfader = nullptr;

    QTimer* m_pTimer = nullptr;
    QTimer* m_pLoadTimer = nullptr;

    State m_state = State::Idle;
    PreviewProgram m_program;
    double m_sourceDurationMs = 0.0;
    double m_targetDurationMs = 0.0;

    double m_startPos01 = 0.0;
    double m_refBpm = 0.0;
    int m_nextWrite = 0;
    int m_loadPolls = 0;

    double m_lastCrossfaderSet = 0.0;
};

} // namespace mixxx::music_sync
