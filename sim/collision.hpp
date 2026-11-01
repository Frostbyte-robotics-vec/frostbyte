#pragma once

#include <optional>

// Kollisionen der Simulation (SIM-05): Roboter als orientiertes Rechteck, Feldelemente als Kreis, Feldrand als Quadrat
// um die Feldmitte. Reine Geometrie; Maße und Positionen kommen vom Aufrufer (Umriss aus sim_params.json, Feldpunkte
// aus field.json). Berührung ohne Überlappung zählt nicht als Kollision.
namespace frostbyte::sim {

struct Vec2 {
    double x_in = 0.0;
    double y_in = 0.0;
};

// Rechteck um (x, y); length_in liegt entlang der Blickrichtung h_deg, width_in quer dazu.
struct Box {
    double x_in = 0.0;
    double y_in = 0.0;
    double length_in = 0.0;
    double width_in = 0.0;
    double h_deg = 0.0;
};

struct Circle {
    double x_in = 0.0;
    double y_in = 0.0;
    double r_in = 0.0;
};

// Kürzeste Verschiebung von a, nach der a und b sich nicht mehr überlappen; leer, wenn kein Kontakt.
std::optional<Vec2> penetration(const Box& a, const Box& b);
std::optional<Vec2> penetration(const Box& a, const Circle& c);

// Verschiebung, die die Box (mit allen Ecken) wieder ins Feld [-field_in/2, field_in/2] bringt; (0, 0), wenn sie drin ist.
Vec2 push_inside_field(const Box& a, double field_in);

}  // namespace frostbyte::sim
