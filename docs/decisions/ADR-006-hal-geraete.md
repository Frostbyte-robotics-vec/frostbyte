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
