#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumRadius = 2.0f;
constexpr float kMaximumRadius = 80.0f;
constexpr float kStrengthLengthRatio = 4.0f;
constexpr std::uint32_t kRingSegments = 24;
const std::u16string kAuthorComment =
    u"\u4f5c\u8005\uff1a\u514b\u91cc\u65af\u63d0\u4e9a\u5a1c";

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 add(Vec3 left, Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 scale(Vec3 value, float amount) noexcept {
    return {value.x * amount, value.y * amount, value.z * amount};
}

Vec3 cross(Vec3 left, Vec3 right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x};
}

Vec3 normalize(Vec3 value) noexcept {
    const float length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (length_squared <= 1.0e-12f) return {0.0f, 1.0f, 0.0f};
    return scale(value, 1.0f / std::sqrt(length_squared));
}

struct Vertex {
    Vec3 position{};
    Vec3 normal{};
    Vec2 uv{};
};

struct MorphOffset {
    std::uint32_t vertex_index = 0;
    Vec3 offset{};
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MorphOffset> radius_offsets;
    std::vector<MorphOffset> strength_offsets;
    std::vector<MorphOffset> falloff_offsets;
    std::array<std::uint32_t, 3> material_index_counts{};
};

std::uint32_t append_vertex(Mesh& mesh, Vertex vertex) {
    const auto index = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(vertex);
    return index;
}

void append_triangle(
    Mesh& mesh,
    std::uint32_t first,
    std::uint32_t second,
    std::uint32_t third) {
    mesh.indices.push_back(first);
    mesh.indices.push_back(second);
    mesh.indices.push_back(third);
}

void append_quad(
    Mesh& mesh,
    const std::array<Vec3, 4>& positions,
    Vec3 normal,
    std::vector<MorphOffset>* morph,
    const std::array<Vec3, 4>& offsets) {
    const std::uint32_t first = static_cast<std::uint32_t>(mesh.vertices.size());
    static constexpr std::array<Vec2, 4> uvs{{
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}}};
    for (std::size_t index = 0; index < positions.size(); ++index) {
        const std::uint32_t vertex_index = append_vertex(
            mesh,
            {positions[index], normal, uvs[index]});
        if (morph != nullptr) morph->push_back({vertex_index, offsets[index]});
    }
    append_triangle(mesh, first, first + 1, first + 2);
    append_triangle(mesh, first, first + 2, first + 3);
}

void append_ring(
    Mesh& mesh,
    Vec3 axis_u,
    Vec3 axis_v,
    Vec3 normal) {
    constexpr float half_width = 0.055f;
    std::array<std::uint32_t, kRingSegments> inner{};
    std::array<std::uint32_t, kRingSegments> outer{};
    for (std::uint32_t segment = 0; segment < kRingSegments; ++segment) {
        const float phase = 2.0f * kPi * static_cast<float>(segment) /
            static_cast<float>(kRingSegments);
        const Vec3 radial = add(
            scale(axis_u, std::cos(phase)),
            scale(axis_v, std::sin(phase)));
        inner[segment] = append_vertex(
            mesh,
            {scale(radial, kMinimumRadius - half_width), normal,
             {static_cast<float>(segment) / static_cast<float>(kRingSegments), 0.0f}});
        outer[segment] = append_vertex(
            mesh,
            {scale(radial, kMinimumRadius + half_width), normal,
             {static_cast<float>(segment) / static_cast<float>(kRingSegments), 1.0f}});
        const Vec3 radius_offset = scale(radial, kMaximumRadius - kMinimumRadius);
        mesh.radius_offsets.push_back({inner[segment], radius_offset});
        mesh.radius_offsets.push_back({outer[segment], radius_offset});
    }
    for (std::uint32_t segment = 0; segment < kRingSegments; ++segment) {
        const std::uint32_t next = (segment + 1) % kRingSegments;
        append_triangle(mesh, inner[segment], outer[segment], outer[next]);
        append_triangle(mesh, inner[segment], outer[next], inner[next]);
    }
}

