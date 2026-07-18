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
Como usar o painel (ordem dos comandos, colunas, preview, troubleshooting): `user-guide.md`.

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

## Fase 6 — Prévia em dois decks ✅ (2026-07-13) — 1º marco técnico (spec §28)
- [x] **Programa de prévia compilado** (`domain/preview_program.h` + `planner/preview_compiler.{h,cpp}`): compila o `TransitionPlan` numa timeline determinística de `ControlWrite` (ações instantâneas viram 1 write; rampas são discretizadas a 1/16 de compasso), ordenada por beat. **Puro/serializável** (spec §20/§28.14), sem tocar no motor. Testado (`music_sync_preview_test.cpp`): expansão de rampa, merge+ordenação de ações/rampas, rampa de duração zero, cópia de metadados. **ctest do módulo: 26/26 verdes.**
- [x] **`PreviewExecutor`** (`preview/preview_executor.{h,cpp}`, QObject na thread da GUI): dirige **dois decks reais** só por `ControlProxy` (thread-safe, nada pesado no áudio — RNF-002). Nomes de controles confirmados na baseline 2.5.6: deck `[ChannelN]` (`play`, `playposition` p/ ler posição e dar seek, `bpm`, `sync_enabled`, `volume`, `orientation`), EQ grave `[EqualizerRack1_[ChannelN]_Effect1],parameter1`, filtro `[QuickEffectRack1_[ChannelN]],super1`, crossfader `[Master],crossfader`; carga via `PlayerManager::slotLoadTrackToPlayer`. Fluxo: carrega o par → espera `track_samples>0` → cue (posições planejadas, orientação A=esq/B=dir, grave de B zerado) → beat-match (`sync_enabled` no B) → executa a automação **guiada pela `playposition` do deck A** (QTimer só amostra a posição — spec §21.3, eventos musicais seguem o playback, não o relógio) → conclui devolvendo os decks ao usuário.
- [x] **Repetir / cancelar / devolver decks / ManualOverride**: `repeat()` re-cue+reexecuta; `cancel()` para a automação sem cortar o áudio; ao terminar/cancelar, `handControlsToUser()` reseta EQ/orientação/volumes/sync; mover o crossfader durante a prévia dispara `ManualOverride` e cede o controle (spec §21.4/RF-013).
- [x] **UI**: no diálogo *Generate sequence*, seletor de par consecutivo + botões **Preview on decks / Repeat / Cancel preview** e rótulo de estado (Loading/Transitioning/Completed/Cancelled/ManualOverride/Failed). Controller: `previewTransition()` (plano→compila→resolve faixas→executa), `repeatPreview()`, `cancelPreview()`, sinal `previewStateChanged`.
- [x] **Build**: `mixxx-lib` + `mixxx-test` + **`mixxx.exe` linkam com a Fase 6** (feature ON).
- Critério de saída é **auditivo** (ouvir uma transição de 32 compassos sem intervenção): o núcleo compilado está testado; a execução no motor exige rodar a GUI com 2 decks e faixas analisadas.
- [ ] (Interativo — usuário) *Generate sequence* → escolher um par → **Preview on decks** e ouvir a transição de ~32 compassos (critério de saída da Fase 6). Ajustar `preferredTransitionBars`/tolerância se necessário.

### Fase 6a — correção: energia vazia no snapshot ✅ (2026-07-13)
Achado em teste real na GUI: **Energy/Sections/Exit @ vinham vazios em toda a biblioteca**, e por isso **todo par caía em `AutoDjFallback`** (sem janelas de transição não há EQ/Bass Swap). Duas causas, ambas corrigidas:
- Faixa "fria" da biblioteca **não tem o waveform em memória** → `getWaveformSummary()` nulo → curva de energia vazia → sem seções nem janelas. `snapshotLibrary()` passou a **carregar o waveform-summary já armazenado** (`AnalysisDao::getAnalysesForTrackByType(TYPE_WAVESUMMARY)` + `WaveformFactory`), sem re-decodificar e fora da thread de áudio; fica nulo se a faixa nunca teve waveform.
- `analyzeMissing()` pedia só `WithBeats` → passou a pedir **`WithBeats | WithWaveform`**, senão faixas analisadas pelo módulo nunca ganham waveform.
- Resultado medido: 215/216 faixas com energia; transições passaram de *Auto DJ em tudo* para **EQ Blend / Cut on phrase**; compatibilidade média 85%→88%, energy fit 39%→62%.

