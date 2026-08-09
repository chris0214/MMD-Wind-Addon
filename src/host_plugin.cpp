#include "physics_control_studio/core.hpp"
#include "physics_control_studio/host_api.hpp"
#include "physics_control_studio/bullet_runtime_layout.hpp"
#include "physics_control_studio/model_target.hpp"
#include "physics_control_studio/physics_track.hpp"
#include "physics_control_studio/track_json.hpp"
#include "physics_control_studio/wind.hpp"

#include "mmd_931_mmhack.hpp"
#include "mmd_931_model.hpp"
#include "mmd_931_morph_formats.hpp"
#include "mmd_931_physics_formats.hpp"
#include "mmd_931_runtime.hpp"
#include "mmd_931_runtime_access.hpp"

#include <windows.h>
#include <windowsx.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pcs_host = physics_control_studio::host_api;

namespace {

constexpr UINT_PTR kHostSubclassId = 0x50435331;
constexpr UINT_PTR kTargetListSubclassId = 0x50435332;
constexpr UINT_PTR kRefreshTimerId = 1;
constexpr UINT kRefreshIntervalMs = 500;
constexpr std::uint32_t kMaximumDisplayedItems = 512;
constexpr std::uint32_t kMaximumWindBodies = 65'536;
constexpr std::size_t kMaximumWindControllers = 16;
constexpr std::uint32_t kControllerMissingRescanFrames = 30;
constexpr std::uint32_t kControllerConnectedRescanFrames = 60;
constexpr float kMaximumCombinedWindAcceleration = 4'000.0f;
constexpr float kWindFilterTimeConstant = 0.08f;
constexpr int kPanelDefaultWidth = 430;
constexpr int kPanelDefaultHeight = 840;
constexpr int kPanelMinimumWidth = 360;
constexpr int kPanelMinimumHeight = 780;
constexpr int kPanelHeaderHeight = 56;
constexpr int kPanelFooterHeight = 42;
constexpr int kPanelPadding = 12;
constexpr int kPanelCornerRadius = 8;
constexpr int kPanelResizeBorder = 6;
constexpr int kCloseButtonSize = 32;
constexpr std::uint32_t kMaximumControllerMorphs = 4'096;
constexpr std::uint32_t kMaximumControllerBones = 4'096;
constexpr int kMaximumPanelWindStrength = 1'000;
constexpr float kControllerMinimumRadius = 2.0f;
constexpr float kControllerMaximumRadius = 80.0f;
constexpr float kControllerMinimumStrengthScale = 0.0f;
constexpr float kControllerMaximumStrengthScale = 4.0f;
constexpr float kControllerMinimumCoreRatio = 0.05f;
constexpr float kControllerMaximumCoreRatio = 0.95f;
constexpr char kControllerRadiusMorph[] = "WT_Radius";
constexpr char kControllerStrengthMorph[] = "WT_Strength";
constexpr char kControllerFalloffMorph[] = "WT_Falloff";

constexpr float controller_strength_scale_from_weight(float weight) noexcept {
    return kControllerMinimumStrengthScale +
        (kControllerMaximumStrengthScale - kControllerMinimumStrengthScale) * weight;
}

static_assert(controller_strength_scale_from_weight(0.0f) == 0.0f);
static_assert(controller_strength_scale_from_weight(0.25f) == 1.0f);
static_assert(controller_strength_scale_from_weight(1.0f) == 4.0f);
// MMD 9.31 x64 btCollisionObject::activate(bool). Unlike forceActivationState
// at +0xED310, this also clears m_deactivationTime at body +0xF0.
[[maybe_unused]] constexpr std::uintptr_t kActivateBodyRva = 0x000ed330;
constexpr wchar_t kPanelClassName[] = L"Mmd931PhysicsControlStudioWindow";
constexpr wchar_t kRequestMessageName[] = L"Mmd931.PhysicsControlStudio.Request.v1";
constexpr COLORREF kPanelColor = RGB(23, 25, 29);
constexpr COLORREF kHeaderColor = RGB(31, 34, 40);
constexpr COLORREF kContentColor = RGB(19, 21, 25);
constexpr COLORREF kBorderColor = RGB(61, 68, 78);
constexpr COLORREF kTextColor = RGB(229, 233, 238);
constexpr COLORREF kMutedTextColor = RGB(157, 166, 178);
constexpr COLORREF kAccentColor = RGB(72, 198, 166);
constexpr COLORREF kWarningColor = RGB(236, 177, 80);

enum ControlId : int {
    kWindEnabledId = 2001,
    kStrengthSliderId,
    kGustSliderId,
    kDirectionXId,
    kDirectionYId,
    kDirectionZId,
    kCenterXId = 2009,
    kCenterYId = 2010,
    kCenterZId = 2011,
    kGroupComboId = 2012,
    kFieldTypeComboId = 2013,
    kDirectionPresetComboId = 2014,
    kFrequencySliderId = 2015,
    kWindPageId = 2016,
    kPhysicsPageId = 2017,
    kDampingEnabledId = 2018,
    kLinearDampingSliderId = 2019,
    kAngularDampingSliderId = 2020,
    kGravityEnabledId = 2021,
    kGravityXId = 2022,
    kGravityYId = 2023,
    kGravityZId = 2024,
    kGravityAccelerationSliderId = 2025,
    kSetKeyId = 2026,
    kDeleteKeyId = 2027,
    kTargetPageId = 2028,
    kWindPresetComboId = 2029,
    kNoiseTypeComboId = 2030,
    kTargetListId = 2031,
    kSelectAllId = 2032,
    kClearSelectionId = 2033,
    kInvertSelectionId = 2034,
    kSaveJsonId = 2035,
    kLoadJsonId = 2036,
    kTurbulenceSliderId = 2037,
    kTargetGroupComboId = 2038,
    kTargetGroupNameId = 2039,
    kSaveTargetGroupId = 2040,
    kApplyTargetGroupId = 2041,
    kDeleteTargetGroupId = 2042,
    kTargetLayerComboId = 2043,
    kRadiusSliderId = 2045,
    kCoreRatioSliderId = 2046,
    kFalloffTypeComboId = 2047,
    kSourceModeComboId = 2049,
};

enum class PanelPage : std::uint8_t {
    Wind,
    Physics,
    Target,
};

enum class WindSourceMode : int {
    Global,
    PmxLocal,
};

enum class TargetLayer : std::uint8_t {
    Wind,
    Damping,
    Gravity,
};

enum class HostStatus : std::uint8_t {
    Unchecked,
    Supported,
    UnsupportedName,
    UnsupportedSize,
    HashReadFailure,
    UnsupportedHash,
    MainStateUnavailable,
    MainWindowUnavailable,
};

enum class PendingOperation : int {
    None,
    Install,
    Uninstall,
};

HMODULE g_module = nullptr;
SRWLOCK g_operation_lock = SRWLOCK_INIT;
SRWLOCK g_wind_lock = SRWLOCK_INIT;
std::atomic_bool g_installed{false};
std::atomic<HostStatus> g_host_status{HostStatus::Unchecked};
std::atomic<HWND> g_host{nullptr};
std::atomic<HWND> g_panel{nullptr};
std::atomic<DWORD> g_ui_thread_id{0};
std::atomic<DWORD> g_frame_thread_id{0};
std::atomic_bool g_frame_thread_mismatch{false};
std::atomic<std::uint64_t> g_begin_scene_count{0};
std::atomic<std::uint64_t> g_end_scene_count{0};
std::atomic<std::uint64_t> g_wind_applied_frames{0};
std::atomic<std::uint64_t> g_wind_applied_bodies{0};
std::atomic<std::int64_t> g_last_wind_qpc{0};
std::atomic<int> g_wind_backend_status{0};
std::atomic<int> g_controller_status{0};
std::atomic<std::uint32_t> g_controller_count{0};
std::atomic<WindSourceMode> g_wind_source_mode{WindSourceMode::Global};
std::atomic_bool g_wind_master_enabled{false};
std::atomic<float> g_wind_master_strength{30.0f};
std::atomic<std::uint32_t> g_current_frame{0};
HMENU g_host_menu = nullptr;
HMENU g_extension_menu = nullptr;
bool g_created_host_menu = false;
UINT g_request_message = 0;
std::atomic<PendingOperation> g_pending_operation{PendingOperation::None};
std::atomic<HWND> g_pending_window{nullptr};
std::atomic_bool g_pending_result{false};
std::wstring g_snapshot_text;
HFONT g_panel_font = nullptr;
HBRUSH g_panel_brush = nullptr;
HBRUSH g_content_brush = nullptr;
bool g_close_button_hot = false;
bool g_syncing_controls = false;
PanelPage g_panel_page = PanelPage::Wind;
TargetLayer g_target_layer = TargetLayer::Wind;
int g_wind_preset_selection = 0;
std::uint32_t g_last_synced_frame = UINT32_MAX;
int g_last_controller_ui_status = -1;
int g_last_controller_ui_count = -1;
std::uint32_t g_selected_key_frame = UINT32_MAX;
physics_control_studio::WindSettings g_wind_settings{};
physics_control_studio::PhysicsSettings g_physics_settings{};
physics_control_studio::PhysicsTrack g_physics_track{};
std::vector<physics_control_studio::TargetGroup> g_target_groups;
std::vector<std::uint32_t> g_target_body_indices;
std::atomic<std::uintptr_t> g_physics_target_model{0};
std::uintptr_t g_wind_filter_model = 0;
std::array<std::uintptr_t, kMaximumWindControllers> g_controller_models{};
std::array<std::array<std::uint32_t, 3>, kMaximumWindControllers>
    g_controller_morph_indices{};
std::size_t g_controller_cache_count = 0;
std::array<std::uintptr_t, 255> g_controller_model_table{};
bool g_controller_model_table_valid = false;
std::uint32_t g_controller_scan_cooldown = 0;
std::vector<std::uint32_t> g_last_damping_body_indices;
std::uintptr_t g_last_damping_model = 0;
std::unordered_map<std::uintptr_t, physics_control_studio::Vec3>
    g_wind_filtered_acceleration;
double g_wind_time_seconds = 0.0;
std::uintptr_t g_target_cache_model = 0;
std::uint32_t g_target_cache_count = UINT32_MAX;
std::wstring g_track_path;
std::wstring g_track_status = L"轨道尚未保存";
HWND g_wind_page = nullptr;
HWND g_physics_page = nullptr;
HWND g_target_page = nullptr;
HWND g_wind_enabled = nullptr;
HWND g_source_mode_combo = nullptr;
HWND g_strength_slider = nullptr;
HWND g_gust_slider = nullptr;
HWND g_turbulence_slider = nullptr;
HWND g_frequency_slider = nullptr;
HWND g_field_type_combo = nullptr;
HWND g_wind_preset_combo = nullptr;
HWND g_noise_type_combo = nullptr;
HWND g_direction_preset_combo = nullptr;
HWND g_direction_x = nullptr;
HWND g_direction_y = nullptr;
HWND g_direction_z = nullptr;
HWND g_center_x = nullptr;
HWND g_center_y = nullptr;
HWND g_center_z = nullptr;
HWND g_radius_slider = nullptr;
HWND g_core_ratio_slider = nullptr;
HWND g_falloff_type_combo = nullptr;
HWND g_group_combo = nullptr;
HWND g_target_list = nullptr;
HWND g_target_layer_combo = nullptr;
HWND g_select_all = nullptr;
HWND g_clear_selection = nullptr;
HWND g_invert_selection = nullptr;
HWND g_target_group_combo = nullptr;
HWND g_target_group_name = nullptr;
HWND g_save_target_group = nullptr;
HWND g_apply_target_group = nullptr;
HWND g_delete_target_group = nullptr;
HWND g_damping_enabled = nullptr;
HWND g_linear_damping_slider = nullptr;
HWND g_angular_damping_slider = nullptr;
HWND g_gravity_enabled = nullptr;
HWND g_gravity_x = nullptr;
HWND g_gravity_y = nullptr;
HWND g_gravity_z = nullptr;
HWND g_gravity_acceleration_slider = nullptr;
HWND g_set_key = nullptr;
HWND g_delete_key = nullptr;
HWND g_save_json = nullptr;
HWND g_load_json = nullptr;
int g_target_selection_anchor = -1;

void reset_controller_cache() noexcept {
    g_controller_models.fill(0);
    for (auto& indices : g_controller_morph_indices) indices.fill(UINT32_MAX);
    g_controller_cache_count = 0;
    g_controller_model_table.fill(0);
    g_controller_model_table_valid = false;
    g_controller_scan_cooldown = 0;
    g_controller_count.store(0);
}

void reset_frame_bridge() noexcept {
    g_ui_thread_id.store(0);
    g_frame_thread_id.store(0);
    g_frame_thread_mismatch.store(false);
    g_begin_scene_count.store(0);
    g_end_scene_count.store(0);
    g_wind_applied_frames.store(0);
    g_wind_applied_bodies.store(0);
    g_last_wind_qpc.store(0);
    g_wind_backend_status.store(0);
    g_controller_status.store(0);
    g_wind_source_mode.store(WindSourceMode::Global);
    g_wind_master_enabled.store(false);
    g_physics_target_model.store(0);
    g_wind_filter_model = 0;
    reset_controller_cache();
    g_last_controller_ui_status = -1;
    g_last_controller_ui_count = -1;
    g_last_damping_body_indices.clear();
    g_last_damping_model = 0;
    g_wind_filtered_acceleration.clear();
    g_wind_time_seconds = 0.0;
}

const wchar_t* frame_bridge_status_wide() noexcept {
    if (g_frame_thread_mismatch.load()) return L"thread_mismatch";
    return g_frame_thread_id.load() == 0 ? L"inactive" : L"online";
}

const char* frame_bridge_status() noexcept {
    if (g_frame_thread_mismatch.load()) return "thread_mismatch";
    return g_frame_thread_id.load() == 0 ? "inactive" : "online";
}

class ExclusiveLock {
public:
    explicit ExclusiveLock(SRWLOCK& lock) noexcept : lock_(lock) {
        AcquireSRWLockExclusive(&lock_);
    }
    ~ExclusiveLock() { ReleaseSRWLockExclusive(&lock_); }
    ExclusiveLock(const ExclusiveLock&) = delete;
    ExclusiveLock& operator=(const ExclusiveLock&) = delete;

private:
    SRWLOCK& lock_;
};

class SharedLock {
public:
    explicit SharedLock(SRWLOCK& lock) noexcept : lock_(lock) {
        AcquireSRWLockShared(&lock_);
    }
    ~SharedLock() { ReleaseSRWLockShared(&lock_); }
    SharedLock(const SharedLock&) = delete;
    SharedLock& operator=(const SharedLock&) = delete;

private:
    SRWLOCK& lock_;
};

class UniqueHandle {
public:
    explicit UniqueHandle(HANDLE value = nullptr) noexcept : value_(value) {}
    ~UniqueHandle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_);
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    HANDLE get() const noexcept { return value_; }
    bool valid() const noexcept {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE value_ = nullptr;
};

const char* status_name(HostStatus status) noexcept {
    switch (status) {
    case HostStatus::Unchecked: return "unchecked";
    case HostStatus::Supported: return "supported";
    case HostStatus::UnsupportedName: return "unsupported_name";
    case HostStatus::UnsupportedSize: return "unsupported_size";
    case HostStatus::HashReadFailure: return "hash_read_failure";
    case HostStatus::UnsupportedHash: return "unsupported_hash";
    case HostStatus::MainStateUnavailable: return "main_state_unavailable";
    case HostStatus::MainWindowUnavailable: return "main_window_unavailable";
    }
    return "unknown";
}

[[maybe_unused]] const wchar_t* status_name_wide(HostStatus status) noexcept {
    switch (status) {
    case HostStatus::Unchecked: return L"unchecked";
    case HostStatus::Supported: return L"supported";
    case HostStatus::UnsupportedName: return L"unsupported_name";
    case HostStatus::UnsupportedSize: return L"unsupported_size";
    case HostStatus::HashReadFailure: return L"hash_read_failure";
    case HostStatus::UnsupportedHash: return L"unsupported_hash";
    case HostStatus::MainStateUnavailable: return L"main_state_unavailable";
    case HostStatus::MainWindowUnavailable: return L"main_window_unavailable";
    }
    return L"unknown";
}

[[maybe_unused]] bool sha256_file(const wchar_t* path, std::string& hex_digest) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD hash_size = 0;
    DWORD copied = 0;
    std::vector<std::uint8_t> object;
    std::vector<std::uint8_t> digest;
    UniqueHandle file(CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr));
    if (!file.valid()) return false;
    if (BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        return false;
    }
    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size),
            sizeof(object_size),
            &copied,
            0) < 0 ||
        BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hash_size),
            sizeof(hash_size),
            &copied,
            0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }
    object.resize(object_size);
    digest.resize(hash_size);
    if (BCryptCreateHash(
            algorithm,
            &hash,
            object.data(),
            static_cast<ULONG>(object.size()),
            nullptr,
            0,
            0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    std::array<std::uint8_t, 64 * 1024> buffer{};
    bool success = true;
    while (success) {
        DWORD bytes_read = 0;
        if (ReadFile(
                file.get(),
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytes_read,
                nullptr) == FALSE) {
            success = false;
            break;
        }
        if (bytes_read == 0) break;
        if (BCryptHashData(hash, buffer.data(), bytes_read, 0) < 0) success = false;
    }
    if (success && BCryptFinishHash(
                       hash,
                       digest.data(),
                       static_cast<ULONG>(digest.size()),
                       0) < 0) {
        success = false;
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!success) return false;

    static constexpr char digits[] = "0123456789ABCDEF";
    hex_digest.clear();
    hex_digest.reserve(digest.size() * 2);
    for (const auto value : digest) {
        hex_digest.push_back(digits[value >> 4]);
        hex_digest.push_back(digits[value & 0x0f]);
    }
    return true;
}

HostStatus validate_host() {
#ifdef PCS_SURROGATE_HOST
    return HostStatus::Supported;
#else
    std::array<wchar_t, 32'768> path{};
    const DWORD length = GetModuleFileNameW(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return HostStatus::UnsupportedName;
    const wchar_t* filename = std::wcsrchr(path.data(), L'\\');
    filename = filename == nullptr ? path.data() : filename + 1;
    if (_wcsicmp(filename, pcs_host::kSupportedExecutableName) != 0)
        return HostStatus::UnsupportedName;

    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (GetFileAttributesExW(path.data(), GetFileExInfoStandard, &attributes) == FALSE)
        return HostStatus::UnsupportedSize;
    const std::uint64_t size =
        (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) |
        attributes.nFileSizeLow;
    if (size != pcs_host::kSupportedExecutableSize) return HostStatus::UnsupportedSize;

    std::string digest;
    if (!sha256_file(path.data(), digest)) return HostStatus::HashReadFailure;
    return digest == pcs_host::kSupportedExecutableSha256
        ? HostStatus::Supported
        : HostStatus::UnsupportedHash;
#endif
}

[[maybe_unused]] std::wstring cp932_name(const char* text, std::size_t capacity) {
    std::size_t length = 0;
    while (length < capacity && text[length] != '\0') ++length;
    if (length == 0) return L"(unnamed)";
    const int required = MultiByteToWideChar(
        932, 0, text, static_cast<int>(length), nullptr, 0);
    if (required <= 0) return L"(encoding error)";
    std::wstring output(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(
        932, 0, text, static_cast<int>(length), output.data(), required);
    return output;
}

std::wstring utf8_to_wide(std::string_view text) {
    if (text.empty()) return {};
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (required <= 0) return {};
    std::wstring output(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        output.data(),
        required);
    return output;
}

std::string wide_to_utf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) return {};
    std::string output(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        output.data(),
        required,
        nullptr,
        nullptr);
    return output;
}

[[maybe_unused]] const wchar_t* body_mode_name(std::uint8_t mode) noexcept {
    switch (mode) {
    case 0: return L"bone";
    case 1: return L"dynamic";
    case 2: return L"dynamic+bone";
    default: return L"unknown";
    }
}

WindSourceMode source_mode_from_settings(
    const physics_control_studio::WindSettings& settings) noexcept {
    return settings.controller_enabled
        ? WindSourceMode::PmxLocal
        : WindSourceMode::Global;
}

void apply_wind_source_mode(
    physics_control_studio::WindSettings& settings,
    WindSourceMode mode) noexcept {
    const bool pmx_local = mode == WindSourceMode::PmxLocal;
    settings.local_enabled = pmx_local;
    settings.controller_enabled = pmx_local;
}

void reset_wind_source_runtime(WindSourceMode mode) noexcept {
    g_last_wind_qpc.store(0);
    g_wind_applied_bodies.store(0);
    g_wind_backend_status.store(0);
    g_wind_filter_model = 0;
    g_wind_filtered_acceleration.clear();
    g_wind_time_seconds = 0.0;
    reset_controller_cache();
    g_controller_status.store(mode == WindSourceMode::PmxLocal ? 1 : 0);
    g_last_controller_ui_status = -1;
    g_last_controller_ui_count = -1;
}

physics_control_studio::ControlSnapshot control_snapshot() noexcept {
    SharedLock lock(g_wind_lock);
    physics_control_studio::ControlSnapshot snapshot{
        g_wind_settings, g_physics_settings};
    apply_wind_source_mode(snapshot.wind, g_wind_source_mode.load());
    physics_control_studio::normalize_wind_settings(snapshot.wind);
    return snapshot;
}

physics_control_studio::ControlSnapshot control_snapshot_for_frame(
    std::uint32_t frame) noexcept {
    SharedLock lock(g_wind_lock);
    auto snapshot = g_physics_track.evaluate(frame, {g_wind_settings, g_physics_settings});
    apply_wind_source_mode(snapshot.wind, g_wind_source_mode.load());
    physics_control_studio::normalize_wind_settings(snapshot.wind);
    // An enabled wind field with an empty custom target is not actionable. Older
    // UI builds could write that state when a wind slider changed while the
    // target list had no selection, so keep active wind usable by falling back
    // to the documented default: all dynamic bodies.
    const auto& target = snapshot.physics.wind_target;
    if (snapshot.wind.enabled &&
        target.kind == physics_control_studio::TargetKind::CustomSet &&
        target.collision_group_mask == 0 && target.rigid_body_indices.empty()) {
        snapshot.physics.wind_target = physics_control_studio::TargetSelection{};
    }
    return snapshot;
}

std::size_t control_key_count() noexcept {
    SharedLock lock(g_wind_lock);
    return g_physics_track.size();
}

std::vector<physics_control_studio::ControlKeyframe> control_keys_snapshot() {
    SharedLock lock(g_wind_lock);
    return g_physics_track.keys();
}

std::wstring default_track_path() {
    std::array<wchar_t, 32'768> path{};
    const DWORD length = GetModuleFileNameW(
        g_module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return L"PhysicsControlStudio.json";
    std::wstring result(path.data(), length);
    const std::size_t separator = result.find_last_of(L"\\/");
    if (separator != std::wstring::npos) result.resize(separator + 1);
    result += L"PhysicsControlStudio.json";
    return result;
}

bool write_utf8_file(const std::wstring& path, const std::string& contents) {
    const HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool success = contents.size() <= std::numeric_limits<DWORD>::max() &&
        WriteFile(
            file,
            contents.data(),
            static_cast<DWORD>(contents.size()),
            &written,
            nullptr) != FALSE &&
        written == contents.size();
    CloseHandle(file);
    return success;
}

bool read_utf8_file(const std::wstring& path, std::string& contents) {
    const HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0 ||
        size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }
    contents.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    const bool success = contents.size() <= std::numeric_limits<DWORD>::max() &&
        ReadFile(
            file,
            contents.data(),
            static_cast<DWORD>(contents.size()),
            &read,
            nullptr) != FALSE &&
        read == contents.size();
    CloseHandle(file);
    return success;
}

bool save_track_file() {
    physics_control_studio::ControlSnapshot current{};
    physics_control_studio::PhysicsTrack track;
    std::vector<physics_control_studio::TargetGroup> target_groups;
    {
        SharedLock lock(g_wind_lock);
        current = {g_wind_settings, g_physics_settings};
        track = g_physics_track;
        target_groups = g_target_groups;
    }
    apply_wind_source_mode(current.wind, g_wind_source_mode.load());
    if (g_track_path.empty()) g_track_path = default_track_path();
    const bool success = write_utf8_file(
        g_track_path,
        physics_control_studio::serialize_track_json(current, track, target_groups));
    g_track_status = success ? L"JSON 已保存" : L"JSON 保存失败";
    return success;
}

bool load_track_file() {
    if (g_track_path.empty()) g_track_path = default_track_path();
    std::string json;
    if (!read_utf8_file(g_track_path, json)) {
        g_track_status = L"未找到 JSON，使用当前参数";
        return false;
    }
    physics_control_studio::ControlSnapshot current{};
    physics_control_studio::PhysicsTrack track;
    std::vector<physics_control_studio::TargetGroup> target_groups;
    std::string error;
    if (!physics_control_studio::deserialize_track_json(
            json, current, track, target_groups, error)) {
        g_track_status = L"JSON 格式无效";
        return false;
    }
    const WindSourceMode source_mode = source_mode_from_settings(current.wind);
    apply_wind_source_mode(current.wind, source_mode);
    {
        ExclusiveLock lock(g_wind_lock);
        g_wind_settings = current.wind;
        g_physics_settings = current.physics;
        g_physics_track = std::move(track);
        g_target_groups = std::move(target_groups);
    }
    g_wind_source_mode.store(source_mode);
    reset_wind_source_runtime(source_mode);
    g_wind_master_enabled.store(current.wind.enabled);
    g_wind_master_strength.store(current.wind.strength);
    g_selected_key_frame = UINT32_MAX;
    g_last_synced_frame = UINT32_MAX;
    g_track_status = L"JSON 已读取";
    return true;
}

bool current_frame_has_key() noexcept {
    SharedLock lock(g_wind_lock);
    return g_physics_track.has_key(g_current_frame.load());
}

const wchar_t* wind_backend_status_wide() noexcept {
    switch (g_wind_backend_status.load()) {
    case 0: return L"off";
    case 1: return L"ready";
    case 2: return L"active";
    case 3: return L"waiting for PMX";
    case -1: return L"invalid settings";
    case -2: return L"read rejected";
    case -3: return L"write rejected";
    case -4: return L"unsupported host";
    default: return L"unknown";
    }
}

const char* wind_backend_status() noexcept {
    switch (g_wind_backend_status.load()) {
    case 0: return "off";
    case 1: return "ready";
    case 2: return "active";
    case 3: return "waiting_for_pmx";
    case -1: return "invalid_settings";
    case -2: return "read_rejected";
    case -3: return "write_rejected";
    case -4: return "unsupported_host";
    default: return "unknown";
    }
}

[[maybe_unused]] bool writable_range(std::uintptr_t address, std::size_t size) noexcept {
    if (address == 0 || size == 0 || address > address + size) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(
            reinterpret_cast<const void*>(address),
            &information,
            sizeof(information)) != sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xff;
    const bool writable = protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
    const std::uintptr_t region_end = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress) + information.RegionSize;
    return writable && address + size <= region_end;
}

