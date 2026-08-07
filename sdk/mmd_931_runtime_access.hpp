#pragma once

#include "mmd_931_runtime.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace mmd931::runtime_access {

inline constexpr wchar_t kExecutableName[] = L"MikuMikudance.exe";
inline constexpr char kExecutableSha256[] =
    "2C9414C21619B4AD85D9C2EF76836F3C34DB7A8ABD07BD6C6176D385F7EFDFB4";
inline constexpr std::uint64_t kExecutableSize = 1723392;
inline constexpr std::size_t kObjectSlotCapacity = 255;
inline constexpr std::size_t kUndoCapacity = 30;
inline constexpr std::size_t kSnapshotAttempts = 3;
inline constexpr std::uint32_t kMaximumPlausibleElementCount = 1000000;

namespace main_state {
inline constexpr std::size_t kEditMode = 0x00000328;
inline constexpr std::size_t kOperationMode = 0x00000378;
inline constexpr std::size_t kModelTable = 0x00000be8;
inline constexpr std::size_t kSelectedModel = 0x000013e0;
inline constexpr std::size_t kViewMode = 0x000013e4;
inline constexpr std::size_t kBackgroundAviEnabled = 0x000013ec;
inline constexpr std::size_t kTimelineStartFrame = 0x0000144c;
inline constexpr std::size_t kCurrentFrame = 0x00001450;
inline constexpr std::size_t kAccessoryTable = 0x0009e840;
inline constexpr std::size_t kMaximumFrame = 0x0009f038;
inline constexpr std::size_t kSelectedAccessory = 0x0009f03c;
inline constexpr std::size_t kImageEnabled = 0x0009f310;
inline constexpr std::size_t kScreenCaptureMode = 0x0009fa80;
inline constexpr std::size_t kTransparentGroundShadow = 0x0009fca2;
inline constexpr std::size_t kGravityDirection = 0x0009fcc4;
inline constexpr std::size_t kGravityAcceleration = 0x0009fcd0;
inline constexpr std::size_t kGravityNoiseStrength = 0x0009fcd4;
inline constexpr std::size_t kMainWindow = 0x000a16c8;
inline constexpr std::size_t kWaveEnabled = 0x000a16ec;
inline constexpr std::size_t kRenderWidth = 0x000a18f8;
inline constexpr std::size_t kRenderHeight = 0x000a18fc;
inline constexpr std::size_t kFrameTimingValue = 0x000a1904;
inline constexpr std::size_t kProjectDirty = 0x000a1b31;
inline constexpr std::size_t kPreAccessoryCount = 0x000a1b50;
inline constexpr std::size_t kPhysicsMode = 0x000a1d4c;
inline constexpr std::size_t kGravityNoiseEnabled = 0x000a1d68;
inline constexpr std::size_t kMediaPlaybackRate = 0x000a1d88;
}  // namespace main_state

namespace model {
inline constexpr std::size_t kBoneArray = 0x00002748;
inline constexpr std::size_t kMorphArray = 0x00002758;
inline constexpr std::size_t kUndoRing = 0x000027a8;
inline constexpr std::size_t kUndoStride = 0x28;
inline constexpr std::size_t kUndoType = 0x00;
inline constexpr std::size_t kUndoFrame = 0x0c;
inline constexpr std::size_t kEnabled = 0x00003109;
inline constexpr std::size_t kMorphCount = 0x0000310c;
inline constexpr std::size_t kBoneCount = 0x00003110;
inline constexpr std::size_t kMaximumFrame = 0x0000354c;
inline constexpr std::size_t kUndoIndex = 0x00003550;
inline constexpr std::size_t kPhysicsEnabled = 0x0000355a;
inline constexpr std::size_t kEdgeScale = 0x0000355c;
inline constexpr std::size_t kVisible = 0x00003b68;
}  // namespace model

namespace bone {
inline constexpr std::size_t kStride = 0x270;
inline constexpr std::size_t kTranslation = 0x148;
inline constexpr std::size_t kQuaternion = 0x154;
inline constexpr std::size_t kFlags = 0x1f5;
}  // namespace bone

namespace morph {
inline constexpr std::size_t kStride = 0x0c0;
inline constexpr std::size_t kWeight = 0x038;
}  // namespace morph

namespace accessory {
inline constexpr std::size_t kPosition = 0x224;
inline constexpr std::size_t kRotation = 0x230;
inline constexpr std::size_t kScale = 0x23c;
inline constexpr std::size_t kCastsShadow = 0x4ac;
inline constexpr std::size_t kAlpha = 0x4b0;
}  // namespace accessory