### Fase 6b — calibragem de janelas e energia ✅ (2026-07-13)
Achados no resultado real (todas as transições saindo com 16 bars, alguns "EQ Blend" de 2–4 bars, e Bass Swap nunca escolhido):
- [x] **Janelas de 32 compassos**: uma frase tem 16 compassos, a janela cobria exatamente uma frase e o `plan()` fazia `min(32, 16)` → **nunca chegava aos 32 compassos do marco**. `computeTransitionWindows` agora **mescla frases consecutivas** até `kPreferredWindowBars = 32`.
- [x] **Fim dos rabichos**: a última frase é truncada (`min(barsPerPhrase, restante)`), podendo virar 2–4 compassos; como o ranking era por *estabilidade de energia* e uma janela curta tem poucas amostras (variância baixa), ela ganhava **estabilidade artificialmente alta** e vencia. Agora candidatas abaixo de `kMinWindowBars = 8` são descartadas e o ranking usa **estabilidade ponderada pelo tamanho** (`confidence = stability * min(1, bars/32)`), com desempate determinístico.
- [x] **Energia relativa à biblioteca** (`planner/energy_normalizer.{h,cpp}`): `overallEnergy` era escalado pelo **máximo teórico** (3×255, três bandas saturadas), que música real nunca atinge — a biblioteca inteira ficava em ~0.15–0.35. Consequências: o gate `overallEnergy > 0.45` do **Bass Swap nunca disparava** e os presets de energia (alvo 0..1) eram inalcançáveis (*energy fit* travado). Agora `normalizeLibraryEnergy()` reescreve `overallEnergy` como **percentil 0..1 relativo à biblioteca** (determinístico, desempate por track id; faixas sem curva ficam de fora e intactas). O sidecar continua guardando o valor bruto — a normalização é derivada, aplicada em `snapshotLibrary`/`loadSnapshots`/`generateSequences` **sobre o mesmo conjunto**, para painel, relatório e preview verem a mesma escala.
- [x] **Testes**: merge até 32 compassos, descarte de rabicho, janela longa vence a curta estável, normalizador (spread 0..1, ignora faixas sem curva, faixa única, destrava o gate de energia). **ctest do módulo: 33/33 verdes.**
- Observação (fora do módulo): BPMs errados do beatgrid do Mixxx (ex.: uma faixa detectada a 187.5 BPM) são tratados corretamente pelo planner — avisa *tempo change above tolerance* e escolhe **Cut on phrase** —, mas convém corrigir o beatgrid na faixa.

### Fase 6c — curadoria híbrida de verdade ✅ (2026-07-13)
Teste real revelou que a decisão de projeto da 1ª sessão (*"híbrido: âncoras + otimização"*) **nunca foi ligada**: o `SequenceOptimizer` suportava locks desde a Fase 4, mas ninguém passava nenhum — o motor maximizava score globalmente e embaralhava a narrativa dos 7 atos.
- [x] **Ato manda na ordem**: `TrackFeatures::act` (1..7) vem do comentário ID3 (`ATO 5 | ÂNCORA | ...`), lido no `NativeAnalysisAdapter`, **migration v5** (`act`, `set_function`). O otimizador particiona por ato e otimiza **dentro** de cada um; faixas sem ato vão para o fim. Concatenar atos cria pares nas fronteiras que nenhuma sub-otimização pontuou → **todos os pares são repontuados** no fim.
- [x] **Âncoras travadas** na posição que o plano deu (rank pelo número da faixa dentro do ato) — **migration v6** (`track_number`, da tag `TRCK`). Sem numeração completa no ato, **nada é travado**: travar ali seria chute.
- [x] **Fatia da curva por ato**: bug introduzido pelo particionamento — cada ato era julgado contra a curva 0..1 inteira (o Ato 5 recebia ordem de "começar baixo e construir"). `Options::curveFrom/curveTo` dão a cada ato a fatia que ele ocupa.
- [x] **Peso da harmonia por ato** (§10.5, em vez de aumento global): 0.34 no Portal/Melodic House/Melodic Techno/Final; **0.14** nos flashes e no peak crossover, onde o plano troca tom por impacto; 0.24 (§18.2) no resto. Os demais pesos são reescalados para **todo perfil somar 1.0**, e o perfil vai na versão dos pesos.
- [x] **A ordem do plano sempre lidera** — e isso substituiu uma tentativa anterior de deixar o motor assumir quando superasse o score. **Comparar por score era o instrumento errado**: o Ato 1 real é `3B → 9B → 6B → 10A → 5A`, ruim pela roda Camelot, então o motor "ganhava" e entregava um set pior. O §8 curou por narrativa e ouvido, e os tons vêm da detecção do Mixxx, que erra. As rotas do motor seguem como alternativas, com os scores lado a lado. **O motor informa, não decide.**