[[maybe_unused]] bool write_process_exact(
    std::uintptr_t address,
    const void* data,
    std::size_t size) noexcept {
    SIZE_T written = 0;
    return WriteProcessMemory(
               GetCurrentProcess(),
               reinterpret_cast<void*>(address),
               data,
               size,
               &written) != FALSE &&
        written == size;
}

[[maybe_unused]] physics_control_studio::Vec3 smooth_wind_acceleration(
    std::uintptr_t body,
    const physics_control_studio::Vec3& raw,
    float delta_seconds) {
    const bool valid = std::isfinite(raw.x) && std::isfinite(raw.y) &&
        std::isfinite(raw.z) && std::isfinite(delta_seconds) &&
        delta_seconds > 0.0f;
    if (!valid) {
        g_wind_filtered_acceleration.erase(body);
        return {};
    }

    auto [state, unused_inserted] = g_wind_filtered_acceleration.try_emplace(
        body, physics_control_studio::Vec3{});
    (void)unused_inserted;
    const float alpha = 1.0f - std::exp(
        -delta_seconds / kWindFilterTimeConstant);
    auto& filtered = state->second;
    filtered.x += (raw.x - filtered.x) * alpha;
    filtered.y += (raw.y - filtered.y) * alpha;
    filtered.z += (raw.z - filtered.z) * alpha;

    const float filtered_squared = filtered.x * filtered.x +
        filtered.y * filtered.y + filtered.z * filtered.z;
    const float raw_squared = raw.x * raw.x + raw.y * raw.y + raw.z * raw.z;
    if (filtered_squared <= 1.0e-8f && raw_squared <= 1.0e-8f) {
        g_wind_filtered_acceleration.erase(state);
        return {};
    }
    return filtered;
}

struct WindControllerSample {
    physics_control_studio::Vec3 center{};
    physics_control_studio::Vec3 direction{1.0f, 0.0f, 0.0f};
    float radius = 20.0f;
    float strength_scale = 1.0f;
    float core_ratio = 0.35f;
};

bool fixed_name_equals(
    const char* value,
    std::size_t capacity,
    std::string_view expected) noexcept {
    if (expected.size() >= capacity) return false;
    std::size_t length = 0;
    while (length < capacity && value[length] != '\0') ++length;
    return length == expected.size() &&
        std::memcmp(value, expected.data(), expected.size()) == 0;
}

bool controller_morph_matches(
    const mmd931::model::morph::RuntimeMorphRecord& morph,
    std::string_view expected) noexcept {
    return fixed_name_equals(morph.name, std::size(morph.name), expected) ||
        fixed_name_equals(
            morph.english_name,
            std::size(morph.english_name),
            expected);
}

float controller_weight(
    const mmd931::model::morph::RuntimeMorphRecord& morph) noexcept {
    return std::isfinite(morph.weight)
        ? std::clamp(morph.weight, 0.0f, 1.0f)
        : 0.0f;
}

bool checked_record_address(
    std::uintptr_t base,
    std::uint32_t index,
    std::size_t stride,
    std::uintptr_t& address) noexcept {
    const auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
    if (stride != 0 && index > maximum / stride) return false;
    const std::uintptr_t offset = static_cast<std::uintptr_t>(index) * stride;
    if (base > maximum - offset) return false;
    address = base + offset;
    return true;
}

bool read_controller_from_model(
    mmd931::runtime_access::ProcessReader& reader,
    std::uintptr_t model,
    std::array<std::uint32_t, 3>& morph_indices,
    WindControllerSample& sample) {
    if (model == 0) return false;
    std::uintptr_t morphs = 0;
    std::uintptr_t bones = 0;
    std::uint32_t morph_count = 0;
    std::uint32_t bone_count = 0;
    if (!reader.read(
            model + mmd931::model::state::kMorphs,
            &morphs,
            sizeof(morphs)) ||
        !reader.read(
            model + mmd931::model::state::kMorphCount,
            &morph_count,
            sizeof(morph_count)) ||
        !reader.read(
            model + mmd931::model::state::kBones,
            &bones,
            sizeof(bones)) ||
        !reader.read(
            model + mmd931::model::state::kBoneCount,
            &bone_count,
            sizeof(bone_count)) ||
        morphs == 0 || bones == 0 || morph_count == 0 ||
        morph_count > kMaximumControllerMorphs || bone_count == 0 ||
        bone_count > kMaximumControllerBones) {
        return false;
    }

    constexpr std::array<std::string_view, 3> names{{
        kControllerRadiusMorph,
        kControllerStrengthMorph,
        kControllerFalloffMorph}};
    std::array<mmd931::model::morph::RuntimeMorphRecord, 3> records{};
    bool cached = std::all_of(
        morph_indices.begin(),
        morph_indices.end(),
        [&](std::uint32_t index) { return index < morph_count; });
    if (cached) {
        for (std::size_t slot = 0; slot < morph_indices.size(); ++slot) {
            std::uintptr_t address = 0;
            if (!checked_record_address(
                    morphs,
                    morph_indices[slot],
                    sizeof(records[slot]),
                    address) ||
                !reader.read(address, &records[slot], sizeof(records[slot])) ||
                !controller_morph_matches(records[slot], names[slot])) {
                cached = false;
                break;
            }
        }
    }
    if (!cached) {
        morph_indices = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
        for (std::uint32_t index = 0; index < morph_count; ++index) {
            std::uintptr_t address = 0;
            mmd931::model::morph::RuntimeMorphRecord morph{};
            if (!checked_record_address(morphs, index, sizeof(morph), address) ||
                !reader.read(address, &morph, sizeof(morph))) {
                return false;
            }
            for (std::size_t slot = 0; slot < names.size(); ++slot) {
                if (morph_indices[slot] == UINT32_MAX &&
                    controller_morph_matches(morph, names[slot])) {
                    morph_indices[slot] = index;
                    records[slot] = morph;
                }
            }
            if (std::none_of(
                    morph_indices.begin(),
                    morph_indices.end(),
                    [](std::uint32_t value) { return value == UINT32_MAX; })) {
                break;
            }
        }
        if (std::any_of(
                morph_indices.begin(),
                morph_indices.end(),
                [](std::uint32_t value) { return value == UINT32_MAX; })) {
            return false;
        }
    }

    std::uintptr_t matrix_address = 0;
    if (!checked_record_address(
            bones,
            0,
            mmd931::model::bone::kStride,
            matrix_address)) {
        return false;
    }
    const auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
    if (matrix_address > maximum - mmd931::model::bone::kWorldMatrix) return false;
    matrix_address += mmd931::model::bone::kWorldMatrix;
    std::array<float, 16> world{};
    if (!reader.read(matrix_address, world.data(), sizeof(world))) return false;

    sample.center = {world[12], world[13], world[14]};
    sample.direction = physics_control_studio::normalize_or_zero(
        {world[0], world[1], world[2]});
    if (!std::isfinite(sample.center.x) || !std::isfinite(sample.center.y) ||
        !std::isfinite(sample.center.z) ||
        sample.direction.x * sample.direction.x +
                sample.direction.y * sample.direction.y +
                sample.direction.z * sample.direction.z <=
            1.0e-8f) {
        return false;
    }

    const float radius_weight = controller_weight(records[0]);
    const float strength_weight = controller_weight(records[1]);
    const float falloff_weight = controller_weight(records[2]);
    sample.radius = kControllerMinimumRadius +
        (kControllerMaximumRadius - kControllerMinimumRadius) * radius_weight;
    sample.strength_scale = controller_strength_scale_from_weight(strength_weight);
    sample.core_ratio = kControllerMinimumCoreRatio +
        (kControllerMaximumCoreRatio - kControllerMinimumCoreRatio) *
            falloff_weight;
    return true;
}

[[maybe_unused]] std::size_t sample_wind_controllers(
    mmd931::runtime_access::ProcessReader& reader,
    const std::array<std::uintptr_t, 255>& models,
    std::array<WindControllerSample, kMaximumWindControllers>& samples) {
    samples = {};
    const bool table_changed = !g_controller_model_table_valid ||
        g_controller_model_table != models;
    if (!table_changed && g_controller_cache_count > 0) {
        bool cache_valid = true;
        for (std::size_t index = 0; index < g_controller_cache_count; ++index) {
            if (g_controller_models[index] == 0 || !read_controller_from_model(
                    reader,
                    g_controller_models[index],
                    g_controller_morph_indices[index],
                    samples[index])) {
                cache_valid = false;
                break;
            }
        }
        if (cache_valid && g_controller_scan_cooldown > 0) {
            --g_controller_scan_cooldown;
            g_controller_count.store(
                static_cast<std::uint32_t>(g_controller_cache_count));
            g_controller_status.store(2);
            return g_controller_cache_count;
        }
    }

    if (!table_changed && g_controller_cache_count == 0 &&
        g_controller_scan_cooldown > 0) {
        --g_controller_scan_cooldown;
        g_controller_count.store(0);
        g_controller_status.store(1);
        return 0;
    }

    g_controller_models.fill(0);
    for (auto& indices : g_controller_morph_indices) indices.fill(UINT32_MAX);
    g_controller_cache_count = 0;
    g_controller_model_table = models;
    g_controller_model_table_valid = true;

    for (const std::uintptr_t model : models) {
        if (model == 0) continue;
        if (g_controller_cache_count >= kMaximumWindControllers) break;
        std::array<std::uint32_t, 3> indices{
            UINT32_MAX, UINT32_MAX, UINT32_MAX};
        WindControllerSample sample{};
        if (read_controller_from_model(reader, model, indices, sample)) {
            const std::size_t slot = g_controller_cache_count++;
            g_controller_models[slot] = model;
            g_controller_morph_indices[slot] = indices;
            samples[slot] = sample;
        }
    }

    g_controller_count.store(
        static_cast<std::uint32_t>(g_controller_cache_count));
    if (g_controller_cache_count > 0) {
        g_controller_scan_cooldown = kControllerConnectedRescanFrames;
        g_controller_status.store(2);
        return g_controller_cache_count;
    }
    g_controller_scan_cooldown = kControllerMissingRescanFrames;
    g_controller_status.store(1);
    return 0;
}

struct PhysicsTargetModel {
    std::uintptr_t model = 0;
    std::uintptr_t rigid_bodies = 0;
    std::uint32_t rigid_body_count = 0;
};

[[maybe_unused]] bool read_physics_target_model(
    mmd931::runtime_access::ProcessReader& reader,
    std::uintptr_t model,
    PhysicsTargetModel& target) {
    target = {};
    if (model == 0) return false;
    std::uintptr_t rigid_bodies = 0;
    std::uint32_t rigid_body_count = 0;
    if (!reader.read(
            model + mmd931::model::state::kRigidBodies,
            &rigid_bodies,
            sizeof(rigid_bodies)) ||
        !reader.read(
            model + mmd931::model::state::kRigidBodyCount,
            &rigid_body_count,
            sizeof(rigid_body_count)) ||
        rigid_bodies == 0 || rigid_body_count == 0 ||
        rigid_body_count > kMaximumWindBodies) {
        return false;
    }
    target = {model, rigid_bodies, rigid_body_count};
    return true;
}

[[maybe_unused]] PhysicsTargetModel resolve_physics_target_model(
    mmd931::runtime_access::ProcessReader& reader,
    std::uint8_t selected_model,
    const std::array<std::uintptr_t, 255>& models) {
    PhysicsTargetModel resolved{};
    const std::uintptr_t model =
        physics_control_studio::select_physics_target_model(
            static_cast<std::size_t>(selected_model),
            g_physics_target_model.load(),
            models,
            [&](std::uintptr_t candidate) {
                PhysicsTargetModel target{};
                if (!read_physics_target_model(reader, candidate, target)) {
                    return false;
                }
                resolved = target;
                return true;
            });
    if (model == 0) resolved = {};
    g_physics_target_model.store(model);
    return resolved;
}

struct PendingBodyWrite {
    void* body = nullptr;
    std::uintptr_t velocity_address = 0;
    std::array<float, 4> original_velocity{};
    std::array<float, 4> updated_velocity{};
    std::uintptr_t angular_velocity_address = 0;
    std::array<float, 4> original_angular_velocity{};
    std::array<float, 4> updated_angular_velocity{};
    std::uintptr_t interpolation_velocity_address = 0;
    std::array<float, 4> original_interpolation_velocity{};
    std::array<float, 4> updated_interpolation_velocity{};
    std::uintptr_t interpolation_angular_velocity_address = 0;
    std::array<float, 4> original_interpolation_angular_velocity{};
    std::array<float, 4> updated_interpolation_angular_velocity{};
    std::uintptr_t force_address = 0;
    std::array<float, 4> original_force{};
    std::array<float, 4> updated_force{};
    std::uintptr_t damping_address = 0;
    std::array<float, 2> original_damping{};
    std::array<float, 2> updated_damping{};
    bool write_velocity = false;
    bool write_angular_velocity = false;
    bool write_interpolation_velocity = false;
    bool write_interpolation_angular_velocity = false;
    bool write_force = false;
    bool write_damping = false;
};

bool has_activation_write(const PendingBodyWrite& write) noexcept {
    return write.write_velocity || write.write_angular_velocity ||
        write.write_interpolation_velocity ||
        write.write_interpolation_angular_velocity || write.write_force;
}

void restore_body_write(const PendingBodyWrite& write) noexcept {
    if (write.write_velocity) write_process_exact(
        write.velocity_address,
        write.original_velocity.data(),
        sizeof(write.original_velocity));
    if (write.write_angular_velocity) write_process_exact(
        write.angular_velocity_address,
        write.original_angular_velocity.data(),
        sizeof(write.original_angular_velocity));
    if (write.write_interpolation_velocity) write_process_exact(
        write.interpolation_velocity_address,
        write.original_interpolation_velocity.data(),
        sizeof(write.original_interpolation_velocity));
    if (write.write_interpolation_angular_velocity) write_process_exact(
        write.interpolation_angular_velocity_address,
        write.original_interpolation_angular_velocity.data(),
        sizeof(write.original_interpolation_angular_velocity));
    if (write.write_force) write_process_exact(
        write.force_address,
        write.original_force.data(),
        sizeof(write.original_force));
    if (write.write_damping) write_process_exact(
        write.damping_address,
        write.original_damping.data(),
        sizeof(write.original_damping));
}

bool commit_body_writes(
    std::vector<PendingBodyWrite>& writes,
    std::uintptr_t base) noexcept {
    std::size_t completed = 0;
    for (; completed < writes.size(); ++completed) {
        auto& current = writes[completed];
        const bool velocity_ok = !current.write_velocity || write_process_exact(
            current.velocity_address,
            current.updated_velocity.data(),
            sizeof(current.updated_velocity));
        const bool angular_ok = velocity_ok &&
            (!current.write_angular_velocity || write_process_exact(
                current.angular_velocity_address,
                current.updated_angular_velocity.data(),
                sizeof(current.updated_angular_velocity)));
        const bool interpolation_ok = angular_ok &&
            (!current.write_interpolation_velocity || write_process_exact(
                current.interpolation_velocity_address,
                current.updated_interpolation_velocity.data(),
                sizeof(current.updated_interpolation_velocity)));
        const bool interpolation_angular_ok = interpolation_ok &&
            (!current.write_interpolation_angular_velocity || write_process_exact(
                current.interpolation_angular_velocity_address,
                current.updated_interpolation_angular_velocity.data(),
                sizeof(current.updated_interpolation_angular_velocity)));
        const bool force_ok = interpolation_angular_ok &&
            (!current.write_force || write_process_exact(
                current.force_address,
                current.updated_force.data(),
                sizeof(current.updated_force)));
        const bool damping_ok = force_ok &&
            (!current.write_damping || write_process_exact(
                current.damping_address,
                current.updated_damping.data(),
                sizeof(current.updated_damping)));
        if (!damping_ok) {
            restore_body_write(current);
            for (std::size_t rollback = 0; rollback < completed; ++rollback) {
                restore_body_write(writes[rollback]);
            }
            return false;
        }
    }

    using ActivationFunction = void (*)(void*, bool);
    const auto activate = reinterpret_cast<ActivationFunction>(
        base + kActivateBodyRva);
    for (const auto& write : writes) {
        if (has_activation_write(write)) activate(write.body, false);
    }
    return true;
}

std::array<float, 2> native_damping_values(
    const mmd931::model::physics::RigidBodyRecord& record) noexcept {
    const auto clamp_damping = [](float value) noexcept {
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    };
    return {
        clamp_damping(record.linear_damping),
        clamp_damping(record.angular_damping)};
}

