#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

#include "analyzer/trackanalysisscheduler.h"
#include "music_sync/domain/arrangement.h"
#include "music_sync/domain/mix_intent.h"
#include "music_sync/analysis/override_repository.h"
#include "music_sync/domain/track_features.h"

namespace mixxx {
class CoreServices;
}

namespace mixxx::music_sync {

class SidecarDatabase;
class PreviewExecutor;
class SetExecutor;

/// Entry point / lifecycle owner of the Music Sync module. It opens the sidecar
/// database, runs migrations, reads native track analysis from the Mixxx
/// library and snapshots it. Everything runs on the GUI thread and off the
/// audio thread; a failure only disables the module.
class MusicSyncController : public QObject {
    Q_OBJECT
  public:
    explicit MusicSyncController(
            std::shared_ptr<mixxx::CoreServices> pCoreServices, QObject* parent = nullptr);
    ~MusicSyncController() override;

    /// Opens the sidecar database and applies migrations. Returns true when the
    /// module is ready.
    bool initialize();

    bool isReady() const {
        return m_ready;
    }

    QString sidecarPath() const;
    int schemaVersion() const;

    bool isModuleEnabled() const;
    bool setModuleEnabled(bool enabled);

    // --- Fase 2: native analysis snapshot ---

    /// Reads native analysis data (BPM, key/Camelot, beatgrid, ReplayGain,
    /// intro/outro, stream info) for up to `limit` tracks from the Mixxx
    /// library, stores the snapshots in the sidecar and returns them.
    QVector<TrackFeatures> snapshotLibrary(int limit);

    /// Loads previously stored snapshots from the sidecar.
    QVector<TrackFeatures> loadSnapshots() const;

    /// Number of snapshots currently stored.
    int snapshotCount() const;

    /// Removes every stored snapshot (the sidecar's analysis cache). Everything
    /// it holds is recomputable from the Mixxx library. Returns how many were
    /// removed, or -1 on failure.
    int clearSnapshots();

    /// Generates ranked candidate sequences (>=3 when possible) from the
    /// analyzed snapshots, using the given intent. Empty when fewer than two
    /// analyzed tracks exist.
    QVector<Arrangement> generateSequences(const MixIntent& intent);

    /// Triggers Mixxx's native analysis for tracks (scanning up to `limit`) that
    /// still need it (no beatgrid/bpm or no key). Returns the number scheduled.
    /// Emits analysisProgress()/analysisFinished() and re-snapshots on finish.
    int analyzeMissing(int limit);

    /// Whether a native analysis run is currently in progress.
    bool isAnalyzing() const {
        return static_cast<bool>(m_pScheduler);
    }

    // --- Fase 6: two-deck transition preview ---

    /// Plans and previews the transition from `from` to `to` on the real decks
    /// (deck 1 = source, deck 2 = target). Returns false if the tracks or deck
    /// controls are unavailable. Progress is reported via previewStateChanged().
    bool previewTransition(
            const TrackFeatures& from, const TrackFeatures& to, const MixIntent& intent);

    /// Runs the last previewed transition again.
    void repeatPreview();

    /// Stops the preview automation and hands the decks back to the user.
    void cancelPreview();

    // --- Fase 7: mini-set executor ---

    /// Compiles `arrangement` into a set program and runs it on the decks.
    /// `fromAct`/`toAct` restrict it to a stretch of the narrative (0 = the whole
    /// set), which is how spec 19's Ensaio 3 rehearses one act, or one act
    /// handing over to the next, without sitting through everything before it.
    /// Returns false when the tracks or deck controls are unavailable.
    bool runSet(const Arrangement& arrangement,
            const MixIntent& intent,
            int fromAct = 0,
            int toAct = 0);

    // --- RF-010: per-pair transition editing ---

    /// The DJ's stored choices, keyed by track pair.
    QHash<PairKey, TransitionOverride> loadOverrides() const;

    /// Act-wide defaults; a pair's own choice beats them.
    QHash<int, TransitionOverride> loadActRules() const;
    bool setActRule(int act, const TransitionOverride& rule);

    /// What actually applies to a pair, pair over act — the same resolution the
    /// preview and the set use, so the panel never shows a different answer.
    TransitionOverride resolvedOverride(
            std::int64_t sourceTrackId, std::int64_t targetTrackId, int act) const;

    /// Pins the type and/or length for one pair. An empty override means "back
    /// to automatic" and removes the stored choice.
    bool setOverride(std::int64_t sourceTrackId,
            std::int64_t targetTrackId,
            const TransitionOverride& override);

    void pauseSet();
    void resumeSet();
    void skipSetTrack();
    void cancelSet();

  signals:
    void analysisProgress(int currentTrackNumber, int totalTracks);
    void analysisFinished();
    void previewStateChanged(int state, const QString& message);
    void setStateChanged(int state, const QString& message);
    void setPositionChanged(int position, const QString& what);

  private:
    std::shared_ptr<mixxx::CoreServices> m_pCoreServices;
    std::unique_ptr<SidecarDatabase> m_pDatabase;
    bool m_ready;
    TrackAnalysisScheduler::Pointer m_pScheduler;
    std::unique_ptr<PreviewExecutor> m_pPreviewExecutor;
    std::unique_ptr<SetExecutor> m_pSetExecutor;
};

} // namespace mixxx::music_sync
