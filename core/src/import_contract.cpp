#include "ykw2/core/import_contract.hpp"

#include <limits>
#include <utility>

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset) {
    return ParseResult<T>::failure(code, offset);
}

bool media_region(std::uint32_t offset_units, std::uint32_t size_units,
                  std::uint64_t image_size, std::uint64_t& offset,
                  std::uint64_t& size) {
    constexpr std::uint64_t kMediaUnit = 0x200;
    offset = static_cast<std::uint64_t>(offset_units) * kMediaUnit;
    size = static_cast<std::uint64_t>(size_units) * kMediaUnit;
    return offset <= image_size && size <= image_size - offset;
}

}  // namespace

ParseResult<NcsdHeader> parse_ncsd_header(ByteView first_header,
                                          std::uint64_t image_size) {
    if (image_size < 0x200) {
        return failure<NcsdHeader>("ncsd_image_too_small", 0);
    }
    if (first_header.size() < 0x200) {
        return failure<NcsdHeader>("truncated_ncsd_header", first_header.size());
    }
    if (!first_header.subview(0x100, 4).starts_with("NCSD")) {
        return failure<NcsdHeader>("invalid_ncsd_magic", 0x100);
    }

    const auto media_units = first_header.u32_le(0x104);
    if (!media_units || *media_units == 0) {
        return failure<NcsdHeader>("invalid_ncsd_media_size", 0x104);
    }
    const auto media_size = static_cast<std::uint64_t>(*media_units) * 0x200u;
    if (media_size > image_size) {
        return failure<NcsdHeader>("ncsd_media_size_exceeds_image", 0x104);
    }

    NcsdHeader header;
    header.media_size_units = *media_units;
    header.media_size_bytes = media_size;
    for (std::size_t index = 0; index < header.partitions.size(); ++index) {
        const auto offset_units = first_header.u32_le(0x120 + index * 8);
        const auto size_units = first_header.u32_le(0x124 + index * 8);
        if (!offset_units || !size_units) {
            return failure<NcsdHeader>("truncated_ncsd_partition_table", 0x120 + index * 8);
        }
        NcsdPartition partition;
        partition.index = static_cast<std::uint32_t>(index);
        if (!media_region(*offset_units, *size_units, image_size,
                          partition.offset, partition.size)) {
            return failure<NcsdHeader>("ncsd_partition_out_of_bounds", 0x120 + index * 8);
        }
        header.partitions[index] = partition;
    }
    return ParseResult<NcsdHeader>::success(header);
}

ParseResult<ImportPlan> build_import_plan(ByteView first_header,
                                          std::uint64_t image_size,
                                          bool rights_confirmed) {
    if (!rights_confirmed) {
        return failure<ImportPlan>("rights_confirmation_required", 0);
    }
    const auto parsed = parse_ncsd_header(first_header, image_size);
    if (!parsed) {
        return failure<ImportPlan>(parsed.error->code, parsed.error->offset);
    }

    ImportPlan plan;
    plan.image_size = image_size;
    plan.rights_confirmed = true;
    for (const auto& partition : parsed.value->partitions) {
        if (partition.size == 0) {
            continue;
        }
        plan.active_partitions.push_back(partition);
    }
    if (plan.active_partitions.empty()) {
        return failure<ImportPlan>("ncsd_has_no_active_partitions", 0x120);
    }
    plan.application_partition = plan.active_partitions.front();
    return ParseResult<ImportPlan>::success(std::move(plan));
}

}  // namespace ykw2::core
