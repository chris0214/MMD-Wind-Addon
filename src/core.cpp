#include "physics_control_studio/core.hpp"

#include <cmath>
#include <type_traits>
#include <utility>

namespace physics_control_studio {
namespace {

bool finite(float value) noexcept {
    return std::isfinite(value) != 0;
}

bool finite(const Vec3& value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool within(float value, float lower, float upper) noexcept {
    return finite(value) && value >= lower && value <= upper;
}

bool any(const RigidBodyPatch& patch) noexcept {
    return patch.mass || patch.linear_damping || patch.angular_damping ||
        patch.restitution || patch.friction || patch.body_mode ||
        patch.collision_group || patch.collision_mask || patch.clear_velocity;
}

bool any(const JointPatch& patch) noexcept {
    return patch.translation_lower || patch.translation_upper ||
        patch.rotation_lower || patch.rotation_upper ||
        patch.spring_translation || patch.spring_rotation ||
        patch.max_separation;
}

bool ordered(const Vec3& lower, const Vec3& upper) noexcept {
    return lower.x <= upper.x && lower.y <= upper.y && lower.z <= upper.z;
}

ValidationResult fail(ValidationCode code, const char* message) {
    return {code, message};
}

ValidationResult validate_target(const Command& command) {
    if (command.target.model_slot >= kMaximumModelSlots) {
        return fail(ValidationCode::InvalidTarget, "model slot is outside 0..254");
    }

    const EntityKind expected = std::visit(
        [](const auto& payload) {
            using Type = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Type, RigidBodyPatch> ||
                          std::is_same_v<Type, RigidBodyImpulse> ||
                          std::is_same_v<Type, ResetRigidBody>) {
                return EntityKind::RigidBody;
            } else if constexpr (std::is_same_v<Type, JointPatch>) {
                return EntityKind::Joint;
            } else if constexpr (std::is_same_v<Type, BonePhysicsOverride>) {
                return EntityKind::Bone;
            } else {
                return EntityKind::World;
            }
        },
        command.payload);

    if (command.target.kind != expected) {
        return fail(ValidationCode::InvalidTarget, "payload does not match target kind");
    }
    if (expected == EntityKind::World && command.target.index != 0) {
        return fail(ValidationCode::InvalidTarget, "world commands require index zero");
    }
    return {};
}

ValidationResult validate_patch(const RigidBodyPatch& patch) {
    if (!any(patch)) return fail(ValidationCode::EmptyPatch, "rigid-body patch is empty");
    if (patch.mass && !within(*patch.mass, 0.0f, 1'000'000.0f))
        return fail(ValidationCode::OutOfRange, "mass is outside the supported range");
    if (patch.linear_damping && !within(*patch.linear_damping, 0.0f, 1.0f))
        return fail(ValidationCode::OutOfRange, "linear damping is outside 0..1");
    if (patch.angular_damping && !within(*patch.angular_damping, 0.0f, 1.0f))
        return fail(ValidationCode::OutOfRange, "angular damping is outside 0..1");
    if (patch.restitution && !within(*patch.restitution, 0.0f, 1.0f))
        return fail(ValidationCode::OutOfRange, "restitution is outside 0..1");
    if (patch.friction && !within(*patch.friction, 0.0f, 100.0f))
        return fail(ValidationCode::OutOfRange, "friction is outside 0..100");
    if (patch.body_mode && *patch.body_mode > 2)
        return fail(ValidationCode::OutOfRange, "body mode is outside 0..2");
    if (patch.collision_group && *patch.collision_group > 15)
        return fail(ValidationCode::OutOfRange, "collision group is outside 0..15");
    return {};
}

ValidationResult validate_patch(const JointPatch& patch) {
    if (!any(patch)) return fail(ValidationCode::EmptyPatch, "joint patch is empty");

    const std::array<const std::optional<Vec3>*, 6> vectors{{
        &patch.translation_lower,
        &patch.translation_upper,
        &patch.rotation_lower,
        &patch.rotation_upper,
        &patch.spring_translation,
        &patch.spring_rotation,
    }};
    for (const auto* value : vectors) {
        if (*value && !finite(**value))
            return fail(ValidationCode::NonFinite, "joint patch contains a non-finite value");
    }

    if (patch.translation_lower && patch.translation_upper &&
        !ordered(*patch.translation_lower, *patch.translation_upper)) {
        return fail(ValidationCode::InvalidLimits, "translation lower limit exceeds upper limit");
    }
    if (patch.rotation_lower && patch.rotation_upper &&
        !ordered(*patch.rotation_lower, *patch.rotation_upper)) {
        return fail(ValidationCode::InvalidLimits, "rotation lower limit exceeds upper limit");
    }

    const auto valid_spring = [](const Vec3& value) {
        return within(value.x, 0.0f, 10'000'000.0f) &&
            within(value.y, 0.0f, 10'000'000.0f) &&
            within(value.z, 0.0f, 10'000'000.0f);
    };
    if (patch.spring_translation && !valid_spring(*patch.spring_translation))
        return fail(ValidationCode::OutOfRange, "translation spring is outside the supported range");
    if (patch.spring_rotation && !valid_spring(*patch.spring_rotation))
        return fail(ValidationCode::OutOfRange, "rotation spring is outside the supported range");
    if (patch.max_separation &&
        !within(*patch.max_separation, 0.0f, 10'000'000.0f)) {
        return fail(ValidationCode::OutOfRange, "maximum separation is outside the supported range");
    }
    return {};
}

ValidationResult validate_patch(const BonePhysicsOverride& patch) {
    if (!within(patch.translation_blend, 0.0f, 1.0f) ||
        !within(patch.rotation_blend, 0.0f, 1.0f)) {
        return fail(ValidationCode::OutOfRange, "bone blend is outside 0..1");
    }
    return {};
}

ValidationResult validate_patch(const WorldGravityPatch& patch) {
    if (!finite(patch.gravity))
        return fail(ValidationCode::NonFinite, "gravity contains a non-finite value");
    if (!within(patch.gravity.x, -10'000.0f, 10'000.0f) ||
        !within(patch.gravity.y, -10'000.0f, 10'000.0f) ||
        !within(patch.gravity.z, -10'000.0f, 10'000.0f)) {
        return fail(ValidationCode::OutOfRange, "gravity is outside the supported world range");
    }
    return {};
}

ValidationResult validate_patch(const RigidBodyImpulse& patch) {
    if (!finite(patch.linear) || !finite(patch.angular))
        return fail(ValidationCode::NonFinite, "impulse contains a non-finite value");
    return {};
}

ValidationResult validate_patch(const ResetRigidBody&) {
    return {};
}

}  // namespace

Capability required_capabilities(const Command& command) noexcept {
    return std::visit(
        [](const auto& payload) {
            using Type = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Type, RigidBodyPatch>) {
                return Capability::ModelRecordWrite | Capability::BulletBodyWrite;
            } else if constexpr (std::is_same_v<Type, JointPatch>) {
                return Capability::ModelRecordWrite | Capability::ConstraintWrite;
            } else if constexpr (std::is_same_v<Type, BonePhysicsOverride>) {
                return Capability::BoneWritebackOverride;
            } else if constexpr (std::is_same_v<Type, WorldGravityPatch>) {
                return Capability::WorldWrite;
            } else {
                return Capability::BulletBodyWrite;
            }
        },
        command.payload);
}

