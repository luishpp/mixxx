# Upstream Baseline

| Item | Valor |
|---|---|
| Upstream | https://github.com/mixxxdj/mixxx |
| Fork (origin) | https://github.com/luishpp/mixxx |
| Baseline tag | `2.5.6` |
| Baseline commit | `3ebac449e7e5fe2a0186596657696e87ce8b0e56` ("Release 2.5.6") |
| Commit date | 2026-03-25 |
| Branch de integração | `music-sync/2.5.6` |
| Data do fork/baseline | 2026-07-12 |

## Como a baseline foi criada
```bash
git clone https://github.com/luishpp/mixxx.git music-sync-dj
cd music-sync-dj
git remote add upstream https://github.com/mixxxdj/mixxx.git
git fetch upstream --tags --prune           # o fork não trazia tags; vieram do upstream
git switch --create music-sync/2.5.6 tags/2.5.6
```

`git describe` na baseline retorna `2.5.6`. A branch `music-sync/2.5.6` deve permanecer
idêntica à tag (nenhuma alteração funcional). Todo o trabalho do Music Sync ocorre em
branches criadas a partir dela (ex.: `feature/music-sync-bootstrap`).

## Dependências pré-compiladas (Windows)
- Bundle: `mixxx-deps-2.5-x64-windows-c15790e` (de `downloads.mixxx.org/dependencies/2.5/Windows/`)
- SHA256: `138e4685ec73c6a6a509f71f8573be581403b091e4ecea2314df2cc79f9720b9`
- Triplet vcpkg: `x64-windows`

## Política de atualização do upstream (durante o MVP)
Manter a baseline fixa; não acompanhar `main`. Aplicar apenas correções upstream
claramente necessárias, registrando cada cherry-pick em `upstream-patches.md`.
