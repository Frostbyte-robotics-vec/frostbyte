# ADR-033: Befehl toggle und Toggle-Anfangszustand in der Simulation

Status: angenommen (4. Nov 2026), Ticket SIM-23. Ändert die Annahme aus ADR-032 („Toggles stehen für Routen in Zielfarbe“).

## Entscheidung
- `Game::start_toggle(t_ms, robot, toggle)` setzt nach `toggle_ms` den Toggle auf seine Zielfarbe (`field.json` `toggles[].zielfarbe`) und damit die Toggle-Farbe aller Goals seines Quadranten (`quadrant`). Antwort wie bei Zyklen (`CycleTicket`); der Roboter ist `toggle_ms` beschäftigt. Unbekannter Toggle oder beschäftigter Roboter: abgelehnt (`rejected_cycles`). `cancel` (Timeout des Players) zieht einen laufenden Toggle zurück, nach 0:00 zählt keiner mehr. Das Setzen gelingt immer (kein Zufall, kein Modell dafür).
- `SimRouteExecutor` ruft `start_toggle` für `toggle`; ohne Spielzustand bleibt es ein Platzhalter (ADR-031).
- `sim_params.json` `spiel.toggle_dauer_s` (Pflicht, > 0, Quelle `schaetzung`, nie gemessen) ist die Dauer. Sie gilt für jeden Toggle gleich.
- **Anfangszustand:** `spiel.toggle_anfang` ist jetzt `neutral`. Quelle: Game Manual v2.1, Appendix A, Zeichnung „Field Element Locations“ (alle vier Toggles gelb) und SC4 (nicht gesetzt = neutral/gelb). Die Option `zielfarbe` bleibt für Tests.
- Nicht modelliert: SC4b (ein Roboter berührt den Toggle bei Matchende, dann zählt er als neutral), Fehlschläge beim Drehen, ein Gegner, der den Toggle zurückdreht (Skills hat keinen).

## Folge
- Die Simulation hat bisher den Gewinn der Toggles gratis angenommen. Mit gelbem Start sinkt P10 deutlich (A-05, Routen v1, Posen aus A-13: 90,5 → 56). Ergebnisse vor SIM-23 sind zu optimistisch.
- Route-Timeouts von `toggle` kürzer als `toggle_dauer_s` brechen den Toggle ab (nichts wird gesetzt). Die Routen sind in A-05 anzupassen; `toggle_dauer_s` ist zuerst am Roboter zu messen.