ValidationResult validate(const Command& command) {
    const auto target = validate_target(command);
    if (!target) return target;
    return std::visit([](const auto& payload) { return validate_patch(payload); }, command.payload);
}

ValidationResult CommandQueue::enqueue(Command command) {
    const auto result = validate(command);
    if (!result) return result;
    commands_.push_back(std::move(command));
    return {};
}

ExecutionReport CommandQueue::execute(Backend& backend) {
    ExecutionReport report{};
    report.submitted = commands_.size();
    if (commands_.empty()) {
        report.committed = true;
        return report;
    }

    for (const auto& command : commands_) {
        const Capability required = required_capabilities(command);
        if (!has_all(backend.capabilities(), required)) {
            report.failed_command_id = command.id;
            report.code = ValidationCode::CapabilityMissing;
            report.message = "backend does not provide all required capabilities";
            return report;
        }
    }

    std::string error;
    if (!backend.begin_transaction(error)) {
        report.code = ValidationCode::BackendFailure;
        report.message = error.empty() ? "backend transaction could not start" : error;
        return report;
    }

    for (const auto& command : commands_) {
        if (!backend.apply(command, error)) {
            backend.rollback_transaction();
            report.failed_command_id = command.id;
            report.code = ValidationCode::BackendFailure;
            report.message = error.empty() ? "backend rejected command" : error;
            return report;
        }
        ++report.applied;
    }

    if (!backend.commit_transaction(error)) {
        backend.rollback_transaction();
        report.code = ValidationCode::BackendFailure;
        report.message = error.empty() ? "backend commit failed" : error;
        return report;
    }

    report.committed = true;
    commands_.clear();
    return report;
}

}  // namespace physics_control_studio

