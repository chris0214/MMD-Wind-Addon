#pragma once

#include "physics_control_studio/wind.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace physics_control_studio {

enum class TargetKind : std::uint8_t {
    AllDynamic,
    CollisionGroup,
    RigidBody,
    CustomSet,
};

struct TargetSelection {
    TargetSelection() = default;
    TargetSelection(TargetKind target_kind, std::uint32_t target_index)
        : kind(target_kind), index(target_index) {}

    TargetKind kind = TargetKind::AllDynamic;
    std::uint32_t index = 0;
    std::uint16_t collision_group_mask = 0;
    std::vector<std::uint32_t> rigid_body_indices;
};

struct TargetGroup {
    std::string name;
    TargetSelection target;
};

struct PhysicsSettings {
    bool damping_enabled = false;
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;
    bool gravity_enabled = false;
    Vec3 gravity_direction{0.0f, -1.0f, 0.0f};
    float gravity_acceleration = 9.8f;
    TargetSelection target{};
};

struct ControlSnapshot {
    WindSettings wind{};
    PhysicsSettings physics{};
};

struct ControlKeyframe {
    std::uint32_t frame = 0;
    ControlSnapshot value{};
};

bool validate_physics_settings(const PhysicsSettings& settings) noexcept;
bool target_matches(
    const TargetSelection& target,
    std::uint32_t body_index,
    std::uint8_t collision_group,
    bool dynamic_body) noexcept;
ControlSnapshot interpolate_control_snapshots(
    const ControlSnapshot& first,
    const ControlSnapshot& second,
    float amount) noexcept;

class PhysicsTrack {
public:
    void set_key(std::uint32_t frame, const ControlSnapshot& value);
    bool erase_key(std::uint32_t frame) noexcept;
    void clear() noexcept;
    bool has_key(std::uint32_t frame) const noexcept;
    std::size_t size() const noexcept;
    ControlSnapshot evaluate(
        std::uint32_t frame,
        const ControlSnapshot& fallback) const noexcept;
    const std::vector<ControlKeyframe>& keys() const noexcept;

private:
    std::vector<ControlKeyframe> keys_;
};

}  // namespace physics_control_studio
