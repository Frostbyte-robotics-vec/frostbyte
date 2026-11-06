#include <cstring>
#include <string>

#include "doctest/doctest.h"
#include "hal/sim/sim_storage.hpp"
#include "match/routine_screen.hpp"

using namespace frostbyte;

namespace {

// Eigener Katalog, unabhängig vom echten.
constexpr const char* kNames[] = {"alpha", "beta", "gamma", "delta", "nothing"};
constexpr int kCount = 5;
constexpr int kDefault = 4;

RoutineMenu make() { return RoutineMenu(kNames, kCount, kDefault); }

Buttons only(bool left, bool center, bool right) { return Buttons{left, center, right}; }

void put(hal::SimStorage& s, const std::string& text) { REQUIRE(s.write(text.data(), text.size())); }

}  // namespace

TEST_CASE("Routinen-Bildschirm: Start ohne Datei zeigt Standard und sagt es") {
    hal::SimStorage store;
    RoutineScreen screen(make());
    screen.init(store);
    CHECK(screen.status() == ScreenStatus::missing);
    CHECK(std::strcmp(screen.selected(), "nothing") == 0);
    ScreenText t;
    screen.render(t);
    CHECK(std::string(t.line[ScreenText::status_line]).find("Standard") != std::string::npos);
}

TEST_CASE("Routinen-Bildschirm: kaputte Datei gibt Standard und eigene Meldung") {
    hal::SimStorage store;
    put(store, "irgendwas kaputtes\n");
    RoutineScreen screen(make());
    screen.init(store);
    CHECK(screen.status() == ScreenStatus::invalid);
    CHECK(std::strcmp(screen.selected(), "nothing") == 0);
}

TEST_CASE("Routinen-Bildschirm: gültige Datei wird geladen") {
    hal::SimStorage store;
    RoutineMenu seed = make();
    seed.handle(MenuKey::next);  // alpha
    REQUIRE(seed.save(store));
    RoutineScreen screen(make());
    screen.init(store);
    CHECK(screen.status() == ScreenStatus::loaded);
    CHECK(std::strcmp(screen.selected(), "alpha") == 0);
}

TEST_CASE("Routinen-Bildschirm: Tasten links/rechts wählen, Mitte speichert, nur einmal je Druck") {
    hal::SimStorage store;
    RoutineScreen screen(make());
    screen.init(store);
    screen.update(store, only(false, false, true), true);  // rechts: alpha
    CHECK(std::strcmp(screen.selected(), "alpha") == 0);
    screen.update(store, only(false, false, true), true);  // gehalten: keine zweite Auswahl
    CHECK(std::strcmp(screen.selected(), "alpha") == 0);
    screen.update(store, only(false, false, false), true);  // losgelassen
    screen.update(store, only(true, false, false), true);   // links: zurück auf nothing
    CHECK(std::strcmp(screen.selected(), "nothing") == 0);
    screen.update(store, only(false, false, false), true);
    screen.update(store, only(false, false, true), true);  // alpha
    screen.update(store, only(false, false, false), true);
    screen.update(store, only(false, true, false), true);  // Mitte: speichern
    CHECK(screen.status() == ScreenStatus::saved);
    RoutineMenu check = make();
    CHECK(check.load(store) == LoadResult::loaded);
    CHECK(std::strcmp(check.selected(), "alpha") == 0);
}

TEST_CASE("Routinen-Bildschirm: Speichern ohne Karte meldet Fehler, Auswahl bleibt im Speicher") {
    hal::SimStorage store;
    store.set_fail_writes(true);
    RoutineScreen screen(make());
    screen.init(store);
    screen.update(store, only(false, false, true), true);
    screen.update(store, only(false, false, false), true);
    screen.update(store, only(false, true, false), true);
    CHECK(screen.status() == ScreenStatus::save_failed);
    CHECK(std::strcmp(screen.selected(), "alpha") == 0);
    ScreenText t;
    screen.render(t);
    CHECK(std::string(t.line[ScreenText::status_line]).find("FEHLER") != std::string::npos);
}

TEST_CASE("Routinen-Bildschirm: nicht editierbar (Match läuft) ignoriert alle Tasten") {
    hal::SimStorage store;
    RoutineScreen screen(make());
    screen.init(store);
    screen.update(store, only(false, false, true), false);
    screen.update(store, only(false, false, false), false);
    screen.update(store, only(false, true, false), false);
    CHECK(std::strcmp(screen.selected(), "nothing") == 0);
    CHECK(screen.status() == ScreenStatus::missing);  // nichts gespeichert
    char buf[96];
    CHECK(store.read(buf, sizeof(buf)) < 0);  // Datei existiert nicht
}

TEST_CASE("Routinen-Bildschirm: gleichzeitiges Drücken mehrerer Tasten löst genau eine Aktion aus, Mitte hat Vorrang") {
    hal::SimStorage store;
    RoutineScreen screen(make());
    screen.init(store);
    screen.update(store, only(true, true, true), true);
    CHECK(screen.status() == ScreenStatus::saved);
    CHECK(std::strcmp(screen.selected(), "nothing") == 0);  // nichts verstellt
}

TEST_CASE("Routinen-Bildschirm: Marke steht genau vor der gewählten Routine") {
    hal::SimStorage store;
    RoutineScreen screen(make());
    screen.init(store);
    screen.update(store, only(false, false, true), true);  // alpha (Index 0)
    ScreenText t;
    screen.render(t);
    int marked = 0;
    for (int i = 0; i < ScreenText::menu_rows; ++i) {
        const std::string row = t.line[i];
        if (row.rfind("> ", 0) == 0) {
            ++marked;
            CHECK(row.find("alpha") != std::string::npos);
        }
        CHECK(row.size() <= ScreenText::cols);
    }
    CHECK(marked == 1);
}

TEST_CASE("Routinen-Bildschirm: Fenster am Ende des Katalogs bleibt im Katalog") {
    hal::SimStorage store;
    RoutineScreen screen(make());  // Standard: letzter Eintrag
    screen.init(store);
    ScreenText t;
    screen.render(t);
    bool found = false;
    for (int i = 0; i < ScreenText::menu_rows; ++i)
        if (std::string(t.line[i]).find("nothing") != std::string::npos) {
            found = true;
            CHECK(std::string(t.line[i]).rfind("> ", 0) == 0);
        }
    CHECK(found);
}
