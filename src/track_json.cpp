#include "physics_control_studio/track_json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace physics_control_studio {
namespace {

struct JsonValue {
    enum class Kind { Null, Boolean, Number, String, Array, Object };
    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    bool parse(JsonValue& output, std::string& error) {
        skip_space();
        if (!parse_value(output)) {
            error = error_.empty() ? "invalid JSON" : error_;
            return false;
        }
        skip_space();
        if (position_ != input_.size()) {
            error = "trailing JSON data";
            return false;
        }
        return true;
    }

private:
    void skip_space() {
        while (position_ < input_.size()) {
            const char value = input_[position_];
            if (value != ' ' && value != '\t' && value != '\r' && value != '\n') break;
            ++position_;
        }
    }

    bool consume(char value) {
        skip_space();
        if (position_ >= input_.size() || input_[position_] != value) return false;
        ++position_;
        return true;
    }

    bool parse_value(JsonValue& output) {
        skip_space();
        if (position_ >= input_.size()) return fail("unexpected end of JSON");
        switch (input_[position_]) {
        case '{': return parse_object(output);
        case '[': return parse_array(output);
        case '"':
            output.kind = JsonValue::Kind::String;
            return parse_string(output.string);
        case 't': return parse_literal("true", JsonValue::Kind::Boolean, output, true);
        case 'f': return parse_literal("false", JsonValue::Kind::Boolean, output, false);
        case 'n': return parse_literal("null", JsonValue::Kind::Null, output, false);
        default: return parse_number(output);
        }
    }

    bool parse_literal(
        std::string_view literal,
        JsonValue::Kind kind,
        JsonValue& output,
        bool boolean) {
        if (input_.substr(position_, literal.size()) != literal)
            return fail("invalid JSON literal");
        position_ += literal.size();
        output.kind = kind;
        output.boolean = boolean;
        return true;
    }

    bool parse_string(std::string& output) {
        if (!consume('"')) return fail("expected string");
        output.clear();
        while (position_ < input_.size()) {
            const char value = input_[position_++];
            if (value == '"') return true;
            if (static_cast<unsigned char>(value) < 0x20) return fail("control character in string");
            if (value != '\\') {
                output.push_back(value);
                continue;
            }
            if (position_ >= input_.size()) return fail("unfinished string escape");
            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            default: return fail("unsupported string escape");
            }
        }
        return fail("unterminated string");
    }

    bool parse_number(JsonValue& output) {
        const std::size_t start = position_;
        if (position_ < input_.size() && input_[position_] == '-') ++position_;
        while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
            ++position_;
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                ++position_;
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
                ++position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                ++position_;
        }
        if (position_ == start) return fail("expected JSON value");
        const std::string token(input_.substr(start, position_ - start));
        char* end = nullptr;
        const double number = std::strtod(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0' || !std::isfinite(number))
            return fail("invalid JSON number");
        output.kind = JsonValue::Kind::Number;
        output.number = number;
        return true;
    }

    bool parse_array(JsonValue& output) {
        if (!consume('[')) return false;
        output.kind = JsonValue::Kind::Array;
        output.array.clear();
        skip_space();
        if (consume(']')) return true;
        for (;;) {
            JsonValue item;
            if (!parse_value(item)) return false;
            output.array.push_back(std::move(item));
            if (consume(']')) return true;
            if (!consume(',')) return fail("expected comma in array");
        }
    }

    bool parse_object(JsonValue& output) {
        if (!consume('{')) return false;
        output.kind = JsonValue::Kind::Object;
        output.object.clear();
        skip_space();
        if (consume('}')) return true;
        for (;;) {
            std::string key;
            if (!parse_string(key)) return false;
            if (!consume(':')) return fail("expected colon in object");
            JsonValue value;
            if (!parse_value(value)) return false;
            if (!output.object.emplace(std::move(key), std::move(value)).second)
                return fail("duplicate object key");
            if (consume('}')) return true;
            if (!consume(',')) return fail("expected comma in object");
        }
    }

