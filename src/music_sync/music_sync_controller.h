#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include "preferences/usersettings.h"

namespace mixxx::music_sync {

class SidecarDatabase;

/// Entry point / lifecycle owner of the Music Sync module. It opens the sidecar
/// database, runs migrations and exposes simple module settings. Everything it
/// does is lazy and off the audio thread; a failure only disables the module
/// and never affects Mixxx playback.
class MusicSyncController : public QObject {
    Q_OBJECT
  public:
    explicit MusicSyncController(UserSettingsPointer pConfig, QObject* parent = nullptr);
    ~MusicSyncController() override;

    /// Opens the sidecar database and applies migrations. Returns true when the
    /// module is ready. Safe to call once; subsequent calls return the cached
    /// readiness.
    bool initialize();

    bool isReady() const {
        return m_ready;
    }

    /// Absolute path of the sidecar database file (valid after initialize()).
    QString sidecarPath() const;

    /// Current sidecar schema version (0 when unavailable).
    int schemaVersion() const;

    /// Persisted module on/off preference, stored in the sidecar. This is just a
    /// stored flag for now — it does not yet change any behaviour.
    bool isModuleEnabled() const;
    bool setModuleEnabled(bool enabled);

  private:
    UserSettingsPointer m_pConfig;
    std::unique_ptr<SidecarDatabase> m_pDatabase;
    bool m_ready;
};

} // namespace mixxx::music_sync
