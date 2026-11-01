# Stolperfallen und Erkenntnisse

Kurze Einträge, neueste oben: Datum · Ticket · Erkenntnis. Jede KI-Sitzung liest diese Datei beim Start eines Tickets mit.

2026-11-01 · S-01 · Windows: PROS nur aus kurzen Pfaden bauen (nicht aus Temp/Scratchpad). Der Arm-Compiler (MinGW) stößt bei den verschachtelten `../`-Includes von liblvgl an die 260-Zeichen-Grenze und meldet „lv_conf_internal.h: No such file". Der Repo-Pfad selbst reicht aus (auch mit Leerzeichen).
2026-11-01 · S-01 · `pros.exe` ist auf manchen Windows-Rechnern ein kaputter Starter („No module named pros"). Stattdessen `py -m pros.cli.main ...`. Der Build selbst braucht nur `make` und `arm-none-eabi-gcc` im PATH.
2026-11-01 · S-01 · Brain-Build prüfen: Binärdateien enthalten nur den eigenen Robotername (`grep -a -c -w runner bin/stacker/hot.package.elf` muss 0 ergeben).