    bool fail(const char* message) {
        if (error_.empty()) {
            error_ = message;
            error_ += " at byte ";
            error_ += std::to_string(position_);
        }
        return false;
    }

    std::string_view input_;
    std::size_t position_ = 0;
    std::string error_;
};

const JsonValue* field(const JsonValue& object, const char* name) {
    if (object.kind != JsonValue::Kind::Object) return nullptr;
    const auto found = object.object.find(name);
    return found == object.object.end() ? nullptr : &found->second;
}

bool read_bool(const JsonValue& object, const char* name, bool& output) {
    const JsonValue* value = field(object, name);
    if (value == nullptr || value->kind != JsonValue::Kind::Boolean) return false;
    output = value->boolean;
    return true;
}

bool read_number(const JsonValue& object, const char* name, float& output) {
    const JsonValue* value = field(object, name);
    if (value == nullptr || value->kind != JsonValue::Kind::Number ||
        value->number < -std::numeric_limits<float>::max() ||
        value->number > std::numeric_limits<float>::max()) {
        return false;
    }
    output = static_cast<float>(value->number);
    return std::isfinite(output) != 0;
}

bool read_u32(const JsonValue& object, const char* name, std::uint32_t& output) {
    const JsonValue* value = field(object, name);
    if (value == nullptr || value->kind != JsonValue::Kind::Number ||
        value->number < 0.0 || value->number > std::numeric_limits<std::uint32_t>::max() ||
        std::floor(value->number) != value->number) {
        return false;
    }
    output = static_cast<std::uint32_t>(value->number);
    return true;
}

bool read_vec3(const JsonValue& object, const char* name, Vec3& output) {
    const JsonValue* value = field(object, name);
    if (value == nullptr || value->kind != JsonValue::Kind::Array || value->array.size() != 3)
        return false;
    float* components[] = {&output.x, &output.y, &output.z};
    for (std::size_t index = 0; index < 3; ++index) {
        if (value->array[index].kind != JsonValue::Kind::Number ||
            !std::isfinite(value->array[index].number) ||
            value->array[index].number < -std::numeric_limits<float>::max() ||
            value->array[index].number > std::numeric_limits<float>::max()) {
            return false;
        }
        *components[index] = static_cast<float>(value->array[index].number);
    }
    return true;
}

bool read_target(const JsonValue& object, TargetSelection& target) {
    std::uint32_t kind = 0;
    std::uint32_t group_mask = 0;
    if (!read_u32(object, "kind", kind) || kind > static_cast<std::uint32_t>(TargetKind::CustomSet) ||
        !read_u32(object, "index", target.index) ||
        !read_u32(object, "collision_group_mask", group_mask) || group_mask > 0xffff) {
        return false;
    }
    target.kind = static_cast<TargetKind>(kind);
    target.collision_group_mask = static_cast<std::uint16_t>(group_mask);
    const JsonValue* bodies = field(object, "rigid_bodies");
    if (bodies == nullptr || bodies->kind != JsonValue::Kind::Array) return false;
    target.rigid_body_indices.clear();
    for (const auto& body : bodies->array) {
        if (body.kind != JsonValue::Kind::Number || body.number < 0.0 || body.number >= 65'536.0 ||
            std::floor(body.number) != body.number) {
            return false;
        }
        target.rigid_body_indices.push_back(static_cast<std::uint32_t>(body.number));
    }
    return true;
}

bool read_target_group(const JsonValue& value, TargetGroup& group) {
    if (value.kind != JsonValue::Kind::Object) return false;
    const JsonValue* name = field(value, "name");
    const JsonValue* target = field(value, "target");
    if (name == nullptr || name->kind != JsonValue::Kind::String || name->string.empty() ||
        name->string.size() > 256 || target == nullptr || !read_target(*target, group.target)) {
        return false;
    }
    PhysicsSettings settings;
    settings.wind_target = group.target;
    settings.damping_target = group.target;
    settings.gravity_target = group.target;
    if (!validate_physics_settings(settings)) return false;
    group.name = name->string;
    return true;
}

