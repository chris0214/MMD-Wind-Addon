#pragma once

#include "physics_control_studio/physics_track.hpp"

#include <string>
#include <string_view>

namespace physics_control_studio {

std::string serialize_track_json(
    const ControlSnapshot& current,
    const PhysicsTrack& track);

std::string serialize_track_json(
    const ControlSnapshot& current,
    const PhysicsTrack& track,
    const std::vector<TargetGroup>& target_groups);

bool deserialize_track_json(
    std::string_view json,
    ControlSnapshot& current,
    PhysicsTrack& track,
    std::string& error);

bool deserialize_track_json(
    std::string_view json,
    ControlSnapshot& current,
    PhysicsTrack& track,
    std::vector<TargetGroup>& target_groups,
    std::string& error);

}  // namespace physics_control_studio
