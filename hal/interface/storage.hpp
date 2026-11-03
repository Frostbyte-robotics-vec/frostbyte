#pragma once

#include <cstddef>

// Eine kleine Datei auf dem Speicher des Roboters (SD-Karte), z. B. die getunten Reglerwerte (D-06, ADR-020).
// Nur außerhalb des Steuer-Tasks benutzen: Lesen in initialize(), Schreiben im niedrig priorisierten Task.
namespace frostbyte {

// Ergebnis beim Laden einer Datei aus dem Speicher (Tuning-Werte, gewählte Routine).
enum class LoadResult { loaded, missing, invalid };

namespace hal {

class Storage {
public:
    virtual ~Storage() = default;

    // Liest bis zu n Byte nach buf. Rückgabe: Anzahl gelesener Byte, -1 wenn es die Datei nicht gibt, nicht lesbar
    // ist oder nicht in n Byte passt (nie eine abgeschnittene Datei liefern).
    virtual int read(char* buf, std::size_t n) const = 0;

    // Ersetzt den Inhalt. Rückgabe false bei jedem Fehler (keine Karte, voll, ...).
    virtual bool write(const char* data, std::size_t n) = 0;
};

}  // namespace hal
}  // namespace frostbyte
