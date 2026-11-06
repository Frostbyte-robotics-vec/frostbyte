#pragma once

#include <cstddef>
#include <cstdio>

#include "hal/interface/storage.hpp"
#include "match/routine_menu.hpp"

// Bildschirm-Logik für das Routinenmenü (A-11, ADR-030): Tasten mit Flankenerkennung, Laden und Speichern mit Statusmeldung,
// Anzeigezeilen als Text. Reine Logik ohne PROS (die drei LLEMU-Tasten lesen und zeichnen macht src/). Nur im niedrig
// priorisierten UI-Task aufrufen: init() liest und update() schreibt die SD-Karte, nie im 10-ms-Steuer-Task.
namespace frostbyte {

// Die drei Touch-Tasten von LLEMU am Brain-Display (links, Mitte, rechts); true = gerade gedrückt.
struct Buttons {
    bool left = false;
    bool center = false;
    bool right = false;
};

enum class ScreenStatus { none, loaded, missing, invalid, saved, save_failed };

// Anzeigezeilen: ein Fenster aus Katalogeinträgen (die gewählte Routine mit Marke "> "), darunter eine Statuszeile.
struct ScreenText {
    static constexpr int menu_rows = 3;
    static constexpr int status_line = menu_rows;
    static constexpr int rows = menu_rows + 1;
    static constexpr std::size_t cols = 26;
    char line[rows][cols + 1] = {};
};

class RoutineScreen {
public:
    explicit RoutineScreen(RoutineMenu menu) : menu_(menu) {}

    // Einmal in initialize(): lädt die Auswahl. Fehlt die Datei oder ist sie kaputt, gilt der Standard und der Status sagt es.
    void init(const hal::Storage& store) {
        switch (menu_.load(store)) {
            case LoadResult::loaded: status_ = ScreenStatus::loaded; break;
            case LoadResult::missing: status_ = ScreenStatus::missing; break;
            case LoadResult::invalid: status_ = ScreenStatus::invalid; break;
        }
    }

    // Alle 100 ms. editable=false (Match läuft): Tasten werden gelesen, aber nicht ausgeführt. Je neuem Druck eine Aktion;
    // werden mehrere Tasten im selben Zyklus neu gedrückt, zählt nur die Mitte (Speichern), nie ein Verstellen dazu.
    void update(hal::Storage& store, const Buttons& now, bool editable) {
        const bool l = now.left && !prev_.left;
        const bool c = now.center && !prev_.center;
        const bool r = now.right && !prev_.right;
        prev_ = now;
        if (!editable)
            return;
        if (c) {
            menu_.handle(MenuKey::confirm);
        } else if (l && !r) {
            menu_.handle(MenuKey::prev);
            status_ = ScreenStatus::none;
        } else if (r && !l) {
            menu_.handle(MenuKey::next);
            status_ = ScreenStatus::none;
        }
        if (menu_.take_save_request())
            status_ = menu_.save(store) ? ScreenStatus::saved : ScreenStatus::save_failed;
    }

    void render(ScreenText& out) const {
        const int n = menu_.count();
        int first = menu_.index() - 1;
        if (first > n - ScreenText::menu_rows)
            first = n - ScreenText::menu_rows;
        if (first < 0)
            first = 0;
        for (int i = 0; i < ScreenText::menu_rows; ++i) {
            const int k = first + i;
            if (k < n)
                std::snprintf(out.line[i], sizeof out.line[i], "%s%s", k == menu_.index() ? "> " : "  ", name_of(k));
            else
                out.line[i][0] = '\0';
        }
        std::snprintf(out.line[ScreenText::status_line], sizeof out.line[ScreenText::status_line], "%s",
                      status_text(status_));
    }

    const char* selected() const { return menu_.selected(); }
    ScreenStatus status() const { return status_; }

private:
    const char* name_of(int k) const {
        // RoutineMenu liefert nur den gewählten Namen; die Fenster-Namen kommen aus dem Katalog des Menüs.
        return menu_.name(k);
    }

    static const char* status_text(ScreenStatus s) {
        switch (s) {
            case ScreenStatus::loaded: return "Gespeicherte Routine";
            case ScreenStatus::missing: return "Keine Datei: Standard";
            case ScreenStatus::invalid: return "Datei defekt: Standard";
            case ScreenStatus::saved: return "Gespeichert";
            case ScreenStatus::save_failed: return "FEHLER: nicht gespeichert";
            case ScreenStatus::none: break;
        }
        return "";
    }

    RoutineMenu menu_;
    Buttons prev_;
    ScreenStatus status_ = ScreenStatus::none;
};

}  // namespace frostbyte