Vec3 arrow_offset(Vec3 position) noexcept {
    return {position.x * (kStrengthLengthRatio - 1.0f), 0.0f, 0.0f};
}

void append_arrow_box(Mesh& mesh) {
    constexpr float start = 0.0f;
    constexpr float end = 2.1f;
    constexpr float half = 0.12f;
    const std::array<std::array<Vec3, 4>, 6> faces{{
        std::array<Vec3, 4>{{
            {start, -half, -half}, {end, -half, -half},
            {end, half, -half}, {start, half, -half}}},
        std::array<Vec3, 4>{{
            {start, half, half}, {end, half, half},
            {end, -half, half}, {start, -half, half}}},
        std::array<Vec3, 4>{{
            {start, -half, half}, {end, -half, half},
            {end, -half, -half}, {start, -half, -half}}},
        std::array<Vec3, 4>{{
            {start, half, -half}, {end, half, -half},
            {end, half, half}, {start, half, half}}},
        std::array<Vec3, 4>{{
            {start, -half, half}, {start, -half, -half},
            {start, half, -half}, {start, half, half}}},
        std::array<Vec3, 4>{{
            {end, -half, -half}, {end, -half, half},
            {end, half, half}, {end, half, -half}}}}};
    const std::array<Vec3, 6> normals{{
        {0.0f, 0.0f, -1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f}}};
    for (std::size_t face = 0; face < faces.size(); ++face) {
        std::array<Vec3, 4> offsets{};
        for (std::size_t index = 0; index < offsets.size(); ++index)
            offsets[index] = arrow_offset(faces[face][index]);
        append_quad(mesh, faces[face], normals[face], &mesh.strength_offsets, offsets);
    }
}

void append_arrow_head(Mesh& mesh) {
    constexpr float base = 2.05f;
    constexpr float tip = 3.0f;
    constexpr float half = 0.38f;
    const std::array<Vec3, 4> corners{{
        {base, -half, -half},
        {base, half, -half},
        {base, half, half},
        {base, -half, half}}};
    const Vec3 point{tip, 0.0f, 0.0f};
    for (std::size_t face = 0; face < corners.size(); ++face) {
        const Vec3 first = corners[face];
        const Vec3 second = corners[(face + 1) % corners.size()];
        const Vec3 normal = normalize(cross(subtract(second, first), subtract(point, first)));
        const std::uint32_t base_index = static_cast<std::uint32_t>(mesh.vertices.size());
        const std::array<Vec3, 3> positions{{first, second, point}};
        for (std::size_t index = 0; index < positions.size(); ++index) {
            const std::uint32_t vertex_index = append_vertex(
                mesh,
                {positions[index], normal,
                 {static_cast<float>(index == 1), static_cast<float>(index == 2)}});
            mesh.strength_offsets.push_back({vertex_index, arrow_offset(positions[index])});
        }
        append_triangle(mesh, base_index, base_index + 1, base_index + 2);
    }
    std::array<Vec3, 4> base_offsets{};
    for (std::size_t index = 0; index < corners.size(); ++index)
        base_offsets[index] = arrow_offset(corners[index]);
    append_quad(
        mesh,
        {corners[3], corners[2], corners[1], corners[0]},
        {-1.0f, 0.0f, 0.0f},
        &mesh.strength_offsets,
        base_offsets);
}