bool prepare_damping_write(
    mmd931::runtime_access::ProcessReader& reader,
    std::uintptr_t body,
    const std::array<float, 2>& damping,
    PendingBodyWrite& pending) noexcept {
    pending.body = reinterpret_cast<void*>(body);
    pending.damping_address =
        body + physics_control_studio::bullet_runtime_layout::kLinearDamping;
    if (!reader.read(
            pending.damping_address,
            pending.original_damping.data(),
            sizeof(pending.original_damping))) {
        return false;
    }
    pending.updated_damping = damping;
    pending.write_damping = true;
    return writable_range(
        pending.damping_address,
        sizeof(pending.updated_damping));
}

[[maybe_unused]] bool restore_native_damping(
    mmd931::runtime_access::ProcessReader& reader,
    std::uintptr_t base,
    const std::array<std::uintptr_t, 255>& models,
    std::uintptr_t model,
    const std::vector<std::uint32_t>& body_indices,
    std::size_t& restored_bodies) {
    restored_bodies = 0;
    if (model == 0 || body_indices.empty() ||
        std::find(models.begin(), models.end(), model) == models.end()) {
        return true;
    }

    std::uintptr_t rigid_bodies = 0;
    std::uint32_t rigid_body_count = 0;
    if (!reader.read(
            model + mmd931::model::state::kRigidBodies,
            &rigid_bodies,
            sizeof(rigid_bodies)) ||
        !reader.read(
            model + mmd931::model::state::kRigidBodyCount,
            &rigid_body_count,
            sizeof(rigid_body_count)) ||
        rigid_bodies == 0 || rigid_body_count > kMaximumWindBodies) {
        return false;
    }

    std::vector<PendingBodyWrite> writes;
    writes.reserve(body_indices.size());
    for (const std::uint32_t index : body_indices) {
        if (index >= rigid_body_count) continue;
        mmd931::model::physics::RigidBodyRecord record{};
        if (!reader.read(
                rigid_bodies + static_cast<std::uintptr_t>(index) * sizeof(record),
                &record,
                sizeof(record))) {
            return false;
        }
        const std::uintptr_t body = static_cast<std::uintptr_t>(record.runtime_object);
        if (body == 0) continue;
        PendingBodyWrite pending{};
        if (!prepare_damping_write(
                reader,
                body,
                native_damping_values(record),
                pending)) {
            return false;
        }
        writes.push_back(pending);
    }
    if (!commit_body_writes(writes, base)) return false;
    restored_bodies = writes.size();
    return true;
}

bool restore_tracked_damping_overrides() noexcept {
#ifdef PCS_SURROGATE_HOST
    g_last_damping_body_indices.clear();
    g_last_damping_model = 0;
    return true;
#else
    if (g_last_damping_model == 0 || g_last_damping_body_indices.empty()) {
        return true;
    }

    mmd931::runtime_access::ProcessReader reader(GetCurrentProcess());
    const std::uintptr_t base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::uintptr_t state = 0;
    std::array<std::uintptr_t, 255> models{};
    if (!reader.read(base + mmd931::runtime::kMainStatePointerRva, &state, sizeof(state)) ||
        state == 0 ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kModelTable,
            models.data(),
            sizeof(models))) {
        return false;
    }

    std::size_t restored_bodies = 0;
    if (!restore_native_damping(
            reader,
            base,
            models,
            g_last_damping_model,
            g_last_damping_body_indices,
            restored_bodies)) {
        return false;
    }
    g_last_damping_body_indices.clear();
    g_last_damping_model = 0;
    return true;
#endif
}

bool wind_effect_active(const physics_control_studio::WindSettings& wind) noexcept {
    return g_wind_master_enabled.load() && g_wind_master_strength.load() > 0.0001f &&
        wind.enabled && wind.strength > 0.0001f;
}

float displayed_wind_strength(
    const physics_control_studio::WindSettings& wind) noexcept {
    return g_wind_master_strength.load() > 0.0001f ? wind.strength : 0.0f;
}

bool apply_physics_frame() {
#ifdef PCS_SURROGATE_HOST
    g_current_frame.store(0);
    const auto controls = control_snapshot_for_frame(0);
    const bool requested_wind_active = wind_effect_active(controls.wind);
    const bool controller_missing = requested_wind_active &&
        g_wind_source_mode.load() == WindSourceMode::PmxLocal;
    const bool wind_active = requested_wind_active && !controller_missing;
    g_controller_status.store(controller_missing ? 1 : 0);
    g_controller_count.store(0);
    if (!wind_active && !controls.physics.damping_enabled &&
        !controls.physics.gravity_enabled) {
        g_last_wind_qpc.store(0);
        g_wind_applied_bodies.store(0);
        g_wind_backend_status.store(controller_missing ? 3 : 0);
        return true;
    }
    if (!physics_control_studio::validate_wind_settings(controls.wind) ||
        !physics_control_studio::validate_physics_settings(controls.physics)) {
        g_wind_backend_status.store(-1);
        return false;
    }
    if (wind_active) {
        g_wind_applied_frames.fetch_add(1);
        g_wind_applied_bodies.store(2);
        g_wind_backend_status.store(2);
    } else {
        g_wind_applied_bodies.store(0);
        g_wind_backend_status.store(controller_missing ? 3 : 1);
    }
    return true;
#else
    if (g_host_status.load() != HostStatus::Supported) {
        g_wind_backend_status.store(-4);
        return false;
    }

    mmd931::runtime_access::ProcessReader reader(GetCurrentProcess());
    const std::uintptr_t base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::uintptr_t state = 0;
    std::uint32_t current_frame = 0;
    std::uint8_t selected_model = 0;
    std::array<float, 3> native_gravity_direction{};
    float native_gravity_acceleration = 0.0f;
    std::array<std::uintptr_t, 255> models{};
    if (!reader.read(base + mmd931::runtime::kMainStatePointerRva, &state, sizeof(state)) ||
        state == 0 ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kCurrentFrame,
            &current_frame,
            sizeof(current_frame)) ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kSelectedModel,
            &selected_model,
            sizeof(selected_model)) ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kGravityDirection,
            native_gravity_direction.data(),
            sizeof(native_gravity_direction)) ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kGravityAcceleration,
            &native_gravity_acceleration,
            sizeof(native_gravity_acceleration)) ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kModelTable,
            models.data(),
            sizeof(models))) {
        g_wind_backend_status.store(-2);
        return false;
    }
    g_current_frame.store(current_frame);

    const auto controls = control_snapshot_for_frame(current_frame);
    if (!physics_control_studio::validate_wind_settings(controls.wind) ||
        !physics_control_studio::validate_physics_settings(controls.physics)) {
        g_wind_filtered_acceleration.clear();
        g_wind_time_seconds = 0.0;
        g_wind_backend_status.store(-1);
        return false;
    }
    auto wind = controls.wind;
    wind.collision_group_mask = 0xffff;
    const bool requested_wind_active = wind_effect_active(wind);
    std::array<physics_control_studio::WindSettings, kMaximumWindControllers>
        wind_sources{};
    std::size_t wind_source_count = 0;
    bool controller_ready = !wind.controller_enabled;
    if (requested_wind_active && wind.controller_enabled) {
        std::array<WindControllerSample, kMaximumWindControllers> controllers{};
        const std::size_t controller_count = sample_wind_controllers(
            reader, models, controllers);
        controller_ready = controller_count > 0;
        for (std::size_t index = 0; index < controller_count; ++index) {
            auto source = wind;
            source.center = controllers[index].center;
            source.direction = controllers[index].direction;
            source.radius = controllers[index].radius;
            source.core_ratio = controllers[index].core_ratio;
            source.strength *= controllers[index].strength_scale;
            if (!physics_control_studio::validate_wind_settings(source)) {
                g_wind_filtered_acceleration.clear();
                g_wind_time_seconds = 0.0;
                g_wind_backend_status.store(-1);
                return false;
            }
            wind_sources[wind_source_count++] = source;
        }
    } else {
        g_controller_status.store(0);
        g_controller_count.store(0);
        if (!wind.controller_enabled) wind_sources[wind_source_count++] = wind;
    }
    const bool controller_missing = requested_wind_active &&
        wind.controller_enabled && !controller_ready;
    const bool wind_active = requested_wind_active && controller_ready;
    const bool damping_active = controls.physics.damping_enabled;
    const bool gravity_active = controls.physics.gravity_enabled;
    const PhysicsTargetModel physics_target =
        resolve_physics_target_model(reader, selected_model, models);

    // Bullet clears the total-force accumulator after each simulation step.
    // Muting wind therefore only stops new force writes and leaves velocity to MMD.
    if (!wind_active || g_wind_filter_model != physics_target.model) {
        g_wind_filtered_acceleration.clear();
        g_wind_filter_model = wind_active ? physics_target.model : 0;
    }

    // Runtime damping is persistent, so return bodies to their PMX values when
    // this plugin no longer owns the previous model or target set.
    if (g_last_damping_model != 0 &&
        (!damping_active || g_last_damping_model != physics_target.model)) {
        std::size_t restored_bodies = 0;
        if (!restore_native_damping(
                reader,
                base,
                models,
                g_last_damping_model,
                g_last_damping_body_indices,
                restored_bodies)) {
            g_wind_backend_status.store(-2);
            return false;
        }
        g_last_damping_body_indices.clear();
        g_last_damping_model = 0;
    }

    const bool timed_velocity_control = wind_active || gravity_active;
    const bool current_control_active = timed_velocity_control || damping_active;
    if (!current_control_active) {
        g_last_wind_qpc.store(0);
        g_wind_time_seconds = 0.0;
        g_wind_applied_bodies.store(0);
        g_wind_backend_status.store(controller_missing ? 3 : 0);
        return true;
    }

    LARGE_INTEGER counter{};
    LARGE_INTEGER frequency{};
    if (timed_velocity_control && (QueryPerformanceCounter(&counter) == FALSE ||
        QueryPerformanceFrequency(&frequency) == FALSE || frequency.QuadPart <= 0)) {
        g_wind_backend_status.store(-2);
        return false;
    }
    const std::int64_t previous_counter = timed_velocity_control
        ? g_last_wind_qpc.exchange(counter.QuadPart)
        : 0;
    const double elapsed_seconds = previous_counter > 0 && counter.QuadPart > previous_counter
        ? static_cast<double>(counter.QuadPart - previous_counter) /
            static_cast<double>(frequency.QuadPart)
        : 1.0 / 60.0;
    const float delta_seconds = static_cast<float>(
        std::clamp(elapsed_seconds, 1.0 / 1000.0, 1.0 / 15.0));
    if (wind_active) {
        g_wind_time_seconds += static_cast<double>(delta_seconds);
        if (!std::isfinite(g_wind_time_seconds)) {
            g_wind_time_seconds = 0.0;
            g_wind_filtered_acceleration.clear();
        }
    } else {
        // Gravity and damping may remain active while the wind is muted; do
        // not carry a stale wind phase or filtered acceleration into the next
        // activation.
        g_wind_time_seconds = 0.0;
        g_wind_filtered_acceleration.clear();
    }
    const double time_seconds = g_wind_time_seconds;

    if (physics_target.model == 0) {
        g_wind_applied_bodies.store(0);
        g_wind_backend_status.store(controller_missing ? 3 : 1);
        return true;
    }

    const std::uintptr_t model = physics_target.model;
    const std::uintptr_t rigid_bodies = physics_target.rigid_bodies;
    const std::uint32_t rigid_body_count = physics_target.rigid_body_count;

    std::vector<PendingBodyWrite> writes;
    writes.reserve(rigid_body_count);
    std::vector<std::uint32_t> next_damping_body_indices;
    next_damping_body_indices.reserve(rigid_body_count);
    const std::vector<std::uint32_t> previous_damping_body_indices =
        g_last_damping_model == model
        ? g_last_damping_body_indices
        : std::vector<std::uint32_t>{};
    std::size_t wind_applied_bodies = 0;

    const auto native_direction = physics_control_studio::normalize_or_zero({
        native_gravity_direction[0],
        native_gravity_direction[1],
        native_gravity_direction[2]});
    const auto desired_direction = physics_control_studio::normalize_or_zero(
        controls.physics.gravity_direction);
    const physics_control_studio::Vec3 gravity_delta{
        (desired_direction.x * controls.physics.gravity_acceleration -
            native_direction.x * native_gravity_acceleration) * 10.0f,
        (desired_direction.y * controls.physics.gravity_acceleration -
            native_direction.y * native_gravity_acceleration) * 10.0f,
        (desired_direction.z * controls.physics.gravity_acceleration -
            native_direction.z * native_gravity_acceleration) * 10.0f};

    for (std::uint32_t index = 0; index < rigid_body_count; ++index) {
        mmd931::model::physics::RigidBodyRecord record{};
        if (!reader.read(
                rigid_bodies + static_cast<std::uintptr_t>(index) * sizeof(record),
                &record,
                sizeof(record))) {
            g_wind_backend_status.store(-2);
            return false;
        }
        const bool dynamic_body =
            record.mode != mmd931::model::physics::BodyMode::BoneSynchronized;
        const bool wind_target = physics_control_studio::target_matches(
            controls.physics.wind_target,
            index,
            record.collision_group,
            dynamic_body);
        const bool damping_target = controls.physics.damping_enabled &&
            physics_control_studio::target_matches(
                controls.physics.damping_target,
                index,
                record.collision_group,
                dynamic_body);
        const bool damping_was_overridden = std::binary_search(
            previous_damping_body_indices.begin(),
            previous_damping_body_indices.end(),
            index);
        const bool gravity_target = controls.physics.gravity_enabled &&
            physics_control_studio::target_matches(
                controls.physics.gravity_target,
                index,
                record.collision_group,
                dynamic_body);
        const bool apply_wind = wind_active && wind_target;
        const bool needs_velocity_sample = apply_wind || gravity_target;
        const std::uintptr_t body = static_cast<std::uintptr_t>(record.runtime_object);
        if (body == 0) {
            if (damping_target || damping_was_overridden) {
                next_damping_body_indices.push_back(index);
            }
            continue;
        }
        if (!apply_wind) g_wind_filtered_acceleration.erase(body);
        if (damping_target) next_damping_body_indices.push_back(index);
        if (!needs_velocity_sample && !damping_target && !damping_was_overridden) {
            continue;
        }

        PendingBodyWrite pending{};
        pending.body = reinterpret_cast<void*>(body);
        pending.velocity_address =
            body + physics_control_studio::bullet_runtime_layout::kLinearVelocity;
        pending.force_address =
            body + physics_control_studio::bullet_runtime_layout::kTotalForce;

        if (needs_velocity_sample) {
            std::array<float, 4> position{};
            if ((apply_wind && !reader.read(
                    body + physics_control_studio::bullet_runtime_layout::kWorldPosition,
                    position.data(),
                    sizeof(position))) || !reader.read(
                    pending.velocity_address,
                    pending.original_velocity.data(),
                    sizeof(pending.original_velocity))) {
                g_wind_backend_status.store(-2);
                return false;
            }
            physics_control_studio::Vec3 velocity{
                pending.original_velocity[0],
                pending.original_velocity[1],
                pending.original_velocity[2]};
            if (apply_wind) {
                physics_control_studio::WindBodySample sample{};
                sample.body_mode = static_cast<std::uint8_t>(record.mode);
                sample.collision_group = record.collision_group;
                sample.position = {position[0], position[1], position[2]};
                sample.linear_velocity = velocity;
                std::array<physics_control_studio::Vec3, kMaximumWindControllers>
                    accelerations{};
                std::size_t acceleration_count = 0;
                for (std::size_t source_index = 0;
                     source_index < wind_source_count;
                     ++source_index) {
                    const auto evaluated = physics_control_studio::evaluate_wind(
                        wind_sources[source_index],
                        sample,
                        time_seconds,
                        delta_seconds);
                    const float squared = evaluated.acceleration.x *
                            evaluated.acceleration.x +
                        evaluated.acceleration.y * evaluated.acceleration.y +
                        evaluated.acceleration.z * evaluated.acceleration.z;
                    if (evaluated.affected && squared > 1.0e-8f) {
                        accelerations[acceleration_count++] = evaluated.acceleration;
                    }
                }
                physics_control_studio::Vec3 combined_acceleration{};
                if (acceleration_count == 1) {
                    combined_acceleration = accelerations[0];
                } else if (acceleration_count > 1) {
                    combined_acceleration =
                        physics_control_studio::combine_wind_accelerations(
                            accelerations.data(),
                            acceleration_count,
                            kMaximumCombinedWindAcceleration);
                    combined_acceleration =
                        physics_control_studio::limit_wind_acceleration_to_speed(
                            combined_acceleration,
                            sample.linear_velocity,
                            delta_seconds,
                            wind.maximum_speed);
                }
                physics_control_studio::Vec3 filtered_acceleration{};
                const float combined_squared = combined_acceleration.x *
                        combined_acceleration.x +
                    combined_acceleration.y * combined_acceleration.y +
                    combined_acceleration.z * combined_acceleration.z;
                if (combined_squared > 1.0e-8f) {
                    ++wind_applied_bodies;
                    filtered_acceleration = smooth_wind_acceleration(
                        body, combined_acceleration, delta_seconds);
                } else {
                    g_wind_filtered_acceleration.erase(body);
                }
                const float acceleration_squared = filtered_acceleration.x *
                        filtered_acceleration.x +
                    filtered_acceleration.y * filtered_acceleration.y +
                    filtered_acceleration.z * filtered_acceleration.z;
                if (acceleration_squared > 1.0e-8f &&
                    std::isfinite(record.mass) && record.mass > 1.0e-6f) {
                    if (!reader.read(
                            pending.force_address,
                            pending.original_force.data(),
                            sizeof(pending.original_force))) {
                        g_wind_backend_status.store(-2);
                        return false;
                    }
                    pending.updated_force = pending.original_force;
                    pending.updated_force[0] += filtered_acceleration.x * record.mass;
                    pending.updated_force[1] += filtered_acceleration.y * record.mass;
                    pending.updated_force[2] += filtered_acceleration.z * record.mass;
                    pending.write_force = true;
                    if (!writable_range(
                            pending.force_address,
                            sizeof(pending.updated_force))) {
                        g_wind_backend_status.store(-3);
                        return false;
                    }
                }
            }
            if (gravity_target) {
                velocity.x += gravity_delta.x * delta_seconds;
                velocity.y += gravity_delta.y * delta_seconds;
                velocity.z += gravity_delta.z * delta_seconds;
                pending.write_velocity = true;
            }
            if (pending.write_velocity) {
                pending.updated_velocity = {
                    velocity.x,
                    velocity.y,
                    velocity.z,
                    pending.original_velocity[3]};
                if (!writable_range(
                        pending.velocity_address,
                        sizeof(pending.updated_velocity))) {
                    g_wind_backend_status.store(-3);
                    return false;
                }
            }
        }
        if (damping_target || damping_was_overridden) {
            pending.damping_address =
                body + physics_control_studio::bullet_runtime_layout::kLinearDamping;
            if (!reader.read(
                    pending.damping_address,
                    pending.original_damping.data(),
                    sizeof(pending.original_damping))) {
                g_wind_backend_status.store(-2);
                return false;
            }
            pending.updated_damping = damping_target
                ? std::array<float, 2>{
                    controls.physics.linear_damping,
                    controls.physics.angular_damping}
                : native_damping_values(record);
            pending.write_damping = true;
            if (!writable_range(
                    pending.damping_address,
                    sizeof(pending.updated_damping))) {
                g_wind_backend_status.store(-3);
                return false;
            }
        }
        if (pending.write_velocity || pending.write_force || pending.write_damping) {
            writes.push_back(pending);
        }
    }

    if (!commit_body_writes(writes, base)) {
        g_wind_backend_status.store(-3);
        return false;
    }

    if (damping_active && !next_damping_body_indices.empty()) {
        g_last_damping_model = model;
        g_last_damping_body_indices = std::move(next_damping_body_indices);
    } else {
        g_last_damping_model = 0;
        g_last_damping_body_indices.clear();
    }

    if (wind_active) {
        g_wind_applied_frames.fetch_add(1);
        g_wind_applied_bodies.store(wind_applied_bodies);
        g_wind_backend_status.store(2);
    } else {
        g_wind_applied_bodies.store(0);
        g_wind_backend_status.store(
            controller_missing ? 3 : current_control_active ? 1 : 0);
    }
    return true;
#endif
}

