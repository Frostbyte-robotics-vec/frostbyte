# ADR-006: Geräteschnittstellen in hal/

Status: angenommen (1. Nov 2026), Ticket F-02. Der Advisor (Opus) hat den Entwurf vor der Umsetzung geprüft.

## Entscheidung
- Vier Schnittstellen in `hal/interface/`: `Motor`, `RotationSensor`, `Imu`, `Gps`. Namespace `frostbyte::hal`, Einheiten Volt, Zoll, Grad, Sekunden (Einheit im Namen).
- Weitere Geräte (Distanz, Farbe, Optik) kommen erst, wenn die Sensorliste feststeht (offene Frage im Plan, CAD-Review 30. Nov). Es gibt noch keine Port-Tabelle; Ports bleiben `TODO(HW)` in `config/`.
- Jedes Gerät meldet Ausfall (`connected()` bzw. `GpsReading::valid`). `Motor::stop()` gibt es für den Halt bei Matchende.
- `hal/sim`: `SimMotor` mit Kennlinie `V = kS*sign(v) + kV*v + kA*a`, Haftreibung ohne Flattern um null, Spannungsbegrenzung, Ausfall einschaltbar. `SimMotorParams` hat keine Standardwerte; die Zahlen gehören nach `sim/sim_params.json` und werden von einem späteren SIM-Ticket geladen (kein JSON-Paket in diesem Ticket). Sensoren sind einfache Fakes, deren Werte Test bzw. Simulation setzt.
- `hal/pros/pros_devices.hpp` ist header-only, weil das Makefile nur `src/` kompiliert. `src/main.cpp` bindet ihn ein, damit beide Roboter ihn kompilieren; instanziert wird nichts. mV, RPM, Zentigrad und Meter werden nur dort umgerechnet.
- Include-Wurzeln: Laptop-Build und Roboter-Build kennen beide `lib/` und das Repo-Wurzelverzeichnis (`-iquote$(ROOT)/lib` im Makefile). Header in `hal/` binden sich als `"hal/..."` ein, nie als `"pros/..."` (Namenskonflikt mit dem PROS-Verzeichnis).

## Offene Frage für einen Menschen
- Welcher Bremsmodus (coast, brake, hold) erfüllt „alle Motoren auf Halt bei 0:00“? `ProsMotor` verlangt den Modus als Konstruktorparameter; die Wahl je Gerät gehört nach config/ (TODO(HW)) und ist eine Regelauslegung.

## Am Roboter zu prüfen
- Vorzeichen von IMU-Heading und GPS-Heading gegen die Feldkonvention (0° = rot -> blau); GPS-Ursprung und Achsen.
- Geschwindigkeitseinheit des Motors (RPM, hier mal 6 zu Grad/s) und Encodereinheit nach `set_encoder_units`.
- Kennlinie der Sim-Motoren gegen Messung (SIM-08).

## Nachtrag F-08 (5. Nov 2026): Motorstrom
Der Advisor (Opus) hat den Entwurf vor der Umsetzung geprüft (u. a.: Betrag statt Vorzeichen, damit nur die Einheit am Roboter zu prüfen ist; Stall-Hook minimal halten, das Stau-Modell bleibt SIM-28).
- `Motor::current_a()` (rein virtuell): Stromaufnahme als **Betrag >= 0 in Ampere**; 0, wenn das Gerät nicht erkannt wird. Nutzer: Claw (ADR-038, Plateau), später Stau-Erkennung.
- `ProsMotor`: `std::abs(get_current_draw()) / 1000.0` (mA -> A, nur dort). `PROS_ERR` zählt als 0, den Ausfall meldet `connected()`.
- `SimMotor`: `I = |V - kV*v| / R`, begrenzt auf `max_current_A` (kV*v = Gegenspannung). Frei laufend im Beharrungszustand ca. `kS / R`, blockiert `V / R`. Bremse und Ausfall: 0. `SimMotorParams` bekommt `resistance_ohm` und `max_current_A` (weiter ohne Standardwerte). Sim-Hook `set_blocked(bool)` hält die Geschwindigkeit bei 0 (Stall); ein Lastmodell gehört nicht hierher.
- Zahlen: `sim/sim_params.json` -> `motoren.v5` (`quelle: schaetzung`). Noch liest sie kein Lader; die Tests setzen eigene Werte.
- Log-Format unverändert (Strom wird nicht geloggt; das wäre eine neue Versionsnummer).

## Am Roboter zu prüfen (F-08)
- Einheit von `get_current_draw()` (mA erwartet) und ob das Vorzeichen von der Drehrichtung abhängt (Betrag wird gebildet).
- Stall-Strom eines V5-Motors bei 12 V gegen `max_strom_A`/`widerstand_ohm` (Schätzung).