bool read_snapshot(const JsonValue& object, ControlSnapshot& snapshot) {
    const JsonValue* wind = field(object, "wind");
    const JsonValue* physics = field(object, "physics");
    if (wind == nullptr || physics == nullptr) return false;
    std::uint32_t field_type = 0;
    std::uint32_t noise_type = 0;
    std::uint32_t falloff_type = static_cast<std::uint32_t>(
        snapshot.wind.falloff_type);
    std::uint32_t group_mask = 0;
    if (!read_bool(*wind, "enabled", snapshot.wind.enabled) ||
        !read_u32(*wind, "field_type", field_type) || field_type > static_cast<std::uint32_t>(WindFieldType::Shear) ||
        !read_u32(*wind, "noise_type", noise_type) || noise_type > static_cast<std::uint32_t>(WindNoiseType::RandomGust) ||
        !read_vec3(*wind, "direction", snapshot.wind.direction) ||
        !read_number(*wind, "strength", snapshot.wind.strength) ||
        !read_number(*wind, "gust", snapshot.wind.gust) ||
        !read_number(*wind, "turbulence", snapshot.wind.turbulence) ||
        !read_number(*wind, "frequency", snapshot.wind.frequency) ||
        !read_vec3(*wind, "center", snapshot.wind.center) ||
        !read_u32(*wind, "collision_group_mask", group_mask) || group_mask > 0xffff ||
        !read_number(*wind, "maximum_speed", snapshot.wind.maximum_speed)) {
        return false;
    }
    if ((field(*wind, "falloff_type") != nullptr &&
            (!read_u32(*wind, "falloff_type", falloff_type) ||
             falloff_type > static_cast<std::uint32_t>(WindFalloffType::Quadratic))) ||
        (field(*wind, "local_enabled") != nullptr &&
            !read_bool(*wind, "local_enabled", snapshot.wind.local_enabled)) ||
        (field(*wind, "controller_enabled") != nullptr &&
            !read_bool(*wind, "controller_enabled", snapshot.wind.controller_enabled)) ||
        (field(*wind, "radius") != nullptr &&
            !read_number(*wind, "radius", snapshot.wind.radius)) ||
        (field(*wind, "core_ratio") != nullptr &&
            !read_number(*wind, "core_ratio", snapshot.wind.core_ratio))) {
        return false;
    }
    snapshot.wind.field_type = static_cast<WindFieldType>(field_type);
    snapshot.wind.noise_type = static_cast<WindNoiseType>(noise_type);
    snapshot.wind.falloff_type = static_cast<WindFalloffType>(falloff_type);
    snapshot.wind.collision_group_mask = static_cast<std::uint16_t>(group_mask);
    normalize_wind_settings(snapshot.wind);

    const JsonValue* legacy_target = field(*physics, "target");
    const JsonValue* wind_target = field(*physics, "wind_target");
    const JsonValue* damping_target = field(*physics, "damping_target");
    const JsonValue* gravity_target = field(*physics, "gravity_target");
    if (!read_bool(*physics, "damping_enabled", snapshot.physics.damping_enabled) ||
        !read_number(*physics, "linear_damping", snapshot.physics.linear_damping) ||
        !read_number(*physics, "angular_damping", snapshot.physics.angular_damping) ||
        !read_bool(*physics, "gravity_enabled", snapshot.physics.gravity_enabled) ||
        !read_vec3(*physics, "gravity_direction", snapshot.physics.gravity_direction) ||
        !read_number(*physics, "gravity_acceleration", snapshot.physics.gravity_acceleration)) {
        return false;
    }
    if (wind_target != nullptr || damping_target != nullptr || gravity_target != nullptr) {
        if (wind_target == nullptr || damping_target == nullptr || gravity_target == nullptr ||
            !read_target(*wind_target, snapshot.physics.wind_target) ||
            !read_target(*damping_target, snapshot.physics.damping_target) ||
            !read_target(*gravity_target, snapshot.physics.gravity_target)) {
            return false;
        }
    } else {
        TargetSelection target;
        if (legacy_target == nullptr || !read_target(*legacy_target, target)) return false;
        snapshot.physics.wind_target = target;
        snapshot.physics.damping_target = target;
        snapshot.physics.gravity_target = std::move(target);
    }
    return validate_wind_settings(snapshot.wind) && validate_physics_settings(snapshot.physics);
}

