# ADR-020: Tuning-Modus für PID-Werte, Speichern als Textdatei auf der SD-Karte

Status: angenommen (2. Nov 2026), Ticket D-06. Nummer 020, weil 018 (D-08, PR #29) und 019 (M-01, PR #30) auf offenen Branches belegt sind.

## Entscheidung
- **Tuning-Modus** (`lib/control/tuning.hpp`, Schicht 4): `TuningMode` verwaltet einen `TuningSet` (PID-Werte lateral und
  angular: kp, ki, kd, i_zone, also 8 Parameter). Er kennt weder Speicher noch PROS. Tasten (nur wenn eingeschaltet):
  Steuerkreuz hoch/runter wählt, links/rechts ändert, X fein/grob, A Speichern anfordern, B Änderungen verwerfen.
  Werte bleiben in festen Grenzen (`tuning_param()`); `i_max_v` und `out_max_v` sind Sicherheitsgrenzen und bleiben in config/.
- **Standard aus:** `config::tuning_enabled = false` in beiden Robotern (Test sichert das). Eingeschaltet gehören A/B/X
  und das Steuerkreuz dem Tuning, nicht dem Fahrer (ADR-014 belegt A/B/X). Nur am Testchassis, nie im Match.
- **Kein Speichern im Steuer-Task:** `take_save_request()` liefert nur ein Flag; geschrieben wird im niedrig priorisierten Task
  (`save_tuning`, nur nach A, nicht bei jeder Änderung, wegen SD-Verschleiß und halb geschriebener Dateien). Gelesen wird
  einmal in `initialize()` (`load_tuning`). Laden/Speichern liegt in `tuning_store.hpp`, getrennt vom Modus.
- **Speicher-Schnittstelle:** `hal/interface/storage.hpp` (`read`, `write`, keine PROS-Header), `hal/sim/sim_storage.hpp`
  (Arbeitsspeicher), `hal/pros/pros_storage.hpp` (`/usd/tuning.txt`). Die Simulation liest nie eine Datei (byte-gleich bei gleichem Seed).
- **Dateiformat v1:** Kopfzeile `# frostbyte-tuning v1`, dann je Parameter `schluessel=wert\n`. Ganz oder gar nicht: falsche
  Version, fehlender/doppelter/unbekannter Schlüssel, nicht endlicher Wert, Wert außerhalb der Grenzen, Müll hinter dem Wert
  oder fehlendes Zeilenende (abgeschnitten) verwirft die ganze Datei; es gelten die Standardwerte aus config/. Änderung am
  Format nur mit neuer Versionsnummer.
- **Log ohne neue Format-Version:** Änderungen erscheinen in der vorhandenen Spalte `event` als `TUNE:lat.kp=1.05`, `TUNE:save`,
  `TUNE:revert` (kein Komma, kein Semikolon). Es steht höchstens ein Ereignis an; bei Handbedienung genügt das.
- **Laufzeitänderung:** `Pid::set_gains()`. Der I-Anteil ist schon mit ki multipliziert, springt also nicht; eine engere
  I-Grenze kappt ihn.
- **Nur eigener Pid:** Mit `motion_uses_lemlib_pid = true` (nur Vergleich in D-07) ist Tuning aus; `LemlibRegulator` bekommt
  die Werte im Konstruktor und bräuchte sonst eine eigene Umrechnung (ki·T, kd/T, siehe docs/notes.md).

- **Stromverlust beim Schreiben:** Die Datei ist dann abgeschnitten (letzte Zeile ohne Zeilenende oder Schlüssel fehlt) und wird komplett verworfen.
- **Rat des Advisors (Opus):** Speichern nur außerhalb des Steuer-Tasks und nur auf Tastendruck, Datei ganz oder gar nicht, Tuning standardmäßig aus, nur mit dem eigenen Pid, Ereignisse in der bestehenden Spalte `event` statt neuer Log-Version.

## Nicht Teil dieses Tickets
- Anzeige auf dem Controller (C-04). `TuningMode::state()` ist die Schnittstelle dafür.
- Verdrahtung im Steuer-Task, Speichern im Low-Prio-Task und `take_event()` ins Log: Ticket D-09 (hängt an F-05, Logger). Geladen wird bereits in `initialize()`. Bis dahin schreibt auf dem Roboter nichts die Datei; „überlebt Neustart“ ist nur am Laptop belegt (Round-Trip über `SimStorage`).
- Konflikt mit der Fahrer-Belegung: Ist Tuning an, feuert `DriverMap` auf A/B/X weiter (toggle_place, HOLD, Assist). D-09 muss die Fahrer-Aktionen im Tuning-Modus sperren.
- Übernahme getunter Werte ins Repo: von Hand in config/ (D-07).
- Atomares Schreiben (Temp-Datei + Umbenennen): am Roboter prüfen, ob die SD-Karte das zuverlässig kann (TODO(HW)).
