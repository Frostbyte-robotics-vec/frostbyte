#include "sim/json.hpp"

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace frostbyte::sim {

namespace {

[[noreturn]] void fail(const std::string& what) { throw std::runtime_error("JSON: " + what); }

void append_utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

}  // namespace

class JsonParser {
public:
    explicit JsonParser(std::string_view s) : s_(s) {}

    Json document() {
        Json j = value();
        ws();
        if (i_ != s_.size())
            fail("Zeichen nach dem Ende bei Position " + std::to_string(i_));
        return j;
    }

private:
    void ws() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r'))
            ++i_;
    }
    char peek() {
        ws();
        if (i_ >= s_.size())
            fail("unerwartetes Ende");
        return s_[i_];
    }
    void expect(char c) {
        if (peek() != c)
            fail(std::string("erwartet '") + c + "' bei Position " + std::to_string(i_));
        ++i_;
    }
    void literal(std::string_view word) {
        if (s_.substr(i_, word.size()) != word)
            fail("unbekanntes Wort bei Position " + std::to_string(i_));
        i_ += word.size();
    }

    Json value() {
        Json j;
        const char c = peek();
        if (c == '{') {
            j.type_ = Json::Type::Object;
            ++i_;
            if (peek() == '}') {
                ++i_;
                return j;
            }
            while (true) {
                if (peek() != '"')
                    fail("Schlüssel erwartet bei Position " + std::to_string(i_));
                j.keys_.push_back(string());
                expect(':');
                j.items_.push_back(value());
                if (peek() == ',') {
                    ++i_;
                    continue;
                }
                expect('}');
                return j;
            }
        }
        if (c == '[') {
            j.type_ = Json::Type::Array;
            ++i_;
            if (peek() == ']') {
                ++i_;
                return j;
            }
            while (true) {
                j.items_.push_back(value());
                if (peek() == ',') {
                    ++i_;
                    continue;
                }
                expect(']');
                return j;
            }
        }
        if (c == '"') {
            j.type_ = Json::Type::String;
            j.string_ = string();
            return j;
        }
        if (c == 't') {
            literal("true");
            j.type_ = Json::Type::Bool;
            j.bool_ = true;
            return j;
        }
        if (c == 'f') {
            literal("false");
            j.type_ = Json::Type::Bool;
            return j;
        }
        if (c == 'n') {
            literal("null");
            return j;
        }
        j.type_ = Json::Type::Number;
        j.number_ = number();
        return j;
    }

    // Erst genau die JSON-Zahl abgrenzen (-?\d+(\.\d+)?([eE][+-]?\d+)?), dann mit strtod umwandeln.
    // Nicht from_chars: Apples libc++ kann es für double erst ab Version 20. strtod hängt vom Gebietsschema ab,
    // aber nichts im Programm ruft setlocale auf: es gilt die C-Locale, der Dezimalpunkt ist immer '.' (ADR-007).
    double number() {
        const std::size_t start = i_;
        const auto digits = [&] {
            const std::size_t from = i_;
            while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9')
                ++i_;
            if (i_ == from)
                fail("Zahl erwartet bei Position " + std::to_string(start));
        };
        if (i_ < s_.size() && s_[i_] == '-')
            ++i_;
        digits();
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            digits();
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-'))
                ++i_;
            digits();
        }
        const std::string text(s_.substr(start, i_ - start));
        errno = 0;
        char* end = nullptr;
        const double v = std::strtod(text.c_str(), &end);
        if (end != text.c_str() + text.size() || errno == ERANGE)
            fail("Zahl ausserhalb des Wertebereichs bei Position " + std::to_string(start));
        return v;
    }

    std::string string() {
        expect('"');
        std::string out;
        while (true) {
            if (i_ >= s_.size())
                fail("Zeichenkette ohne Ende");
            const char c = s_[i_++];
            if (c == '"')
                return out;
            if (c != '\\') {
                out += c;  // UTF-8-Bytes unverändert übernehmen
                continue;
            }
            if (i_ >= s_.size())
                fail("Escape ohne Zeichen");
            const char e = s_[i_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (i_ + 4 > s_.size())
                        fail("\\u ohne 4 Hexziffern");
                    const auto [p, ec] = std::from_chars(s_.data() + i_, s_.data() + i_ + 4, cp, 16);
                    if (ec != std::errc() || p != s_.data() + i_ + 4)
                        fail("\\u ohne 4 Hexziffern");
                    i_ += 4;
                    append_utf8(out, cp);
                    break;
                }
                default: fail(std::string("unbekanntes Escape \\") + e);
            }
        }
    }

    std::string_view s_;
    std::size_t i_ = 0;
};

bool Json::as_bool() const {
    if (type_ != Type::Bool)
        fail("Wahrheitswert erwartet");
    return bool_;
}

double Json::as_number() const {
    if (type_ != Type::Number)
        fail("Zahl erwartet");
    return number_;
}

const std::string& Json::as_string() const {
    if (type_ != Type::String)
        fail("Zeichenkette erwartet");
    return string_;
}

std::size_t Json::size() const {
    if (type_ != Type::Array && type_ != Type::Object)
        fail("Array oder Objekt erwartet");
    return items_.size();
}

const Json& Json::operator[](std::size_t i) const {
    if (i >= size())
        fail("Index " + std::to_string(i) + " außerhalb");
    return items_[i];
}

const std::string& Json::key(std::size_t i) const {
    if (type_ != Type::Object || i >= keys_.size())
        fail("Schlüssel " + std::to_string(i) + " fehlt");
    return keys_[i];
}

const Json* Json::find(std::string_view key) const {
    if (type_ != Type::Object)
        fail("Objekt erwartet");
    for (std::size_t i = 0; i < keys_.size(); ++i)
        if (keys_[i] == key)
            return &items_[i];
    return nullptr;
}

const Json& Json::at(std::string_view key) const {
    const Json* j = find(key);
    if (!j)
        fail("Schlüssel \"" + std::string(key) + "\" fehlt");
    return *j;
}

Json parse_json(std::string_view text) { return JsonParser(text).document(); }

Json load_json_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Datei nicht lesbar: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse_json(ss.str());
}

}  // namespace frostbyte::sim