void write_vec3(std::ostringstream& output, const Vec3& value) {
    output << '[' << value.x << ", " << value.y << ", " << value.z << ']';
}

void write_json_string(std::ostringstream& output, std::string_view value) {
    output << '"';
    for (const char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(character) >= 0x20) output << character;
            break;
        }
    }
    output << '"';
}

void write_target(std::ostringstream& output, const TargetSelection& target, int indent) {
    const std::string pad(static_cast<std::size_t>(indent), ' ');
    output << "{\n" << pad << "  \"kind\": " << static_cast<unsigned>(target.kind)
           << ",\n" << pad << "  \"index\": " << target.index
           << ",\n" << pad << "  \"collision_group_mask\": " << target.collision_group_mask
           << ",\n" << pad << "  \"rigid_bodies\": [";
    for (std::size_t index = 0; index < target.rigid_body_indices.size(); ++index) {
        if (index != 0) output << ", ";
        output << target.rigid_body_indices[index];
    }
    output << "]\n" << pad << '}';
}

void write_snapshot(std::ostringstream& output, const ControlSnapshot& snapshot, int indent) {
    const std::string pad(static_cast<std::size_t>(indent), ' ');
    output << "{\n" << pad << "  \"wind\": {\n"
           << pad << "    \"enabled\": " << (snapshot.wind.enabled ? "true" : "false") << ",\n"
           << pad << "    \"field_type\": " << static_cast<unsigned>(snapshot.wind.field_type) << ",\n"
           << pad << "    \"noise_type\": " << static_cast<unsigned>(snapshot.wind.noise_type) << ",\n"
           << pad << "    \"falloff_type\": " << static_cast<unsigned>(snapshot.wind.falloff_type) << ",\n"
           << pad << "    \"local_enabled\": " << (snapshot.wind.local_enabled ? "true" : "false") << ",\n"
           << pad << "    \"controller_enabled\": " << (snapshot.wind.controller_enabled ? "true" : "false") << ",\n"
           << pad << "    \"direction\": ";
    write_vec3(output, snapshot.wind.direction);
    output << ",\n" << pad << "    \"strength\": " << snapshot.wind.strength
           << ",\n" << pad << "    \"gust\": " << snapshot.wind.gust
           << ",\n" << pad << "    \"turbulence\": " << snapshot.wind.turbulence
           << ",\n" << pad << "    \"frequency\": " << snapshot.wind.frequency
           << ",\n" << pad << "    \"center\": ";
    write_vec3(output, snapshot.wind.center);
    output << ",\n" << pad << "    \"radius\": " << snapshot.wind.radius
           << ",\n" << pad << "    \"core_ratio\": " << snapshot.wind.core_ratio
           << ",\n" << pad << "    \"collision_group_mask\": " << snapshot.wind.collision_group_mask
           << ",\n" << pad << "    \"maximum_speed\": " << snapshot.wind.maximum_speed
           << "\n" << pad << "  },\n" << pad << "  \"physics\": {\n"
           << pad << "    \"damping_enabled\": " << (snapshot.physics.damping_enabled ? "true" : "false") << ",\n"
           << pad << "    \"linear_damping\": " << snapshot.physics.linear_damping
           << ",\n" << pad << "    \"angular_damping\": " << snapshot.physics.angular_damping
           << ",\n" << pad << "    \"gravity_enabled\": " << (snapshot.physics.gravity_enabled ? "true" : "false") << ",\n"
           << pad << "    \"gravity_direction\": ";
    write_vec3(output, snapshot.physics.gravity_direction);
    output << ",\n" << pad << "    \"gravity_acceleration\": " << snapshot.physics.gravity_acceleration
           << ",\n" << pad << "    \"wind_target\": ";
    write_target(output, snapshot.physics.wind_target, indent + 4);
    output << ",\n" << pad << "    \"damping_target\": ";
    write_target(output, snapshot.physics.damping_target, indent + 4);
    output << ",\n" << pad << "    \"gravity_target\": ";
    write_target(output, snapshot.physics.gravity_target, indent + 4);
    output << "\n" << pad << "  }\n" << pad << '}';
}

}  // namespace

