# Log-Format v1 (Roboter und Simulation identisch)

Erste Zeile: `# frostbyte-log v1 robot=<stacker|runner> source=<robot|sim> seed=<n|-> build=<git-hash>`
Zweite Zeile: Spaltennamen. Danach eine Zeile alle 20 ms pro Roboter. Trennzeichen Komma, Dezimalpunkt `.`.
Leere Zelle = Wert nicht vorhanden. Spalten nie umbenennen oder umsortieren ohne neue Versionsnummer. Neue Ereignis-Token in der Spalte `event` (z. B. TUNE:…) sind keine Formatänderung, solange Spalten, Einheiten und bestehende Token unverändert bleiben; Leser müssen unbekannte Token ignorieren (Entscheidung 2. Nov 2026, D-06).

| Spalte | Einheit | Bedeutung |
| --- | --- | --- |
| t_s | s | Matchzeit (0 … 60) |
| robot | – | stacker oder runner |
| x_in, y_in, h_deg | Zoll, Grad | Pose aus der Odometrie (Ursprung Feldmitte, 0° = rote → blaue Station) |
| gx_in, gy_in, gh_deg, g_err_in | Zoll, Grad | GPS-Pose und Güte (leer, wenn keine) |
| tx_in, ty_in, th_deg | Zoll, Grad | wahre Pose (nur Simulation, am Roboter leer) |
| lift_deg, arm_deg, claw_in | Grad, Zoll | Ist-Werte der Mechanismen (arm_deg am Runner leer) |
| lift_set_deg, arm_set_deg | Grad | Sollwerte |
| cycle | Text | Zustand des Zyklus: BEREIT, AUFNEHMEN, GREIFEN, HEBEN, AM_GOAL, SETZEN, RUECKZUG, FEHLER |
| route_step | Text | aktueller Routenschritt, z. B. `12:cycle NEUTRAL_R1` |
| has_cup, has_pin, pin_yellow_up | 0/1 | Besitz und Ausrichtung |
| score | Punkte | laufender Punktestand (Simulation: Scorer; Roboter: Schätzung) |
| cpu_ms, batt_v, temp_max_c | ms, V, °C | Gesundheit |
| event | Text | Ereignisse dieses Takts, mit `;` getrennt: TIMEOUT:<zustand>, BLOCKED, COLLISION:<mit>, RULE:<regel>, PLACED:<goal>:<ebene>, TUNE:<schlüssel>=<wert> / TUNE:save / TUNE:revert (Tuning-Modus, nur am Testchassis; ADR-020), NOTE:<text> (Hinweis der Live-Handsteuerung im Browser, z. B. Ersatz-Startpose; build=live, ADR-025) |
