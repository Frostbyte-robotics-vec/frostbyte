# ADR-001: Architektur der Codebasis

Status: angenommen (1. Nov 2026)

## Entscheidung
- PROS 4 (C++20) mit LemLib hinter eigener Schnittstelle; doctest für Laptop-Tests.
- Fünf Schichten (match → tasks → subsystems → control/localization → hal), ein 10-ms-Steuer-Task.
- Hardware hinter hal/interface; hal/pros auf dem Brain, hal/sim am Laptop.
- Simulationsplattform frostbyte_sim führt denselben Code aus und schreibt dasselbe Log wie der Roboter.

## Begründung
Software und Routen sollen ohne fertigen Roboter testbar sein; Skills-Läufe scheitern oft an Software (Reihenfolge, Timeouts, Zeit).

## Folgen
Jede Hardware-Annahme liegt in config/ oder sim/sim_params.json und wird am Roboter bestätigt.
