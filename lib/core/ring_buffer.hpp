#pragma once

#include <array>
#include <cstddef>

// Ringpuffer fester Größe ohne dynamischen Speicher. Ist er voll, wird der älteste Eintrag überschrieben
// und dropped() erhöht. Nicht thread-sicher: die Übergabe zwischen Tasks kommt mit dem Logger-Ticket.
namespace frostbyte {

template <typename T, std::size_t N>
class RingBuffer {
    static_assert(N > 0, "Kapazität muss größer 0 sein");

public:
    void push(const T& v) {
        if (size_ == N) {
            head_ = (head_ + 1) % N;
            --size_;
            ++dropped_;
        }
        data_[(head_ + size_) % N] = v;
        ++size_;
    }

    bool pop(T& out) {
        if (size_ == 0)
            return false;
        out = data_[head_];
        head_ = (head_ + 1) % N;
        --size_;
        return true;
    }

    // i = 0 ist der älteste Eintrag; i < size() vorausgesetzt.
    const T& at(std::size_t i) const { return data_[(head_ + i) % N]; }

    void clear() {
        head_ = 0;
        size_ = 0;
    }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    static constexpr std::size_t capacity() { return N; }
    std::size_t dropped() const { return dropped_; }

private:
    std::array<T, N> data_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    std::size_t dropped_ = 0;
};

}  // namespace frostbyte
