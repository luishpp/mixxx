#include "music_sync/music_sync_controller.h"

#include <QDir>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>
#include <QVariant>
#include <algorithm>

#include "analyzer/analyzerprogress.h"
#include "analyzer/analyzerscheduledtrack.h"
#include "analyzer/analyzerthread.h"
#include "coreservices.h"
#include "library/dao/analysisdao.h"
#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "mixer/playermanager.h"
#include "music_sync/analysis/advanced_analysis_adapter.h"
#include "music_sync/analysis/analysis_repository.h"
#include "music_sync/analysis/native_analysis_adapter.h"
#include "music_sync/analysis/override_repository.h"
#include "music_sync/planner/energy_normalizer.h"
#include "music_sync/planner/preview_compiler.h"
#include "music_sync/planner/sequence_optimizer.h"
#include "music_sync/planner/set_compiler.h"
#include "music_sync/planner/transition_planner.h"
#include "music_sync/preview/preview_executor.h"
#include "music_sync/preview/set_executor.h"
#include "music_sync/sidecar_database.h"
#include "preferences/usersettings.h"
#include "track/track.h"
#include "track/trackid.h"
#include "util/logger.h"
#include "waveform/waveform.h"
#include "waveform/waveformfactory.h"

namespace {
const mixxx::Logger kLogger("music_sync");

const QString kSidecarFileName = QStringLiteral("music-sync-dj.sqlite");
const QString kSettingModuleEnabled = QStringLiteral("module_enabled");

// A cold library track has no waveform summary in memory, so the advanced
// analysis (energy/sections/transition windows) would come up empty. Load the
// already-stored overview from the library DB (no decode, no audio thread) so
// getWaveformSummary() returns data. Silently leaves it null if the track was
// never waveform-analyzed.
void ensureWaveformSummaryLoaded(
        TrackCollection* pCollection, const TrackPointer& pTrack, TrackId trackId) {
    if (!pCollection || !pTrack || !pTrack->getWaveformSummary().isNull()) {
        return;
    }
    const QList<AnalysisDao::AnalysisInfo> analyses =
            pCollection->getAnalysisDAO().getAnalysesForTrackByType(
                    trackId, AnalysisDao::TYPE_WAVESUMMARY);
    for (const AnalysisDao::AnalysisInfo& analysis : analyses) {
        if (WaveformFactory::waveformSummaryVersionToVersionClass(analysis.version) !=
                WaveformFactory::VC_USE) {
            continue;
        }
        ConstWaveformPointer pSummary(WaveformFactory::loadWaveformFromAnalysis(analysis));
        if (!pSummary.isNull()) {
            pTrack->setWaveformSummary(pSummary);
            return;
        }
    }
}
} // anonymous namespace

