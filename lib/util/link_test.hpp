#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "core/ring_buffer.hpp"

// Funk-Link-Test (L-01, ADR-040): Ping/Pong zwischen den beiden Robotern, um Latenz und Verlust des V5-Funklinks zu messen.
// Reine Logik ohne PROS und ohne dynamischen Speicher; Senden und Empfangen stehen in src/link_test.hpp. Alle Zeiten sind
// pros::millis() des jeweiligen Roboters (32 Bit, laufen ueber); gerechnet wird nur mit Differenzen auf dem Pinger. Die Uhren
// der beiden Roboter sind nicht synchron: t_pong_ms steht im Paket, wird aber nur zur Diagnose mitgeschrieben, nie verrechnet.
namespace frostbyte {

struct LinkPacket {
    enum class Kind : std::uint8_t { ping = 1, pong = 2 };
    static constexpr std::size_t wire_size =
        14;  // Art 1, Nummer 4, t_ping 4, t_pong 4, Pruefsumme 1 (alles little endian)

    Kind kind = Kind::ping;
    std::uint32_t seq = 0;
    std::uint32_t t_ping_ms = 0;  // Sendezeit des Pings (Uhr des Pingers), der Pong gibt sie unveraendert zurueck
    std::uint32_t t_pong_ms = 0;  // Uhr des Reflektors beim Zurueckschicken; im Ping 0

    void encode(std::uint8_t* out) const {
        out[0] = static_cast<std::uint8_t>(kind);
        put32(out + 1, seq);
        put32(out + 5, t_ping_ms);
        put32(out + 9, t_pong_ms);
        out[13] = checksum(out);
    }

    // false bei falscher Laenge, unbekannter Art oder falscher Pruefsumme; `out` bleibt dann unveraendert.
    static bool decode(const std::uint8_t* in, std::size_t n, LinkPacket& out) {
        if (n != wire_size || (in[0] != 1 && in[0] != 2) || in[13] != checksum(in))
            return false;
        out.kind = static_cast<Kind>(in[0]);
        out.seq = get32(in + 1);
        out.t_ping_ms = get32(in + 5);
        out.t_pong_ms = get32(in + 9);
        return true;
    }

private:
    static void put32(std::uint8_t* p, std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            p[i] = static_cast<std::uint8_t>(v >> (8 * i));
    }
    static std::uint32_t get32(const std::uint8_t* p) {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<std::uint32_t>(p[i]) << (8 * i);
        return v;
    }
    static std::uint8_t checksum(const std::uint8_t* p) {
        std::uint8_t c = 0xA5;
        for (std::size_t i = 0; i < wire_size - 1; ++i)
            c = static_cast<std::uint8_t>(c ^ p[i]);
        return c;
    }
};

// Ergebnis eines Pings: rtt_ms = Zeit zwischen Senden und Empfang des Pongs, lost_rtt = kein Pong innerhalb des Timeouts.
struct LinkRecord {
    static constexpr std::uint32_t lost_rtt = 0xFFFFFFFFu;
    std::uint32_t seq;
    std::uint32_t t_send_ms;
    std::uint32_t rtt_ms;
};

struct LinkStats {
    std::uint32_t sent = 0;
    std::uint32_t received = 0;   // Pong rechtzeitig und zum ersten Mal
    std::uint32_t lost = 0;       // Timeout (oder Platzmangel) ohne Pong
    std::uint32_t late = 0;       // Pong kam, nachdem der Ping schon als verloren galt
    std::uint32_t duplicate = 0;  // zweiter Pong zu einem schon beantworteten Ping
    std::uint32_t reordered = 0;  // Pong kam nach dem Pong eines neueren Pings (zaehlt auch in received)
    std::uint32_t unmatched = 0;  // Pong zu keinem bekannten Ping (zu alt, nie gesendet, Sendezeit passt nicht)
};

// Seite des Pingers: schickt im festen Takt Pings (mehrere gleichzeitig unterwegs) und schreibt je Ping genau einen Eintrag,
// sobald der Ausgang feststeht (Pong oder Timeout). Die Eintraege holt das Programm mit take_record() ab und schreibt sie weg.
class LinkPinger {
public:
    static constexpr std::size_t max_open = 32;  // so viele Pings koennen gleichzeitig offen sein
    static constexpr std::size_t record_capacity = 64;

    LinkPinger(std::uint32_t period_ms, std::uint32_t timeout_ms) : period_(period_ms), timeout_(timeout_ms) {}

    // true und `out` gefuellt, wenn jetzt ein Ping zu senden ist. Das Programm sendet `out` und meldet es nicht zurueck:
    // gesendet gilt ab hier (ein Sendefehler zaehlt als Verlust, das ist fuer den Test ehrlich).
    bool next_ping(std::uint32_t now_ms, LinkPacket& out) {
        if (started_ && static_cast<std::uint32_t>(now_ms - last_send_ms_) < period_)
            return false;
        started_ = true;
        last_send_ms_ = now_ms;
        const std::uint32_t seq = next_seq_++;
        Slot& s = slot(seq);
        if (s.state == State::open)  // Platzmangel: der Ping von vor max_open Nummern gilt als verloren
            finish_lost(s);
        s = Slot{State::open, seq, now_ms};
        ++stats_.sent;
        out = LinkPacket{LinkPacket::Kind::ping, seq, now_ms, 0};
        return true;
    }

