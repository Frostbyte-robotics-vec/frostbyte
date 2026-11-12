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
- Im Browser angesehen (headless Chrome über DevTools-Protokoll): Cup kippt sichtbar um die Kante und liegt danach ruhig. Nicht gesehen: liegende Pins mit gelber Seite.
- Fehlt `physics.js` auf einer Seite, steht ein `NOTE:` im Log und Ereignisfeed; die Simulation läuft dann ohne Physik weiter.

## Nachtrag SIM-39: Greifen, Heben, Ablegen, Stapeln
Rat des Advisors (Opus) eingearbeitet: Befehle über den Steuertakt, kein Teleport, Stützflächen in jedem Schritt, Reichweite mit echten Zahlen gegengerechnet.
- **Nur mit Block `physik.greifen`** (sonst keine Hand, keine Stapel, Ebene-1-Objekte werden wie bisher übersprungen). Alle Zahlen dort sind Schätzwerte (`quelle`), auch `stapel_stufe_in` (`ebenen.stufe_in` in field.json ist null, C-06; field.json bleibt unberührt).
- **Stützfläche statt Stapelverwaltung:** Ein Teil ruht auf dem Boden, einem Goal (`hoehe_in` aus field.json) oder einem stehenden Teil darunter (dessen z + Stapelstufe), jeweils nur innerhalb des Fangradius um dessen Mitte. Das gilt in jedem Schritt: verschwindet die Stütze (Teil gegriffen, Cup umgekippt), fällt das Teil obenauf. Beim Start setzen sich Ebene-1-Objekte (`objekte_start`) von unten nach oben auf ihre Stütze (`settle`), ohne Stütze fallen sie. Die 21 Pins obenauf sind damit zum ersten Mal sichtbar.
- **Landung:** Ein abgelegtes Teil wird auf dem Weg nach unten zur Mitte der Stütze gezogen (`fang_zug_1_s`), landet mit kleinem Rückprall, Ereignis `STACK:<id>`. Außerhalb des Fangradius fällt es daneben auf den Boden (Minimalform von „schlecht gesetzter Stapel kippt“; ein Kippen vom Stapel ist nicht modelliert).
- **Befehle** `{grab}`, `{release}`, `{lift, dir}` gelten am nächsten Steuertakt (`opts.cmdAt(name, t_ms)` wie `voltsAt`, im Browser `command()` aus den Tasten E, Q, F, V). Der Lift fährt mit `lift_geschw_in_s`; gehaltene Teile werden mit endlicher Geschwindigkeit zur Hand gezogen (kein Teleport) und folgen dem Roboter.
- **Greifen:** Ein freies (nichts obendrauf), stehendes Teil vor dem Umriss, nächstes gewinnt; die Hand-Höhe muss zur Höhe des Teils passen (Teile auf einem Stapel nur mit angehobenem Lift). Liegende Objekte werden **nicht** gegriffen (Aufrichten nicht modelliert). **SG6:** höchstens ein Pin und ein Cup, sonst wird abgelehnt und `RULE:SG6` geloggt (RuleGuard, nicht umgangen). Die Stapelreihenfolge (SC2) ist Wertung und wird hier nicht erzwungen.
- **Reichweite am Goal:** Der Roboter stoppt am Goal-Kreis (`live.kollision.radius_in.goals`, 4,0 geschätzt); die Hand liegt `vorn_in` vor dem Umriss. Damit sie im Fangradius liegt, muss `Goal-Radius − vorn_in ≤ fangradius_in` gelten (Test `test_hand_erreicht_goal_mit_echten_parametern`). Mit dem echten Goal-Radius (ca. 5,9 in laut CAD) wäre das nicht mehr erfüllt; dann `vorn_in`/`fangradius_in` anpassen.
- **Im Browser geprüft:** Stacker fährt zum Cup-Stapel, hebt den Lift, greift den Pin obenauf (Cup bleibt), fährt zum RED_ALLIANCE_1 und legt ab: `STACK`, Pin steht auf 3,25 in in der Goal-Mitte.

## Nachtrag SIM-40: liegende Teile greifen
- Mit `physik.greifen.aufricht_rad_s` (Schätzwert) greift der Roboter auch liegende Teile (Reichweite, SG6 und Freiheit wie bei stehenden); ohne die Zahl bleibt es bei stehenden (Altverhalten aus SIM-39).
- Ein gehaltenes Teil wird über seinen **Fußpunkt** und die Neigung geführt: beim Greifen wandert der Bezugspunkt vom Schwerpunkt zum Fußpunkt, so wie ihn `pose()` schon zeichnet (kein Sprung); `moveHeld` zieht den Fußpunkt zur Hand und nimmt die Neigung mit `aufricht_rad_s` auf 0 zurück. Erst wenn es dort steht, gilt es als angekommen. Danach verhält es sich wie jedes gehaltene Teil (Ablegen, Stapeln).
- Nicht modelliert: Greifen eines kippenden Teils (nur stehend oder liegend), Greifen aus der Mitte des liegenden Teils (die Hand fasst immer den Fußpunkt).
