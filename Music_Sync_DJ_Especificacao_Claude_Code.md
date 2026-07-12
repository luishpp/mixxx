# Music Sync DJ

## Especificação de produto e guia de implementação para Claude Code

**Projeto:** Music Sync DJ  
**Tipo:** Fork pessoal e local-first do Mixxx para criação assistida de DJ sets  
**Versão do documento:** 2.1 — fork conectado ao repositório `luishpp/mixxx`  
**Data de referência:** 12 de julho de 2026  
**Repositório principal:** `https://github.com/luishpp/mixxx`  
**Repositório upstream:** `https://github.com/mixxxdj/mixxx`  
**Baseline recomendada:** tag estável `2.5.6` do Mixxx  
**Plataforma inicial:** Windows 10/11 64 bits  
**Plataformas herdadas do Mixxx:** Windows, macOS e Linux  
**Idioma inicial da extensão:** Português do Brasil  
**Formato de entrada prioritário:** `.mp3`  
**Licença esperada do fork distribuído:** GPLv2, em conformidade com o Mixxx

---

# 1. Resumo da decisão arquitetural

O Music Sync DJ será desenvolvido como um **fork pessoal do Mixxx**, e não como uma aplicação independente em .NET, Vue, MAUI ou Electron.

O projeto deverá reutilizar do Mixxx:

- motor de áudio em tempo real;
- decks e mixer;
- decodificação de formatos;
- sincronização de BPM e fase;
- beat grids;
- detecção de tonalidade;
- waveforms;
- biblioteca musical;
- cues e regiões de intro/outro;
- efeitos, EQ e filtros;
- Auto DJ;
- gravação do master;
- controles internos;
- suporte futuro a MIDI/HID.

O Music Sync DJ adicionará:

- análise de energia e estrutura musical;
- detecção ou estimativa de frases;
- análise de risco vocal e conflito de graves;
- criação de uma curva narrativa de energia;
- ordenação inteligente e explicável das faixas;
- planejamento declarativo das transições;
- execução automática de transições sincronizadas;
- prévia e comparação de alternativas;
- persistência de projetos de set;
- relatório com timestamps, decisões e avisos;
- worker externo opcional para Music Information Retrieval e modelos de IA.

A prioridade não é reconstruir um software de DJ. A prioridade é **usar a base madura do Mixxx e implementar a camada de assistência inteligente**.

---

# 2. Instruções principais para o Claude Code

Este documento é a fonte de verdade inicial do projeto. O repositório de trabalho é `https://github.com/luishpp/mixxx`; o repositório `mixxxdj/mixxx` é apenas o upstream. Antes de alterar código:

1. Leia integralmente esta especificação.
2. Leia `README.md`, `CONTRIBUTING.md`, `CMakeLists.txt` e o guia de desenvolvimento do Mixxx.
3. Confirme que `git remote get-url origin` retorna `https://github.com/luishpp/mixxx.git`.
4. Confirme que `git remote get-url upstream` retorna `https://github.com/mixxxdj/mixxx.git`.
5. Identifique a tag ou commit exato usado como baseline e registre-o em `docs/music-sync/upstream-baseline.md`.
6. Não substitua o motor de áudio, a biblioteca, os decks, os waveforms ou o sistema de controles do Mixxx.
7. Não introduza .NET, MAUI, Vue, Electron, ASP.NET Core ou uma API HTTP local no MVP.
8. Implemente novas funções em módulos isolados sob o namespace `mixxx::music_sync` sempre que possível.
9. Minimize alterações invasivas em classes centrais do Mixxx.
10. Prefira composição, adaptadores, sinais Qt e interfaces pequenas a patches espalhados.
11. Nunca execute acesso a disco, banco, rede, Python, modelos de IA ou serialização pesada na thread de áudio.
12. Não faça alocação dinâmica, locks bloqueantes ou logging volumoso em callbacks de áudio.
13. Use os analisadores nativos do Mixxx como fonte primária para BPM, beats, tonalidade, waveform e ReplayGain.
14. Use um worker externo somente para análises que o Mixxx não fornece adequadamente.
15. Mantenha o sistema totalmente funcional sem Python, LLM, GPU ou internet.
16. A primeira implementação do planejador deve ser determinística e baseada em regras.
17. Cada sugestão de ordem ou transição deve apresentar uma explicação derivada da pontuação usada.
18. Não modifique nem sobrescreva os arquivos de áudio originais.
19. Não adicione músicas comerciais ao repositório.
20. Gere fixtures sintéticas para testes de BPM, beat grid, tonalidade, energia e clipping.
21. Preserve a capacidade de compilar e executar o Mixxx padrão quando o recurso Music Sync estiver desativado.
22. Coloque funcionalidades experimentais atrás de feature flags ou opções de CMake.
23. Atualize `docs/music-sync/implementation-status.md` após cada etapa relevante.
24. Registre decisões arquiteturais em `docs/music-sync/adr/`.
25. Não altere a base inteira para adequá-la ao projeto. O projeto deve se adequar aos padrões do Mixxx.
26. Não comece por uma timeline estilo DAW. Primeiro valide análise, sequência e execução automática.
27. Não implemente renderização offline acelerada no MVP. Use a reprodução e gravação em tempo real do Mixxx.
28. Execute os testes existentes do Mixxx antes e depois de alterações relevantes.
29. Mantenha commits pequenos, compiláveis e focados.
30. Não faça reformatação em massa de arquivos do upstream.
31. Ao encontrar ambiguidade, escolha a alternativa com menor impacto sobre o upstream e documente a decisão.
32. O primeiro objetivo funcional é gerar e executar um mini-set de cinco faixas com transições sincronizadas e gravar o resultado.
---

# 3. Contexto do projeto

O usuário é entusiasta de house, techno e trance, deseja criar seus próprios sets e fornecerá as faixas em arquivos `.mp3`.

O código-fonte e o histórico do projeto serão mantidos no fork `https://github.com/luishpp/mixxx`.

O projeto é pessoal. Não haverá, neste momento:

- SaaS;
- contas de usuários;
- cobrança;
- hospedagem em nuvem;
- API pública;
- sincronização entre dispositivos;
- distribuição comercial fechada;
- necessidade de proteger código proprietário.

Esse contexto torna o Mixxx uma fundação adequada porque o projeto pode aceitar:

- C++ e Qt como stack principal;
- arquitetura desktop;
- licença GPLv2;
- evolução por fork;
- uso local e offline;
- integração direta com o motor de áudio existente.

---

# 4. Visão do produto

O Music Sync DJ deverá permitir que uma pessoa sem experiência profissional como DJ:

1. importe ou use faixas já presentes na biblioteca do Mixxx;
2. analise as músicas;
3. defina a jornada desejada para o set;
4. receba sugestões de ordem;
5. entenda por que as faixas foram ordenadas daquela forma;
6. revise pontos de entrada e saída;
7. ouça prévias de transições;
8. execute o set automaticamente nos decks;
9. intervenha manualmente quando desejar;
10. grave o resultado final pelo próprio Mixxx.

A proposta não é retirar o controle artístico do usuário. A IA é uma assistente para análise, preparação, sequência e execução técnica.

---

# 5. Objetivos

## 5.1 Objetivo principal

Transformar o Auto DJ do Mixxx em um **DJ assistido por análise musical, narrativa de energia e planejamento de transições**.

## 5.2 Objetivos específicos

- Reutilizar BPM, beat grid, tonalidade, waveform e ReplayGain do Mixxx.
- Estimar energia ao longo de cada faixa.
- Identificar regiões de intro, breakdown, build, drop e outro.
- Estimar frases de 8, 16 e 32 compassos.
- Medir risco de conflito vocal e de graves.
- Criar uma matriz de compatibilidade entre faixas.
- Sugerir mais de uma sequência possível.
- Respeitar faixas obrigatórias, excluídas e posições bloqueadas.
- Criar transições alinhadas a beats e frases.
- Automatizar crossfader, ganho, EQ, filtro e sincronização.
- Permitir intervenção manual durante a execução.
- Gravar o master em WAV ou outro formato já suportado pelo Mixxx.
- Gerar tracklist com timestamps e relatório da sessão.

## 5.3 Não objetivos do MVP

O MVP não deverá incluir:

