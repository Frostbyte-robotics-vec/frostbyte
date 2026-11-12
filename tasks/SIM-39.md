---
id: SIM-39
phase: "5"
titel: Greifen, Ablegen und Stapeln im Browser-Livesim (Cups/Pins aufnehmen, auf Goals und Stapel setzen, Fall auf die Stapelspitze)
status: done
owner: arjanssen
branch: ticket/sim-39
depends: [SIM-38]
hw: nein
---

## Ziel
(Anpassung beim Bau: „ein schlecht gesetzter Stapel kippt“ ist nur als Minimalform umgesetzt: außerhalb des Fangradius fällt das Teil neben das Goal; Kippen vom Stapel und Greifen liegender Teile sind nicht modelliert, siehe ADR-051-Nachtrag.)
Aufbauend auf der Physik von SIM-38: Der Roboter im Livesim kann ein Objekt greifen (Besitz höchstens 1 Pin + 1 Cup, SG6), mit dem Lift heben, vor einem Goal oder Stapel ablegen. Das Objekt fällt auf die höchste Stützfläche (Boden, Goal, Stapelspitze), rastet auf der Goal-Mitte ein und bleibt sichtbar gestapelt; ein schlecht gesetzter Stapel kippt.

## Fertig, wenn …
- Tasten fürs Greifen/Ablegen/Heben (vorher index.html komplett auf freie Tasten prüfen) und gleiche Schnittstelle in `livesim.js` für Tests
- SG6: ein zweites Teil derselben Art wird abgelehnt und als Ereignis geloggt (RuleGuard nicht umgehen)
- Greifreichweite wie `ObjectField::nearest_pickup` (vergrößerter Umriss, nächstes Objekt)
- Ablegen über einem Goal landet auf `hoehe_in` (field.json) bzw. der Stapelspitze; Stufenhöhe aus der Objekthöhe (`ebenen.stufe_in` ist null, C-06) als Schätzung in `physik`, nicht in field.json
- Tests mit Fixture-Werten (Fall auf Goal, Stapel mehrerer Teile, SG6-Ablehnung, Fangradius)
- Viewer zeichnet gehaltene und gestapelte Objekte; ADR-051-Nachtrag; Laptop-Tests grün, Roboter bauen, `board.py validate` grün

## Kontext
tasks/SIM-38.md, docs/decisions/ADR-026 und ADR-024 (Stapelregeln), docs/rules_scoring.md, tasks/C-06.md (Stufenhöhe offen).

## Hardware-Prüfung
—

## Übergabe
Stand (2026-11-12): umgesetzt, bereit für Review. Baut auf ticket/sim-38 auf (PR #114 noch offen; Abhängigkeit bewusst übersprungen, weil SIM-39 dessen Physik braucht; PR gegen ticket/sim-38). Stützflächen statt Stapelverwaltung (Boden, Goal, Teil darunter), Hand/Lift/Greifen/Ablegen mit Befehlen am Steuertakt, SG6 mit RULE:SG6, Ebene-1-Pins als Körper auf ihrer Stütze, Tasten E/Q/F/V, Block `physik.greifen` (Schätzwerte), ADR-051-Nachtrag. Tests: physics_test.js (75 ok, Mutationen Stütze-weg und SG6 → rot), livesim_test.js 80 ok, ctest, validate. Im Browser (Chrome/CDP) vom Cup-Stapel zum Goal gefahren und gestapelt.
Am Rechner zu prüfen: Gefühl der Schätzwerte (Hubhöhe, Handabstand, Fangradius), liegende Pins greifen (noch nicht), Goal-Radius echt messen (ADR: Goal-Radius − vorn_in ≤ fangradius_in).

KI-Review PR #115 (Empfehlung: ändern): physics_test.js:307 ist fast tautologisch (`kein SG6 || hand.length === 2`) → beides prüfen (`&&`, Hand = {CUP, PIN}). Stapelt auf #114.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt (Folge von SIM-38) · –
2026-11-12 · arjanssen (KI) · Stützflächen, Greifen, Lift, Ablegen, Stapeln, SG6, Tasten, Tests, ADR-Nachtrag · siehe Branch
