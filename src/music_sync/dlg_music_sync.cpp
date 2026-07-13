#include "music_sync/dlg_music_sync.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <cmath>

#include "coreservices.h"
#include "music_sync/domain/arrangement.h"
#include "music_sync/domain/mix_intent.h"
#include "music_sync/music_sync_controller.h"
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
} // anonymous namespace

namespace mixxx::music_sync {

DlgMusicSync::DlgMusicSync(QWidget* pParent, std::shared_ptr<mixxx::CoreServices> pCoreServices)
        : QDialog(pParent),
          m_pController(new MusicSyncController(std::move(pCoreServices), this)),
          m_pStatusLabel(nullptr),
          m_pEnabledCheckBox(nullptr),
          m_pSnapshotButton(nullptr),
          m_pReloadButton(nullptr),
          m_pAnalyzeButton(nullptr),
          m_pEnergyPreset(nullptr),
          m_pGenerateButton(nullptr),
          m_pTable(nullptr),
          m_pSummaryLabel(nullptr) {
    setWindowTitle(tr("Music Sync DJ"));
    resize(760, 480);

    const bool ready = m_pController->initialize();

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
    m_pEnabledCheckBox->setEnabled(ready);
    m_pEnabledCheckBox->setChecked(ready && m_pController->isModuleEnabled());
    connect(m_pEnabledCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgMusicSync::slotModuleEnabledToggled);
    pLayout->addWidget(m_pEnabledCheckBox);

    // Action row.
    auto* pActions = new QHBoxLayout();
    m_pSnapshotButton = new QPushButton(tr("Read native analysis from library"), this);
    m_pSnapshotButton->setEnabled(ready);
    connect(m_pSnapshotButton, &QPushButton::clicked, this, &DlgMusicSync::slotSnapshotLibrary);
    pActions->addWidget(m_pSnapshotButton);

    m_pReloadButton = new QPushButton(tr("Reload snapshots"), this);
    m_pReloadButton->setEnabled(ready);
    connect(m_pReloadButton, &QPushButton::clicked, this, &DlgMusicSync::slotReloadSnapshots);
    pActions->addWidget(m_pReloadButton);

    m_pAnalyzeButton = new QPushButton(tr("Analyze missing (Mixxx)"), this);
    m_pAnalyzeButton->setEnabled(ready);
    connect(m_pAnalyzeButton, &QPushButton::clicked, this, &DlgMusicSync::slotAnalyzeMissing);
    pActions->addWidget(m_pAnalyzeButton);

    m_pEnergyPreset = new QComboBox(this);
    m_pEnergyPreset->addItem(tr("Ascending"), static_cast<int>(EnergyPreset::Ascending));
    m_pEnergyPreset->addItem(tr("Center peak"), static_cast<int>(EnergyPreset::CenterPeak));
    m_pEnergyPreset->addItem(tr("Late peak"), static_cast<int>(EnergyPreset::LatePeak));
    m_pEnergyPreset->addItem(tr("Waves"), static_cast<int>(EnergyPreset::Waves));
    m_pEnergyPreset->addItem(tr("Constant"), static_cast<int>(EnergyPreset::Constant));
    m_pEnergyPreset->setCurrentIndex(2); // Late peak
    m_pEnergyPreset->setEnabled(ready);
    pActions->addWidget(m_pEnergyPreset);

    m_pGenerateButton = new QPushButton(tr("Generate sequence"), this);
    m_pGenerateButton->setEnabled(ready);
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

    // Show any snapshots stored in a previous session.
    if (ready) {
        populateTable(m_pController->loadSnapshots());
    }
}

void DlgMusicSync::slotModuleEnabledToggled(bool checked) {
    if (!m_pController->setModuleEnabled(checked)) {
        kLogger.warning() << "Could not persist module_enabled setting";
    }
}

void DlgMusicSync::slotSnapshotLibrary() {
    m_pSnapshotButton->setEnabled(false);
    const QVector<TrackFeatures> rows = m_pController->snapshotLibrary(kSnapshotLimit);
    populateTable(rows);
    m_pSnapshotButton->setEnabled(true);
}

void DlgMusicSync::slotReloadSnapshots() {
    populateTable(m_pController->loadSnapshots());
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
        }
    }
    if (!best.warnings.isEmpty()) {
        report += QStringLiteral("\n") + tr("Warnings: ") +
                best.warnings.join(QStringLiteral("; "));
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Generated sequence"));
    dialog.resize(760, 560);
    auto* layout = new QVBoxLayout(&dialog);
    auto* view = new QPlainTextEdit(&dialog);
    view->setReadOnly(true);
    view->setPlainText(report);
    layout->addWidget(view);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog.exec();

    m_pSummaryLabel->setText(
            tr("Generated %1 sequence alternative(s).").arg(arrangements.size()));
}

void DlgMusicSync::setBusy(bool busy) {
    m_pSnapshotButton->setEnabled(!busy);
    m_pReloadButton->setEnabled(!busy);
    m_pAnalyzeButton->setEnabled(!busy);
    m_pGenerateButton->setEnabled(!busy);
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
