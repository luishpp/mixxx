#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

#include "analyzer/trackanalysisscheduler.h"
#include "music_sync/domain/arrangement.h"
#include "music_sync/domain/mix_intent.h"
#include "music_sync/domain/track_features.h"

namespace mixxx {
class CoreServices;
}

namespace mixxx::music_sync {

class SidecarDatabase;
class PreviewExecutor;

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

  signals:
    void analysisProgress(int currentTrackNumber, int totalTracks);
    void analysisFinished();
    void previewStateChanged(int state, const QString& message);

  private:
    std::shared_ptr<mixxx::CoreServices> m_pCoreServices;
    std::unique_ptr<SidecarDatabase> m_pDatabase;
    bool m_ready;
    TrackAnalysisScheduler::Pointer m_pScheduler;
    std::unique_ptr<PreviewExecutor> m_pPreviewExecutor;
};

} // namespace mixxx::music_sync
