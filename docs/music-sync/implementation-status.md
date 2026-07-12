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

## Próximas fases (roadmap)
- **Fase 1** — Esqueleto do módulo `src/music_sync/`, controller, painel/diálogo vazio atrás da flag, sidecar SQLite + 1ª migration, desativação segura.
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
