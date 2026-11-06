#include <cstring>
#include <string>

#include "doctest/doctest.h"
#include "hal/sim/sim_storage.hpp"
#include "match/routine_menu.hpp"

using namespace frostbyte;

namespace {

// Eigener Katalog, unabhängig vom echten.
constexpr const char* kNames[] = {"alpha", "beta", "gamma", "nothing"};
constexpr int kCount = 4;
constexpr int kDefault = 3;  // "nothing"

RoutineMenu make() { return RoutineMenu(kNames, kCount, kDefault); }

void put(hal::SimStorage& s, const std::string& text) { REQUIRE(s.write(text.data(), text.size())); }

}  // namespace

TEST_CASE("Routinenmenü: ohne Speicher gilt der sichere Standard") {
    RoutineMenu m = make();
    CHECK(std::strcmp(m.selected(), "nothing") == 0);
    CHECK(m.index() == 3);
    CHECK_FALSE(m.take_save_request());
}

TEST_CASE("Routinenmenü: next und prev laufen im Kreis") {
    RoutineMenu m = make();
    m.handle(MenuKey::next);
    CHECK(std::strcmp(m.selected(), "alpha") == 0);
    m.handle(MenuKey::prev);
    CHECK(std::strcmp(m.selected(), "nothing") == 0);
    m.handle(MenuKey::prev);
    CHECK(std::strcmp(m.selected(), "gamma") == 0);
}

TEST_CASE("Routinenmenü: Speichern wird nur auf confirm angefordert, nicht bei jedem Tastendruck") {
    RoutineMenu m = make();
    m.handle(MenuKey::next);
    m.handle(MenuKey::next);
    CHECK_FALSE(m.take_save_request());
    m.handle(MenuKey::confirm);
    CHECK(m.take_save_request());
    CHECK_FALSE(m.take_save_request());  // Flag wird beim Abholen gelöscht
}

TEST_CASE("Routinenmenü: Auswahl überlebt einen Neustart (neues Menü, derselbe Speicher)") {
    hal::SimStorage store;
    RoutineMenu a = make();
    a.handle(MenuKey::next);
    a.handle(MenuKey::next);  // beta
    a.handle(MenuKey::confirm);
    REQUIRE(a.save(store));

    RoutineMenu b = make();
    CHECK(b.load(store) == LoadResult::loaded);
    CHECK(std::strcmp(b.selected(), "beta") == 0);
}

TEST_CASE("Routinenmenü: fehlende Datei -> missing, Standard bleibt") {
    hal::SimStorage store;
    RoutineMenu m = make();
    CHECK(m.load(store) == LoadResult::missing);
    CHECK(std::strcmp(m.selected(), "nothing") == 0);
}

TEST_CASE("Routinenmenü: kaputte Dateien -> invalid, Standard bleibt") {
    const char* bad[] = {
        "",                                             // leer
        "Müll\n",                                       // keine Kopfzeile
        "# frostbyte-routine v2\nroutine=beta\n",       // falsche Version
        "# frostbyte-routine v1\n",                     // Schlüssel fehlt
        "# frostbyte-routine v1\nroutine=beta",         // abgeschnitten, kein Zeilenende
        "# frostbyte-routine v1\nroutine=unbekannt\n",  // Name nicht im Katalog (z. B. Routine des anderen Roboters)
        "# frostbyte-routine v1\nroutine=\n",           // leerer Name
        "# frostbyte-routine v1\nroutine=beta\nroutine=alpha\n",  // doppelt
        "# frostbyte-routine v1\nroutine=beta\nextra=1\n",        // unbekannter Schlüssel
        "# frostbyte-routine v1\nroutine=beta \n",                // Leerzeichen hinter dem Namen
    };
    for (const char* text : bad) {
        CAPTURE(text);
        hal::SimStorage store;
        put(store, text);
        RoutineMenu m = make();
        CHECK(m.load(store) == LoadResult::invalid);
        CHECK(std::strcmp(m.selected(), "nothing") == 0);
    }
}

TEST_CASE("Routinenmenü: gespeichert wird der Name, nicht die Nummer (Katalog darf sich ändern)") {
    hal::SimStorage store;
    RoutineMenu a = make();
    a.handle(MenuKey::next);
    a.handle(MenuKey::next);  // beta
    REQUIRE(a.save(store));

    constexpr const char* kOther[] = {"zeta", "gamma", "beta", "nothing"};
    RoutineMenu b(kOther, 4, 3);
    CHECK(b.load(store) == LoadResult::loaded);
    CHECK(std::strcmp(b.selected(), "beta") == 0);
}

TEST_CASE("Routinenmenü: Schreibfehler wird gemeldet, die Auswahl im Speicher bleibt") {
    hal::SimStorage store;
    store.set_fail_writes(true);
    RoutineMenu m = make();
    m.handle(MenuKey::next);  // alpha
    CHECK_FALSE(m.save(store));
    CHECK(std::strcmp(m.selected(), "alpha") == 0);
}

TEST_CASE("Routinenmenü: ungültige Konstruktorwerte -> Menü bleibt benutzbar") {
    RoutineMenu m(kNames, kCount, 99);  // Standard-Index außerhalb: auf 0 zurückfallen
    CHECK(m.index() == 0);
    RoutineMenu empty(nullptr, 0, 0);
    empty.handle(MenuKey::next);  // darf nicht abstürzen
    CHECK(std::strcmp(empty.selected(), "") == 0);
}

TEST_CASE("Routinenmenü: der echte Katalog enthält die Pflichtroutinen aus plan.md und nothing als Standard") {
    RoutineMenu m = make_routine_menu();
    CHECK(std::strcmp(m.selected(), "nothing") == 0);
    for (const char* must :
         {"safe_red", "safe_blue", "awp_red", "awp_blue", "skills", "drive_straight", "drive_turns", "nothing"}) {
        bool found = false;
        for (int i = 0; i < routine_catalog_count; ++i)
            found = found || std::strcmp(routine_catalog[i], must) == 0;
        CAPTURE(must);
        CHECK(found);
    }
}