void append_falloff_gauge(Mesh& mesh) {
    constexpr float x = -0.58f;
    constexpr float half_width = 0.035f;
    constexpr float minimum_y = -0.9f;
    constexpr float maximum_y = 0.9f;
    const std::array<Vec3, 4> bar{{
        {x - half_width, minimum_y, 0.10f},
        {x + half_width, minimum_y, 0.10f},
        {x + half_width, maximum_y, 0.10f},
        {x - half_width, maximum_y, 0.10f}}};
    append_quad(mesh, bar, {0.0f, 0.0f, 1.0f}, nullptr, {});

    constexpr float marker_half_x = 0.16f;
    constexpr float marker_half_y = 0.09f;
    const std::array<Vec3, 4> marker{{
        {x - marker_half_x, minimum_y, 0.13f},
        {x, minimum_y - marker_half_y, 0.13f},
        {x + marker_half_x, minimum_y, 0.13f},
        {x, minimum_y + marker_half_y, 0.13f}}};
    std::array<Vec3, 4> offsets{};
    offsets.fill({0.0f, maximum_y - minimum_y, 0.0f});
    append_quad(
        mesh,
        marker,
        {0.0f, 0.0f, 1.0f},
        &mesh.falloff_offsets,
        offsets);
}

Mesh make_mesh() {
    Mesh mesh;
    append_ring(mesh, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    append_ring(mesh, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f});
    append_ring(mesh, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f});
    const std::size_t gauge_begin = mesh.indices.size();
    append_falloff_gauge(mesh);
    constexpr std::size_t marker_index_count = 6;
    const std::array<std::uint32_t, marker_index_count> marker_indices{{
        mesh.indices[gauge_begin + 6],
        mesh.indices[gauge_begin + 7],
        mesh.indices[gauge_begin + 8],
        mesh.indices[gauge_begin + 9],
        mesh.indices[gauge_begin + 10],
        mesh.indices[gauge_begin + 11]}};
    mesh.indices.erase(
        mesh.indices.begin() + static_cast<std::ptrdiff_t>(gauge_begin + 6),
        mesh.indices.begin() + static_cast<std::ptrdiff_t>(gauge_begin + 12));
    const std::size_t range_index_count = mesh.indices.size();

    append_arrow_box(mesh);
    append_arrow_head(mesh);
    const std::size_t arrow_index_count = mesh.indices.size() - range_index_count;
    mesh.indices.insert(mesh.indices.end(), marker_indices.begin(), marker_indices.end());

    mesh.material_index_counts = {
        static_cast<std::uint32_t>(range_index_count),
        static_cast<std::uint32_t>(arrow_index_count),
        static_cast<std::uint32_t>(marker_index_count)};
    return mesh;
}

template <typename Value>
void write_value(std::ofstream& stream, const Value& value) {
    static_assert(std::is_trivially_copyable_v<Value>);
    stream.write(
        reinterpret_cast<const char*>(&value),
        static_cast<std::streamsize>(sizeof(value)));
}

void write_bytes(std::ofstream& stream, const void* data, std::size_t size) {
    if (size == 0) return;
    stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
}

void write_text(std::ofstream& stream, const std::u16string& text) {
    const std::size_t byte_count = text.size() * sizeof(char16_t);
    write_value(stream, static_cast<std::uint32_t>(byte_count));
    write_bytes(stream, text.data(), byte_count);
}

void write_vec2(std::ofstream& stream, Vec2 value) {
    write_value(stream, value.x);
    write_value(stream, value.y);
}

void write_vec3(std::ofstream& stream, Vec3 value) {
    write_value(stream, value.x);
    write_value(stream, value.y);
    write_value(stream, value.z);
}

void write_vec4(std::ofstream& stream, float x, float y, float z, float w) {
    write_value(stream, x);
    write_value(stream, y);
    write_value(stream, z);
    write_value(stream, w);
}

void write_material(
    std::ofstream& stream,
    const std::u16string& name,
    const std::u16string& english_name,
    const std::array<float, 4>& diffuse,
    const std::array<float, 3>& ambient,
    const std::array<float, 4>& edge,
    float edge_size,
    std::uint32_t index_count) {
    write_text(stream, name);
    write_text(stream, english_name);
    write_vec4(stream, diffuse[0], diffuse[1], diffuse[2], diffuse[3]);
    write_vec3(stream, {0.05f, 0.05f, 0.05f});
    write_value(stream, 8.0f);
    write_vec3(stream, {ambient[0], ambient[1], ambient[2]});
    write_value(stream, std::uint8_t{0x11});
    write_vec4(stream, edge[0], edge[1], edge[2], edge[3]);
    write_value(stream, edge_size);
    write_value(stream, std::int8_t{-1});
    write_value(stream, std::int8_t{-1});
    write_value(stream, std::uint8_t{0});
    write_value(stream, std::uint8_t{1});
    write_value(stream, std::uint8_t{0});
    write_text(stream, u"");
    write_value(stream, static_cast<std::int32_t>(index_count));
}

