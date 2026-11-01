<!-- Erzeugt aus config/stacker.hpp und config/runner.hpp mit frostbyte_controls_export. Nicht von Hand ändern. -->
# Fahrerbedienung

Greifen, Setzen und Fahren liegen auf beiden Robotern auf denselben Tasten. Jede manuelle Fahreingabe bricht laufende Makros am Antrieb sofort ab. Der Controller vibriert bei 15 s und 10 s Restzeit.

## stacker

| Taste | Aktion |
| --- | --- |
| Linker Stick (hoch/runter) | Fahren vor/zurück |
| Rechter Stick (links/rechts) | Drehen |
| R1 | Greifen |
| R2 | Setzen |
| L1 | Cup drehen |
| L2 | Langsam-Modus (gehalten) |
| B | Antrieb HOLD (gehalten) |
| A | Toggle setzen |
| X | Fahrassistenz an/aus (startet aus) |
| Y | Endgame-Modus (Lift einfahren + Hold) |

Fahrerprofile (Tuning, am Roboter mit dem Fahrer einstellen):

| Profil | Totzone | Exponent | Höchstgeschwindigkeit |
| --- | --- | --- | --- |
| Standard | 0.05 | 2.0 | 1.00 |
| Vorsichtig | 0.08 | 3.0 | 0.70 |

## runner

| Taste | Aktion |
| --- | --- |
| Linker Stick (hoch/runter) | Fahren vor/zurück |
| Rechter Stick (links/rechts) | Drehen |
| R1 | Greifen |
| R2 | Setzen |
| L1 | Cup drehen |
| L2 | Langsam-Modus (gehalten) |
| B | Antrieb HOLD (gehalten) |
| A | Toggle setzen |
| X | Fahrassistenz an/aus (startet aus) |

Fahrerprofile (Tuning, am Roboter mit dem Fahrer einstellen):

| Profil | Totzone | Exponent | Höchstgeschwindigkeit |
| --- | --- | --- | --- |
| Standard | 0.05 | 2.0 | 1.00 |
| Vorsichtig | 0.08 | 3.0 | 0.70 |

