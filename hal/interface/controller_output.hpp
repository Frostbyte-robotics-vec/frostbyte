#pragma once

// Ausgaben an den Fahrer-Controller: Vibration und Textzeilen. Keine PROS-Header (CLAUDE.md); die PROS-Seite liegt in
// hal/pros/pros_controller_output.hpp. Nur außerhalb des Steuer-Tasks benutzen (der Controller nimmt höchstens alle 50 ms
// einen Befehl an und blockiert sonst).
namespace frostbyte::hal {

class ControllerOutput {
public:
    virtual ~ControllerOutput() = default;

    // Muster aus '.' (kurz), '-' (lang) und ' ' (Pause), wie bei PROS. Rückgabe false, wenn der Controller es nicht annahm.
    virtual bool rumble(const char* pattern) = 0;

    // Schreibt text (höchstens 15 Zeichen) in Zeile 0..2 des Controllers. Rückgabe false, wenn er es nicht annahm.
    virtual bool print(int line, const char* text) = 0;
};

}  // namespace frostbyte::hal
