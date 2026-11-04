# ADR-034: target_p10 gilt einmal je Allianz

Status: angenommen (4. Nov 2026), Ticket SIM-24. Der Advisor (Opus) hat bei A-05 festgestellt: Der Spielstand ist die Allianz-Summe, ein Ziel je Roboter ist keine eigene Messgröße.

## Entscheidung
- `target_p10` in einer Route ist ein Ziel für den Allianz-Punktestand (Spalte `score`, in beiden Logs gleich, ADR-026). `tools/sim_report.py` prüft es einmal: P10 des Teamstands gegen den höchsten `target_p10` der beiden Routen. Eine Meldung, nicht je Roboter.
- Stehen die Punktstände beider Logs auseinander, ist das ein Fehler der Simulation und die Abnahme schlägt fehl (Schutz, kein Normalfall).
- Ziele werden nicht gesenkt, damit etwas grün wird. Der bisherige Höchstwert 120 gilt, bis ein Mensch einen anderen Wert festlegt (Frage in tasks/A-05.md). Die Routendateien bleiben unverändert.

## Folge
- Der Runner-Wert 80 hat keine Wirkung mehr, solange der Stacker 120 verlangt. Wer nur die Runner-Route ändert, sieht am Bericht weiter das Allianz-Ziel.
