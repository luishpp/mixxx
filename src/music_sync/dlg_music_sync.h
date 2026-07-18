#pragma once

#include <QDialog>
#include <QPointer>
#include <QVector>
#include <memory>

#include "music_sync/domain/track_features.h"

namespace mixxx {
class CoreServices;
}

class QCheckBox;
class QComboBox;
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
    void slotClearSnapshots();
    void slotAnalyzeMissing();
    void slotAnalysisProgress(int currentTrackNumber, int totalTracks);
    void slotAnalysisFinished();
    void slotGenerateSequence();

  private:
    void populateTable(const QVector<TrackFeatures>& rows);
    void setBusy(bool busy);
    /// Actions are usable only when the sidecar opened, the module is enabled
    /// and no analysis run is in flight. Single place so the three conditions
    /// cannot disagree.
    void updateActionsEnabled();

    MusicSyncController* m_pController;
    bool m_ready;
    bool m_busy;
    QLabel* m_pStatusLabel;
    QCheckBox* m_pEnabledCheckBox;
    QPushButton* m_pSnapshotButton;
    QPushButton* m_pClearButton;
    QPushButton* m_pAnalyzeButton;
    QComboBox* m_pEnergyPreset;
    QPushButton* m_pGenerateButton;
    QTableWidget* m_pTable;
    QLabel* m_pSummaryLabel;
    /// The generated-sequence window is modeless (Mixxx stays usable while the
    /// set runs), so at most one is kept open at a time. QPointer self-nulls when
    /// the dialog is destroyed (WA_DeleteOnClose).
    QPointer<QDialog> m_pSequenceDialog;
};

} // namespace mixxx::music_sync
