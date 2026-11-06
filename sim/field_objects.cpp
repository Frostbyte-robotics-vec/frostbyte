#include "sim/field_objects.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace frostbyte::sim {

ObjectField::ObjectField(std::vector<FieldObject> objects, double robot_share)
    : objects_(std::move(objects)), robot_share_(robot_share) {}

ObjectContact ObjectField::step(const Box& robot, double field_in) {
    ObjectContact out;
    Box box = robot;
    const double half = field_in / 2.0;
    for (FieldObject& o : objects_) {
        const auto mtv = penetration(box, Circle{o.x_in, o.y_in, o.r_in});
        if (!mtv)
            continue;
        if (o.standing) {
            o.standing = false;
            out.toppled.push_back(o.id);
            continue;
        }
        // Objekt weicht um (1 - Anteil) entgegen der Verschiebung des Roboters, bleibt aber ganz im Feld.
        const double want_x = o.x_in - mtv->x_in * (1.0 - robot_share_);
        const double want_y = o.y_in - mtv->y_in * (1.0 - robot_share_);
        const double new_x = std::clamp(want_x, -half + o.r_in, half - o.r_in);
        const double new_y = std::clamp(want_y, -half + o.r_in, half - o.r_in);
        // Was das Objekt nicht abfängt (Wand), trägt der Roboter.
        const Vec2 shift{mtv->x_in * robot_share_ + (want_x - new_x) * -1.0, mtv->y_in * robot_share_ + (want_y - new_y) * -1.0};
        o.x_in = new_x;
        o.y_in = new_y;
        out.robot_shift.x_in += shift.x_in;
        out.robot_shift.y_in += shift.y_in;
        box.x_in += shift.x_in;
        box.y_in += shift.y_in;
    }
    return out;
}

namespace {

double positive(const Json& j, const char* what) {
    const double v = j.as_number();
    if (!(v > 0.0))
        throw std::runtime_error(std::string("sim_params feldobjekte.") + what + " muss > 0 sein");
    return v;
}

}  // namespace

ObjectField load_field_objects(const Json& params, const Json& field) {
    const Json* block = params.find("feldobjekte");
    const Json* start = field.find("objekte_start");
    if (!block)
        return {};
    const Json& radius = block->at("radius_in");
    const double r_cup = positive(radius.at("cup"), "radius_in.cup");
    const double r_pin = positive(radius.at("pin"), "radius_in.pin");
    const double share = block->at("roboter_anteil").as_number();
    if (!(share >= 0.0 && share <= 1.0))
        throw std::runtime_error("sim_params feldobjekte.roboter_anteil muss in [0, 1] liegen");
    std::vector<FieldObject> list;
    if (start) {
        const auto add = [&](const char* key, double r) {
            const Json* items = start->find(key);
            if (!items)
                return;
            for (std::size_t i = 0; i < items->size(); ++i) {
                const Json& e = (*items)[i];
                const Json* x = e.find("x_in");
                const Json* y = e.find("y_in");
                const Json* level = e.find("ebene");
                if (!x || !y || x->is_null() || y->is_null())
                    continue;  // Platzhalter (null-Koordinaten) nie als Hindernis
                if (level && !level->is_null() && level->as_number() != 0.0)
                    continue;  // liegt oben auf Goal oder Cup, kein Bodenkörper
                const Json* lage = e.find("lage");
                const bool standing = !(lage && lage->type() == Json::Type::String && lage->as_string() == "liegt");
                list.push_back(FieldObject{e.at("id").as_string(), x->as_number(), y->as_number(), r, standing});
            }
        };
        add("cups", r_cup);
        add("pins", r_pin);
    }
    return ObjectField(std::move(list), share);
}

}  // namespace frostbyte::sim