class Reader {
public:
    virtual ~Reader() = default;
    virtual bool read(
        std::uintptr_t address,
        void *destination,
        std::size_t size) const noexcept = 0;
};

class ProcessReader final : public Reader {
public:
    explicit ProcessReader(HANDLE process) noexcept : process_(process) {}

    bool read(
        std::uintptr_t address,
        void *destination,
        std::size_t size) const noexcept override {
        SIZE_T bytes_read = 0;
        return process_ != nullptr &&
            ReadProcessMemory(
                process_,
                reinterpret_cast<const void *>(address),
                destination,
                size,
                &bytes_read) != FALSE &&
            bytes_read == size;
    }

private:
    HANDLE process_ = nullptr;
};

enum class SnapshotStatus : std::uint8_t {
    Ok,
    Partial,
    UnsupportedHost,
    InvalidModuleBase,
    MainStateUnavailable,
    ReadFailure,
    Unstable,
};

inline constexpr std::string_view status_name(SnapshotStatus status) noexcept {
    switch (status) {
    case SnapshotStatus::Ok: return "ok";
    case SnapshotStatus::Partial: return "partial";
    case SnapshotStatus::UnsupportedHost: return "unsupported_host";
    case SnapshotStatus::InvalidModuleBase: return "invalid_module_base";
    case SnapshotStatus::MainStateUnavailable: return "main_state_unavailable";
    case SnapshotStatus::ReadFailure: return "read_failure";
    case SnapshotStatus::Unstable: return "unstable";
    }
    return "unknown";
}

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct UndoSnapshot {
    bool available = false;
    std::int32_t index = -1;
    std::int32_t type = -1;
    std::int32_t frame = -1;
};

struct BoneSnapshot {
    bool available = false;
    Vec3 translation{};
    Quaternion quaternion{};
    std::array<std::uint8_t, 3> flags{};
};

struct MorphSnapshot {
    bool available = false;
    float weight = 0.0f;
};

struct ModelSnapshot {
    std::uint16_t slot = 0;
    std::uintptr_t address = 0;
    bool selected = false;
    bool readable = false;
    bool enabled = false;
    bool visible = false;
    bool physics_enabled = false;
    std::uint32_t bone_count = 0;
    std::uint32_t morph_count = 0;
    std::uint32_t maximum_frame = 0;
    float edge_scale = 0.0f;
    UndoSnapshot undo{};
    BoneSnapshot first_bone{};
    MorphSnapshot first_morph{};
};

struct AccessorySnapshot {
    std::uint16_t slot = 0;
    std::uintptr_t address = 0;
    bool selected = false;
    bool readable = false;
    Vec3 position{};
    Vec3 rotation{};
    float scale = 0.0f;
    float alpha = 0.0f;
    bool casts_shadow = false;
};

struct ProjectSnapshot {
    SnapshotStatus status = SnapshotStatus::ReadFailure;
    bool stable = false;
    std::size_t attempts = 0;
    std::uintptr_t module_base = 0;
    std::uintptr_t main_state = 0;
    std::uintptr_t main_window = 0;
    std::uintptr_t failed_address = 0;
    std::uint8_t edit_mode = 0;
    std::uint8_t operation_mode = 0;
    std::uint8_t selected_model = 0;
    std::uint8_t selected_accessory = 0;
    std::int32_t view_mode = 0;
    std::uint32_t timeline_start_frame = 0;
    std::uint32_t current_frame = 0;
    std::uint32_t maximum_frame = 0;
    bool background_avi_enabled = false;
    bool wave_enabled = false;
    bool image_enabled = false;
    std::int32_t screen_capture_mode = 0;
    bool transparent_ground_shadow = false;
    Vec3 gravity_direction{};
    float gravity_acceleration = 0.0f;
    std::uint32_t gravity_noise_strength = 0;
    bool gravity_noise_enabled = false;
    std::uint32_t render_width = 0;
    std::uint32_t render_height = 0;
    float frame_timing_value = 0.0f;
    bool project_dirty = false;
    std::int32_t pre_accessory_count = 0;
    std::uint8_t physics_mode = 0;
    float media_playback_rate = 0.0f;
    std::vector<ModelSnapshot> models;
    std::vector<AccessorySnapshot> accessories;
    std::vector<std::uintptr_t> unreadable_objects;
};

