#include "music_sync/dlg_music_sync.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "coreservices.h"
#include "music_sync/domain/arrangement.h"
#include "music_sync/domain/mix_intent.h"
#include "music_sync/domain/transition_plan.h"
#include "music_sync/music_sync_controller.h"
#include "music_sync/planner/transition_planner.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync");

// How many library tracks to snapshot per click (spec exit criterion: ~20+).
constexpr int kSnapshotLimit = 500;

QString msToClock(std::int64_t ms) {
    if (ms <= 0) {
        return QStringLiteral("—");
    }
    const std::int64_t totalSeconds = ms / 1000;
    return QStringLiteral("%1:%2")
            .arg(totalSeconds / 60)
            .arg(totalSeconds % 60, 2, 10, QChar('0'));
}

QString replayGainText(double ratio) {
    if (ratio <= 0.0) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1 dB").arg(20.0 * std::log10(ratio), 0, 'f', 1);
}

QString dashIfEmpty(const QString& text) {
    return text.isEmpty() ? QStringLiteral("—") : text;
}

// Mirrors mixxx::music_sync::PreviewExecutor::State (kept as int to avoid
// coupling the panel to the engine-facing executor header).
QString previewStateText(int state) {
    switch (state) {
    case 1:
        return QObject::tr("Loading");
    case 2:
        return QObject::tr("Transitioning");
    case 3:
        return QObject::tr("Completed");
    case 4:
        return QObject::tr("Cancelled");
    case 5:
        return QObject::tr("Manual override");
    case 6:
        return QObject::tr("Failed");
    default:
        return QObject::tr("Idle");
    }
}
} // anonymous namespace

