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

## Fase 2 — Adaptação da análise nativa ✅ (2026-07-12)
- [x] `NativeAnalysisAdapter` (`analysis/`): lê BPM, key, beatgrid, ReplayGain, intro/outro (cues) e stream info de um `Track` do Mixxx para um `TrackFeatures`. **Camelot via `KeyUtils::keyToString(key, Lancelot)`** (reuso do Mixxx, sem tabela própria). Detecção de "precisa análise" espelhando o gate do Mixxx.
- [x] `TrackFeatures` (`domain/`) e `AnalysisRepository` (`analysis/`): persistência no sidecar.
- [x] **Migration v2**: tabela `MusicSyncTrackFeatures` (snapshot por `mixxx_track_id`).
- [x] Controller passou a receber `CoreServices`; `snapshotLibrary(limit)` lê faixas da biblioteca (`getTrackCollectionManager()` + query `library`), extrai e grava snapshots; `loadSnapshots()`/`snapshotCount()`.
- [x] Painel: tabela (Artista, Título, BPM, Camelot, Key, Duração, ReplayGain, Analisado) + botões "ler análise nativa" e "recarregar".
- [x] **Build ON** OK; **testes** `MusicSyncSidecarDatabaseTest` (agora valida migração v2) e `MusicSyncAnalysisRepositoryTest` passam (2/2).
- [x] OFF intacto por construção: única mudança upstream é uma linha dentro do `#ifdef` de `slotMusicSync` (passa `CoreServices` em vez de `getSettings()`).
- [x] **Fase 2b**: botão "Analyze missing (Mixxx)" — `MusicSyncController::analyzeMissing()` dispara a análise nativa do Mixxx via `Library::createTrackAnalysisScheduler` para faixas sem beatgrid/bpm ou sem key; progresso e conclusão refletidos no painel, com re-snapshot ao terminar.
- [ ] (Interativo — usuário) Abrir *Options → Music Sync*, "ler análise nativa" (conferir ~20 faixas) e "Analyze missing" (rodar a análise do Mixxx para faixas faltantes).

## Fase 3 — Análise avançada mínima ✅ (2026-07-12)
- [x] `AdvancedAnalysisAdapter`: **curva de energia** e **presença de graves** derivadas do *waveform summary* do Mixxx (bandas low/mid/high; energia = low+mid+high, graves = low), **sem re-decodificar áudio**; curvas normalizadas 0..1 + energia geral comparável entre faixas.
- [x] **Frases** computadas analiticamente do tempo (firstBeat + BPM, constante — robusto p/ eletrônica); fases de 16 compassos.
- [x] **Migration v3**: colunas `overall_energy`/`energy_curve`/`bass_curve`/`phrase_markers`/`advanced_analyzer_version` (curvas e frases em JSON) em `MusicSyncTrackFeatures`.
- [x] O snapshot também computa e grava os campos avançados; painel ganhou colunas **Energy** e **Phrases**.
- [x] **Testes** do núcleo DSP puro (`computeCurves` bucketização/normalização; `computePhrases` tempo constante + guards). ctest do módulo: **6/6 verdes**.
- [x] **Fase 3b**: **seções** (heurística de energia: Intro/Groove/Build/Drop/Breakdown/Outro, com "Build" antes de um Drop) e **janelas de transição** de entrada/saída (phrase-aligned, ranqueadas por estabilidade de energia); **migration v4** (`sections`/`entry_windows`/`exit_windows` em JSON); painel com colunas **Sections** e **Exit @**; testes das heurísticas. **ctest do módulo: 9/9**.
- Correção manual dos campos avançados fica como refinamento futuro (não bloqueia o MVP).
- [ ] (Interativo — usuário) Conferir na GUI ~5 faixas eletrônicas com janelas de transição plausíveis (critério de saída da Fase 3).

