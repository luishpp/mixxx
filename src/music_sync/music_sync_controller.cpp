#include "music_sync/music_sync_controller.h"

#include <QDir>

#include "music_sync/sidecar_database.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync");

const QString kSidecarFileName = QStringLiteral("music-sync-dj.sqlite");

// Sidecar settings keys.
const QString kSettingModuleEnabled = QStringLiteral("module_enabled");
} // anonymous namespace

namespace mixxx::music_sync {

MusicSyncController::MusicSyncController(UserSettingsPointer pConfig, QObject* parent)
        : QObject(parent),
          m_pConfig(std::move(pConfig)),
          m_ready(false) {
}

MusicSyncController::~MusicSyncController() = default;

bool MusicSyncController::initialize() {
    if (m_ready) {
        return true;
    }
    if (!m_pConfig) {
        kLogger.warning() << "No settings available; Music Sync stays disabled";
        return false;
    }

    const QString path =
            QDir(m_pConfig->getSettingsPath()).absoluteFilePath(kSidecarFileName);
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
    if (m_pConfig) {
        return QDir(m_pConfig->getSettingsPath()).absoluteFilePath(kSidecarFileName);
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

} // namespace mixxx::music_sync

#include "moc_music_sync_controller.cpp"