namespace mixxx::music_sync {

DlgMusicSync::DlgMusicSync(QWidget* pParent, std::shared_ptr<mixxx::CoreServices> pCoreServices)
        : QDialog(pParent),
          m_pController(new MusicSyncController(std::move(pCoreServices), this)),
          m_ready(false),
          m_busy(false),
          m_pStatusLabel(nullptr),
          m_pEnabledCheckBox(nullptr),
          m_pSnapshotButton(nullptr),
          m_pReloadButton(nullptr),
          m_pClearButton(nullptr),
          m_pAnalyzeButton(nullptr),
          m_pEnergyPreset(nullptr),
          m_pGenerateButton(nullptr),
          m_pTable(nullptr),
          m_pSummaryLabel(nullptr) {
    setWindowTitle(tr("Music Sync DJ"));
    resize(760, 480);

    const bool ready = m_pController->initialize();
    m_ready = ready;

    auto* pLayout = new QVBoxLayout(this);

    auto* pTitle = new QLabel(tr("Music Sync DJ"), this);
    QFont titleFont = pTitle->font();
    titleFont.setBold(true);
    pTitle->setFont(titleFont);
    pLayout->addWidget(pTitle);

    m_pStatusLabel = new QLabel(this);
    m_pStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pStatusLabel->setWordWrap(true);
    if (ready) {
        m_pStatusLabel->setText(tr("Sidecar database: %1 (schema v%2)")
                                        .arg(m_pController->sidecarPath())
                                        .arg(m_pController->schemaVersion()));
    } else {
        m_pStatusLabel->setText(
                tr("Sidecar database unavailable — Music Sync is disabled. "
                   "Mixxx playback is unaffected."));
    }
    pLayout->addWidget(m_pStatusLabel);

    m_pEnabledCheckBox = new QCheckBox(
            tr("Enable Music Sync (stored in the sidecar)"), this);
    // setChecked() before connect(): the slot touches widgets built further
    // down, so it must not fire while the panel is still being assembled.
    m_pEnabledCheckBox->setChecked(ready && m_pController->isModuleEnabled());
    connect(m_pEnabledCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgMusicSync::slotModuleEnabledToggled);
    pLayout->addWidget(m_pEnabledCheckBox);

    // Action row.
    auto* pActions = new QHBoxLayout();
    // Enabled state for every action comes from updateActionsEnabled() below.
    m_pSnapshotButton = new QPushButton(tr("Read native analysis from library"), this);
    connect(m_pSnapshotButton, &QPushButton::clicked, this, &DlgMusicSync::slotSnapshotLibrary);
    pActions->addWidget(m_pSnapshotButton);

    m_pReloadButton = new QPushButton(tr("Reload snapshots"), this);
    connect(m_pReloadButton, &QPushButton::clicked, this, &DlgMusicSync::slotReloadSnapshots);
    pActions->addWidget(m_pReloadButton);

    m_pClearButton = new QPushButton(tr("Clear snapshots"), this);
    m_pClearButton->setToolTip(
            tr("Removes every stored snapshot. Nothing is lost permanently — they "
               "are recomputed from the Mixxx library."));
    connect(m_pClearButton, &QPushButton::clicked, this, &DlgMusicSync::slotClearSnapshots);
    pActions->addWidget(m_pClearButton);

    m_pAnalyzeButton = new QPushButton(tr("Analyze missing (Mixxx)"), this);
    connect(m_pAnalyzeButton, &QPushButton::clicked, this, &DlgMusicSync::slotAnalyzeMissing);
    pActions->addWidget(m_pAnalyzeButton);

    m_pEnergyPreset = new QComboBox(this);
    m_pEnergyPreset->addItem(tr("Ascending"), static_cast<int>(EnergyPreset::Ascending));
    m_pEnergyPreset->addItem(tr("Center peak"), static_cast<int>(EnergyPreset::CenterPeak));
    m_pEnergyPreset->addItem(tr("Late peak"), static_cast<int>(EnergyPreset::LatePeak));
    m_pEnergyPreset->addItem(tr("Waves"), static_cast<int>(EnergyPreset::Waves));
    m_pEnergyPreset->addItem(tr("Constant"), static_cast<int>(EnergyPreset::Constant));
    m_pEnergyPreset->setCurrentIndex(2); // Late peak
    pActions->addWidget(m_pEnergyPreset);

    m_pGenerateButton = new QPushButton(tr("Generate sequence"), this);
    connect(m_pGenerateButton, &QPushButton::clicked, this, &DlgMusicSync::slotGenerateSequence);
    pActions->addWidget(m_pGenerateButton);

    connect(m_pController,
            &MusicSyncController::analysisProgress,
            this,
            &DlgMusicSync::slotAnalysisProgress);
    connect(m_pController,
            &MusicSyncController::analysisFinished,
            this,
            &DlgMusicSync::slotAnalysisFinished);

    m_pSummaryLabel = new QLabel(this);
    pActions->addWidget(m_pSummaryLabel);
    pActions->addStretch(1);
    pLayout->addLayout(pActions);

    // Track table.
    m_pTable = new QTableWidget(this);
    m_pTable->setColumnCount(12);
    m_pTable->setHorizontalHeaderLabels(QStringList()
            << tr("Artist") << tr("Title") << tr("BPM") << tr("Camelot")
            << tr("Key") << tr("Duration") << tr("ReplayGain") << tr("Analyzed")
            << tr("Energy") << tr("Phrases") << tr("Sections") << tr("Exit @"));
    m_pTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pTable->verticalHeader()->setVisible(false);
    m_pTable->horizontalHeader()->setStretchLastSection(true);
    pLayout->addWidget(m_pTable, 1);

    auto* pButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::close);
    pLayout->addWidget(pButtons);

    updateActionsEnabled();
    if (ready && !m_pEnabledCheckBox->isChecked()) {
        m_pSummaryLabel->setText(tr("Music Sync is off — enable it to use the panel."));
    }

    // Show any snapshots stored in a previous session.
    if (ready) {
        populateTable(m_pController->loadSnapshots());
    }
}

