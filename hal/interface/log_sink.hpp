#pragma once

#include <cstddef>

// Ziel der Log-Datei (SD-Karte am Roboter, Speicher in Tests). Anders als hal::Storage ersetzt append() nie die Datei,
// sondern hängt an. Nur außerhalb des Steuer-Tasks benutzen: der Logger-Task schreibt, der Steuer-Task nie (F-05).
namespace frostbyte::hal {

class LogSink {
public:
    virtual ~LogSink() = default;

    // Beginnt eine neue Datei (die bisherige wird abgeschlossen; nichts wird überschrieben). Danach hängt append() an
    // die neue an. Rückgabe false, wenn keine neue Datei möglich ist (keine Karte, alle Nummern belegt, ...).
    virtual bool begin_file() = 0;

    // Hängt n Byte an. Rückgabe false bei jedem Fehler (keine Karte, voll, Datei nicht geöffnet, ...).
    virtual bool append(const char* data, std::size_t n) = 0;

    // Schiebt Gepuffertes auf den Datenträger. Rückgabe false bei Fehler.
    virtual bool flush() = 0;
};

}  // namespace frostbyte::hal
