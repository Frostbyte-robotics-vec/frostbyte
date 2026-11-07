#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>

// Versionsanzeige fuer den Startbildschirm (L-03, Release-Ablauf in docs/release.md). Reine Formatierung ohne PROS:
// Version, Commit und "dirty" liefert der Build (Makefile), src/ reicht sie hier hinein und zeigt den Text im
// niedrig priorisierten UI-Task, nie im Steuer-Task.
namespace frostbyte {

struct BuildInfo {
    const char* version = "";  // z. B. "1.2.0" (Datei version.txt)
    const char* commit = "";   // kurzer Git-Hash; leer, wenn der Build ohne git lief
    bool dirty = false;        // true: Arbeitsverzeichnis hatte ungesicherte Aenderungen
    const char* date = "";     // Build-Datum (__DATE__); leer = weglassen
};

struct BuildInfoText {
    static constexpr std::size_t cols = 26;  // Breite einer LCD-Zeile wie ScreenText
};

// Schreibt "v<version> <commit|-> [dirty|<date>]" nach out (NUL-terminiert, hoechstens size-1 Zeichen). Ist es zu lang,
// wird das Datum zuerst gekuerzt; "dirty" ersetzt das Datum und bleibt sichtbar, solange Platz fuer Version+Commit+dirty da ist.
inline void format_build_info(const BuildInfo& info, char* out, std::size_t size) {
    if (out == nullptr || size == 0)
        return;
    const char* commit = (info.commit != nullptr && info.commit[0] != '\0') ? info.commit : "-";
    const char* tail = info.dirty ? "dirty" : (info.date != nullptr ? info.date : "");
    const char* version = info.version != nullptr ? info.version : "";
    const std::size_t limit = size - 1;
    const std::size_t tail_len = std::strlen(tail);
    const std::size_t head_len = std::strlen(version) + std::strlen(commit) + 3;  // "v", zwei Leerzeichen
    if (info.dirty && tail_len > 0 && head_len + tail_len > limit) {
        // Version und Commit kuerzen, damit "dirty" nicht verloren geht: Commit zuerst, dann Version.
        char head[BuildInfoText::cols + 1];
        std::snprintf(head, sizeof head, "v%s %s", version, commit);
        const std::size_t keep = limit > tail_len + 1 ? limit - tail_len - 1 : 0;
        if (std::strlen(head) > keep)
            head[keep] = '\0';
        std::snprintf(out, size, "%s %s", head, tail);
        return;
    }
    if (tail_len > 0)
        std::snprintf(out, size, "v%s %s %s", version, commit, tail);
    else
        std::snprintf(out, size, "v%s %s", version, commit);
}

}  // namespace frostbyte