void write_vertex_morph(
    std::ofstream& stream,
    const std::u16string& name,
    const std::u16string& runtime_name,
    const std::vector<MorphOffset>& offsets) {
    write_text(stream, name);
    write_text(stream, runtime_name);
    write_value(stream, std::uint8_t{4});
    write_value(stream, std::uint8_t{1});
    write_value(stream, static_cast<std::uint32_t>(offsets.size()));
    for (const MorphOffset& offset : offsets) {
        write_value(stream, static_cast<std::uint8_t>(offset.vertex_index));
        write_vec3(stream, offset.offset);
    }
}

void write_display_frame_header(
    std::ofstream& stream,
    const std::u16string& name,
    const std::u16string& english_name,
    std::uint32_t count) {
    write_text(stream, name);
    write_text(stream, english_name);
    write_value(stream, std::uint8_t{1});
    write_value(stream, count);
}

bool write_pmx(const std::filesystem::path& path, const Mesh& mesh, std::string& error) {
    if (mesh.vertices.empty() || mesh.vertices.size() > 255) {
        error = "wind source must fit the PMX one-byte vertex index";
        return false;
    }
    for (const std::uint32_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            error = "mesh index is outside the vertex array";
            return false;
        }
    }
    const auto validate_morph = [&](const std::vector<MorphOffset>& offsets) {
        for (const MorphOffset& offset : offsets) {
            if (offset.vertex_index >= mesh.vertices.size()) return false;
        }
        return true;
    };
    if (!validate_morph(mesh.radius_offsets) || !validate_morph(mesh.strength_offsets) ||
        !validate_morph(mesh.falloff_offsets)) {
        error = "morph offset is outside the vertex array";
        return false;
    }
    const std::uint64_t material_total =
        static_cast<std::uint64_t>(mesh.material_index_counts[0]) +
        static_cast<std::uint64_t>(mesh.material_index_counts[1]) +
        static_cast<std::uint64_t>(mesh.material_index_counts[2]);
    if (material_total != mesh.indices.size()) {
        error = "material index sections do not cover the mesh";
        return false;
    }

    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "cannot create output file";
        return false;
    }

    static constexpr std::array<char, 4> magic{{'P', 'M', 'X', ' '}};
    static constexpr std::array<std::uint8_t, 8> globals{{
        0,
        0,
        1,
        1,
        1,
        1,
        1,
        1}};
    write_bytes(stream, magic.data(), magic.size());
    write_value(stream, 2.0f);
    write_value(stream, std::uint8_t{8});
    write_bytes(stream, globals.data(), globals.size());
    write_text(stream, u"WindTool \u98a8\u6e90\u5236\u5fa1\u5668");
    write_text(stream, u"WindTool Wind Source Controller");
    write_text(stream, kAuthorComment);
    write_text(stream, kAuthorComment);

    write_value(stream, static_cast<std::uint32_t>(mesh.vertices.size()));
    for (const Vertex& vertex : mesh.vertices) {
        write_vec3(stream, vertex.position);
        write_vec3(stream, vertex.normal);
        write_vec2(stream, vertex.uv);
        write_value(stream, std::uint8_t{0});
        write_value(stream, std::int8_t{0});
        write_value(stream, 1.0f);
    }

    write_value(stream, static_cast<std::uint32_t>(mesh.indices.size()));
    for (const std::uint32_t index : mesh.indices)
        write_value(stream, static_cast<std::uint8_t>(index));

    write_value(stream, std::uint32_t{0});
    write_value(stream, std::uint32_t{3});
    write_material(
        stream,
        u"\u5c40\u90e8\u534a\u5f84",
        u"Wind radius",
        {0.08f, 0.58f, 0.92f, 0.34f},
        {0.04f, 0.20f, 0.32f},
        {0.12f, 0.75f, 1.0f, 0.85f},
        0.22f,
        mesh.material_index_counts[0]);
    write_material(
        stream,
        u"\u98a8\u5411\u30fb\u5f37\u5ea6",
        u"Wind direction and strength",
        {0.96f, 0.30f, 0.06f, 0.92f},
        {0.36f, 0.08f, 0.02f},
        {0.38f, 0.05f, 0.01f, 1.0f},
        0.32f,
        mesh.material_index_counts[1]);
    write_material(
        stream,
        u"\u6e1b\u8870\u6838\u5fc3",
        u"Falloff core ratio",
        {1.0f, 0.78f, 0.05f, 0.96f},
        {0.34f, 0.22f, 0.01f},
        {0.38f, 0.24f, 0.0f, 1.0f},
        0.28f,
        mesh.material_index_counts[2]);

    write_value(stream, std::uint32_t{1});
    write_text(stream, u"\u98a8\u6e90\u5236\u5fa1");
    write_text(stream, u"WT_Source");
    write_vec3(stream, {0.0f, 0.0f, 0.0f});
    write_value(stream, std::int8_t{-1});
    write_value(stream, std::int32_t{0});
    write_value(stream, std::uint16_t{0x001e});
    write_vec3(stream, {3.0f, 0.0f, 0.0f});

    write_value(stream, std::uint32_t{3});
    write_vertex_morph(
        stream, u"\u98a8\u5834\u7bc4\u56f2", u"WT_Radius", mesh.radius_offsets);
    write_vertex_morph(
        stream, u"\u98a8\u529b\u500d\u7387", u"WT_Strength", mesh.strength_offsets);
    write_vertex_morph(
        stream, u"\u6e1b\u8870\u6838\u5fc3", u"WT_Falloff", mesh.falloff_offsets);

    write_value(stream, std::uint32_t{2});
    write_display_frame_header(stream, u"Root", u"Root", 1);
    write_value(stream, std::uint8_t{0});
    write_value(stream, std::int8_t{0});
    write_display_frame_header(stream, u"\u98a8\u529b\u5236\u5fa1", u"Wind Control", 3);
    for (std::uint8_t morph_index = 0; morph_index < 3; ++morph_index) {
        write_value(stream, std::uint8_t{1});
        write_value(stream, morph_index);
    }

    write_value(stream, std::uint32_t{0});
    write_value(stream, std::uint32_t{0});
    if (!stream.good()) {
        error = "failed while writing PMX data";
        return false;
    }
    return true;
}

