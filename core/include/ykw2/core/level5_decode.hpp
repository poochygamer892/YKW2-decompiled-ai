#pragma once

#include <cstdint>
#include <vector>

#include "ykw2/core/byte_view.hpp"

namespace ykw2::core {

struct Level5DecodeOptions {
    // Prevent a corrupt header from forcing an unbounded allocation.
    std::uint64_t max_output_size = 256ull * 1024ull * 1024ull;
};

ParseResult<std::vector<std::uint8_t>> decode_level5(
    ByteView compressed,
    Level5DecodeOptions options = {});

}  // namespace ykw2::core