std::wstring capture_physics_snapshot() {
    std::wostringstream output;
    output << L"WindTool diagnostics\r\n"
           << L"Wind backend: " << wind_backend_status_wide()
           << L" | affected frames=" << g_wind_applied_frames.load()
           << L" bodies=" << g_wind_applied_bodies.load() << L"\r\n"
           << L"Frame bridge: " << frame_bridge_status_wide()
           << L" | UI thread=" << g_ui_thread_id.load()
           << L" callback thread=" << g_frame_thread_id.load()
           << L" begin=" << g_begin_scene_count.load()
           << L" end=" << g_end_scene_count.load() << L"\r\n\r\n";
#ifdef PCS_SURROGATE_HOST
    output
        << L"Frame: 42\r\n"
        << L"Selected model slot: 0\r\n"
        << L"Rigid bodies: 2\r\n"
        << L"Joints: 1\r\n\r\n"
        << L"[Rigid Bodies]\r\n"
        << L"#0 surrogate-body | bone=3 mode=dynamic mass=1.000\r\n"
        << L"#1 surrogate-body-2 | bone=4 mode=dynamic+bone mass=2.000\r\n"
        << L"\r\n[Joint Constraints]\r\n"
        << L"#0 surrogate-joint | bodies=0->1 springT=(1, 2, 3)\r\n";
    return output.str();
#else
    const HostStatus status = validate_host();
    g_host_status.store(status);
    if (status != HostStatus::Supported) {
        output << L"Host status: " << status_name_wide(status) << L"\r\n";
        return output.str();
    }

    mmd931::runtime_access::ProcessReader reader(GetCurrentProcess());
    const std::uintptr_t base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::uintptr_t state = 0;
    if (!reader.read(
            base + mmd931::runtime::kMainStatePointerRva,
            &state,
            sizeof(state)) ||
        state == 0) {
        g_host_status.store(HostStatus::MainStateUnavailable);
        output << L"Main state is not available yet.\r\n";
        return output.str();
    }

    std::uint32_t frame = 0;
    std::uint8_t selected_model = 0;
    std::array<std::uintptr_t, 255> models{};
    if (!reader.read(
            state + mmd931::runtime_access::main_state::kCurrentFrame,
            &frame,
            sizeof(frame)) ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kSelectedModel,
            &selected_model,
            sizeof(selected_model)) ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kModelTable,
            models.data(),
            sizeof(models))) {
        output << L"Project state could not be read.\r\n";
        return output.str();
    }

    output << L"Frame: " << frame << L"\r\n"
           << L"Selected model slot: " << static_cast<unsigned>(selected_model)
           << L"\r\n";
    if (selected_model >= models.size() || models[selected_model] == 0) {
        output << L"No readable model is selected.\r\n";
        return output.str();
    }

    const std::uintptr_t model = models[selected_model];
    std::uintptr_t rigid_bodies = 0;
    std::uintptr_t joints = 0;
    std::uint32_t rigid_body_count = 0;
    std::uint32_t joint_count = 0;
    if (!reader.read(
            model + mmd931::model::state::kRigidBodies,
            &rigid_bodies,
            sizeof(rigid_bodies)) ||
        !reader.read(
            model + mmd931::model::state::kJoints,
            &joints,
            sizeof(joints)) ||
        !reader.read(
            model + mmd931::model::state::kRigidBodyCount,
            &rigid_body_count,
            sizeof(rigid_body_count)) ||
        !reader.read(
            model + mmd931::model::state::kJointCount,
            &joint_count,
            sizeof(joint_count)) ||
        rigid_body_count > physics_control_studio::kMaximumRuntimeElements ||
        joint_count > physics_control_studio::kMaximumRuntimeElements) {
        output << L"Physics inventory is unavailable or implausible.\r\n";
        return output.str();
    }

    output << L"Rigid bodies: " << rigid_body_count << L"\r\n"
           << L"Joints: " << joint_count << L"\r\n\r\n";
    output << std::fixed << std::setprecision(3);

    const std::uint32_t body_limit =
        rigid_body_count < kMaximumDisplayedItems ? rigid_body_count : kMaximumDisplayedItems;
    output << L"[Rigid Bodies]\r\n";
    for (std::uint32_t index = 0; index < body_limit; ++index) {
        mmd931::model::physics::RigidBodyRecord record{};
        if (rigid_bodies == 0 || !reader.read(
                rigid_bodies + static_cast<std::uintptr_t>(index) * sizeof(record),
                &record,
                sizeof(record))) {
            output << L"#" << index << L" <read failed>\r\n";
            continue;
        }
        output << L"#" << index << L" " << cp932_name(record.name, sizeof(record.name))
               << L" | bone=" << record.bone_index
               << L" mode=" << body_mode_name(static_cast<std::uint8_t>(record.mode))
               << L" group=" << static_cast<unsigned>(record.collision_group)
               << L" mask=0x" << std::hex << record.no_collision_group << std::dec
               << L" mass=" << record.mass
               << L" damp=" << record.linear_damping << L"/" << record.angular_damping
               << L" rest=" << record.restitution
               << L" friction=" << record.friction << L"\r\n";
    }
    if (body_limit < rigid_body_count) {
        output << L"... " << (rigid_body_count - body_limit)
               << L" additional rigid bodies omitted\r\n";
    }

    const std::uint32_t joint_limit =
        joint_count < kMaximumDisplayedItems ? joint_count : kMaximumDisplayedItems;
    output << L"\r\n[Joint Constraints]\r\n";
    for (std::uint32_t index = 0; index < joint_limit; ++index) {
        mmd931::model::physics::JointRecord record{};
        if (joints == 0 || !reader.read(
                joints + static_cast<std::uintptr_t>(index) * sizeof(record),
                &record,
                sizeof(record))) {
            output << L"#" << index << L" <read failed>\r\n";
            continue;
        }
        output << L"#" << index << L" " << cp932_name(record.name, sizeof(record.name))
               << L" | bodies=" << record.rigid_body_a << L"->" << record.rigid_body_b
               << L" springT=(" << record.spring_translation[0] << L", "
               << record.spring_translation[1] << L", " << record.spring_translation[2]
               << L") springR=(" << record.spring_rotation[0] << L", "
               << record.spring_rotation[1] << L", " << record.spring_rotation[2]
               << L") maxSep=" << record.max_separation << L"\r\n";
    }
    if (joint_limit < joint_count) {
        output << L"... " << (joint_count - joint_limit)
               << L" additional joints omitted\r\n";
    }
    return output.str();
#endif
}

void refresh_snapshot() {
    g_snapshot_text = capture_physics_snapshot();
}

void set_float_text(HWND control, float value) {
    if (control == nullptr) return;
    wchar_t text[32]{};
    std::swprintf(text, std::size(text), L"%.2f", static_cast<double>(value));
    wchar_t current[32]{};
    GetWindowTextW(control, current, static_cast<int>(std::size(current)));
    if (std::wcscmp(current, text) != 0) SetWindowTextW(control, text);
}

void set_button_check(HWND control, bool checked) {
    if (control == nullptr) return;
    const LRESULT desired = checked ? BST_CHECKED : BST_UNCHECKED;
    if (Button_GetCheck(control) != desired) Button_SetCheck(control, desired);
}

void set_combo_selection(HWND control, int selection) {
    if (control != nullptr && ComboBox_GetCurSel(control) != selection)
        ComboBox_SetCurSel(control, selection);
}

void set_slider_position(HWND control, int position) {
    if (control != nullptr && SendMessageW(control, TBM_GETPOS, 0, 0) != position)
        SendMessageW(control, TBM_SETPOS, TRUE, position);
}

void set_control_enabled(HWND control, bool enabled) {
    if (control != nullptr && IsWindowEnabled(control) != enabled)
        EnableWindow(control, enabled);
}

void set_control_visible(HWND control, bool visible) {
    if (control != nullptr && (IsWindowVisible(control) != FALSE) != visible)
        ShowWindow(control, visible ? SW_SHOWNA : SW_HIDE);
}

bool read_float_text(HWND control, float& value) {
    wchar_t text[128]{};
    if (control == nullptr || GetWindowTextW(control, text, std::size(text)) <= 0)
        return false;
    wchar_t* end = nullptr;
    const float parsed = std::wcstof(text, &end);
    if (end == text || *end != L'\0' || std::isfinite(parsed) == 0) return false;
    value = parsed;
    return true;
}

void set_control_font(HWND control, HFONT font) {
    if (control != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
    }
}

HWND create_panel_control(
    HWND parent,
    DWORD extended_style,
    const wchar_t* class_name,
    const wchar_t* text,
    DWORD style,
    ControlId id) {
    return CreateWindowExW(
        extended_style,
        class_name,
        text,
        WS_CHILD | style,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_module,
        nullptr);
}

int direction_preset_index(const physics_control_studio::Vec3& direction) noexcept {
    constexpr float epsilon = 0.0001f;
    const auto almost_equal = [epsilon](float left, float right) {
        return std::abs(left - right) < epsilon;
    };
    if (almost_equal(direction.x, 1.0f) && almost_equal(direction.y, 0.0f) &&
        almost_equal(direction.z, 0.0f))
        return 1;
    if (almost_equal(direction.x, -1.0f) && almost_equal(direction.y, 0.0f) &&
        almost_equal(direction.z, 0.0f))
        return 2;
    if (almost_equal(direction.x, 0.0f) && almost_equal(direction.y, 1.0f) &&
        almost_equal(direction.z, 0.0f))
        return 3;
    if (almost_equal(direction.x, 0.0f) && almost_equal(direction.y, -1.0f) &&
        almost_equal(direction.z, 0.0f))
        return 4;
    if (almost_equal(direction.x, 0.0f) && almost_equal(direction.y, 0.0f) &&
        almost_equal(direction.z, 1.0f))
        return 5;
    if (almost_equal(direction.x, 0.0f) && almost_equal(direction.y, 0.0f) &&
        almost_equal(direction.z, -1.0f))
        return 6;
    return 0;
}

physics_control_studio::Vec3 direction_from_preset(int selection) noexcept {
    switch (selection) {
    case 1: return {1.0f, 0.0f, 0.0f};
    case 2: return {-1.0f, 0.0f, 0.0f};
    case 3: return {0.0f, 1.0f, 0.0f};
    case 4: return {0.0f, -1.0f, 0.0f};
    case 5: return {0.0f, 0.0f, 1.0f};
    case 6: return {0.0f, 0.0f, -1.0f};
    default: return {};
    }
}

void apply_wind_preset(int selection) {
    ExclusiveLock lock(g_wind_lock);
    auto& wind = g_wind_settings;
    switch (selection) {
    case 1:
        wind.strength = 8.0f;
        wind.gust = 0.10f;
        wind.turbulence = 0.05f;
        wind.frequency = 0.25f;
        wind.maximum_speed = 55.0f;
        wind.noise_type = physics_control_studio::WindNoiseType::Smooth;
        break;
    case 2:
        wind.strength = 18.0f;
        wind.gust = 0.16f;
        wind.turbulence = 0.09f;
        wind.frequency = 0.42f;
        wind.maximum_speed = 95.0f;
        wind.noise_type = physics_control_studio::WindNoiseType::Perlin;
        break;
    case 3:
        wind.strength = 35.0f;
        wind.gust = 0.24f;
        wind.turbulence = 0.14f;
        wind.frequency = 0.65f;
        wind.maximum_speed = 165.0f;
        wind.noise_type = physics_control_studio::WindNoiseType::Perlin;
        break;
    case 4:
        wind.strength = 72.0f;
        wind.gust = 0.36f;
        wind.turbulence = 0.23f;
        wind.frequency = 0.92f;
        wind.maximum_speed = 260.0f;
        wind.noise_type = physics_control_studio::WindNoiseType::Fractal;
        break;
    case 5:
        wind.strength = 125.0f;
        wind.gust = 0.52f;
        wind.turbulence = 0.34f;
        wind.frequency = 1.20f;
        wind.maximum_speed = 420.0f;
        wind.noise_type = physics_control_studio::WindNoiseType::RandomGust;
        break;
    case 6:
        wind.strength = 230.0f;
        wind.gust = 0.70f;
        wind.turbulence = 0.52f;
        wind.frequency = 1.65f;
        wind.maximum_speed = 700.0f;
        wind.noise_type = physics_control_studio::WindNoiseType::Fractal;
        break;
    case 7:
        wind.strength = 650.0f;
        wind.gust = 0.85f;
        wind.turbulence = 0.65f;
        wind.frequency = 1.90f;
        wind.maximum_speed = 1'400.0f;
        wind.noise_type = physics_control_studio::WindNoiseType::Fractal;
        break;
    default:
        return;
    }
    g_wind_preset_selection = selection;
    g_wind_master_strength.store(wind.strength);
}

bool populate_target_combo(bool force = false) {
    if (g_group_combo == nullptr) return false;
    std::uintptr_t model = 0;
    [[maybe_unused]] std::uintptr_t rigid_bodies = 0;
    std::uint32_t rigid_body_count = 0;
#ifdef PCS_SURROGATE_HOST
    model = 1;
    rigid_body_count = 2;
    g_physics_target_model.store(model);
#else
    mmd931::runtime_access::ProcessReader reader(GetCurrentProcess());
    const std::uintptr_t base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::uintptr_t state = 0;
    std::uint8_t selected_model = 0;
    std::array<std::uintptr_t, 255> models{};
    if (!reader.read(base + mmd931::runtime::kMainStatePointerRva, &state, sizeof(state)) ||
        state == 0 ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kSelectedModel,
            &selected_model,
            sizeof(selected_model)) ||
        !reader.read(
            state + mmd931::runtime_access::main_state::kModelTable,
            models.data(),
            sizeof(models))) {
        return false;
    }
    const PhysicsTargetModel target =
        resolve_physics_target_model(reader, selected_model, models);
    model = target.model;
    rigid_bodies = target.rigid_bodies;
    rigid_body_count = target.rigid_body_count;
#endif
    if (!force && model == g_target_cache_model &&
        rigid_body_count == g_target_cache_count) {
        return false;
    }
    g_target_cache_model = model;
    g_target_cache_count = rigid_body_count;
    g_target_body_indices.clear();
    g_target_selection_anchor = -1;
    ComboBox_ResetContent(g_group_combo);
    if (g_target_list != nullptr) ListBox_ResetContent(g_target_list);
    ComboBox_AddString(g_group_combo, L"全部动态刚体");
    if (g_target_list != nullptr) ListBox_AddString(g_target_list, L"全部动态刚体");
    for (int group = 0; group < 16; ++group) {
        wchar_t label[32]{};
        std::swprintf(label, std::size(label), L"碰撞组 %d", group);
        ComboBox_AddString(g_group_combo, label);
        if (g_target_list != nullptr) ListBox_AddString(g_target_list, label);
    }

#ifdef PCS_SURROGATE_HOST
    g_target_body_indices = {0, 1};
    ComboBox_AddString(g_group_combo, L"刚体 0  测试头发");
    ComboBox_AddString(g_group_combo, L"刚体 1  测试裙摆");
    if (g_target_list != nullptr) {
        ListBox_AddString(g_target_list, L"刚体 0  测试头发");
        ListBox_AddString(g_target_list, L"刚体 1  测试裙摆");
    }
#else
    if (model != 0 && rigid_bodies != 0) {
        mmd931::runtime_access::ProcessReader reader(GetCurrentProcess());
        const std::uint32_t limit = std::min(rigid_body_count, kMaximumDisplayedItems);
        for (std::uint32_t index = 0; index < limit; ++index) {
            mmd931::model::physics::RigidBodyRecord record{};
            if (!reader.read(
                    rigid_bodies + static_cast<std::uintptr_t>(index) * sizeof(record),
                    &record,
                    sizeof(record)) ||
                record.mode == mmd931::model::physics::BodyMode::BoneSynchronized ||
                record.runtime_object == 0) {
                continue;
            }
            const std::wstring name = cp932_name(record.name, sizeof(record.name));
            wchar_t label[96]{};
            std::swprintf(
                label,
                std::size(label),
                L"刚体 %u  %.64ls",
                static_cast<unsigned>(index),
                name.c_str());
            ComboBox_AddString(g_group_combo, label);
            if (g_target_list != nullptr) ListBox_AddString(g_target_list, label);
            g_target_body_indices.push_back(index);
        }
    }
#endif
    g_last_synced_frame = UINT32_MAX;
    return true;
}

int target_combo_selection(const physics_control_studio::TargetSelection& target) {
    if (target.kind == physics_control_studio::TargetKind::AllDynamic) return 0;
    if (target.kind == physics_control_studio::TargetKind::CollisionGroup &&
        target.index < 16) {
        return static_cast<int>(target.index + 1);
    }
    if (target.kind == physics_control_studio::TargetKind::RigidBody) {
        const auto found = std::find(
            g_target_body_indices.begin(), g_target_body_indices.end(), target.index);
        if (found != g_target_body_indices.end()) {
            return 17 + static_cast<int>(found - g_target_body_indices.begin());
        }
    }
    return 0;
}

physics_control_studio::TargetSelection target_from_combo_selection(int selection) {
    if (selection <= 0) return {};
    if (selection <= 16) {
        return {
            physics_control_studio::TargetKind::CollisionGroup,
            static_cast<std::uint32_t>(selection - 1)};
    }
    const std::size_t body_item = static_cast<std::size_t>(selection - 17);
    if (body_item < g_target_body_indices.size()) {
        return {
            physics_control_studio::TargetKind::RigidBody,
            g_target_body_indices[body_item]};
    }
    return {};
}

void sync_target_list(const physics_control_studio::TargetSelection& target) {
    if (g_target_list == nullptr) return;
    ListBox_SetSel(g_target_list, FALSE, -1);
    if (target.kind == physics_control_studio::TargetKind::AllDynamic) {
        ListBox_SetSel(g_target_list, TRUE, 0);
        return;
    }
    if (target.kind == physics_control_studio::TargetKind::CollisionGroup && target.index < 16) {
        ListBox_SetSel(g_target_list, TRUE, static_cast<int>(target.index + 1));
        return;
    }
    if (target.kind == physics_control_studio::TargetKind::RigidBody) {
        const auto found = std::find(
            g_target_body_indices.begin(), g_target_body_indices.end(), target.index);
        if (found != g_target_body_indices.end()) {
            ListBox_SetSel(
                g_target_list,
                TRUE,
                17 + static_cast<int>(found - g_target_body_indices.begin()));
        }
        return;
    }
    if (target.kind == physics_control_studio::TargetKind::CustomSet) {
        for (int group = 0; group < 16; ++group) {
            if ((target.collision_group_mask & (1u << group)) != 0)
                ListBox_SetSel(g_target_list, TRUE, group + 1);
        }
        for (const std::uint32_t body : target.rigid_body_indices) {
            const auto found = std::find(
                g_target_body_indices.begin(), g_target_body_indices.end(), body);
            if (found != g_target_body_indices.end()) {
                ListBox_SetSel(
                    g_target_list,
                    TRUE,
                    17 + static_cast<int>(found - g_target_body_indices.begin()));
            }
        }
    }
}

physics_control_studio::TargetSelection target_from_list_selection() {
    if (g_target_list == nullptr) return {};
    if (ListBox_GetSel(g_target_list, 0) > 0) return {};
    std::uint16_t group_mask = 0;
    std::vector<std::uint32_t> bodies;
    for (int group = 0; group < 16; ++group) {
        if (ListBox_GetSel(g_target_list, group + 1) > 0)
            group_mask = static_cast<std::uint16_t>(group_mask | (1u << group));
    }
    for (std::size_t index = 0; index < g_target_body_indices.size(); ++index) {
        if (ListBox_GetSel(g_target_list, 17 + static_cast<int>(index)) > 0)
            bodies.push_back(g_target_body_indices[index]);
    }
    if (bodies.empty() && group_mask != 0 && (group_mask & (group_mask - 1)) == 0) {
        for (std::uint32_t group = 0; group < 16; ++group) {
            if ((group_mask & (1u << group)) != 0)
                return {physics_control_studio::TargetKind::CollisionGroup, group};
        }
    }
    if (group_mask == 0 && bodies.size() == 1)
        return {physics_control_studio::TargetKind::RigidBody, bodies.front()};
    physics_control_studio::TargetSelection target;
    target.kind = physics_control_studio::TargetKind::CustomSet;
    target.collision_group_mask = group_mask;
    target.rigid_body_indices = std::move(bodies);
    return target;
}

std::size_t selected_target_item_count() {
    if (g_target_list == nullptr) return 0;
    const LRESULT count = SendMessageW(g_target_list, LB_GETSELCOUNT, 0, 0);
    return count > 0 ? static_cast<std::size_t>(count) : 0;
}

void update_settings_from_controls(bool update_target = false);

bool target_selections_equal(
    const physics_control_studio::TargetSelection& left,
    const physics_control_studio::TargetSelection& right) {
    return left.kind == right.kind && left.index == right.index &&
        left.collision_group_mask == right.collision_group_mask &&
        left.rigid_body_indices == right.rigid_body_indices;
}

const physics_control_studio::TargetSelection& target_for_layer(
    const physics_control_studio::PhysicsSettings& settings,
    TargetLayer layer) noexcept {
    switch (layer) {
    case TargetLayer::Wind: return settings.wind_target;
    case TargetLayer::Damping: return settings.damping_target;
    case TargetLayer::Gravity: return settings.gravity_target;
    }
    return settings.wind_target;
}

physics_control_studio::TargetSelection& target_for_layer(
    physics_control_studio::PhysicsSettings& settings,
    TargetLayer layer) noexcept {
    switch (layer) {
    case TargetLayer::Wind: return settings.wind_target;
    case TargetLayer::Damping: return settings.damping_target;
    case TargetLayer::Gravity: return settings.gravity_target;
    }
    return settings.wind_target;
}

const char* target_layer_name(TargetLayer layer) noexcept {
    switch (layer) {
    case TargetLayer::Wind: return "wind";
    case TargetLayer::Damping: return "damping";
    case TargetLayer::Gravity: return "gravity";
    }
    return "wind";
}

const char* target_kind_name(
    const physics_control_studio::TargetSelection& target) noexcept {
    switch (target.kind) {
    case physics_control_studio::TargetKind::AllDynamic: return "all_dynamic";
    case physics_control_studio::TargetKind::CollisionGroup: return "collision_group";
    case physics_control_studio::TargetKind::RigidBody: return "rigid_body";
    case physics_control_studio::TargetKind::CustomSet: return "custom_set";
    }
    return "custom_set";
}

void populate_target_group_combo(int selected_index = -1) {
    if (g_target_group_combo == nullptr) return;
    std::vector<physics_control_studio::TargetGroup> groups;
    {
        SharedLock lock(g_wind_lock);
        groups = g_target_groups;
    }
    ComboBox_ResetContent(g_target_group_combo);
    for (const auto& group : groups) {
        const std::wstring name = utf8_to_wide(group.name);
        ComboBox_AddString(g_target_group_combo, name.c_str());
    }
    if (!groups.empty()) {
        const int clamped = selected_index >= 0 &&
                selected_index < static_cast<int>(groups.size())
            ? selected_index
            : 0;
        ComboBox_SetCurSel(g_target_group_combo, clamped);
    }
}

