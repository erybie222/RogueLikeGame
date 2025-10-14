# RogueLikeGame

Prosty szkielet projektu C++ z CMake i (opcjonalnie) vcpkg (GLFW, GLM, Vulkan, ImGui).

## Quick start

Repo zawiera wbudowane vcpkg jako submoduł w `extern/vcpkg` oraz presety CMake.

### macOS (arm64)

Wymagania:
- Homebrew
- Ninja i pkg-config (pkgconf w Homebrew)

Zainstaluj narzędzia:

```zsh
/opt/homebrew/bin/brew install ninja pkg-config
```

Build i uruchomienie z vcpkg:

```zsh
cmake --preset macos-debug
cmake --build --preset macos-debug-build -j 6
./build/macos-debug/bin/RogueLikeGame
```

Oczekiwany wynik uruchomienia (na start):

```
Hello World!
```

### Windows (x64)

Wymagania:
- MSVC (Visual Studio 2022 lub Visual Studio Build Tools 2022 z komponentami C++),
- CMake,
- Ninja (zalecane dla szybszych buildów).

Instalacja narzędzi (przykładowo):
- Visual Studio 2022 Community (Desktop development with C++), albo „Build Tools for Visual Studio 2022”.
- CMake i Ninja przez winget lub Chocolatey, np.:
  - winget: `winget install -e --id Kitware.CMake` oraz `winget install -e --id Ninja-build.Ninja`
  - choco: `choco install cmake ninja -y`

Build i uruchomienie z vcpkg (PowerShell):

```powershell
cmake --preset win-debug
cmake --build --preset win-debug-build
./build/win-debug/bin/RogueLikeGame.exe
```

Dla wersji Release użyj presetów `macos-release` / `win-release` i odpowiednich ścieżek do binarek.

## Ustawienia i presety

Plik `CMakePresets.json` definiuje presety:
- `macos-debug` – Ninja, arch: arm64, vcpkg włączony, `CMAKE_MAKE_PROGRAM` wskazuje na `/opt/homebrew/bin/ninja`.
- `macos-release` – analogicznie jak wyżej, tylko `Release`.
- `win-debug` – Ninja, vcpkg włączony, triplet `x64-windows`.
- `win-release` – analogicznie jak wyżej, tylko `Release`.


## Vulkan na macOS/Windows

Przez vcpkg instalowane są `vulkan-loader` i `vulkan-headers`. Na macOS backendem dla Vulkan jest zwykle MoltenVK (część Vulkan SDK). Jeśli będziesz używać faktycznego renderingu Vulkan, rozważ instalację Vulkan SDK (z MoltenVK) i/lub dostosowanie RPATH/packaging zgodnie z dokumentacją loadera:
- https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderApplicationInterface.md#bundling-the-loader-with-an-application

Na Windows wymagany jest sterownik/runtime Vulkan (zwykle zapewniany przez sterowniki GPU lub Vulkan SDK).

## Rozwiązywanie problemów

macOS:
- Błąd podczas budowy `glfw3` w vcpkg: „Could not find pkg-config” – zainstaluj pkg-config:
  ```zsh
  /opt/homebrew/bin/brew install pkg-config
  ```
- CMake nie znajduje Ninja: upewnij się, że Ninja jest zainstalowane i preset wskazuje właściwą ścieżkę (`CMAKE_MAKE_PROGRAM`).
  ```zsh
  /opt/homebrew/bin/brew install ninja
  which ninja
  ```
- Inna ścieżka Homebrew: na Intel macOS Homebrew bywa pod `/usr/local`. Zaktualizuj `CMAKE_MAKE_PROGRAM` w `CMakePresets.json` odpowiednio.

Windows:
- Brak kompilatora MSVC: zainstaluj Visual Studio 2022 (Desktop development with C++) lub Build Tools 2022.
- CMake nie znajduje Ninja: zainstaluj Ninja (winget/choco) i sprawdź, czy jest w PATH (`ninja --version`).
- Konflikty generatorów: presety ustawiają generator na Ninja; unikaj mieszania z Visual Studio Generator w tym projekcie.

## Struktura

- `CMakeLists.txt` – główny plik CMake; opcja `ENABLE_VCPKG_DEPS` steruje użyciem bibliotek z vcpkg.
- `vcpkg.json` – manifest zależności vcpkg (GLFW, GLM, Vulkan, ImGui z backendami GLFW/Vulkan).
- `extern/vcpkg` – kopia vcpkg w repo (submoduł).
- `src/main.cpp` – prosta aplikacja Hello World.

fix to swithing x86 to x64 on windows:
# (opcjonalnie) czyść stary cache presetu
Remove-Item -Recurse -Force .\build\win-debug -ErrorAction Ignore

# załaduj środowisko VS 2022: host x64, target x64  ✅
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -host_arch=x64 -arch=x64 && set' |
  ForEach-Object {
    if ($_ -match '^(INCLUDE|LIB|LIBPATH|PATH)=') {
      $n,$v = $_ -split '=',2
      Set-Item -Path Env:$n -Value $v
    }
  }

 (krótka kontrola – powinno pokazać HostX64 przed HostX86):
 ($env:PATH -split ';' | Select-String 'MSVC\\.*\\bin\\Host')

cmake --preset win-debug          # konfiguracja (Debug)
cmake --build --preset win-debug-build   # kompilacja
./build/win-debug/bin/RogueLikeGame.exe