- uma nova aplicação desktop separada;
- interface MAUI, Vue, React ou Electron;
- renderização offline mais rápida que o tempo real;
- editor multitrack completo estilo DAW;
- performance em nuvem;
- streaming de catálogos protegidos;
- download de músicas;
- remoção de DRM;
- scratching automatizado;
- geração de músicas;
- clonagem de artistas ou vozes;
- separação obrigatória de stems;
- LLM obrigatório;
- aplicativo móvel;
- publicação automática em plataformas.

---

# 6. Baseline e estratégia de fork

## 6.1 Baseline inicial

Usar a tag estável `2.5.6` do Mixxx como ponto inicial.

Motivos:

- é uma release estável;
- reduz volatilidade durante o primeiro MVP;
- já utiliza a base moderna do Mixxx;
- permite validar o conceito antes de acompanhar branches em desenvolvimento;
- facilita comparar regressões com uma versão oficial conhecida.

## 6.2 Repositório principal e remotes Git

O repositório oficial de trabalho do Music Sync DJ é:

```text
https://github.com/luishpp/mixxx
```

Esse repositório é o **repository of record** do projeto pessoal. Todo código, branch, commit, tag, issue e documentação específica do Music Sync DJ deverá ser criado nele. O repositório oficial do Mixxx será usado somente como fonte upstream.

Configuração obrigatória dos remotes:

```text
origin    -> https://github.com/luishpp/mixxx.git
upstream  -> https://github.com/mixxxdj/mixxx.git
```

Clone inicial recomendado:

```bash
git clone https://github.com/luishpp/mixxx.git music-sync-dj
cd music-sync-dj

git remote add upstream https://github.com/mixxxdj/mixxx.git
git fetch --all --tags --prune

git remote -v
```

O resultado de `git remote -v` deverá mostrar `luishpp/mixxx` como `origin` e `mixxxdj/mixxx` como `upstream`.

Criação da branch de baseline do Music Sync DJ:

```bash
git switch --create music-sync/2.5.6 tags/2.5.6
git push --set-upstream origin music-sync/2.5.6
```

Se o diretório local já tiver sido clonado de outra origem, corrigir os remotes antes de desenvolver:

```bash
git remote set-url origin https://github.com/luishpp/mixxx.git

git remote get-url upstream >/dev/null 2>&1 \
  && git remote set-url upstream https://github.com/mixxxdj/mixxx.git \
  || git remote add upstream https://github.com/mixxxdj/mixxx.git

git fetch --all --tags --prune
```

Regras:

- nunca enviar commits do Music Sync DJ para `mixxxdj/mixxx`;
- não desenvolver diretamente na branch `main` do fork;
- usar `music-sync/2.5.6` como branch de integração do MVP;
- criar branches curtas a partir dela para cada entrega;
- publicar todas as branches de trabalho em `origin`;
- abrir pull requests contra `luishpp/mixxx`, nunca contra o upstream, salvo decisão explícita do proprietário;
- antes de qualquer `push`, validar `git remote get-url origin`;
- não usar `--force` na branch `main` nem em `music-sync/2.5.6`.

Convenção recomendada de branches:

```text
music-sync/2.5.6
feature/music-sync-analysis
feature/music-sync-set-planner
feature/music-sync-transition-planner
feature/music-sync-auto-executor
feature/music-sync-project-persistence
fix/music-sync-<descricao-curta>
docs/music-sync-<descricao-curta>
```

## 6.3 Política de atualização do upstream

Durante o MVP:

- não acompanhar `main` continuamente;
- manter a baseline fixa;
- aplicar apenas correções upstream claramente necessárias;
- registrar cada cherry-pick em `docs/music-sync/upstream-patches.md`;
- reavaliar migração para nova versão estável somente após o MVP.

Após o MVP:

1. criar branch de migração;
2. executar toda a suíte de testes;
3. comparar APIs alteradas;
4. migrar módulos por etapas;
5. não misturar migração de upstream com novas funcionalidades.

## 6.4 Distribuição e licença

O Mixxx é distribuído sob GPLv2. O Music Sync DJ deverá:

- manter avisos de copyright e licença;
- disponibilizar o código-fonte correspondente caso o fork seja distribuído;
- documentar dependências adicionais e suas licenças;
- evitar bibliotecas incompatíveis com GPLv2;
- não remover créditos do Mixxx;
- identificar claramente o projeto como um fork não oficial.

Como o projeto é pessoal, a licença não impede o desenvolvimento pretendido.

---

# 7. Arquitetura de alto nível

```text
┌──────────────────────────────────────────────────────────┐
│                    Interface do Mixxx                    │
│  Biblioteca | Decks | Waveforms | Auto DJ | Music Sync  │
└──────────────────────────┬───────────────────────────────┘
                           │
┌──────────────────────────▼───────────────────────────────┐
│              Music Sync Application Layer               │
│  Projetos | Intenção | Sequência | Transições | Sessão  │
└──────────────┬───────────────────────┬───────────────────┘
               │                       │
┌──────────────▼─────────────┐  ┌──────▼──────────────────┐
│ Native Analysis Adapter    │  │ Optional AI Worker     │
│ BPM, key, beats, waveform, │  │ energy, phrases, vocal │
│ ReplayGain, intro/outro    │  │ density, sections      │
└──────────────┬─────────────┘  └──────┬──────────────────┘
               │                       │ JSONL/QProcess
┌──────────────▼───────────────────────▼───────────────────┐
│                  Music Sync Planner                     │
│ Pair scoring | energy curve | optimizer | explanations  │
└──────────────────────────┬───────────────────────────────┘
                           │ TransitionPlan
┌──────────────────────────▼───────────────────────────────┐
│                  Set Execution Engine                   │
│ Deck loading | sync | cues | EQ | crossfader | effects  │
└──────────────────────────┬───────────────────────────────┘
                           │ Mixxx Controls
┌──────────────────────────▼───────────────────────────────┐
│                    Mixxx Audio Engine                   │
│ Decks | Mixer | Effects | SoundManager | Recording      │
└──────────────────────────────────────────────────────────┘
```

---

# 8. Componentes do Mixxx a reutilizar

## 8.1 Biblioteca

Usar a biblioteca existente como fonte para:

- caminho da faixa;
- título, artista, álbum e gênero;
- duração;
- BPM;
- tonalidade;
- cues;
- intro/outro;
- ReplayGain;
- estado de análise;
- playlists e crates.

Não criar uma segunda biblioteca musical completa.

## 8.2 Analisadores nativos

Reutilizar os analisadores do Mixxx para:

- beat detection;
- beat grid;
- BPM;
- key detection;
- waveform summary;
- ReplayGain;
- silêncio inicial e final;
- dados de intro/outro disponíveis.

Adicionar novos analisadores somente para informações ausentes:

- energia temporal;
- seções;
- frases;
- densidade vocal;
- presença de graves;
- qualidade de janelas de transição.

## 8.3 Engine e decks

Reutilizar:

- reprodução;
- alteração de tempo;
- preservação de pitch;
- sync;
- posição de reprodução;
- loops;
- cue points;
- channel gains;
- EQ;
- filtros;
- efeitos;
- crossfader;
- master output.

## 8.4 Mixxx Controls

O planejador e executor deverão interagir com o motor por controles internos sempre que possível.

Exemplos conceituais:

```text
[Channel1],play
[Channel1],rate
[Channel1],sync_enabled
[Channel1],volume
[Channel1],filterLow
[Channel1],filterMid
[Channel1],filterHigh
[Master],crossfader
[Recording],toggle_recording
```

Os nomes reais devem ser confirmados na versão baseline antes da implementação.

## 8.5 Auto DJ

Reutilizar do Auto DJ:

- fila;
- carregamento automático de decks;
- mudança entre faixas;
- intro/outro cues;
- gerenciamento básico de execução;
- crossfade existente como fallback.

Não assumir que o Auto DJ atual faz beatmatching, phrase matching ou narrativa de energia. Esses recursos pertencem ao Music Sync DJ.

## 8.6 Gravação

No MVP, usar a gravação do master já existente no Mixxx.

Consequência:

- um set de 90 minutos levará aproximadamente 90 minutos para ser gravado;
- a execução poderá ser acompanhada e interrompida;
- a saída refletirá exatamente o motor em tempo real;
- renderização acelerada ficará para uma fase posterior.

---

# 9. Princípios de integração

## 9.1 Isolamento

Todo código novo deverá ficar preferencialmente em:

```text
src/music_sync/
```

Namespace:

