#include "physics_control_studio/host_api.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace pcs_host = physics_control_studio::host_api;

namespace {

template <typename Function>
Function resolve(HMODULE module, const char* name) {
    static_assert(sizeof(Function) == sizeof(FARPROC));
    FARPROC raw = GetProcAddress(module, name);
    Function result = nullptr;
    std::memcpy(&result, &raw, sizeof(result));
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "expected DLL path\n";
        return 1;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    if (module == nullptr) {
        std::cerr << "LoadLibrary failed error=" << GetLastError() << '\n';
        return 1;
    }

    const auto get_version = resolve<pcs_host::GetApiVersionFn>(
        module, "MmdPhysicsGetApiVersion");
    const auto install = resolve<pcs_host::InstallFn>(module, "MmdPhysicsInstall");
    const auto install_for_window = resolve<pcs_host::InstallForWindowFn>(
        module, "MmdPhysicsInstallForWindow");
    const auto uninstall = resolve<pcs_host::UninstallFn>(module, "MmdPhysicsUninstall");
    const auto is_installed = resolve<pcs_host::IsInstalledFn>(
        module, "MmdPhysicsIsInstalled");
    const auto get_status = resolve<pcs_host::GetStatusJsonFn>(
        module, "MmdPhysicsGetStatusJsonW");
    const auto get_panel = resolve<pcs_host::GetPanelWindowFn>(
        module, "MmdPhysicsGetPanelWindow");
    const auto dispatch = resolve<pcs_host::DispatchCommandFn>(
        module, "MmdPhysicsDispatchCommand");
    const auto frame_boundary = resolve<pcs_host::FrameBoundaryFn>(
        module, "MmdPhysicsOnFrameBoundary");

    bool passed = get_version != nullptr && install != nullptr &&
        install_for_window != nullptr && uninstall != nullptr &&
        is_installed != nullptr && get_status != nullptr && get_panel != nullptr &&
        dispatch != nullptr && frame_boundary != nullptr;
    if (passed) passed = get_version() == pcs_host::kApiVersion;
    if (passed) passed = install() == FALSE;
    if (passed) passed = install_for_window(GetConsoleWindow()) == FALSE;
    if (passed) passed = is_installed() == FALSE;
    if (passed) passed = get_panel() == nullptr;
    if (passed) passed = dispatch(pcs_host::kCommandTogglePanel) == FALSE;
    if (passed) {
        passed = frame_boundary(pcs_host::FrameBoundary::BeginScene) == FALSE;
    }

    DWORD required = 0;
    std::vector<wchar_t> json;
    if (passed) {
        required = get_status(nullptr, 0);
        json.resize(required);
        passed = required > 1 && get_status(json.data(), required) == required;
    }
    if (passed) {
        const std::wstring value(json.data());
        passed = value.find(L"unsupported_name") != std::wstring::npos &&
            value.find(L"write_backend\":\"bullet_velocity") != std::wstring::npos &&
            value.find(L"wind_enabled\":false") != std::wstring::npos &&
            value.find(L"frame_bridge\":\"inactive") != std::wstring::npos;
    }
    if (passed) passed = uninstall() == TRUE && is_installed() == FALSE;

    FreeLibrary(module);
    if (!passed) {
        std::cerr << "host contract fixture failed\n";
        return 1;
    }
    std::cout << "PASS exports=9 fail_closed=1 write_backend=bullet_velocity\n";
    return 0;
}