    void on_packet(const LinkPacket& p, std::uint32_t now_ms) {
        if (p.kind != LinkPacket::Kind::pong)
            return;
        if (p.seq >= next_seq_) {
            ++stats_.unmatched;
            return;
        }
        Slot& s = slot(p.seq);
        if (s.seq != p.seq || s.state == State::free || s.t_send_ms != p.t_ping_ms) {
            ++stats_.unmatched;
            return;
        }
        switch (s.state) {
            case State::answered: ++stats_.duplicate; return;
            case State::lost: ++stats_.late; return;
            default: break;
        }
        s.state = State::answered;
        ++stats_.received;
        if (has_answered_ && p.seq < highest_answered_)
            ++stats_.reordered;
        else
            highest_answered_ = p.seq;
        has_answered_ = true;
        push({s.seq, s.t_send_ms, static_cast<std::uint32_t>(now_ms - s.t_send_ms)});
    }

    // Schliesst Pings ab, deren Timeout vorbei ist (aelteste zuerst).
    void update(std::uint32_t now_ms) {
        const std::uint32_t oldest = next_seq_ > max_open ? next_seq_ - static_cast<std::uint32_t>(max_open) : 0;
        for (std::uint32_t q = oldest; q < next_seq_; ++q) {
            Slot& s = slot(q);
            if (s.state == State::open && s.seq == q && static_cast<std::uint32_t>(now_ms - s.t_send_ms) >= timeout_)
                finish_lost(s);
        }
    }

    bool take_record(LinkRecord& out) { return records_.pop(out); }
    std::size_t records_dropped() const { return records_.dropped(); }  // Programm hat zu selten abgeholt
    const LinkStats& stats() const { return stats_; }

private:
    enum class State : std::uint8_t { free, open, answered, lost };
    struct Slot {
        State state = State::free;
        std::uint32_t seq = 0;
        std::uint32_t t_send_ms = 0;
    };

    Slot& slot(std::uint32_t seq) { return slots_[seq % max_open]; }
    void finish_lost(Slot& s) {
        s.state = State::lost;
        ++stats_.lost;
        push({s.seq, s.t_send_ms, LinkRecord::lost_rtt});
    }
    void push(const LinkRecord& r) { records_.push(r); }

    std::uint32_t period_;
    std::uint32_t timeout_;
    bool started_ = false;
    std::uint32_t last_send_ms_ = 0;
    std::uint32_t next_seq_ = 0;
    bool has_answered_ = false;
    std::uint32_t highest_answered_ = 0;
    std::array<Slot, max_open> slots_{};
    RingBuffer<LinkRecord, record_capacity> records_;
    LinkStats stats_;
};

// Seite des Reflektors: macht aus jedem gueltigen Ping einen Pong.
class LinkReflector {
public:
    bool reflect(const LinkPacket& in, std::uint32_t now_ms, LinkPacket& out) {
        if (in.kind != LinkPacket::Kind::ping)
            return false;
        out = LinkPacket{LinkPacket::Kind::pong, in.seq, in.t_ping_ms, now_ms};
        ++reflected_;
        return true;
    }
    std::uint32_t reflected() const { return reflected_; }

private:
    std::uint32_t reflected_ = 0;
};

// Zeilen des Link-Protokolls (frostbyte-linklog v1, eigenes kleines Format, nicht das Roboter-Log). Rueckgabe: Laenge ohne
// Endnull, 0 wenn es nicht in den Puffer passt (dann wurde nichts verwertbares geschrieben).
inline std::size_t link_log_end(int n, std::size_t cap) {
    return (n > 0 && static_cast<std::size_t>(n) < cap) ? static_cast<std::size_t>(n) : 0;
}
inline std::size_t link_log_header(char* buf, std::size_t cap, const char* robot, const char* role, const char* build) {
    return link_log_end(
        std::snprintf(buf, cap, "# frostbyte-linklog v1 robot=%s role=%s build=%s\n", robot, role, build), cap);
}
inline std::size_t link_log_columns(char* buf, std::size_t cap) {
    return link_log_end(std::snprintf(buf, cap, "seq,t_send_s,rtt_ms\n"), cap);
}
// Leere rtt-Zelle = verloren.
inline std::size_t link_log_row(char* buf, std::size_t cap, const LinkRecord& r) {
    if (r.rtt_ms == LinkRecord::lost_rtt)
        return link_log_end(
            std::snprintf(buf, cap, "%u,%u.%03u,\n", static_cast<unsigned>(r.seq),
                          static_cast<unsigned>(r.t_send_ms / 1000), static_cast<unsigned>(r.t_send_ms % 1000)),
            cap);
    return link_log_end(std::snprintf(buf, cap, "%u,%u.%03u,%u\n", static_cast<unsigned>(r.seq),
                                      static_cast<unsigned>(r.t_send_ms / 1000),
                                      static_cast<unsigned>(r.t_send_ms % 1000), static_cast<unsigned>(r.rtt_ms)),
                        cap);
}

}  // namespace frostbyte