std::wstring target_group_name_from_edit() {
    if (g_target_group_name == nullptr) return {};
    const int length = GetWindowTextLengthW(g_target_group_name);
    if (length <= 0) return {};
    std::wstring name(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(g_target_group_name, name.data(), length + 1);
    name.resize(static_cast<std::size_t>(length));
    const auto first = name.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    const auto last = name.find_last_not_of(L" \t\r\n");
    name = name.substr(first, last - first + 1);
    if (name.size() > 64) name.resize(64);
    return name;
}

void sync_target_group_name_from_combo() {
    if (g_target_group_combo == nullptr || g_target_group_name == nullptr) return;
    const int selection = ComboBox_GetCurSel(g_target_group_combo);
    if (selection < 0) return;
    wchar_t name[128]{};
    ComboBox_GetLBText(g_target_group_combo, selection, name);
    SetWindowTextW(g_target_group_name, name);
}

bool save_target_group() {
    const std::wstring wide_name = target_group_name_from_edit();
    const std::string name = wide_to_utf8(wide_name);
    if (name.empty()) {
        g_track_status = L"请输入分组名称";
        return false;
    }
    const auto target = target_from_list_selection();
    int selected_index = 0;
    {
        ExclusiveLock lock(g_wind_lock);
        const auto found = std::find_if(
            g_target_groups.begin(),
            g_target_groups.end(),
            [&](const physics_control_studio::TargetGroup& group) {
                return group.name == name;
            });
        if (found == g_target_groups.end()) {
            g_target_groups.push_back({name, target});
            selected_index = static_cast<int>(g_target_groups.size() - 1);
        } else {
            found->target = target;
            selected_index = static_cast<int>(found - g_target_groups.begin());
        }
    }
    populate_target_group_combo(selected_index);
    g_track_status = L"目标分组已保存";
    save_track_file();
    return true;
}

bool apply_target_group() {
    if (g_target_group_combo == nullptr) return false;
    const int selection = ComboBox_GetCurSel(g_target_group_combo);
    physics_control_studio::TargetSelection target;
    {
        SharedLock lock(g_wind_lock);
        if (selection < 0 || selection >= static_cast<int>(g_target_groups.size())) {
            g_track_status = L"请选择目标分组";
            return false;
        }
        target = g_target_groups[static_cast<std::size_t>(selection)].target;
    }
    g_syncing_controls = true;
    sync_target_list(target);
    g_syncing_controls = false;
    update_settings_from_controls(true);
    sync_target_group_name_from_combo();
    g_track_status = L"目标分组已应用";
    return true;
}

bool delete_target_group() {
    if (g_target_group_combo == nullptr) return false;
    const int selection = ComboBox_GetCurSel(g_target_group_combo);
    {
        ExclusiveLock lock(g_wind_lock);
        if (selection < 0 || selection >= static_cast<int>(g_target_groups.size())) {
            g_track_status = L"请选择目标分组";
            return false;
        }
        g_target_groups.erase(g_target_groups.begin() + selection);
    }
    populate_target_group_combo(selection);
    if (ComboBox_GetCount(g_target_group_combo) > 0) {
        sync_target_group_name_from_combo();
    } else if (g_target_group_name != nullptr) {
        SetWindowTextW(g_target_group_name, L"");
    }
    g_track_status = L"目标分组已删除";
    save_track_file();
    return true;
}

LRESULT CALLBACK target_list_subclass_proc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    UINT_PTR,
    DWORD_PTR) {
    if (message == WM_LBUTTONDOWN) {
        const DWORD hit = static_cast<DWORD>(SendMessageW(
            window,
            LB_ITEMFROMPOINT,
            0,
            MAKELPARAM(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))));
        const int item = LOWORD(hit);
        const bool outside = HIWORD(hit) != 0;
        const bool shift = (wparam & MK_SHIFT) != 0 ||
            (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (!outside && shift &&
            g_target_selection_anchor >= 0) {
            const bool append = (wparam & MK_CONTROL) != 0 ||
                (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (!append) ListBox_SetSel(window, FALSE, -1);
            if (item == 0) {
                ListBox_SetSel(window, FALSE, -1);
                ListBox_SetSel(window, TRUE, 0);
            } else {
                ListBox_SetSel(window, FALSE, 0);
                const int first = std::max(1, std::min(g_target_selection_anchor, item));
                const int last = std::max(g_target_selection_anchor, item);
                for (int index = first; index <= last; ++index)
                    ListBox_SetSel(window, TRUE, index);
            }
            SendMessageW(window, LB_SETCARETINDEX, item, FALSE);
            SendMessageW(
                GetParent(window),
                WM_COMMAND,
                MAKEWPARAM(kTargetListId, LBN_SELCHANGE),
                reinterpret_cast<LPARAM>(window));
            return 0;
        }
        if (!outside) g_target_selection_anchor = item;
    }
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(window, target_list_subclass_proc, kTargetListSubclassId);
    return DefSubclassProc(window, message, wparam, lparam);
}

void sync_page_visibility() {
    const std::array<HWND, 7> common_controls{{
        g_wind_page,
        g_physics_page,
        g_target_page,
        g_set_key,
        g_delete_key,
        g_save_json,
        g_load_json}};
    for (const HWND control : common_controls) set_control_visible(control, true);
    const bool wind_page = g_panel_page == PanelPage::Wind;
    const bool physics_page = g_panel_page == PanelPage::Physics;
    const bool target_page = g_panel_page == PanelPage::Target;
    const std::array<HWND, 17> wind_controls{{
        g_wind_enabled,
        g_source_mode_combo,
        g_wind_preset_combo,
        g_field_type_combo,
        g_noise_type_combo,
        g_strength_slider,
        g_gust_slider,
        g_turbulence_slider,
        g_frequency_slider,
        g_direction_preset_combo,
        g_direction_x,
        g_direction_y,
        g_direction_z,
        g_center_x,
        g_center_y,
        g_center_z,
        g_falloff_type_combo}};
    for (const HWND control : wind_controls) {
        set_control_visible(control, wind_page);
    }
    const std::array<HWND, 8> physics_controls{{
        g_damping_enabled,
        g_linear_damping_slider,
        g_angular_damping_slider,
        g_gravity_enabled,
        g_gravity_x,
        g_gravity_y,
        g_gravity_z,
        g_gravity_acceleration_slider}};
    for (const HWND control : physics_controls) {
        set_control_visible(control, physics_page);
    }
    const std::array<HWND, 10> target_controls{{
        g_target_layer_combo,
        g_target_list,
        g_select_all,
        g_clear_selection,
        g_invert_selection,
        g_target_group_combo,
        g_target_group_name,
        g_save_target_group,
        g_apply_target_group,
        g_delete_target_group}};
    for (const HWND control : target_controls) {
        set_control_visible(control, target_page);
    }
    set_control_visible(g_radius_slider, false);
    set_control_visible(g_core_ratio_slider, false);
    set_control_visible(g_group_combo, false);
}

void sync_controls(bool evaluate_track = true) {
    const auto controls = evaluate_track
        ? control_snapshot_for_frame(g_current_frame.load())
        : control_snapshot();
    if (evaluate_track) {
        ExclusiveLock lock(g_wind_lock);
        g_wind_settings = controls.wind;
        g_physics_settings = controls.physics;
    }
    const auto& settings = controls.wind;
    const auto& physics = controls.physics;
    g_syncing_controls = true;
    set_button_check(g_wind_enabled, g_wind_master_enabled.load());
    const WindSourceMode source_mode = g_wind_source_mode.load();
    set_combo_selection(g_source_mode_combo, static_cast<int>(source_mode));
    set_combo_selection(g_wind_preset_combo, g_wind_preset_selection);
    set_combo_selection(g_field_type_combo, static_cast<int>(settings.field_type));
    set_combo_selection(g_noise_type_combo, static_cast<int>(settings.noise_type));
    set_combo_selection(g_falloff_type_combo, static_cast<int>(settings.falloff_type));
    set_slider_position(
        g_strength_slider,
        static_cast<int>(displayed_wind_strength(settings)));
    set_slider_position(g_gust_slider, static_cast<int>(settings.gust * 100.0f));
    set_slider_position(
        g_turbulence_slider, static_cast<int>(settings.turbulence * 100.0f));
    set_slider_position(g_frequency_slider, static_cast<int>(settings.frequency * 100.0f));
    set_combo_selection(g_direction_preset_combo, direction_preset_index(settings.direction));
    set_float_text(g_direction_x, settings.direction.x);
    set_float_text(g_direction_y, settings.direction.y);
    set_float_text(g_direction_z, settings.direction.z);
    set_float_text(g_center_x, settings.center.x);
    set_float_text(g_center_y, settings.center.y);
    set_float_text(g_center_z, settings.center.z);
    set_slider_position(g_radius_slider, static_cast<int>(settings.radius));
    set_slider_position(
        g_core_ratio_slider, static_cast<int>(settings.core_ratio * 100.0f));
    set_button_check(g_damping_enabled, physics.damping_enabled);
    set_slider_position(
        g_linear_damping_slider, static_cast<int>(physics.linear_damping * 100.0f));
    set_slider_position(
        g_angular_damping_slider, static_cast<int>(physics.angular_damping * 100.0f));
    set_button_check(g_gravity_enabled, physics.gravity_enabled);
    set_float_text(g_gravity_x, physics.gravity_direction.x);
    set_float_text(g_gravity_y, physics.gravity_direction.y);
    set_float_text(g_gravity_z, physics.gravity_direction.z);
    set_slider_position(
        g_gravity_acceleration_slider,
        static_cast<int>(physics.gravity_acceleration * 10.0f));
    set_combo_selection(g_target_layer_combo, static_cast<int>(g_target_layer));
    const auto& active_target = target_for_layer(physics, g_target_layer);
    set_combo_selection(g_group_combo, target_combo_selection(active_target));
    if (!target_selections_equal(target_from_list_selection(), active_target))
        sync_target_list(active_target);
    const bool radial = settings.field_type == physics_control_studio::WindFieldType::RadialOut ||
        settings.field_type == physics_control_studio::WindFieldType::RadialIn;
    const bool controller = source_mode == WindSourceMode::PmxLocal;
    const bool uses_direction = !radial && !controller;
    const bool uses_center = !controller && (radial ||
        settings.field_type == physics_control_studio::WindFieldType::Vortex ||
        settings.field_type == physics_control_studio::WindFieldType::Updraft ||
        settings.field_type == physics_control_studio::WindFieldType::Downburst ||
        settings.field_type == physics_control_studio::WindFieldType::Shear);
    set_control_enabled(g_direction_preset_combo, uses_direction);
    set_control_enabled(g_direction_x, uses_direction);
    set_control_enabled(g_direction_y, uses_direction);
    set_control_enabled(g_direction_z, uses_direction);
    set_control_enabled(g_center_x, uses_center);
    set_control_enabled(g_center_y, uses_center);
    set_control_enabled(g_center_z, uses_center);
    set_control_enabled(g_radius_slider, false);
    set_control_enabled(g_core_ratio_slider, false);
    set_control_enabled(g_falloff_type_combo, controller);
    set_control_enabled(g_linear_damping_slider, physics.damping_enabled);
    set_control_enabled(g_angular_damping_slider, physics.damping_enabled);
    set_control_enabled(g_gravity_x, physics.gravity_enabled);
    set_control_enabled(g_gravity_y, physics.gravity_enabled);
    set_control_enabled(g_gravity_z, physics.gravity_enabled);
    set_control_enabled(g_gravity_acceleration_slider, physics.gravity_enabled);
    sync_page_visibility();
    g_last_synced_frame = g_current_frame.load();
    g_last_controller_ui_status = g_controller_status.load();
    g_last_controller_ui_count = static_cast<int>(g_controller_count.load());
    g_syncing_controls = false;
}

void update_settings_from_controls(bool update_target) {
    if (g_syncing_controls) return;
    ExclusiveLock lock(g_wind_lock);
    auto wind = g_wind_settings;
    auto physics = g_physics_settings;
    wind.strength = static_cast<float>(
        SendMessageW(g_strength_slider, TBM_GETPOS, 0, 0));
    wind.gust = static_cast<float>(
        SendMessageW(g_gust_slider, TBM_GETPOS, 0, 0)) / 100.0f;
    wind.turbulence = static_cast<float>(
        SendMessageW(g_turbulence_slider, TBM_GETPOS, 0, 0)) / 100.0f;
    wind.frequency = static_cast<float>(
        SendMessageW(g_frequency_slider, TBM_GETPOS, 0, 0)) / 100.0f;
    physics.linear_damping = static_cast<float>(
        SendMessageW(g_linear_damping_slider, TBM_GETPOS, 0, 0)) / 100.0f;
    physics.angular_damping = static_cast<float>(
        SendMessageW(g_angular_damping_slider, TBM_GETPOS, 0, 0)) / 100.0f;
    physics.gravity_acceleration = static_cast<float>(
        SendMessageW(g_gravity_acceleration_slider, TBM_GETPOS, 0, 0)) / 10.0f;
    const int field_selection = ComboBox_GetCurSel(g_field_type_combo);
    if (field_selection >= 0 && field_selection <= 7) {
        wind.field_type = static_cast<physics_control_studio::WindFieldType>(
            field_selection);
    }
    const int noise_selection = ComboBox_GetCurSel(g_noise_type_combo);
    if (noise_selection >= 0 && noise_selection <= 4) {
        wind.noise_type = static_cast<physics_control_studio::WindNoiseType>(
            noise_selection);
    }
    const int falloff_selection = ComboBox_GetCurSel(g_falloff_type_combo);
    if (falloff_selection >= 0 && falloff_selection <= 3) {
        wind.falloff_type = static_cast<physics_control_studio::WindFalloffType>(
            falloff_selection);
    }
    WindSourceMode source_mode = g_wind_source_mode.load();
    const int source_selection = ComboBox_GetCurSel(g_source_mode_combo);
    if (source_selection >= static_cast<int>(WindSourceMode::Global) &&
        source_selection <= static_cast<int>(WindSourceMode::PmxLocal)) {
        source_mode = static_cast<WindSourceMode>(source_selection);
    }
    apply_wind_source_mode(wind, source_mode);
    physics_control_studio::normalize_wind_settings(wind);
    if (update_target) {
        auto selected_target = g_target_list != nullptr
            ? target_from_list_selection()
            : target_from_combo_selection(ComboBox_GetCurSel(g_group_combo));
        target_for_layer(physics, g_target_layer) = selected_target;
    }
    wind.collision_group_mask =
        physics.wind_target.kind == physics_control_studio::TargetKind::CollisionGroup
        ? static_cast<std::uint16_t>(1u << physics.wind_target.index)
        : 0xffff;

    float value = 0.0f;
    if (read_float_text(g_direction_x, value)) wind.direction.x = value;
    if (read_float_text(g_direction_y, value)) wind.direction.y = value;
    if (read_float_text(g_direction_z, value)) wind.direction.z = value;
    if (read_float_text(g_center_x, value)) wind.center.x = value;
    if (read_float_text(g_center_y, value)) wind.center.y = value;
    if (read_float_text(g_center_z, value)) wind.center.z = value;
    if (read_float_text(g_gravity_x, value)) physics.gravity_direction.x = value;
    if (read_float_text(g_gravity_y, value)) physics.gravity_direction.y = value;
    if (read_float_text(g_gravity_z, value)) physics.gravity_direction.z = value;

    if (physics_control_studio::validate_wind_settings(wind) &&
        physics_control_studio::validate_physics_settings(physics)) {
        const bool enabled_changed = wind.enabled != g_wind_settings.enabled ||
            physics.gravity_enabled != g_physics_settings.gravity_enabled;
        const bool source_changed = source_mode != g_wind_source_mode.load();
        g_wind_settings = wind;
        g_physics_settings = physics;
        g_wind_source_mode.store(source_mode);
        if (source_changed) reset_wind_source_runtime(source_mode);
        if (enabled_changed) g_last_wind_qpc.store(0);
    }
}

RECT close_button_rect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{
        client.right - kPanelPadding - kCloseButtonSize,
        (kPanelHeaderHeight - kCloseButtonSize) / 2,
        client.right - kPanelPadding,
        (kPanelHeaderHeight + kCloseButtonSize) / 2};
}

void layout_panel(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = std::max(0, static_cast<int>(client.right));
    const int field_x = 134;
    const int field_width = std::max(150, width - field_x - 24);
    const int triple_width = std::max(48, (field_width - 12) / 3);
    const int page_width = std::max(82, (width - 56) / 3);
    const int action_width = std::max(72, (width - 64) / 4);
    const auto move = [](HWND control, int x, int y, int w, int h) {
        if (control != nullptr) MoveWindow(control, x, y, w, h, TRUE);
    };
    move(g_wind_page, 20, 64, page_width, 28);
    move(g_physics_page, 28 + page_width, 64, page_width, 28);
    move(g_target_page, 36 + page_width * 2, 64, page_width, 28);
    move(g_wind_enabled, 20, 100, 112, 28);
    move(g_source_mode_combo, 188, 100, std::max(120, width - 212), 160);
    move(g_wind_preset_combo, field_x, 136, field_width, 200);
    move(g_field_type_combo, field_x, 172, field_width, 220);
    move(g_noise_type_combo, field_x, 208, field_width, 200);
    move(g_strength_slider, field_x, 244, std::max(80, field_width - 58), 28);
    move(g_gust_slider, field_x, 280, std::max(80, field_width - 58), 28);
    move(g_turbulence_slider, field_x, 316, std::max(80, field_width - 58), 28);
    move(g_frequency_slider, field_x, 352, std::max(80, field_width - 58), 28);
    move(g_direction_preset_combo, field_x, 388, field_width, 180);
    move(g_direction_x, field_x, 424, triple_width, 25);
    move(g_direction_y, field_x + triple_width + 6, 424, triple_width, 25);
    move(g_direction_z, field_x + (triple_width + 6) * 2, 424, triple_width, 25);
    move(g_center_x, field_x, 460, triple_width, 25);
    move(g_center_y, field_x + triple_width + 6, 460, triple_width, 25);
    move(g_center_z, field_x + (triple_width + 6) * 2, 460, triple_width, 25);
    move(g_radius_slider, -1000, -1000, 10, 10);
    move(g_core_ratio_slider, -1000, -1000, 10, 10);
    move(g_falloff_type_combo, field_x, 496, field_width, 160);

    move(g_damping_enabled, 20, 100, 190, 28);
    move(g_linear_damping_slider, field_x, 136, std::max(80, field_width - 58), 28);
    move(g_angular_damping_slider, field_x, 172, std::max(80, field_width - 58), 28);
    move(g_gravity_enabled, 20, 208, 190, 28);
    move(g_gravity_x, field_x, 244, triple_width, 25);
    move(g_gravity_y, field_x + triple_width + 6, 244, triple_width, 25);
    move(g_gravity_z, field_x + (triple_width + 6) * 2, 244, triple_width, 25);
    move(
        g_gravity_acceleration_slider,
        field_x,
        280,
        std::max(80, field_width - 58),
        28);

    move(g_group_combo, -1000, -1000, 10, 10);
    move(g_target_layer_combo, 112, 100, std::max(120, width - 136), 120);
    move(g_target_list, 20, 158, std::max(120, width - 40), 182);
    const int target_button_width = std::max(80, (width - 56) / 3);
    move(g_select_all, 20, 350, target_button_width, 28);
    move(g_clear_selection, 28 + target_button_width, 350, target_button_width, 28);
    move(g_invert_selection, 36 + target_button_width * 2, 350, target_button_width, 28);
    move(g_target_group_combo, 112, 388, std::max(120, width - 136), 180);
    move(g_target_group_name, 112, 424, std::max(120, width - 136), 25);
    move(g_save_target_group, 20, 458, target_button_width, 28);
    move(g_apply_target_group, 28 + target_button_width, 458, target_button_width, 28);
    move(g_delete_target_group, 36 + target_button_width * 2, 458, target_button_width, 28);

    const int action_y = 610;
    move(g_set_key, 20, action_y, action_width, 30);
    move(g_delete_key, 28 + action_width, action_y, action_width, 30);
    move(g_save_json, 36 + action_width * 2, action_y, action_width, 30);
    move(g_load_json, 44 + action_width * 3, action_y, action_width, 30);
    HRGN region = CreateRoundRectRgn(
        0,
        0,
        client.right + 1,
        client.bottom + 1,
        kPanelCornerRadius,
        kPanelCornerRadius);
    if (region != nullptr) SetWindowRgn(window, region, TRUE);
}

void create_panel_resources(HWND window) {
    if (g_panel_brush == nullptr) g_panel_brush = CreateSolidBrush(kPanelColor);
    if (g_content_brush == nullptr) g_content_brush = CreateSolidBrush(kContentColor);
    const HDC dc = GetDC(window);
    const int dpi = dc == nullptr ? 96 : GetDeviceCaps(dc, LOGPIXELSY);
    if (dc != nullptr) ReleaseDC(window, dc);
    if (g_panel_font == nullptr) {
        g_panel_font = CreateFontW(
            -MulDiv(10, dpi, 72),
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI");
    }
}

void destroy_panel_resources() {
    if (g_panel_font != nullptr) DeleteObject(g_panel_font);
    if (g_panel_brush != nullptr) DeleteObject(g_panel_brush);
    if (g_content_brush != nullptr) DeleteObject(g_content_brush);
    g_panel_font = nullptr;
    g_panel_brush = nullptr;
    g_content_brush = nullptr;
}

RECT timeline_rect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{20, 654, client.right - 20, client.bottom - kPanelFooterHeight - 8};
}

std::pair<std::uint32_t, std::uint32_t> timeline_range() {
    const std::uint32_t focus = g_selected_key_frame != UINT32_MAX
        ? g_selected_key_frame
        : g_current_frame.load();
    const std::uint32_t start = focus > 60 ? focus - 60 : 0;
    return {start, start + 120};
}