## Fase 7 — Executor de mini-set ✅ (2026-07-13)
**Critério de saída cumprido e comprovado no log**: 5 faixas contínuas com 4 transições automáticas
(`Transition 1->2 … 4->5`, decks alternando `[Channel1]`/`[Channel2]`, pré-carga uma faixa à frente).

- [x] **Fase 7a — núcleo puro** (`domain/set_program.h` + `planner/set_compiler.{h,cpp}`): o arranjo aprovado vira `SetProgram` — `SetItem` (deck, entrada, entrega, duração) + `SetTransition` (o `PreviewProgram` compilado de cada passagem). As **transições são planejadas primeiro** (cada uma decide onde a faixa que sai entrega e onde a que entra começa); os decks alternam (`i % 2`), para que enquanto um toca o próximo já esteja carregado e no cue. Tudo pré-calculado: o executor **nunca planeja com áudio tocando** (§21.3 / RNF-002). Faixa ausente **trunca o set e avisa** — pular reordenaria a narrativa em silêncio.
- [x] **`DeckAdapter`** (`preview/deck_adapter.{h,cpp}`): todos os controles de um deck atrás de um objeto. Necessário porque o set **rotaciona decks** (o que entrou vira o que sai) — o struct do preview era amarrado a source/target fixos. Hoje é a **única** definição de "deck": os únicos `ControlProxy` fora dele são os crossfaders, que são do mixer.
- [x] **`SetExecutor`** (`preview/set_executor.{h,cpp}`): máquina de estados (§21.1) `Preparing → Playing → Transitioning → Playing → … → Completed`, mais `Paused`/`ManualOverride`/`Cancelled`/`Failed`. Toca até a janela de saída (`reachedExit`) → dispara a transição pré-compilada → o deck alvo vira o vivo → o liberado carrega a faixa seguinte. **Pause/resume/skip/cancel** + ManualOverride (crossfader). A entrega **espera o deck carregar** em vez de cortar para o silêncio (§21.4); cancelar/override **param a automação sem cortar o áudio** e devolvem os decks.
- [x] **Controller/UI**: `runSet()` **resolve todas as faixas antes de começar** — começar e falhar na 3ª deixaria os decks no ar; ele se recusa a rodar meio set. Painel: **Run set / Pause / Resume / Skip / Stop**, rótulo de estado ao vivo e (RF-011) a faixa que está sendo posta no deck livre. O set **sobrevive ao fechamento do diálogo**; só a prévia é cancelada.
- [x] **Testes**: decks alternam, 1 transição por par consecutivo com ids batendo, entrada/saída concordam com as transições que as produziram, faixa única sem entrega, arranjo vazio, faixa ausente trunca+avisa, estimativa soma os spans, e `reachedExit` (pura/estática) incl. os casos sem dado — onde chutar uma entrega cortaria a faixa no meio. **ctest do módulo: 61/61 verdes.**

