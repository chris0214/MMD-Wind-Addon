#include "physics_control_studio/wind.hpp"

#include <algorithm>
#include <cmath>

namespace physics_control_studio {
namespace {

constexpr float kPi = 3.14159265358979323846f;

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

float smooth_noise(double phase) noexcept {
    const float first = std::sin(static_cast<float>(phase * 1.731 + 0.37));
    const float second = std::sin(static_cast<float>(phase * 3.117 + 2.11));
    return first * 0.65f + second * 0.35f;
}

float hash_noise(std::int32_t value) noexcept {
    std::uint32_t bits = static_cast<std::uint32_t>(value);
    bits ^= bits >> 16;
    bits *= 0x7feb352du;
    bits ^= bits >> 15;
    bits *= 0x846ca68bu;
    bits ^= bits >> 16;
    return static_cast<float>(bits & 0x00ffffffu) / 8'388'607.5f - 1.0f;
}

float fade(float value) noexcept {
    return value * value * value * (value * (value * 6.0f - 15.0f) + 10.0f);
}

float perlin_noise(double phase) noexcept {
    const auto cell = static_cast<std::int32_t>(std::floor(phase));
    const float local = static_cast<float>(phase - static_cast<double>(cell));
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
        const float wave = std::max(0.0f, std::sin(static_cast<float>(phase)));
        return wave * wave * 2.0f - 0.45f;
    }
    case WindNoiseType::RandomGust: {
        const double scaled = phase * 0.18;
        const auto cell = static_cast<std::int32_t>(std::floor(scaled));
        const float local = static_cast<float>(scaled - static_cast<double>(cell));
        const float first = hash_noise(cell);
        const float second = hash_noise(cell + 1);
        return first + (second - first) * fade(local);
    }
    }
    return 0.0f;
}

}  // namespace

bool validate_wind_settings(const WindSettings& settings) noexcept {
    return valid_field_type(settings.field_type) && valid_noise_type(settings.noise_type) &&
        finite(settings.direction) && finite(settings.center) &&
        finite(settings.strength) && settings.strength >= 0.0f &&
        settings.strength <= 500.0f && finite(settings.gust) &&
        settings.gust >= 0.0f && settings.gust <= 1.0f &&
        finite(settings.turbulence) && settings.turbulence >= 0.0f &&
        settings.turbulence <= 1.0f && finite(settings.frequency) &&
        settings.frequency >= 0.0f && settings.frequency <= 20.0f &&
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

float wind_modulation(const WindSettings& settings, double time_seconds) noexcept {
    if (!std::isfinite(time_seconds) || !finite(settings.frequency) ||
        !finite(settings.gust) || !finite(settings.turbulence)) {
        return 0.0f;
    }
    const double phase = time_seconds * static_cast<double>(settings.frequency) *
        static_cast<double>(2.0f * kPi);
    const float value = 1.0f + settings.gust * std::sin(static_cast<float>(phase)) +
        settings.turbulence * wind_noise(settings.noise_type, phase);
    return std::max(0.0f, value);
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
        const float phase = static_cast<float>(
            time_seconds * static_cast<double>(settings.frequency) * 2.0 * kPi);
        const Vec3 noise{
            std::sin(body.position.y * 0.47f + body.position.z * 0.19f + phase * 1.37f),
            std::sin(body.position.z * 0.41f + body.position.x * 0.23f + phase * 1.71f),
            std::sin(body.position.x * 0.43f + body.position.y * 0.17f + phase * 1.13f)};
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

    result.modulation = wind_modulation(settings, time_seconds);
    if (result.modulation <= 0.0f) return result;

    const Vec3 direction = wind_field_direction(settings, body, time_seconds);
    if (length_squared(direction) <= 1.0e-8f) return result;
    const float acceleration = settings.strength * result.modulation;
    Vec3 velocity = add(body.linear_velocity, scale(direction, acceleration * delta_seconds));
    const float speed_squared = length_squared(velocity);
    const float maximum_squared = settings.maximum_speed * settings.maximum_speed;
    if (speed_squared > maximum_squared) {
        velocity = scale(velocity, settings.maximum_speed / std::sqrt(speed_squared));
    }
    result.affected = true;
    result.linear_velocity = velocity;
    return result;
}

}  // namespace physics_control_studio