## Fase 4 — Motor de sequência ✅ (2026-07-12)
- [x] **Compatibilidade harmônica (Camelot)** explícita e testada (`HarmonicCompatibility`): parse "8A"/"12B" + regras da roda (mesmo código 1.0; ±1 mesma letra 0.9; relativa A/B 0.75; distantes menor; incompatível nunca bloqueia).
- [x] **Curva de energia** (`EnergyCurve`): presets ascendente/pico-central/pico-final/ondas/constante/custom + amostragem linear.
- [x] **`PairScore` versionado** (`PairScorer`, pesos §18.2) com breakdown decomponível (harmônica/tempo/frase/energia/vocal/janela/estilo) + penalidades (tempo acima da tolerância; faixa não analisada). Componentes puros e testáveis.
- [x] **Explicações** (`ExplanationBuilder`) derivadas do mesmo breakdown (RF-008).
- [x] **Testes**: Camelot, tempo, pair score (forte > fraco), penalidade de não-analisada, presets, explicação. **ctest do módulo: 16/16**.
- [x] **Fase 4b**: `SequenceOptimizer` (guloso com look-ahead + 2-opt, **posições travadas** p/ curadoria híbrida, **≥3 alternativas**, determinístico) com testes (cobertura de todas as faixas, locks respeitados, reprodutível). Painel: seletor de preset de energia + botão **"Generate sequence"** que roda o otimizador sobre os snapshots analisados e mostra a sequência com score e explicação por par. **ctest do módulo: 19/19**.
- Persistência de projetos/arranjos (migration) e UI de *locking* ficam como refinamento — o engine já suporta posições travadas.
- [ ] (Interativo — usuário) *Options → Music Sync* → "Read native analysis" → **Generate sequence**, conferir a ordem + explicações por par (respeitando a curva de energia escolhida).

## Fase 5 — Planejador de transições ✅ (2026-07-12)
- [x] **Domínio** `domain/transition_plan.h`: `TransitionType` (Crossfade, EqBlend, BassSwap, FilterTransition, CutOnPhrase, AutoDjFallback), `ControlAction` (controle @ beat, valor instantâneo) e `AutomationRamp` (rampa linear em [fromBeat,toBeat]); `TransitionPlan` = plano declarativo/serializável (tipo, posições source/target em ms, duração em compassos/beats/ms, `targetBpm` + `sourceRateRatio`/`targetRateRatio` para beatsync, automações, score, confiança, explicação, warnings). **Sem tocar no motor de áudio** — o executor (fase posterior) consome o plano.
- [x] **`TransitionPlanner`** (`planner/`): `chooseType()` escolhe a estratégia a partir das features (sem janelas ou não analisado → **AutoDjFallback**; tempo muito acima da tolerância *ou* key incompatível → **CutOnPhrase**; dançante + harmônico + janelas confiáveis → **BassSwap**; janelas confiáveis → **EqBlend**; senão **Crossfade**). `plan()` computa posições (da 1ª janela de saída/entrada), duração (preferida, limitada pelos compassos da janela; curta para cut/fallback), rates de beatsync p/ o `targetBpm`, automações por tipo (bass swap/EQ blend com kill+troca de graves; filtro; cut na frase; crossfade linear), score/confiança (do `PairScorer` + confiança das janelas) e explicação/warnings.
- [x] **Testes** (`music_sync_planner_test.cpp`): plano válido p/ par bom (posições/rates/duração/automações/tipo), fallback sem janelas, cut quando incompatível. **ctest do módulo: 22/22 verdes.**
- [x] Painel "Generate sequence" agora anexa, por par consecutivo, a **transição escolhida** (tipo, compassos, confiança).
- Persistência do plano de transição e edição manual ficam como refinamento; o executor real chega na Fase 6.
- [ ] (Interativo — usuário) *Generate sequence* e conferir os tipos de transição sugeridos entre faixas (EQ/Bass Swap para pares harmônicos, Cut para saltos de tempo/tom).

## Próximas fases (roadmap)
- **Fase 1, 2, 3, 4, 5** — ✅ concluídas (acima).
- **Fase 6** — Prévia em dois decks (1º marco técnico: transição automática de 32 compassos entre 2 faixas), consumindo o `TransitionPlan`.
- **Fase 7** — Executor de mini-set (máquina de estados, fila, automações, ManualOverride).
- **Fase 8** — Gravação e relatórios (WAV master via Mixxx, tracklist, session-report). **→ set de 35 faixas executável e gravável.**
- **Fase 9–10** — Worker Python opcional; Echo Out/loop-out; stems; LLM de intenção; render offline.

## Limitações conhecidas
- Gravação é em tempo real (set de 90 min = 90 min); render offline fica para pós-MVP.
- Detecção de vocais/seções é heurística no MVP (worker Python melhora depois).
- Momentos de artista ao vivo (timing de mashup, "deixar a pista cantar") permanecem manuais via ManualOverride.
