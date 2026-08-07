#pragma once

#include "physics_control_studio/core.hpp"

#include <cstdint>

namespace physics_control_studio {

enum class WindFieldType : std::uint8_t {
    Directional,
    Turbulence,
    Vortex,
    RadialOut,
    RadialIn,
    Updraft,
    Downburst,
    Shear,
};

enum class WindNoiseType : std::uint8_t {
    Smooth,
    Perlin,
    Fractal,
    Pulse,
    RandomGust,
};

struct WindSettings {
    bool enabled = false;
    WindFieldType field_type = WindFieldType::Directional;
    WindNoiseType noise_type = WindNoiseType::Smooth;
    Vec3 direction{1.0f, 0.0f, 0.0f};
    float strength = 30.0f;
    float gust = 0.25f;
    float turbulence = 0.12f;
    float frequency = 0.65f;
    Vec3 center{};
    std::uint16_t collision_group_mask = 0xffff;
    float maximum_speed = 160.0f;
};

struct WindBodySample {
    std::uint8_t body_mode = 0;
    std::uint8_t collision_group = 0;
    Vec3 position{};
    Vec3 linear_velocity{};
};

struct WindEvaluation {
    bool affected = false;
    float modulation = 0.0f;
    Vec3 linear_velocity{};
};

bool validate_wind_settings(const WindSettings& settings) noexcept;
Vec3 normalize_or_zero(const Vec3& value) noexcept;
float wind_modulation(const WindSettings& settings, double time_seconds) noexcept;
Vec3 wind_field_direction(
    const WindSettings& settings,
    const WindBodySample& body,
    double time_seconds) noexcept;
WindEvaluation evaluate_wind(
    const WindSettings& settings,
    const WindBodySample& body,
    double time_seconds,
    float delta_seconds) noexcept;

}  // namespace physics_control_studio
