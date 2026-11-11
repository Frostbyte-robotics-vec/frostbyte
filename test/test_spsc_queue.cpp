#include <thread>

#include "core/spsc_queue.hpp"
#include "doctest/doctest.h"

using frostbyte::SpscQueue;

TEST_CASE("SpscQueue: leer, pop liefert nichts und lässt den Wert in Ruhe") {
    SpscQueue<int, 4> q;
    int v = 42;
    CHECK(q.empty());
    CHECK_FALSE(q.try_pop(v));
    CHECK(v == 42);
}

TEST_CASE("SpscQueue: FIFO-Reihenfolge, auch über den Umlauf hinweg") {
    SpscQueue<int, 4> q;
    int next_in = 0, next_out = 0, v = 0;
    for (int round = 0; round < 10; ++round) {
        CHECK(q.try_push(next_in++));
        CHECK(q.try_push(next_in++));
        CHECK(q.try_push(next_in++));
        REQUIRE(q.try_pop(v));
        CHECK(v == next_out++);
        REQUIRE(q.try_pop(v));
        CHECK(v == next_out++);
        REQUIRE(q.try_pop(v));
        CHECK(v == next_out++);
    }
    CHECK(q.empty());
}

TEST_CASE("SpscQueue: voll -> neuester Eintrag fällt weg, dropped zählt, nichts wird überschrieben") {
    SpscQueue<int, 3> q;
    CHECK(q.try_push(1));
    CHECK(q.try_push(2));
    CHECK(q.try_push(3));
    CHECK_FALSE(q.try_push(4));
    CHECK_FALSE(q.try_push(5));
    CHECK(q.dropped() == 2);
    CHECK(q.size() == 3);
    int v = 0;
    REQUIRE(q.try_pop(v));
    CHECK(v == 1);
    CHECK(q.try_push(6));  // nach einem pop wieder Platz
    REQUIRE(q.try_pop(v));
    CHECK(v == 2);
    REQUIRE(q.try_pop(v));
    CHECK(v == 3);
    REQUIRE(q.try_pop(v));
    CHECK(v == 6);
    CHECK(q.dropped() == 2);
}

TEST_CASE("SpscQueue: Erzeuger und Verbraucher in zwei Threads, Reihenfolge bleibt, Verlust wird gezählt") {
    SpscQueue<int, 8> q;
    constexpr int total = 20000;
    int received = 0, last = -1;
    bool ordered = true;
    std::thread consumer([&] {
        int v = 0;
        // Der Erzeuger blockiert nie; der Verbraucher liest, bis er alle Zahlen oder den Schlusswert sieht.
        while (true) {
            if (q.try_pop(v)) {
                if (v == total)
                    break;
                if (v <= last)
                    ordered = false;
                last = v;
                ++received;
            }
        }
    });
    for (int i = 0; i < total; ++i)
        (void)q.try_push(i);
    const int data_dropped = static_cast<int>(q.dropped());
    while (!q.try_push(total)) {  // Schlusswert muss ankommen (diese Versuche zählen nicht als Datenverlust)
    }
    consumer.join();
    CHECK(ordered);
    CHECK(received + data_dropped == total);
}
