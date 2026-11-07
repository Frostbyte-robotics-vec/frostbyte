#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hal/pros/pros_link_transport.hpp"
#include "main.h"
#include "util/link_protocol.hpp"

// Funk zum anderen Roboter im Match (L-02, ADR-043), aktiv nur mit config::link_enabled. Ein niedrig priorisierter Task
// (comms_task) spricht als Einziger mit pros::Link; der Steuer-Task und die Route benutzen nur LinkShared, und das nur mit
// Try-Lock ohne Wartezeit: klappt die Sperre nicht, bleibt der Aufruf wirkungslos (false) und der Aufrufer versucht es im
// naechsten Takt wieder. So blockiert der Funk nie den 10-ms-Takt. Ausfall des Links stoppt nichts: Signale laufen in die Frist,
// wait_for in sein Timeout (RoutePlayer, ADR-029).
// TODO(HW): Port, Link-Id, Rollen und Paketgroesse stehen in config/ und sind ungeprueft (L-04). Die Sitzungsnummer kommt aus
// der Startzeit; zwei Neustarts mit gleicher Zeit in der letzten Stelle koennten sich verwechseln (Folge: eine Bestaetigung
// zaehlt faelschlich; das Signal bleibt gesehen).
static_assert(frostbyte::LinkFrame::max_wire_size <= config::link_max_packet_bytes,
              "Funk-Frame passt nicht in link_max_packet_bytes (TODO(HW) in config/)");

namespace frostbyte {

class LinkShared {
public:
    explicit LinkShared(const LinkConfig& cfg) : ep_(self_robot(), 0, cfg) {}

    void init(std::uint8_t session, const LinkConfig& cfg) { ep_ = LinkEndpoint(self_robot(), session, cfg); }

    // Steuer-Task und Routen: alle ohne Wartezeit.
    bool try_send_signal(std::string_view name, std::uint32_t now_ms) {
        if (!locked(0))
            return false;
        const bool queued = ep_.send_signal(name, now_ms);
        unlock();
        return queued;
    }
    bool try_signal_seen(std::string_view name) {
        if (!locked(0))
            return false;
        const bool seen = ep_.signal_seen(name);
        unlock();
        return seen;
    }
    void try_set_status(const LinkStatus& s) {
        if (locked(0)) {
            ep_.set_status(s);
            unlock();
        }
    }
    void try_set_goals(const std::array<std::uint8_t, LinkFrame::kGoals>& g) {
        if (locked(0)) {
            ep_.set_goals(g);
            unlock();
        }
    }
    bool try_connected(std::uint32_t now_ms) {
        if (!locked(0))
            return false;
        const bool c = ep_.connected(now_ms);
        unlock();
        return c;
    }

    // Nur der Comms-Task.
    LinkEndpoint& endpoint() { return ep_; }
    bool lock_blocking() { return locked(10); }
    void unlock() { mutex_.give(); }

private:
    static LinkRobot self_robot() { return config::link_self_is_stacker ? LinkRobot::stacker : LinkRobot::runner; }
    bool locked(std::uint32_t timeout_ms) { return mutex_.take(timeout_ms); }

    LinkEndpoint ep_;
    pros::Mutex mutex_;
};

inline LinkShared& link_shared() {
    static LinkShared s{LinkConfig{config::link_status_period_ms, config::link_goals_period_ms, config::link_retry_ms,
                                   config::link_signal_deadline_ms, config::link_loss_ms}};
    return s;
}

inline void comms_task(void*) {
    if (config::link_port < 1 || config::link_port > 21)
        return;  // nicht festgelegt (TODO(HW)): kein Funk, Routen laufen ohne Partner (wait_for -> Timeout)
    static hal::ProsLinkTransport transport(static_cast<std::uint8_t>(config::link_port), config::link_id,
                                            config::link_is_transmitter);
    LinkShared& shared = link_shared();
    const LinkConfig cfg{config::link_status_period_ms, config::link_goals_period_ms, config::link_retry_ms,
                         config::link_signal_deadline_ms, config::link_loss_ms};
    const std::uint32_t boot_ms = pros::millis();
    if (shared.lock_blocking()) {
        shared.init(static_cast<std::uint8_t>(boot_ms ^ (boot_ms >> 8)), cfg);
        shared.unlock();
    }
    std::uint32_t now_ms = boot_ms;
    std::uint8_t buf[LinkFrame::max_wire_size];
    while (true) {
        if (transport.connected() && shared.lock_blocking()) {
            const std::uint32_t t = pros::millis();
            for (int i = 0; i < 8; ++i) {  // hoechstens 8 Pakete je Durchlauf, damit der Task nie haengt
                const std::size_t got = transport.receive(buf, sizeof buf);
                if (got == 0)
                    break;
                shared.endpoint().on_bytes(buf, got, t);
            }
            std::size_t n = 0;
            for (int i = 0; i < 4 && shared.endpoint().next_frame(t, buf, n); ++i)
                transport.send(buf, n);
            shared.unlock();
        }
        pros::c::task_delay_until(&now_ms, config::link_task_period_ms);
    }
}

}  // namespace frostbyte
