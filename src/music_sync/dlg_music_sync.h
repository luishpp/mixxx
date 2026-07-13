#pragma once

#include <QDialog>
#include <QVector>
#include <memory>

#include "music_sync/domain/track_features.h"

namespace mixxx {
class CoreServices;
}

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace mixxx::music_sync {

class MusicSyncController;

/// Music Sync panel. It initializes the module and shows a table of native
/// analysis snapshots (BPM, Camelot, key, duration, ReplayGain, analyzed
/// status) read from the Mixxx library. Non-modal; never touches the audio
/// engine.
class DlgMusicSync : public QDialog {
    Q_OBJECT
  public:
    DlgMusicSync(QWidget* pParent, std::shared_ptr<mixxx::CoreServices> pCoreServices);
    ~DlgMusicSync() override = default;

  private slots:
    void slotModuleEnabledToggled(bool checked);
    void slotSnapshotLibrary();
    void slotReloadSnapshots();
    void slotAnalyzeMissing();
    void slotAnalysisProgress(int currentTrackNumber, int totalTracks);
    void slotAnalysisFinished();

  private:
    void populateTable(const QVector<TrackFeatures>& rows);
    void setBusy(bool busy);

    MusicSyncController* m_pController;
    QLabel* m_pStatusLabel;
    QCheckBox* m_pEnabledCheckBox;
    QPushButton* m_pSnapshotButton;
    QPushButton* m_pReloadButton;
    QPushButton* m_pAnalyzeButton;
    QTableWidget* m_pTable;
    QLabel* m_pSummaryLabel;
};

} // namespace mixxx::music_sync