```cpp
namespace mixxx::music_sync {
}
```

Recursos estáticos:

```text
res/music_sync/
```

Documentação:

```text
docs/music-sync/
```

Worker externo:

```text
music-sync-ai/
```

## 9.2 Modificações no core

Alterações no core do Mixxx devem ser limitadas a:

- registrar o módulo;
- expor sinais ou interfaces realmente necessários;
- criar pontos de extensão pequenos;
- integrar a tela ou painel;
- adicionar controles internos;
- fornecer acesso seguro a dados já existentes.

Cada mudança em arquivo upstream deve conter uma justificativa em ADR quando não for trivial.

## 9.3 Thread de áudio

Na thread de áudio é proibido:

- executar Python;
- chamar processo externo;
- acessar SQLite;
- ler arquivo do projeto;
- serializar JSON;
- executar LLM;
- adquirir mutex bloqueante;
- esperar future/promise;
- fazer alocação grande;
- escrever logs frequentes;
- calcular otimização da sequência.

A thread de áudio deverá apenas consumir estado previamente calculado e estruturas apropriadas para tempo real.

## 9.4 Comunicação entre threads

Usar os padrões já existentes no Mixxx.

Quando necessário:

- sinais e slots Qt com conexão apropriada;
- atomics para valores simples;
- buffers ou filas lock-free existentes;
- snapshots imutáveis de planos;
- troca de estado fora do callback de áudio.

Não introduzir um mecanismo paralelo sem primeiro verificar as abstrações existentes.

---

# 10. Estrutura sugerida do repositório

```text
music-sync-dj/
├─ CMakeLists.txt                       # upstream, alteração mínima
├─ README.md
├─ CONTRIBUTING.md
├─ CLAUDE.md                            # regras específicas do fork
├─ Music_Sync_DJ_Especificacao_Claude_Code.md
├─ src/
│  ├─ ...                               # código original do Mixxx
│  └─ music_sync/
│     ├─ music_sync_controller.h/.cpp
│     ├─ feature_flags.h
│     ├─ analysis/
│     │  ├─ native_analysis_adapter.h/.cpp
│     │  ├─ analysis_repository.h/.cpp
│     │  ├─ analysis_models.h
│     │  ├─ energy_analyzer.h/.cpp
│     │  ├─ phrase_analyzer.h/.cpp
│     │  ├─ section_analyzer.h/.cpp
│     │  └─ worker_client.h/.cpp
│     ├─ domain/
│     │  ├─ mix_project.h
│     │  ├─ mix_intent.h
│     │  ├─ track_features.h
│     │  ├─ arrangement.h
│     │  └─ transition_plan.h
│     ├─ planner/
│     │  ├─ harmonic_compatibility.h/.cpp
│     │  ├─ tempo_compatibility.h/.cpp
│     │  ├─ pair_scorer.h/.cpp
│     │  ├─ sequence_optimizer.h/.cpp
│     │  ├─ transition_planner.h/.cpp
│     │  └─ explanation_builder.h/.cpp
│     ├─ execution/
│     │  ├─ set_executor.h/.cpp
│     │  ├─ execution_state_machine.h/.cpp
│     │  ├─ deck_adapter.h/.cpp
│     │  ├─ automation_scheduler.h/.cpp
│     │  └─ transition_runtime.h/.cpp
│     ├─ persistence/
│     │  ├─ project_repository.h/.cpp
│     │  ├─ sidecar_database.h/.cpp
│     │  ├─ migrations/
│     │  └─ json_contracts.h/.cpp
│     ├─ controls/
│     │  ├─ music_sync_controls.h/.cpp
│     │  └─ control_keys.h
│     ├─ ui/
│     │  ├─ dlg_music_sync.h/.cpp
│     │  ├─ arrangement_model.h/.cpp
│     │  ├─ transition_model.h/.cpp
│     │  ├─ project_model.h/.cpp
│     │  └─ widgets/
│     └─ tests/
├─ res/
│  ├─ ...
│  └─ music_sync/
│     ├─ presets/
│     ├─ schemas/
│     └─ icons/
├─ music-sync-ai/
│  ├─ pyproject.toml
│  ├─ src/music_sync_ai/
│  │  ├─ cli.py
│  │  ├─ contracts.py
│  │  ├─ energy.py
│  │  ├─ phrases.py
│  │  ├─ sections.py
│  │  └─ vocals.py
│  └─ tests/
├─ docs/
│  └─ music-sync/
│     ├─ architecture.md
│     ├─ implementation-status.md
│     ├─ upstream-baseline.md
│     ├─ upstream-patches.md
│     ├─ testing.md
│     ├─ audio-evaluation.md
│     └─ adr/
└─ tools/
   └─ music_sync/
      ├─ generate_audio_fixtures.py
      └─ verify_worker.py
```

A estrutura deverá ser adaptada aos padrões reais do repositório após inspeção. Não criar diretórios redundantes se o Mixxx já possuir local adequado.

---

# 11. Persistência

## 11.1 Fonte de dados principal

A biblioteca do Mixxx continuará sendo a fonte de verdade para dados nativos das faixas.

## 11.2 Banco sidecar do Music Sync

Usar um SQLite separado para dados exclusivos do Music Sync:

```text
<diretório de configuração do Mixxx>/music-sync-dj.sqlite
```

Motivos:

- reduzir alterações no schema principal do Mixxx;
- diminuir conflitos ao atualizar o upstream;
- permitir descartar o módulo sem danificar a biblioteca;
- versionar as próprias migrations;
- armazenar curvas e projetos sem sobrecarregar tabelas nativas.

## 11.3 Chave de correlação da faixa

Cada registro deverá usar uma combinação estável:

- ID interno da faixa no Mixxx, quando disponível;
- localização normalizada;
- hash parcial ou completo do arquivo;
- tamanho;
- data de modificação.

Não depender apenas do caminho, pois arquivos podem ser movidos.

## 11.4 Tabelas iniciais

```text
MusicSyncSchemaMigrations
MusicSyncTrackFeatures
MusicSyncTrackSections
MusicSyncTransitionWindows
MusicSyncProjects
MusicSyncProjectTracks
MusicSyncArrangements
MusicSyncArrangementItems
MusicSyncTransitionPlans
MusicSyncExecutions
MusicSyncExecutionEvents
MusicSyncSettings
```

## 11.5 Regras

- não armazenar áudio no banco;
- usar UTC internamente;
- cachear por hash e versão do analisador;
- preservar correções manuais;
- invalidar somente dados automáticos quando o analisador mudar;
- usar transações em alterações de projeto;
- realizar migrations antes de abrir o módulo;
- falha no sidecar não deve impedir o Mixxx de iniciar.

---

# 12. Modelo de domínio

## 12.1 TrackFeatures

```text
TrackFeatures
- MixxxTrackId
- FileFingerprint
- AnalyzerVersion
- Bpm
- BpmConfidence
- Key
- CamelotCode
- KeyConfidence
- OverallEnergy
- EnergyCurve[]
- VocalDensityCurve[]
- BassPresenceCurve[]
- PhraseMarkers[]
- Sections[]
- EntryWindows[]
- ExitWindows[]
- Warnings[]
- ManualOverrides
```

BPM, key e beats devem preferir os valores do Mixxx. O sidecar deverá armazenar apenas snapshot, confiança adicional ou override quando necessário.

## 12.2 MixProject

```text
MixProject
- Id
- Name
- Description
- TargetDurationMs
- CandidateTrackIds[]
- RequiredTrackIds[]
- ExcludedTrackIds[]
- LockedPositions[]
- Intent
- EnergyCurve
- ScoringProfile
- TransitionProfile
- SelectedArrangementId
- CreatedAt
- UpdatedAt
```

## 12.3 MixIntent

```text
MixIntent
- EnergyPreset
- CustomEnergyPoints[]
- GenreJourney[]
- StartBpm
- EndBpm
- MaxTempoChangePercent
- PeakPosition
- PreferredTransitionBars
- TransitionStyle
- AvoidVocalOverlap
- PreserveImportantBreakdowns
- ClosingMood
```

## 12.4 Arrangement

```text
Arrangement
- Id
- ProjectId
- Items[]
- TotalDurationMs
- TotalScore
- EnergyFitScore
- Warnings[]
- Explanation
- AlgorithmVersion
```

## 12.5 ArrangementItem

