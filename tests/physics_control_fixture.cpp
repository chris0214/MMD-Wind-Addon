#include "physics_control_studio/bullet_runtime_layout.hpp"
#include "physics_control_studio/core.hpp"
#include "physics_control_studio/model_memory_view.hpp"
#include "physics_control_studio/physics_track.hpp"
#include "physics_control_studio/track_json.hpp"
#include "physics_control_studio/wind.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace pcs = physics_control_studio;

namespace {

int g_cases = 0;
int g_failures = 0;

void check(bool condition, const char* name) {
    ++g_cases;
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL " << name << '\n';
    }
}

class FakeBackend final : public pcs::Backend {
public:
    explicit FakeBackend(pcs::Capability capabilities) : capabilities_(capabilities) {}

    pcs::Capability capabilities() const noexcept override { return capabilities_; }

    bool begin_transaction(std::string&) override {
        ++begin_count;
        staged.clear();
        return begin_succeeds;
    }

    bool apply(const pcs::Command& command, std::string& error) override {
        if (command.id == fail_on_id) {
            error = "injected apply failure";
            return false;
        }
        staged.push_back(command.id);
        return true;
    }

    bool commit_transaction(std::string& error) override {
        if (!commit_succeeds) {
            error = "injected commit failure";
            return false;
        }
        committed = staged;
        ++commit_count;
        return true;
    }

    void rollback_transaction() noexcept override {
        staged.clear();
        ++rollback_count;
    }

    pcs::Capability capabilities_ = pcs::Capability::None;
    bool begin_succeeds = true;
    bool commit_succeeds = true;
    std::uint64_t fail_on_id = 0;
    int begin_count = 0;
    int commit_count = 0;
    int rollback_count = 0;
    std::vector<std::uint64_t> staged;
    std::vector<std::uint64_t> committed;
};

pcs::Command rigid_command(std::uint64_t id, float mass) {
    pcs::RigidBodyPatch patch{};
    patch.mass = mass;
    return {id, {pcs::EntityKind::RigidBody, 0, 3}, patch};
}

void test_validation() {
    check(static_cast<bool>(pcs::validate(rigid_command(1, 2.0f))), "valid rigid body patch");
    check(
        pcs::validate(rigid_command(2, -1.0f)).code == pcs::ValidationCode::OutOfRange,
        "negative mass rejected");

    pcs::RigidBodyPatch empty{};
    pcs::Command empty_command{3, {pcs::EntityKind::RigidBody, 0, 0}, empty};
    check(
        pcs::validate(empty_command).code == pcs::ValidationCode::EmptyPatch,
        "empty rigid body patch rejected");

    pcs::JointPatch joint{};
    joint.rotation_lower = pcs::Vec3{1.0f, 0.0f, 0.0f};
    joint.rotation_upper = pcs::Vec3{-1.0f, 0.0f, 0.0f};
    pcs::Command joint_command{4, {pcs::EntityKind::Joint, 0, 1}, joint};
    check(
        pcs::validate(joint_command).code == pcs::ValidationCode::InvalidLimits,
        "inverted joint limit rejected");

    pcs::WorldGravityPatch gravity{};
    gravity.gravity.x = std::numeric_limits<float>::infinity();
    pcs::Command gravity_command{5, {pcs::EntityKind::World, 0, 0}, gravity};
    check(
        pcs::validate(gravity_command).code == pcs::ValidationCode::NonFinite,
        "non-finite gravity rejected");

    pcs::BonePhysicsOverride bone{};
    bone.rotation_blend = 1.1f;
    pcs::Command bone_command{6, {pcs::EntityKind::Bone, 0, 9}, bone};
    check(
        pcs::validate(bone_command).code == pcs::ValidationCode::OutOfRange,
        "bone blend outside range rejected");
}

void test_capabilities_and_transactions() {
    pcs::CommandQueue queue;
    check(static_cast<bool>(queue.enqueue(rigid_command(10, 4.0f))), "enqueue rigid command");

    FakeBackend read_only(pcs::Capability::None);
    const auto refused = queue.execute(read_only);
    check(!refused.committed, "read-only backend refuses write");
    check(refused.code == pcs::ValidationCode::CapabilityMissing, "missing capability reported");
    check(read_only.begin_count == 0, "capability gate precedes transaction");
    check(queue.size() == 1, "refused queue remains pending");

    FakeBackend writable(
        pcs::Capability::ModelRecordWrite | pcs::Capability::BulletBodyWrite);
    const auto accepted = queue.execute(writable);
    check(accepted.committed, "writable backend commits");
    check(accepted.applied == 1, "one command applied");
    check(queue.empty(), "committed queue clears");
    check(writable.committed == std::vector<std::uint64_t>{10}, "command id committed");

    check(static_cast<bool>(queue.enqueue(rigid_command(20, 1.0f))), "enqueue first rollback command");
    check(static_cast<bool>(queue.enqueue(rigid_command(21, 2.0f))), "enqueue second rollback command");
    writable.fail_on_id = 21;
    const auto failed = queue.execute(writable);
    check(!failed.committed, "failed batch does not commit");
    check(failed.applied == 1, "report preserves applied count before failure");
    check(writable.rollback_count == 1, "failed batch rolls back");
    check(queue.size() == 2, "failed queue remains available for retry");
}

