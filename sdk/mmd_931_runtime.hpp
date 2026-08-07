#pragma once

#include <windows.h>
#include <d3d9.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace mmd931::runtime {

constexpr std::uintptr_t kMainStatePointerRva = 0x001445f8;
constexpr std::size_t kMainStateSize = 0x000a55e0;

namespace main_state {
constexpr std::size_t kInstance = 0x00000000;
constexpr std::size_t kDirectSoundContext = 0x000000d0;
constexpr std::size_t kDirectSoundReady = 0x000002d8;
constexpr std::size_t kBackgroundAviTexture = 0x0009f2c0;
constexpr std::size_t kBackgroundAviUploadSurface = 0x0009f2c8;
constexpr std::size_t kFrameCaptureTexture = 0x0009fa78;
constexpr std::size_t kAviGpuReadbackSurface = 0x0009fa88;
constexpr std::size_t kAviSystemMemorySurface = 0x0009fa90;
constexpr std::size_t kResetInvalidationBuffer0 = 0x0009fd78;
constexpr std::size_t kResetInvalidationBuffer1 = 0x0009fe78;
constexpr std::size_t kResetInvalidationBuffer2 = 0x0009ff78;
constexpr std::size_t kRenderPhysicsContext = 0x0009fcb8;
constexpr std::size_t kOutputViewportActive = 0x000a11e4;
constexpr std::size_t kMainWindow = 0x000a16c8;
constexpr std::size_t kD3DContext = 0x000a16e0;
constexpr std::size_t kRenderWidth = 0x000a18f8;
constexpr std::size_t kRenderHeight = 0x000a18fc;
constexpr std::size_t kOpenPath = 0x000a1924;
constexpr std::size_t kFrameTimingValue = 0x000a1904;
constexpr std::size_t kProjectDirty = 0x000a1b31;
constexpr std::size_t kWndProcActive = 0x000a1e18;
}  // namespace main_state

namespace d3d_context {
constexpr std::size_t kDirect3D9 = 0x0003a9b8;
constexpr std::size_t kDevice9 = 0x0003a9c0;
constexpr std::size_t kBackBufferWidth = 0x0003a9c8;
constexpr std::size_t kBackBufferHeight = 0x0003a9cc;
constexpr std::size_t kProjectionAspect = 0x0003a9d0;
constexpr std::size_t kFocusWindow = 0x0003aa08;
constexpr std::size_t kCustomRenderTarget = 0x0003aa28;
constexpr std::size_t kBackBufferSurface = 0x0003aa30;
constexpr std::size_t kOriginalDepthStencil = 0x0003aa38;
constexpr std::size_t kAuxiliaryDefaultPoolTexture = 0x0003aa48;
constexpr std::size_t kManagedEmbeddedTexture = 0x0003aa50;
constexpr std::size_t kAuxiliaryTextureSurface = 0x0003aa58;
constexpr std::size_t kAuxiliaryDepthStencil = 0x0003aa60;
constexpr std::size_t kDeviceEventListener = 0x0003aa70;
}  // namespace d3d_context

template <typename T>
inline T &field(void *base, std::size_t offset) {
    static_assert(!std::is_pointer_v<T> || sizeof(T) == sizeof(void *));
    return *reinterpret_cast<T *>(static_cast<std::uint8_t *>(base) + offset);
}

inline void *main_state_from_module(HMODULE host = GetModuleHandleW(nullptr)) {
    if (host == nullptr) {
        return nullptr;
    }
    auto base = reinterpret_cast<std::uint8_t *>(host);
    return *reinterpret_cast<void **>(base + kMainStatePointerRva);
}

inline HWND main_window(void *state) {
    return state == nullptr ? nullptr : field<HWND>(state, main_state::kMainWindow);
}

inline void *d3d_state(void *state) {
    return state == nullptr ? nullptr : field<void *>(state, main_state::kD3DContext);
}

inline void *render_physics_context(void *state) {
    return state == nullptr ? nullptr : field<void *>(state, main_state::kRenderPhysicsContext);
}

inline IDirect3D9 *direct3d9(void *d3d) {
    return d3d == nullptr ? nullptr : field<IDirect3D9 *>(d3d, d3d_context::kDirect3D9);
}

inline IDirect3DDevice9 *device9(void *d3d) {
    return d3d == nullptr ? nullptr : field<IDirect3DDevice9 *>(d3d, d3d_context::kDevice9);
}

inline std::uint32_t back_buffer_width(void *d3d) {
    return d3d == nullptr ? 0 : field<std::uint32_t>(d3d, d3d_context::kBackBufferWidth);
}

inline std::uint32_t back_buffer_height(void *d3d) {
    return d3d == nullptr ? 0 : field<std::uint32_t>(d3d, d3d_context::kBackBufferHeight);
}

}  // namespace mmd931::runtime
