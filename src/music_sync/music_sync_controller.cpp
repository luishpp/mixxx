#include "music_sync/music_sync_controller.h"

#include <QDir>
#include <QSqlQuery>
#include <QSqlRecord>

#include "coreservices.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "music_sync/analysis/analysis_repository.h"
#include "music_sync/analysis/native_analysis_adapter.h"
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
          m_ready(false) {
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

} // namespace mixxx::music_sync

#include "moc_music_sync_controller.cpp"
