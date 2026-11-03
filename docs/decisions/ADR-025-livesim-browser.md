# ADR-025: Fahrmodell im Browser (Live-Handsteuerung)

Status: angenommen (3. Nov 2026), Ticket SIM-14.

## Entscheidung
- `tools/viewer/livesim.js` (ohne DOM, läuft im Browser und unter Node) ist eine **vereinfachte Kopie** des C++-Fahrmodells aus `sim/drive.cpp` (ADR-008): pro Seite `V = kS·sgn(v) + kV·v + kA·a`, Haftreibung, Anhalten beim Vorzeichenwechsel, Schlupfgrenze `mu·g`, Spannung auf `min(motor_max_V, Akku)` begrenzt, halbimplizite Integration mit Heading in der Schrittmitte. Nicht enthalten: Kollision, Mechanismen, Sensoren, Rauschen, Objekte.
- **Keine Modellzahlen im JS.** Alles kommt aus `sim/sim_params.json`: `roboter.<name>.antrieb/akku`, `welt` (Schritt, Steuertakt, g) und der neue Block `live` (`langsam_faktor`, `ersatzpose_in` je Roboter; beide `quelle: schaetzung`). Die Seite lädt die Datei zur Laufzeit (lokal `../../sim/sim_params.json`, veröffentlicht `sim_params.json` neben dem Viewer; CI und Dashboard-Workflow kopieren sie).
- **Feste Schritte**: Akkumulator in ms, ein Schritt = `welt.schritt_ms`, Steuerung (Tasten → Volt) alle `welt.steuer_ms`. Ein Bild darf höchstens 100 ms nachholen (Tab im Hintergrund: Pause via `visibilitychange`, danach kein Aufholen). Gleicher Tastenverlauf ergibt dieselbe Pose, unabhängig von der Bildrate (Node-Test).
- **Eingabe → Volt in einer Funktion** (`FBLive.keysToVolts`): Arcade, W/S vor/zurück, A/D drehen (A = gegen den Uhrzeigersinn, passend zur Heading-Konvention aus ADR-008), Shift = Langsam-Modus (`live.langsam_faktor`). Später durch Gamepad ersetzbar, ohne das Modell anzufassen.
- Tab wechselt den gesteuerten Roboter (der andere bekommt 0 V und rollt aus), R setzt beide auf die Startpose zurück. Startposen aus `field.json` (`START_STACKER`, `START_RUNNER`); bei `null` die Ersatzpose aus `live.ersatzpose_in`, im Overlay als Hinweis gezeigt.
- **CI**: Node führt die JS-Tests (`tools/viewer/test_livesim.py`, eigener CI-Schritt, weil `tools/viewer` kein Paket ist); bisher liefen JS-Tests nur unter macOS (osascript). In der CI schlägt ein fehlendes Node fehl statt zu überspringen.
- **Kein WebAssembly jetzt**: Das C++-Modell hängt an Json-Loader, Welt und Collision; ein Wasm-Build brächte Toolchain (Emscripten) und ein neues Paket in die CI für ein Modell mit drei Gleichungen. Die Kopie ist klein und per Test gesichert.

## Folgen / offen
- **Zwei Modelle, die auseinanderlaufen können.** Der Abgleich JS gegen C++ (gleiche Eingaben, gleiche Pose) folgt in SIM-17. Bis dahin ist die Live-Seite nur für Test und Vorführung, nie Quelle für Messwerte oder Routen-Bewertung (dafür `frostbyte_sim`).
- Fahrdynamik-Unterschiede zum echten Roboter (Schlupf beim Drehen, Spannungseinbruch) fehlen wie im C++-Modell.
- Die Tests prüfen Verhalten relativ zu Fixture-Parametern (stationäre Geschwindigkeit `(V−kS)/kV`, Drehung auf der Stelle, Determinismus), nicht die Schätzwerte aus `sim_params.json`.