namespace mixxx::music_sync {

MusicSyncController::MusicSyncController(
        std::shared_ptr<mixxx::CoreServices> pCoreServices, QObject* parent)
        : QObject(parent),
          m_pCoreServices(std::move(pCoreServices)),
          m_ready(false),
          m_pScheduler(TrackAnalysisScheduler::NullPointer()) {
}

MusicSyncController::~MusicSyncController() = default;

bool MusicSyncController::initialize() {
    if (m_ready) {
        return true;
    }
    if (!m_pCoreServices) {
        kLogger.warning() << "No core services; Music Sync stays disabled";
        return false;
    }
    const UserSettingsPointer pConfig = m_pCoreServices->getSettings();
    if (!pConfig) {
        kLogger.warning() << "No settings available; Music Sync stays disabled";
        return false;
    }

    const QString path =
            QDir(pConfig->getSettingsPath()).absoluteFilePath(kSidecarFileName);
    m_pDatabase = std::make_unique<SidecarDatabase>(path);

    if (!m_pDatabase->open() || !m_pDatabase->applyMigrations()) {
        kLogger.warning() << "Sidecar unavailable; Music Sync stays disabled";
        m_pDatabase.reset();
        return false;
    }

    m_ready = true;
    kLogger.info() << "Music Sync initialized (schema v" << schemaVersion() << ")";
    return true;
}

QString MusicSyncController::sidecarPath() const {
    if (m_pDatabase) {
        return m_pDatabase->filePath();
    }
    if (m_pCoreServices) {
        if (const UserSettingsPointer pConfig = m_pCoreServices->getSettings()) {
            return QDir(pConfig->getSettingsPath()).absoluteFilePath(kSidecarFileName);
        }
    }
    return QString();
}

int MusicSyncController::schemaVersion() const {
    return m_pDatabase ? m_pDatabase->schemaVersion() : 0;
}

bool MusicSyncController::isModuleEnabled() const {
    if (!m_pDatabase) {
        return false;
    }
    return m_pDatabase->getSetting(kSettingModuleEnabled, QStringLiteral("0")) ==
            QStringLiteral("1");
}

bool MusicSyncController::setModuleEnabled(bool enabled) {
    if (!m_pDatabase) {
        return false;
    }
    return m_pDatabase->setSetting(
            kSettingModuleEnabled, enabled ? QStringLiteral("1") : QStringLiteral("0"));
}

QVector<TrackFeatures> MusicSyncController::snapshotLibrary(int limit) {
    QVector<TrackFeatures> result;
    if (!m_ready || !m_pCoreServices) {
        return result;
    }
    const std::shared_ptr<TrackCollectionManager> pTrackCollectionManager =
            m_pCoreServices->getTrackCollectionManager();
    if (!pTrackCollectionManager) {
        return result;
    }
    TrackCollection* pCollection = pTrackCollectionManager->internalCollection();
    if (!pCollection) {
        return result;
    }

    QSqlQuery idQuery(pCollection->database());
    idQuery.prepare(QStringLiteral(
            "SELECT id FROM library WHERE mixxx_deleted=0 ORDER BY id LIMIT :limit"));
    idQuery.bindValue(QStringLiteral(":limit"), limit);
    if (!idQuery.exec()) {
        kLogger.warning() << "Could not query library track ids:" << idQuery.lastError();
        return result;
    }

    AnalysisRepository repository(m_pDatabase->database());
    const int idColumn = idQuery.record().indexOf(QStringLiteral("id"));
    while (idQuery.next()) {
        const TrackId trackId(idQuery.value(idColumn));
        const TrackPointer pTrack = pTrackCollectionManager->getTrackById(trackId);
        if (!pTrack) {
            continue;
        }
        ensureWaveformSummaryLoaded(pCollection, pTrack, trackId);
        TrackFeatures features = NativeAnalysisAdapter::extract(pTrack);
        AdvancedAnalysisAdapter::compute(pTrack, features.durationMs, &features);
        repository.upsert(features);
        result.append(features);
    }
    kLogger.info() << "Snapshotted" << result.size() << "library tracks";

    // Make the sidecar mirror the library instead of accumulating orphans:
    // drop snapshots whose track is gone. This needs the COMPLETE live id set,
    // not the limited batch above — pruning against a partial set would delete
    // valid snapshots. If the query fails we skip pruning rather than risk it.
    QSqlQuery liveQuery(pCollection->database());
    if (liveQuery.exec(QStringLiteral(
                "SELECT id FROM library WHERE mixxx_deleted=0"))) {
        QVector<std::int64_t> liveIds;
        const int liveIdColumn = liveQuery.record().indexOf(QStringLiteral("id"));
        while (liveQuery.next()) {
            liveIds.append(liveQuery.value(liveIdColumn).toLongLong());
        }
        repository.removeMissing(liveIds);
    } else {
        kLogger.warning() << "Could not list live library ids; skipping prune:"
                          << liveQuery.lastError();
    }

    // The sidecar keeps the raw value; callers see the library-relative scale.
    normalizeLibraryEnergy(&result);
    return result;
}

int MusicSyncController::clearSnapshots() {
    if (!m_pDatabase) {
        return -1;
    }
    return AnalysisRepository(m_pDatabase->database()).clear();
}

QVector<TrackFeatures> MusicSyncController::loadSnapshots() const {
    if (!m_pDatabase) {
        return {};
    }
    QVector<TrackFeatures> all = AnalysisRepository(m_pDatabase->database()).loadAll();
    normalizeLibraryEnergy(&all);
    return all;
}

int MusicSyncController::snapshotCount() const {
    if (!m_pDatabase) {
        return 0;
    }
    return AnalysisRepository(m_pDatabase->database()).count();
}

QVector<Arrangement> MusicSyncController::generateSequences(const MixIntent& intent) {
    QVector<Arrangement> out;
    if (!m_pDatabase) {
        return out;
    }
    const AnalysisRepository repository(m_pDatabase->database());
    // Normalize over the whole set (exactly what loadSnapshots() does) before
    // filtering, so the energy scale here matches what the panel/preview see.
    QVector<TrackFeatures> all = repository.loadAll();
    normalizeLibraryEnergy(&all);
    QVector<TrackFeatures> analyzed;
    for (const TrackFeatures& features : all) {
        if (features.analyzed) {
            analyzed.append(features);
        }
    }
    if (analyzed.size() < 2) {
        return out;
    }
    SequenceOptimizer::Options options;
    options.intent = intent;
    options.numAlternatives = 3;
    return SequenceOptimizer::arrange(analyzed, options);
}

int MusicSyncController::analyzeMissing(int limit) {
    if (!m_ready || !m_pCoreServices || m_pScheduler) {
        return 0; // not ready, or a run is already in progress
    }
    const std::shared_ptr<Library> pLibrary = m_pCoreServices->getLibrary();
    const std::shared_ptr<TrackCollectionManager> pTrackCollectionManager =
            m_pCoreServices->getTrackCollectionManager();
    if (!pLibrary || !pTrackCollectionManager) {
        return 0;
    }
    TrackCollection* pCollection = pTrackCollectionManager->internalCollection();
    if (!pCollection) {
        return 0;
    }

    QSqlQuery idQuery(pCollection->database());
    idQuery.prepare(QStringLiteral(
            "SELECT id FROM library WHERE mixxx_deleted=0 ORDER BY id LIMIT :limit"));
    idQuery.bindValue(QStringLiteral(":limit"), limit);
    if (!idQuery.exec()) {
        kLogger.warning() << "Could not query library track ids:" << idQuery.lastError();
        return 0;
    }

    QList<AnalyzerScheduledTrack> tracksToAnalyze;
    const int idColumn = idQuery.record().indexOf(QStringLiteral("id"));
    while (idQuery.next()) {
        const TrackId trackId(idQuery.value(idColumn));
        const TrackPointer pTrack = pTrackCollectionManager->getTrackById(trackId);
        if (pTrack && NativeAnalysisAdapter::needsAnalysis(pTrack)) {
            tracksToAnalyze.append(AnalyzerScheduledTrack(trackId));
        }
    }
    if (tracksToAnalyze.isEmpty()) {
        return 0;
    }

    const int numThreads = std::max(1, QThread::idealThreadCount());
    m_pScheduler = pLibrary->createTrackAnalysisScheduler(
            numThreads,
            // WithWaveform is required: the advanced analysis (energy/sections/
            // transition windows) reads the waveform summary, so beats alone
            // would leave every transition on the Auto DJ fallback path.
            static_cast<AnalyzerModeFlags>(AnalyzerModeFlags::WithBeats |
                    AnalyzerModeFlags::WithWaveform | AnalyzerModeFlags::LowPriority));

    connect(m_pScheduler.get(),
            &TrackAnalysisScheduler::progress,
            this,
            [this](AnalyzerProgress, int currentTrackNumber, int totalTracks) {
                emit analysisProgress(currentTrackNumber, totalTracks);
            });
    connect(m_pScheduler.get(),
            &TrackAnalysisScheduler::finished,
            this,
            [this, limit]() {
                // Tracks now have BPM/key; refresh the snapshots, then tear the
                // scheduler down (same pattern as Mixxx's AnalysisFeature).
                snapshotLibrary(limit);
                m_pScheduler.reset();
                emit analysisFinished();
            });

    const int scheduled = m_pScheduler->scheduleTracks(tracksToAnalyze);
    if (scheduled > 0) {
        m_pScheduler->resume();
    } else {
        m_pScheduler.reset();
    }
    kLogger.info() << "Scheduled" << scheduled << "tracks for native analysis";
    return scheduled;
}

bool MusicSyncController::previewTransition(
        const TrackFeatures& from, const TrackFeatures& to, const MixIntent& intent) {
    if (!m_pCoreServices) {
        return false;
    }
    const std::shared_ptr<PlayerManager> pPlayerManager =
            m_pCoreServices->getPlayerManager();
    const std::shared_ptr<TrackCollectionManager> pTrackCollectionManager =
            m_pCoreServices->getTrackCollectionManager();
    if (!pPlayerManager || !pTrackCollectionManager) {
        return false;
    }
    if (pPlayerManager->numberOfDecks() < 2) {
        kLogger.warning() << "Need at least two decks for a transition preview";
        return false;
    }
    const TrackPointer pSource = pTrackCollectionManager->getTrackById(
            TrackId(QVariant(static_cast<qlonglong>(from.mixxxTrackId))));
    const TrackPointer pTarget = pTrackCollectionManager->getTrackById(
            TrackId(QVariant(static_cast<qlonglong>(to.mixxxTrackId))));
    if (!pSource || !pTarget) {
        kLogger.warning() << "Could not resolve source/target tracks for preview";
        return false;
    }

    PairKey key;
    key.sourceTrackId = from.mixxxTrackId;
    key.targetTrackId = to.mixxxTrackId;
    // Preview what the set will actually do, including the DJ's own choice.
    const TransitionPlan plan = TransitionPlanner::plan(
            from, to, intent, resolvedOverride(from.mixxxTrackId, to.mixxxTrackId, from.act));
    const PreviewProgram program = PreviewCompiler::compile(plan);

    if (!m_pPreviewExecutor) {
        m_pPreviewExecutor = std::make_unique<PreviewExecutor>(
                pPlayerManager, /*source deck*/ 0, /*target deck*/ 1, this);
        connect(m_pPreviewExecutor.get(),
                &PreviewExecutor::stateChanged,
                this,
                &MusicSyncController::previewStateChanged);
    }
    m_pPreviewExecutor->preview(program,
            pSource,
            pTarget,
            static_cast<double>(from.durationMs),
            static_cast<double>(to.durationMs));
    return true;
}

void MusicSyncController::repeatPreview() {
    if (m_pPreviewExecutor) {
        m_pPreviewExecutor->repeat();
    }
}

void MusicSyncController::cancelPreview() {
    if (m_pPreviewExecutor) {
        m_pPreviewExecutor->cancel();
    }
}

bool MusicSyncController::runSet(const Arrangement& arrangement,
        const MixIntent& intent,
        int fromAct,
        int toAct) {
    if (!m_pCoreServices || arrangement.items.isEmpty()) {
        return false;
    }
    const std::shared_ptr<PlayerManager> pPlayerManager =
            m_pCoreServices->getPlayerManager();
    const std::shared_ptr<TrackCollectionManager> pTrackCollectionManager =
            m_pCoreServices->getTrackCollectionManager();
    if (!pPlayerManager || !pTrackCollectionManager ||
            pPlayerManager->numberOfDecks() < SetCompiler::kDeckCount) {
        kLogger.warning() << "Need at least" << SetCompiler::kDeckCount << "decks to run a set";
        return false;
    }

    QHash<std::int64_t, TrackFeatures> byId;
    for (const TrackFeatures& features : loadSnapshots()) {
        byId.insert(features.mixxxTrackId, features);
    }
    // Scope first, compile second: the compiler assigns decks and plans every
    // handover for whatever it is given, so an excerpt comes out self-contained.
    const Arrangement scoped =
            SetCompiler::scopeToActs(arrangement, byId, fromAct, toAct);
    if (scoped.items.isEmpty()) {
        kLogger.warning() << "No tracks in acts" << fromAct << "-" << toAct;
        return false;
    }
    const SetProgram program =
            SetCompiler::compile(scoped, byId, intent, loadOverrides(), loadActRules());
    if (program.items.isEmpty()) {
        return false;
    }

    // Resolve every track up front: starting a set only to fail three tracks in
    // would leave the decks mid-air.
    QVector<TrackPointer> tracks;
    for (const SetItem& item : program.items) {
        const TrackPointer pTrack = pTrackCollectionManager->getTrackById(
                TrackId(QVariant(static_cast<qlonglong>(item.mixxxTrackId))));
        if (!pTrack) {
            kLogger.warning() << "Cannot resolve track" << item.mixxxTrackId
                              << "; refusing to start the set";
            return false;
        }
        tracks.append(pTrack);
    }

    if (!m_pSetExecutor) {
        m_pSetExecutor = std::make_unique<SetExecutor>(pPlayerManager, this);
        connect(m_pSetExecutor.get(),
                &SetExecutor::stateChanged,
                this,
                &MusicSyncController::setStateChanged);
        connect(m_pSetExecutor.get(),
                &SetExecutor::positionChanged,
                this,
                &MusicSyncController::setPositionChanged);
    }
    kLogger.info() << "Running set:" << program.explanation;
    m_pSetExecutor->start(program, tracks);
    return true;
}

QHash<PairKey, TransitionOverride> MusicSyncController::loadOverrides() const {
    if (!m_pDatabase) {
        return {};
    }
    return OverrideRepository(m_pDatabase->database()).loadAll();
}

QHash<int, TransitionOverride> MusicSyncController::loadActRules() const {
    if (!m_pDatabase) {
        return {};
    }
    return OverrideRepository(m_pDatabase->database()).loadActRules();
}

bool MusicSyncController::setActRule(int act, const TransitionOverride& rule) {
    if (!m_pDatabase) {
        return false;
    }
    return OverrideRepository(m_pDatabase->database()).saveActRule(act, rule);
}

TransitionOverride MusicSyncController::resolvedOverride(
        std::int64_t sourceTrackId, std::int64_t targetTrackId, int act) const {
    PairKey key;
    key.sourceTrackId = sourceTrackId;
    key.targetTrackId = targetTrackId;
    return OverrideRepository::resolve(loadOverrides(), loadActRules(), key, act);
}

bool MusicSyncController::setOverride(std::int64_t sourceTrackId,
        std::int64_t targetTrackId,
        const TransitionOverride& override) {
    if (!m_pDatabase) {
        return false;
    }
    PairKey key;
    key.sourceTrackId = sourceTrackId;
    key.targetTrackId = targetTrackId;
    return OverrideRepository(m_pDatabase->database()).save(key, override);
}

void MusicSyncController::pauseSet() {
    if (m_pSetExecutor) {
        m_pSetExecutor->pause();
    }
}

void MusicSyncController::resumeSet() {
    if (m_pSetExecutor) {
        m_pSetExecutor->resume();
    }
}

void MusicSyncController::skipSetTrack() {
    if (m_pSetExecutor) {
        m_pSetExecutor->skip();
    }
}

void MusicSyncController::cancelSet() {
    if (m_pSetExecutor) {
        m_pSetExecutor->cancel();
    }
}

} // namespace mixxx::music_sync

#include "moc_music_sync_controller.cpp"
