#pragma once

#include <windows.h>
#include <d3d9.h>

#include <cstddef>
#include <cstring>
#include <cstdint>

namespace mmd931 {

struct Matrix4x4 {
    float value[16];
};

// Both host material exporters return the leading D3DMATERIAL9 record verbatim.
using Material = D3DMATERIAL9;
using MaterialBlob = Material;

static_assert(sizeof(Matrix4x4) == 0x40, "MMD matrix ABI mismatch");
static_assert(sizeof(Material) == 0x44, "MMD material ABI mismatch");
static_assert(offsetof(Material, Diffuse) == 0x00, "MMD diffuse offset mismatch");
static_assert(offsetof(Material, Ambient) == 0x10, "MMD ambient offset mismatch");
static_assert(offsetof(Material, Specular) == 0x20, "MMD specular offset mismatch");
static_assert(offsetof(Material, Emissive) == 0x30, "MMD emissive offset mismatch");
static_assert(offsetof(Material, Power) == 0x40, "MMD power offset mismatch");

struct Exports {
    using GetFrameTimeFn = float (*)();
    using GetPmdNumFn = int (*)();
    using GetPmdFilenameFn = const char *(*)(int model_index);
    using GetPmdOrderFn = int (*)(int model_index);
    using GetPmdMatNumFn = std::uint32_t (*)(int model_index);
    using GetPmdMaterialFn = Material *(*)(Material *output, int model_index, std::uint32_t material_index);
    using GetPmdBoneNumFn = std::uint32_t (*)(int model_index);
    using GetPmdBoneNameFn = const char *(*)(int model_index, int bone_index);
    using GetPmdBoneWorldMatFn = void *(*)(void *output, int model_index, int bone_index);
    using GetPmdMorphNumFn = std::uint32_t (*)(int model_index);
    using GetPmdMorphNameFn = const char *(*)(int model_index, int morph_index);
    using GetPmdMorphValueFn = float (*)(int model_index, int morph_index);
    using GetPmdDispFn = std::uint64_t (*)(int model_index);
    using GetPmdIdFn = std::uint32_t (*)(int model_index);

    using GetAcsNumFn = int (*)();
    using GetPreAcsNumFn = std::uint32_t (*)();
    using GetAcsFilenameFn = const char *(*)(int accessory_index);
    using GetAcsOrderFn = int (*)(int accessory_index);
    using GetAcsWorldMatFn = void *(*)(void *output, int accessory_index);
    using GetAcsFloatFn = float (*)(int accessory_index);
    using GetAcsDispFn = std::uint64_t (*)(int accessory_index);
    using GetAcsIdFn = std::uint32_t (*)(int accessory_index);
    using GetAcsMatNumFn = std::uint32_t (*)(int accessory_index);
    using GetAcsMaterialFn = Material *(*)(Material *output, int accessory_index, int material_index);

    using GetCurrentObjectFn = int (*)();
    using GetCurrentMaterialFn = std::uint32_t (*)();
    using GetCurrentTechnicFn = std::uint32_t (*)();
    using SetRenderRepeatCountFn = void (*)(std::uint32_t count);
    using GetRenderRepeatCountFn = std::uint32_t (*)();
    using GetEnglishModeFn = std::uint8_t (*)();

    HMODULE host = nullptr;
    GetFrameTimeFn ExpGetFrameTime = nullptr;
    GetPmdNumFn ExpGetPmdNum = nullptr;
    GetPmdFilenameFn ExpGetPmdFilename = nullptr;
    GetPmdOrderFn ExpGetPmdOrder = nullptr;
    GetPmdMatNumFn ExpGetPmdMatNum = nullptr;
    GetPmdMaterialFn ExpGetPmdMaterial = nullptr;
    GetPmdBoneNumFn ExpGetPmdBoneNum = nullptr;
    GetPmdBoneNameFn ExpGetPmdBoneName = nullptr;
    GetPmdBoneWorldMatFn ExpGetPmdBoneWorldMat = nullptr;
    GetPmdMorphNumFn ExpGetPmdMorphNum = nullptr;
    GetPmdMorphNameFn ExpGetPmdMorphName = nullptr;
    GetPmdMorphValueFn ExpGetPmdMorphValue = nullptr;
    GetPmdDispFn ExpGetPmdDisp = nullptr;
    GetPmdIdFn ExpGetPmdID = nullptr;

    GetAcsNumFn ExpGetAcsNum = nullptr;
    GetPreAcsNumFn ExpGetPreAcsNum = nullptr;
    GetAcsFilenameFn ExpGetAcsFilename = nullptr;
    GetAcsOrderFn ExpGetAcsOrder = nullptr;
    GetAcsWorldMatFn ExpGetAcsWorldMat = nullptr;
    GetAcsFloatFn ExpGetAcsX = nullptr;
    GetAcsFloatFn ExpGetAcsY = nullptr;
    GetAcsFloatFn ExpGetAcsZ = nullptr;
    GetAcsFloatFn ExpGetAcsRx = nullptr;
    GetAcsFloatFn ExpGetAcsRy = nullptr;
    GetAcsFloatFn ExpGetAcsRz = nullptr;
    GetAcsFloatFn ExpGetAcsSi = nullptr;
    GetAcsFloatFn ExpGetAcsTr = nullptr;
    GetAcsDispFn ExpGetAcsDisp = nullptr;
    GetAcsIdFn ExpGetAcsID = nullptr;
    GetAcsMatNumFn ExpGetAcsMatNum = nullptr;
    GetAcsMaterialFn ExpGetAcsMaterial = nullptr;

