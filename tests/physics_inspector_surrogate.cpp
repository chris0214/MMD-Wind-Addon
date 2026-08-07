#include "physics_control_studio/host_api.hpp"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace pcs_host = physics_control_studio::host_api;

namespace {

constexpr int kWindEnabledId = 2001;
constexpr int kStrengthSliderId = 2002;
constexpr int kDirectionYId = 2005;
constexpr int kGroupComboId = 2012;
constexpr int kFieldTypeComboId = 2013;
constexpr int kDirectionPresetComboId = 2014;
constexpr int kFrequencySliderId = 2015;
constexpr int kWindPageId = 2016;
constexpr int kPhysicsPageId = 2017;
constexpr int kDampingEnabledId = 2018;
constexpr int kLinearDampingSliderId = 2019;
constexpr int kAngularDampingSliderId = 2020;
constexpr int kGravityEnabledId = 2021;
constexpr int kGravityAccelerationSliderId = 2025;
constexpr int kSetKeyId = 2026;
constexpr int kDeleteKeyId = 2027;
constexpr int kTargetPageId = 2028;
constexpr int kWindPresetComboId = 2029;
constexpr int kNoiseTypeComboId = 2030;
constexpr int kTargetListId = 2031;
constexpr int kSelectAllId = 2032;
constexpr int kClearSelectionId = 2033;
constexpr int kInvertSelectionId = 2034;
constexpr int kSaveJsonId = 2035;
constexpr int kLoadJsonId = 2036;
constexpr int kTurbulenceSliderId = 2037;
constexpr int kTargetGroupComboId = 2038;
constexpr int kTargetGroupNameId = 2039;
constexpr int kSaveTargetGroupId = 2040;
constexpr int kApplyTargetGroupId = 2041;
constexpr int kDeleteTargetGroupId = 2042;

int g_cases = 0;
int g_failures = 0;

void check(bool condition, const char* name) {
    ++g_cases;
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL " << name << '\n';
    }
}

template <typename Function>
Function resolve(HMODULE module, const char* name) {
    static_assert(sizeof(Function) == sizeof(FARPROC));
    FARPROC raw = GetProcAddress(module, name);
    Function result = nullptr;
    std::memcpy(&result, &raw, sizeof(result));
    return result;
}

LRESULT CALLBACK host_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(window, message, wparam, lparam);
}

void pump_messages() {
    MSG message{};
    for (int iteration = 0; iteration < 50; ++iteration) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(2);
    }
}

