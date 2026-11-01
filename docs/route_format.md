# Routen-Format v1 (Ticket A-02/A-03)

Eine Datei = ein Roboter. Eine Anweisung pro Zeile, `#` leitet Kommentare ein. Zahlen in Zoll, Grad, Sekunden.
Namen (Goals, Loader, Toggles, Posen) müssen in field/field.json existieren, sonst Fehler mit Zeilennummer.

| Anweisung | Bedeutung |
| --- | --- |
| `target_p10 <punkte>` | Abnahmegrenze für die Simulation (P10 der Punkte) |
| `start <POSE>` oder `start pose x= y= h=` | Startpose setzen |
| `goto <NAME> [tol=<in>] [timeout=<s>]` | zu einem Feldpunkt fahren (Anfahrrichtung aus field.json) |
| `goto pose x= y= h= [timeout=]` | zu einer Pose fahren |
| `turn <deg> [timeout=]` | auf Richtung drehen |
| `intake until=unit [timeout=]` | Aufnahme laufen lassen, bis der Bumper eine Einheit meldet |
| `cycle goal=<GOAL> level=<n\|auto> [source=preload\|quiver] [timeout=]` | kompletter Zyklus bis Rückzug |
| `toggle <TOGGLE> [timeout=]` | Toggle auf Zielfarbe stellen (nur Runner) |
| `signal <name>` | Ereignis an den anderen Roboter senden |
| `wait_for <robot>.<name> [timeout=]` | auf Ereignis warten |
| `wait <s>` | warten |
| `parallel { … }` | Anweisungen gleichzeitig; fertig, wenn alle fertig sind |
| `repeat <n> { … }` | Block n-mal |
| `at <t>s <anweisung>` | Zeitwächter: ab Matchzeit t wird die laufende Anweisung abgebrochen und diese ausgeführt |
| `end hold` | Ende, alle Mechanismen halten |

Jede Anweisung mit `timeout` akzeptiert `on_timeout=skip|retry|abort` (Standard `skip`).