int timeline_frame_x(const RECT& bounds, std::uint32_t frame) {
    const auto [start, end] = timeline_range();
    const int left = bounds.left + 56;
    const int right = bounds.right - 12;
    if (end <= start || right <= left) return left;
    const double amount = static_cast<double>(frame - start) /
        static_cast<double>(end - start);
    return left + static_cast<int>(amount * static_cast<double>(right - left));
}

void paint_timeline(HWND window, HDC dc) {
    const RECT bounds = timeline_rect(window);
    if (bounds.bottom <= bounds.top + 28 || bounds.right <= bounds.left) return;
    HBRUSH background = CreateSolidBrush(kContentColor);
    FillRect(dc, &bounds, background);
    DeleteObject(background);
    HPEN border = CreatePen(PS_SOLID, 1, kBorderColor);
    HGDIOBJ old_pen = SelectObject(dc, border);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);

    const auto [start, end] = timeline_range();
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_panel_font);
    SetTextColor(dc, kTextColor);
    RECT title{bounds.left + 12, bounds.top + 5, bounds.right - 12, bounds.top + 27};
    wchar_t header[96]{};
    std::swprintf(
        header,
        std::size(header),
        L"物理关键帧轨道   F%u - F%u",
        static_cast<unsigned>(start),
        static_cast<unsigned>(end));
    DrawTextW(dc, header, -1, &title, DT_SINGLELINE | DT_VCENTER);

    const int ruler_top = bounds.top + 32;
    const int lane_y = std::min(static_cast<int>(bounds.bottom) - 20, ruler_top + 34);
    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(47, 52, 60));
    old_pen = SelectObject(dc, grid_pen);
    for (std::uint32_t frame = start - start % 10; frame <= end; frame += 10) {
        if (frame < start) continue;
        const int x = timeline_frame_x(bounds, frame);
        MoveToEx(dc, x, ruler_top, nullptr);
        LineTo(dc, x, bounds.bottom - 8);
        if (frame % 30 == 0) {
            wchar_t label[24]{};
            std::swprintf(label, std::size(label), L"%u", static_cast<unsigned>(frame));
            RECT tick{x - 18, ruler_top - 2, x + 18, ruler_top + 18};
            SetTextColor(dc, kMutedTextColor);
            DrawTextW(dc, label, -1, &tick, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }
        if (end - frame < 10) break;
    }
    SelectObject(dc, old_pen);
    DeleteObject(grid_pen);

    SetTextColor(dc, kMutedTextColor);
    RECT lane_label{bounds.left + 10, lane_y - 12, bounds.left + 54, lane_y + 12};
    DrawTextW(dc, L"参数", -1, &lane_label, DT_SINGLELINE | DT_VCENTER);
    HPEN lane_pen = CreatePen(PS_SOLID, 2, RGB(76, 84, 95));
    old_pen = SelectObject(dc, lane_pen);
    MoveToEx(dc, bounds.left + 56, lane_y, nullptr);
    LineTo(dc, bounds.right - 12, lane_y);
    SelectObject(dc, old_pen);
    DeleteObject(lane_pen);

    const auto keys = control_keys_snapshot();
    for (const auto& key : keys) {
        if (key.frame < start || key.frame > end) continue;
        const int x = timeline_frame_x(bounds, key.frame);
        const bool selected = key.frame == g_selected_key_frame;
        POINT diamond[4]{{x, lane_y - 7}, {x + 7, lane_y}, {x, lane_y + 7}, {x - 7, lane_y}};
        HBRUSH key_brush = CreateSolidBrush(selected ? kWarningColor : kAccentColor);
        HPEN key_pen = CreatePen(PS_SOLID, 1, selected ? RGB(255, 220, 140) : kAccentColor);
        old_brush = SelectObject(dc, key_brush);
        old_pen = SelectObject(dc, key_pen);
        Polygon(dc, diamond, 4);
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
        DeleteObject(key_pen);
        DeleteObject(key_brush);
    }

    const std::uint32_t current = g_current_frame.load();
    if (current >= start && current <= end) {
        const int x = timeline_frame_x(bounds, current);
        HPEN playhead = CreatePen(PS_SOLID, 2, RGB(232, 104, 112));
        old_pen = SelectObject(dc, playhead);
        MoveToEx(dc, x, bounds.top + 25, nullptr);
        LineTo(dc, x, bounds.bottom - 5);
        SelectObject(dc, old_pen);
        DeleteObject(playhead);
    }
}

bool select_timeline_key(HWND window, POINT point) {
    const RECT bounds = timeline_rect(window);
    if (!PtInRect(&bounds, point)) return false;
    const auto [start, end] = timeline_range();
    std::uint32_t closest = UINT32_MAX;
    int closest_distance = 9;
    for (const auto& key : control_keys_snapshot()) {
        if (key.frame < start || key.frame > end) continue;
        const int distance = std::abs(point.x - timeline_frame_x(bounds, key.frame));
        if (distance < closest_distance) {
            closest = key.frame;
            closest_distance = distance;
        }
    }
    g_selected_key_frame = closest;
    InvalidateRect(window, &bounds, FALSE);
    return true;
}

void paint_panel(HWND window) {
    PAINTSTRUCT paint{};
    const HDC paint_dc = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);
    HDC buffer_dc = nullptr;
    HBITMAP buffer_bitmap = nullptr;
    HGDIOBJ previous_bitmap = nullptr;
    if (client.right > 0 && client.bottom > 0) {
        buffer_dc = CreateCompatibleDC(paint_dc);
        if (buffer_dc != nullptr) {
            buffer_bitmap = CreateCompatibleBitmap(paint_dc, client.right, client.bottom);
            if (buffer_bitmap != nullptr)
                previous_bitmap = SelectObject(buffer_dc, buffer_bitmap);
        }
    }
    const HDC dc = buffer_dc != nullptr && buffer_bitmap != nullptr ? buffer_dc : paint_dc;
    FillRect(dc, &client, g_panel_brush);

    RECT header{0, 0, client.right, kPanelHeaderHeight};
    HBRUSH header_brush = CreateSolidBrush(kHeaderColor);
    FillRect(dc, &header, header_brush);
    DeleteObject(header_brush);

    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_panel_font);
    SetTextColor(dc, kTextColor);
    RECT title{16, 8, client.right - 120, 31};
    DrawTextW(dc, L"WindTool", -1, &title, DT_SINGLELINE | DT_VCENTER);
    SetTextColor(dc, kMutedTextColor);
    RECT subtitle{16, 28, client.right - 120, 48};
    DrawTextW(
        dc,
        g_panel_page == PanelPage::Wind
            ? L"风力系统"
            : g_panel_page == PanelPage::Physics ? L"刚体物理" : L"批量目标",
        -1,
        &subtitle,
        DT_SINGLELINE | DT_VCENTER);

    const bool bridge_online = g_frame_thread_id.load() != 0 &&
        !g_frame_thread_mismatch.load();
    const auto controls = control_snapshot();
    const auto& settings = controls.wind;
    const auto& physics = controls.physics;
    const bool any_enabled = wind_effect_active(settings) || physics.damping_enabled ||
        physics.gravity_enabled;
    const int backend_status = g_wind_backend_status.load();
    const bool backend_ok = backend_status >= 0;
    const bool waiting_for_pmx = backend_status == 3;
    const COLORREF status_color = !bridge_online || !backend_ok || waiting_for_pmx
        ? kWarningColor
        : any_enabled ? kAccentColor : kMutedTextColor;
    HBRUSH status_brush = CreateSolidBrush(
        status_color);
    HGDIOBJ old_brush = SelectObject(dc, status_brush);
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, client.right - 102, 21, client.right - 94, 29);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(status_brush);
    SetTextColor(dc, status_color);
    RECT live{client.right - 89, 12, client.right - 49, 38};
    DrawTextW(
        dc,
        !bridge_online || waiting_for_pmx
            ? L"等待"
            : any_enabled ? L"启用" : L"关闭",
        -1,
        &live,
        DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    const RECT close = close_button_rect(window);
    if (g_close_button_hot) {
        HBRUSH hover = CreateSolidBrush(RGB(55, 59, 67));
        FillRect(dc, &close, hover);
        DeleteObject(hover);
    }
    HPEN close_pen = CreatePen(PS_SOLID, 2, kMutedTextColor);
    HGDIOBJ previous_pen = SelectObject(dc, close_pen);
    MoveToEx(dc, close.left + 10, close.top + 10, nullptr);
    LineTo(dc, close.right - 10, close.bottom - 10);
    MoveToEx(dc, close.right - 10, close.top + 10, nullptr);
    LineTo(dc, close.left + 10, close.bottom - 10);
    SelectObject(dc, previous_pen);
    DeleteObject(close_pen);

    SelectObject(dc, g_panel_font);
    SetTextColor(dc, kMutedTextColor);
    const auto draw_label = [&](const wchar_t* text, int top) {
        RECT label{20, top, 128, top + 26};
        DrawTextW(dc, text, -1, &label, DT_SINGLELINE | DT_VCENTER);
    };
    if (g_panel_page == PanelPage::Wind) {
        RECT source_label{140, 99, 184, 127};
        DrawTextW(
            dc,
            L"风源",
            -1,
            &source_label,
            DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        draw_label(L"环境预设", 135);
        draw_label(L"风力类型", 171);
        draw_label(L"噪波类型", 207);
        draw_label(L"风力强度", 243);
        draw_label(L"阵风幅度", 279);
        draw_label(L"噪波强度", 315);
        draw_label(L"变化频率", 351);
        draw_label(L"方向预设", 387);
        draw_label(L"方向 X Y Z", 423);
        draw_label(L"中心 X Y Z", 459);
        draw_label(L"距离衰减", 495);
    } else if (g_panel_page == PanelPage::Physics) {
        draw_label(L"线性阻尼", 135);
        draw_label(L"角阻尼", 171);
        draw_label(L"重力 X Y Z", 243);
        draw_label(L"重力强度", 279);
    } else {
        draw_label(L"作用层", 99);
        RECT target_hint{20, 130, client.right - 20, 154};
        DrawTextW(
            dc,
            L"直接点击复选；Shift 连选，Ctrl+Shift 追加",
            -1,
            &target_hint,
            DT_SINGLELINE | DT_VCENTER);
        draw_label(L"已保存分组", 387);
        draw_label(L"分组名称", 423);
    }

    wchar_t value_text[128]{};
    SetTextColor(dc, kTextColor);
    if (g_panel_page == PanelPage::Wind) {
        std::swprintf(
            value_text,
            std::size(value_text),
            L"%.0f",
            static_cast<double>(displayed_wind_strength(settings)));
        RECT strength_value{client.right - 62, 235, client.right - 24, 255};
        DrawTextW(dc, value_text, -1, &strength_value, DT_SINGLELINE | DT_RIGHT);
        std::swprintf(
            value_text,
            std::size(value_text),
            L"%.0f%%",
            static_cast<double>(settings.gust * 100.0f));
        RECT gust_value{client.right - 72, 271, client.right - 24, 291};
        DrawTextW(dc, value_text, -1, &gust_value, DT_SINGLELINE | DT_RIGHT);
        std::swprintf(
            value_text,
            std::size(value_text),
            L"%.2f Hz",
            static_cast<double>(settings.frequency));
        std::swprintf(
            value_text,
            std::size(value_text),
            L"%.0f%%",
            static_cast<double>(settings.turbulence * 100.0f));
        RECT turbulence_value{client.right - 72, 307, client.right - 24, 327};
        DrawTextW(dc, value_text, -1, &turbulence_value, DT_SINGLELINE | DT_RIGHT);
        std::swprintf(
            value_text,
            std::size(value_text),
            L"%.2f Hz",
            static_cast<double>(settings.frequency));
        RECT frequency_value{client.right - 92, 343, client.right - 24, 363};
        DrawTextW(dc, value_text, -1, &frequency_value, DT_SINGLELINE | DT_RIGHT);
    } else if (g_panel_page == PanelPage::Physics) {
        std::swprintf(
            value_text,
            std::size(value_text),
            L"%.0f%%",
            static_cast<double>(physics.linear_damping * 100.0f));
        RECT linear_value{client.right - 72, 127, client.right - 24, 147};
        DrawTextW(dc, value_text, -1, &linear_value, DT_SINGLELINE | DT_RIGHT);
        std::swprintf(
            value_text,
            std::size(value_text),
            L"%.0f%%",
            static_cast<double>(physics.angular_damping * 100.0f));
        RECT angular_value{client.right - 72, 163, client.right - 24, 183};
        DrawTextW(dc, value_text, -1, &angular_value, DT_SINGLELINE | DT_RIGHT);
        std::swprintf(
            value_text,
            std::size(value_text),
            L"%.1f",
            static_cast<double>(physics.gravity_acceleration));
        RECT gravity_value{client.right - 72, 271, client.right - 24, 291};
        DrawTextW(dc, value_text, -1, &gravity_value, DT_SINGLELINE | DT_RIGHT);
    }
    paint_timeline(window, dc);

    HPEN border_pen = CreatePen(PS_SOLID, 1, kBorderColor);
    previous_pen = SelectObject(dc, border_pen);
    old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, 0, 0, client.right, client.bottom);
    SelectObject(dc, old_brush);
    SelectObject(dc, previous_pen);
    DeleteObject(border_pen);

    SelectObject(dc, g_panel_font);
    SetTextColor(dc, kMutedTextColor);
    RECT footer{
        kPanelPadding,
        client.bottom - kPanelFooterHeight,
        client.right - kPanelPadding,
        client.bottom};
    const int controller_status = g_controller_status.load();
    wchar_t source_text_buffer[32]{};
    const wchar_t* source_text = L"全局风场";
    if (g_wind_source_mode.load() == WindSourceMode::PmxLocal) {
        if (controller_status == 2) {
            std::swprintf(
                source_text_buffer,
                std::size(source_text_buffer),
                L"PMX已连接 %u",
                static_cast<unsigned>(g_controller_count.load()));
            source_text = source_text_buffer;
        } else {
            source_text = controller_status == 1 ? L"PMX未发现" : L"PMX局部";
        }
    }
    std::swprintf(
        value_text,
        std::size(value_text),
        L"F%u  |  %zu 键  |  %zu 项目标  |  %ls  |  %ls",
        static_cast<unsigned>(g_current_frame.load()),
        control_key_count(),
        selected_target_item_count(),
        g_track_status.c_str(),
        source_text);
    DrawTextW(dc, value_text, -1, &footer, DT_SINGLELINE | DT_VCENTER);
    if (dc != paint_dc) {
        BitBlt(
            paint_dc,
            0,
            0,
            client.right,
            client.bottom,
            dc,
            0,
            0,
            SRCCOPY);
        SelectObject(buffer_dc, previous_bitmap);
        DeleteObject(buffer_bitmap);
        DeleteDC(buffer_dc);
    } else if (buffer_dc != nullptr) {
        DeleteDC(buffer_dc);
    }
    EndPaint(window, &paint);
}

void paint_owner_button(const DRAWITEMSTRUCT& item) {
    const HDC dc = item.hDC;
    RECT bounds = item.rcItem;
    FillRect(dc, &bounds, g_panel_brush);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_panel_font);

    if (item.CtlID == kWindEnabledId || item.CtlID == kDampingEnabledId ||
        item.CtlID == kGravityEnabledId) {
        const auto controls = control_snapshot();
        const bool checked = item.CtlID == kWindEnabledId
            ? g_wind_master_enabled.load()
            : item.CtlID == kDampingEnabledId
                ? controls.physics.damping_enabled
                : controls.physics.gravity_enabled;
        RECT toggle{bounds.left + 1, bounds.top + 5, bounds.left + 39, bounds.bottom - 5};
        HBRUSH toggle_brush = CreateSolidBrush(checked ? kAccentColor : RGB(60, 66, 75));
        HPEN toggle_pen = CreatePen(PS_SOLID, 1, checked ? kAccentColor : kBorderColor);
        HGDIOBJ old_brush = SelectObject(dc, toggle_brush);
        HGDIOBJ old_pen = SelectObject(dc, toggle_pen);
        RoundRect(dc, toggle.left, toggle.top, toggle.right, toggle.bottom, 16, 16);
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
        DeleteObject(toggle_pen);
        DeleteObject(toggle_brush);

        const int knob_size = toggle.bottom - toggle.top - 4;
        const int knob_left = checked
            ? toggle.right - knob_size - 2
            : toggle.left + 2;
        HBRUSH knob = CreateSolidBrush(RGB(241, 244, 247));
        old_brush = SelectObject(dc, knob);
        old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(
            dc,
            knob_left,
            toggle.top + 2,
            knob_left + knob_size,
            toggle.bottom - 2);
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
        DeleteObject(knob);

        wchar_t label[64]{};
        GetWindowTextW(item.hwndItem, label, std::size(label));
        SetTextColor(dc, kTextColor);
        RECT text_rect{bounds.left + 50, bounds.top, bounds.right, bounds.bottom};
        DrawTextW(dc, label, -1, &text_rect, DT_SINGLELINE | DT_VCENTER);
        return;
    }

    if (item.CtlID == kWindPageId || item.CtlID == kPhysicsPageId ||
        item.CtlID == kTargetPageId) {
        const bool selected = item.CtlID == kWindPageId
            ? g_panel_page == PanelPage::Wind
            : item.CtlID == kPhysicsPageId
                ? g_panel_page == PanelPage::Physics
                : g_panel_page == PanelPage::Target;
        HBRUSH button_brush = CreateSolidBrush(
            selected ? RGB(45, 78, 70) : RGB(39, 43, 50));
        FillRect(dc, &bounds, button_brush);
        DeleteObject(button_brush);
        HPEN border = CreatePen(PS_SOLID, 1, selected ? kAccentColor : kBorderColor);
        HGDIOBJ old_pen = SelectObject(dc, border);
        HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        SelectObject(dc, old_brush);
        SelectObject(dc, old_pen);
        DeleteObject(border);
        wchar_t label[64]{};
        GetWindowTextW(item.hwndItem, label, std::size(label));
        SetTextColor(dc, selected ? kTextColor : kMutedTextColor);
        DrawTextW(dc, label, -1, &bounds, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        return;
    }

    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool keyed = item.CtlID == kSetKeyId && current_frame_has_key();
    HBRUSH button_brush = CreateSolidBrush(
        pressed ? RGB(52, 58, 66) : keyed ? RGB(45, 78, 70) : RGB(39, 43, 50));
    FillRect(dc, &bounds, button_brush);
    DeleteObject(button_brush);
    HPEN border = CreatePen(
        PS_SOLID,
        1,
        pressed || keyed ? kAccentColor : kBorderColor);
    HGDIOBJ old_pen = SelectObject(dc, border);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);
    wchar_t label[64]{};
    GetWindowTextW(item.hwndItem, label, std::size(label));
    SetTextColor(dc, kTextColor);
    DrawTextW(dc, label, -1, &bounds, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

void paint_group_item(const DRAWITEMSTRUCT& item) {
    if (item.itemID == static_cast<UINT>(-1)) return;
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    HBRUSH background = CreateSolidBrush(selected ? RGB(45, 78, 70) : kContentColor);
    FillRect(item.hDC, &item.rcItem, background);
    DeleteObject(background);
    wchar_t text[64]{};
    SendMessageW(
        item.hwndItem,
        CB_GETLBTEXT,
        item.itemID,
        reinterpret_cast<LPARAM>(text));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, kTextColor);
    SelectObject(item.hDC, g_panel_font);
    RECT label = item.rcItem;
    label.left += 8;
    DrawTextW(item.hDC, text, -1, &label, DT_SINGLELINE | DT_VCENTER);
}

