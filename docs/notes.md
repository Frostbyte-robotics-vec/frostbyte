# Stolperfallen und Erkenntnisse

Kurze Einträge, neueste oben: Datum · Ticket · Erkenntnis. Jede KI-Sitzung liest diese Datei beim Start eines Tickets mit.

2026-11-01 · SIM-07 · Viewer (tools/viewer/index.html) läuft per Doppelklick (file://): dort geht fetch nicht, Logs und field.json über Dateiauswahl/Drag-and-drop. Über `python3 -m http.server` aus dem Repo-Wurzelordner gehen auch URL-Parameter `?log=../../out/sim/x.csv&log=…&t=12&selftest`.
2026-11-01 · SIM-07 · JS ohne node testen: `osascript -l JavaScript datei.js` (JavaScriptCore, macOS). tools/viewer/test_logparse.py nutzt das; Browser-Selbsttest: index.html?selftest. Bild prüfen: Chrome headless `--screenshot` mit `--use-angle=swiftshader --enable-unsafe-swiftshader`.
2026-11-01 · SIM-07 · Heading-Konvention ist nicht festgelegt (log_format.md sagt nur „0° = rote → blaue Station“). Viewer nimmt an: +x = rot→blau, gegen den Uhrzeigersinn positiv; steht an einer Stelle (placeOnField in index.html).
2026-11-01 · S-02 · Windows mit Visual Studio: `cmake -S test -B build-test` wählt VS (mehrere Konfigurationen), dann braucht `ctest` ein `-C Debug`, sonst „Not Run“. Einfacher: `-G Ninja` (MSYS2: `pacman -S mingw-w64-ucrt-x86_64-ninja`, PATH `C:\msys64\ucrt64\bin`), dann läuft die Zeile aus CLAUDE.md unverändert.
2026-11-01 · S-01 · Windows: PROS nur aus kurzen Pfaden bauen (nicht aus Temp/Scratchpad). Der Arm-Compiler (MinGW) stößt bei den verschachtelten `../`-Includes von liblvgl an die 260-Zeichen-Grenze und meldet „lv_conf_internal.h: No such file". Der Repo-Pfad selbst reicht aus (auch mit Leerzeichen).
2026-11-01 · S-01 · `pros.exe` ist auf manchen Windows-Rechnern ein kaputter Starter („No module named pros"). Stattdessen `py -m pros.cli.main ...`. Der Build selbst braucht nur `make` und `arm-none-eabi-gcc` im PATH.
2026-11-01 · S-01 · Brain-Build prüfen: Binärdateien enthalten nur den eigenen Robotername (`grep -a -c -w runner bin/stacker/hot.package.elf` muss 0 ergeben).
