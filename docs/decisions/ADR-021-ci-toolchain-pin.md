# ADR-021: ARM-Toolchain in der CI fest auf 14.3.rel1

Status: angenommen (2. Nov 2026), Ticket S-09.

## Problem
Die CI installierte `gcc-arm-none-eabi` per `apt` auf `ubuntu-latest`. Der Paketstand wechselte zwischen zwei Läufen von 14.2 auf 13.2; `common.mk` verlangt `--std=gnu++26` (ab GCC 14), der Roboterbau brach auf main ab (Lauf 41406373566).

## Entscheidung
- Die CI lädt die Arm GNU Toolchain **14.3.rel1** (x86_64, `arm-none-eabi`) von developer.arm.com, prüft die SHA-256 (von Arm veröffentlicht) und cached sie. Das ist dieselbe Version, die auf dem Laptop von arjanssen in der PROS-Erweiterung von VS Code liegt (14.3.Rel1; bei anderen Teammitgliedern nicht geprüft).
- Kein neues Paket im Projekt, nur eine feste Werkzeugversion; deshalb dieser ADR.

## Update-Weg
Version, URL-Dateiname, SHA-256 und Cache-Key in `.github/workflows/ci.yml` gemeinsam ändern, gleichzeitig mit der lokalen PROS-Toolchain. Die SHA-256 steht unter `<URL>.sha256asc` bei Arm.

## Nicht gewählt
`CXX_STANDARD` auf `gnu++23` senken: ändert den Sprachstandard beider Roboter, und die CI hinge weiter vom Paketstand des Runners ab.
