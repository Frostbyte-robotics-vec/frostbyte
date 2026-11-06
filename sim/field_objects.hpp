#pragma once

#include <string>
#include <vector>

#include "sim/collision.hpp"
#include "sim/json.hpp"
#include "sim/objects.hpp"

// Feldobjekte (Cups und Pins am Boden) als Kollisionskörper (SIM-30, ADR-025-Nachtrag). Jedes Objekt ist ein Kreis mit
// Zustand steht/liegt. Erste Überlappung mit einem Roboter kippt ein stehendes Objekt (ein Ereignis, noch keine
// Verschiebung); jede weitere Überlappung schiebt es: das Objekt weicht um (1 - robot_share) der Überlappung, der Roboter
// um robot_share. Objekte stoßen nicht aneinander und bleiben im Feld. Aufnehmen und Setzen bleibt Sache des
// Ereignismodells (SIM-04); gehaltene Objekte kennt dieses Modell nicht. Alle Zahlen aus sim_params.json (feldobjekte).
namespace frostbyte::sim {

struct FieldObject {
    std::string id;
    double x_in = 0.0;
    double y_in = 0.0;
    double r_in = 0.0;
    bool standing = true;  // false = liegt (umgestoßen oder von Anfang an, z. B. Pin am Boden)
    bool fallen = false;   // vom Roboter umgestoßen (nicht von Anfang an liegend): Aufnahme ist schwerer (SIM-34)
    bool taken = false;    // aufgenommen: kein Körper mehr, nicht mehr sichtbar (SIM-34)
};

// Aufnahme vom Boden (SIM-34, ADR-025-Nachtrag): Objekte in Reichweite des Roboterumrisses sind aufnehmbar; ein vom
// Roboter umgestoßenes Objekt kostet Zeit und erhöht die Fehlschlagwahrscheinlichkeit. Ohne diesen Block (reach_in < 0)
// bleibt die Aufnahme wie in SIM-22: kein Objekt wird gesucht oder entfernt.
struct PickupParams {
    double reach_in = -1.0;  // < 0: Brücke aus
    PickMod fallen;          // Zuschlag auf Dauer und Fehlschlag für ein umgestoßenes Objekt
};

struct ObjectContact {
    Vec2 robot_shift;                  // Verschiebung des Roboters, damit er das Objekt nicht durchdringt
    std::vector<std::string> toppled;  // ids der in diesem Schritt umgestoßenen Objekte, in Listenreihenfolge
};

class ObjectField {
public:
    ObjectField() = default;
    // robot_share in [0, 1]: 0 = Objekt gibt ganz nach, 1 = Objekt ist wie eine Wand.
    ObjectField(std::vector<FieldObject> objects, double robot_share);

    // Ein Physikschritt für einen Roboter (Feld [-field_in/2, field_in/2]). Verschiebt Objekte; die Verschiebung des
    // Roboters gibt der Aufrufer weiter (set_position). Objekte in fester Listenreihenfolge, keine Zufallsziehung.
    ObjectContact step(const Box& robot, double field_in);

    // Aufnahme (SIM-34). nearest_pickup: Index des Objekts, das den um reach_in vergrößerten Umriss berührt und dem
    // Roboterzentrum am nächsten liegt (bei Gleichstand das erste der Liste); -1, wenn keins oder die Brücke aus ist.
    // pick_mod: Zuschlag für dieses Objekt (leer, wenn es nicht umgestoßen wurde). take: Objekt entfernen, es stört
    // die Kollision nicht mehr.
    void set_pickup(const PickupParams& p) { pickup_ = p; }
    const PickupParams& pickup() const { return pickup_; }
    int nearest_pickup(const Box& robot) const;
    PickMod pick_mod(int index) const;
    void take(int index);

    const std::vector<FieldObject>& objects() const { return objects_; }
    double robot_share() const { return robot_share_; }

private:
    std::vector<FieldObject> objects_;
    double robot_share_ = 0.5;
    PickupParams pickup_;
};

// Liest params.feldobjekte (radius_in.cup/pin, roboter_anteil, optional aufnahme.reichweite_in/liegt_zusatz_dauer_s/
// liegt_zusatz_fehlschlag) und field.objekte_start: nur Objekte am Boden
// (ebene 0) mit Koordinaten; Pins mit lage "liegt" liegen von Anfang an. Fehlt feldobjekte oder objekte_start: leer.
// Radius <= 0 oder Anteil außerhalb [0, 1]: std::runtime_error.
ObjectField load_field_objects(const Json& params, const Json& field);

}  // namespace frostbyte::sim
