#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Kalibriertabelle mit N Punkten (x -> y) und monotoner kubischer Interpolation (Fritsch-Carlson): keine Überschwinger
// zwischen den Punkten, kein dynamischer Speicher. Außerhalb der Tabelle wird auf die Endpunkte begrenzt, nie
// extrapoliert. Beide Spalten müssen endlich und streng steigend sein; sonst bleibt die Tabelle ungültig (valid() = false).
namespace frostbyte {

template <int N>
class MonotoneTable {
    static_assert(N >= 2, "mindestens zwei Punkte");

public:
    MonotoneTable() = default;

    // false und ungültig, wenn die Punkte nicht endlich oder nicht streng steigend sind.
    bool set(const std::array<double, N>& x, const std::array<double, N>& y) {
        valid_ = false;
        for (int i = 0; i < N; ++i) {
            if (!std::isfinite(x[i]) || !std::isfinite(y[i]))
                return false;
            if (i > 0 && (x[i] <= x[i - 1] || y[i] <= y[i - 1]))
                return false;
        }
        x_ = x;
        y_ = y;
        std::array<double, N - 1> d{};
        for (int i = 0; i < N - 1; ++i)
            d[i] = (y_[i + 1] - y_[i]) / (x_[i + 1] - x_[i]);
        m_[0] = d[0];
        m_[N - 1] = d[N - 2];
        for (int i = 1; i < N - 1; ++i)
            m_[i] = (d[i - 1] + d[i]) / 2.0;  // alle Sekanten > 0, also kein Vorzeichenwechsel
        for (int i = 0; i < N - 1; ++i) {
            const double a = m_[i] / d[i], b = m_[i + 1] / d[i];
            const double r = a * a + b * b;
            if (r > 9.0) {
                const double tau = 3.0 / std::sqrt(r);
                m_[i] = tau * a * d[i];
                m_[i + 1] = tau * b * d[i];
            }
        }
        valid_ = true;
        return true;
    }

    bool valid() const { return valid_; }
    double x_min() const { return x_[0]; }
    double x_max() const { return x_[N - 1]; }
    double y_min() const { return y_[0]; }
    double y_max() const { return y_[N - 1]; }

    // y(x), außerhalb auf die Endpunkte begrenzt. Ungültige Tabelle: 0.
    double eval(double x) const {
        if (!valid_)
            return 0.0;
        if (x <= x_[0])
            return y_[0];
        if (x >= x_[N - 1])
            return y_[N - 1];
        int i = 0;
        while (i < N - 2 && x > x_[i + 1])
            ++i;
        const double h = x_[i + 1] - x_[i];
        const double t = (x - x_[i]) / h;
        const double t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * y_[i] + (t3 - 2 * t2 + t) * h * m_[i] + (-2 * t3 + 3 * t2) * y_[i + 1] +
               (t3 - t2) * h * m_[i + 1];
    }

    // x(y) durch Halbieren (die Tabelle ist streng steigend), außerhalb auf die Endpunkte begrenzt. Feste Schrittzahl.
    double invert(double y) const {
        if (!valid_)
            return 0.0;
        if (y <= y_[0])
            return x_[0];
        if (y >= y_[N - 1])
            return x_[N - 1];
        double lo = x_[0], hi = x_[N - 1];
        for (int k = 0; k < 50; ++k) {
            const double mid = (lo + hi) / 2.0;
            (eval(mid) < y ? lo : hi) = mid;
        }
        return (lo + hi) / 2.0;
    }

private:
    std::array<double, N> x_{};
    std::array<double, N> y_{};
    std::array<double, N> m_{};
    bool valid_ = false;
};

}  // namespace frostbyte
