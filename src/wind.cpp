#include "physics_control_studio/wind.hpp"

#include <algorithm>
#include <cmath>

namespace physics_control_studio {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kTwoPi = 2.0 * kPi;
// Keep procedural noise in a finite, repeatable domain. The period is long
// enough that a normal session will not notice the repeat, while avoiding
// loss of precision in integer cell calculations after very long runtimes.
constexpr std::int64_t kNoiseCellPeriod = 1ll << 20;

bool finite(float value) noexcept {
    return std::isfinite(value) != 0;
}

bool finite(const Vec3& value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool valid_field_type(WindFieldType type) noexcept {
    switch (type) {
    case WindFieldType::Directional:
    case WindFieldType::Turbulence:
    case WindFieldType::Vortex:
    case WindFieldType::RadialOut:
    case WindFieldType::RadialIn:
    case WindFieldType::Updraft:
    case WindFieldType::Downburst:
    case WindFieldType::Shear:
        return true;
    }
    return false;
}

bool valid_noise_type(WindNoiseType type) noexcept {
    switch (type) {
    case WindNoiseType::Smooth:
    case WindNoiseType::Perlin:
    case WindNoiseType::Fractal:
    case WindNoiseType::Pulse:
    case WindNoiseType::RandomGust:
        return true;
    }
    return false;
}

bool valid_falloff_type(WindFalloffType type) noexcept {
    switch (type) {
    case WindFalloffType::Hard:
    case WindFalloffType::Linear:
    case WindFalloffType::Smooth:
    case WindFalloffType::Quadratic:
        return true;
    }
    return false;
}

float length_squared(const Vec3& value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

Vec3 scale(const Vec3& value, float factor) noexcept {
    return {value.x * factor, value.y * factor, value.z * factor};
}

Vec3 add(const Vec3& left, const Vec3& right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 subtract(const Vec3& left, const Vec3& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 cross(const Vec3& left, const Vec3& right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x};
}

double sine_phase(double phase) noexcept {
    return std::sin(std::remainder(phase, kTwoPi));
}

std::int64_t wrapped_noise_cell(std::int64_t value) noexcept {
    const std::int64_t wrapped = value % kNoiseCellPeriod;
    return wrapped >= 0 ? wrapped : wrapped + kNoiseCellPeriod;
}

float smooth_noise(double phase) noexcept {
    const float first = static_cast<float>(sine_phase(phase * 1.731 + 0.37));
    const float second = static_cast<float>(sine_phase(phase * 3.117 + 2.11));
    return first * 0.65f + second * 0.35f;
}

float hash_noise(std::int64_t value) noexcept {
    std::uint64_t bits = static_cast<std::uint64_t>(wrapped_noise_cell(value));
    bits ^= bits >> 30;
    bits *= 0xbf58476d1ce4e5b9ull;
    bits ^= bits >> 27;
    bits *= 0x94d049bb133111ebull;
    bits ^= bits >> 31;
    return static_cast<float>((bits >> 40) & 0x00ffffffu) / 8'388'607.5f - 1.0f;
}

float fade(float value) noexcept {
    return value * value * value * (value * (value * 6.0f - 15.0f) + 10.0f);
}

float perlin_noise(double phase) noexcept {
    double wrapped = std::fmod(phase, static_cast<double>(kNoiseCellPeriod));
    if (wrapped < 0.0) wrapped += static_cast<double>(kNoiseCellPeriod);
    const auto cell = static_cast<std::int64_t>(std::floor(wrapped));
    const float local = static_cast<float>(wrapped - static_cast<double>(cell));
    const float first = hash_noise(cell) * local;
    const float second = hash_noise(cell + 1) * (local - 1.0f);
    return first + (second - first) * fade(local);
}

float fractal_noise(double phase) noexcept {
    float value = 0.0f;
    float amplitude = 0.58f;
    double frequency = 1.0;
    for (int octave = 0; octave < 4; ++octave) {
        value += perlin_noise(phase * frequency + octave * 17.0) * amplitude;
        frequency *= 2.07;
        amplitude *= 0.5f;
    }
    return value;
}

float wind_noise(WindNoiseType type, double phase) noexcept {
    switch (type) {
    case WindNoiseType::Smooth:
        return smooth_noise(phase);
    case WindNoiseType::Perlin:
        return perlin_noise(phase * 0.45) * 2.2f;
    case WindNoiseType::Fractal:
        return fractal_noise(phase * 0.32) * 2.0f;
    case WindNoiseType::Pulse: {
        const float wave = std::max(0.0f, static_cast<float>(sine_phase(phase)));
        return wave * wave * 2.0f - 0.45f;
    }
    case WindNoiseType::RandomGust: {
        const double scaled = phase * 0.18;
            double wrapped = std::fmod(scaled, static_cast<double>(kNoiseCellPeriod));
            if (wrapped < 0.0) wrapped += static_cast<double>(kNoiseCellPeriod);
            const auto cell = static_cast<std::int64_t>(std::floor(wrapped));
            const float local = static_cast<float>(wrapped - static_cast<double>(cell));
        const float first = hash_noise(cell);
        const float second = hash_noise(cell + 1);
        return first + (second - first) * fade(local);
    }
    }
    return 0.0f;
}

}  // namespace

void normalize_wind_settings(WindSettings& settings) noexcept {
    if (settings.controller_enabled) settings.local_enabled = true;
}

bool validate_wind_settings(const WindSettings& settings) noexcept {
    return valid_field_type(settings.field_type) && valid_noise_type(settings.noise_type) &&
        valid_falloff_type(settings.falloff_type) &&
        (!settings.controller_enabled || settings.local_enabled) &&
        finite(settings.direction) && finite(settings.center) &&
        finite(settings.strength) && settings.strength >= 0.0f &&
        settings.strength <= 4'000.0f && finite(settings.gust) &&
        settings.gust >= 0.0f && settings.gust <= 1.0f &&
        finite(settings.turbulence) && settings.turbulence >= 0.0f &&
        settings.turbulence <= 1.0f && finite(settings.frequency) &&
        settings.frequency >= 0.0f && settings.frequency <= 20.0f &&
        finite(settings.radius) && settings.radius >= 0.1f &&
        settings.radius <= 1'000.0f && finite(settings.core_ratio) &&
        settings.core_ratio >= 0.0f && settings.core_ratio <= 1.0f &&
        finite(settings.maximum_speed) && settings.maximum_speed > 0.0f &&
        settings.maximum_speed <= 10'000.0f &&
        length_squared(settings.direction) > 1.0e-8f;
}

Vec3 normalize_or_zero(const Vec3& value) noexcept {
    if (!finite(value)) return {};
    const float squared = length_squared(value);
    if (squared <= 1.0e-8f) return {};
    return scale(value, 1.0f / std::sqrt(squared));
}

Vec3 combine_wind_accelerations(
    const Vec3* accelerations,
    std::size_t count,
    float maximum_magnitude) noexcept {
    if (accelerations == nullptr || count == 0 || !finite(maximum_magnitude) ||
        maximum_magnitude <= 0.0f) {
        return {};
    }

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        if (!finite(accelerations[index])) return {};
        x += static_cast<double>(accelerations[index].x);
        y += static_cast<double>(accelerations[index].y);
        z += static_cast<double>(accelerations[index].z);
    }
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return {};

    const double squared = x * x + y * y + z * z;
    if (squared <= 1.0e-16) return {};
    const double maximum = static_cast<double>(maximum_magnitude);
    if (squared > maximum * maximum) {
        const double factor = maximum / std::sqrt(squared);
        x *= factor;
        y *= factor;
        z *= factor;
    }
    return {
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(z)};
}

Vec3 limit_wind_acceleration_to_speed(
    const Vec3& acceleration,
    const Vec3& linear_velocity,
    float delta_seconds,
    float maximum_speed) noexcept {
    if (!finite(acceleration) || !finite(linear_velocity) || !finite(delta_seconds) ||
        delta_seconds <= 0.0f || !finite(maximum_speed) || maximum_speed <= 0.0f) {
        return {};
    }
    Vec3 velocity = add(linear_velocity, scale(acceleration, delta_seconds));
    const float speed_squared = length_squared(velocity);
    const float maximum_squared = maximum_speed * maximum_speed;
    if (speed_squared > maximum_squared) {
        velocity = scale(velocity, maximum_speed / std::sqrt(speed_squared));
    }
    return scale(subtract(velocity, linear_velocity), 1.0f / delta_seconds);
}

float wind_modulation(const WindSettings& settings, double time_seconds) noexcept {
    if (!std::isfinite(time_seconds) || !finite(settings.frequency) ||
        !finite(settings.gust) || !finite(settings.turbulence)) {
        return 0.0f;
    }
    const double phase = time_seconds * static_cast<double>(settings.frequency) * kTwoPi;
    const float value = 1.0f + settings.gust * static_cast<float>(sine_phase(phase)) +
        settings.turbulence * wind_noise(settings.noise_type, phase);
    return std::max(0.0f, value);
}

float wind_distance_attenuation(
    const WindSettings& settings,
    const Vec3& position) noexcept {
    if (!settings.local_enabled) return 1.0f;
    if (!finite(position) || !finite(settings.center) || !finite(settings.radius) ||
        !finite(settings.core_ratio) || settings.radius <= 0.0f) {
        return 0.0f;
    }

    const Vec3 offset = subtract(position, settings.center);
    const float distance_squared = length_squared(offset);
    const float radius_squared = settings.radius * settings.radius;
    if (distance_squared >= radius_squared) return 0.0f;
    if (settings.falloff_type == WindFalloffType::Hard) return 1.0f;

    const float core_radius = settings.radius *
        std::clamp(settings.core_ratio, 0.0f, 1.0f);
    const float distance = std::sqrt(distance_squared);
    if (distance <= core_radius) return 1.0f;
    const float falloff_width = settings.radius - core_radius;
    if (falloff_width <= 1.0e-6f) return 1.0f;
    const float t = std::clamp(
        (distance - core_radius) / falloff_width,
        0.0f,
        1.0f);
    const float remaining = 1.0f - t;
    switch (settings.falloff_type) {
    case WindFalloffType::Hard:
        return 1.0f;
    case WindFalloffType::Linear:
        return remaining;
    case WindFalloffType::Smooth:
        return 1.0f - t * t * (3.0f - 2.0f * t);
    case WindFalloffType::Quadratic:
        return remaining * remaining;
    }
    return 0.0f;
}

Vec3 wind_field_direction(
    const WindSettings& settings,
    const WindBodySample& body,
    double time_seconds) noexcept {
    if (!finite(body.position) || !std::isfinite(time_seconds)) return {};
    const Vec3 axis = normalize_or_zero(settings.direction);
    const Vec3 radial = subtract(body.position, settings.center);
    switch (settings.field_type) {
    case WindFieldType::Directional:
        return axis;
    case WindFieldType::Turbulence: {
        const double phase = time_seconds * static_cast<double>(settings.frequency) * kTwoPi;
        const Vec3 noise{
            static_cast<float>(sine_phase(body.position.y * 0.47 + body.position.z * 0.19 + phase * 1.37)),
            static_cast<float>(sine_phase(body.position.z * 0.41 + body.position.x * 0.23 + phase * 1.71)),
            static_cast<float>(sine_phase(body.position.x * 0.43 + body.position.y * 0.17 + phase * 1.13))};
        return normalize_or_zero(add(scale(axis, 0.35f), scale(noise, 0.65f)));
    }
    case WindFieldType::Vortex:
        return normalize_or_zero(cross(axis, radial));
    case WindFieldType::RadialOut:
        return normalize_or_zero(radial);
    case WindFieldType::RadialIn:
        return scale(normalize_or_zero(radial), -1.0f);
    case WindFieldType::Updraft: {
        const Vec3 vertical{0.0f, 1.0f, 0.0f};
        const Vec3 inward{-radial.x, 0.0f, -radial.z};
        return normalize_or_zero(add(scale(vertical, 0.85f), scale(normalize_or_zero(inward), 0.15f)));
    }
    case WindFieldType::Downburst: {
        const Vec3 outward{radial.x, 0.0f, radial.z};
        return normalize_or_zero(add({0.0f, -0.72f, 0.0f}, scale(normalize_or_zero(outward), 0.68f)));
    }
    case WindFieldType::Shear: {
        const float height = body.position.y - settings.center.y;
        const float side = height >= 0.0f ? 1.0f : -1.0f;
        const Vec3 lateral = normalize_or_zero(cross({0.0f, 1.0f, 0.0f}, axis));
        return normalize_or_zero(add(scale(axis, side * 0.82f), scale(lateral, 0.18f)));
    }
    }
    return {};
}

WindEvaluation evaluate_wind(
    const WindSettings& settings,
    const WindBodySample& body,
    double time_seconds,
    float delta_seconds) noexcept {
    WindEvaluation result{};
    result.linear_velocity = body.linear_velocity;
    if (!settings.enabled || !validate_wind_settings(settings) ||
        body.body_mode == 0 || body.collision_group > 15 ||
        (settings.collision_group_mask & (1u << body.collision_group)) == 0 ||
        !finite(body.position) || !finite(body.linear_velocity) ||
        !finite(delta_seconds) || delta_seconds <= 0.0f || delta_seconds > 0.25f) {
        return result;
    }

    result.modulation = wind_modulation(settings, time_seconds) *
        wind_distance_attenuation(settings, body.position);
    if (result.modulation <= 0.0f) return result;

    const Vec3 direction = wind_field_direction(settings, body, time_seconds);
    if (length_squared(direction) <= 1.0e-8f) return result;
    const float acceleration = settings.strength * result.modulation;
    const Vec3 raw_acceleration = scale(direction, acceleration);
    const Vec3 limited_acceleration = limit_wind_acceleration_to_speed(
        raw_acceleration,
        body.linear_velocity,
        delta_seconds,
        settings.maximum_speed);
    result.affected = true;
    result.linear_velocity = add(
        body.linear_velocity,
        scale(limited_acceleration, delta_seconds));
    result.acceleration = limited_acceleration;
    return result;
}

}  // namespace physics_control_studio
