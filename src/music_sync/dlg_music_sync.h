#pragma once

#include <QDialog>

#include "preferences/usersettings.h"

class QCheckBox;
class QLabel;

namespace mixxx::music_sync {

class MusicSyncController;

/// Minimal Music Sync panel (Fase 1). It initializes the module, shows the
/// sidecar status and lets the user toggle a persisted module setting. It is
/// non-modal and does not touch the audio engine.
class DlgMusicSync : public QDialog {
    Q_OBJECT
  public:
    DlgMusicSync(QWidget* pParent, UserSettingsPointer pConfig);
    ~DlgMusicSync() override = default;

  private slots:
    void slotModuleEnabledToggled(bool checked);

  private:
    // Owned via Qt parent/child (parented to this dialog).
    MusicSyncController* m_pController;
    QLabel* m_pStatusLabel;
    QCheckBox* m_pEnabledCheckBox;
};

} // namespace mixxx::music_sync
