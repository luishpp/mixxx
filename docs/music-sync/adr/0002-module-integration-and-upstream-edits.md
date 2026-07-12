# ADR 0002 — Module skeleton integration and minimal upstream edits

- **Status:** Aceito
- **Data:** 2026-07-12
- **Fase:** 1 (esqueleto do módulo)

## Contexto

A Fase 1 adiciona o módulo `mixxx::music_sync` (controller, sidecar SQLite, painel
vazio) e o torna acessível pela interface do Mixxx. Isso exige tocar em alguns
arquivos do upstream — algo que as regras do projeto (spec §9.2, CLAUDE.md) mandam
minimizar e justificar por ADR.

## Decisão

### Isolamento
Todo o código do módulo fica em `src/music_sync/` sob o namespace `mixxx::music_sync`,
compilado em `mixxx-lib` apenas quando a opção de CMake `MUSIC_SYNC_ENABLED` está ON
(que define `MIXXX_MUSIC_SYNC_ENABLED` globalmente).

Arquivos do módulo:
- `feature_flags.h`
- `sidecar_database.{h,cpp}` — SQLite sidecar (`music-sync-dj.sqlite`), conexão
  dedicada `"MUSIC_SYNC"`, tabelas `MusicSyncSchemaMigrations` e `MusicSyncSettings`,
  migração v1 com versionamento próprio.
- `music_sync_controller.{h,cpp}` — abre o sidecar, aplica migrações, expõe settings.
- `dlg_music_sync.{h,cpp}` — painel mínimo (status + um setting persistido).
- Teste: `src/test/music_sync_sidecar_test.cpp` (migração + persistência).

### Edições no upstream (todas guardadas por `#ifdef MIXXX_MUSIC_SYNC_ENABLED`)
1. **`CMakeLists.txt`** — bloco `if(MUSIC_SYNC_ENABLED) target_sources(mixxx-lib …)` +
   registro do teste em `mixxx-test`, junto aos demais blocos de features opcionais.
2. **`src/widget/wmainmenubar.h`** — sinal `void showMusicSync();`.
3. **`src/widget/wmainmenubar.cpp`** — uma `QAction` "Music Sync" no menu *Options*,
   conectada ao sinal (mesmo padrão do item "About").
4. **`src/mixxxmainwindow.h`** — forward-decl de `DlgMusicSync`, slot `slotMusicSync()`
   e membro `m_pMusicSyncDlg`.
5. **`src/mixxxmainwindow.cpp`** — include do diálogo, init do membro, `connect` do
   sinal no `connectMenuBar()` e `slotMusicSync()` (cria o diálogo de forma preguiçosa,
   parented à janela principal; nada toca o sidecar até o usuário abrir o painel).

### Padrões seguidos (do próprio Mixxx)
- Diálogo modelado em `DlgDeveloperTools` (QDialog + `UserSettingsPointer`, mostrado com `show()`).
- Menu modelado no item "About" (`WMainMenuBar` emite sinal → `MixxxMainWindow` abre o diálogo).
- SQLite via `QSqlDatabase`/`QSqlQuery` (padrão de `settingsdao`/`mixxxdb`), com conexão separada.
- Logging via `mixxx::Logger` (`const mixxx::Logger kLogger("music_sync");`), sem `Q_LOGGING_CATEGORY`.

## Consequências

- Com `MUSIC_SYNC_ENABLED=OFF` (default), o pré-processador remove todas as adições:
  o binário e o comportamento do Mixxx ficam idênticos à baseline. Verificado por build OFF.
- Com a flag ON, surge o item de menu *Options → Music Sync* que abre o painel.
- Superfície de edição no upstream: 4 arquivos, todas as mudanças guardadas e pequenas.
- Nada roda na thread de áudio; a inicialização do sidecar é preguiçosa (na primeira
  abertura do painel), portanto não afeta a inicialização nem a reprodução.

## Alternativas consideradas
- **Página de preferências** em vez de item de menu — mais invasivo (registrar página no
  `DlgPreferences`); adiado.
- **Controller criado na inicialização** (em `CoreServices`/`MixxxMainWindow`) — mais
  acoplamento no core; para a Fase 1, o diálogo cria o controller sob demanda (superfície mínima).
