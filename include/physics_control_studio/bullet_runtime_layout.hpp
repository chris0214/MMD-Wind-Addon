#pragma once

#include <cstddef>

namespace physics_control_studio::bullet_runtime_layout {

// Verified against MMD 9.31 x64's btRigidBody reset and solver routines.
inline constexpr std::size_t kWorldPosition = 0x40;
inline constexpr std::size_t kInterpolationLinearVelocity = 0x90;
inline constexpr std::size_t kInterpolationAngularVelocity = 0xa0;
inline constexpr std::size_t kActivationState = 0xec;
inline constexpr std::size_t kLinearVelocity = 0x150;
inline constexpr std::size_t kAngularVelocity = 0x160;
inline constexpr std::size_t kTotalForce = 0x1d0;
inline constexpr std::size_t kTotalTorque = 0x1e0;
inline constexpr std::size_t kLinearDamping = 0x1f0;
inline constexpr std::size_t kAngularDamping = 0x1f4;
inline constexpr std::size_t kRigidBodySize = 0x250;

}  // namespace physics_control_studio::bullet_runtime_layout