void DlgMusicSync::updateActionsEnabled() {
    if (!m_pSnapshotButton) {
        return; // the panel is still being assembled
    }
    const bool enabled = m_ready && m_pEnabledCheckBox->isChecked() && !m_busy;
    m_pSnapshotButton->setEnabled(enabled);
    m_pReloadButton->setEnabled(enabled);
    m_pClearButton->setEnabled(enabled);
    m_pAnalyzeButton->setEnabled(enabled);
    m_pEnergyPreset->setEnabled(enabled);
    m_pGenerateButton->setEnabled(enabled);
    // The switch itself stays usable unless the sidecar failed or work is running.
    m_pEnabledCheckBox->setEnabled(m_ready && !m_busy);
}

void DlgMusicSync::slotModuleEnabledToggled(bool checked) {
    if (!m_pController->setModuleEnabled(checked)) {
        kLogger.warning() << "Could not persist module_enabled setting";
    }
    updateActionsEnabled();
    if (m_pSummaryLabel) {
        m_pSummaryLabel->setText(
                checked ? QString() : tr("Music Sync is off — enable it to use the panel."));
    }
}

void DlgMusicSync::slotSnapshotLibrary() {
    setBusy(true);
    const QVector<TrackFeatures> rows = m_pController->snapshotLibrary(kSnapshotLimit);
    populateTable(rows);
    setBusy(false);
}

void DlgMusicSync::slotReloadSnapshots() {
    populateTable(m_pController->loadSnapshots());
}

