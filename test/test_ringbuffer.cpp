#include "core/ring_buffer.hpp"
#include "doctest/doctest.h"

using frostbyte::RingBuffer;

TEST_CASE("RingBuffer: leer am Anfang") {
    RingBuffer<int, 4> rb;
    CHECK(rb.empty());
    CHECK(rb.size() == 0);
    CHECK(rb.capacity() == 4);
    int v = 42;
    CHECK_FALSE(rb.pop(v));
    CHECK(v == 42);
}

TEST_CASE("RingBuffer: FIFO-Reihenfolge") {
    RingBuffer<int, 4> rb;
    rb.push(1);
    rb.push(2);
    rb.push(3);
    CHECK(rb.size() == 3);
    int v = 0;
    REQUIRE(rb.pop(v));
    CHECK(v == 1);
    REQUIRE(rb.pop(v));
    CHECK(v == 2);
    REQUIRE(rb.pop(v));
    CHECK(v == 3);
    CHECK(rb.empty());
}

TEST_CASE("RingBuffer: voll überschreibt das Älteste und zählt Verluste") {
    RingBuffer<int, 3> rb;
    for (int i = 1; i <= 5; ++i)
        rb.push(i);
    CHECK(rb.size() == 3);
    CHECK(rb.dropped() == 2);
    CHECK(rb.at(0) == 3);
    CHECK(rb.at(1) == 4);
    CHECK(rb.at(2) == 5);
    int v = 0;
    REQUIRE(rb.pop(v));
    CHECK(v == 3);
}

TEST_CASE("RingBuffer: Umlauf über das Ende des Speichers") {
    RingBuffer<int, 3> rb;
    int v = 0;
    for (int round = 0; round < 10; ++round) {
        rb.push(round * 2);
        rb.push(round * 2 + 1);
        REQUIRE(rb.pop(v));
        CHECK(v == round * 2);
        REQUIRE(rb.pop(v));
        CHECK(v == round * 2 + 1);
    }
    CHECK(rb.empty());
    CHECK(rb.dropped() == 0);
}

TEST_CASE("RingBuffer: clear leert, dropped bleibt") {
    RingBuffer<int, 2> rb;
    rb.push(1);
    rb.push(2);
    rb.push(3);
    rb.clear();
    CHECK(rb.empty());
    CHECK(rb.dropped() == 1);
    rb.push(9);
    CHECK(rb.at(0) == 9);
}
