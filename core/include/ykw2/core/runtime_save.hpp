#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ykw2::core {

struct RuntimeSaveState {
    bool active = false;
    std::string map_resource;
    float player_x = 0.0f;
    float player_y = 0.0f;
    double simulation_seconds = 0.0;
    std::uint64_t frame = 0;
};

std::vector<std::uint8_t> serialize_runtime_save(const RuntimeSaveState& state);
std::optional<RuntimeSaveState> parse_runtime_save(const std::vector<std::uint8_t>& bytes);

}  // namespace ykw2::core