std::string serialize_track_json(
    const ControlSnapshot& current,
    const PhysicsTrack& track) {
    return serialize_track_json(current, track, {});
}

std::string serialize_track_json(
    const ControlSnapshot& current,
    const PhysicsTrack& track,
    const std::vector<TargetGroup>& target_groups) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(9);
    output << "{\n  \"version\": 3,\n  \"current\": ";
    write_snapshot(output, current, 2);
    output << ",\n  \"keyframes\": [";
    const auto& keys = track.keys();
    for (std::size_t index = 0; index < keys.size(); ++index) {
        output << (index == 0 ? "\n" : ",\n")
               << "    {\"frame\": " << keys[index].frame << ", \"value\": ";
        write_snapshot(output, keys[index].value, 4);
        output << '}';
    }
    if (!keys.empty()) output << '\n';
    output << "  ],\n  \"target_groups\": [";
    for (std::size_t index = 0; index < target_groups.size(); ++index) {
        output << (index == 0 ? "\n" : ",\n") << "    {\"name\": ";
        write_json_string(output, target_groups[index].name);
        output << ", \"target\": ";
        write_target(output, target_groups[index].target, 4);
        output << '}';
    }
    if (!target_groups.empty()) output << '\n';
    output << "  ]\n}\n";
    return output.str();
}

bool deserialize_track_json(
    std::string_view json,
    ControlSnapshot& current,
    PhysicsTrack& track,
    std::string& error) {
    std::vector<TargetGroup> ignored_groups;
    return deserialize_track_json(json, current, track, ignored_groups, error);
}

bool deserialize_track_json(
    std::string_view json,
    ControlSnapshot& current,
    PhysicsTrack& track,
    std::vector<TargetGroup>& target_groups,
    std::string& error) {
    JsonValue root;
    JsonParser parser(json);
    if (!parser.parse(root, error) || root.kind != JsonValue::Kind::Object) {
        if (error.empty()) error = "root must be an object";
        return false;
    }
    std::uint32_t version = 0;
    const JsonValue* current_value = field(root, "current");
    const JsonValue* keyframes = field(root, "keyframes");
    if (!read_u32(root, "version", version) || version < 1 || version > 3 ||
        current_value == nullptr ||
        keyframes == nullptr || keyframes->kind != JsonValue::Kind::Array ||
        !read_snapshot(*current_value, current)) {
        error = "unsupported or incomplete physics track document";
        return false;
    }

    PhysicsTrack parsed_track;
    for (const auto& key : keyframes->array) {
        std::uint32_t frame = 0;
        const JsonValue* value = field(key, "value");
        ControlSnapshot snapshot{};
        if (!read_u32(key, "frame", frame) || value == nullptr ||
            !read_snapshot(*value, snapshot) || parsed_track.has_key(frame)) {
            error = "invalid or duplicate physics keyframe";
            return false;
        }
        parsed_track.set_key(frame, snapshot);
    }

    std::vector<TargetGroup> parsed_groups;
    const JsonValue* groups = field(root, "target_groups");
    if (groups != nullptr) {
        if (groups->kind != JsonValue::Kind::Array) {
            error = "target_groups must be an array";
            return false;
        }
        for (const auto& value : groups->array) {
            TargetGroup group;
            if (!read_target_group(value, group) ||
                std::any_of(
                    parsed_groups.begin(),
                    parsed_groups.end(),
                    [&](const TargetGroup& existing) { return existing.name == group.name; })) {
                error = "invalid or duplicate target group";
                return false;
            }
            parsed_groups.push_back(std::move(group));
        }
    }
    track = std::move(parsed_track);
    target_groups = std::move(parsed_groups);
    error.clear();
    return true;
}

}  // namespace physics_control_studio
