# Implementation Status

Progresso real do Music Sync DJ (fork do Mixxx 2.5.6). Atualizar a cada etapa relevante.

## Fase 0 — Preparação do fork ✅ (2026-07-12)
- [x] Fork clonado; remotes `origin`=luishpp/mixxx, `upstream`=mixxxdj/mixxx.
- [x] Branch de baseline `music-sync/2.5.6` criada a partir de `tags/2.5.6` (commit `3ebac449`), pristina.
- [x] Ambiente de build no Windows fechado (VS Build Tools 2022: MSVC 14.44 + CMake 3.31 + Ninja).
- [x] Dependências pré-compiladas baixadas e extraídas (`mixxx-deps-2.5-x64-windows-c15790e`).
- [x] **Build do Mixxx 2.5.6 sem modificações: sucesso** (`mixxx.exe` gerado; `--version` → `Mixxx 2.5.6`).
- [x] **Suíte de testes upstream: 100% passed, 0 failed (854 testes)**.
- [x] Scaffolding: `CLAUDE.md`, `docs/music-sync/` (baseline, building-windows, ADR-0001), spec v2.1 na raiz.
- [x] Flag de CMake `MUSIC_SYNC_ENABLED` declarada (placeholder, default OFF, sem módulo ainda).
- [ ] (Interativo — a cargo do usuário) Abrir a GUI e reproduzir uma faixa para conferência auditiva.

Comandos de build validados em `building-windows.md`. Baseline documentada em `upstream-baseline.md`.

## Fase 1 — Esqueleto do módulo ✅ (2026-07-12)
- [x] Módulo `src/music_sync/` (namespace `mixxx::music_sync`), compilado em `mixxx-lib` só com `MUSIC_SYNC_ENABLED=ON`:
  - `feature_flags.h`, `sidecar_database.{h,cpp}`, `music_sync_controller.{h,cpp}`, `dlg_music_sync.{h,cpp}`.
- [x] **Sidecar SQLite** (`music-sync-dj.sqlite` no dir de config do Mixxx), conexão dedicada `"MUSIC_SYNC"`,
  tabelas `MusicSyncSchemaMigrations` + `MusicSyncSettings`, migração v1 com versionamento próprio; falha isolada (não derruba o Mixxx).
- [x] **Painel** *Options → Music Sync* (diálogo mínimo: status do sidecar + um setting persistido).
- [x] Edições upstream mínimas e guardadas por `#ifdef MIXXX_MUSIC_SYNC_ENABLED` (CMake + `wmainmenubar.{h,cpp}` + `mixxxmainwindow.{h,cpp}`) — ver ADR-0002.
- [x] **Build OFF** (default): baseline compila/linka idêntica (verificado).
- [x] **Build ON**: `mixxx.exe` + `mixxx-test.exe` compilam e linkam.
- [x] **Teste** `MusicSyncSidecarDatabaseTest.MigratesAndPersistsSettings` passa (migração idempotente + persistência de settings).
- [ ] (Interativo — usuário) Abrir *Options → Music Sync*, marcar o checkbox e reabrir para confirmar o setting salvo, sem afetar a reprodução.

Nota de build: reconfigurar OFF→ON no mesmo diretório exige forçar o AUTOMOC a re-parsear
(remover `mixxx-lib_autogen/timestamp` e `ParseCache.txt`); um diretório de build limpo evita isso.

## Próximas fases (roadmap)
- **Fase 1** — ✅ concluída (acima).
- **Fase 2** — Adaptação da análise nativa (ler BPM/beats/key/waveform/ReplayGain/cues do Mixxx; snapshot no sidecar).
- **Fase 2** — Adaptação da análise nativa (ler BPM/beats/key/waveform/ReplayGain/cues do Mixxx; snapshot no sidecar).
- **Fase 3** — Análise avançada mínima (energia, frases heurísticas, seções, graves, janelas de transição).
- **Fase 4** — Motor de sequência (Camelot, pair score versionado, otimizador, ≥3 alternativas, explicações; curadoria **híbrida**: âncoras dos atos travadas + otimização do restante).
- **Fase 5** — Planejador de transições (plano declarativo: crossfade, EQ Blend, Bass Swap, Cut em frase; fallback Auto DJ).
- **Fase 6** — Prévia em dois decks (1º marco técnico: transição automática de 32 compassos entre 2 faixas).
- **Fase 7** — Executor de mini-set (máquina de estados, fila, automações, ManualOverride).
- **Fase 8** — Gravação e relatórios (WAV master via Mixxx, tracklist, session-report). **→ set de 35 faixas executável e gravável.**
- **Fase 9–10** — Worker Python opcional; Echo Out/loop-out; stems; LLM de intenção; render offline.

## Limitações conhecidas
- Gravação é em tempo real (set de 90 min = 90 min); render offline fica para pós-MVP.
- Detecção de vocais/seções é heurística no MVP (worker Python melhora depois).
- Momentos de artista ao vivo (timing de mashup, "deixar a pista cantar") permanecem manuais via ManualOverride.
