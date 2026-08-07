#pragma once

#include "physics_control_studio/core.hpp"

#include "mmd_931_model.hpp"
#include "mmd_931_physics_formats.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace physics_control_studio {

struct ModelPhysicsInventory {
    std::uint32_t rigid_body_count = 0;
    std::uint32_t joint_count = 0;
};

class ModelMemoryView {
public:
    explicit ModelMemoryView(const void* model) noexcept : model_(model) {}

    bool available() const noexcept { return model_ != nullptr; }

    std::optional<ModelPhysicsInventory> inventory() const noexcept {
        if (model_ == nullptr) return std::nullopt;

        ModelPhysicsInventory result{};
        read_value(mmd931::model::state::kRigidBodyCount, result.rigid_body_count);
        read_value(mmd931::model::state::kJointCount, result.joint_count);
        if (result.rigid_body_count > kMaximumRuntimeElements ||
            result.joint_count > kMaximumRuntimeElements) {
            return std::nullopt;
        }
        return result;
    }

    std::optional<mmd931::model::physics::RigidBodyRecord> rigid_body(
        std::uint32_t index) const noexcept {
        const auto counts = inventory();
        if (!counts || index >= counts->rigid_body_count) return std::nullopt;

        std::uintptr_t records = 0;
        read_value(mmd931::model::state::kRigidBodies, records);
        if (records == 0) return std::nullopt;

        mmd931::model::physics::RigidBodyRecord result{};
        std::memcpy(
            &result,
            reinterpret_cast<const void*>(
                records + static_cast<std::uintptr_t>(index) *
                    mmd931::model::rigid_body::kStride),
            sizeof(result));
        return result;
    }

    std::optional<mmd931::model::physics::JointRecord> joint(
        std::uint32_t index) const noexcept {
        const auto counts = inventory();
        if (!counts || index >= counts->joint_count) return std::nullopt;

        std::uintptr_t records = 0;
        read_value(mmd931::model::state::kJoints, records);
        if (records == 0) return std::nullopt;

        mmd931::model::physics::JointRecord result{};
        std::memcpy(
            &result,
            reinterpret_cast<const void*>(
                records + static_cast<std::uintptr_t>(index) *
                    mmd931::model::joint::kStride),
            sizeof(result));
        return result;
    }

private:
    template <typename Value>
    void read_value(std::size_t offset, Value& output) const noexcept {
        std::memcpy(
            &output,
            static_cast<const std::byte*>(model_) + offset,
            sizeof(output));
    }

    const void* model_ = nullptr;
};

}  // namespace physics_control_studio

