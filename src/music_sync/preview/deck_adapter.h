#pragma once

#include <QObject>
#include <QString>

class ControlProxy;

namespace mixxx::music_sync {

/// All the Mixxx controls of one deck, in one place (spec 8.4). The set
/// executor rotates decks — the one that just came in becomes the one going out
/// — so the controls cannot be hard-wired to a fixed "source" and "target" the
/// way a single-transition preview can.
///
/// GUI thread only: ControlProxy must be created and used on one thread, and
/// every write here is thread-safe and non-blocking, so nothing touches the
/// audio path (RNF-002).
class DeckAdapter : public QObject {
    Q_OBJECT
  public:
    /// `deckIndex` is 0-based; deck 0 is Mixxx's [Channel1].
    explicit DeckAdapter(int deckIndex, QObject* parent = nullptr);

    int index() const {
        return m_deckIndex;
    }
    QString group() const {
        return m_group;
    }

    /// Whether the deck's controls resolved. False means Mixxx has fewer decks
    /// than the set needs.
    bool valid() const;

    /// A track is loaded and ready to seek. track_samples stays at the previous
    /// track's value until the new one is ready, so callers must compare against
    /// the track they asked for rather than trusting this alone.
    bool isLoaded() const;

    double position() const;   // playposition, 0..1
    void seek(double pos01);   // seeking IS writing playposition on this baseline
    double bpm() const;

    bool isPlaying() const;
    void setPlaying(bool playing);
    void setSync(bool enabled);
    /// Snap seeks and cues to the beatgrid. Without this a seek lands on an
    /// arbitrary millisecond, which is how a synced deck can still come in
    /// off-beat: sync matches tempo, it does not rescue a bad landing point.
    void setQuantize(bool enabled);
    /// One-shot phase alignment against the other playing deck. Tempo lock alone
    /// keeps the BPM equal; this is what puts the downbeats on top of each other.
    void syncPhase();
    /// One-shot tempo AND phase match against the other playing deck — exactly the
    /// deck's SYNC button. Enabling sync_enabled alone is not enough: without a
    /// sync leader the incoming deck can end up leading at its own native tempo,
    /// so the two BPMs stay a hair apart and the beats drift into a flam over a
    /// long blend ("samba"). This sets the rate to match, so they stay locked.
    void matchTempoAndPhase();
    void setVolume(double volume);   // 0..1, 1 = unity
    void setEqLow(double value);     // 0 kill .. 1 unity .. 4 boost
    void setFilter(double value);    // quick effect super knob, 0..1, 0.5 neutral
    void setOrientation(int orientation); // EngineChannel::LEFT/CENTER/RIGHT

    /// Hands the deck back to the user: nothing left killed, filtered, silenced
    /// or tempo-locked. Does not touch playback — the caller decides whether the
    /// audio keeps running (spec 21.4: never cut the sound abruptly).
    void handBackToUser();

  private:
    int m_deckIndex;
    QString m_group;
    ControlProxy* m_pPlay = nullptr;
    ControlProxy* m_pPlayPosition = nullptr;
    ControlProxy* m_pTrackSamples = nullptr;
    ControlProxy* m_pBpm = nullptr;
    ControlProxy* m_pSyncEnabled = nullptr;
    ControlProxy* m_pSyncPhase = nullptr;
    ControlProxy* m_pBeatSync = nullptr;
    ControlProxy* m_pQuantize = nullptr;
    ControlProxy* m_pVolume = nullptr;
    ControlProxy* m_pOrientation = nullptr;
    ControlProxy* m_pEqLow = nullptr;
    ControlProxy* m_pFilter = nullptr;
};

} // namespace mixxx::music_sync