    GetCurrentObjectFn ExpGetCurrentObject = nullptr;
    GetCurrentMaterialFn ExpGetCurrentMaterial = nullptr;
    GetCurrentTechnicFn ExpGetCurrentTechnic = nullptr;
    SetRenderRepeatCountFn ExpSetRenderRepeatCount = nullptr;
    GetRenderRepeatCountFn ExpGetRenderRepeatCount = nullptr;
    GetEnglishModeFn ExpGetEnglishMode = nullptr;

    bool load(HMODULE module = GetModuleHandleW(nullptr)) {
        host = module;
        if (host == nullptr) {
            return false;
        }

        bool ok = true;
#define MMD_RESOLVE(name, type) \
        static_assert(sizeof(type) == sizeof(FARPROC), "function pointer size mismatch"); \
        FARPROC raw_##name = GetProcAddress(host, #name); \
        std::memcpy(&name, &raw_##name, sizeof(name)); \
        ok = (name != nullptr) && ok
        MMD_RESOLVE(ExpGetFrameTime, GetFrameTimeFn);
        MMD_RESOLVE(ExpGetPmdNum, GetPmdNumFn);
        MMD_RESOLVE(ExpGetPmdFilename, GetPmdFilenameFn);
        MMD_RESOLVE(ExpGetPmdOrder, GetPmdOrderFn);
        MMD_RESOLVE(ExpGetPmdMatNum, GetPmdMatNumFn);
        MMD_RESOLVE(ExpGetPmdMaterial, GetPmdMaterialFn);
        MMD_RESOLVE(ExpGetPmdBoneNum, GetPmdBoneNumFn);
        MMD_RESOLVE(ExpGetPmdBoneName, GetPmdBoneNameFn);
        MMD_RESOLVE(ExpGetPmdBoneWorldMat, GetPmdBoneWorldMatFn);
        MMD_RESOLVE(ExpGetPmdMorphNum, GetPmdMorphNumFn);
        MMD_RESOLVE(ExpGetPmdMorphName, GetPmdMorphNameFn);
        MMD_RESOLVE(ExpGetPmdMorphValue, GetPmdMorphValueFn);
        MMD_RESOLVE(ExpGetPmdDisp, GetPmdDispFn);
        MMD_RESOLVE(ExpGetPmdID, GetPmdIdFn);
        MMD_RESOLVE(ExpGetAcsNum, GetAcsNumFn);
        MMD_RESOLVE(ExpGetPreAcsNum, GetPreAcsNumFn);
        MMD_RESOLVE(ExpGetAcsFilename, GetAcsFilenameFn);
        MMD_RESOLVE(ExpGetAcsOrder, GetAcsOrderFn);
        MMD_RESOLVE(ExpGetAcsWorldMat, GetAcsWorldMatFn);
        MMD_RESOLVE(ExpGetAcsX, GetAcsFloatFn);
        MMD_RESOLVE(ExpGetAcsY, GetAcsFloatFn);
        MMD_RESOLVE(ExpGetAcsZ, GetAcsFloatFn);
        MMD_RESOLVE(ExpGetAcsRx, GetAcsFloatFn);
        MMD_RESOLVE(ExpGetAcsRy, GetAcsFloatFn);
        MMD_RESOLVE(ExpGetAcsRz, GetAcsFloatFn);
        MMD_RESOLVE(ExpGetAcsSi, GetAcsFloatFn);
        MMD_RESOLVE(ExpGetAcsTr, GetAcsFloatFn);
        MMD_RESOLVE(ExpGetAcsDisp, GetAcsDispFn);
        MMD_RESOLVE(ExpGetAcsID, GetAcsIdFn);
        MMD_RESOLVE(ExpGetAcsMatNum, GetAcsMatNumFn);
        MMD_RESOLVE(ExpGetAcsMaterial, GetAcsMaterialFn);
        MMD_RESOLVE(ExpGetCurrentObject, GetCurrentObjectFn);
        MMD_RESOLVE(ExpGetCurrentMaterial, GetCurrentMaterialFn);
        MMD_RESOLVE(ExpGetCurrentTechnic, GetCurrentTechnicFn);
        MMD_RESOLVE(ExpSetRenderRepeatCount, SetRenderRepeatCountFn);
        MMD_RESOLVE(ExpGetRenderRepeatCount, GetRenderRepeatCountFn);
        MMD_RESOLVE(ExpGetEnglishMode, GetEnglishModeFn);
#undef MMD_RESOLVE
        return ok;
    }
};

}  // namespace mmd931