void test_model_memory_view() {
    std::vector<std::byte> model(0x4000);
    std::array<mmd931::model::physics::RigidBodyRecord, 2> bodies{};
    std::array<mmd931::model::physics::JointRecord, 1> joints{};
    bodies[1].mass = 7.5f;
    bodies[1].bone_index = 12;
    joints[0].rigid_body_a = 0;
    joints[0].rigid_body_b = 1;
    joints[0].spring_rotation[2] = 8.0f;

    const std::uintptr_t body_pointer = reinterpret_cast<std::uintptr_t>(bodies.data());
    const std::uintptr_t joint_pointer = reinterpret_cast<std::uintptr_t>(joints.data());
    const std::uint32_t body_count = static_cast<std::uint32_t>(bodies.size());
    const std::uint32_t joint_count = static_cast<std::uint32_t>(joints.size());
    std::memcpy(model.data() + mmd931::model::state::kRigidBodies, &body_pointer, sizeof(body_pointer));
    std::memcpy(model.data() + mmd931::model::state::kJoints, &joint_pointer, sizeof(joint_pointer));
    std::memcpy(model.data() + mmd931::model::state::kRigidBodyCount, &body_count, sizeof(body_count));
    std::memcpy(model.data() + mmd931::model::state::kJointCount, &joint_count, sizeof(joint_count));

    pcs::ModelMemoryView view(model.data());
    const auto inventory = view.inventory();
    check(inventory && inventory->rigid_body_count == 2, "inventory reads rigid body count");
    check(inventory && inventory->joint_count == 1, "inventory reads joint count");

    const auto body = view.rigid_body(1);
    check(body && body->mass == 7.5f, "rigid body snapshot reads mass");
    check(body && body->bone_index == 12, "rigid body snapshot reads bone binding");
    check(!view.rigid_body(2), "rigid body index bounds checked");

    const auto joint = view.joint(0);
    check(joint && joint->rigid_body_b == 1, "joint snapshot reads body link");
    check(joint && joint->spring_rotation[2] == 8.0f, "joint snapshot reads spring");
}

