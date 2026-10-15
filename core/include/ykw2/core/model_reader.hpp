#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "ykw2/core/byte_view.hpp"

namespace ykw2::core {

// Bounded geometry extracted from one Level-5 XMPR mesh. The preview arrays
// are intentionally capped: the mobile scene only needs a small wireframe
// sample while the full mesh remains in the imported private cache.
struct ModelGeometryInfo {
    std::uint32_t vertex_count = 0;
    std::uint32_t triangle_index_count = 0;
    std::uint16_t primitive_type = 0;
    std::uint32_t material_hash = 0;
    std::vector<float> preview_positions;
    std::vector<std::uint16_t> preview_indices;
};

struct ModelReadOptions {
    std::uint32_t max_vertices = 1u * 1024u * 1024u;
    std::uint32_t max_indices = 3u * 1024u * 1024u;
    std::size_t preview_vertices = 256;
    std::size_t preview_indices = 768;
    std::uint64_t max_decoded_bytes = 64ull * 1024ull * 1024ull;
};

ParseResult<ModelGeometryInfo> parse_xmpr_model(
    ByteView bytes, ModelReadOptions options = {});

}  // namespace ykw2::core