```text
ArrangementItem
- TrackId
- Position
- Locked
- PlannedSourceInMs
- PlannedSourceOutMs
- EffectiveBpm
- PairScoreFromPrevious
- TransitionToNext
```

## 12.6 TransitionPlan

```text
TransitionPlan
- Id
- SourceTrackId
- TargetTrackId
- Type
- SourceExitBeat
- TargetEntryBeat
- DurationBeats
- DurationBars
- TargetBpm
- SourceRateRatio
- TargetRateRatio
- CrossfaderCurve
- GainAutomation[]
- EqAutomation[]
- FilterAutomation[]
- EffectAutomation[]
- Score
- Confidence
- Explanation
- Warnings[]
```

## 12.7 SetExecution

```text
SetExecution
- Id
- ProjectId
- ArrangementId
- State
- CurrentItemIndex
- StartedAt
- FinishedAt
- RecordingEnabled
- RecordingPath
- Events[]
- Error
```

---

# 13. Requisitos funcionais

## RF-001 — Usar biblioteca existente

O Music Sync deverá listar e filtrar faixas já presentes na biblioteca do Mixxx.

Deverá permitir:

- selecionar uma playlist;
- selecionar uma crate;
- selecionar faixas manualmente;
- filtrar por artista, gênero, BPM, key e duração;
- excluir faixas sem removê-las da biblioteca;
- solicitar análise nativa quando faltarem BPM, beat grid ou key.

## RF-002 — Análise nativa

Para cada faixa, obter do Mixxx quando disponível:

- duração;
- BPM;
- beat grid;
- tonalidade;
- ReplayGain;
- waveform;
- cues;
- intro/outro;
- silêncio inicial/final;
- status de análise.

O módulo deve sinalizar valores ausentes ou de baixa confiança.

## RF-003 — Análise avançada

Calcular ou importar:

- energia geral;
- curva de energia;
- densidade vocal;
- presença de graves;
- possíveis seções;
- marcações de frase;
- janelas de entrada e saída;
- confiança de cada característica.

O sistema deverá funcionar com uma implementação simples em C++ ou heurísticas quando o worker Python estiver desabilitado.

## RF-004 — Correções manuais

Permitir correção de:

- BPM;
- primeiro beat;
- beat grid;
- tonalidade;
- intro/outro;
- energia geral;
- seções;
- frases;
- janelas de transição.

Correções manuais terão prioridade sobre análises automáticas.

## RF-005 — Projeto de set

Permitir criar, salvar, duplicar, renomear e excluir projetos.

Um projeto deverá registrar:

- faixas candidatas;
- faixas obrigatórias;
- faixas excluídas;
- duração alvo;
- curva de energia;
- BPM desejado;
- tolerância de tempo;
- estilo de transição;
- regras de vocais;
- posições bloqueadas;
- sequência selecionada.

## RF-006 — Curva de energia

Presets iniciais:

- ascendente;
- pico central;
- pico final;
- ondas;
- constante;
- jornada personalizada.

Formato:

```json
[
  { "position": 0.00, "energy": 0.25 },
  { "position": 0.35, "energy": 0.50 },
  { "position": 0.70, "energy": 0.78 },
  { "position": 0.90, "energy": 1.00 },
  { "position": 1.00, "energy": 0.65 }
]
```

## RF-007 — Ordenação automática

O sistema deverá gerar pelo menos três alternativas de sequência considerando:

- compatibilidade harmônica;
- diferença de BPM;
- qualidade da janela de saída;
- qualidade da janela de entrada;
- compatibilidade de frases;
- aderência à curva de energia;
- risco vocal;
- risco de graves;
- continuidade de gênero;
- duração alvo;
- faixas bloqueadas;
- repetição excessiva de artista;
- confiança da análise.

## RF-008 — Explicações

Exemplo:

> Transição forte: diferença de 1,6 BPM, movimento Camelot 8A → 9A, energia crescente, intro de 32 compassos e baixo risco vocal. Atenção: a tonalidade da faixa de destino tem confiança moderada.

A explicação deverá ser produzida pelos mesmos componentes usados na pontuação.

## RF-009 — Planejamento de transições

Tipos iniciais:

1. Crossfade simples.
2. EQ Blend.
3. Bass Swap.
4. Filter Transition.
5. Cut em frase.
6. Fallback do Auto DJ.

Tipos posteriores:

- Echo Out;
- loop de saída;
- transição usando stems;
- transição de breakdown para intro;
- transição com efeito sincronizado.

## RF-010 — Prévia

O usuário deverá poder:

- carregar o par de faixas nos decks;
- posicionar nos pontos planejados;
- executar somente a transição;
- repetir a prévia;
- escolher entre alternativas;
- alterar duração e tipo;
- retornar à reprodução normal.

O MVP pode usar os decks reais para a prévia. Não é obrigatório criar um renderer separado.

## RF-011 — Fila inteligente

A sequência aprovada deverá ser convertida para uma fila executável.

Regras:

- não destruir a fila atual sem confirmação;
- permitir salvar uma cópia;
- indicar qual faixa será carregada em cada deck;
- mostrar a próxima transição;
- permitir pausar o executor;
- permitir retomar;
- permitir pular uma faixa;
- permitir abortar e devolver controle manual ao usuário.

## RF-012 — Execução automática

O executor deverá:

- carregar a faixa correta;
- aplicar cue de entrada;
- definir tempo alvo;
- ativar sync quando apropriado;
- iniciar no beat e frase planejados;
- executar automações;
- concluir a troca de deck;
- preparar a próxima faixa;
- registrar eventos e desvios.

## RF-013 — Intervenção manual

Durante a execução:

- qualquer ação manual importante deve suspender automação conflitante;
- o usuário poderá assumir os decks;
- o executor deverá indicar estado `ManualOverride`;
- a retomada exigirá reconciliação do estado atual;
- o sistema nunca deverá lutar contra o usuário pelo mesmo controle.

## RF-014 — Gravação

O sistema deverá oferecer:

- iniciar gravação antes do set;
- confirmar pasta e formato;
- registrar o caminho final;
- parar a gravação ao concluir;
- preservar gravação parcial em caso de interrupção;
- gerar relatório da sessão.

## RF-015 — Tracklist e relatório

Gerar:

- ordem das faixas;
- artista e título;
- timestamp aproximado de entrada;
- timestamp da transição;
- tipo de transição;
- BPM efetivo;
- alertas;
- versão do Music Sync;
- baseline do Mixxx;
- hash ou fingerprint dos arquivos;
- caminho da gravação.

Formatos:

```text
tracklist.txt
session-report.json
project.json
```

## RF-016 — Recuperação

O projeto deverá sobreviver a:

- fechamento inesperado;
- worker Python indisponível;
- faixa movida;
- falha de análise;
- falha ao carregar deck;
- dispositivo de áudio indisponível;
- gravação não iniciada;
- automação cancelada.

## RF-017 — Feature flags

Prever flags para:

```text
MUSIC_SYNC_ENABLED
MUSIC_SYNC_AI_WORKER
MUSIC_SYNC_ADVANCED_ANALYSIS
MUSIC_SYNC_EXPERIMENTAL_TRANSITIONS
MUSIC_SYNC_STEMS
MUSIC_SYNC_OFFLINE_RENDER
```

O nome e mecanismo final devem seguir os padrões do CMake e configuração do Mixxx.

---

# 14. Requisitos não funcionais

## RNF-001 — Operação local

- sem internet obrigatória;
- sem upload de áudio;
- sem telemetria específica do projeto por padrão;
- sem API externa obrigatória;
- sem autenticação.

## RNF-002 — Segurança em tempo real

- nenhuma chamada bloqueante na thread de áudio;
- automações pré-calculadas;
- dados de runtime em estruturas leves;
- nenhuma inferência de IA durante transição;
- falha do planejador não pode causar dropout.

## RNF-003 — Compatibilidade

Prioridade:

1. Windows 11 64 bits;
2. Windows 10 64 bits suportado pela baseline;
3. Linux;
4. macOS.

A extensão não deve adicionar dependência exclusiva de Windows ao core.

## RNF-004 — Reprodutibilidade

Registrar:

- tag e commit do Mixxx;
- versão do Music Sync;
- versão do analisador;
- versão do worker;
- algoritmo de sequência;
- pesos;
- seed, quando aplicável;
- overrides manuais;
- fingerprint das faixas.

## RNF-005 — Desempenho