void test_wind_field() {
    const pcs::Vec3 normalized = pcs::normalize_or_zero({3.0f, 4.0f, 0.0f});
    check(std::abs(normalized.x - 0.6f) < 0.0001f, "wind direction normalizes x");
    check(std::abs(normalized.y - 0.8f) < 0.0001f, "wind direction normalizes y");

    pcs::WindSettings settings{};
    settings.enabled = true;
    settings.direction = {1.0f, 0.0f, 0.0f};
    settings.strength = 60.0f;
    settings.gust = 0.4f;
    settings.turbulence = 0.2f;
    settings.frequency = 0.75f;
    settings.center = {0.0f, 0.0f, 0.0f};
    settings.collision_group_mask = 1u << 3;
    settings.maximum_speed = 20.0f;
    check(pcs::validate_wind_settings(settings), "wind settings validate");
    check(
        pcs::wind_modulation(settings, 2.5) == pcs::wind_modulation(settings, 2.5),
        "wind gust is deterministic");
    pcs::WindBodySample body{};
    body.body_mode = 1;
    body.collision_group = 3;
    body.position = {0.0f, 0.0f, 0.0f};
    const auto affected = pcs::evaluate_wind(settings, body, 0.0, 1.0f / 60.0f);
    check(affected.affected && affected.linear_velocity.x > 0.0f, "wind affects matching dynamic body");

    body.body_mode = 0;
    check(!pcs::evaluate_wind(settings, body, 0.0, 1.0f / 60.0f).affected,
          "wind skips bone-synchronized body");
    body.body_mode = 1;
    body.collision_group = 2;
    check(!pcs::evaluate_wind(settings, body, 0.0, 1.0f / 60.0f).affected,
          "wind respects collision group filter");

    body.collision_group = 3;
    body.linear_velocity = {19.9f, 0.0f, 0.0f};
    const auto limited = pcs::evaluate_wind(settings, body, 0.0, 1.0f / 60.0f);
    check(limited.linear_velocity.x <= 20.0001f, "wind clamps maximum speed");

    settings.maximum_speed = 200.0f;
    settings.gust = 0.0f;
    settings.turbulence = 0.0f;
    settings.direction = {0.0f, 1.0f, 0.0f};
    settings.field_type = pcs::WindFieldType::Directional;
    body.position = {5.0f, 0.0f, 0.0f};
    body.linear_velocity = {};
    const auto upward = pcs::evaluate_wind(settings, body, 0.0, 1.0f / 60.0f);
    check(upward.linear_velocity.y > 0.0f && upward.linear_velocity.x == 0.0f,
          "directional field follows the selected axis");

    settings.field_type = pcs::WindFieldType::Vortex;
    const auto vortex = pcs::evaluate_wind(settings, body, 0.0, 1.0f / 60.0f);
    check(vortex.linear_velocity.z < 0.0f && std::abs(vortex.linear_velocity.x) < 0.0001f,
          "vortex field circles around the selected axis");

    settings.field_type = pcs::WindFieldType::RadialOut;
    const auto radial_out = pcs::evaluate_wind(settings, body, 0.0, 1.0f / 60.0f);
    check(radial_out.linear_velocity.x > 0.0f, "radial out field pushes from center");

    settings.field_type = pcs::WindFieldType::RadialIn;
    const auto radial_in = pcs::evaluate_wind(settings, body, 0.0, 1.0f / 60.0f);
    check(radial_in.linear_velocity.x < 0.0f, "radial in field pulls toward center");

    settings.field_type = pcs::WindFieldType::Turbulence;
    const auto turbulence = pcs::evaluate_wind(settings, body, 1.0, 1.0f / 60.0f);
    check(turbulence.affected &&
              (std::abs(turbulence.linear_velocity.x) > 0.0001f ||
               std::abs(turbulence.linear_velocity.y) > 0.0001f ||
               std::abs(turbulence.linear_velocity.z) > 0.0001f),
          "turbulence field produces spatial motion");

    const std::array<pcs::WindNoiseType, 5> noise_types{{
        pcs::WindNoiseType::Smooth,
        pcs::WindNoiseType::Perlin,
        pcs::WindNoiseType::Fractal,
        pcs::WindNoiseType::Pulse,
        pcs::WindNoiseType::RandomGust}};
    for (const auto noise_type : noise_types) {
        settings.noise_type = noise_type;
        check(
            pcs::validate_wind_settings(settings) &&
                pcs::wind_modulation(settings, 1.75) ==
                    pcs::wind_modulation(settings, 1.75),
            "professional wind noise is valid and deterministic");
    }

    settings.noise_type = pcs::WindNoiseType::Fractal;
    settings.field_type = pcs::WindFieldType::Updraft;
    check(
        pcs::evaluate_wind(settings, body, 1.0, 1.0f / 60.0f).linear_velocity.y > 0.0f,
        "updraft field rises around its center");
    settings.field_type = pcs::WindFieldType::Downburst;
    check(
        pcs::evaluate_wind(settings, body, 1.0, 1.0f / 60.0f).linear_velocity.y < 0.0f,
        "downburst field drives bodies downward");
    settings.field_type = pcs::WindFieldType::Shear;
    check(
        pcs::evaluate_wind(settings, body, 1.0, 1.0f / 60.0f).affected,
        "wind shear field produces a height-dependent force");
}

void test_physics_track() {
    pcs::PhysicsSettings physics{};
    check(pcs::validate_physics_settings(physics), "default physics settings validate");
    check(
        pcs::target_matches(physics.target, 7, 3, true),
        "all-dynamic target accepts dynamic body");
    physics.target = {pcs::TargetKind::CollisionGroup, 3};
    check(
        pcs::target_matches(physics.target, 7, 3, true) &&
            !pcs::target_matches(physics.target, 7, 2, true),
        "collision-group target filters bodies");
    physics.target = {pcs::TargetKind::RigidBody, 7};
    check(
        pcs::target_matches(physics.target, 7, 2, true) &&
            !pcs::target_matches(physics.target, 6, 2, true),
        "rigid-body target filters by index");
    physics.target = {};
    physics.target.kind = pcs::TargetKind::CustomSet;
    physics.target.collision_group_mask = 1u << 4;
    physics.target.rigid_body_indices = {7, 12};
    check(
        pcs::target_matches(physics.target, 3, 4, true) &&
            pcs::target_matches(physics.target, 7, 2, true) &&
            !pcs::target_matches(physics.target, 8, 2, true),
        "custom target combines collision groups and rigid bodies");

    pcs::ControlSnapshot first{};
    first.wind.enabled = true;
    first.wind.strength = 10.0f;
    first.physics.damping_enabled = true;
    first.physics.linear_damping = 0.1f;
    first.physics.gravity_enabled = true;
    first.physics.gravity_acceleration = 9.8f;
    pcs::ControlSnapshot second = first;
    second.wind.strength = 50.0f;
    second.physics.linear_damping = 0.9f;
    second.physics.gravity_acceleration = 1.8f;
    second.physics.target = {pcs::TargetKind::RigidBody, 12};

    pcs::PhysicsTrack track;
    track.set_key(10, first);
    track.set_key(30, second);
    check(track.size() == 2 && track.has_key(10), "physics track inserts sorted keys");
    const auto middle = track.evaluate(20, {});
    check(
        std::abs(middle.wind.strength - 30.0f) < 0.0001f &&
            std::abs(middle.physics.linear_damping - 0.5f) < 0.0001f &&
            std::abs(middle.physics.gravity_acceleration - 5.8f) < 0.0001f,
        "physics track linearly interpolates numeric controls");
    check(
        middle.physics.target.kind == pcs::TargetKind::AllDynamic,
        "physics track holds discrete target until next key");
    check(
        track.evaluate(30, {}).physics.target.kind == pcs::TargetKind::RigidBody,
        "physics track switches discrete target on key");
    check(track.erase_key(10) && !track.has_key(10), "physics track deletes key");
}

