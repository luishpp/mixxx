#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

#include "music_sync/domain/track_features.h"

namespace mixxx {
class CoreServices;
}

namespace mixxx::music_sync {

class SidecarDatabase;

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

  private:
    std::shared_ptr<mixxx::CoreServices> m_pCoreServices;
    std::unique_ptr<SidecarDatabase> m_pDatabase;
    bool m_ready;
};

} // namespace mixxx::music_sync
