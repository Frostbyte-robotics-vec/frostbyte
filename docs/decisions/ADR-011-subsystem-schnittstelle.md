# ADR-011: Schnittstelle der Subsysteme (am Beispiel Drive)

Status: angenommen (1. Nov 2026), Ticket D-01. Der Advisor (Opus) hat darauf hingewiesen, dass ein zusätzliches `output()` die Regel „nur request(), state(), update()“ verletzt.

## Entscheidung
- Jedes Subsystem in `lib/subsystems/` hat genau `request(Befehl, now_s)`, `state()` und `update(now_s)`. `update()` gibt die Ausgabe des Takts zurück (bei Drive: `DriveOutput` mit Spannung links/rechts und Bremse). Kein weiterer öffentlicher Zugriff.
- Die Subsysteme kennen kein `hal::Motor`; die aufrufende Schicht (src/, Schicht 2 und darunter verdrahtet) gibt die Ausgabe an die Motoren weiter. So bleibt `lib/` frei von Hardware, und Tests brauchen keine Fakes.
- Zeit kommt als Parameter (`now_s`), nie aus einer Uhr im Subsystem. Jeder Befehl hat ein Timeout über `StateMachine`: ohne neuen Befehl fällt der Zustand auf `idle` (Antrieb steht). HOLD hat ein eigenes, längeres Timeout.
- Der Plan nennt `update(snapshot)`; solange es den Messwert-Snapshot im Subsystem-Code noch nicht gibt (Drive braucht keine Messwerte), genügt `now_s`. Braucht ein Subsystem Messwerte, wird der Snapshot ein zweiter Parameter.

## Folgen
- M-01, M-03, M-04 folgen diesem Muster (Ausgabe als Rückgabewert von `update()`).
