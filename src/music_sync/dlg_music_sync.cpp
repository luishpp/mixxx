#include "music_sync/dlg_music_sync.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

#include "music_sync/music_sync_controller.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("music_sync");
} // anonymous namespace

namespace mixxx::music_sync {

DlgMusicSync::DlgMusicSync(QWidget* pParent, UserSettingsPointer pConfig)
        : QDialog(pParent),
          m_pController(new MusicSyncController(std::move(pConfig), this)),
          m_pStatusLabel(nullptr),
          m_pEnabledCheckBox(nullptr) {
    setWindowTitle(tr("Music Sync DJ"));

    const bool ready = m_pController->initialize();

    auto* pLayout = new QVBoxLayout(this);

    auto* pTitle = new QLabel(tr("Music Sync DJ"), this);
    QFont titleFont = pTitle->font();
    titleFont.setBold(true);
    pTitle->setFont(titleFont);
    pLayout->addWidget(pTitle);

    pLayout->addWidget(new QLabel(
            tr("Assistance module — Fase 1 skeleton. No mixing behaviour yet."), this));

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

    auto* pButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::close);
    pLayout->addWidget(pButtons);
}

void DlgMusicSync::slotModuleEnabledToggled(bool checked) {
    if (!m_pController->setModuleEnabled(checked)) {
        kLogger.warning() << "Could not persist module_enabled setting";
    }
}

} // namespace mixxx::music_sync

#include "moc_dlg_music_sync.cpp"
