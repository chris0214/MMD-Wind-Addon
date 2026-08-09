#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace physics_control_studio {

template <std::size_t ModelCount, typename HasRigidBodies>
std::uintptr_t select_physics_target_model(
    std::size_t selected_index,
    std::uintptr_t cached_model,
    const std::array<std::uintptr_t, ModelCount>& models,
    HasRigidBodies&& has_rigid_bodies) {
    const auto valid = [&](std::uintptr_t model) {
        return model != 0 && has_rigid_bodies(model);
    };

    if (selected_index < models.size() && valid(models[selected_index])) {
        return models[selected_index];
    }
    if (cached_model != 0 &&
        std::find(models.begin(), models.end(), cached_model) != models.end() &&
        valid(cached_model)) {
        return cached_model;
    }
    for (const std::uintptr_t model : models) {
        if (model != cached_model && valid(model)) return model;
    }
    return 0;
}

}  // namespace physics_control_studio
