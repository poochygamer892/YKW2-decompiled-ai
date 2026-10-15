#include "ykw2/core/model_reader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "ykw2/core/level5_decode.hpp"

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset = 0) {
    return ParseResult<T>::failure(code, offset);
}

bool range_ok(ByteView bytes, std::uint64_t offset, std::uint64_t size) {
    return offset <= bytes.size() && size <= bytes.size() - offset;
}

std::optional<float> float_le(ByteView bytes, std::size_t offset) {
    const auto raw = bytes.u32_le(offset);
    if (!raw) {
        return std::nullopt;
    }
    float value = 0.0f;
    const auto word = *raw;
    std::memcpy(&value, &word, sizeof(value));
    return std::isfinite(value) ? std::optional<float>(value) : std::nullopt;
}

ParseResult<std::vector<std::uint8_t>> decode_region(
    ByteView bytes, std::size_t offset, std::size_t size,
    std::uint64_t max_output, std::string_view code) {
    if (!bytes.contains(offset, size)) {
        return failure<std::vector<std::uint8_t>>(code, offset);
    }
    const auto decoded = decode_level5(
        bytes.subview(offset, size), Level5DecodeOptions{max_output});
    if (decoded) {
        return decoded;
    }
    // Raw substreams occur in small test/dev meshes. Only accept a raw stream
    // when it does not claim an invalid Level-5 output size.
    std::vector<std::uint8_t> raw(size);
    if (size != 0) {
        std::copy_n(bytes.data() + offset, size, raw.data());
    }
    return ParseResult<std::vector<std::uint8_t>>::success(std::move(raw));
}

ParseResult<std::vector<std::uint8_t>> parse_xpvb(
    ByteView bytes, std::uint32_t& vertex_count, ModelGeometryInfo& result,
    const ModelReadOptions& options) {
    if (bytes.size() < 0x14 || !bytes.starts_with("XPVB")) {
        return failure<std::vector<std::uint8_t>>("xppr_invalid_xpvb", 0);
    }
    const auto attributes_offset = bytes.u16_le(0x04);
    const auto fixed_offset = bytes.u16_le(0x06);
    const auto vertices_offset = bytes.u16_le(0x08);
    const auto stride = bytes.u16_le(0x0a);
    const auto count = bytes.u32_le(0x0c);
    if (!attributes_offset || !fixed_offset || !vertices_offset || !stride || !count ||
        *stride == 0 || *count > options.max_vertices ||
        *attributes_offset > *fixed_offset || *fixed_offset > *vertices_offset ||
        *vertices_offset > bytes.size()) {
        return failure<std::vector<std::uint8_t>>("xppr_invalid_xpvb_layout", 0x04);
    }
    const auto attributes = decode_region(
        bytes, *attributes_offset, *fixed_offset - *attributes_offset,
        options.max_decoded_bytes, "xppr_xpvb_attributes_out_of_bounds");
    if (!attributes || attributes.value->size() < 40) {
        return failure<std::vector<std::uint8_t>>("xppr_xpvb_attributes_invalid", *attributes_offset);
    }
    std::array<std::uint8_t, 10> count_by_slot{};
    std::array<std::uint8_t, 10> offset_by_slot{};
    std::array<std::uint8_t, 10> type_by_slot{};
    for (std::size_t slot = 0; slot < 10; ++slot) {
        count_by_slot[slot] = (*attributes.value)[slot * 4];
        offset_by_slot[slot] = (*attributes.value)[slot * 4 + 1];
        type_by_slot[slot] = (*attributes.value)[slot * 4 + 3];
    }
    const auto fixed = decode_region(
        bytes, *fixed_offset, *vertices_offset - *fixed_offset,
        options.max_decoded_bytes, "xppr_xpvb_fixed_out_of_bounds");
    if (!fixed) {
        return failure<std::vector<std::uint8_t>>(fixed.error->code, fixed.error->offset);
    }
    const auto vertex_bytes = decode_region(
        bytes, *vertices_offset, bytes.size() - *vertices_offset,
        options.max_decoded_bytes, "xppr_xpvb_vertices_out_of_bounds");
    if (!vertex_bytes) {
        return failure<std::vector<std::uint8_t>>(
            vertex_bytes.error->code, vertex_bytes.error->offset);
    }
    if (count_by_slot[0] == 0 || type_by_slot[0] != 2 || count_by_slot[0] < 3 ||
        static_cast<std::uint64_t>(*stride) * *count > vertex_bytes.value->size()) {
        return failure<std::vector<std::uint8_t>>("xppr_xpvb_position_attribute_missing", 0);
    }
    vertex_count = *count;
    result.vertex_count = *count;
    const auto preview_count = std::min<std::size_t>(*count, options.preview_vertices);
    result.preview_positions.reserve(preview_count * 3);
    for (std::size_t vertex = 0; vertex < preview_count; ++vertex) {
        const auto base = vertex * *stride + offset_by_slot[0];
        if (base > vertex_bytes.value->size() ||
            vertex_bytes.value->size() - base < 12) {
            return failure<std::vector<std::uint8_t>>("xppr_xpvb_position_out_of_bounds", base);
        }
        for (std::size_t component = 0; component < 3; ++component) {
            const auto value = float_le(
                ByteView(vertex_bytes.value->data(), vertex_bytes.value->size()),
                base + component * 4);
            if (!value) {
                return failure<std::vector<std::uint8_t>>("xppr_xpvb_position_invalid", base);
            }
            result.preview_positions.push_back(*value);
        }
    }
    return ParseResult<std::vector<std::uint8_t>>::success({});
}

}  // namespace

