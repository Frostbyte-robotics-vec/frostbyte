# ADR-022: GoalBook (Platzierhöhe je Goal und Ebene)

Status: angenommen (3. Nov 2026), Ticket C-02. Der Advisor (Opus) hat vorgegeben: erst klären, was eine „Ebene“ physisch ist; keine Höhen erfinden; offene Werte als `null` mit Folgeticket sichtbar machen; Feldhöhe und Roboterversatz getrennt halten.

## Entscheidung
- `lib/util/goal_book.hpp` (`GoalBook`, bewusst in der untersten Schicht: der CycleController (tasks, C-01) und der RoutePlayer (match, A-03) rufen es beide nach unten auf), reine Logik ohne PROS, kein dynamischer Speicher. Gefüllt wird sie mit `std::array<GoalEntry, 9>` (Name, Goal-Höhe) plus Ebenenstufe; die Laptop-Tests lesen dafür `field/field.json`. Der Roboter parst kein JSON; wie die Goal-Daten auf den Roboter kommen (z. B. Erzeugung aus field.json zur Build-Zeit), ist offen und wird mit C-06 bzw. beim ersten Aufrufer entschieden.
- `place_height_in(goal, level)` (Index oder Name) liefert die Höhe über dem Boden im Feldrahmen: **Ebene 0 = `hoehe_in` des Goals, jede weitere Ebene `+ stufe_in`**, Ebenen 0–5. Außerhalb von 0–5 oder bei ungültigem Goal: `std::nullopt`.
- **Der Greifer-Versatz (Greifer ↔ gemessene Lifthöhe) steckt nicht im GoalBook.** Er ist roboterspezifisch (`config/`, `TODO(HW)`); der Aufrufer rechnet ihn dazu, bevor er `LiftSubsystem::request_move` ruft.
- `field/field.json` bekommt `ebenen: {anzahl: 6, stufe_in: null}`.

## Grenzen / offen
- **`stufe_in` ist `null`.** Die Höhe eines gestapelten Cups/Pins steht weder im Repo noch in den bisherigen CAD-Messungen. Bis sie gemessen ist, liefert das GoalBook nur Ebene 0; höhere Ebenen sind „unbekannt“, nie ein geratener Wert. Folgeticket C-06.
- Modell: eine gleichmäßige Stufe. Ob die Höhe davon abhängt, ob unten ein Cup oder ein Pin liegt, ist offen und wird mit C-06 geklärt; dann kommt der Objekttyp als Argument dazu.
- Offen für C-03: `level=auto` (route_format.md) wird nicht im GoalBook aufgelöst, sondern vom Aufrufer.
