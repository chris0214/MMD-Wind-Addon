#pragma once

#include <windows.h>

#include <cstdint>

namespace physics_control_studio::host_api {

inline constexpr std::uint32_t kApiVersion = 0x00010001;
inline constexpr UINT kCommandTogglePanel = 0xea00;
inline constexpr UINT kCommandRefreshPanel = 0xea01;
inline constexpr wchar_t kSupportedExecutableName[] = L"MikuMikudance.exe";
inline constexpr std::uint64_t kSupportedExecutableSize = 1'723'392;
inline constexpr char kSupportedExecutableSha256[] =
    "2C9414C21619B4AD85D9C2EF76836F3C34DB7A8ABD07BD6C6176D385F7EFDFB4";

enum class FrameBoundary : std::uint32_t {
    BeginScene = 1,
    EndScene = 2,
};

using GetApiVersionFn = std::uint32_t (*)();
using InstallFn = BOOL (*)();
using InstallForWindowFn = BOOL (*)(HWND);
using UninstallFn = BOOL (*)();
using IsInstalledFn = BOOL (*)();
using GetStatusJsonFn = DWORD (*)(wchar_t*, DWORD);
using GetPanelWindowFn = HWND (*)();
using DispatchCommandFn = BOOL (*)(UINT);
using FrameBoundaryFn = BOOL (*)(FrameBoundary);

}  // namespace physics_control_studio::host_api

extern "C" {

__declspec(dllexport) std::uint32_t MmdPhysicsGetApiVersion();
__declspec(dllexport) BOOL MmdPhysicsInstall();
__declspec(dllexport) BOOL MmdPhysicsInstallForWindow(HWND host_window);
__declspec(dllexport) BOOL MmdPhysicsUninstall();
__declspec(dllexport) BOOL MmdPhysicsIsInstalled();
__declspec(dllexport) DWORD MmdPhysicsGetStatusJsonW(wchar_t* output, DWORD capacity);
__declspec(dllexport) HWND MmdPhysicsGetPanelWindow();
__declspec(dllexport) BOOL MmdPhysicsDispatchCommand(UINT command);
__declspec(dllexport) BOOL MmdPhysicsOnFrameBoundary(
    physics_control_studio::host_api::FrameBoundary boundary);

}