void test_track_json() {
    pcs::ControlSnapshot current{};
    current.wind.enabled = true;
    current.wind.field_type = pcs::WindFieldType::Downburst;
    current.wind.noise_type = pcs::WindNoiseType::Fractal;
    current.wind.strength = 125.0f;
    current.physics.damping_enabled = true;
    current.physics.target.kind = pcs::TargetKind::CustomSet;
    current.physics.target.collision_group_mask = static_cast<std::uint16_t>(1u << 6);
    current.physics.target.rigid_body_indices = {2, 9};
    pcs::PhysicsTrack source;
    source.set_key(12, current);
    auto second = current;
    second.wind.strength = 230.0f;
    second.physics.gravity_enabled = true;
    source.set_key(48, second);

    std::vector<pcs::TargetGroup> source_groups{
        {"hair", current.physics.target},
        {"skirt", {pcs::TargetKind::CollisionGroup, 3}}};
    const std::string json = pcs::serialize_track_json(current, source, source_groups);
    pcs::ControlSnapshot restored{};
    pcs::PhysicsTrack restored_track;
    std::vector<pcs::TargetGroup> restored_groups;
    std::string error;
    check(
        pcs::deserialize_track_json(
            json, restored, restored_track, restored_groups, error),
        "physics track JSON round trip parses");
    check(
        restored_track.size() == 2 && restored_track.has_key(12) &&
            restored.wind.noise_type == pcs::WindNoiseType::Fractal &&
            restored.physics.target.kind == pcs::TargetKind::CustomSet &&
            restored.physics.target.rigid_body_indices.size() == 2 &&
            restored_groups.size() == 2 && restored_groups[0].name == "hair" &&
            restored_groups[1].target.kind == pcs::TargetKind::CollisionGroup,
        "physics track JSON preserves professional controls and batch targets");
    const std::size_t groups_field = json.find(",\n  \"target_groups\"");
    const std::string legacy_json = groups_field == std::string::npos
        ? json
        : json.substr(0, groups_field) + "\n}\n";
    restored_groups = source_groups;
    check(
        pcs::deserialize_track_json(
            legacy_json, restored, restored_track, restored_groups, error) &&
            restored_groups.empty(),
        "legacy physics track JSON loads without target groups");
    check(
        !pcs::deserialize_track_json("{\"version\":1}", restored, restored_track, error),
        "incomplete physics track JSON is rejected");
}

void test_bullet_runtime_layout() {
    namespace layout = pcs::bullet_runtime_layout;
    check(layout::kWorldPosition == 0x40, "Bullet world position offset is version locked");
    check(
        layout::kInterpolationLinearVelocity == 0x90,
        "Bullet interpolation velocity offset is documented");
    check(
        layout::kLinearVelocity == 0x150 &&
            layout::kLinearVelocity != layout::kInterpolationLinearVelocity,
        "wind writes the solver linear velocity field");
    check(layout::kActivationState == 0xec, "Bullet activation offset is version locked");
    check(
        layout::kLinearDamping == 0x1f0 && layout::kAngularDamping == 0x1f4,
        "Bullet damping offsets are version locked");
    check(layout::kRigidBodySize == 0x250, "Bullet rigid body size is version locked");
}

}  // namespace

int main() {
    test_validation();
    test_capabilities_and_transactions();
    test_model_memory_view();
    test_wind_field();
    test_physics_track();
    test_track_json();
    test_bullet_runtime_layout();

    if (g_failures != 0) {
        std::cerr << "FAIL cases=" << g_cases << " failures=" << g_failures << '\n';
        return 1;
    }
    std::cout << "PASS cases=" << g_cases << '\n';
    return 0;
}
