#pragma once

#include <windows.h>
#include <d3d9.h>

#include <cstdint>
#include <cstring>

namespace mmd931::mme {

enum class ModelKind : std::int32_t {
    Accessory = 0,
    PmdOrPmx = 1,
};

using ObjectId = std::uint32_t;

using InitializeFn = int (*)(IDirect3DDevice9 *device);
using CleanupFn = void (*)(IDirect3DDevice9 *device);
using OnCreateModelFn = void (*)(
    IDirect3DDevice9 *device,
    ObjectId id,
    const char *filename,
    ModelKind kind,
    std::uint32_t material_count,
    void *reserved0,
    IUnknown *reserved1);
using OnDeleteModelFn = void (*)(IDirect3DDevice9 *device, ObjectId id);
using OnBeginSceneFn = void (*)(IDirect3DDevice9 *device);
using OnEndSceneFn = void (*)(IDirect3DDevice9 *device);
using OnDrawPrimitiveFn = HRESULT (*)(
    IDirect3DDevice9 *device,
    D3DPRIMITIVETYPE primitive_type,
    UINT start_vertex,
    UINT primitive_count);
using OnDrawIndexedPrimitiveFn = HRESULT (*)(
    IDirect3DDevice9 *device,
    D3DPRIMITIVETYPE primitive_type,
    INT base_vertex_index,
    UINT min_vertex_index,
    UINT vertex_count,
    UINT start_index,
    UINT primitive_count);
using OnLostDeviceFn = void (*)(IDirect3DDevice9 *device);
using OnResetDeviceFn = void (*)(IDirect3DDevice9 *device);
using OnClearFn = HRESULT (*)(
    IDirect3DDevice9 *device,
    DWORD rect_count,
    const D3DRECT *rects,
    DWORD flags,
    D3DCOLOR color,
    float z,
    DWORD stencil,
    BOOL is_main_target);

struct Callbacks {
    HMODULE module = nullptr;
    InitializeFn Initialize = nullptr;
    CleanupFn Cleanup = nullptr;
    OnCreateModelFn OnCreateModel = nullptr;
    OnDeleteModelFn OnDeleteModel = nullptr;
    OnBeginSceneFn OnBeginScene = nullptr;
    OnEndSceneFn OnEndScene = nullptr;
    OnDrawPrimitiveFn OnDrawPrimitive = nullptr;
    OnDrawIndexedPrimitiveFn OnDrawIndexedPrimitive = nullptr;
    OnLostDeviceFn OnLostDevice = nullptr;
    OnResetDeviceFn OnResetDevice = nullptr;
    OnClearFn OnClear = nullptr;

    bool resolve(HMODULE effect_module) {
        clear();
        module = effect_module;
        if (module == nullptr) {
            return false;
        }

        bool ok = true;
        ok = resolve_one("Initialize", Initialize) && ok;
        ok = resolve_one("Cleanup", Cleanup) && ok;
        ok = resolve_one("OnCreateModel", OnCreateModel) && ok;
        ok = resolve_one("OnDeleteModel", OnDeleteModel) && ok;
        ok = resolve_one("OnBeginScene", OnBeginScene) && ok;
        ok = resolve_one("OnEndScene", OnEndScene) && ok;
        ok = resolve_one("OnDrawPrimitive", OnDrawPrimitive) && ok;
        ok = resolve_one("OnDrawIndexedPrimitive", OnDrawIndexedPrimitive) && ok;
        ok = resolve_one("OnLostDevice", OnLostDevice) && ok;
        ok = resolve_one("OnResetDevice", OnResetDevice) && ok;
        ok = resolve_one("OnClear", OnClear) && ok;
        return ok;
    }

    bool complete() const {
        return module != nullptr && Initialize != nullptr && Cleanup != nullptr &&
            OnCreateModel != nullptr && OnDeleteModel != nullptr &&
            OnBeginScene != nullptr && OnEndScene != nullptr &&
            OnDrawPrimitive != nullptr && OnDrawIndexedPrimitive != nullptr &&
            OnLostDevice != nullptr && OnResetDevice != nullptr && OnClear != nullptr;
    }

    void clear() {
        module = nullptr;
        Initialize = nullptr;
        Cleanup = nullptr;
        OnCreateModel = nullptr;
        OnDeleteModel = nullptr;
        OnBeginScene = nullptr;
        OnEndScene = nullptr;
        OnDrawPrimitive = nullptr;
        OnDrawIndexedPrimitive = nullptr;
        OnLostDevice = nullptr;
        OnResetDevice = nullptr;
        OnClear = nullptr;
    }

private:
    template <typename FunctionPointer>
    bool resolve_one(const char *name, FunctionPointer &output) {
        static_assert(sizeof(FunctionPointer) == sizeof(FARPROC));
        FARPROC raw = GetProcAddress(module, name);
        std::memcpy(&output, &raw, sizeof(output));
        return output != nullptr;
    }
};

}  // namespace mmd931::mme