inline ProjectSnapshot unavailable_snapshot(SnapshotStatus status) {
    ProjectSnapshot snapshot{};
    snapshot.status = status;
    return snapshot;
}

class RuntimeAccess {
public:
    RuntimeAccess(const Reader &reader, std::uintptr_t module_base) noexcept
        : reader_(reader), module_base_(module_base) {}

    ProjectSnapshot capture() const {
        ProjectSnapshot snapshot{};
        for (std::size_t attempt = 1; attempt <= kSnapshotAttempts; ++attempt) {
            snapshot = capture_once(attempt);
            if (snapshot.status != SnapshotStatus::Unstable) {
                return snapshot;
            }
        }
        return snapshot;
    }

private:
    struct Checkpoint {
        std::uintptr_t main_state = 0;
        std::uint32_t current_frame = 0;
        std::uint8_t selected_model = 0;
        std::uint8_t selected_accessory = 0;
    };

    bool add_offset(
        std::uintptr_t base,
        std::size_t offset,
        std::uintptr_t &address) const noexcept {
        if (base > std::numeric_limits<std::uintptr_t>::max() - offset) {
            return false;
        }
        address = base + offset;
        return true;
    }

    bool read_bytes(
        std::uintptr_t address,
        void *destination,
        std::size_t size,
        ProjectSnapshot &snapshot) const noexcept {
        if (!reader_.read(address, destination, size)) {
            snapshot.failed_address = address;
            return false;
        }
        return true;
    }

    template <typename T>
    bool read_value(
        std::uintptr_t base,
        std::size_t offset,
        T &value,
        ProjectSnapshot &snapshot) const noexcept {
        std::uintptr_t address = 0;
        return add_offset(base, offset, address) &&
            read_bytes(address, &value, sizeof(value), snapshot);
    }

    bool read_checkpoint(
        std::uintptr_t state,
        Checkpoint &checkpoint,
        ProjectSnapshot &snapshot) const noexcept {
        checkpoint.main_state = state;
        return read_value(
                   state,
                   main_state::kCurrentFrame,
                   checkpoint.current_frame,
                   snapshot) &&
            read_value(
                   state,
                   main_state::kSelectedModel,
                   checkpoint.selected_model,
                   snapshot) &&
            read_value(
                   state,
                   main_state::kSelectedAccessory,
                   checkpoint.selected_accessory,
                   snapshot);
    }

    bool read_model(ModelSnapshot &output, ProjectSnapshot &snapshot) const noexcept {
        std::uintptr_t bone_array = 0;
        std::uintptr_t morph_array = 0;
        std::uint8_t enabled = 0;
        std::uint8_t visible = 0;
        std::uint8_t physics_enabled = 0;
        if (!read_value(output.address, model::kEnabled, enabled, snapshot) ||
            !read_value(output.address, model::kVisible, visible, snapshot) ||
            !read_value(output.address, model::kPhysicsEnabled, physics_enabled, snapshot) ||
            !read_value(output.address, model::kBoneCount, output.bone_count, snapshot) ||
            !read_value(output.address, model::kMorphCount, output.morph_count, snapshot) ||
            !read_value(output.address, model::kMaximumFrame, output.maximum_frame, snapshot) ||
            !read_value(output.address, model::kEdgeScale, output.edge_scale, snapshot) ||
            !read_value(output.address, model::kUndoIndex, output.undo.index, snapshot) ||
            !read_value(output.address, model::kBoneArray, bone_array, snapshot) ||
            !read_value(output.address, model::kMorphArray, morph_array, snapshot)) {
            return false;
        }
        if (output.bone_count > kMaximumPlausibleElementCount ||
            output.morph_count > kMaximumPlausibleElementCount) {
            return false;
        }
        output.enabled = enabled != 0;
        output.visible = visible != 0;
        output.physics_enabled = physics_enabled != 0;

        if (output.undo.index >= 0 &&
            static_cast<std::size_t>(output.undo.index) < kUndoCapacity) {
            const std::size_t undo_offset = model::kUndoRing +
                static_cast<std::size_t>(output.undo.index) * model::kUndoStride;
            if (!read_value(
                    output.address,
                    undo_offset + model::kUndoType,
                    output.undo.type,
                    snapshot) ||
                !read_value(
                    output.address,
                    undo_offset + model::kUndoFrame,
                    output.undo.frame,
                    snapshot)) {
                return false;
            }
            output.undo.available = true;
        }

        if (output.bone_count != 0 && bone_array != 0) {
            std::uintptr_t translation = 0;
            std::uintptr_t quaternion = 0;
            std::uintptr_t flags = 0;
            if (!add_offset(bone_array, bone::kTranslation, translation) ||
                !add_offset(bone_array, bone::kQuaternion, quaternion) ||
                !add_offset(bone_array, bone::kFlags, flags) ||
                !read_bytes(
                    translation,
                    &output.first_bone.translation,
                    sizeof(output.first_bone.translation),
                    snapshot) ||
                !read_bytes(
                    quaternion,
                    &output.first_bone.quaternion,
                    sizeof(output.first_bone.quaternion),
                    snapshot) ||
                !read_bytes(
                    flags,
                    output.first_bone.flags.data(),
                    output.first_bone.flags.size(),
                    snapshot)) {
                return false;
            }
            output.first_bone.available = true;
        }

        if (output.morph_count != 0 && morph_array != 0) {
            if (!read_value(
                    morph_array,
                    morph::kWeight,
                    output.first_morph.weight,
                    snapshot)) {
                return false;
            }
            output.first_morph.available = true;
        }
        output.readable = true;
        return true;
    }

