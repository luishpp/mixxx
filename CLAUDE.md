# Music Sync DJ — Claude Code Rules

Este repositório é um **fork pessoal do Mixxx 2.5.6**. A fonte de verdade é
`Music_Sync_DJ_Especificacao_Claude_Code.md` (v2.1). Leia-a antes de implementar.

## Remotes e branches
- `origin`  = https://github.com/luishpp/mixxx.git  (repositório de trabalho)
- `upstream`= https://github.com/mixxxdj/mixxx.git   (somente fonte upstream)
- Baseline: tag `2.5.6` (commit `3ebac449`). Branch de integração: `music-sync/2.5.6`.
- Nunca desenvolver em `main`; nunca `push --force` em `main`/`music-sync/2.5.6`.
- Antes de qualquer push, validar `git remote get-url origin`. PRs sempre contra `luishpp/mixxx`.

## Arquitetura
- NÃO introduzir .NET, Vue, MAUI, Electron, ASP.NET Core ou API HTTP local.
- Reutilizar do Mixxx: engine de áudio, biblioteca, analisadores (BPM/beat/key/waveform/ReplayGain),
  decks/mixer, EQ/filtros/efeitos, Controls, Auto DJ e gravação do master.
- Código novo em `src/music_sync/`, namespace `mixxx::music_sync`; recursos em `res/music_sync/`;
  docs em `docs/music-sync/`; worker Python opcional em `music-sync-ai/`.
- Atrás da flag de CMake `MUSIC_SYNC_ENABLED` (default OFF). O Mixxx deve compilar e rodar
  normalmente com o recurso desativado.

## Thread de áudio (proibido)
Nunca executar na thread de áudio: I/O de disco, SQLite, Python, JSON, LLM, locks bloqueantes,
espera por future/promise, alocação grande, logging volumoso ou cálculo de otimização.
Automações são pré-calculadas e consumidas como snapshots imutáveis.

## Regras de trabalho
- Minimizar alterações em arquivos do upstream; justificar cada uma em `docs/music-sync/adr/`.
- Não reformatar arquivos do upstream não relacionados.
- Rodar o build e os testes existentes do Mixxx antes e depois de mudanças relevantes.
- Usar fixtures sintéticas; nunca incluir músicas comerciais no repositório.
- Não modificar/sobrescrever arquivos de áudio originais do usuário.
- Atualizar `docs/music-sync/implementation-status.md` a cada etapa relevante.
- Planejador determinístico e baseado em regras; cada sugestão de ordem/transição traz explicação.
- Primeiro marco técnico: transição automática estável entre duas faixas usando os decks reais.

## Build no Windows
Ver `docs/music-sync/building-windows.md` (toolchain, deps pré-compiladas, comandos validados).
