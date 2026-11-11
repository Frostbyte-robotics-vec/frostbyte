# ADR-047: Logger – Schlange, Logger-Task, Log-Ziel

Status: angenommen (11. Nov 2026), Ticket F-05. Der Advisor (Opus) hat den Entwurf vor der Umsetzung geprüft.

## Entscheidung
- **Übergabe zwischen den Tasks:** `lib/core/spsc_queue.hpp`, eine sperrenfreie Schlange für einen Erzeuger und einen Verbraucher (`std::atomic`, wie `PhaseFlag`). Der Steuer-Task blockiert nie: ist sie voll, fällt die **neueste** Zeile weg (`dropped()` zählt). `RingBuffer` bleibt unverändert (nicht thread-sicher, überschreibt die älteste Zeile).
- **Zeilen werden kopiert, nicht verwiesen:** `LogRecord` hat feste Textpuffer (`cycle` 12, `route_step` 32, `event` 64 Zeichen, bei Überlänge abgeschnitten). `logfmt::Row` hält nur Zeiger; die Quelle darf nach `record()` ungültig werden.
- **Eine Formatierung für Roboter und Simulation:** der Logger-Task ruft `logfmt::format_header/format_row` (ADR-007). Test: Ausgabe byte-gleich zu direktem `format_row`. Kopfzeile am Roboter: `source=robot seed=- build=<FB_COMMIT>`.
- **Ziel:** neue Schnittstelle `hal::LogSink` (`append`, `flush`) in `hal/interface/`. Nicht `hal::Storage`: dessen `write` ersetzt die ganze Datei. Umsetzungen: `MemoryLogSink` (`hal/sim/`, Tests) und `ProsLogSink` (`hal/pros/`, `fopen`/`fwrite`/`fflush` je Schreibvorgang).
- **Takt:** Steuer-Task 10 ms, eine Zeile alle 20 ms (jeder zweite Takt, `LogCadence`, gleich `welt.log_ms` in der Simulation). Logger-Task alle 50 ms mit `TASK_PRIORITY_MIN + 1`; er läuft nicht unter Prüfprogrammen (F-03, L-01) und nur, wenn die Karte die Datei öffnen ließ. Kein Log im Zustand Disabled (die Karte würde sonst zwischen den Läufen volllaufen). `t_s` = Zeit seit Beginn der aktuellen Phase (Autonomie bzw. Fahrer, je 60 s).
- **Dateiname:** `/usd/log_NNN.csv`, kleinste freie Nummer 001–999. Das Brain hat keine Uhr; so überschreibt ein neuer Start nie eine frühere Datei.
- **Verlust sichtbar:** geht eine Zeile verloren (Schlange voll oder Schreibfehler), trägt die nächste gespeicherte Zeile `LOGDROP:<anzahl>` im Feld `event`. Neues Ereignis-Token, keine Formatänderung (docs/log_format.md). Zusätzlich neues Token `PHASE:AUTONOMOUS` / `PHASE:DRIVER` in der ersten Zeile einer Phase.
- **Puffer:** 256 Zeilen (ca. 5 s bei 50 Zeilen/s) für Pausen der Karte.
- **Gefüllte Spalten heute:** `t_s`, `robot`, `cpu_ms` (Rechenzeit des Vortakts) und `event`. Alle anderen bleiben leer, bis der Steuer-Task sie berechnet (A-10, M-10, F-06). Nichts wird erfunden.
- **tools/log_plot.py:** nur Standardbibliothek, schreibt HTML mit SVG (wie `sim_report.py`); kein matplotlib, also kein neues Paket.

## Folgen / am Roboter zu prüfen (TODO(HW))
- Schreibpausen der SD-Karte: reichen 256 Zeilen? (`LOGDROP` im Log zeigt es.)
- Stromverlust mitten im Schreiben: es gibt nur `fflush`, kein `fsync`; die letzten ≤ 50 ms können fehlen oder die Datei kann beschädigt sein.
- Karte fehlt: kein Logger-Task, Roboter fährt trotzdem (Warnung kommt mit F-06).
- Kostet der Task am Roboter Takt-Jitter? (Anzeige aus F-04.)
