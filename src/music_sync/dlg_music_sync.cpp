#include "music_sync/dlg_music_sync.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <cmath>
#include <optional>

#include "coreservices.h"
#include "music_sync/analysis/override_repository.h"
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

/// "3:04" -> 184000 ms. Empty or unparseable means "no choice", which is how the
/// field says "let the analysis decide" rather than "position zero".
std::optional<std::int64_t> clockToMs(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }
    const QStringList parts = trimmed.split(QChar(':'));
    bool okMinutes = false;
    bool okSeconds = false;
    if (parts.size() == 2) {
        const int minutes = parts.at(0).toInt(&okMinutes);
        const int seconds = parts.at(1).toInt(&okSeconds);
        if (okMinutes && okSeconds && minutes >= 0 && seconds >= 0 && seconds < 60) {
            return static_cast<std::int64_t>(minutes) * 60000 + seconds * 1000;
        }
    }
    return std::nullopt;
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

// Mirrors mixxx::music_sync::SetExecutor::State.
QString setStateText(int state) {
    switch (state) {
    case 1:
        return QObject::tr("Preparing");
    case 2:
        return QObject::tr("Playing");
    case 3:
        return QObject::tr("Transitioning");
    case 4:
        return QObject::tr("Preparing next");
    case 5:
        return QObject::tr("Paused");
    case 6:
        return QObject::tr("Manual override");
    case 7:
        return QObject::tr("Completed");
    case 8:
        return QObject::tr("Cancelled");
    case 9:
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
    const QString header =
            tr("Best of %1 alternative(s) — average compatibility %2%, energy fit %3%%4")
                    .arg(arrangements.size())
                    .arg(qRound(best.totalScore * 100.0))
                    .arg(qRound(best.energyFitScore * 100.0))
                    .arg(best.warnings.isEmpty()
                                    ? QString()
                                    : QStringLiteral("  —  ") +
                                            best.warnings.join(QStringLiteral("; ")));

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Generated sequence"));
    dialog.resize(1180, 700);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(header, &dialog));

    // One row per track: what plays, where it hands over, and how. Editing lives
    // in the row it belongs to instead of in a combo you have to hunt for.
    auto* grid = new QTableWidget(&dialog);
    grid->setColumnCount(8);
    grid->setHorizontalHeaderLabels(QStringList()
            << tr("#") << tr("Act") << tr("Track") << tr("Exit @")
            << tr("Transition") << tr("Bars") << tr("Match") << tr("Plan"));
    grid->setRowCount(best.items.size());
    grid->setSelectionBehavior(QAbstractItemView::SelectRows);
    grid->verticalHeader()->setVisible(false);
    grid->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(grid, 1);

    const auto rebuildGrid = [this, grid, &best, &byId, intent]() {
        const QHash<int, TransitionOverride> actRules = m_pController->loadActRules();
        const QHash<PairKey, TransitionOverride> pairs = m_pController->loadOverrides();
        for (int i = 0; i < best.items.size(); ++i) {
            const TrackFeatures a = byId.value(best.items.at(i).mixxxTrackId);
            const auto setCell = [&](int column, const QString& text) {
                auto* item = new QTableWidgetItem(text);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                grid->setItem(i, column, item);
            };
            setCell(0, QString::number(i + 1));
            setCell(1, a.act > 0 ? QString::number(a.act) : QStringLiteral("—"));
            setCell(2,
                    QStringLiteral("%1 - %2%3")
                            .arg(dashIfEmpty(a.artist),
                                    a.title.isEmpty() ? dashIfEmpty(a.artist) : a.title,
                                    best.items.at(i).locked ? tr("  [locked]") : QString()));
            if (i + 1 >= best.items.size()) {
                // The closer hands over to nobody: nothing here to plan or edit.
                for (int column = 3; column < 8; ++column) {
                    setCell(column, QStringLiteral("—"));
                }
                continue;
            }
            const TrackFeatures b = byId.value(best.items.at(i + 1).mixxxTrackId);
            PairKey key;
            key.sourceTrackId = a.mixxxTrackId;
            key.targetTrackId = b.mixxxTrackId;
            const TransitionOverride resolved =
                    OverrideRepository::resolve(pairs, actRules, key, a.act);
            const TransitionPlan plan = TransitionPlanner::plan(a, b, intent, resolved);

            setCell(3, msToClock(plan.sourceExitMs));
            // Say where each value came from, so "why is this a Cut?" is answerable
            // without reading the source.
            const auto origin = [&](bool fromPair, bool fromAct) {
                return fromPair ? tr(" (pair)") : (fromAct ? tr(" (act)") : QString());
            };
            setCell(4,
                    transitionTypeName(plan.type) +
                            origin(pairs.value(key).type.has_value(),
                                    actRules.value(a.act).type.has_value()));
            setCell(5,
                    QString::number(plan.durationBars) +
                            origin(pairs.value(key).bars.has_value(),
                                    actRules.value(a.act).bars.has_value()));
            setCell(6, QStringLiteral("%1%").arg(qRound(
                               best.items.at(i + 1).pairScoreFromPrevious * 100.0)));
            setCell(7,
                    plan.warnings.isEmpty()
                            ? best.items.at(i + 1).explanationFromPrevious
                            : plan.warnings.join(QStringLiteral("; ")));
        }
        grid->resizeColumnsToContents();
    };
    rebuildGrid();

    // Editing acts on the selected row, so the thing you change is the thing you
    // are looking at (RF-010).
    auto* editRow = new QHBoxLayout();
    editRow->addWidget(new QLabel(tr("Selected transition:"), &dialog));
    auto* typeEdit = new QComboBox(&dialog);
    typeEdit->addItem(tr("Automatic"), -1);
    typeEdit->addItem(tr("Crossfade"), static_cast<int>(TransitionType::Crossfade));
    typeEdit->addItem(tr("EQ Blend"), static_cast<int>(TransitionType::EqBlend));
    typeEdit->addItem(tr("Bass Swap"), static_cast<int>(TransitionType::BassSwap));
    typeEdit->addItem(tr("Breakdown swap"), static_cast<int>(TransitionType::BreakdownSwap));
    typeEdit->addItem(tr("Filter"), static_cast<int>(TransitionType::FilterTransition));
    typeEdit->addItem(tr("Cut on phrase"), static_cast<int>(TransitionType::CutOnPhrase));
    editRow->addWidget(typeEdit);
    editRow->addWidget(new QLabel(tr("Bars:"), &dialog));
    auto* barsEdit = new QComboBox(&dialog);
    barsEdit->addItem(tr("Automatic"), -1);
    for (int bars : {8, 16, 32, 64}) {
        barsEdit->addItem(QString::number(bars), bars);
    }
    editRow->addWidget(barsEdit);
    editRow->addWidget(new QLabel(tr("Exit @:"), &dialog));
    auto* exitEdit = new QLineEdit(&dialog);
    exitEdit->setMaximumWidth(70);
    exitEdit->setPlaceholderText(tr("auto"));
    exitEdit->setToolTip(tr("mm:ss — where this track hands over. Empty = the analysis "
                            "decides. This is how a flash is kept to 90 s (spec 9)."));
    editRow->addWidget(exitEdit);
    auto* previewButton = new QPushButton(tr("Preview on decks"), &dialog);
    auto* repeatButton = new QPushButton(tr("Repeat"), &dialog);
    auto* stopButton = new QPushButton(tr("Cancel preview"), &dialog);
    editRow->addWidget(previewButton);
    editRow->addWidget(repeatButton);
    editRow->addWidget(stopButton);
    editRow->addStretch(1);
    layout->addLayout(editRow);

    // Act-wide rule: spec 16 thinks in blocks ("long blends in the melodic acts,
    // fast ones in the flashes"), so setting 36 pairs by hand is the wrong tool.
    auto* actRow = new QHBoxLayout();
    actRow->addWidget(new QLabel(tr("Rule for act:"), &dialog));
    auto* actPick = new QComboBox(&dialog);
    for (int act = 1; act <= 7; ++act) {
        actPick->addItem(tr("Act %1").arg(act), act);
    }
    actRow->addWidget(actPick);
    auto* actType = new QComboBox(&dialog);
    auto* actBars = new QComboBox(&dialog);
    for (int i = 0; i < typeEdit->count(); ++i) {
        actType->addItem(typeEdit->itemText(i), typeEdit->itemData(i));
    }
    for (int i = 0; i < barsEdit->count(); ++i) {
        actBars->addItem(barsEdit->itemText(i), barsEdit->itemData(i));
    }
    actRow->addWidget(actType);
    actRow->addWidget(new QLabel(tr("Bars:"), &dialog));
    actRow->addWidget(actBars);
    auto* applyAct = new QPushButton(tr("Apply to act"), &dialog);
    actRow->addWidget(applyAct);
    auto* resetEdits = new QPushButton(tr("Reset all edits"), &dialog);
    resetEdits->setToolTip(
            tr("Sets every transition back to Automatic — all pair choices and act "
               "rules. Snapshots are not affected."));
    actRow->addWidget(resetEdits);
    actRow->addStretch(1);
    layout->addLayout(actRow);

    auto* previewStatus = new QLabel(
            tr("A pair's own choice beats its act's rule; both beat automatic."), &dialog);
    previewStatus->setWordWrap(true);
    layout->addWidget(previewStatus);

    const auto selectedRow = [grid, &best]() {
        const int row = grid->currentRow();
        return (row >= 0 && row + 1 < best.items.size()) ? row : -1;
    };
    const auto pairKeyAt = [&best](int i) {
        PairKey key;
        key.sourceTrackId = best.items.at(i).mixxxTrackId;
        key.targetTrackId = best.items.at(i + 1).mixxxTrackId;
        return key;
    };
    const auto showSelection = [this, selectedRow, pairKeyAt, typeEdit, barsEdit, exitEdit]() {
        const int row = selectedRow();
        // setCurrentIndex fires currentIndexChanged, which would save straight
        // back over what we just read; block while syncing the widgets.
        const QSignalBlocker b1(typeEdit);
        const QSignalBlocker b2(barsEdit);
        const QSignalBlocker b3(exitEdit);
        const bool editable = row >= 0;
        typeEdit->setEnabled(editable);
        barsEdit->setEnabled(editable);
        exitEdit->setEnabled(editable);
        if (!editable) {
            return;
        }
        // Only the pair's OWN choice is shown: its act's rule is context, not
        // something you would be editing from this row.
        const TransitionOverride own = m_pController->loadOverrides().value(pairKeyAt(row));
        typeEdit->setCurrentIndex(
                typeEdit->findData(own.type ? static_cast<int>(*own.type) : -1));
        barsEdit->setCurrentIndex(barsEdit->findData(own.bars ? *own.bars : -1));
        exitEdit->setText(own.sourceExitMs ? msToClock(*own.sourceExitMs) : QString());
    };
    const auto saveSelection =
            [this, selectedRow, pairKeyAt, typeEdit, barsEdit, exitEdit, rebuildGrid]() {
                const int row = selectedRow();
                if (row < 0) {
                    return;
                }
                TransitionOverride override;
                if (typeEdit->currentData().toInt() >= 0) {
                    override.type = static_cast<TransitionType>(typeEdit->currentData().toInt());
                }
                if (barsEdit->currentData().toInt() > 0) {
                    override.bars = barsEdit->currentData().toInt();
                }
                override.sourceExitMs = clockToMs(exitEdit->text());
                const PairKey key = pairKeyAt(row);
                m_pController->setOverride(key.sourceTrackId, key.targetTrackId, override);
                rebuildGrid();
            };
    connect(grid, &QTableWidget::itemSelectionChanged, &dialog, [showSelection]() {
        showSelection();
    });
    connect(typeEdit,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            &dialog,
            [saveSelection](int) { saveSelection(); });
    connect(barsEdit,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            &dialog,
            [saveSelection](int) { saveSelection(); });
    connect(exitEdit, &QLineEdit::editingFinished, &dialog, [saveSelection]() {
        saveSelection();
    });
    connect(applyAct,
            &QPushButton::clicked,
            &dialog,
            [this, actPick, actType, actBars, rebuildGrid, previewStatus]() {
                TransitionOverride rule;
                if (actType->currentData().toInt() >= 0) {
                    rule.type = static_cast<TransitionType>(actType->currentData().toInt());
                }
                if (actBars->currentData().toInt() > 0) {
                    rule.bars = actBars->currentData().toInt();
                }
                const int act = actPick->currentData().toInt();
                m_pController->setActRule(act, rule);
                rebuildGrid();
                previewStatus->setText(rule.isEmpty()
                                ? tr("Act %1 back to automatic.").arg(act)
                                : tr("Act %1 rule applied — pairs with their own "
                                     "choice keep it.")
                                          .arg(act));
            });
    connect(resetEdits,
            &QPushButton::clicked,
            &dialog,
            [this, &dialog, rebuildGrid, showSelection, previewStatus]() {
                if (QMessageBox::question(&dialog,
                            tr("Reset all edits"),
                            tr("Set every transition back to Automatic?\n\nThis clears all "
                               "pair choices and act rules. Your snapshots and the "
                               "generated order are not affected."),
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No) != QMessageBox::Yes) {
                    return;
                }
                const int removed = m_pController->resetTransitionEdits();
                rebuildGrid();
                showSelection();
                previewStatus->setText(removed < 0
                                ? tr("Could not reset the edits.")
                                : tr("Reset %1 edit(s) — everything is Automatic again.")
                                          .arg(removed));
            });
    connect(previewButton,
            &QPushButton::clicked,
            &dialog,
            [this, selectedRow, &best, &byId, intent, previewStatus]() {
                const int row = selectedRow();
                if (row < 0) {
                    previewStatus->setText(tr("Select a transition row first."));
                    return;
                }
                const TrackFeatures from = byId.value(best.items.at(row).mixxxTrackId);
                const TrackFeatures to = byId.value(best.items.at(row + 1).mixxxTrackId);
                if (!m_pController->previewTransition(from, to, intent)) {
                    previewStatus->setText(tr("Preview unavailable — need two decks and "
                                              "both tracks in the library."));
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
                previewStatus->setText(previewStateText(state) + QStringLiteral(" — ") + message);
            });
    grid->selectRow(0);
    showSelection();

    // --- Fase 7: run the whole set ---
    auto* setRow = new QHBoxLayout();
    // Spec 19 (Ensaio 3) rehearses blocks: one act, or one act handing over to
    // the next, without sitting through everything before it.
    auto* scopeSelector = new QComboBox(&dialog);
    scopeSelector->addItem(tr("Whole set"), 0);
    for (int act = 1; act <= 7; ++act) {
        scopeSelector->addItem(tr("Act %1").arg(act), act * 100 + act);
    }
    for (int act = 1; act < 7; ++act) {
        scopeSelector->addItem(
                tr("Act %1 → %2").arg(act).arg(act + 1), act * 100 + act + 1);
    }
    setRow->addWidget(scopeSelector);

    auto* runSetButton = new QPushButton(tr("Run set"), &dialog);
    runSetButton->setToolTip(
            tr("Plays the whole sequence on the decks, handing over automatically."));
    auto* pauseButton = new QPushButton(tr("Pause"), &dialog);
    auto* resumeButton = new QPushButton(tr("Resume"), &dialog);
    auto* skipButton = new QPushButton(tr("Skip"), &dialog);
    auto* stopSetButton = new QPushButton(tr("Stop set"), &dialog);
    setRow->addWidget(runSetButton);
    setRow->addWidget(pauseButton);
    setRow->addWidget(resumeButton);
    setRow->addWidget(skipButton);
    setRow->addWidget(stopSetButton);
    setRow->addStretch(1);
    layout->addLayout(setRow);
    auto* setStatus = new QLabel(
            tr("Runs the sequence end to end. Moving the crossfader hands the decks back."),
            &dialog);
    setStatus->setWordWrap(true);
    layout->addWidget(setStatus);

    connect(runSetButton,
            &QPushButton::clicked,
            &dialog,
            [this, &best, intent, setStatus, scopeSelector]() {
                const int scope = scopeSelector->currentData().toInt();
                const int fromAct = scope / 100;
                const int toAct = scope % 100;
                if (!m_pController->runSet(best, intent, fromAct, toAct)) {
                    setStatus->setText(tr("Cannot run it — need two decks, every track in "
                                          "the library, and tracks in the chosen acts."));
                }
            });
    connect(pauseButton, &QPushButton::clicked, &dialog, [this]() {
        m_pController->pauseSet();
    });
    connect(resumeButton, &QPushButton::clicked, &dialog, [this]() {
        m_pController->resumeSet();
    });
    connect(skipButton, &QPushButton::clicked, &dialog, [this]() {
        m_pController->skipSetTrack();
    });
    connect(stopSetButton, &QPushButton::clicked, &dialog, [this]() {
        m_pController->cancelSet();
    });
    auto* setQueue = new QLabel(&dialog);
    setQueue->setWordWrap(true);
    layout->addWidget(setQueue);

    connect(m_pController,
            &MusicSyncController::setStateChanged,
            &dialog,
            [setStatus](int state, const QString& message) {
                setStatus->setText(setStateText(state) + QStringLiteral(" — ") + message);
            });
    // RF-011: say which track is going onto a deck. The executor loads one deck
    // ahead, so this fires for the track being cued, not the one playing.
    connect(m_pController,
            &MusicSyncController::setPositionChanged,
            &dialog,
            [setQueue, &best](int position, const QString& what) {
                setQueue->setText(tr("Cued %1/%2 on the free deck: %3")
                                          .arg(position + 1)
                                          .arg(best.items.size())
                                          .arg(what));
            });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog.exec();
    // Stop any running automation if the user closes the dialog mid-preview.
    // The set keeps running: it is meant to outlive the dialog.
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
