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
#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "mixer/playermanager.h"
#include "music_sync/analysis/advanced_analysis_adapter.h"
#include "music_sync/analysis/analysis_repository.h"
#include "music_sync/analysis/native_analysis_adapter.h"
#include "music_sync/planner/preview_compiler.h"
#include "music_sync/planner/sequence_optimizer.h"
#include "music_sync/planner/transition_planner.h"
#include "music_sync/preview/preview_executor.h"
#include "music_sync/sidecar_database.h"
#include "preferences/usersettings.h"
#include "track/track.h"
#include "track/trackid.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync");

const QString kSidecarFileName = QStringLiteral("music-sync-dj.sqlite");
const QString kSettingModuleEnabled = QStringLiteral("module_enabled");
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
        TrackFeatures features = NativeAnalysisAdapter::extract(pTrack);
        AdvancedAnalysisAdapter::compute(pTrack, features.durationMs, &features);
        repository.upsert(features);
        result.append(features);
    }
    kLogger.info() << "Snapshotted" << result.size() << "library tracks";
    return result;
}

QVector<TrackFeatures> MusicSyncController::loadSnapshots() const {
    if (!m_pDatabase) {
        return {};
    }
    return AnalysisRepository(m_pDatabase->database()).loadAll();
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
    QVector<TrackFeatures> analyzed;
    for (const TrackFeatures& features : repository.loadAll()) {
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
            static_cast<AnalyzerModeFlags>(
                    AnalyzerModeFlags::WithBeats | AnalyzerModeFlags::LowPriority));

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

    const TransitionPlan plan = TransitionPlanner::plan(from, to, intent);
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

} // namespace mixxx::music_sync

#include "moc_music_sync_controller.cpp"
