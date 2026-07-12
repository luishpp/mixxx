# Building on Windows (validated commands)

Comandos que **funcionaram** nesta máquina (Windows 11, VS Build Tools 2022) para compilar a
baseline `2.5.6`. Referência oficial: https://github.com/mixxxdj/mixxx/wiki/Compiling-on-Windows

## Pré-requisitos
- **MSVC + toolchain**: VS Build Tools 2022 (MSVC 14.44, C++20) — inclui CMake 3.31 e Ninja.
  Não é necessário o workload "Desktop development with C++" do VS Community; o Build Tools basta.
- Ambiente de compilador inicializado por:
  `"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64`
  (coloca `cl`, `cmake`, `ninja`, `ctest` no PATH).

## 1. Dependências pré-compiladas
```bat
tools\windows_buildenv.bat setup
```
Baixa e verifica (SHA256) `mixxx-deps-2.5-x64-windows-c15790e.zip` (~1.5 GB) em `buildenv\`.

> **GOTCHA:** o `tools\unzip.ps1` embutido fica **extremamente lento / trava** quando `buildenv\`
> já existe (faz `New-Item -Force` por arquivo, dezenas de milhares deles). Se isso ocorrer,
> interrompa e **extraia o zip com `tar`** (nativo, rápido):
> ```bat
> tar -xf buildenv\mixxx-deps-2.5-x64-windows-c15790e.zip -C buildenv
> ```
> O download + verificação SHA256 do `.bat` funcionam bem; só a etapa de unzip é o problema.

## 2. Configurar (portable / RelWithDebInfo)
Executado dentro do ambiente do `VsDevCmd.bat`. A CLI do CMake NÃO lê o `CMakeSettings.json`
gerado pelo `.bat`; por isso os flags são passados explicitamente:
```bat
set "BE=%CD%\buildenv\mixxx-deps-2.5-x64-windows-c15790e"
cmake -S . -B build\x64__portable -G Ninja ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_TOOLCHAIN_FILE=%BE%\scripts\buildsystems\vcpkg.cmake ^
  -DMIXXX_VCPKG_ROOT=%BE% -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DOPTIMIZE=portable -DQT6=ON -DKEYFINDER=OFF -DFFMPEG=OFF ^
  -DBATTERY=ON -DBROADCAST=ON -DBULK=ON -DHID=ON -DHSS1394=ON ^
  -DLOCALECOMPARE=ON -DLILV=ON -DMAD=ON -DMEDIAFOUNDATION=ON -DMODPLUG=ON -DOPUS=ON ^
  -DQTKEYCHAIN=ON -DSTATIC_DEPS=OFF -DVINYLCONTROL=ON -DWAVPACK=ON ^
  -DDEBUG_ASSERTIONS_FATAL=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## 3. Compilar
```bat
cmake --build build\x64__portable --parallel
```
Gera `build\x64__portable\mixxx.exe` (e `mixxx-test.exe`), com o runtime Qt6 já implantado ao lado.

## 4. Testar
```bat
ctest --test-dir build\x64__portable --output-on-failure
```

## Resultado validado (2026-07-12)
- Build: sucesso (exit 0).
- `mixxx.exe --version` → `Mixxx 2.5.6`.
- `ctest`: **100% tests passed, 0 tests failed out of 854** (alguns testes marcados como *Disabled* pelo upstream não executam).