Metas iniciais:

- abrir painel Music Sync sem degradar a inicialização normal;
- ordenar 100 faixas em menos de 5 segundos após análise em hardware comum;
- atualizar scores após mover uma faixa em menos de 1 segundo;
- não causar dropout de áudio durante análise em background;
- manter UI responsiva durante worker e otimização;
- carregar projeto de 100 faixas em até 3 segundos.

## RNF-006 — Tolerância a falhas

- worker externo opcional;
- timeout e cancelamento;
- processo externo não pode derrubar o Mixxx;
- JSON inválido deve ser rejeitado com mensagem clara;
- sidecar indisponível deve desabilitar somente o Music Sync;
- plano inválido nunca deve ser enviado ao executor.

## RNF-007 — Qualidade de código

- C++20 conforme baseline;
- Qt conforme baseline;
- CMake existente;
- padrões de estilo do Mixxx;
- testes no framework existente;
- sem duplicar abstrações já presentes;
- sem caminhos absolutos;
- sem dependências globais desnecessárias.

## RNF-008 — Explicabilidade

Toda pontuação deverá ser decomponível em fatores.

Exemplo:

```json
{
  "total": 0.84,
  "harmonic": 0.95,
  "tempo": 0.91,
  "energy": 0.78,
  "phrase": 0.82,
  "vocalSafety": 0.72,
  "windowQuality": 0.88,
  "penalties": [
    { "type": "moderate_key_confidence", "value": 0.03 }
  ]
}
```

---

# 15. Pipeline de análise

## 15.1 Etapa A — Garantir análise nativa

Para cada faixa selecionada:

1. verificar se BPM existe;
2. verificar beat grid;
3. verificar key;
4. verificar waveform;
5. verificar ReplayGain;
6. solicitar análise do Mixxx quando necessário;
7. aguardar conclusão fora da thread de áudio;
8. coletar snapshot dos dados.

## 15.2 Etapa B — Energia

Construir curva com janelas temporais combinando:

- loudness local;
- RMS;
- spectral centroid;
- spectral flux;
- densidade de onsets;
- presença de graves;
- contraste com regiões vizinhas.

Evitar tratar volume como sinônimo de energia.

## 15.3 Etapa C — Frases

Implementação inicial heurística:

- usar beat grid do Mixxx;
- agrupar em compassos de quatro tempos;
- testar offsets de downbeat;
- identificar mudanças recorrentes em 8, 16 e 32 compassos;
- favorecer variações de energia e onset no primeiro compasso;
- fornecer confiança;
- permitir correção manual.

## 15.4 Etapa D — Seções

Tipos:

```text
Intro
Groove
Verse
Build
Drop
Breakdown
Outro
Unknown
```

A primeira versão poderá usar regras. Não bloquear o MVP por segmentação perfeita.

## 15.5 Etapa E — Vocais

Níveis possíveis:

```text
0.0 = instrumental
1.0 = vocal dominante
```

O MVP poderá usar:

- estimativa heurística;
- marcação manual;
- worker opcional.

Separação de stems não é requisito.

## 15.6 Etapa F — Graves

Calcular presença relativa de graves por região para evitar duas linhas de baixo dominantes durante o blend.

## 15.7 Etapa G — Janelas de transição

Cada janela deverá ter:

```text
- StartBeat
- EndBeat
- Bars
- PhraseAligned
- Energy
- EnergyStability
- VocalRisk
- BassRisk
- InstrumentalScore
- Confidence
```

---

# 16. Worker externo opcional

## 16.1 Responsabilidade

O worker Python poderá executar:

- energia avançada;
- estimativa de frases;
- segmentação estrutural;
- densidade vocal;
- modelos ONNX;
- stems experimentais.

Não deverá executar:

- reprodução;
- mixagem em tempo real;
- controle de decks;
- gravação;
- alterações diretas no banco do Mixxx.

## 16.2 Processo

Usar `QProcess` ou abstração já existente.

Comunicação:

- request JSON por arquivo ou stdin;
- eventos JSON Lines no stdout;
- logs no stderr;
- timeout;
- cancelamento;
- versão de schema.

## 16.3 Contrato de request

```json
{
  "schemaVersion": "1.0",
  "jobId": "job-123",
  "track": {
    "path": "C:/Music/track.mp3",
    "fingerprint": "sha256:..."
  },
  "native": {
    "bpm": 124.2,
    "key": "A minor",
    "beatTimesMs": [512, 995, 1478]
  },
  "options": {
    "energy": true,
    "phrases": true,
    "sections": true,
    "vocals": false
  }
}
```

## 16.4 Contrato de resultado

```json
{
  "schemaVersion": "1.0",
  "workerVersion": "0.1.0",
  "jobId": "job-123",
  "energy": {
    "overall": 0.72,
    "curve": [
      { "timeMs": 0, "value": 0.18 },
      { "timeMs": 30000, "value": 0.42 }
    ]
  },
  "phrases": [
    { "startBeat": 1, "bars": 32, "confidence": 0.74 }
  ],
  "sections": [
    {
      "type": "Intro",
      "startMs": 0,
      "endMs": 62000,
      "energy": 0.31,
      "vocalDensity": 0.05,
      "confidence": 0.78
    }
  ],
  "warnings": []
}
```

## 16.5 Eventos JSONL

```json
{"type":"progress","jobId":"job-123","stage":"decode","percent":10}
{"type":"progress","jobId":"job-123","stage":"energy","percent":45}
{"type":"progress","jobId":"job-123","stage":"sections","percent":80}
{"type":"result","jobId":"job-123","resultFile":"C:/Temp/job-123.json"}
```

## 16.6 Fallback

Quando o worker não estiver disponível:

- usar dados nativos;
- aplicar energia simples;
- assumir risco vocal desconhecido;
- usar intro/outro existentes;
- reduzir confiança;
- manter o planejador funcional.

---

# 17. Compatibilidade harmônica

Usar tonalidade nativa do Mixxx e converter para Camelot em um componente testado.

Exemplos:

```text
A minor -> 8A
C major -> 8B
E minor -> 9A
G major -> 9B
```

Compatibilidade inicial:

- mesmo código: excelente;
- número anterior ou seguinte, mesma letra: excelente;
- mesmo número, A/B: boa;
- outros movimentos: configuráveis;
- incompatibilidade não bloqueia a transição;
- confiança baixa reduz a influência da tonalidade.

Não gravar Camelot sobre a tonalidade original do Mixxx. Manter como representação derivada.

---

# 18. Motor de ordenação

## 18.1 Estratégia

1. Filtrar faixas inválidas e excluídas.
2. Reservar posições bloqueadas.
3. Calcular matriz de compatibilidade.
4. Gerar rota inicial por busca gulosa com look-ahead.
5. Melhorar por inserção, realocação, swap e `2-opt`.
6. Ajustar duração e curva de energia.
7. Produzir pelo menos três alternativas.
8. Gerar explicações e avisos.

## 18.2 Pontuação de par

```text
PairScore =
    0.24 * HarmonicCompatibility
  + 0.20 * TempoCompatibility
  + 0.17 * PhraseCompatibility
  + 0.17 * EnergyFit
  + 0.10 * VocalSafety
  + 0.07 * TransitionWindowQuality
  + 0.05 * StyleContinuity
  - Penalties
```

Pesos configuráveis e versionados.

## 18.3 Penalidades

- alteração de tempo acima da tolerância;
- key de baixa confiança;
- beat grid inconsistente;
- choque vocal;
- sobreposição de graves;
- janela curta;
- salto de energia inadequado;
- duração distante do alvo;
- repetição excessiva de artista;
- mesma estratégia de transição repetida em excesso;
- faixa não analisada.

## 18.4 Presets de tempo

```text
Conservador: ±3%
Equilibrado: ±5%
Flexível: ±8%
```

Acima do limite, gerar aviso forte ou evitar o par.

## 18.5 Determinismo

Com os mesmos dados, pesos e seed, o resultado deverá ser repetível.

---

# 19. Planejamento de transições

## 19.1 Regras gerais

- alinhar frase com frase;
- preferir 16 ou 32 compassos para house, techno e trance;
- usar 8 compassos somente quando musicalmente justificável;
- evitar vocais dominantes simultâneos;
- evitar graves fortes simultâneos;
- preservar drops e breakdowns importantes;
- evitar cortar o clímax de uma faixa obrigatória;
- preparar automações antes da execução;
- manter headroom.

