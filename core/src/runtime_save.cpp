#include "ykw2/core/runtime_save.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>

namespace ykw2::core {
namespace {

constexpr std::string_view kMagic = "YKW2SV01";
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kMaximumPathBytes = 4096;

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }
}

void append_u64(std::vector<std::uint8_t>& output, std::uint64_t value) {
    for (unsigned int shift = 0; shift < 64; shift += 8) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }
}

void append_float(std::vector<std::uint8_t>& output, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(output, bits);
}

void append_double(std::vector<std::uint8_t>& output, double value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u64(output, bits);
}

bool read_u32(const std::vector<std::uint8_t>& bytes, std::size_t& offset,
              std::uint32_t& value) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    offset += 4;
    return true;
}

bool read_u64(const std::vector<std::uint8_t>& bytes, std::size_t& offset,
              std::uint64_t& value) {
    if (offset > bytes.size() || bytes.size() - offset < 8) {
        return false;
    }
    value = 0;
    for (unsigned int shift = 0; shift < 64; shift += 8) {
        value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
    }
    return true;
}

bool read_float(const std::vector<std::uint8_t>& bytes, std::size_t& offset, float& value) {
    std::uint32_t bits = 0;
    if (!read_u32(bytes, offset, bits)) {
        return false;
    }
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool read_double(const std::vector<std::uint8_t>& bytes, std::size_t& offset, double& value) {
    std::uint64_t bits = 0;
    if (!read_u64(bytes, offset, bits)) {
        return false;
    }
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool valid_path(std::string_view path) {
    if (path.empty() || path.size() > kMaximumPathBytes || path.front() == '/' ||
        path.find('\\') != std::string_view::npos || path.find('\0') != std::string_view::npos) {
        return false;
    }
    std::size_t component_start = 0;
    while (component_start <= path.size()) {
        const auto separator = path.find('/', component_start);
        const auto component_end = separator == std::string_view::npos ? path.size() : separator;
        const auto component = path.substr(component_start, component_end - component_start);
        if (component == ".." || component.empty()) {
            return false;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        component_start = separator + 1;
    }
    return true;
}

}  // namespace

std::vector<std::uint8_t> serialize_runtime_save(const RuntimeSaveState& state) {
    const auto path = std::string_view(state.map_resource);
    if (!valid_path(path) || path.size() > std::numeric_limits<std::uint32_t>::max()) {
        return {};
    }

    std::vector<std::uint8_t> output;
    output.reserve(kMagic.size() + 40 + path.size());
    output.insert(output.end(), kMagic.begin(), kMagic.end());
    append_u32(output, kVersion);
    output.push_back(state.active ? 1 : 0);
    output.insert(output.end(), 3, 0);
    append_u32(output, static_cast<std::uint32_t>(path.size()));
    append_float(output, state.player_x);
    append_float(output, state.player_y);
    append_double(output, state.simulation_seconds);
    append_u64(output, state.frame);
    output.insert(output.end(), path.begin(), path.end());
    return output;
}

std::optional<RuntimeSaveState> parse_runtime_save(const std::vector<std::uint8_t>& bytes) {
    constexpr std::size_t kFixedHeaderSize = 8 + 4 + 4 + 4 + 4 + 8 + 8;
    if (bytes.size() < kFixedHeaderSize ||
        !std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
        return std::nullopt;
    }

    std::size_t offset = kMagic.size();
    std::uint32_t version = 0;
    if (!read_u32(bytes, offset, version) || version != kVersion || offset + 4 > bytes.size()) {
        return std::nullopt;
    }
    const bool active = bytes[offset] != 0;
    offset += 4;

    std::uint32_t path_size = 0;
    if (!read_u32(bytes, offset, path_size) || path_size > kMaximumPathBytes ||
        offset > bytes.size() || bytes.size() - offset != path_size + 4 + 4 + 8 + 8) {
        return std::nullopt;
    }

    RuntimeSaveState state;
    state.active = active;
    if (!read_float(bytes, offset, state.player_x) ||
        !read_float(bytes, offset, state.player_y) ||
        !read_double(bytes, offset, state.simulation_seconds) ||
        !read_u64(bytes, offset, state.frame)) {
        return std::nullopt;
    }
    state.map_resource.assign(reinterpret_cast<const char*>(bytes.data() + offset), path_size);
    if (!valid_path(state.map_resource)) {
        return std::nullopt;
    }
    return state;
}

}  // namespace ykw2::core
