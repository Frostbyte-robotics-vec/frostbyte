#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>

#include "hal/interface/link_transport.hpp"
#include "pros/link.hpp"

// pros::Link im Paketmodus als LinkTransport (L-02, ADR-043). Header-only wie pros_devices.hpp (ADR-006).
// TODO(HW): am Roboter pruefen (L-04): ob receive() ohne Daten PROS_ERR liefert, ob raw_receivable_size() im Paketmodus
// brauchbar ist, ob nach EBADMSG der Puffer geleert werden muss, die tatsaechliche Paketobergrenze (config::link_max_packet_bytes)
// und was transmit() bei getrennter Verbindung tut.
namespace frostbyte::hal {

class ProsLinkTransport : public LinkTransport {
public:
    ProsLinkTransport(std::uint8_t port, const char* id, bool transmitter)
        : link_(port, id, transmitter ? pros::E_LINK_TX : pros::E_LINK_RX, true) {}

    bool connected() const override { return const_cast<pros::Link&>(link_).connected(); }

    bool send(const std::uint8_t* data, std::size_t n) override {
        return link_.transmit(const_cast<std::uint8_t*>(data), static_cast<std::uint16_t>(n)) != PROS_ERR;
    }

    std::size_t receive(std::uint8_t* buf, std::size_t cap) override {
        if (link_.raw_receivable_size() == 0)
            return 0;
        errno = 0;
        const std::uint32_t got = link_.receive(buf, static_cast<std::uint16_t>(cap));
        return (got == PROS_ERR || errno != 0 || got > cap) ? 0 : got;
    }

private:
    pros::Link link_;
};

}  // namespace frostbyte::hal
