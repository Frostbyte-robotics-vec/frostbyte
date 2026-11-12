# ADR-051: Physik der Spielobjekte im Browser-Livesim

Status: angenommen (12. Nov 2026), Ticket SIM-38. Rat des Advisors (Opus) eingearbeitet.

## Entscheidung
- Neues, DOM-freies Modul `tools/viewer/physics.js` (feste Schritte, deterministisch, **keine Physik-Bibliothek**, kein neues Paket). `livesim.js` ruft es auf; der Viewer zeichnet aus `objects()[i].draw` (Fußpunkt, Höhe, Neigung, Richtung) statt zwischen stehend und liegend zu springen.
- **Nur mit Block `physik` in `sim/sim_params.json`.** Ohne ihn läuft `livesim.js` exakt wie in SIM-30 (alle bisherigen Tests, Livesim-Referenz gegen C++ unverändert). Alle Zahlen im Block haben `quelle: schaetzung` (Ausnahme `hoehe_in`: Manual/CAD); die Tests nutzen eigene Fixture-Werte.
- **Die C++-Simulation bleibt ereignisbasiert** (ADR-026) und bekommt die Physik nicht. Der Livesim bleibt Vorführung, keine Quelle für Messwerte.
- **Modell:** Zylinder (Radius aus `feldobjekte.radius_in`, Höhe, Schwerpunkt). Kippen als Pendel um die Bodenkante, Drehmoment `m·g·(c·sin θ − r·cos θ)`, Kippwinkel `atan(r/c)` (= `atan(2r/h)` bei Schwerpunkt in halber Höhe). Der Roboter erzwingt den Winkel kinematisch (Berührhöhe `kontakt_hoehe_in`), darunter richtet sich das Objekt wieder auf, darüber fällt es. Aufschlag mit Rückprall, Ausschwingen, dann liegend: Gleiten mit Reibung bis zur Ruhe. Freier Fall in z mit Landung auf einer Stützfläche (`setSupport`, für SIM-39).
- **Neu gegenüber `sim/field_objects.cpp`:** Objekt gegen Objekt (Kreis gegen Kreis; ein schnelles liegendes Objekt kippt ein stehendes), Ausweichen liegender Objekte als Geschwindigkeit über `ausweich_s` statt als Sprung, Roboter weicht beim Schieben zeitverteilt zurück.
- **Abweichung bei `TOPPLED:<id>`:** das Token kommt erst, wenn das Objekt den Kippwinkel überschreitet (nicht schon bei der ersten Berührung); sanft angetippte Objekte richten sich wieder auf und erzeugen keins. Der Roboter im Ereignis ist der, der das Objekt zuletzt berührt hat.
- **Kollisionskreis liegender Objekte** liegt im Schwerpunkt (nicht am Fußpunkt); er springt beim Aufschlag um `r + c` in Kipprichtung, die Zeichnung bleibt stetig.

## Folgen / offen
- Rollen liegender Cups, Kollision fallender Objekte untereinander und die Roboterhöhe (Roboter fährt nicht über liegende Teile) sind nicht modelliert.
- Greifen, Ablegen, Stapeln: SIM-39.
- Im Browser nicht angesehen (siehe Übergabe SIM-38).