class Reader {
public:
    explicit Reader(std::vector<std::uint8_t> data) : data_(std::move(data)) {}

    template <typename Value>
    bool read(Value& value) {
        static_assert(std::is_trivially_copyable_v<Value>);
        if (!take(&value, sizeof(value))) return false;
        return true;
    }

    bool read_text(std::u16string& text) {
        std::uint32_t byte_count = 0;
        if (!read(byte_count) || (byte_count % sizeof(char16_t)) != 0) return false;
        text.resize(byte_count / sizeof(char16_t));
        return take(text.data(), byte_count);
    }

    bool skip(std::size_t size) {
        if (size > data_.size() - offset_) return false;
        offset_ += size;
        return true;
    }

    std::size_t remaining() const noexcept { return data_.size() - offset_; }

private:
    bool take(void* destination, std::size_t size) {
        if (size > data_.size() - offset_) return false;
        if (size != 0) std::memcpy(destination, data_.data() + offset_, size);
        offset_ += size;
        return true;
    }

    std::vector<std::uint8_t> data_;
    std::size_t offset_ = 0;
};

bool skip_text(Reader& reader) {
    std::u16string text;
    return reader.read_text(text);
}

bool skip_material(Reader& reader, std::int64_t& surface_total) {
    if (!skip_text(reader) || !skip_text(reader) || !reader.skip(sizeof(float) * 4) ||
        !reader.skip(sizeof(float) * 3) || !reader.skip(sizeof(float)) ||
        !reader.skip(sizeof(float) * 3) || !reader.skip(sizeof(std::uint8_t)) ||
        !reader.skip(sizeof(float) * 4) || !reader.skip(sizeof(float)) ||
        !reader.skip(sizeof(std::int8_t) * 2) || !reader.skip(sizeof(std::uint8_t))) {
        return false;
    }
    std::uint8_t shared_toon = 0;
    if (!reader.read(shared_toon) || !reader.skip(sizeof(std::uint8_t)) ||
        shared_toon > 1 || !skip_text(reader)) {
        return false;
    }
    std::int32_t surface_count = 0;
    if (!reader.read(surface_count) || surface_count < 0) return false;
    surface_total += surface_count;
    return true;
}

