# RogueLikeGame

## 🛠️ Presety CMake

### Configure Presets
- **debug** – dev.
- **release** – finalna , zopytmalizowana wersja.

### Build Presets
- **debug-build** – buduje projekt w trybie Debug.
- **release-build** – buduje projekt w trybie Release.
- **debug-run** – buduje i uruchamia program w Debug.
- **release-run** – buduje i uruchamia program w Release.

## 🖥️ Użycie w terminalu

### Debug
```bash
cmake --preset debug # konfiguracja projektu
cmake --build --preset debug-build # kompilacja projektu
cmake --build --preset debug-run # kompilacja i uruchomienie
```
### Release
```bash
cmake --preset release # konfiguracja projektu
cmake --build --preset release-build # kompilacja projektu
cmake --build --preset release-run # kompilacja i uruchomienie

```