std::wstring read_status(pcs_host::GetStatusJsonFn get_status) {
    const DWORD required = get_status(nullptr, 0);
    std::vector<wchar_t> buffer(required);
    if (required <= 1 || get_status(buffer.data(), required) != required) return {};
    return std::wstring(buffer.data());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 1;

    std::string track_path = argv[1];
    const std::size_t separator = track_path.find_last_of("\\/");
    track_path = (separator == std::string::npos ? std::string{} : track_path.substr(0, separator + 1)) +
        "PhysicsControlStudio.json";
    DeleteFileA(track_path.c_str());

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = host_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"PhysicsInspectorSurrogateHost";
    check(RegisterClassExW(&window_class) != 0, "register surrogate host class");

    const HWND host = CreateWindowExW(
        0,
        window_class.lpszClassName,
        L"Physics Inspector Surrogate",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100,
        100,
        640,
        480,
        nullptr,
        CreateMenu(),
        instance,
        nullptr);
    check(host != nullptr, "create visible surrogate host");
    pump_messages();

    HMODULE module = LoadLibraryA(argv[1]);
    check(module != nullptr, "load surrogate physics DLL");
    if (module == nullptr) return 1;

    const auto install = resolve<pcs_host::InstallForWindowFn>(
        module, "MmdPhysicsInstallForWindow");
    const auto uninstall = resolve<pcs_host::UninstallFn>(module, "MmdPhysicsUninstall");
    const auto is_installed = resolve<pcs_host::IsInstalledFn>(
        module, "MmdPhysicsIsInstalled");
    const auto get_panel = resolve<pcs_host::GetPanelWindowFn>(
        module, "MmdPhysicsGetPanelWindow");
    const auto dispatch = resolve<pcs_host::DispatchCommandFn>(
        module, "MmdPhysicsDispatchCommand");
    const auto get_status = resolve<pcs_host::GetStatusJsonFn>(
        module, "MmdPhysicsGetStatusJsonW");
    const auto frame_boundary = resolve<pcs_host::FrameBoundaryFn>(
        module, "MmdPhysicsOnFrameBoundary");
    check(
        install != nullptr && uninstall != nullptr && is_installed != nullptr &&
            get_panel != nullptr && dispatch != nullptr && get_status != nullptr &&
            frame_boundary != nullptr,
        "resolve physics control exports");

    check(
        frame_boundary(pcs_host::FrameBoundary::BeginScene) == FALSE,
        "reject frame callback before install");
    check(install(host) == TRUE && is_installed() == TRUE, "install physics controller");
    check(GetMenuItemCount(GetMenu(host)) == 1, "append WindTool menu");
    wchar_t menu_label[64]{};
    GetMenuStringW(GetMenu(host), 0, menu_label, std::size(menu_label), MF_BYPOSITION);
    check(std::wstring(menu_label) == L"WindTool", "brand MMD menu as WindTool");
    check(
        frame_boundary(static_cast<pcs_host::FrameBoundary>(99)) == FALSE,
        "reject unknown frame boundary");

    std::atomic_bool wrong_thread_result{true};
    std::thread wrong_thread([&] {
        wrong_thread_result.store(
            frame_boundary(pcs_host::FrameBoundary::BeginScene) == TRUE);
    });
    wrong_thread.join();
    check(!wrong_thread_result.load(), "reject non-UI frame callback thread");
    check(
        read_status(get_status).find(L"frame_bridge\":\"thread_mismatch") !=
            std::wstring::npos,
        "report frame callback thread mismatch");

    check(
        uninstall() == TRUE && install(host) == TRUE && is_installed() == TRUE,
        "reset frame bridge state on reinstall");
    check(
        frame_boundary(pcs_host::FrameBoundary::BeginScene) == TRUE,
        "accept begin-scene frame callback");
    check(
        frame_boundary(pcs_host::FrameBoundary::EndScene) == TRUE,
        "accept end-scene frame callback");
    const std::wstring online_status = read_status(get_status);
    check(
        online_status.find(L"frame_bridge\":\"online") != std::wstring::npos &&
            online_status.find(L"begin_scene_count\":1") != std::wstring::npos &&
            online_status.find(L"end_scene_count\":1") != std::wstring::npos,
        "report online frame bridge counters");
    check(dispatch(pcs_host::kCommandTogglePanel) == TRUE, "dispatch panel toggle");
    pump_messages();

    const HWND panel = get_panel();
    check(panel != nullptr && IsWindowVisible(panel), "show wind control panel");
    const LONG_PTR panel_style = GetWindowLongPtrW(panel, GWL_STYLE);
    check(
        (panel_style & WS_POPUP) != 0 && (panel_style & WS_CAPTION) == 0 &&
            (panel_style & WS_CLIPCHILDREN) != 0 &&
            (panel_style & WS_CLIPSIBLINGS) != 0 && GetWindow(panel, GW_OWNER) == host,
        "create owned viewport overlay");
    wchar_t title[128]{};
    GetWindowTextW(panel, title, static_cast<int>(std::size(title)));
    check(
        std::wstring(title) == L"WindTool",
        "brand viewport panel as WindTool");

    const HWND wind_enabled = GetDlgItem(panel, kWindEnabledId);
    const HWND strength = GetDlgItem(panel, kStrengthSliderId);
    const HWND group = GetDlgItem(panel, kGroupComboId);
    const HWND field_type = GetDlgItem(panel, kFieldTypeComboId);
    const HWND direction_preset = GetDlgItem(panel, kDirectionPresetComboId);
    const HWND frequency = GetDlgItem(panel, kFrequencySliderId);
    const HWND wind_page = GetDlgItem(panel, kWindPageId);
    const HWND physics_page = GetDlgItem(panel, kPhysicsPageId);
    const HWND damping_enabled = GetDlgItem(panel, kDampingEnabledId);
    const HWND linear_damping = GetDlgItem(panel, kLinearDampingSliderId);
    const HWND angular_damping = GetDlgItem(panel, kAngularDampingSliderId);
    const HWND gravity_enabled = GetDlgItem(panel, kGravityEnabledId);
    const HWND gravity_acceleration = GetDlgItem(panel, kGravityAccelerationSliderId);
    const HWND set_key = GetDlgItem(panel, kSetKeyId);
    const HWND delete_key = GetDlgItem(panel, kDeleteKeyId);
    const HWND target_page = GetDlgItem(panel, kTargetPageId);
    const HWND wind_preset = GetDlgItem(panel, kWindPresetComboId);
    const HWND noise_type = GetDlgItem(panel, kNoiseTypeComboId);
    const HWND target_list = GetDlgItem(panel, kTargetListId);
    const HWND select_all = GetDlgItem(panel, kSelectAllId);
    const HWND clear_selection = GetDlgItem(panel, kClearSelectionId);
    const HWND invert_selection = GetDlgItem(panel, kInvertSelectionId);
    const HWND save_json = GetDlgItem(panel, kSaveJsonId);
    const HWND load_json = GetDlgItem(panel, kLoadJsonId);
    const HWND turbulence = GetDlgItem(panel, kTurbulenceSliderId);
    const HWND target_group_combo = GetDlgItem(panel, kTargetGroupComboId);
    const HWND target_group_name = GetDlgItem(panel, kTargetGroupNameId);
    const HWND save_target_group = GetDlgItem(panel, kSaveTargetGroupId);
    const HWND apply_target_group = GetDlgItem(panel, kApplyTargetGroupId);
    const HWND delete_target_group = GetDlgItem(panel, kDeleteTargetGroupId);
    check(
        wind_enabled != nullptr && strength != nullptr && group != nullptr && field_type != nullptr &&
            direction_preset != nullptr && frequency != nullptr && wind_page != nullptr &&
            physics_page != nullptr && damping_enabled != nullptr &&
            linear_damping != nullptr && angular_damping != nullptr &&
            gravity_enabled != nullptr && gravity_acceleration != nullptr &&
            set_key != nullptr && delete_key != nullptr && target_page != nullptr &&
            wind_preset != nullptr && noise_type != nullptr && target_list != nullptr &&
            select_all != nullptr && clear_selection != nullptr &&
            invert_selection != nullptr && save_json != nullptr &&
            load_json != nullptr && turbulence != nullptr &&
            target_group_combo != nullptr && target_group_name != nullptr &&
            save_target_group != nullptr && apply_target_group != nullptr &&
            delete_target_group != nullptr,
        "create focused animated physics controls");
    RECT wind_page_bounds{};
    RECT wind_toggle_bounds{};
    RECT strength_bounds{};
    GetWindowRect(wind_page, &wind_page_bounds);
    GetWindowRect(wind_enabled, &wind_toggle_bounds);
    GetWindowRect(strength, &strength_bounds);
    check(
        wind_page_bounds.top < wind_toggle_bounds.top &&
            wind_toggle_bounds.top < strength_bounds.top &&
            wind_page_bounds.right > wind_page_bounds.left &&
            strength_bounds.right > strength_bounds.left,
        "complete first-show layout before presenting the panel");
    check(
        SendMessageW(strength, TBM_GETPOS, 0, 0) == 30 &&
            SendMessageW(turbulence, TBM_GETPOS, 0, 0) == 12 &&
            SendMessageW(frequency, TBM_GETPOS, 0, 0) == 65 &&
            ComboBox_GetCount(group) == 19 && ComboBox_GetCount(field_type) == 8 &&
            ComboBox_GetCount(direction_preset) == 7 &&
            ComboBox_GetCount(wind_preset) == 7 && ComboBox_GetCount(noise_type) == 5 &&
            ListBox_GetCount(target_list) == 19,
        "initialize professional wind modes, noise and body targets");
    check(
        SendMessageW(linear_damping, TBM_GETPOS, 0, 0) == 5 &&
            SendMessageW(angular_damping, TBM_GETPOS, 0, 0) == 5 &&
            SendMessageW(gravity_acceleration, TBM_GETPOS, 0, 0) == 98,
        "initialize damping and gravity defaults");
    ValidateRect(panel, nullptr);
    SendMessageW(panel, WM_TIMER, 1, 0);
    check(
        GetUpdateRect(panel, nullptr, FALSE) == FALSE,
        "idle refresh timer does not invalidate the whole panel");

    SendMessageW(physics_page, BM_CLICK, 0, 0);
    pump_messages();
    check(
        IsWindowVisible(damping_enabled) && IsWindowVisible(gravity_enabled) &&
            !IsWindowVisible(wind_enabled),
        "switch to body physics page");
    SendMessageW(wind_page, BM_CLICK, 0, 0);
    pump_messages();
    check(IsWindowVisible(wind_enabled), "return to wind page");

    ComboBox_SetCurSel(wind_preset, 6);
    SendMessageW(
        panel,
        WM_COMMAND,
        MAKEWPARAM(kWindPresetComboId, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(wind_preset));
    check(
        SendMessageW(strength, TBM_GETPOS, 0, 0) == 230 &&
            SendMessageW(turbulence, TBM_GETPOS, 0, 0) == 52 &&
            ComboBox_GetCurSel(noise_type) == 2,
        "apply localized hurricane wind preset");

    SendMessageW(target_page, BM_CLICK, 0, 0);
    pump_messages();
    check(
        IsWindowVisible(target_list) && IsWindowVisible(select_all) &&
            !IsWindowVisible(wind_enabled),
        "show visual batch target page");
    SendMessageW(clear_selection, BM_CLICK, 0, 0);
    SendMessageW(invert_selection, BM_CLICK, 0, 0);
    check(
        SendMessageW(target_list, LB_GETSELCOUNT, 0, 0) == 18,
        "batch target invert selects groups and named bodies");
    SendMessageW(clear_selection, BM_CLICK, 0, 0);
    RECT first_body{};
    RECT second_body{};
    SendMessageW(target_list, LB_GETITEMRECT, 17, reinterpret_cast<LPARAM>(&first_body));
    SendMessageW(target_list, LB_GETITEMRECT, 18, reinterpret_cast<LPARAM>(&second_body));
    const LPARAM first_point = MAKELPARAM(10, (first_body.top + first_body.bottom) / 2);
    const LPARAM second_point = MAKELPARAM(10, (second_body.top + second_body.bottom) / 2);
    SendMessageW(target_list, WM_LBUTTONDOWN, MK_LBUTTON, first_point);
    SendMessageW(target_list, WM_LBUTTONUP, 0, first_point);
    SendMessageW(target_list, WM_LBUTTONDOWN, MK_LBUTTON | MK_SHIFT, second_point);
    SendMessageW(target_list, WM_LBUTTONUP, 0, second_point);
    check(
        SendMessageW(target_list, LB_GETSELCOUNT, 0, 0) == 2,
        "shift click selects a continuous rigid-body range");
    SetWindowTextW(target_group_name, L"头发与裙摆");
    SendMessageW(save_target_group, BM_CLICK, 0, 0);
    SendMessageW(clear_selection, BM_CLICK, 0, 0);
    SendMessageW(apply_target_group, BM_CLICK, 0, 0);
    check(
        ComboBox_GetCount(target_group_combo) == 1 &&
            SendMessageW(target_list, LB_GETSELCOUNT, 0, 0) == 2,
        "save and reapply a named rigid-body group");
    SendMessageW(delete_target_group, BM_CLICK, 0, 0);
    check(
        ComboBox_GetCount(target_group_combo) == 0,
        "delete a saved rigid-body group");
    SendMessageW(wind_page, BM_CLICK, 0, 0);

    ComboBox_SetCurSel(direction_preset, 3);
    SendMessageW(
        panel,
        WM_COMMAND,
        MAKEWPARAM(kDirectionPresetComboId, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(direction_preset));
    wchar_t direction_y[32]{};
    GetWindowTextW(GetDlgItem(panel, kDirectionYId), direction_y, std::size(direction_y));
    check(std::wstring(direction_y) == L"1.00", "apply direction axis preset immediately");

    SendMessageW(wind_enabled, BM_CLICK, 0, 0);
    pump_messages();
    check(
        frame_boundary(pcs_host::FrameBoundary::BeginScene) == TRUE,
        "apply enabled wind on begin scene");
    const std::wstring wind_status = read_status(get_status);
    check(
        wind_status.find(L"write_backend\":\"bullet_velocity") != std::wstring::npos &&
            wind_status.find(L"mode\":\"animated_physics_control") != std::wstring::npos &&
            wind_status.find(L"wind_enabled\":true") != std::wstring::npos &&
            wind_status.find(L"wind_backend\":\"active") != std::wstring::npos &&
            wind_status.find(L"wind_applied_bodies\":2") != std::wstring::npos,
        "report active wind write backend");
    check(
        frame_boundary(pcs_host::FrameBoundary::BeginScene) == TRUE &&
            read_status(get_status).find(L"wind_applied_frames\":2") !=
                std::wstring::npos,
        "apply wind continuously while the timeline frame is unchanged");
    SendMessageW(strength, TBM_SETPOS, TRUE, 0);
    SendMessageW(panel, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(strength));
    check(
        frame_boundary(pcs_host::FrameBoundary::BeginScene) == TRUE &&
            read_status(get_status).find(L"wind_enabled\":false") != std::wstring::npos &&
            read_status(get_status).find(L"wind_backend\":\"released") !=
                std::wstring::npos,
        "release residual wind velocity when strength reaches zero");
    check(
        frame_boundary(pcs_host::FrameBoundary::BeginScene) == TRUE &&
            read_status(get_status).find(L"wind_backend\":\"off") !=
                std::wstring::npos,
        "remain fully stopped after zero-strength release");
    SendMessageW(strength, TBM_SETPOS, TRUE, 45);
    SendMessageW(panel, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(strength));
    check(
        read_status(get_status).find(L"wind_enabled\":true") != std::wstring::npos,
        "preserve wind enable state while tuning sliders");
    check(
        frame_boundary(pcs_host::FrameBoundary::BeginScene) == TRUE,
        "resume wind after restoring positive strength");
    SendMessageW(wind_enabled, BM_CLICK, 0, 0);
    check(
        frame_boundary(pcs_host::FrameBoundary::BeginScene) == TRUE &&
            read_status(get_status).find(L"wind_backend\":\"released") !=
                std::wstring::npos,
        "master wind switch releases residual velocity");
    SendMessageW(wind_enabled, BM_CLICK, 0, 0);

    ListBox_SetSel(target_list, FALSE, -1);
    ListBox_SetSel(target_list, TRUE, 17);
    SendMessageW(target_list, LB_SETCARETINDEX, 17, FALSE);
    SendMessageW(
        panel,
        WM_COMMAND,
        MAKEWPARAM(kTargetListId, LBN_SELCHANGE),
        reinterpret_cast<LPARAM>(target_list));
    SendMessageW(damping_enabled, BM_CLICK, 0, 0);
    SendMessageW(gravity_enabled, BM_CLICK, 0, 0);
    SendMessageW(set_key, BM_CLICK, 0, 0);
    pump_messages();
    const std::wstring keyed_status = read_status(get_status);
    check(
        keyed_status.find(L"damping_enabled\":true") != std::wstring::npos &&
            keyed_status.find(L"gravity_enabled\":true") != std::wstring::npos &&
            keyed_status.find(L"target_kind\":\"rigid_body") != std::wstring::npos &&
            keyed_status.find(L"target_index\":0") != std::wstring::npos &&
            keyed_status.find(L"keyframes\":1") != std::wstring::npos,
        "keyframe local rigid-body physics controls");
    check(GetFileAttributesA(track_path.c_str()) != INVALID_FILE_ATTRIBUTES,
          "auto-save keyframes to JSON beside the plugin");
    SendMessageW(delete_key, BM_CLICK, 0, 0);
    pump_messages();
    check(
        read_status(get_status).find(L"keyframes\":0") != std::wstring::npos,
        "delete physics keyframe at current MMD frame");

    check(dispatch(pcs_host::kCommandRefreshPanel) == TRUE, "dispatch stable refresh");
    pump_messages();

    check(dispatch(pcs_host::kCommandTogglePanel) == TRUE, "dispatch panel hide");
    pump_messages();
    check(!IsWindowVisible(panel), "hide inspector panel");
    check(uninstall() == TRUE && is_installed() == FALSE, "uninstall physics controller");
    check(
        frame_boundary(pcs_host::FrameBoundary::EndScene) == FALSE,
        "reject frame callback after uninstall");
    check(GetMenuItemCount(GetMenu(host)) == 0, "remove Physics Studio menu");

    FreeLibrary(module);
    DeleteFileA(track_path.c_str());
    DestroyWindow(host);
    if (g_failures != 0) {
        std::cerr << "FAIL cases=" << g_cases << " failures=" << g_failures << '\n';
        return 1;
    }
    std::cout << "PASS cases=" << g_cases
              << " menu=1 panel=1 wind=1 frame_bridge=1\n";
    return 0;
}
