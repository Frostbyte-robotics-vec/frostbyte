# ADR-004: doctest für Laptop-Tests

Status: angenommen (1. Nov 2026), Ticket S-02

## Entscheidung
- Unit-Tests laufen mit doctest 2.4.11. Der einzelne Header liegt im Repo (`test/third_party/doctest/doctest.h`, MIT-Lizenz daneben), damit der Test-Build ohne Netz konfiguriert.
- `test/CMakeLists.txt` baut `lib/` und `hal/sim/` (alle `.cpp`) als Bibliothek `frostbyte_lib` und alle `test/test_*.cpp` zu `frostbyte_tests`. Neue Tests: Datei `test/test_<name>.cpp` anlegen, CMake neu laufen lassen. Include-Pfade: `lib/`, `hal/`, Repo-Wurzel.
- C++20, `-Wall -Wextra -Wpedantic`. In `lib/` und `hal/sim/` keine PROS-Header.

## Folgen
- Windows: `-G Ninja` (oder `-G "MinGW Makefiles"`) angeben, sonst sucht CMake Visual Studio: `cmake -S test -B build-test -G Ninja`.
- Fremdcode unter `test/third_party/` darf nicht formatiert werden (`tools/format.py` überspringt `third_party`).