## 19.2 Crossfade simples

Usar como:

- fallback;
- transição de baixa confiança;
- faixas com intro/outro limpos;
- modo seguro.

## 19.3 EQ Blend

Estratégia:

1. iniciar faixa B com graves reduzidos;
2. misturar médios e agudos progressivamente;
3. reduzir graves de A próximo ao ponto de troca;
4. realizar bass swap no início de frase;
5. concluir crossfader e ganho.

## 19.4 Bass Swap

Usar quando:

- as duas faixas possuem kick estável;
- a frase está bem identificada;
- a entrada de B é clara;
- o risco vocal é baixo.

## 19.5 Filter Transition

Usar com moderação. O filtro não deve mascarar uma transição mal planejada.

## 19.6 Cut em frase

Usar quando:

- sobreposição causaria conflito;
- A termina com impacto;
- B inicia com ataque claro;
- há alinhamento de frase;
- o corte intencional preserva energia.

## 19.7 Fallback Auto DJ

Quando confiança for insuficiente:

- usar intro/outro do Mixxx;
- usar modo Auto DJ apropriado;
- apresentar aviso;
- evitar automações avançadas.

---

# 20. Plano declarativo de automação

O planejador não deve manipular controles diretamente. Ele deve produzir um plano.

Exemplo:

```json
{
  "sourceDeck": 1,
  "targetDeck": 2,
  "sourceExitBeat": 641,
  "targetEntryBeat": 1,
  "durationBeats": 128,
  "targetBpm": 126.0,
  "events": [
    {
      "atBeat": 0,
      "actions": {
        "targetPlay": true,
        "targetLowEq": 0.0,
        "targetVolume": 0.0
      }
    },
    {
      "fromBeat": 0,
      "toBeat": 96,
      "automation": {
        "targetVolume": [0.0, 1.0],
        "crossfader": [-1.0, 0.0]
      }
    },
    {
      "atBeat": 96,
      "actions": {
        "sourceLowEq": 0.0,
        "targetLowEq": 1.0
      }
    },
    {
      "fromBeat": 96,
      "toBeat": 128,
      "automation": {
        "sourceVolume": [1.0, 0.0],
        "crossfader": [0.0, 1.0]
      }
    }
  ]
}
```

O formato interno poderá ser C++, mas deverá ser serializável para teste e relatório.

---

# 21. Executor de set

## 21.1 Máquina de estados

```text
Idle
Preparing
LoadingSource
LoadingTarget
Cueing
WaitingForStart
Transitioning
HandingOff
PreparingNext
Paused
ManualOverride
Completed
Cancelled
Failed
```

## 21.2 Responsabilidades

- validar plano;
- determinar decks;
- carregar faixas;
- aguardar carregamento;
- aplicar cue e rate;
- preparar EQ e ganho;
- iniciar faixa no momento correto;
- executar automações;
- verificar desvio de beat;
- concluir troca;
- preparar próximo item;
- registrar eventos.

## 21.3 Clock e agendamento

A automação deverá ser baseada em:

- posição da faixa;
- beat grid;
- beat atual;
- fase;
- estado do deck.

Evitar timers de parede como única referência. `QTimer` poderá coordenar a UI, mas eventos musicais devem ser guiados pelo estado de reprodução.

## 21.4 Desvio e recuperação

Se a execução se desviar:

- pequeno desvio: corrigir gradualmente quando seguro;
- beat grid inválido: cair para crossfade simples;
- faixa não carregada: pausar e avisar;
- usuário mover jog/rate/crossfader: entrar em `ManualOverride`;
- erro crítico: parar automação sem interromper abruptamente o áudio.

## 21.5 Controles do módulo

Adicionar controles conceituais:

```text
[MusicSync],enabled
[MusicSync],generate_arrangement
[MusicSync],start
[MusicSync],pause
[MusicSync],resume
[MusicSync],cancel
[MusicSync],skip
[MusicSync],preview_transition
[MusicSync],current_item
[MusicSync],state
[MusicSync],confidence
```

Confirmar convenções e evitar colisão com controles existentes.

---

# 22. Interface do usuário

## 22.1 Estratégia do MVP

Usar a interface existente do Mixxx e adicionar um painel ou diálogo dedicado.

Não criar uma aplicação separada.

Como a interface QML do Mixxx está em evolução, o MVP deverá:

- evitar uma reescrita geral de skin;
- usar componentes compatíveis com a baseline;
- manter o módulo desacoplado da futura migração QML;
- criar modelos C++ reutilizáveis por Qt Widgets ou QML.

## 22.2 Telas ou seções

### A. Seleção

- playlist/crate/faixas selecionadas;
- filtros;
- status de análise;
- botão analisar faltantes.

### B. Intenção

- duração;
- curva de energia;
- BPM inicial/final;
- tolerância;
- estilo de transição;
- restrições de vocais;
- faixas obrigatórias.

### C. Alternativas

- três sequências;
- score total;
- curva prevista;
- avisos;
- explicações por par.

### D. Editor simples

- lista ordenada;
- mover faixa;
- bloquear posição;
- excluir;
- selecionar transição;
- duração em compassos;
- tipo;
- prévia.

### E. Execução

- faixa atual;
- próxima faixa;
- transição atual;
- contagem de compassos;
- estado;
- botão iniciar, pausar, retomar, pular e cancelar;
- status da gravação.

## 22.3 Timeline

No MVP, usar uma representação simples baseada em lista e blocos.

Uma timeline multitrack completa ficará para fase posterior.

## 22.4 UX

- presets antes de controles avançados;
- termos técnicos com ajuda curta;
- confiança sempre visível;
- correção manual acessível;
- distinção entre recomendação e obrigação;
- confirmação antes de substituir fila ou iniciar gravação;
- nenhum modal durante transição crítica;
- atalhos de teclado opcionais.

---

# 23. Estratégia de IA

## 23.1 O que é IA neste projeto

- Music Information Retrieval;
- classificação de energia;
- detecção de estrutura;
- estimativa vocal;
- recomendação de sequência;
- seleção de transição;
- explicação das recomendações;
- interpretação opcional de intenção textual.

## 23.2 O que não depende de LLM

- BPM;
- beat grid;
- key;
- sync;
- execução de controles;
- posição exata;
- gravação;
- medição de áudio;
- validação de plano.

## 23.3 Linguagem natural

Opcional após o MVP.

Entrada:

> Quero começar progressivo, crescer sem pressa, chegar ao techno no final e encerrar com uma faixa emocional.

Saída:

```json
{
  "energyPreset": "late_peak",
  "transitionStyle": "smooth",
  "genreJourney": [
    "progressive house",
    "melodic house",
    "melodic techno",
    "techno",
    "emotional close"
  ],
  "peakPosition": 0.86,
  "maxTempoChangePercent": 5,
  "preferredTransitionBars": 32
}
```

Sempre existirão controles visuais equivalentes.

---

# 24. Gravação e exportação

## 24.1 MVP

Usar gravação em tempo real do Mixxx.

Fluxo:

1. validar dispositivo de áudio;
2. validar pasta de gravação;
3. iniciar gravação;
4. iniciar executor;
5. executar o set;
6. parar gravação;
7. localizar arquivo;
8. gerar tracklist e relatório.

## 24.2 Qualidade

- preferir WAV para arquivo mestre;
- evitar clipping por gain staging;
- reservar headroom durante blends;
- usar limiter apenas como proteção, se já houver mecanismo adequado;
- não normalizar repetidamente;
- permitir MP3 como cópia de conveniência quando suportado.

## 24.3 Renderização offline futura

Somente depois do MVP, avaliar:

- modo headless do engine;
- callback de áudio simulado;
- render sem dispositivo físico;
- execução mais rápida que tempo real;
- equivalência auditiva com o engine ao vivo.

Não criar renderer FFmpeg paralelo no MVP, pois isso duplicaria comportamento de EQ, filtros, sync e efeitos.

---

# 25. Testes

## 25.1 Regra principal

Executar os testes existentes do Mixxx e adicionar testes específicos do Music Sync.

## 25.2 Testes unitários

Cobrir:

- Camelot;
- compatibilidade harmônica;
- compatibilidade de BPM;
- interpolação de energia;
- pair score;
- penalidades;
- posições bloqueadas;
- duração prevista;
- seleção de janela;
- construção de explicações;
- serialização do plano;
- validação da máquina de estados;
- transições de estado;
- fallback sem worker.