void paint_target_item(const DRAWITEMSTRUCT& item) {
    if (item.itemID == static_cast<UINT>(-1)) return;
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    HBRUSH background = CreateSolidBrush(selected ? RGB(39, 57, 55) : kContentColor);
    FillRect(item.hDC, &item.rcItem, background);
    DeleteObject(background);
    RECT check{
        item.rcItem.left + 8,
        item.rcItem.top + 6,
        item.rcItem.left + 22,
        item.rcItem.top + 20};
    HBRUSH check_brush = CreateSolidBrush(selected ? kAccentColor : RGB(36, 40, 47));
    HPEN check_pen = CreatePen(PS_SOLID, 1, selected ? kAccentColor : kBorderColor);
    HGDIOBJ old_brush = SelectObject(item.hDC, check_brush);
    HGDIOBJ old_pen = SelectObject(item.hDC, check_pen);
    Rectangle(item.hDC, check.left, check.top, check.right, check.bottom);
    if (selected) {
        HPEN mark = CreatePen(PS_SOLID, 2, RGB(245, 248, 250));
        SelectObject(item.hDC, mark);
        MoveToEx(item.hDC, check.left + 3, check.top + 7, nullptr);
        LineTo(item.hDC, check.left + 6, check.bottom - 3);
        LineTo(item.hDC, check.right - 2, check.top + 3);
        SelectObject(item.hDC, check_pen);
        DeleteObject(mark);
    }
    SelectObject(item.hDC, old_pen);
    SelectObject(item.hDC, old_brush);
    DeleteObject(check_pen);
    DeleteObject(check_brush);

    wchar_t text[96]{};
    SendMessageW(
        item.hwndItem,
        LB_GETTEXT,
        item.itemID,
        reinterpret_cast<LPARAM>(text));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, kTextColor);
    SelectObject(item.hDC, g_panel_font);
    RECT label = item.rcItem;
    label.left += 32;
    DrawTextW(item.hDC, text, -1, &label, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

LRESULT CALLBACK panel_window_proc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        create_panel_resources(window);
        constexpr DWORD kDwmUseImmersiveDarkMode = 20;
        constexpr DWORD kDwmWindowCornerPreference = 33;
        const BOOL dark_mode = TRUE;
        const DWORD rounded_corner = 2;
        DwmSetWindowAttribute(
            window,
            kDwmUseImmersiveDarkMode,
            &dark_mode,
            sizeof(dark_mode));
        DwmSetWindowAttribute(
            window,
            kDwmWindowCornerPreference,
            &rounded_corner,
            sizeof(rounded_corner));
        SetLayeredWindowAttributes(window, 0, 248, LWA_ALPHA);
        g_wind_page = create_panel_control(
            window, 0, L"BUTTON", L"风力系统", BS_OWNERDRAW, kWindPageId);
        g_physics_page = create_panel_control(
            window, 0, L"BUTTON", L"刚体", BS_OWNERDRAW, kPhysicsPageId);
        g_target_page = create_panel_control(
            window, 0, L"BUTTON", L"目标", BS_OWNERDRAW, kTargetPageId);
        g_wind_enabled = create_panel_control(
            window, 0, L"BUTTON", L"启用风力", BS_OWNERDRAW, kWindEnabledId);
        g_source_mode_combo = create_panel_control(
            window,
            0,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            kSourceModeComboId);
        g_wind_preset_combo = create_panel_control(
            window,
            0,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            kWindPresetComboId);
        g_field_type_combo = create_panel_control(
            window,
            0,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            kFieldTypeComboId);
        g_noise_type_combo = create_panel_control(
            window,
            0,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            kNoiseTypeComboId);
        g_strength_slider = create_panel_control(
            window, 0, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS, kStrengthSliderId);
        g_gust_slider = create_panel_control(
            window, 0, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS, kGustSliderId);
        g_turbulence_slider = create_panel_control(
            window,
            0,
            TRACKBAR_CLASSW,
            L"",
            TBS_HORZ | TBS_NOTICKS,
            kTurbulenceSliderId);
        g_frequency_slider = create_panel_control(
            window, 0, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS, kFrequencySliderId);
        g_direction_preset_combo = create_panel_control(
            window,
            0,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            kDirectionPresetComboId);
        g_direction_x = create_panel_control(
            window, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_CENTER | ES_AUTOHSCROLL, kDirectionXId);
        g_direction_y = create_panel_control(
            window, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_CENTER | ES_AUTOHSCROLL, kDirectionYId);
        g_direction_z = create_panel_control(
            window, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_CENTER | ES_AUTOHSCROLL, kDirectionZId);
        g_center_x = create_panel_control(
            window, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_CENTER | ES_AUTOHSCROLL, kCenterXId);
        g_center_y = create_panel_control(
            window, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_CENTER | ES_AUTOHSCROLL, kCenterYId);
        g_center_z = create_panel_control(
            window, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_CENTER | ES_AUTOHSCROLL, kCenterZId);
        g_radius_slider = create_panel_control(
            window, 0, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS, kRadiusSliderId);
        g_core_ratio_slider = create_panel_control(
            window,
            0,
            TRACKBAR_CLASSW,
            L"",
            TBS_HORZ | TBS_NOTICKS,
            kCoreRatioSliderId);
        g_falloff_type_combo = create_panel_control(
            window,
            0,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            kFalloffTypeComboId);
        g_group_combo = create_panel_control(
            window,
            0,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            kGroupComboId);
        g_target_layer_combo = create_panel_control(
            window,
            0,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            kTargetLayerComboId);
        g_target_list = create_panel_control(
            window,
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            L"",
            LBS_MULTIPLESEL | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS |
                LBS_NOINTEGRALHEIGHT |
                WS_VSCROLL | WS_BORDER,
            kTargetListId);
        g_select_all = create_panel_control(
            window, 0, L"BUTTON", L"全部", BS_OWNERDRAW, kSelectAllId);
        g_clear_selection = create_panel_control(
            window, 0, L"BUTTON", L"清空", BS_OWNERDRAW, kClearSelectionId);
        g_invert_selection = create_panel_control(
            window, 0, L"BUTTON", L"反选", BS_OWNERDRAW, kInvertSelectionId);
        g_target_group_combo = create_panel_control(
            window,
            0,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            kTargetGroupComboId);
        g_target_group_name = create_panel_control(
            window,
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            ES_AUTOHSCROLL,
            kTargetGroupNameId);
        g_save_target_group = create_panel_control(
            window, 0, L"BUTTON", L"保存分组", BS_OWNERDRAW, kSaveTargetGroupId);
        g_apply_target_group = create_panel_control(
            window, 0, L"BUTTON", L"应用分组", BS_OWNERDRAW, kApplyTargetGroupId);
        g_delete_target_group = create_panel_control(
            window, 0, L"BUTTON", L"删除分组", BS_OWNERDRAW, kDeleteTargetGroupId);
        g_damping_enabled = create_panel_control(
            window, 0, L"BUTTON", L"覆盖阻尼", BS_OWNERDRAW, kDampingEnabledId);
        g_linear_damping_slider = create_panel_control(
            window,
            0,
            TRACKBAR_CLASSW,
            L"",
            TBS_HORZ | TBS_NOTICKS,
            kLinearDampingSliderId);
        g_angular_damping_slider = create_panel_control(
            window,
            0,
            TRACKBAR_CLASSW,
            L"",
            TBS_HORZ | TBS_NOTICKS,
            kAngularDampingSliderId);
        g_gravity_enabled = create_panel_control(
            window, 0, L"BUTTON", L"覆盖重力", BS_OWNERDRAW, kGravityEnabledId);
        g_gravity_x = create_panel_control(
            window, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_CENTER | ES_AUTOHSCROLL, kGravityXId);
        g_gravity_y = create_panel_control(
            window, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_CENTER | ES_AUTOHSCROLL, kGravityYId);
        g_gravity_z = create_panel_control(
            window, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_CENTER | ES_AUTOHSCROLL, kGravityZId);
        g_gravity_acceleration_slider = create_panel_control(
            window,
            0,
            TRACKBAR_CLASSW,
            L"",
            TBS_HORZ | TBS_NOTICKS,
            kGravityAccelerationSliderId);
        g_set_key = create_panel_control(
            window, 0, L"BUTTON", L"设置关键帧", BS_OWNERDRAW, kSetKeyId);
        g_delete_key = create_panel_control(
            window, 0, L"BUTTON", L"删除关键帧", BS_OWNERDRAW, kDeleteKeyId);
        g_save_json = create_panel_control(
            window, 0, L"BUTTON", L"保存 JSON", BS_OWNERDRAW, kSaveJsonId);
        g_load_json = create_panel_control(
            window, 0, L"BUTTON", L"读取 JSON", BS_OWNERDRAW, kLoadJsonId);
        if (g_wind_page == nullptr || g_physics_page == nullptr ||
            g_target_page == nullptr || g_wind_enabled == nullptr ||
            g_source_mode_combo == nullptr ||
            g_wind_preset_combo == nullptr || g_field_type_combo == nullptr ||
            g_noise_type_combo == nullptr ||
            g_strength_slider == nullptr || g_gust_slider == nullptr ||
            g_turbulence_slider == nullptr ||
            g_frequency_slider == nullptr || g_direction_preset_combo == nullptr ||
            g_direction_x == nullptr ||
            g_direction_y == nullptr || g_direction_z == nullptr ||
            g_center_x == nullptr || g_center_y == nullptr || g_center_z == nullptr ||
            g_radius_slider == nullptr || g_core_ratio_slider == nullptr ||
            g_falloff_type_combo == nullptr ||
            g_group_combo == nullptr || g_target_layer_combo == nullptr ||
            g_target_list == nullptr ||
            g_select_all == nullptr || g_clear_selection == nullptr ||
            g_invert_selection == nullptr || g_target_group_combo == nullptr ||
            g_target_group_name == nullptr || g_save_target_group == nullptr ||
            g_apply_target_group == nullptr || g_delete_target_group == nullptr ||
            g_damping_enabled == nullptr ||
            g_linear_damping_slider == nullptr || g_angular_damping_slider == nullptr ||
            g_gravity_enabled == nullptr || g_gravity_x == nullptr ||
            g_gravity_y == nullptr || g_gravity_z == nullptr ||
            g_gravity_acceleration_slider == nullptr || g_set_key == nullptr ||
            g_delete_key == nullptr || g_save_json == nullptr ||
            g_load_json == nullptr) {
            return -1;
        }
        SetWindowSubclass(
            g_target_list, target_list_subclass_proc, kTargetListSubclassId, 0);
        SendMessageW(
            g_strength_slider,
            TBM_SETRANGE,
            TRUE,
            MAKELPARAM(0, kMaximumPanelWindStrength));
        SendMessageW(g_gust_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(g_turbulence_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(g_frequency_slider, TBM_SETRANGE, TRUE, MAKELPARAM(5, 400));
        SendMessageW(g_radius_slider, TBM_SETRANGE, TRUE, MAKELPARAM(1, 200));
        SendMessageW(g_core_ratio_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(g_linear_damping_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(g_angular_damping_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(
            g_gravity_acceleration_slider,
            TBM_SETRANGE,
            TRUE,
            MAKELPARAM(0, 300));
        ComboBox_AddString(g_source_mode_combo, L"全局风场");
        ComboBox_AddString(g_source_mode_combo, L"PMX 局部");
        ComboBox_AddString(g_wind_preset_combo, L"自定义");
        ComboBox_AddString(g_wind_preset_combo, L"微风");
        ComboBox_AddString(g_wind_preset_combo, L"轻风");
        ComboBox_AddString(g_wind_preset_combo, L"稳定风");
        ComboBox_AddString(g_wind_preset_combo, L"强风");
        ComboBox_AddString(g_wind_preset_combo, L"烈风");
        ComboBox_AddString(g_wind_preset_combo, L"飓风");
        ComboBox_AddString(g_wind_preset_combo, L"狂暴风");
        ComboBox_AddString(g_field_type_combo, L"定向风");
        ComboBox_AddString(g_field_type_combo, L"空间湍流");
        ComboBox_AddString(g_field_type_combo, L"涡旋风");
        ComboBox_AddString(g_field_type_combo, L"径向外推");
        ComboBox_AddString(g_field_type_combo, L"径向吸入");
        ComboBox_AddString(g_field_type_combo, L"热上升气流");
        ComboBox_AddString(g_field_type_combo, L"下击暴流");
        ComboBox_AddString(g_field_type_combo, L"风切变");
        ComboBox_AddString(g_noise_type_combo, L"平滑阵风");
        ComboBox_AddString(g_noise_type_combo, L"柏林噪波");
        ComboBox_AddString(g_noise_type_combo, L"分形湍流");
        ComboBox_AddString(g_noise_type_combo, L"脉冲阵风");
        ComboBox_AddString(g_noise_type_combo, L"随机阵风");
        ComboBox_AddString(g_falloff_type_combo, L"硬边界");
        ComboBox_AddString(g_falloff_type_combo, L"线性衰减");
        ComboBox_AddString(g_falloff_type_combo, L"平滑衰减");
        ComboBox_AddString(g_falloff_type_combo, L"平方衰减");
        ComboBox_AddString(g_direction_preset_combo, L"自定义");
        ComboBox_AddString(g_direction_preset_combo, L"+X");
        ComboBox_AddString(g_direction_preset_combo, L"-X");
        ComboBox_AddString(g_direction_preset_combo, L"+Y");
        ComboBox_AddString(g_direction_preset_combo, L"-Y");
        ComboBox_AddString(g_direction_preset_combo, L"+Z");
        ComboBox_AddString(g_direction_preset_combo, L"-Z");
        ComboBox_AddString(g_target_layer_combo, L"风力层");
        ComboBox_AddString(g_target_layer_combo, L"阻尼层");
        ComboBox_AddString(g_target_layer_combo, L"重力层");
        ComboBox_SetCurSel(g_target_layer_combo, static_cast<int>(g_target_layer));
        populate_target_combo(true);
        const std::array<HWND, 45> controls{{
            g_wind_page,
            g_physics_page,
            g_target_page,
            g_wind_enabled,
            g_source_mode_combo,
            g_wind_preset_combo,
            g_field_type_combo,
            g_noise_type_combo,
            g_strength_slider,
            g_gust_slider,
            g_turbulence_slider,
            g_frequency_slider,
            g_direction_preset_combo,
            g_direction_x,
            g_direction_y,
            g_direction_z,
            g_center_x,
            g_center_y,
            g_center_z,
            g_radius_slider,
            g_core_ratio_slider,
            g_falloff_type_combo,
            g_group_combo,
            g_target_layer_combo,
            g_target_list,
            g_select_all,
            g_clear_selection,
            g_invert_selection,
            g_target_group_combo,
            g_target_group_name,
            g_save_target_group,
            g_apply_target_group,
            g_delete_target_group,
            g_damping_enabled,
            g_linear_damping_slider,
            g_angular_damping_slider,
            g_gravity_enabled,
            g_gravity_x,
            g_gravity_y,
            g_gravity_z,
            g_gravity_acceleration_slider,
            g_set_key,
            g_delete_key,
            g_save_json,
            g_load_json,
        }};
        for (const HWND control : controls) set_control_font(control, g_panel_font);
        SetWindowTheme(g_source_mode_combo, L"", nullptr);
        SetWindowTheme(g_field_type_combo, L"", nullptr);
        SetWindowTheme(g_wind_preset_combo, L"", nullptr);
        SetWindowTheme(g_noise_type_combo, L"", nullptr);
        SetWindowTheme(g_falloff_type_combo, L"", nullptr);
        SetWindowTheme(g_direction_preset_combo, L"", nullptr);
        SetWindowTheme(g_group_combo, L"", nullptr);
        SetWindowTheme(g_target_layer_combo, L"", nullptr);
        SetWindowTheme(g_target_list, L"", nullptr);
        SetWindowTheme(g_target_group_combo, L"", nullptr);
        SendMessageW(g_group_combo, CB_SETDROPPEDWIDTH, 360, 0);
        load_track_file();
        populate_target_group_combo();
        sync_target_group_name_from_combo();
        layout_panel(window);
        sync_controls();
        SetTimer(window, kRefreshTimerId, kRefreshIntervalMs, nullptr);
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        const int notification = HIWORD(wparam);
        if ((id == kWindPageId || id == kPhysicsPageId || id == kTargetPageId) &&
            notification == BN_CLICKED) {
            g_panel_page = id == kWindPageId
                ? PanelPage::Wind
                : id == kPhysicsPageId ? PanelPage::Physics : PanelPage::Target;
            sync_page_visibility();
            InvalidateRect(g_wind_page, nullptr, FALSE);
            InvalidateRect(g_physics_page, nullptr, FALSE);
            InvalidateRect(g_target_page, nullptr, FALSE);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if ((id == kWindEnabledId || id == kDampingEnabledId ||
             id == kGravityEnabledId) && notification == BN_CLICKED) {
            {
                ExclusiveLock lock(g_wind_lock);
                if (id == kWindEnabledId) {
                    const bool enabled = !g_wind_master_enabled.load();
                    g_wind_master_enabled.store(enabled);
                    g_wind_settings.enabled = enabled;
                } else if (id == kDampingEnabledId) {
                    g_physics_settings.damping_enabled =
                        !g_physics_settings.damping_enabled;
                } else {
                    g_physics_settings.gravity_enabled =
                        !g_physics_settings.gravity_enabled;
                }
            }
            g_last_wind_qpc.store(0);
            sync_controls(false);
            InvalidateRect(reinterpret_cast<HWND>(lparam), nullptr, FALSE);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (id == kSourceModeComboId && notification == CBN_SELCHANGE &&
            !g_syncing_controls) {
            update_settings_from_controls(false);
            g_last_wind_qpc.store(0);
            sync_controls(false);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (id == kWindPresetComboId && notification == CBN_SELCHANGE &&
            !g_syncing_controls) {
            const int selection = ComboBox_GetCurSel(g_wind_preset_combo);
            apply_wind_preset(selection);
            sync_controls(false);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if ((id == kGroupComboId || id == kFieldTypeComboId ||
             id == kNoiseTypeComboId || id == kFalloffTypeComboId) &&
            notification == CBN_SELCHANGE) {
            if (id == kNoiseTypeComboId || id == kFalloffTypeComboId)
                g_wind_preset_selection = 0;
            update_settings_from_controls(id == kGroupComboId);
            sync_controls(false);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (id == kTargetLayerComboId && notification == CBN_SELCHANGE &&
            !g_syncing_controls) {
            update_settings_from_controls(true);
            const int selection = ComboBox_GetCurSel(g_target_layer_combo);
            if (selection >= 0 && selection <= 2) {
                g_target_layer = static_cast<TargetLayer>(selection);
            }
            sync_controls(false);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (id == kTargetListId && notification == LBN_SELCHANGE &&
            !g_syncing_controls) {
            const int caret = static_cast<int>(
                SendMessageW(g_target_list, LB_GETCARETINDEX, 0, 0));
            if (caret == 0 && ListBox_GetSel(g_target_list, 0) > 0) {
                ListBox_SetSel(g_target_list, FALSE, -1);
                ListBox_SetSel(g_target_list, TRUE, 0);
            } else {
                ListBox_SetSel(g_target_list, FALSE, 0);
            }
            update_settings_from_controls(true);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (id == kTargetGroupComboId && notification == CBN_SELCHANGE) {
            sync_target_group_name_from_combo();
            return 0;
        }
        if ((id == kSaveTargetGroupId || id == kApplyTargetGroupId ||
             id == kDeleteTargetGroupId) && notification == BN_CLICKED) {
            if (id == kSaveTargetGroupId) {
                save_target_group();
            } else if (id == kApplyTargetGroupId) {
                apply_target_group();
            } else {
                delete_target_group();
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if ((id == kSelectAllId || id == kClearSelectionId ||
             id == kInvertSelectionId) && notification == BN_CLICKED) {
            g_syncing_controls = true;
            if (id == kSelectAllId) {
                ListBox_SetSel(g_target_list, FALSE, -1);
                ListBox_SetSel(g_target_list, TRUE, 0);
            } else if (id == kClearSelectionId) {
                ListBox_SetSel(g_target_list, FALSE, -1);
            } else {
                ListBox_SetSel(g_target_list, FALSE, 0);
                const int count = ListBox_GetCount(g_target_list);
                for (int item = 1; item < count; ++item) {
                    ListBox_SetSel(
                        g_target_list,
                        ListBox_GetSel(g_target_list, item) <= 0,
                        item);
                }
            }
            g_syncing_controls = false;
            update_settings_from_controls(true);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (id == kDirectionPresetComboId && notification == CBN_SELCHANGE &&
            !g_syncing_controls) {
            const int selection = ComboBox_GetCurSel(g_direction_preset_combo);
            if (selection > 0) {
                ExclusiveLock lock(g_wind_lock);
                g_wind_settings.direction = direction_from_preset(selection);
            }
            sync_controls(false);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if ((id == kDirectionXId || id == kDirectionYId || id == kDirectionZId ||
             id == kCenterXId || id == kCenterYId || id == kCenterZId ||
             id == kGravityXId || id == kGravityYId || id == kGravityZId) &&
            notification == EN_CHANGE && !g_syncing_controls) {
            update_settings_from_controls(false);
            if (id == kDirectionXId || id == kDirectionYId || id == kDirectionZId) {
                ComboBox_SetCurSel(g_direction_preset_combo, 0);
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if ((id == kDirectionXId || id == kDirectionYId || id == kDirectionZId ||
             id == kCenterXId || id == kCenterYId || id == kCenterZId ||
             id == kGravityXId || id == kGravityYId || id == kGravityZId) &&
            notification == EN_KILLFOCUS) {
            sync_controls(false);
            return 0;
        }
        if ((id == kSetKeyId || id == kDeleteKeyId) && notification == BN_CLICKED) {
            update_settings_from_controls(false);
            {
                ExclusiveLock lock(g_wind_lock);
                if (id == kSetKeyId) {
                    g_physics_track.set_key(
                        g_current_frame.load(),
                        {g_wind_settings, g_physics_settings});
                    g_selected_key_frame = g_current_frame.load();
                } else {
                    const std::uint32_t frame = g_selected_key_frame != UINT32_MAX
                        ? g_selected_key_frame
                        : g_current_frame.load();
                    g_physics_track.erase_key(frame);
                    g_selected_key_frame = UINT32_MAX;
                }
            }
            save_track_file();
            InvalidateRect(g_set_key, nullptr, FALSE);
            InvalidateRect(g_delete_key, nullptr, FALSE);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if ((id == kSaveJsonId || id == kLoadJsonId) && notification == BN_CLICKED) {
            if (id == kSaveJsonId) {
                update_settings_from_controls(false);
                save_track_file();
            } else {
                load_track_file();
                populate_target_group_combo();
                sync_target_group_name_from_combo();
                sync_controls(false);
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    }
    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
        if (measure != nullptr &&
            (measure->CtlID == kGroupComboId || measure->CtlID == kFieldTypeComboId ||
             measure->CtlID == kDirectionPresetComboId ||
             measure->CtlID == kWindPresetComboId ||
             measure->CtlID == kNoiseTypeComboId ||
             measure->CtlID == kSourceModeComboId ||
             measure->CtlID == kFalloffTypeComboId ||
             measure->CtlID == kTargetLayerComboId ||
             measure->CtlID == kTargetGroupComboId ||
             measure->CtlID == kTargetListId)) {
            measure->itemHeight = measure->CtlID == kTargetListId ? 27 : 25;
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (item == nullptr) break;
        if (item->CtlID == kWindEnabledId || item->CtlID == kDampingEnabledId ||
            item->CtlID == kGravityEnabledId || item->CtlID == kWindPageId ||
            item->CtlID == kPhysicsPageId || item->CtlID == kTargetPageId ||
            item->CtlID == kSetKeyId || item->CtlID == kDeleteKeyId ||
            item->CtlID == kSelectAllId || item->CtlID == kClearSelectionId ||
            item->CtlID == kInvertSelectionId || item->CtlID == kSaveJsonId ||
            item->CtlID == kLoadJsonId || item->CtlID == kSaveTargetGroupId ||
            item->CtlID == kApplyTargetGroupId ||
            item->CtlID == kDeleteTargetGroupId) {
            paint_owner_button(*item);
            return TRUE;
        }
        if (item->CtlID == kGroupComboId || item->CtlID == kFieldTypeComboId ||
            item->CtlID == kDirectionPresetComboId ||
            item->CtlID == kWindPresetComboId || item->CtlID == kNoiseTypeComboId ||
            item->CtlID == kSourceModeComboId ||
            item->CtlID == kFalloffTypeComboId ||
            item->CtlID == kTargetLayerComboId ||
            item->CtlID == kTargetGroupComboId) {
            paint_group_item(*item);
            return TRUE;
        }
        if (item->CtlID == kTargetListId) {
            paint_target_item(*item);
            return TRUE;
        }
        break;
    }
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lparam) == g_strength_slider ||
            reinterpret_cast<HWND>(lparam) == g_gust_slider ||
            reinterpret_cast<HWND>(lparam) == g_turbulence_slider ||
            reinterpret_cast<HWND>(lparam) == g_frequency_slider ||
            reinterpret_cast<HWND>(lparam) == g_linear_damping_slider ||
            reinterpret_cast<HWND>(lparam) == g_angular_damping_slider ||
            reinterpret_cast<HWND>(lparam) == g_gravity_acceleration_slider) {
            if (reinterpret_cast<HWND>(lparam) == g_strength_slider ||
                reinterpret_cast<HWND>(lparam) == g_gust_slider ||
                reinterpret_cast<HWND>(lparam) == g_turbulence_slider ||
                reinterpret_cast<HWND>(lparam) == g_frequency_slider) {
                g_wind_preset_selection = 0;
                ComboBox_SetCurSel(g_wind_preset_combo, 0);
            }
            if (reinterpret_cast<HWND>(lparam) == g_strength_slider) {
                g_wind_master_strength.store(static_cast<float>(
                    SendMessageW(g_strength_slider, TBM_GETPOS, 0, 0)));
            }
            update_settings_from_controls(false);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_TIMER:
        if (wparam == kRefreshTimerId && IsWindowVisible(window)) {
            const bool targets_changed = populate_target_combo();
            const std::uint32_t frame = g_current_frame.load();
            const int controller_status = g_controller_status.load();
            const int controller_count = static_cast<int>(g_controller_count.load());
            if (targets_changed || g_last_synced_frame != frame ||
                g_last_controller_ui_status != controller_status ||
                g_last_controller_ui_count != controller_count) {
                sync_controls();
                RECT client{};
                GetClientRect(window, &client);
                client.top = kPanelHeaderHeight;
                InvalidateRect(window, &client, FALSE);
            }
        }
        return 0;
    case WM_SIZE:
        layout_panel(window);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize.x = kPanelMinimumWidth;
        limits->ptMinTrackSize.y = kPanelMinimumHeight;
        return 0;
    }
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(window, &point);
        RECT client{};
        GetClientRect(window, &client);
        const RECT close = close_button_rect(window);
        if (PtInRect(&close, point)) return HTCLIENT;
        if (point.x < kPanelResizeBorder) return HTLEFT;
        if (point.x >= client.right - kPanelResizeBorder) return HTRIGHT;
        if (point.y < kPanelResizeBorder) return HTTOP;
        if (point.y >= client.bottom - kPanelResizeBorder) return HTBOTTOM;
        if (point.y < kPanelHeaderHeight) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_MOUSEMOVE: {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        const RECT close = close_button_rect(window);
        const bool hot = PtInRect(&close, point) != FALSE;
        if (hot != g_close_button_hot) {
            g_close_button_hot = hot;
            InvalidateRect(window, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0};
        TrackMouseEvent(&tracking);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_close_button_hot) {
            g_close_button_hot = false;
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP: {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        const RECT close = close_button_rect(window);
        if (PtInRect(&close, point)) {
            ShowWindow(window, SW_HIDE);
        } else {
            select_timeline_key(window, point);
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT: {
        const HDC control_dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(control_dc, kTextColor);
        SetBkColor(control_dc, kContentColor);
        return reinterpret_cast<LRESULT>(g_content_brush);
    }
    case WM_CTLCOLORLISTBOX: {
        const HDC control_dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(control_dc, kTextColor);
        SetBkColor(control_dc, kContentColor);
        return reinterpret_cast<LRESULT>(g_content_brush);
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint_panel(window);
        return 0;
    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;
    case WM_NCDESTROY:
        KillTimer(window, kRefreshTimerId);
        save_track_file();
        g_panel.store(nullptr);
        g_close_button_hot = false;
        g_wind_page = nullptr;
        g_physics_page = nullptr;
        g_target_page = nullptr;
        g_wind_enabled = nullptr;
        g_source_mode_combo = nullptr;
        g_wind_preset_combo = nullptr;
        g_field_type_combo = nullptr;
        g_noise_type_combo = nullptr;
        g_strength_slider = nullptr;
        g_gust_slider = nullptr;
        g_turbulence_slider = nullptr;
        g_frequency_slider = nullptr;
        g_direction_preset_combo = nullptr;
        g_direction_x = nullptr;
        g_direction_y = nullptr;
        g_direction_z = nullptr;
        g_center_x = nullptr;
        g_center_y = nullptr;
        g_center_z = nullptr;
        g_radius_slider = nullptr;
        g_core_ratio_slider = nullptr;
        g_falloff_type_combo = nullptr;
        g_group_combo = nullptr;
        g_target_layer_combo = nullptr;
        g_target_list = nullptr;
        g_select_all = nullptr;
        g_clear_selection = nullptr;
        g_invert_selection = nullptr;
        g_target_group_combo = nullptr;
        g_target_group_name = nullptr;
        g_save_target_group = nullptr;
        g_apply_target_group = nullptr;
        g_delete_target_group = nullptr;
        g_damping_enabled = nullptr;
        g_linear_damping_slider = nullptr;
        g_angular_damping_slider = nullptr;
        g_gravity_enabled = nullptr;
        g_gravity_x = nullptr;
        g_gravity_y = nullptr;
        g_gravity_z = nullptr;
        g_gravity_acceleration_slider = nullptr;
        g_set_key = nullptr;
        g_delete_key = nullptr;
        g_save_json = nullptr;
        g_load_json = nullptr;
        g_target_selection_anchor = -1;
        g_target_body_indices.clear();
        g_target_cache_model = 0;
        g_target_cache_count = UINT32_MAX;
        g_last_synced_frame = UINT32_MAX;
        g_last_controller_ui_status = -1;
        g_last_controller_ui_count = -1;
        destroy_panel_resources();
        break;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

bool register_panel_class() {
    WNDCLASSEXW existing{};
    existing.cbSize = sizeof(existing);
    if (GetClassInfoExW(g_module, kPanelClassName, &existing)) return true;
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = panel_window_proc;
    window_class.hInstance = g_module;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = kPanelClassName;
    return RegisterClassExW(&window_class) != 0;
}

HWND ensure_panel() {
    HWND panel = g_panel.load();
    if (panel != nullptr && IsWindow(panel)) return panel;
    if (!register_panel_class()) return nullptr;
    const HWND host = g_host.load();
    RECT host_client{};
    GetClientRect(host, &host_client);
    POINT host_origin{0, 0};
    ClientToScreen(host, &host_origin);
    const int client_width = host_client.right - host_client.left;
    const int panel_x = host_origin.x + std::max(
        20,
        client_width - kPanelDefaultWidth - 300);
    const int panel_y = host_origin.y + 48;
    panel = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kPanelClassName,
        L"WindTool",
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        panel_x,
        panel_y,
        kPanelDefaultWidth,
        kPanelDefaultHeight,
        host,
        nullptr,
        g_module,
        nullptr);
    if (panel != nullptr) g_panel.store(panel);
    return panel;
}

bool handle_command(UINT command) {
    if (command != pcs_host::kCommandTogglePanel &&
        command != pcs_host::kCommandRefreshPanel) {
        return false;
    }
    const HWND panel = ensure_panel();
    if (panel == nullptr) return false;
    if (command == pcs_host::kCommandTogglePanel && IsWindowVisible(panel)) {
        ShowWindow(panel, SW_HIDE);
        return true;
    }
    if (command == pcs_host::kCommandRefreshPanel) refresh_snapshot();
    layout_panel(panel);
    sync_controls();
    ShowWindow(panel, SW_SHOWNOACTIVATE);
    RedrawWindow(
        panel,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    return true;
}

LRESULT CALLBACK host_subclass_proc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    UINT_PTR,
    DWORD_PTR) {
    if (message == WM_COMMAND &&
        handle_command(static_cast<UINT>(LOWORD(wparam)))) {
        return 0;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, host_subclass_proc, kHostSubclassId);
        const HWND panel = g_panel.load();
        if (panel != nullptr && IsWindow(panel)) DestroyWindow(panel);
        g_host.store(nullptr);
        g_installed.store(false);
        reset_frame_bridge();
        g_host_menu = nullptr;
        g_extension_menu = nullptr;
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

bool append_menu(HWND host) {
    g_host_menu = GetMenu(host);
    g_created_host_menu = false;
    if (g_host_menu == nullptr) {
        g_host_menu = CreateMenu();
        if (g_host_menu == nullptr || SetMenu(host, g_host_menu) == FALSE) return false;
        g_created_host_menu = true;
    }
    g_extension_menu = CreatePopupMenu();
    if (g_extension_menu == nullptr ||
        !AppendMenuW(
            g_extension_menu,
            MF_STRING,
            pcs_host::kCommandTogglePanel,
            L"打开 WindTool") ||
        !AppendMenuW(
            g_host_menu,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(g_extension_menu),
            L"WindTool")) {
        if (g_extension_menu != nullptr) DestroyMenu(g_extension_menu);
        g_extension_menu = nullptr;
        return false;
    }
    DrawMenuBar(host);
    return true;
}

void remove_menu(HWND host) {
    if (g_host_menu != nullptr && g_extension_menu != nullptr) {
        const int count = GetMenuItemCount(g_host_menu);
        for (int index = 0; index < count; ++index) {
            if (GetSubMenu(g_host_menu, index) == g_extension_menu) {
                RemoveMenu(g_host_menu, static_cast<UINT>(index), MF_BYPOSITION);
                break;
            }
        }
        DestroyMenu(g_extension_menu);
        if (g_created_host_menu) {
            SetMenu(host, nullptr);
            DestroyMenu(g_host_menu);
        }
        DrawMenuBar(host);
    }
    g_host_menu = nullptr;
    g_extension_menu = nullptr;
    g_created_host_menu = false;
}

bool install_on_ui_thread(HWND host) {
    if (!IsWindow(host)) return false;
    if (g_installed.load()) return g_host.load() == host;
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = static_cast<DWORD>(sizeof(controls));
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&controls);
    if (!SetWindowSubclass(host, host_subclass_proc, kHostSubclassId, 0)) return false;
    if (!append_menu(host)) {
        RemoveWindowSubclass(host, host_subclass_proc, kHostSubclassId);
        return false;
    }
    reset_frame_bridge();
    g_ui_thread_id.store(GetCurrentThreadId());
    g_host.store(host);
    g_installed.store(true);
    return true;
}

bool uninstall_on_ui_thread(HWND host) {
    if (!g_installed.load()) return true;
    const bool damping_restored = restore_tracked_damping_overrides();
    const HWND panel = g_panel.load();
    if (panel != nullptr && IsWindow(panel)) DestroyWindow(panel);
    if (host != nullptr && IsWindow(host)) {
        RemoveWindowSubclass(host, host_subclass_proc, kHostSubclassId);
        remove_menu(host);
    }
    g_host.store(nullptr);
    g_installed.store(false);
    reset_frame_bridge();
    g_snapshot_text.clear();
    return damping_restored;
}

LRESULT CALLBACK request_hook(int code, WPARAM wparam, LPARAM lparam) {
    if (code >= 0 && g_request_message != 0) {
        const auto* message = reinterpret_cast<const CWPSTRUCT*>(lparam);
        const HWND pending = g_pending_window.load();
        if (message != nullptr && message->hwnd == pending &&
            message->message == g_request_message) {
            const PendingOperation operation = g_pending_operation.load();
            const bool result = operation == PendingOperation::Install
                ? install_on_ui_thread(pending)
                : uninstall_on_ui_thread(pending);
            g_pending_result.store(result);
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

bool run_on_window_thread(PendingOperation operation, HWND host) {
    if (host == nullptr || !IsWindow(host)) return false;
    const DWORD thread = GetWindowThreadProcessId(host, nullptr);
    if (thread == 0) return false;
    if (thread == GetCurrentThreadId()) {
        return operation == PendingOperation::Install
            ? install_on_ui_thread(host)
            : uninstall_on_ui_thread(host);
    }
    if (g_request_message == 0) {
        g_request_message = RegisterWindowMessageW(kRequestMessageName);
        if (g_request_message == 0) return false;
    }
    g_pending_operation.store(operation);
    g_pending_window.store(host);
    g_pending_result.store(false);
    const HHOOK hook = SetWindowsHookExW(WH_CALLWNDPROC, request_hook, g_module, thread);
    if (hook == nullptr) return false;
    DWORD_PTR ignored = 0;
    const LRESULT delivered = SendMessageTimeoutW(
        host,
        g_request_message,
        0,
        0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK,
        5000,
        &ignored);
    UnhookWindowsHookEx(hook);
    const bool result = delivered != 0 && g_pending_result.load();
    g_pending_operation.store(PendingOperation::None);
    g_pending_window.store(nullptr);
    return result;
}

struct WindowSearch {
    DWORD process_id = 0;
    HWND result = nullptr;
};

BOOL CALLBACK find_main_window(HWND window, LPARAM parameter) {
    auto* search = reinterpret_cast<WindowSearch*>(parameter);
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == search->process_id && GetWindow(window, GW_OWNER) == nullptr &&
        IsWindowVisible(window)) {
        search->result = window;
        return FALSE;
    }
    return TRUE;
}

HWND resolve_host_window() {
    mmd931::mmhack::Exports hack;
    if (hack.resolve() && hack.GetMMDMainWindow != nullptr) {
        const HWND window = hack.GetMMDMainWindow();
        if (window != nullptr && IsWindow(window)) return window;
    }

    mmd931::runtime_access::ProcessReader reader(GetCurrentProcess());
    const std::uintptr_t base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::uintptr_t state = 0;
    if (reader.read(
            base + mmd931::runtime::kMainStatePointerRva,
            &state,
            sizeof(state)) &&
        state != 0) {
        std::uintptr_t window = 0;
        if (reader.read(
                state + mmd931::runtime_access::main_state::kMainWindow,
                &window,
                sizeof(window)) &&
            IsWindow(reinterpret_cast<HWND>(window))) {
            return reinterpret_cast<HWND>(window);
        }
    }

    WindowSearch search{GetCurrentProcessId(), nullptr};
    EnumWindows(find_main_window, reinterpret_cast<LPARAM>(&search));
    return search.result;
}

std::wstring status_json() {
    const HostStatus status = g_host_status.load();
    const bool installed = g_installed.load();
    const HWND panel = g_panel.load();
    const auto controls = control_snapshot();
    const auto& wind = controls.wind;
    const auto& physics = controls.physics;
    const auto& active_target = target_for_layer(physics, g_target_layer);
    const WindSourceMode source_mode = g_wind_source_mode.load();
    const int controller_status = g_controller_status.load();
    const std::uint32_t controller_count = g_controller_count.load();
    const char* controller_state = source_mode != WindSourceMode::PmxLocal
        ? "inactive"
        : controller_status == 2 ? "connected"
        : controller_status == 1 ? "missing" : "idle";
    char narrow[1280]{};
    std::snprintf(
        narrow,
        sizeof(narrow),
        "{\"component\":\"WindTool\",\"api_version\":65537,"
        "\"installed\":%s,\"host_status\":\"%s\","
        "\"panel_visible\":%s,\"write_backend\":\"bullet_force_accumulator\"," 
        "\"activation_backend\":\"bullet_activate\"," 
        "\"mode\":\"animated_physics_control\",\"wind_enabled\":%s,"
        "\"wind_strength\":%.2f,\"wind_source\":\"%s\","
        "\"controller_status\":\"%s\",\"controller_count\":%u,"
        "\"controller_limit\":%zu,"
        "\"damping_enabled\":%s,\"linear_damping\":%.3f,"
        "\"angular_damping\":%.3f,\"gravity_enabled\":%s,"
        "\"gravity_acceleration\":%.3f,\"target_layer\":\"%s\"," 
        "\"target_kind\":\"%s\",\"target_index\":%u,"
        "\"wind_target_index\":%u,\"damping_target_index\":%u,"
        "\"gravity_target_index\":%u,\"current_frame\":%u,\"keyframes\":%zu,"
        "\"wind_backend\":\"%s\",\"wind_applied_frames\":%llu,"
        "\"wind_applied_bodies\":%llu,\"frame_bridge\":\"%s\","
        "\"ui_thread_id\":%lu,\"frame_thread_id\":%lu,"
        "\"begin_scene_count\":%llu,\"end_scene_count\":%llu}",
        installed ? "true" : "false",
        status_name(status),
        panel != nullptr && IsWindowVisible(panel) ? "true" : "false",
        wind_effect_active(wind) ? "true" : "false",
        static_cast<double>(displayed_wind_strength(wind)),
        source_mode == WindSourceMode::PmxLocal ? "pmx_local" : "global",
        controller_state,
        static_cast<unsigned>(controller_count),
        kMaximumWindControllers,
        physics.damping_enabled ? "true" : "false",
        static_cast<double>(physics.linear_damping),
        static_cast<double>(physics.angular_damping),
        physics.gravity_enabled ? "true" : "false",
        static_cast<double>(physics.gravity_acceleration),
        target_layer_name(g_target_layer),
        target_kind_name(active_target),
        static_cast<unsigned>(active_target.index),
        static_cast<unsigned>(physics.wind_target.index),
        static_cast<unsigned>(physics.damping_target.index),
        static_cast<unsigned>(physics.gravity_target.index),
        static_cast<unsigned>(g_current_frame.load()),
        control_key_count(),
        wind_backend_status(),
        static_cast<unsigned long long>(g_wind_applied_frames.load()),
        static_cast<unsigned long long>(g_wind_applied_bodies.load()),
        frame_bridge_status(),
        static_cast<unsigned long>(g_ui_thread_id.load()),
        static_cast<unsigned long>(g_frame_thread_id.load()),
        static_cast<unsigned long long>(g_begin_scene_count.load()),
        static_cast<unsigned long long>(g_end_scene_count.load()));
    std::wstring result;
    for (const unsigned char character : std::string(narrow))
        result.push_back(static_cast<wchar_t>(character));
    return result;
}

}  // namespace

extern "C" __declspec(dllexport) std::uint32_t MmdPhysicsGetApiVersion() {
    return pcs_host::kApiVersion;
}

extern "C" __declspec(dllexport) BOOL MmdPhysicsInstallForWindow(HWND requested) {
    ExclusiveLock lock(g_operation_lock);
    const HostStatus status = validate_host();
    g_host_status.store(status);
    if (status != HostStatus::Supported) return FALSE;
    const HWND host = requested != nullptr && IsWindow(requested)
        ? requested
        : resolve_host_window();
    if (host == nullptr) {
        g_host_status.store(HostStatus::MainWindowUnavailable);
        return FALSE;
    }
    return run_on_window_thread(PendingOperation::Install, host) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL MmdPhysicsInstall() {
    return MmdPhysicsInstallForWindow(nullptr);
}

extern "C" __declspec(dllexport) BOOL MmdPhysicsUninstall() {
    ExclusiveLock lock(g_operation_lock);
    if (!g_installed.load()) return TRUE;
    return run_on_window_thread(PendingOperation::Uninstall, g_host.load()) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL MmdPhysicsIsInstalled() {
    return g_installed.load() ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) DWORD MmdPhysicsGetStatusJsonW(
    wchar_t* output,
    DWORD capacity) {
    const std::wstring json = status_json();
    const DWORD required = static_cast<DWORD>(json.size() + 1);
    if (output == nullptr || capacity == 0) return required;
    if (capacity < required) {
        output[0] = L'\0';
        return required;
    }
    std::wmemcpy(output, json.c_str(), required);
    return required;
}

extern "C" __declspec(dllexport) HWND MmdPhysicsGetPanelWindow() {
    return g_panel.load();
}

extern "C" __declspec(dllexport) BOOL MmdPhysicsDispatchCommand(UINT command) {
    if (!g_installed.load() ||
        (command != pcs_host::kCommandTogglePanel &&
         command != pcs_host::kCommandRefreshPanel)) {
        return FALSE;
    }
    const HWND host = g_host.load();
    return host != nullptr && PostMessageW(host, WM_COMMAND, command, 0) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL MmdPhysicsOnFrameBoundary(
    pcs_host::FrameBoundary boundary) {
    if (!g_installed.load()) return FALSE;
    if (boundary != pcs_host::FrameBoundary::BeginScene &&
        boundary != pcs_host::FrameBoundary::EndScene) {
        return FALSE;
    }

    const DWORD current_thread = GetCurrentThreadId();
    const DWORD ui_thread = g_ui_thread_id.load();
    if (ui_thread == 0 || current_thread != ui_thread) {
        g_frame_thread_mismatch.store(true);
        return FALSE;
    }

    DWORD expected_thread = 0;
    if (!g_frame_thread_id.compare_exchange_strong(expected_thread, current_thread) &&
        expected_thread != current_thread) {
        g_frame_thread_mismatch.store(true);
        return FALSE;
    }

    if (boundary == pcs_host::FrameBoundary::BeginScene) {
        g_begin_scene_count.fetch_add(1, std::memory_order_relaxed);
        if (!apply_physics_frame()) return FALSE;
    } else {
        g_end_scene_count.fetch_add(1, std::memory_order_relaxed);
    }
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    if (reason == DLL_PROCESS_DETACH) {
        g_installed.store(false);
        reset_frame_bridge();
        g_module = nullptr;
    }
    return TRUE;
}
