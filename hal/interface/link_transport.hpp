#pragma once

#include <cstddef>
#include <cstdint>

// Funkverbindung zum anderen Roboter als Byte-Paket-Kanal (L-02, ADR-043). Nicht blockierend; nur im Comms-Task benutzen,
// nie im Steuer-Task. Das Protokoll (Frames, Bestätigung, Frist) liegt in util/link_protocol.hpp.
namespace frostbyte::hal {

class LinkTransport {
public:
    virtual ~LinkTransport() = default;

    // Gegenstelle erreichbar (Funk steht)?
    virtual bool connected() const = 0;

    // Schickt ein Paket. false bei jedem Fehler; das Protokoll wiederholt, was wichtig ist.
    virtual bool send(const std::uint8_t* data, std::size_t n) = 0;

    // Holt ein Paket nach buf (höchstens cap Byte). Rückgabe: Länge, 0 wenn nichts da ist oder das Paket nicht lesbar war.
    virtual std::size_t receive(std::uint8_t* buf, std::size_t cap) = 0;
};

}  // namespace frostbyte::hal
