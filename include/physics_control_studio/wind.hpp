#pragma once

#include "physics_control_studio/core.hpp"

#include <cstddef>
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

enum class WindFalloffType : std::uint8_t {
    Hard,
    Linear,
    Smooth,
    Quadratic,
};

struct WindSettings {
    bool enabled = false;
    WindFieldType field_type = WindFieldType::Directional;
    WindNoiseType noise_type = WindNoiseType::Smooth;
    WindFalloffType falloff_type = WindFalloffType::Smooth;
    bool local_enabled = false;
    bool controller_enabled = false;
    Vec3 direction{1.0f, 0.0f, 0.0f};
    float strength = 30.0f;
    float gust = 0.25f;
    float turbulence = 0.12f;
    float frequency = 0.65f;
    Vec3 center{};
    float radius = 20.0f;
    float core_ratio = 0.35f;
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
    // Acceleration is the force-independent wind contribution. The host can
    // integrate it through Bullet's native force accumulator instead of
    // replacing the solver velocity every frame.
    Vec3 acceleration{};
    Vec3 linear_velocity{};
};

bool validate_wind_settings(const WindSettings& settings) noexcept;
void normalize_wind_settings(WindSettings& settings) noexcept;
Vec3 normalize_or_zero(const Vec3& value) noexcept;
Vec3 combine_wind_accelerations(
    const Vec3* accelerations,
    std::size_t count,
    float maximum_magnitude) noexcept;
Vec3 limit_wind_acceleration_to_speed(
    const Vec3& acceleration,
    const Vec3& linear_velocity,
    float delta_seconds,
    float maximum_speed) noexcept;
float wind_modulation(const WindSettings& settings, double time_seconds) noexcept;
float wind_distance_attenuation(
    const WindSettings& settings,
    const Vec3& position) noexcept;
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
