#include "physics_control_studio/physics_track.hpp"

#include <algorithm>
#include <cmath>

namespace physics_control_studio {
namespace {

bool finite(float value) noexcept {
    return std::isfinite(value);
}

bool finite(const Vec3& value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

float lerp(float first, float second, float amount) noexcept {
    return first + (second - first) * amount;
}

Vec3 lerp(const Vec3& first, const Vec3& second, float amount) noexcept {
    return {
        lerp(first.x, second.x, amount),
        lerp(first.y, second.y, amount),
        lerp(first.z, second.z, amount)};
}

float length_squared(const Vec3& value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

bool valid_target(const TargetSelection& target) noexcept {
    const bool kind_valid =
        target.kind == TargetKind::AllDynamic ||
        (target.kind == TargetKind::CollisionGroup && target.index < 16) ||
        (target.kind == TargetKind::RigidBody && target.index < 65'536) ||
        target.kind == TargetKind::CustomSet;
    const bool bodies_valid = std::all_of(
        target.rigid_body_indices.begin(),
        target.rigid_body_indices.end(),
        [](std::uint32_t index) { return index < 65'536; });
    return kind_valid && bodies_valid;
}

}  // namespace

bool validate_physics_settings(const PhysicsSettings& settings) noexcept {
    return valid_target(settings.wind_target) &&
        valid_target(settings.damping_target) &&
        valid_target(settings.gravity_target) && finite(settings.linear_damping) &&
        settings.linear_damping >= 0.0f && settings.linear_damping <= 1.0f &&
        finite(settings.angular_damping) && settings.angular_damping >= 0.0f &&
        settings.angular_damping <= 1.0f && finite(settings.gravity_direction) &&
        length_squared(settings.gravity_direction) > 1.0e-8f &&
        finite(settings.gravity_acceleration) && settings.gravity_acceleration >= 0.0f &&
        settings.gravity_acceleration <= 100.0f;
}

bool target_matches(
    const TargetSelection& target,
    std::uint32_t body_index,
    std::uint8_t collision_group,
    bool dynamic_body) noexcept {
    if (!dynamic_body || collision_group > 15) return false;
    switch (target.kind) {
    case TargetKind::AllDynamic:
        return true;
    case TargetKind::CollisionGroup:
        return target.index == collision_group;
    case TargetKind::RigidBody:
        return target.index == body_index;
    case TargetKind::CustomSet:
        return (target.collision_group_mask & (1u << collision_group)) != 0 ||
            std::find(
                target.rigid_body_indices.begin(),
                target.rigid_body_indices.end(),
                body_index) != target.rigid_body_indices.end();
    }
    return false;
}

ControlSnapshot interpolate_control_snapshots(
    const ControlSnapshot& first,
    const ControlSnapshot& second,
    float amount) noexcept {
    const float t = std::clamp(amount, 0.0f, 1.0f);
    ControlSnapshot result = t >= 1.0f ? second : first;
    result.wind.direction = lerp(first.wind.direction, second.wind.direction, t);
    result.wind.strength = lerp(first.wind.strength, second.wind.strength, t);
    result.wind.gust = lerp(first.wind.gust, second.wind.gust, t);
    result.wind.turbulence = lerp(first.wind.turbulence, second.wind.turbulence, t);
    result.wind.frequency = lerp(first.wind.frequency, second.wind.frequency, t);
    result.wind.center = lerp(first.wind.center, second.wind.center, t);
    result.wind.radius = lerp(first.wind.radius, second.wind.radius, t);
    result.wind.core_ratio = lerp(
        first.wind.core_ratio, second.wind.core_ratio, t);
    result.wind.maximum_speed = lerp(
        first.wind.maximum_speed, second.wind.maximum_speed, t);
    result.physics.linear_damping = lerp(
        first.physics.linear_damping, second.physics.linear_damping, t);
    result.physics.angular_damping = lerp(
        first.physics.angular_damping, second.physics.angular_damping, t);
    result.physics.gravity_direction = lerp(
        first.physics.gravity_direction, second.physics.gravity_direction, t);
    result.physics.gravity_acceleration = lerp(
        first.physics.gravity_acceleration, second.physics.gravity_acceleration, t);
    normalize_wind_settings(result.wind);
    return result;
}

void PhysicsTrack::set_key(std::uint32_t frame, const ControlSnapshot& value) {
    ControlSnapshot normalized = value;
    normalize_wind_settings(normalized.wind);
    const auto found = std::lower_bound(
        keys_.begin(), keys_.end(), frame,
        [](const ControlKeyframe& key, std::uint32_t value_frame) {
            return key.frame < value_frame;
        });
    if (found != keys_.end() && found->frame == frame) {
        found->value = normalized;
        return;
    }
    keys_.insert(found, ControlKeyframe{frame, normalized});
}

bool PhysicsTrack::erase_key(std::uint32_t frame) noexcept {
    const auto found = std::lower_bound(
        keys_.begin(), keys_.end(), frame,
        [](const ControlKeyframe& key, std::uint32_t value_frame) {
            return key.frame < value_frame;
        });
    if (found == keys_.end() || found->frame != frame) return false;
    keys_.erase(found);
    return true;
}

void PhysicsTrack::clear() noexcept {
    keys_.clear();
}

bool PhysicsTrack::has_key(std::uint32_t frame) const noexcept {
    const auto found = std::lower_bound(
        keys_.begin(), keys_.end(), frame,
        [](const ControlKeyframe& key, std::uint32_t value_frame) {
            return key.frame < value_frame;
        });
    return found != keys_.end() && found->frame == frame;
}

std::size_t PhysicsTrack::size() const noexcept {
    return keys_.size();
}

ControlSnapshot PhysicsTrack::evaluate(
    std::uint32_t frame,
    const ControlSnapshot& fallback) const noexcept {
    if (keys_.empty()) return fallback;
    const auto next = std::lower_bound(
        keys_.begin(), keys_.end(), frame,
        [](const ControlKeyframe& key, std::uint32_t value_frame) {
            return key.frame < value_frame;
        });
    if (next == keys_.begin()) return next->value;
    if (next == keys_.end()) return keys_.back().value;
    if (next->frame == frame) return next->value;
    const auto previous = next - 1;
    const float amount = static_cast<float>(frame - previous->frame) /
        static_cast<float>(next->frame - previous->frame);
    return interpolate_control_snapshots(previous->value, next->value, amount);
}

const std::vector<ControlKeyframe>& PhysicsTrack::keys() const noexcept {
    return keys_;
}

}  // namespace physics_control_studio