void DlgMusicSync::slotClearSnapshots() {
    const int stored = m_pController->snapshotCount();
    if (stored <= 0) {
        m_pSummaryLabel->setText(tr("No snapshots stored."));
        return;
    }
    if (QMessageBox::question(this,
                tr("Clear snapshots"),
                tr("Remove all %1 stored snapshot(s)?\n\nNothing is lost permanently: "
                   "they are recomputed from the Mixxx library with "
                   "\"Read native analysis from library\".")
                        .arg(stored),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    const int removed = m_pController->clearSnapshots();
    if (removed < 0) {
        m_pSummaryLabel->setText(tr("Could not clear the snapshots."));
        return;
    }
    populateTable(m_pController->loadSnapshots());
    m_pSummaryLabel->setText(tr("Cleared %1 snapshot(s).").arg(removed));
}

void DlgMusicSync::slotAnalyzeMissing() {
    setBusy(true);
    const int scheduled = m_pController->analyzeMissing(kSnapshotLimit);
    if (scheduled <= 0) {
        setBusy(false);
        m_pSummaryLabel->setText(tr("Nothing to analyze — all scanned tracks are analyzed."));
    } else {
        m_pSummaryLabel->setText(tr("Analyzing %1 track(s)…").arg(scheduled));
    }
}

void DlgMusicSync::slotAnalysisProgress(int currentTrackNumber, int totalTracks) {
    m_pSummaryLabel->setText(
            tr("Analyzing %1/%2…").arg(currentTrackNumber).arg(totalTracks));
}

void DlgMusicSync::slotAnalysisFinished() {
    setBusy(false);
    populateTable(m_pController->loadSnapshots());
}

void DlgMusicSync::slotGenerateSequence() {
    MixIntent intent;
    intent.energyPreset = static_cast<EnergyPreset>(m_pEnergyPreset->currentData().toInt());
    intent.maxTempoChangePercent = tempo_tolerance::kBalanced;

    const QVector<Arrangement> arrangements = m_pController->generateSequences(intent);
    if (arrangements.isEmpty()) {
        m_pSummaryLabel->setText(
                tr("Need at least 2 analyzed tracks to generate a sequence."));
        return;
    }

    QHash<qint64, TrackFeatures> byId;
    for (const TrackFeatures& features : m_pController->loadSnapshots()) {
        byId.insert(features.mixxxTrackId, features);
    }

    const Arrangement& best = arrangements.first();
    QString report =
            tr("Best of %1 alternative(s) — average compatibility %2%, energy fit %3%\n\n")
                    .arg(arrangements.size())
                    .arg(qRound(best.totalScore * 100.0))
                    .arg(qRound(best.energyFitScore * 100.0));
    TrackFeatures prevFeatures;
    bool havePrev = false;
    for (const ArrangementItem& item : best.items) {
        const TrackFeatures features = byId.value(item.mixxxTrackId);
        report += QStringLiteral("%1. %2 - %3  (%4 BPM, %5)%6\n")
                          .arg(item.position + 1, 2)
                          .arg(features.artist.isEmpty() ? QStringLiteral("?") : features.artist,
                                  features.title.isEmpty() ? QStringLiteral("?") : features.title,
                                  features.bpm > 0.0 ? QString::number(features.bpm, 'f', 1)
                                                     : QStringLiteral("—"),
                                  features.camelot.isEmpty() ? QStringLiteral("—")
                                                             : features.camelot,
                                  item.locked ? tr("  [locked]") : QString());
        if (item.position > 0) {
            report += QStringLiteral("      [%1%] %2\n")
                              .arg(qRound(item.pairScoreFromPrevious * 100.0))
                              .arg(item.explanationFromPrevious);
            if (havePrev) {
                const TransitionPlan plan =
                        TransitionPlanner::plan(prevFeatures, features, intent);
                report += QStringLiteral("      ↳ %1 — %2 bars, %3% conf\n")
                                  .arg(transitionTypeName(plan.type))
                                  .arg(plan.durationBars)
                                  .arg(qRound(plan.confidence * 100.0));
            }
        }
        prevFeatures = features;
        havePrev = true;
    }
    if (!best.warnings.isEmpty()) {
        report += QStringLiteral("\n") + tr("Warnings: ") +
                best.warnings.join(QStringLiteral("; "));
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Generated sequence"));
    dialog.resize(760, 620);
    auto* layout = new QVBoxLayout(&dialog);
    auto* view = new QPlainTextEdit(&dialog);
    view->setReadOnly(true);
    view->setPlainText(report);
    layout->addWidget(view);

    // --- Fase 6: two-deck preview of a chosen consecutive pair ---
    auto* previewRow = new QHBoxLayout();
    auto* pairSelector = new QComboBox(&dialog);
    {
        struct PairEntry {
            QString sortKey; // titles only: drives the alphabetical order
            QString label;   // shown: set position + titles
            int index;       // position in the arrangement (the item data)
        };
        QVector<PairEntry> entries;
        for (int i = 0; i + 1 < best.items.size(); ++i) {
            const TrackFeatures a = byId.value(best.items.at(i).mixxxTrackId);
            const TrackFeatures b = byId.value(best.items.at(i + 1).mixxxTrackId);
            const QString text = QStringLiteral("%1 → %2").arg(
                    a.title.isEmpty() ? dashIfEmpty(a.artist) : a.title,
                    b.title.isEmpty() ? dashIfEmpty(b.artist) : b.title);
            // Sorting by title detaches the list from the numbered report above,
            // so keep the set position in the label.
            entries.append({text,
                    QStringLiteral("%1. %2").arg(i + 1, 2, 10, QChar('0')).arg(text),
                    i});
        }
        std::sort(entries.begin(),
                entries.end(),
                [](const PairEntry& x, const PairEntry& y) {
                    const int cmp = QString::compare(x.sortKey, y.sortKey, Qt::CaseInsensitive);
                    return cmp != 0 ? cmp < 0 : x.index < y.index;
                });
        for (const PairEntry& entry : entries) {
            pairSelector->addItem(entry.label, entry.index);
        }
    }
    auto* previewButton = new QPushButton(tr("Preview on decks"), &dialog);
    auto* repeatButton = new QPushButton(tr("Repeat"), &dialog);
    auto* stopButton = new QPushButton(tr("Cancel preview"), &dialog);
    previewRow->addWidget(pairSelector, 1);
    previewRow->addWidget(previewButton);
    previewRow->addWidget(repeatButton);
    previewRow->addWidget(stopButton);
    layout->addLayout(previewRow);
    auto* previewStatus = new QLabel(
            tr("Loads deck 1 = A and deck 2 = B, beat-matches and runs the transition."),
            &dialog);
    previewStatus->setWordWrap(true);
    layout->addWidget(previewStatus);

    const bool canPreview = pairSelector->count() > 0;
    previewButton->setEnabled(canPreview);
    repeatButton->setEnabled(canPreview);
    stopButton->setEnabled(canPreview);

    connect(previewButton,
            &QPushButton::clicked,
            &dialog,
            [this, pairSelector, &best, &byId, intent, previewStatus]() {
                const int i = pairSelector->currentData().toInt();
                if (i < 0 || i + 1 >= best.items.size()) {
                    return;
                }
                const TrackFeatures from = byId.value(best.items.at(i).mixxxTrackId);
                const TrackFeatures to = byId.value(best.items.at(i + 1).mixxxTrackId);
                if (!m_pController->previewTransition(from, to, intent)) {
                    previewStatus->setText(tr(
                            "Preview unavailable — need at least two decks and both "
                            "tracks in the library."));
                }
            });
    connect(repeatButton, &QPushButton::clicked, &dialog, [this]() {
        m_pController->repeatPreview();
    });
    connect(stopButton, &QPushButton::clicked, &dialog, [this]() {
        m_pController->cancelPreview();
    });
    connect(m_pController,
            &MusicSyncController::previewStateChanged,
            &dialog,
            [previewStatus](int state, const QString& message) {
                previewStatus->setText(
                        previewStateText(state) + QStringLiteral(" — ") + message);
            });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog.exec();
    // Stop any running automation if the user closes the dialog mid-preview.
    m_pController->cancelPreview();

    m_pSummaryLabel->setText(
            tr("Generated %1 sequence alternative(s).").arg(arrangements.size()));
}

void DlgMusicSync::setBusy(bool busy) {
    m_busy = busy;
    updateActionsEnabled();
}

void DlgMusicSync::populateTable(const QVector<TrackFeatures>& rows) {
    m_pTable->setRowCount(rows.size());
    int analyzedCount = 0;
    for (int row = 0; row < rows.size(); ++row) {
        const TrackFeatures& f = rows.at(row);
        if (f.analyzed) {
            ++analyzedCount;
        }
        int column = 0;
        const auto setCell = [&](const QString& text) {
            m_pTable->setItem(row, column++, new QTableWidgetItem(text));
        };
        setCell(dashIfEmpty(f.artist));
        setCell(dashIfEmpty(f.title));
        setCell(f.bpm > 0.0 ? QString::number(f.bpm, 'f', 1) : QStringLiteral("—"));
        setCell(dashIfEmpty(f.camelot));
        setCell(dashIfEmpty(f.keyText));
        setCell(msToClock(f.durationMs));
        setCell(replayGainText(f.replaygainRatio));
        setCell(f.analyzed ? tr("Yes") : tr("No"));
        setCell(f.energyCurve.isEmpty()
                        ? QStringLiteral("—")
                        : QString::number(f.overallEnergy, 'f', 2));
        setCell(f.phrases.isEmpty() ? QStringLiteral("—")
                                    : QString::number(f.phrases.size()));
        setCell(f.sections.isEmpty() ? QStringLiteral("—")
                                     : QString::number(f.sections.size()));
        setCell(f.exitWindows.isEmpty()
                        ? QStringLiteral("—")
                        : msToClock(f.exitWindows.first().startMs));
    }
    m_pTable->resizeColumnsToContents();
    m_pSummaryLabel->setText(
            tr("%1 track(s), %2 analyzed").arg(rows.size()).arg(analyzedCount));
}

} // namespace mixxx::music_sync

#include "moc_dlg_music_sync.cpp"