ParseResult<ModelGeometryInfo> parse_xmpr_model(ByteView bytes, ModelReadOptions options) {
    if (bytes.size() < 0x54 || !bytes.starts_with("XMPR")) {
        return failure<ModelGeometryInfo>("xmpr_invalid_header", 0);
    }
    const auto xprm_length = bytes.u32_le(0x08);
    const auto properties_offset = bytes.u32_le(0x0c);
    const auto nodes_offset = bytes.u32_le(0x28);
    const auto nodes_length = bytes.u32_le(0x2c);
    const auto xprm_offset = static_cast<std::size_t>(bytes.u32_le(0x04).value_or(0));
    if (!xprm_length || !properties_offset || !nodes_offset || !nodes_length ||
        !range_ok(bytes, *properties_offset, 0x34) ||
        !range_ok(bytes, xprm_offset, *xprm_length) ||
        *xprm_length < 20) {
        return failure<ModelGeometryInfo>("xmpr_invalid_layout", 0x04);
    }
    const auto material = bytes.u32_le(*properties_offset + 4);
    if (!material) {
        return failure<ModelGeometryInfo>("xmpr_properties_truncated", *properties_offset);
    }
    const auto xpvb_offset = bytes.u32_le(xprm_offset + 4);
    const auto xpvb_length = bytes.u32_le(xprm_offset + 8);
    const auto xpvi_offset = bytes.u32_le(xprm_offset + 12);
    const auto xpvi_length = bytes.u32_le(xprm_offset + 16);
    if (!xpvb_offset || !xpvb_length || !xpvi_offset || !xpvi_length ||
        !range_ok(bytes, xprm_offset + *xpvb_offset, *xpvb_length) ||
        !range_ok(bytes, xprm_offset + *xpvi_offset, *xpvi_length) ||
        *xpvb_length > options.max_decoded_bytes || *xpvi_length > options.max_decoded_bytes) {
        return failure<ModelGeometryInfo>("xmpr_buffer_out_of_bounds", xprm_offset);
    }
    ModelGeometryInfo result;
    result.material_hash = *material;
    std::uint32_t vertex_count = 0;
    const auto vertices = parse_xpvb(
        bytes.subview(xprm_offset + *xpvb_offset, *xpvb_length),
        vertex_count, result, options);
    if (!vertices) {
        return failure<ModelGeometryInfo>(vertices.error->code, vertices.error->offset);
    }
    const auto indices_bytes = bytes.subview(xprm_offset + *xpvi_offset, *xpvi_length);
    if (indices_bytes.size() < 12 || !indices_bytes.starts_with("XPVI")) {
        return failure<ModelGeometryInfo>("xppr_invalid_xpvi", xprm_offset + *xpvi_offset);
    }
    const auto primitive = indices_bytes.u16_le(4);
    const auto faces_offset = indices_bytes.u16_le(6);
    const auto face_count = indices_bytes.u32_le(8);
    if (!primitive || !faces_offset || !face_count || *face_count > options.max_indices ||
        *faces_offset > indices_bytes.size()) {
        return failure<ModelGeometryInfo>("xppr_invalid_xpvi_layout", 4);
    }
    const auto faces = decode_region(
        indices_bytes, *faces_offset, indices_bytes.size() - *faces_offset,
        options.max_decoded_bytes, "xppr_xpvi_faces_out_of_bounds");
    if (!faces) {
        return failure<ModelGeometryInfo>(faces.error->code, faces.error->offset);
    }
    if (static_cast<std::uint64_t>(*face_count) * 2 > faces.value->size()) {
        return failure<ModelGeometryInfo>("xppr_xpvi_faces_truncated", *faces_offset);
    }
    result.primitive_type = *primitive;
    const auto index_count = std::min<std::size_t>(*face_count, options.preview_indices);
    std::vector<std::uint16_t> raw_indices;
    raw_indices.reserve(index_count);
    for (std::size_t i = 0; i < index_count; ++i) {
        const auto low = (*faces.value)[i * 2];
        const auto high = (*faces.value)[i * 2 + 1];
        const auto index = static_cast<std::uint16_t>(low | (high << 8));
        if (index >= vertex_count) {
            return failure<ModelGeometryInfo>("xppr_xpvi_vertex_index_out_of_bounds", i * 2);
        }
        raw_indices.push_back(index);
    }
    if (*primitive == 0) {
        result.preview_indices = std::move(raw_indices);
    } else if (*primitive == 2) {
        bool flip = false;
        for (std::size_t i = 0; i + 2 < raw_indices.size(); ++i) {
            const auto a = raw_indices[i];
            const auto b = raw_indices[i + 1];
            const auto c = raw_indices[i + 2];
            if (a != b && b != c && c != a) {
                if (flip) {
                    result.preview_indices.insert(result.preview_indices.end(), {b, a, c});
                } else {
                    result.preview_indices.insert(result.preview_indices.end(), {a, b, c});
                }
            }
            flip = !flip;
        }
    } else {
        return failure<ModelGeometryInfo>("xppr_unsupported_xpvi_primitive", 4);
    }
    result.triangle_index_count = *primitive == 2
                                      ? static_cast<std::uint32_t>(*face_count >= 2 ? *face_count - 2 : 0)
                                      : *face_count;
    return ParseResult<ModelGeometryInfo>::success(std::move(result));
}

}  // namespace ykw2::core
