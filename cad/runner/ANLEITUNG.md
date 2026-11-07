# Runner in Fusion 360 nachbauen (CAD-01)

Das Script baut den Runner aus echten VEX-Teilen der VEX-CAD Fusion 360 Library 2.0.0, mit Gelenken,
Montagereihenfolge und zwei Tracking-Rädern. `pruefung.py` prüft das Ergebnis. Entscheidung: ADR-040.

## Einmalig einrichten
1. Bibliothek „VEX-CAD-Fusion-360-Library-2.0.0“ herunterladen und entpacken (Achtung: der Ordner liegt nach dem Entpacken doppelt
   ineinander).
2. Ordner anlegen: `%APPDATA%\Autodesk\Autodesk Fusion 360\API\Scripts\FrostbyteRunner\`
3. `FrostbyteRunner.py` und `pruefung.py` aus diesem Ordner hineinkopieren.
4. In `FrostbyteRunner.py` oben `LIB` auf den eigenen Bibliothekspfad setzen (der innere der beiden gleichnamigen Ordner).

## Bauen
1. Fusion öffnen, **Utilities → Add-Ins → Scripts and Add-Ins** (Shift+S).
2. Bei „My Scripts“ auf **+** klicken und den Ordner `FrostbyteRunner` wählen, dann **Run**.
3. Dauer etwa 4–5 Minuten. Fusion zeigt in der Zeit „Keine Rückmeldung“. Das ist normal, bitte nicht schließen.
4. Ergebnis: neues Dokument „Frostbyte Runner“. Das Protokoll steht in `letzter_lauf.txt` neben dem Script.
5. Dokument speichern (Strg+S). Das Script speichert nicht selbst.

## Prüfen (optional, 8–13 Minuten)
`pruefung.py` im selben Dialog starten (zuerst den Ordner wählen, dann die Datei). Ergebnis in `pruefung.txt`:
- Startvolumen (Grenze 15″ × 15″ × 15″)
- Kollisionen in der Startstellung. Überschneidungen innerhalb eines Bibliotheksteils (z. B. Rollen/Nabe eines Omnirads)
  werden ignoriert, Gewinde, Passungen und Zahneingriffe ebenfalls.
- Lift in 6 Schritten durchfahren, Kollisionen je Schritt
- Werkzeugzugang je Schraube (Schraubendreher 0,2″ dick, 3″ lang): frei, Vormontage, „erst vor Baugruppe X montieren“ oder
  nicht erreichbar

Stand 07.11.2026: Startvolumen 14,77 × 14,88 × 14,70″, 0 Kollisionen in Start und Liftweg, 129 Schrauben, 0 nicht erreichbar.

## Bewegen
Gelenk anklicken und ziehen, oder Rechtsklick → **Drive Joints**:
- `Arme unten`: Lift (4-Gelenk, Ritzel 12T → 60T gekoppelt)
- `Backe L` / `Backe R`: Greifer (über Zahnstangen gekoppelt)
- `Motorwelle ..` / `Rad ..`: Antrieb (84T → 60T gekoppelt)
- `Kipprolle`, `Rolle unten`, `Rolle oben`: Aufnahme (Kette gekoppelt)
- `Odometrie Laengs/Quer Schwinge`: Federweg ±2,6° (etwa ±0,07″ am Rad); `Odometrie Laengs/Quer Rad`: Raddrehung

## Odometrie (für D-11 / M-14)
Bezug: Mitte zwischen den vier Antriebsrädern, x vorn, y links (ADR-012).

| Wert | CAD | Bedeutung |
| --- | --- | --- |
| `parallel_y_in` | 2.100 | Längsrad (misst vorwärts), sitzt links vorn |
| `perpendicular_x_in` | 3.700 | Querrad (misst seitlich), sitzt rechts vorn |
| Raddurchmesser | 2.752 | 2.75″ AS Omni, nominell – am Roboter kalibrieren |
| Spurbreite | 11.57 | Mitte Antriebsrad links bis rechts |

Aufbau: V5 Rotation Sensor direkt auf der Radwelle (1:1), Schwinge und Halter gedruckt (PETG). Die Brain-Platte sitzt dafür
auf 0,5″ + 0,125″ Distanzhülsen. Noch nicht modelliert: Gummiband/Feder, die die Schwingen auf den Boden drückt (M-14).

## Koordinaten im Script
Zoll; X rechts, Y vorn (Fahrtrichtung), Z oben, Boden Z = 0. Ursprung = Mitte zwischen den Antriebsrädern.