bool verify_pmx(const std::filesystem::path& path, std::string& error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "cannot reopen generated PMX";
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff end = stream.tellg();
    if (end <= 0 || static_cast<std::uint64_t>(end) >
            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        error = "generated PMX has an invalid file size";
        return false;
    }
    stream.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(end));
    stream.read(reinterpret_cast<char*>(data.data()), end);
    if (!stream) {
        error = "cannot read generated PMX";
        return false;
    }
    Reader reader(std::move(data));
    std::array<char, 4> magic{};
    float version = 0.0f;
    std::uint8_t global_count = 0;
    std::array<std::uint8_t, 8> globals{};
    if (!reader.read(magic) || !reader.read(version) || !reader.read(global_count) ||
        global_count != globals.size() || !reader.read(globals) ||
        magic != std::array<char, 4>{{'P', 'M', 'X', ' '}} || version != 2.0f ||
        globals != std::array<std::uint8_t, 8>{{0, 0, 1, 1, 1, 1, 1, 1}}) {
        error = "generated PMX header does not match the MMD 9.31-compatible profile";
        return false;
    }
    std::u16string model_name;
    std::u16string model_english_name;
    std::u16string model_comment;
    std::u16string model_english_comment;
    if (!reader.read_text(model_name) || !reader.read_text(model_english_name) ||
        !reader.read_text(model_comment) || !reader.read_text(model_english_comment) ||
        model_name != u"WindTool \u98a8\u6e90\u5236\u5fa1\u5668" ||
        model_english_name != u"WindTool Wind Source Controller" ||
        model_comment != kAuthorComment || model_english_comment != kAuthorComment) {
        error = "generated PMX model text or UTF-16 encoding is invalid";
        return false;
    }

    std::uint32_t vertex_count = 0;
    if (!reader.read(vertex_count) || vertex_count == 0 || vertex_count > 255) {
        error = "generated PMX vertex count is invalid";
        return false;
    }
    for (std::uint32_t index = 0; index < vertex_count; ++index) {
        std::uint8_t deform = 0;
        if (!reader.skip(sizeof(float) * 8) || !reader.read(deform) || deform != 0 ||
            !reader.skip(sizeof(std::int8_t) + sizeof(float))) {
            error = "generated PMX vertex block is invalid";
            return false;
        }
    }

    std::uint32_t index_count = 0;
    if (!reader.read(index_count) || index_count == 0 || (index_count % 3) != 0 ||
        !reader.skip(index_count)) {
        error = "generated PMX surface block is invalid";
        return false;
    }
    std::uint32_t texture_count = 0;
    if (!reader.read(texture_count) || texture_count != 0) {
        error = "generated PMX unexpectedly references textures";
        return false;
    }
    std::uint32_t material_count = 0;
    std::int64_t surface_total = 0;
    if (!reader.read(material_count) || material_count != 3) {
        error = "generated PMX material count is invalid";
        return false;
    }
    for (std::uint32_t index = 0; index < material_count; ++index) {
        if (!skip_material(reader, surface_total)) {
            error = "generated PMX material block is invalid";
            return false;
        }
    }
    if (surface_total != index_count) {
        error = "generated PMX materials do not cover all surfaces";
        return false;
    }

    std::uint32_t bone_count = 0;
    std::u16string bone_name;
    std::u16string bone_english_name;
    std::uint16_t bone_flags = 0;
    if (!reader.read(bone_count) || bone_count != 1 || !reader.read_text(bone_name) ||
        !reader.read_text(bone_english_name) || bone_english_name != u"WT_Source" ||
        !reader.skip(sizeof(float) * 3 + sizeof(std::int8_t) + sizeof(std::int32_t)) ||
        !reader.read(bone_flags) || bone_flags != 0x001e ||
        !reader.skip(sizeof(float) * 3)) {
        error = "generated PMX controller bone is invalid";
        return false;
    }

    std::uint32_t morph_count = 0;
    if (!reader.read(morph_count) || morph_count != 3) {
        error = "generated PMX morph count is invalid";
        return false;
    }
    const std::array<std::u16string, 3> expected_morphs{{
        u"WT_Radius",
        u"WT_Strength",
        u"WT_Falloff"}};
    const std::array<std::u16string, 3> expected_local_morphs{{
        u"\u98a8\u5834\u7bc4\u56f2",
        u"\u98a8\u529b\u500d\u7387",
        u"\u6e1b\u8870\u6838\u5fc3"}};
    for (std::uint32_t morph_index = 0; morph_index < morph_count; ++morph_index) {
        std::u16string name;
        std::u16string runtime_name;
        std::uint8_t panel = 0;
        std::uint8_t type = 0;
        std::uint32_t offset_count = 0;
        if (!reader.read_text(name) || !reader.read_text(runtime_name) ||
            name != expected_local_morphs[morph_index] ||
            runtime_name != expected_morphs[morph_index] || !reader.read(panel) ||
            !reader.read(type) || panel != 4 || type != 1 ||
            !reader.read(offset_count) || offset_count == 0 ||
            !reader.skip(static_cast<std::size_t>(offset_count) *
                (sizeof(std::uint8_t) + sizeof(float) * 3))) {
            error = "generated PMX runtime morph block is invalid";
            return false;
        }
    }

    std::uint32_t frame_count = 0;
    if (!reader.read(frame_count) || frame_count != 2) {
        error = "generated PMX display frame count is invalid";
        return false;
    }
    for (std::uint32_t frame = 0; frame < frame_count; ++frame) {
        std::uint8_t special = 0;
        std::uint32_t element_count = 0;
        if (!skip_text(reader) || !skip_text(reader) || !reader.read(special) ||
            !reader.read(element_count) || special != 1) {
            error = "generated PMX display frame is invalid";
            return false;
        }
        for (std::uint32_t element = 0; element < element_count; ++element) {
            std::uint8_t type = 0;
            if (!reader.read(type) || type > 1 || !reader.skip(1)) {
                error = "generated PMX display frame element is invalid";
                return false;
            }
        }
    }
    std::uint32_t rigid_body_count = 1;
    std::uint32_t joint_count = 1;
    if (!reader.read(rigid_body_count) || rigid_body_count != 0 ||
        !reader.read(joint_count) || joint_count != 0 || reader.remaining() != 0) {
        error = "generated PMX has an invalid physics footer";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: generate_wind_source_pmx <output.pmx>\n";
        return 2;
    }
    const Mesh mesh = make_mesh();
    std::string error;
    const std::filesystem::path path(argv[1]);
    if (!write_pmx(path, mesh, error) || !verify_pmx(path, error)) {
        std::cerr << "failed to generate WindTool PMX: " << error << '\n';
        return 3;
    }
    std::cout << "generated " << path.string() << " vertices=" << mesh.vertices.size()
              << " triangles=" << mesh.indices.size() / 3
              << " morphs=3 verified=1\n";
    return 0;
}
