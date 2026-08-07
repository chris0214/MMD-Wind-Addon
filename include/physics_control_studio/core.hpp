#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace physics_control_studio {

inline constexpr std::uint32_t kMaximumModelSlots = 255;
inline constexpr std::uint32_t kMaximumRuntimeElements = 1'000'000;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class EntityKind : std::uint8_t {
    World,
    RigidBody,
    Joint,
    Bone,
};

struct Target {
    EntityKind kind = EntityKind::World;
    std::uint32_t model_slot = 0;
    std::uint32_t index = 0;
};

enum class Capability : std::uint32_t {
    None = 0,
    ModelRecordWrite = 1u << 0,
    BulletBodyWrite = 1u << 1,
    ConstraintWrite = 1u << 2,
    BoneWritebackOverride = 1u << 3,
    WorldWrite = 1u << 4,
};

constexpr Capability operator|(Capability left, Capability right) noexcept {
    return static_cast<Capability>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr Capability operator&(Capability left, Capability right) noexcept {
    return static_cast<Capability>(
        static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

constexpr bool has_all(Capability available, Capability required) noexcept {
    return (available & required) == required;
}

struct RigidBodyPatch {
    std::optional<float> mass;
    std::optional<float> linear_damping;
    std::optional<float> angular_damping;
    std::optional<float> restitution;
    std::optional<float> friction;
    std::optional<std::uint8_t> body_mode;
    std::optional<std::uint8_t> collision_group;
    std::optional<std::uint16_t> collision_mask;
    bool clear_velocity = false;
    bool reactivate = true;
};

struct JointPatch {
    std::optional<Vec3> translation_lower;
    std::optional<Vec3> translation_upper;
    std::optional<Vec3> rotation_lower;
    std::optional<Vec3> rotation_upper;
    std::optional<Vec3> spring_translation;
    std::optional<Vec3> spring_rotation;
    std::optional<float> max_separation;
    bool rebuild_constraint = true;
};

struct BonePhysicsOverride {
    bool enabled = true;
    bool apply_translation = true;
    bool apply_rotation = true;
    float translation_blend = 1.0f;
    float rotation_blend = 1.0f;
};

struct WorldGravityPatch {
    Vec3 gravity{0.0f, -98.0f, 0.0f};
};

struct RigidBodyImpulse {
    Vec3 linear{};
    Vec3 angular{};
};

struct ResetRigidBody {
    bool clear_velocity = true;
};

using CommandPayload = std::variant<
    RigidBodyPatch,
    JointPatch,
    BonePhysicsOverride,
    WorldGravityPatch,
    RigidBodyImpulse,
    ResetRigidBody>;

struct Command {
    std::uint64_t id = 0;
    Target target{};
    CommandPayload payload{};
};

enum class ValidationCode : std::uint8_t {
    Ok,
    EmptyPatch,
    InvalidTarget,
    NonFinite,
    OutOfRange,
    InvalidLimits,
    CapabilityMissing,
    BackendFailure,
};

struct ValidationResult {
    ValidationCode code = ValidationCode::Ok;
    std::string message;

    explicit operator bool() const noexcept { return code == ValidationCode::Ok; }
};

Capability required_capabilities(const Command& command) noexcept;
ValidationResult validate(const Command& command);

class Backend {
public:
    virtual ~Backend() = default;
    virtual Capability capabilities() const noexcept = 0;
    virtual bool begin_transaction(std::string& error) = 0;
    virtual bool apply(const Command& command, std::string& error) = 0;
    virtual bool commit_transaction(std::string& error) = 0;
    virtual void rollback_transaction() noexcept = 0;
};

struct ExecutionReport {
    bool committed = false;
    std::size_t submitted = 0;
    std::size_t applied = 0;
    std::optional<std::uint64_t> failed_command_id;
    ValidationCode code = ValidationCode::Ok;
    std::string message;
};

class CommandQueue {
public:
    ValidationResult enqueue(Command command);
    std::size_t size() const noexcept { return commands_.size(); }
    bool empty() const noexcept { return commands_.empty(); }
    void clear() noexcept { commands_.clear(); }
    ExecutionReport execute(Backend& backend);

private:
    std::vector<Command> commands_;
};

}  // namespace physics_control_studio

