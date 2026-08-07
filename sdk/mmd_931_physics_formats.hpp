#pragma once

#include "mmd_931_model.hpp"

#include <cstddef>
#include <cstdint>

namespace mmd931::model::physics {

enum class ShapeType : std::uint8_t {
    Sphere = 0,
    Box = 1,
    Capsule = 2,
};

// The host checks this byte directly. Keep the raw values; the semantic
// labels are intentionally conservative because mode 0 is synchronized from
// bones while modes 1/2 are consumed by the dynamic-body path.
enum class BodyMode : std::uint8_t {
    BoneSynchronized = 0,
    Dynamic = 1,
    DynamicBoneAligned = 2,
};

struct RigidBodyRecord {
    char name[0x14];
    std::uint64_t wide_name;
    std::uint64_t wide_english_name;
    std::int32_t bone_index;
    std::uint8_t collision_group;
    std::uint8_t reserved_2d;
    std::uint16_t no_collision_group;
    ShapeType shape_type;
    std::uint8_t reserved_31[3];
    float shape_size[3];
    float position[3];
    float rotation[3];
    float mass;
    BodyMode mode;
    std::uint8_t reserved_5d[3];
    std::uint32_t runtime_body_id;
    float linear_damping;
    float angular_damping;
    float restitution;
    float friction;
    std::uint32_t reserved_74;
    std::uint64_t runtime_object;
    float initial_world_matrix[16];
};

struct JointRecord {
    char name[0x14];
    std::uint64_t wide_name;
    std::uint64_t wide_english_name;
    std::int32_t rigid_body_a;
    std::int32_t rigid_body_b;
    float position[3];
    float rotation[3];
    float translation_lower[3];
    float translation_upper[3];
    float rotation_lower[3];
    float rotation_upper[3];
    float spring_translation[3];
    float spring_rotation[3];
    std::uint32_t runtime_constraint_id;
    float max_separation;
};

namespace bullet {
constexpr std::size_t kMotionStatePointer = 0x218;
constexpr std::size_t kMotionStateSetWorldTransform = 0x10;
constexpr std::size_t kRigidBodyTransform = 0x10;
constexpr std::size_t kRigidBodyLinearVelocity = 0x90;
constexpr std::size_t kRigidBodyAngularVelocity = 0xa0;
constexpr std::size_t kRigidBodyCollisionShape = 0xd0;
constexpr std::size_t kRigidBodyActivationState = 0xec;
constexpr std::size_t kRigidBodyFriction = 0xf4;
constexpr std::size_t kRigidBodyRestitution = 0xf8;
constexpr std::size_t kRigidBodyInternalType = 0x108;
constexpr std::size_t kRigidBodyInverseMass = 0x170;
constexpr std::size_t kRigidBodyRuntimeId = 0x248;

constexpr std::size_t kGeneric6DofSpringConstraintSize = 0x5b0;
constexpr std::size_t kSpringAxisCount = 6;
constexpr std::size_t kSpringEnabled = 0x560;
constexpr std::size_t kSpringEquilibrium = 0x568;
constexpr std::size_t kSpringStiffness = 0x580;
constexpr std::size_t kSpringDamping = 0x598;
constexpr std::size_t kLinearMotorTargetVelocity = 0x370;
constexpr std::size_t kLinearMotorMaxForce = 0x380;
constexpr std::size_t kAngularMotorBase = 0x3c0;
constexpr std::size_t kAngularMotorStride = 0x38;
constexpr std::size_t kAngularMotorTargetVelocity = 0x08;
constexpr std::size_t kAngularMotorMaxForce = 0x0c;

constexpr std::size_t kConstraintGetInfo1VtableOffset = 0x18;
constexpr std::size_t kConstraintGetInfo2VtableOffset = 0x20;
constexpr std::size_t kSolverBodySize = 0x70;
constexpr std::size_t kSolverConstraintRowSize = 0xb0;

constexpr std::size_t kWorldStepSimulationVtableOffset = 0x38;
constexpr std::size_t kWorldAddConstraintVtableOffset = 0x48;
constexpr std::size_t kWorldRemoveConstraintVtableOffset = 0x50;
constexpr std::size_t kWorldSetGravityVtableOffset = 0x68;
constexpr std::size_t kWorldGetNumConstraintsVtableOffset = 0xa0;
constexpr std::size_t kWorldGetConstraintVtableOffset = 0xb0;
constexpr std::size_t kWorldAddRigidBodyVtableOffset = 0x118;

constexpr float kDefaultFixedTimeStep = 1.0f / 60.0f;
constexpr std::int32_t kDefaultMaxSubSteps = 10;
constexpr float kDefaultGravityY = -98.0f;
constexpr float kBroadphaseWorldMin = -10000.0f;
constexpr float kBroadphaseWorldMax = 10000.0f;
constexpr std::uint32_t kBroadphaseMaxHandles = 1500000;
}  // namespace bullet

static_assert(sizeof(RigidBodyRecord) == 0xc0);
static_assert(offsetof(RigidBodyRecord, bone_index) == 0x28);
static_assert(offsetof(RigidBodyRecord, shape_size) == 0x34);
static_assert(offsetof(RigidBodyRecord, mass) == 0x58);
static_assert(offsetof(RigidBodyRecord, mode) == 0x5c);
static_assert(offsetof(RigidBodyRecord, runtime_body_id) == 0x60);
static_assert(offsetof(RigidBodyRecord, friction) == 0x70);
static_assert(offsetof(RigidBodyRecord, runtime_object) == 0x78);
static_assert(offsetof(RigidBodyRecord, initial_world_matrix) == 0x80);
static_assert(sizeof(JointRecord) == 0x98);
static_assert(offsetof(JointRecord, rigid_body_a) == 0x28);
static_assert(offsetof(JointRecord, rigid_body_b) == 0x2c);
static_assert(offsetof(JointRecord, spring_rotation) == 0x84);
static_assert(offsetof(JointRecord, runtime_constraint_id) == 0x90);
static_assert(offsetof(JointRecord, max_separation) == 0x94);

}  // namespace mmd931::model::physics
