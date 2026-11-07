#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>

// Funk-Protokoll zwischen den beiden Robotern (L-02, ADR-043): Status, Goal-Stand und Signale mit Bestätigung. Reine Logik ohne
// PROS und ohne dynamischen Speicher; Senden und Empfangen über die Schnittstelle hal/interface/link_transport.hpp, ein Task
// außerhalb des Steuer-Tasks ruft next_frame()/on_bytes() (src/link_comms.hpp). Zeiten sind pros::millis() (32 Bit, laufen
// über; gerechnet wird nur mit Differenzen). Die Uhren der beiden Roboter sind nicht synchron und werden nie verglichen.
namespace frostbyte {

enum class LinkRobot : std::uint8_t { stacker = 0, runner = 1 };

// Was der Partner vom anderen Roboter sehen kann. Software-Festlegung (ADR-043), kein Spielregel-Satz.
struct LinkStatus {
    static constexpr std::uint8_t has_pin = 1;
    static constexpr std::uint8_t has_cup = 2;
    std::uint8_t route_step = 0;  // Zeile des laufenden Routenbefehls, 0 = keiner (zum Anzeigen und im Log)
    std::uint8_t flags = 0;       // has_pin | has_cup (SG6)
};

// Ereignisname wie in Routen (docs/route_format.md): a-z, 0-9, _, 1..31 Zeichen.
inline bool link_name_valid(std::string_view s) {
    if (s.empty() || s.size() > 31)
        return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

// Ein Frame auf dem Draht (alles little endian):
//   Magic 0xF1 · Art · Sender · Sitzung · Nummer (2) · Nutzlast · Prüfsumme (XOR über alles davor, Start 0xA5)
// Das erste Byte ist nie 1 oder 2, damit der Link-Test (ADR-041, Pakete mit Art 1/2) und dieses Protokoll einander nicht
// versehentlich lesen. Die Größe je Art ist fest; ein Frame mit anderer Länge wird abgelehnt.
struct LinkFrame {
    enum class Kind : std::uint8_t { status = 0x10, goals = 0x11, signal = 0x12, ack = 0x13 };
    static constexpr std::uint8_t magic = 0xF1;
    static constexpr int kGoals = 9;             // wie GoalBook::kGoals
    static constexpr std::size_t name_len = 32;  // 31 Zeichen + Endnull
    static constexpr std::size_t header_size = 6;
    static constexpr std::size_t max_wire_size = header_size + name_len + 1;

    Kind kind = Kind::status;
    LinkRobot sender = LinkRobot::stacker;
    std::uint8_t session = 0;  // je Start des Roboters neu (Neustart ist von Wiederholung unterscheidbar)
    std::uint16_t seq = 0;
    LinkStatus status{};                       // status
    std::array<std::uint8_t, kGoals> goals{};  // goals: Objekte je Goal (GoalBook-Reihenfolge, ADR-043)
    char name[name_len] = {};                  // signal
    std::uint8_t ack_session = 0;              // ack: Sitzung und Nummer des bestätigten Signals
    std::uint16_t ack_seq = 0;

    static std::size_t payload_size(Kind k) {
        switch (k) {
            case Kind::status: return 2;
            case Kind::goals: return kGoals;
            case Kind::signal: return name_len;
            case Kind::ack: return 3;
        }
        return 0;
    }

    // Schreibt den Frame nach `out` (mindestens max_wire_size Byte), Rückgabe: Länge.
    std::size_t encode(std::uint8_t* out) const {
        out[0] = magic;
        out[1] = static_cast<std::uint8_t>(kind);
        out[2] = static_cast<std::uint8_t>(sender);
        out[3] = session;
        out[4] = static_cast<std::uint8_t>(seq & 0xFF);
        out[5] = static_cast<std::uint8_t>(seq >> 8);
        std::uint8_t* p = out + header_size;
        switch (kind) {
            case Kind::status:
                p[0] = status.route_step;
                p[1] = status.flags;
                break;
            case Kind::goals: std::memcpy(p, goals.data(), kGoals); break;
            case Kind::signal:
                std::memset(p, 0, name_len);
                for (std::size_t i = 0; i < name_len - 1 && name[i] != '\0'; ++i)
                    p[i] = static_cast<std::uint8_t>(name[i]);
                break;
            case Kind::ack:
                p[0] = ack_session;
                p[1] = static_cast<std::uint8_t>(ack_seq & 0xFF);
                p[2] = static_cast<std::uint8_t>(ack_seq >> 8);
                break;
        }
        const std::size_t n = header_size + payload_size(kind) + 1;
        out[n - 1] = checksum(out, n - 1);
        return n;
    }

    // false bei falscher Länge, Magic, Art, Sender, Prüfsumme oder (signal) ungültigem Namen; `out` bleibt dann unverändert.
    static bool decode(const std::uint8_t* in, std::size_t n, LinkFrame& out) {
        if (n < header_size + 1 || in[0] != magic)
            return false;
        const std::uint8_t k = in[1];
        if (k < 0x10 || k > 0x13 || in[2] > 1)
            return false;
        const Kind kind = static_cast<Kind>(k);
        if (n != header_size + payload_size(kind) + 1 || in[n - 1] != checksum(in, n - 1))
            return false;
        LinkFrame f;
        f.kind = kind;
        f.sender = static_cast<LinkRobot>(in[2]);
        f.session = in[3];
        f.seq = static_cast<std::uint16_t>(in[4] | (in[5] << 8));
        const std::uint8_t* p = in + header_size;
        switch (kind) {
            case Kind::status:
                f.status.route_step = p[0];
                f.status.flags = p[1];
                break;
            case Kind::goals: std::memcpy(f.goals.data(), p, kGoals); break;
            case Kind::signal: {
                std::size_t len = 0;
                while (len < name_len && p[len] != 0)
                    ++len;
                for (std::size_t i = len; i < name_len; ++i)
                    if (p[i] != 0)
                        return false;
                if (len == name_len)
                    return false;
                std::memcpy(f.name, p, len);
                if (!link_name_valid(std::string_view(f.name, len)))
                    return false;
                break;
            }
            case Kind::ack:
                f.ack_session = p[0];
                f.ack_seq = static_cast<std::uint16_t>(p[1] | (p[2] << 8));
                break;
        }
        out = f;
        return true;
    }

private:
    static std::uint8_t checksum(const std::uint8_t* p, std::size_t n) {
        std::uint8_t c = 0xA5;
        for (std::size_t i = 0; i < n; ++i)
            c = static_cast<std::uint8_t>(c ^ p[i]);
        return c;
    }
};

// Zeiten in ms. Startwerte (Software-Entscheidung); die Werte je Roboter stehen in config/, Messung am Gerät: L-04.
struct LinkConfig {
    std::uint32_t status_period_ms = 200;  // Status zugleich als Lebenszeichen
    std::uint32_t goals_period_ms = 500;
    std::uint32_t retry_ms = 50;              // Signal erneut senden, bis bestätigt
    std::uint32_t signal_deadline_ms = 2000;  // danach gibt der Sender auf (Zähler signals_expired)
    std::uint32_t loss_ms = 1000;             // so lange nichts gehört = Verbindung weg
};

struct LinkCommsStats {
    std::uint32_t frames_in = 0;       // gültig, vom Partner
    std::uint32_t frames_bad = 0;      // ungültig (Länge, Prüfsumme, Magic, Art)
    std::uint32_t frames_own = 0;      // gültig, aber mit unserem eigenen Sender (Echo/falsche Rolle)
    std::uint32_t signals_sent = 0;    // erstmals gesendete Signale
    std::uint32_t signal_retries = 0;  // Wiederholungen
    std::uint32_t signals_acked = 0;
    std::uint32_t signals_expired = 0;   // Frist ohne Bestätigung
    std::uint32_t signals_rejected = 0;  // ungültiger Name oder Warteschlange voll
    std::uint32_t signals_overflow = 0;  // empfangenes Signal, Tabelle voll
    std::uint32_t acks_dropped = 0;      // Bestätigung wegen vollem Puffer nicht gemerkt (der Sender wiederholt)
};

// Ein Endpunkt je Roboter. Nicht thread-sicher: der aufrufende Task sperrt (src/link_comms.hpp).
class LinkEndpoint {
public:
    static constexpr std::size_t max_pending = 8;  // unbestätigte eigene Signale
    static constexpr std::size_t max_seen = 16;    // verschiedene empfangene Signalnamen
    static constexpr std::size_t max_acks = 8;     // noch zu sendende Bestätigungen

    LinkEndpoint(LinkRobot self, std::uint8_t session, const LinkConfig& cfg)
        : self_(self), session_(session), cfg_(cfg) {}

    // Merkt das Signal vor; es wird wiederholt, bis der Partner bestätigt oder die Frist abläuft. false (und gezählt) bei
    // ungültigem Namen oder voller Warteschlange.
    bool send_signal(std::string_view name, std::uint32_t now_ms) {
        if (!link_name_valid(name) || npending_ == max_pending) {
            ++stats_.signals_rejected;
            return false;
        }
        Pending& p = pending_[npending_++];
        p = Pending{};
        std::memcpy(p.name, name.data(), name.size());
        p.seq = next_seq_++;
        p.first_ms = now_ms;
        return true;
    }

    void set_status(const LinkStatus& s) { status_ = s; }
    void set_goals(const std::array<std::uint8_t, LinkFrame::kGoals>& g) {
        goals_ = g;
        has_goals_ = true;
    }

    // true und `buf` (mindestens LinkFrame::max_wire_size) gefüllt, wenn jetzt ein Frame zu senden ist; je Aufruf höchstens
    // einer. Reihenfolge: Bestätigung, fälliges Signal, Status, Goal-Stand. Der Aufrufer ruft so lange, bis false kommt.
    bool next_frame(std::uint32_t now_ms, std::uint8_t* buf, std::size_t& n) {
        expire(now_ms);
        LinkFrame f;
        f.sender = self_;
        f.session = session_;
        if (nacks_ > 0) {
            f.kind = LinkFrame::Kind::ack;
            f.seq = next_seq_++;
            f.ack_session = acks_[0].session;
            f.ack_seq = acks_[0].seq;
            for (std::size_t i = 1; i < nacks_; ++i)
                acks_[i - 1] = acks_[i];
            --nacks_;
            n = f.encode(buf);
            return true;
        }
        for (std::size_t i = 0; i < npending_; ++i) {
            Pending& p = pending_[i];
            if (p.sent && static_cast<std::uint32_t>(now_ms - p.last_ms) < cfg_.retry_ms)
                continue;
            if (p.sent)
                ++stats_.signal_retries;
            else
                ++stats_.signals_sent;
            p.sent = true;
            p.last_ms = now_ms;
            f.kind = LinkFrame::Kind::signal;
            f.seq = p.seq;
            std::memcpy(f.name, p.name, sizeof f.name);
            n = f.encode(buf);
            return true;
        }
        if (!status_sent_ || static_cast<std::uint32_t>(now_ms - last_status_ms_) >= cfg_.status_period_ms) {
            status_sent_ = true;
            last_status_ms_ = now_ms;
            f.kind = LinkFrame::Kind::status;
            f.seq = next_seq_++;
            f.status = status_;
            n = f.encode(buf);
            return true;
        }
        if (has_goals_ &&
            (!goals_sent_ || static_cast<std::uint32_t>(now_ms - last_goals_ms_) >= cfg_.goals_period_ms)) {
            goals_sent_ = true;
            last_goals_ms_ = now_ms;
            f.kind = LinkFrame::Kind::goals;
            f.seq = next_seq_++;
            f.goals = goals_;
            n = f.encode(buf);
            return true;
        }
        return false;
    }

    // Ein empfangenes Paket verarbeiten (beliebiger Inhalt; Ungültiges wird nur gezählt).
    void on_bytes(const std::uint8_t* data, std::size_t n, std::uint32_t now_ms) {
        LinkFrame f;
        if (!LinkFrame::decode(data, n, f)) {
            ++stats_.frames_bad;
            return;
        }
        if (f.sender == self_) {
            ++stats_.frames_own;
            return;
        }
        ++stats_.frames_in;
        has_rx_ = true;
        last_rx_ms_ = now_ms;
        switch (f.kind) {
            case LinkFrame::Kind::status:
                if (newer(f, peer_status_.has_value(), status_session_, status_seq_)) {
                    peer_status_ = f.status;
                    status_session_ = f.session;
                    status_seq_ = f.seq;
                }
                break;
            case LinkFrame::Kind::goals:
                if (newer(f, peer_goals_.has_value(), goals_session_, goals_seq_)) {
                    peer_goals_ = f.goals;
                    goals_session_ = f.session;
                    goals_seq_ = f.seq;
                }
                break;
            case LinkFrame::Kind::signal: {
                if (nacks_ < max_acks)
                    acks_[nacks_++] = Ack{f.session, f.seq};
                else
                    ++stats_.acks_dropped;
                const std::string_view name(f.name);
                if (!seen(name)) {
                    if (nseen_ < max_seen)
                        std::memcpy(seen_[nseen_++], f.name, sizeof f.name);
                    else
                        ++stats_.signals_overflow;
                }
                break;
            }
            case LinkFrame::Kind::ack:
                if (f.ack_session != session_)
                    break;  // gilt einem Signal aus einer früheren Sitzung
                for (std::size_t i = 0; i < npending_; ++i) {
                    if (pending_[i].seq != f.ack_seq)
                        continue;
                    for (std::size_t j = i + 1; j < npending_; ++j)
                        pending_[j - 1] = pending_[j];
                    --npending_;
                    ++stats_.signals_acked;
                    break;
                }
                break;
        }
    }

    // Hat der Partner dieses Signal geschickt? Einmal gesehen bleibt es gesehen (wie der Signalbus der Simulation).
    bool signal_seen(std::string_view name) const { return seen(name); }
    std::size_t pending_signals() const { return npending_; }

    // Innerhalb loss_ms etwas vom Partner gehört? Vor dem ersten Frame: nein.
    bool connected(std::uint32_t now_ms) const {
        return has_rx_ && static_cast<std::uint32_t>(now_ms - last_rx_ms_) < cfg_.loss_ms;
    }

    const std::optional<LinkStatus>& peer_status() const { return peer_status_; }
    const std::optional<std::array<std::uint8_t, LinkFrame::kGoals>>& peer_goals() const { return peer_goals_; }
    const LinkCommsStats& stats() const { return stats_; }

private:
    struct Pending {
        char name[LinkFrame::name_len] = {};
        std::uint16_t seq = 0;
        std::uint32_t first_ms = 0;
        std::uint32_t last_ms = 0;
        bool sent = false;
    };
    struct Ack {
        std::uint8_t session = 0;
        std::uint16_t seq = 0;
    };

    void expire(std::uint32_t now_ms) {
        for (std::size_t i = 0; i < npending_;) {
            if (static_cast<std::uint32_t>(now_ms - pending_[i].first_ms) >= cfg_.signal_deadline_ms) {
                for (std::size_t j = i + 1; j < npending_; ++j)
                    pending_[j - 1] = pending_[j];
                --npending_;
                ++stats_.signals_expired;
            } else {
                ++i;
            }
        }
    }

    // Neuer als der zuletzt angenommene Frame dieser Art: andere Sitzung (Neustart) oder größere Nummer (16 Bit, mit Umlauf).
    static bool newer(const LinkFrame& f, bool have, std::uint8_t last_session, std::uint16_t last_seq) {
        if (!have || f.session != last_session)
            return true;
        const std::uint16_t d = static_cast<std::uint16_t>(f.seq - last_seq);
        return d != 0 && d < 0x8000;
    }

    bool seen(std::string_view name) const {
        for (std::size_t i = 0; i < nseen_; ++i)
            if (name == std::string_view(seen_[i]))
                return true;
        return false;
    }

    LinkRobot self_;
    std::uint8_t session_;
    LinkConfig cfg_;
    std::uint16_t next_seq_ = 0;

    std::array<Pending, max_pending> pending_{};
    std::size_t npending_ = 0;
    std::array<Ack, max_acks> acks_{};
    std::size_t nacks_ = 0;
    char seen_[max_seen][LinkFrame::name_len] = {};
    std::size_t nseen_ = 0;

    LinkStatus status_{};
    bool status_sent_ = false;
    std::uint32_t last_status_ms_ = 0;
    std::array<std::uint8_t, LinkFrame::kGoals> goals_{};
    bool has_goals_ = false;
    bool goals_sent_ = false;
    std::uint32_t last_goals_ms_ = 0;

    bool has_rx_ = false;
    std::uint32_t last_rx_ms_ = 0;
    std::optional<LinkStatus> peer_status_;
    std::uint8_t status_session_ = 0;
    std::uint16_t status_seq_ = 0;
    std::optional<std::array<std::uint8_t, LinkFrame::kGoals>> peer_goals_;
    std::uint8_t goals_session_ = 0;
    std::uint16_t goals_seq_ = 0;

    LinkCommsStats stats_;
};

}  // namespace frostbyte
