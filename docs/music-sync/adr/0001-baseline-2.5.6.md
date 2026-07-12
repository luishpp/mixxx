# ADR 0001 — Baseline Mixxx 2.5.6

- **Status:** Aceito
- **Data:** 2026-07-12
- **Contexto do projeto:** fork pessoal do Mixxx para adicionar a camada de assistência inteligente (`mixxx::music_sync`).

## Contexto

O Music Sync DJ é desenvolvido como fork do Mixxx (decisão registrada na spec v2.1), reutilizando
o motor de áudio maduro em vez de reconstruí-lo. É preciso fixar um ponto de partida estável e
reproduzível para o MVP.

## Decisão

Usar a tag estável **`2.5.6`** do Mixxx como baseline (commit `3ebac449e7e5fe2a0186596657696e87ce8b0e56`),
na branch de integração `music-sync/2.5.6` do fork `github.com/luishpp/mixxx`.

Motivos:
- release estável, reduz volatilidade durante o primeiro MVP;
- base moderna do Mixxx (Qt6, QML emergente, vcpkg);
- permite comparar regressões contra uma versão oficial conhecida;
- dependências pré-compiladas oficiais disponíveis para Windows (`mixxx-deps-2.5-x64-windows`).

## Alternativas consideradas

- **Acompanhar `main`** — rejeitado: volatilidade alta e APIs em movimento durante o MVP.
- **Baseline 2.4.x** — rejeitado: 2.5.x já é a base moderna e a 2.5.6 é a mais recente estável.
- **Reescrever o motor (spec v1.0: .NET/Vue/Python/FFmpeg)** — rejeitado na spec v2.1: duplicaria
  EQ, filtros, sync, efeitos e gravação já maduros no Mixxx.

## Consequências

- Licença efetiva do produto distribuído: **GPLv2** (do Mixxx). Dependências adicionais devem ser
  compatíveis com GPLv2.
- Atualizações de upstream ficam congeladas durante o MVP; correções pontuais entram por cherry-pick
  registrado em `upstream-patches.md`.
- Plataformas herdadas: Windows (prioridade), macOS e Linux.
- Build no Windows validado (ver `building-windows.md`): compila, inicia (`--version` → `Mixxx 2.5.6`)
  e passa 100% dos testes upstream (854).
