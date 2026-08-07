#pragma once

#include "mmd_931_exports.hpp"

#include <windows.h>
#include <d3d9.h>

#include <cstdint>
#include <cstring>

namespace mmd931::mmhack {

enum class SphereMapMode : std::uint8_t {
    None = 0,
    Multiply = 1,
    Add = 2,
    SubTexture = 3,
};

// Texture and effect pointers returned here are borrowed from the current draw state.
// AddRef them before retaining them beyond the active MME callback.
struct Exports {
    using GetAcsAttachedPmdFn = bool (*)(
        std::uint64_t accessory_id,
        std::uint64_t *model_id,
        std::int32_t *bone_index);
    using GetBlendModeFn = std::int32_t (*)();
    using GetClearColorFn = D3DCOLOR (*)();
    using GetCurrentDrawTypeFn = std::int32_t (*)();
    using GetCurrentEffectFn = void *(*)();
    using GetCurrentFrameTimeFn = float (*)();
    using GetCurrentModelIdFn = std::uint64_t (*)();
    using GetCurrentSubsetIndexFn = std::int32_t (*)();
    using GetWindowFn = HWND (*)();
    using GetLightViewProjMatrixFn = void (*)(
        Matrix4x4 *light_view,
        Matrix4x4 *light_projection,
        Matrix4x4 *light_view_projection);
    using GetMaterialNameFn = const wchar_t *(*)(
        std::uint64_t object_id,
        std::uint32_t material_index,
        std::int32_t name_kind);
    using GetSphereMapModeFn = SphereMapMode (*)();
    using GetTextureFn = IDirect3DBaseTexture9 *(*)();
    using GetBoolFn = bool (*)();
    using GetProjectPathFn = const wchar_t *(*)();

    HMODULE module = nullptr;
    GetAcsAttachedPmdFn GetAcsAttachedPmd = nullptr;
    GetBlendModeFn GetBlendMode = nullptr;
    GetClearColorFn GetClearColor = nullptr;
    GetCurrentDrawTypeFn GetCurrentDrawType = nullptr;
    GetCurrentEffectFn GetCurrentEffect = nullptr;
    GetCurrentFrameTimeFn GetCurrentFrameTime = nullptr;
    GetCurrentModelIdFn GetCurrentModelID = nullptr;
    GetCurrentSubsetIndexFn GetCurrentSubsetIndex = nullptr;
    GetWindowFn GetDrawnWindow = nullptr;
    GetLightViewProjMatrixFn GetLightViewProjMatrix = nullptr;
    GetWindowFn GetMMDMainWindow = nullptr;
    GetMaterialNameFn GetMaterialName = nullptr;
    GetSphereMapModeFn GetSphereMapMode = nullptr;
    GetTextureFn GetSphereMapTexture = nullptr;
    GetTextureFn GetTexture = nullptr;
    GetTextureFn GetToonTexture = nullptr;
    GetBoolFn IsDebugMode = nullptr;
    GetBoolFn IsEditMode = nullptr;
    GetBoolFn IsEffectFileUsed = nullptr;
    GetBoolFn IsToonUsed = nullptr;
    GetProjectPathFn LoadedPMMFile = nullptr;
    GetProjectPathFn SavedPMMFile = nullptr;

    bool resolve(HMODULE hack_module = GetModuleHandleW(L"MMHack.dll")) {
        clear();
        module = hack_module;
        if (module == nullptr) {
            return false;
        }

        bool ok = true;
        ok = resolve_one("GetAcsAttachedPmd", GetAcsAttachedPmd) && ok;
        ok = resolve_one("GetBlendMode", GetBlendMode) && ok;
        ok = resolve_one("GetClearColor", GetClearColor) && ok;
        ok = resolve_one("GetCurrentDrawType", GetCurrentDrawType) && ok;
        ok = resolve_one("GetCurrentEffect", GetCurrentEffect) && ok;
        ok = resolve_one("GetCurrentFrameTime", GetCurrentFrameTime) && ok;
        ok = resolve_one("GetCurrentModelID", GetCurrentModelID) && ok;
        ok = resolve_one("GetCurrentSubsetIndex", GetCurrentSubsetIndex) && ok;
        ok = resolve_one("GetDrawnWindow", GetDrawnWindow) && ok;
        ok = resolve_one("GetLightViewProjMatrix", GetLightViewProjMatrix) && ok;
        ok = resolve_one("GetMMDMainWindow", GetMMDMainWindow) && ok;
        ok = resolve_one("GetMaterialName", GetMaterialName) && ok;
        ok = resolve_one("GetSphereMapMode", GetSphereMapMode) && ok;
        ok = resolve_one("GetSphereMapTexture", GetSphereMapTexture) && ok;
        ok = resolve_one("GetTexture", GetTexture) && ok;
        ok = resolve_one("GetToonTexture", GetToonTexture) && ok;
        ok = resolve_one("IsDebugMode", IsDebugMode) && ok;
        ok = resolve_one("IsEditMode", IsEditMode) && ok;
        ok = resolve_one("IsEffectFileUsed", IsEffectFileUsed) && ok;
        ok = resolve_one("IsToonUsed", IsToonUsed) && ok;
        ok = resolve_one("LoadedPMMFile", LoadedPMMFile) && ok;
        ok = resolve_one("SavedPMMFile", SavedPMMFile) && ok;
        return ok;
    }

    bool complete() const {
        return module != nullptr && GetAcsAttachedPmd != nullptr &&
            GetBlendMode != nullptr && GetClearColor != nullptr &&
            GetCurrentDrawType != nullptr && GetCurrentEffect != nullptr &&
            GetCurrentFrameTime != nullptr && GetCurrentModelID != nullptr &&
            GetCurrentSubsetIndex != nullptr && GetDrawnWindow != nullptr &&
            GetLightViewProjMatrix != nullptr && GetMMDMainWindow != nullptr &&
            GetMaterialName != nullptr && GetSphereMapMode != nullptr &&
            GetSphereMapTexture != nullptr && GetTexture != nullptr &&
            GetToonTexture != nullptr && IsDebugMode != nullptr &&
            IsEditMode != nullptr && IsEffectFileUsed != nullptr &&
            IsToonUsed != nullptr && LoadedPMMFile != nullptr && SavedPMMFile != nullptr;
    }

    void clear() {
        *this = Exports{};
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

}  // namespace mmd931::mmhack
