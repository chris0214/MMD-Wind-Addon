#pragma once

#include "mmd_931_model.hpp"

#include <cstddef>
#include <cstdint>

namespace mmd931::model::morph {

enum class Type : std::uint8_t {
    Group = 0,
    Vertex = 1,
    Bone = 2,
    Uv = 3,
    AdditionalUv1 = 4,
    AdditionalUv2 = 5,
    AdditionalUv3 = 6,
    AdditionalUv4 = 7,
    Material = 8,
};

enum class Panel : std::uint8_t {
    Reserved = 0,
    Eyebrow = 1,
    Eye = 2,
    Mouth = 3,
    Other = 4,
};

enum class MaterialOperation : std::uint8_t {
    Multiply = 0,
    Add = 1,
};

struct GroupMorphEntry {
    std::int32_t morph_index;
    float weight;
};

struct VertexMorphEntry {
    std::uint32_t vertex_index;
    float offset[3];
};

struct BoneMorphEntry {
    std::int32_t bone_index;
    float translation[3];
    float rotation[4];
};

struct UvMorphEntry {
    std::uint32_t vertex_index;
    float offset[4];
};

// The parser expands PMX's packed material payload into this aligned record.
// The two reserved words are zero-filled alignment holes, not disk fields.
struct MaterialMorphEntry {
    std::int32_t material_index;
    MaterialOperation operation;
    std::uint8_t reserved_05[3];
    float diffuse[4];
    float specular[3];
    std::uint32_t reserved_24;
    float specular_power;
    float ambient[3];
    std::uint32_t reserved_38;
    float edge_color[4];
    float edge_size;
    float texture_tint[4];
    float sphere_tint[4];
    float toon_tint[4];
};

// PMD/PMX share this x64 runtime record. Pointer fields are represented as
// 64-bit addresses so the recovered layout remains stable in offline tools.
struct RuntimeMorphRecord {
    char name[0x14];
    char english_name[0x14];
    std::uint64_t wide_name;
    std::uint64_t wide_english_name;
    float weight;
    std::uint32_t element_count;
    std::uint32_t uv_count;
    std::uint32_t additional_uv1_count;
    std::uint32_t additional_uv2_count;
    std::uint32_t additional_uv3_count;
    std::uint32_t additional_uv4_count;
    std::uint32_t bone_count;
    std::uint32_t group_count;
    std::uint32_t material_count;
    Panel panel;
    Type type;
    std::uint8_t reserved_62[6];
    std::uint64_t vertex_entries;
    std::uint64_t bone_entries;
    std::uint64_t group_entries;
    std::uint64_t uv_entries;
    std::uint64_t additional_uv1_entries;
    std::uint64_t additional_uv2_entries;
    std::uint64_t additional_uv3_entries;
    std::uint64_t additional_uv4_entries;
    std::uint64_t material_entries;
    std::uint8_t reserved_b0[0x10];
};

// Parser-generated unique-target arrays used to restore the vertex baseline
// before active morph offsets are accumulated each frame.
struct VertexMorphTargetBaseline {
    std::uint32_t vertex_index;
    float position[3];
};

struct UvMorphTargetBaseline {
    std::uint32_t vertex_index;
    float value[4];
};

static_assert(sizeof(GroupMorphEntry) == 0x08);
static_assert(sizeof(VertexMorphEntry) == 0x10);
static_assert(sizeof(BoneMorphEntry) == 0x20);
static_assert(sizeof(UvMorphEntry) == 0x14);
static_assert(sizeof(MaterialMorphEntry) == 0x80);
static_assert(offsetof(MaterialMorphEntry, diffuse) == 0x08);
static_assert(offsetof(MaterialMorphEntry, specular) == 0x18);
static_assert(offsetof(MaterialMorphEntry, specular_power) == 0x28);
static_assert(offsetof(MaterialMorphEntry, ambient) == 0x2c);
static_assert(offsetof(MaterialMorphEntry, edge_color) == 0x3c);
static_assert(offsetof(MaterialMorphEntry, texture_tint) == 0x50);
static_assert(offsetof(MaterialMorphEntry, sphere_tint) == 0x60);
static_assert(offsetof(MaterialMorphEntry, toon_tint) == 0x70);
static_assert(sizeof(RuntimeMorphRecord) == 0xc0);
static_assert(offsetof(RuntimeMorphRecord, weight) == 0x38);
static_assert(offsetof(RuntimeMorphRecord, type) == 0x61);
static_assert(offsetof(RuntimeMorphRecord, vertex_entries) == 0x68);
static_assert(offsetof(RuntimeMorphRecord, material_entries) == 0xa8);
static_assert(sizeof(VertexMorphTargetBaseline) == 0x10);
static_assert(sizeof(UvMorphTargetBaseline) == 0x14);

inline float group_child_weight(float group_weight, const GroupMorphEntry &entry) {
    return group_weight * entry.weight;
}

inline void accumulate_vertex_offset(
    float (&position)[3],
    const VertexMorphEntry &entry,
    float morph_weight) {
    for (std::size_t component = 0; component < 3; ++component) {
        position[component] += entry.offset[component] * morph_weight;
    }
}

inline void accumulate_uv_offset(
    float (&value)[4],
    const UvMorphEntry &entry,
    float morph_weight) {
    for (std::size_t component = 0; component < 4; ++component) {
        value[component] += entry.offset[component] * morph_weight;
    }
}

inline void accumulate_material_component(
    float &multiplicative,
    float &additive,
    float morph_value,
    MaterialOperation operation,
    float morph_weight) {
    if (operation == MaterialOperation::Multiply) {
        multiplicative *= 1.0f + (morph_value - 1.0f) * morph_weight;
    }
    else {
        additive += morph_value * morph_weight;
    }
}

inline float combine_material_component(
    float base,
    float multiplicative,
    float additive) {
    return base * multiplicative + additive;
}

}  // namespace mmd931::model::morph
