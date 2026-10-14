#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "ykw2/core/byte_view.hpp"

namespace ykw2::core {

struct NcsdPartition {
    std::uint32_t index = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

struct NcsdHeader {
    std::uint32_t media_size_units = 0;
    std::uint64_t media_size_bytes = 0;
    std::array<NcsdPartition, 8> partitions{};
};

// The view must contain the first 0x200 bytes of the image. The NCSD magic is
// at 0x100 and all partition offsets/sizes are measured in 0x200-byte units.
ParseResult<NcsdHeader> parse_ncsd_header(ByteView first_header,
                                          std::uint64_t image_size);

struct ImportPlan {
    std::uint64_t image_size = 0;
    NcsdPartition application_partition{};
    std::vector<NcsdPartition> active_partitions;
    bool rights_confirmed = false;
};

// Creates a bounded, read-only plan. Actual extraction is performed by a
// platform adapter that can stream from the user-selected document into the
// app-private cache; this function never retains the source image.
ParseResult<ImportPlan> build_import_plan(ByteView first_header,
                                          std::uint64_t image_size,
                                          bool rights_confirmed);

}  // namespace ykw2::core
