#pragma once

#include <cstddef>
#include <cstring>

#include "hal/interface/storage.hpp"

// Auswahlmenü für die Routine (A-06, ADR-030). Reine Logik ohne PROS und ohne Anzeige: Eingabe sind Tasten, Ausgabe der
// gewählte Name. Zeichnen auf dem Brain und das Lesen der Berührung sind Sache von src/ (Hardware-Ticket A-11).
// Gespeichert wird der Name (nicht die Nummer), damit sich der Katalog ändern darf. Laden: einmal in initialize(). Speichern:
// nur im niedrig priorisierten UI-Task, wenn take_save_request() true war, nie im 10-ms-Steuer-Task.
namespace frostbyte {

enum class MenuKey { next, prev, confirm };

// Routinen-Katalog v1 (docs/plan.md, "Routinen-Katalog"). Der Standard `nothing` steht ganz hinten und gilt bei fehlender
// oder kaputter Datei (drive_straight und drive_turns sind die Antriebs-Prüfungen aus D-07, routes/drive_*.route): stehen bleiben ist die einzige Routine, die nie etwas falsch macht.
inline constexpr const char* routine_catalog[] = {"safe_red",    "safe_blue",      "awp_red",     "awp_blue", "skills",
                                                  "test_square", "drive_straight", "drive_turns", "nothing"};
inline constexpr int routine_catalog_count = static_cast<int>(sizeof(routine_catalog) / sizeof(routine_catalog[0]));
inline constexpr int routine_default_index = routine_catalog_count - 1;

inline constexpr const char* routine_header = "# frostbyte-routine v1";

class RoutineMenu {
public:
    // Ungültige Eingaben fallen auf Index 0 bzw. einen leeren Namen zurück, das Menü bleibt benutzbar.
    RoutineMenu(const char* const* names, int count, int default_index)
        : names_(count > 0 ? names : nullptr),
          count_(names ? (count > 0 ? count : 0) : 0),
          index_(default_index >= 0 && default_index < count_ ? default_index : 0) {}

    const char* selected() const { return count_ > 0 ? names_[index_] : ""; }
    int index() const { return index_; }
    int count() const { return count_; }
    // Name des Eintrags k (für Anzeige); außerhalb des Katalogs leer.
    const char* name(int k) const { return (k >= 0 && k < count_) ? names_[k] : ""; }

    void handle(MenuKey k) {
        if (count_ == 0)
            return;
        switch (k) {
            case MenuKey::next: index_ = (index_ + 1) % count_; break;
            case MenuKey::prev: index_ = (index_ + count_ - 1) % count_; break;
            case MenuKey::confirm: save_requested_ = true; break;
        }
    }

    // Liefert true genau einmal pro confirm.
    bool take_save_request() {
        const bool r = save_requested_;
        save_requested_ = false;
        return r;
    }

    // Ganz oder gar nicht: bei missing oder invalid bleibt die Auswahl unverändert.
    LoadResult load(const hal::Storage& store) {
        char buf[96];
        const int n = store.read(buf, sizeof(buf));
        if (n < 0)
            return LoadResult::missing;
        const int idx = parse(buf, static_cast<std::size_t>(n));
        if (idx < 0)
            return LoadResult::invalid;
        index_ = idx;
        return LoadResult::loaded;
    }

    // Bei Fehler bleibt die Auswahl im Speicher, wie sie ist.
    bool save(hal::Storage& store) const {
        if (count_ == 0)
            return false;
        char buf[96];
        const std::size_t head = std::strlen(routine_header);
        const std::size_t name = std::strlen(selected());
        const char key[] = "\nroutine=";
        const std::size_t klen = sizeof(key) - 1;
        if (head + klen + name + 1 > sizeof(buf))
            return false;
        std::memcpy(buf, routine_header, head);
        std::memcpy(buf + head, key, klen);
        std::memcpy(buf + head + klen, selected(), name);
        buf[head + klen + name] = '\n';
        return store.write(buf, head + klen + name + 1);
    }

private:
    // Index des Namens oder -1. Format: Kopfzeile, dann genau eine Zeile "routine=<name>", beide mit \n.
    int parse(const char* text, std::size_t n) const {
        const std::size_t head = std::strlen(routine_header);
        const char key[] = "\nroutine=";
        const std::size_t klen = sizeof(key) - 1;
        if (n < head + klen + 2 || std::memcmp(text, routine_header, head) != 0 ||
            std::memcmp(text + head, key, klen) != 0 || text[n - 1] != '\n')
            return -1;
        const char* name = text + head + klen;
        const std::size_t len = n - (head + klen) - 1;
        if (std::memchr(name, '\n', len))  // weitere Zeile: doppelter oder unbekannter Schlüssel
            return -1;
        for (int i = 0; i < count_; ++i)
            if (std::strlen(names_[i]) == len && std::memcmp(names_[i], name, len) == 0)
                return i;
        return -1;
    }

    const char* const* names_;
    int count_;
    int index_;
    bool save_requested_ = false;
};

inline RoutineMenu make_routine_menu() {
    return RoutineMenu(routine_catalog, routine_catalog_count, routine_default_index);
}

}  // namespace frostbyte
