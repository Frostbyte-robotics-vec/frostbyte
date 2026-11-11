#pragma once

#include <array>
#include <atomic>
#include <cstddef>

// Warteschlange fester Größe für genau einen Erzeuger und genau einen Verbraucher (zwei Tasks), ohne Sperre und ohne
// dynamischen Speicher. Der Erzeuger (Steuer-Task) blockiert nie: ist sie voll, fällt der neue Eintrag weg und
// dropped() zählt ihn. Der Verbraucher (Logger-Task) ruft nur try_pop() auf. Fasst N Einträge.
namespace frostbyte {

template <typename T, std::size_t N>
class SpscQueue {
    static_assert(N > 0, "Kapazität muss größer 0 sein");
    static constexpr std::size_t slots = N + 1;  // ein Platz bleibt frei, sonst sind voll und leer nicht unterscheidbar

public:
    // Nur vom Erzeuger aufrufen.
    bool try_push(const T& v) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next = (tail + 1) % slots;
        if (next == head_.load(std::memory_order_acquire)) {
            dropped_.store(dropped_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
            return false;
        }
        data_[tail] = v;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    // Nur vom Verbraucher aufrufen. Ist die Schlange leer, bleibt out unverändert.
    bool try_pop(T& out) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire))
            return false;
        out = data_[head];
        head_.store((head + 1) % slots, std::memory_order_release);
        return true;
    }

    // Von beiden Seiten lesbar (Momentaufnahme).
    std::size_t size() const {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        return (tail + slots - head) % slots;
    }
    bool empty() const { return size() == 0; }
    static constexpr std::size_t capacity() { return N; }
    std::size_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    std::array<T, slots> data_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
    std::atomic<std::size_t> dropped_{0};
};

}  // namespace frostbyte
