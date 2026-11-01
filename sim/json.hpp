#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Kleiner JSON-Leser für field.json und sim_params.json (ADR-007: kein Fremdpaket).
// Nur Laptop (Simulation, Tests); Fehler als std::runtime_error.
namespace frostbyte::sim {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }

    bool as_bool() const;
    double as_number() const;
    const std::string& as_string() const;

    // Array oder Objekt: Anzahl Elemente.
    std::size_t size() const;
    const Json& operator[](std::size_t i) const;
    // Objekt: Schlüssel in Dateireihenfolge.
    const std::string& key(std::size_t i) const;
    const Json* find(std::string_view key) const;
    const Json& at(std::string_view key) const;

private:
    friend class JsonParser;
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> items_;
    std::vector<std::string> keys_;
};

Json parse_json(std::string_view text);
Json load_json_file(const std::string& path);

}  // namespace frostbyte::sim