## 25.3 Fixtures sintéticas

Gerar:

- click track 120 BPM;
- click track 128 BPM;
- mudança de BPM;
- silêncio inicial/final;
- kick em quatro tempos;
- tons em tonalidades conhecidas;
- mudança de energia a cada 32 compassos;
- região vocal simulada;
- grave forte em região específica;
- arquivo corrompido;
- MP3 com tags incompletas.

## 25.4 Testes de integração

- ler dados de faixa da biblioteca;
- disparar análise nativa;
- executar worker e cancelar;
- salvar e reabrir projeto;
- gerar três sequências;
- carregar duas faixas em decks;
- posicionar cues;
- iniciar e concluir uma transição;
- detectar override manual;
- iniciar e parar gravação em ambiente de teste quando viável.

## 25.5 Testes de tempo real

- nenhuma alocação inesperada na rotina crítica adicionada;
- nenhuma chamada a banco no callback;
- nenhuma espera por worker;
- ausência de deadlocks;
- execução estável com buffer baixo razoável;
- análise em background sem dropout.

## 25.6 Avaliação auditiva

Checklist:

- kicks alinhados;
- frase coerente;
- ausência de flam excessivo;
- graves controlados;
- vocais não conflitam;
- volume estável;
- drop preservado;
- transição não parece arbitrária;
- time-stretch aceitável;
- sem clipping;
- início e fim limpos.

---

# 26. Critérios de aceite do MVP

O MVP será considerado funcional quando:

1. O fork compilar e executar a partir da baseline `2.5.6`.
2. O Mixxx funcionar normalmente com Music Sync desabilitado.
3. O usuário selecionar pelo menos cinco faixas da biblioteca.
4. BPM, beat grid e key forem obtidos dos analisadores nativos.
5. Energia e janelas de transição forem calculadas.
6. O usuário definir uma curva de energia.
7. O sistema gerar pelo menos três sequências.
8. Cada par apresentar score, explicação e avisos.
9. O usuário reordenar e bloquear faixas.
10. O sistema planejar transições de 16 ou 32 compassos.
11. O usuário ouvir uma prévia usando os decks.
12. A sequência aprovada ser carregada na fila.
13. O executor realizar um mini-set de cinco faixas.
14. As transições usarem sync e automações previstas.
15. O usuário puder interromper e assumir manualmente.
16. O Mixxx gravar o master em WAV.
17. O sistema gerar tracklist e relatório.
18. O projeto ser salvo e reaberto.
19. Falha do worker não derrubar o Mixxx.
20. Os testes centrais passarem.
21. Não houver dependência de internet.
22. Nenhuma faixa original for modificada.

---

# 27. Fases de implementação

## Fase 0 — Preparação do fork

Entregas:

- fork e remotes configurados;
- baseline registrada;
- build Windows funcionando;
- testes upstream executados;
- `CLAUDE.md` criado;
- feature flag do Music Sync;
- documentação inicial.

Critério de saída:

- binário do Mixxx compila e inicia sem alteração funcional.

## Fase 1 — Esqueleto do módulo

Entregas:

- diretório `src/music_sync`;
- controller do módulo;
- painel ou diálogo vazio;
- sidecar SQLite;
- migrations;
- logs;
- desativação segura.

Critério de saída:

- painel abre, salva configuração e não afeta reprodução.

## Fase 2 — Adaptação da análise nativa

Entregas:

- leitura de BPM, beats, key, waveform, ReplayGain e cues;
- status de análise;
- comando para analisar faltantes;
- snapshot no sidecar;
- testes.

Critério de saída:

- selecionar 20 faixas e visualizar dados nativos consistentes.

## Fase 3 — Análise avançada mínima

Entregas:

- energia geral e curva;
- frases heurísticas;
- seções simples;
- presença de graves;
- janelas de entrada/saída;
- correção manual.

Critério de saída:

- cinco faixas eletrônicas exibem janelas plausíveis para transição.

## Fase 4 — Motor de sequência

Entregas:

- curva de energia;
- Camelot;
- matriz de compatibilidade;
- pair score;
- otimizador;
- três alternativas;
- explicações.

Critério de saída:

- gerar sequência reproduzível e respeitar posições bloqueadas.

## Fase 5 — Planejador de transições

Entregas:

- crossfade;
- EQ Blend;
- Bass Swap;
- Cut em frase;
- plano serializável;
- validações;
- fallback Auto DJ.

Critério de saída:

- gerar planos válidos para os pares da sequência.

## Fase 6 — Prévia em dois decks

Entregas:

- carregar par;
- posicionar cues;
- definir rate;
- iniciar sync;
- executar plano;
- repetir;
- cancelar;
- devolver decks ao usuário.

Critério de saída:

- ouvir uma transição de 32 compassos sem intervenção manual.

## Fase 7 — Executor de mini-set

Entregas:

- máquina de estados;
- fila inteligente;
- preparo da próxima faixa;
- automações;
- pause/resume/skip/cancel;
- manual override;
- eventos.

Critério de saída:

- executar cinco faixas continuamente.

## Fase 8 — Gravação e relatórios

Entregas:

- integração com recording;
- WAV master;
- tracklist;
- timestamps;
- session report;
- recuperação de gravação parcial.

Critério de saída:

- gerar set gravado e relatório correspondente.

## Fase 9 — Worker opcional

Entregas:

- Python packaging;
- QProcess;
- JSONL;
- timeout/cancelamento;
- energia avançada;
- vocais/seções opcionais;
- fallback.

Critério de saída:

- worker melhora análise sem se tornar dependência obrigatória.

## Fase 10 — Evoluções

- timeline avançada;
- editor de curvas de automação;
- QML;
- stems;
- LLM para intenção;
- perfis por gênero;
- renderização offline;
- suporte aprimorado a controladoras;
- exportação de projeto para outros softwares.

---

# 28. Primeiro marco técnico recomendado

Construir uma prova vertical de duas faixas dentro do Mixxx:

1. Selecionar duas faixas já analisadas.
2. Ler BPM e beat grids nativos.
3. Escolher uma janela final de 32 compassos em A.
4. Escolher uma janela inicial de 32 compassos em B.
5. Carregar A no deck 1 e B no deck 2.
6. Ajustar B ao BPM alvo.
7. Posicionar ambas nos beats planejados.
8. Iniciar B alinhada à frase de A.
9. Aplicar crossfade equal-power.
10. Aplicar bass swap no compasso planejado.
11. Concluir a transferência para B.
12. Permitir cancelamento e override manual.
13. Gravar a prévia pelo Mixxx.
14. Salvar o plano e os eventos em JSON.
15. Avaliar auditivamente.

Não implementar o otimizador completo antes que essa transição seja tecnicamente estável.

---

# 29. Build e ambiente de desenvolvimento

## 29.1 Pré-requisitos

Seguir os pré-requisitos oficiais da baseline do Mixxx.

O repositório oficial fornece scripts para preparar dependências. No Windows, utilizar o script indicado pelo próprio projeto.

## 29.2 Comandos esperados

Exemplo conceitual:

```powershell
# Preparar dependências do Mixxx no Windows
.\tools\windows_buildenv.bat

# Configurar
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Compilar
cmake --build build --parallel

# Testar
ctest --test-dir build --output-on-failure
```

Os comandos reais deverão ser confirmados na baseline e registrados no `README.md`.