    bool read_accessory(
        AccessorySnapshot &output,
        ProjectSnapshot &snapshot) const noexcept {
        std::uint8_t casts_shadow = 0;
        if (!read_value(output.address, accessory::kPosition, output.position, snapshot) ||
            !read_value(output.address, accessory::kRotation, output.rotation, snapshot) ||
            !read_value(output.address, accessory::kScale, output.scale, snapshot) ||
            !read_value(output.address, accessory::kAlpha, output.alpha, snapshot) ||
            !read_value(
                output.address,
                accessory::kCastsShadow,
                casts_shadow,
                snapshot)) {
            return false;
        }
        output.casts_shadow = casts_shadow != 0;
        output.readable = true;
        return true;
    }

    ProjectSnapshot capture_once(std::size_t attempt) const {
        ProjectSnapshot snapshot{};
        snapshot.attempts = attempt;
        snapshot.module_base = module_base_;
        if (module_base_ == 0) {
            snapshot.status = SnapshotStatus::InvalidModuleBase;
            return snapshot;
        }

        std::uintptr_t pointer_address = 0;
        if (!add_offset(
                module_base_,
                mmd931::runtime::kMainStatePointerRva,
                pointer_address) ||
            !read_bytes(
                pointer_address,
                &snapshot.main_state,
                sizeof(snapshot.main_state),
                snapshot) ||
            snapshot.main_state == 0) {
            snapshot.status = SnapshotStatus::MainStateUnavailable;
            return snapshot;
        }

        Checkpoint before{};
        if (!read_checkpoint(snapshot.main_state, before, snapshot)) {
            snapshot.status = SnapshotStatus::ReadFailure;
            return snapshot;
        }

        std::uint8_t background_avi_enabled = 0;
        std::uint8_t wave_enabled = 0;
        std::uint8_t image_enabled = 0;
        std::uint8_t transparent_ground_shadow = 0;
        std::uint8_t gravity_noise_enabled = 0;
        std::uint8_t project_dirty = 0;
        std::array<std::uintptr_t, kObjectSlotCapacity> model_table{};
        std::array<std::uintptr_t, kObjectSlotCapacity> accessory_table{};
        const std::uintptr_t state = snapshot.main_state;
        if (!read_value(state, main_state::kEditMode, snapshot.edit_mode, snapshot) ||
            !read_value(
                state,
                main_state::kOperationMode,
                snapshot.operation_mode,
                snapshot) ||
            !read_value(
                state,
                main_state::kSelectedModel,
                snapshot.selected_model,
                snapshot) ||
            !read_value(
                state,
                main_state::kSelectedAccessory,
                snapshot.selected_accessory,
                snapshot) ||
            !read_value(state, main_state::kViewMode, snapshot.view_mode, snapshot) ||
            !read_value(
                state,
                main_state::kTimelineStartFrame,
                snapshot.timeline_start_frame,
                snapshot) ||
            !read_value(
                state,
                main_state::kCurrentFrame,
                snapshot.current_frame,
                snapshot) ||
            !read_value(
                state,
                main_state::kMaximumFrame,
                snapshot.maximum_frame,
                snapshot) ||
            !read_value(
                state,
                main_state::kBackgroundAviEnabled,
                background_avi_enabled,
                snapshot) ||
            !read_value(state, main_state::kWaveEnabled, wave_enabled, snapshot) ||
            !read_value(state, main_state::kImageEnabled, image_enabled, snapshot) ||
            !read_value(
                state,
                main_state::kScreenCaptureMode,
                snapshot.screen_capture_mode,
                snapshot) ||
            !read_value(
                state,
                main_state::kTransparentGroundShadow,
                transparent_ground_shadow,
                snapshot) ||
            !read_value(
                state,
                main_state::kGravityDirection,
                snapshot.gravity_direction,
                snapshot) ||
            !read_value(
                state,
                main_state::kGravityAcceleration,
                snapshot.gravity_acceleration,
                snapshot) ||
            !read_value(
                state,
                main_state::kGravityNoiseStrength,
                snapshot.gravity_noise_strength,
                snapshot) ||
            !read_value(
                state,
                main_state::kGravityNoiseEnabled,
                gravity_noise_enabled,
                snapshot) ||
            !read_value(
                state,
                main_state::kMainWindow,
                snapshot.main_window,
                snapshot) ||
            !read_value(
                state,
                main_state::kRenderWidth,
                snapshot.render_width,
                snapshot) ||
            !read_value(
                state,
                main_state::kRenderHeight,
                snapshot.render_height,
                snapshot) ||
            !read_value(
                state,
                main_state::kFrameTimingValue,
                snapshot.frame_timing_value,
                snapshot) ||
            !read_value(
                state,
                main_state::kProjectDirty,
                project_dirty,
                snapshot) ||
            !read_value(
                state,
                main_state::kPreAccessoryCount,
                snapshot.pre_accessory_count,
                snapshot) ||
            !read_value(
                state,
                main_state::kPhysicsMode,
                snapshot.physics_mode,
                snapshot) ||
            !read_value(
                state,
                main_state::kMediaPlaybackRate,
                snapshot.media_playback_rate,
                snapshot) ||
            !read_value(state, main_state::kModelTable, model_table, snapshot) ||
            !read_value(
                state,
                main_state::kAccessoryTable,
                accessory_table,
                snapshot)) {
            snapshot.status = SnapshotStatus::ReadFailure;
            return snapshot;
        }

        snapshot.background_avi_enabled = background_avi_enabled != 0;
        snapshot.wave_enabled = wave_enabled != 0;
        snapshot.image_enabled = image_enabled != 0;
        snapshot.transparent_ground_shadow = transparent_ground_shadow != 0;
        snapshot.gravity_noise_enabled = gravity_noise_enabled != 0;
        snapshot.project_dirty = project_dirty != 0;

        bool partial = false;
        for (std::size_t slot = 0; slot < model_table.size(); ++slot) {
            if (model_table[slot] == 0) {
                continue;
            }
            ModelSnapshot object{};
            object.slot = static_cast<std::uint16_t>(slot);
            object.address = model_table[slot];
            object.selected = slot == snapshot.selected_model;
            if (!read_model(object, snapshot)) {
                partial = true;
                snapshot.unreadable_objects.push_back(object.address);
            }
            snapshot.models.push_back(object);
        }
        for (std::size_t slot = 0; slot < accessory_table.size(); ++slot) {
            if (accessory_table[slot] == 0) {
                continue;
            }
            AccessorySnapshot object{};
            object.slot = static_cast<std::uint16_t>(slot);
            object.address = accessory_table[slot];
            object.selected = slot == snapshot.selected_accessory;
            if (!read_accessory(object, snapshot)) {
                partial = true;
                snapshot.unreadable_objects.push_back(object.address);
            }
            snapshot.accessories.push_back(object);
        }

        Checkpoint after{};
        std::uintptr_t final_main_state = 0;
        if (!read_checkpoint(state, after, snapshot) ||
            !read_bytes(
                pointer_address,
                &final_main_state,
                sizeof(final_main_state),
                snapshot)) {
            snapshot.status = SnapshotStatus::ReadFailure;
            return snapshot;
        }
        snapshot.stable = final_main_state == state &&
            before.current_frame == after.current_frame &&
            before.selected_model == after.selected_model &&
            before.selected_accessory == after.selected_accessory;
        if (!snapshot.stable) {
            snapshot.status = SnapshotStatus::Unstable;
            return snapshot;
        }
        snapshot.status = partial ? SnapshotStatus::Partial : SnapshotStatus::Ok;
        return snapshot;
    }

    const Reader &reader_;
    std::uintptr_t module_base_ = 0;
};

}  // namespace mmd931::runtime_access