### Fase 7a1 — achados do teste real (ouvido > log)
O log dizia "5 faixas, 4 transições, tudo verde". O ouvido discordou, e estava certo nas duas vezes:
- [x] **Cortes secos no Portal**: as 4 transições saíram `Cut on phrase` e **soaram mal**. A regra era minha (`keyCompat < 0.4 → Cut`) e o §16 **nunca pede corte para tom incompatível** — pede **"troca por breakdown"**. Novo `TransitionType::BreakdownSwap`, escolhido quando tom/tempo brigam **e** a faixa que sai tem `Breakdown`/`Outro` nos últimos 40% para pousar; sem zona de pouso, o corte segue sendo a escolha honesta. Automação deliberadamente gradual (grave puxado cedo, filtro varrendo, faixa nova crescendo por baixo).
- [x] **`beatSync` saiu do tipo e foi para o plano**: derivá-lo do tipo estava errado — **só o planejador sabe se os tempos casam**. Um breakdown swap por choque de tom **quer** sync (tempos batem); o mesmo tipo por salto de tempo **não pode**.
- [x] **Batidas descasadas**: o handover automático funcionava e o tempo travava (+1.6%), mas as batidas entravam tortas. **Tempo e fase são problemas diferentes** e só o primeiro estava resolvido: a entrada é um milissegundo da análise e o seek caía entre batidas — sync iguala o BPM, não resgata pouso ruim. Adicionados `quantize` (antes do seek) e `beatsync_phase` (depois do play, só quando o plano pede sync). **Confirmado pelo usuário: "bem mais sincronizada".**
- [x] **Dívida quitada**: o `PreviewExecutor` duplicava a camada de controles. Isso **já havia custado**: a correção de fase foi só para o set, e a prévia ficou com o mesmo bug. Migrado para o `DeckAdapter` (−133/+71 linhas) — a prévia herdou `quantize`/`syncPhase` **sem uma linha de correção própria**.

### Fase 7a2 — ponto de entrada da faixa (Enter @)
Simétrico ao **Exit @**, a pedido do usuário (intro longa derrubava a energia na virada):
- [x] **`TransitionOverride::targetEntryMs`** — onde a faixa **que entra** começa. O automático já
  preferia o ponto de baixa energia (a intro), que é justamente o que mata o clímax; agora o DJ
  aponta um valor depois da intro. Clampeado ao comprimento da faixa (não deixa o seek passar do
  fim). **Migration v9** (`target_entry_ms` em `MusicSyncTransitionOverrides`). Fluxo já existente
  honra o valor (`plan.targetEntryMs → program.targetStartMs → item.startMs → seek`, no
  `beginTransition`).
- [x] **UI track-centric** (achado em teste real): a primeira versão era *transition-centric* — o
  `Enter @` da linha era onde a **próxima** faixa entrava, guardado no par de saída da linha. O
  usuário leu, com razão, como *"onde a faixa DESTA linha entra"*: ajustou o Enter @ da Innerbloom
  esperando que ela pulasse a intro, mas o valor foi parar no BONDI, e a Innerbloom começou do
  0:00. Corrigido: **cada linha é uma faixa** e o `Enter @` dela é o ponto de entrada dela
  (guardado no par de **entrada**, `i-1 → i`); `Exit @`/`Transition`/`Bars` continuam no par de
  **saída** (`i → i+1`). Editor grava nos dois pares via read-modify-write (um não apaga o outro).
  Abertura sem Enter @; fechamento sem Exit @/tipo/bars.
- [x] **Testes**: round-trip de exit+entry no repositório (independentes, sobrevivem ao reload),
  planner honra o override e clampeia entrada fora do fim. **ctest do módulo: 80/80 verdes.**

## Próximas fases (roadmap)
- **Fase 1 a 7** — ✅ concluídas (acima).
- **Fase 8** — Gravação e relatórios (WAV master via Mixxx, tracklist, session-report). **→ set de 35 faixas executável e gravável.**
- **Fase 9–10** — Worker Python opcional; Echo Out/loop-out; stems; LLM de intenção; render offline.

## Decisões deliberadas (não são lacunas)
- **Sem seleção por duração-alvo.** `generateSequences` usa **todas** as faixas analisadas; o
  `MixIntent::targetDurationMs` existe e **não é lido**. O §18 sugere três versões (90 min ≈ 17
  faixas, 2h ≈ 25, 2h30–3h ≈ tudo) e o §9 fala em ~6 âncoras longas por apresentação — mas
  **decisão do usuário (2026-07-13): o plano é orientação, não lei**. O set atual (37 faixas,
  ~1h41) é o desejado. O motor propõe e explica; quem corta faixa é o DJ. Não reabrir como bug.

## Limitações conhecidas
- Gravação é em tempo real (set de 90 min = 90 min); render offline fica para pós-MVP.
- Detecção de vocais/seções é heurística no MVP (worker Python melhora depois).
- Momentos de artista ao vivo (timing de mashup, "deixar a pista cantar") permanecem manuais via ManualOverride.