## 29.3 Worker opcional

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -e .\music-sync-ai[dev]
python -m pytest .\music-sync-ai\tests
```

## 29.4 Diagnóstico

Criar um script que verifique:

- compilador;
- CMake;
- dependências do Mixxx;
- Python opcional;
- versão do worker;
- acesso de leitura às faixas;
- acesso de escrita ao diretório de configuração;
- dispositivo de áudio;
- diretório de gravação.

---

# 30. Regras de código

## 30.1 C++

Seguir `CONTRIBUTING.md`, `.clang-format` e guias do Mixxx.

Regras adicionais:

- namespace `mixxx::music_sync`;
- `std::chrono` para durações;
- RAII;
- smart pointers;
- sem `new/delete` direto;
- interfaces pequenas;
- imutabilidade para planos finalizados;
- `enum class` para estados;
- evitar singletons novos;
- cancellation explícito;
- validação antes do runtime;
- nenhum catch genérico silencioso.

## 30.2 Qt

- parent ownership correto;
- signals/slots tipados;
- modelos separados da UI;
- não bloquear event loop;
- `QProcess` somente fora de caminhos críticos;
- traduções com mecanismos do Mixxx;
- não acoplar domínio a widgets.

## 30.3 Python

- Python opcional;
- type hints;
- Pydantic ou validação equivalente;
- `pytest`;
- lint/format via configuração do módulo;
- stdout reservado a JSONL;
- stderr para logs;
- sem escrita direta no banco;
- funções DSP pequenas e testáveis;
- versões fixadas após spike.

## 30.4 Persistência

- migrations versionadas;
- statements preparados;
- transações;
- índices;
- paths normalizados;
- falhas isoladas;
- backup simples antes de migration destrutiva.

---

# 31. Definition of Done

Uma tarefa está concluída somente quando:

- o Mixxx compila;
- testes upstream relevantes passam;
- testes Music Sync passam;
- Music Sync desabilitado mantém comportamento padrão;
- cancelamento e erro foram tratados;
- não há acesso bloqueante na thread de áudio;
- logs úteis foram adicionados;
- documentação foi atualizada;
- não há caminho absoluto fixo;
- nenhuma música comercial foi adicionada;
- a funcionalidade foi testada com fluxo real;
- limitações foram registradas;
- alterações em arquivos upstream são mínimas e justificadas;
- licença da dependência foi verificada;
- commit está focado e compilável.

---

# 32. Riscos técnicos

| Risco | Impacto | Mitigação |
|---|---:|---|
| Complexidade do código-base do Mixxx | Alto | Módulo isolado, leitura do Developer Guide e mudanças pequenas |
| Patch invasivo no engine | Alto | Usar Controls, adapters e sinais antes de alterar core |
| Dropout por trabalho pesado | Crítico | Zero Python, banco, locks ou análise na thread de áudio |
| BPM metade/dobro | Alto | Dados nativos, hipóteses e correção manual |
| Downbeat incorreto | Alto | Heurística, confiança e fallback |
| Key incorreta | Médio | Confiança e compatibilidade não bloqueante |
| Choque vocal | Alto | Densidade vocal, janelas instrumentais e preview |
| Acúmulo de graves | Alto | Curva de graves e bass swap |
| Automação luta contra usuário | Crítico | Estado ManualOverride e prioridade humana |
| API de controles muda no upstream | Médio | Baseline fixa e adapter central |
| QML em evolução | Médio | Modelos desacoplados e UI simples no MVP |
| Gravação em tempo real é lenta | Médio | Aceitar no MVP; avaliar offline depois |
| Worker Python instável | Médio | Opcional, timeout, isolamento e fallback |
| Sidecar inconsistente com biblioteca | Médio | Fingerprint, reconciliação e migrations |
| GPL incompatível com dependência | Alto | Revisão antes de incorporar |
| Resultado correto, mas sem narrativa | Alto | Curva de energia, alternativas e avaliação auditiva |
| Atualização do upstream complexa | Alto | Baseline fixa, ADRs e patches isolados |

---

# 33. ADRs obrigatórios

Criar ADR para:

1. baseline `2.5.6`;
2. sidecar SQLite;
3. extensão do Auto DJ versus executor separado;
4. uso de Mixxx Controls;
5. estratégia de UI da baseline;
6. worker Python opcional;
7. algoritmo de frases;
8. pair score e pesos;
9. manual override;
10. integração com gravação;
11. política de atualização upstream;
12. eventual migração para QML;
13. stems;
14. renderização offline;
15. dependências adicionais e licenças.

---

# 34. Migração da especificação anterior

A arquitetura anterior propunha:

```text
Vue 3 + ASP.NET Core + SQLite + Python + FFmpeg
```

Essa estrutura não deve ser implementada no fork atual.

Substituições:

| Especificação anterior | Nova abordagem |
|---|---|
| Vue 3 | UI do Mixxx / Qt |
| ASP.NET Core | Application layer C++ dentro do Mixxx |
| API HTTP local | chamadas internas, signals/slots e Controls |
| SQLite principal | biblioteca Mixxx + sidecar Music Sync |
| FFmpeg como mixer | engine do Mixxx |
| renderer próprio | reprodução e recording do Mixxx |
| frontend separado | painel/diálogo integrado |
| jobs .NET | jobs Qt e analisadores existentes |
| MAUI | não aplicável |
| desktop launcher | binário do fork do Mixxx |
| time-stretch FFmpeg | engine/time-stretch do Mixxx |

O worker Python continua possível, porém apenas como complemento de análise.

---

# 35. CLAUDE.md recomendado

Criar `CLAUDE.md` no root com uma versão curta destas regras:

```markdown
# Music Sync DJ — Claude Code Rules

- Este repositório é um fork do Mixxx 2.5.6.
- Leia a especificação completa antes de implementar.
- Não introduza .NET, Vue, MAUI, Electron ou API HTTP.
- Reutilize engine, biblioteca, analyzers, waveforms, Controls, Auto DJ e recording.
- Código novo deve ficar preferencialmente em src/music_sync e namespace mixxx::music_sync.
- Não execute I/O, SQLite, Python, JSON ou locks bloqueantes na thread de áudio.
- O worker Python é opcional e nunca controla áudio.
- Mantenha o Mixxx funcional com MUSIC_SYNC desabilitado.
- Não reformate arquivos não relacionados.
- Execute build e testes existentes antes e depois de mudanças.
- Use fixtures sintéticas; nunca inclua músicas comerciais.
- Atualize docs/music-sync/implementation-status.md.
- Registre decisões em docs/music-sync/adr/.
- Primeiro marco: transição automática estável entre duas faixas usando os decks reais.
```

---

# 36. Resultado esperado da primeira entrega do Claude Code

A primeira entrega deverá conter:

1. `origin` validado como `https://github.com/luishpp/mixxx.git`;
2. `upstream` validado como `https://github.com/mixxxdj/mixxx.git`;
3. baseline e remotes documentados;
4. build do Mixxx validado no Windows;
5. suíte de testes upstream executável;
6. `CLAUDE.md`;
7. `docs/music-sync/implementation-status.md`;
8. ADR da baseline;
9. feature flag do Music Sync;
10. módulo `src/music_sync` registrado;
11. painel ou diálogo mínimo acessível;
12. sidecar SQLite criado com migration inicial;
13. leitura de uma lista de faixas selecionadas;
14. exibição de BPM e key já presentes no Mixxx;
15. nenhum worker Python obrigatório;
16. nenhuma alteração no motor de áudio;
17. Mixxx funcionando normalmente com o recurso desativado.

A segunda entrega deverá priorizar a prova vertical de transição entre duas faixas.

---

# 37. Referências técnicas

- Music Sync DJ — repositório principal: https://github.com/luishpp/mixxx
- Mixxx: https://mixxx.org/
- Mixxx — repositório upstream: https://github.com/mixxxdj/mixxx
- Release 2.5.6: https://github.com/mixxxdj/mixxx/releases/tag/2.5.6
- Developer Guide: https://github.com/mixxxdj/mixxx/wiki/Developer-Guide
- Contributing: https://github.com/mixxxdj/mixxx/blob/main/CONTRIBUTING.md
- Manual: https://manual.mixxx.org/
- Auto DJ: https://manual.mixxx.org/2.5/en/chapters/djing_with_mixxx.html
- GPLv2: https://github.com/mixxxdj/mixxx/blob/main/LICENSE
- Qt: https://doc.qt.io/
- CMake: https://cmake.org/documentation/

As páginas e APIs devem ser verificadas contra a tag exata usada como baseline. Documentação de `main` pode divergir da versão `2.5.6`.

---

# 38. Princípio final do produto

O Music Sync DJ deverá combinar quatro camadas:

1. **Curadoria humana** — seleção de músicas, intenção e decisões artísticas.
2. **Análise musical** — BPM, beat grid, key, energia, frases, seções e riscos.
3. **Planejamento inteligente** — sequência, transições, explicações e alternativas.
4. **Execução madura** — decks, sync, EQ, efeitos, mixer e gravação fornecidos pelo Mixxx.

O projeto terá sucesso quando uma pessoa sem técnica de DJ conseguir selecionar suas músicas, gerar uma jornada coerente, entender as recomendações, ouvir as transições, executar o set automaticamente e ainda assumir o controle a qualquer momento.
