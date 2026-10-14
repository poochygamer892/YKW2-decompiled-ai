#pragma once

#include <cstdint>
#include <string_view>

#include "ykw2/core/import_contract.hpp"
#include "ykw2/core/resource_source.hpp"

namespace ykw2::core {

struct NcchHeader {
    std::uint32_t content_size_units = 0;
    std::uint64_t content_size_bytes = 0;
    std::uint32_t romfs_offset_units = 0;
    std::uint32_t romfs_size_units = 0;
    std::uint64_t romfs_offset = 0;
    std::uint64_t romfs_size = 0;
    bool no_romfs = false;
    bool no_crypto = false;
};

ParseResult<NcchHeader> parse_ncch_header(ByteView first_header,
                                          std::uint64_t partition_size);

class ResourceSink {
public:
    virtual ~ResourceSink() = default;

    virtual bool begin_file(std::string_view path, std::uint64_t size) = 0;
    virtual bool write(const std::uint8_t* data, std::size_t size) = 0;
    virtual bool end_file() = 0;
    virtual void abort_file() {}
};

struct RomFsExtractOptions {
    std::uint64_t max_files = 0;
    std::uint64_t max_total_bytes = 0;
};

struct RomFsExtractStats {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
};

ParseResult<RomFsExtractStats> extract_romfs(
    const ResourceSource& source,
    std::uint64_t romfs_offset,
    std::uint64_t romfs_size,
    ResourceSink& sink,
    RomFsExtractOptions options = {});

ParseResult<RomFsExtractStats> extract_application_romfs(
    const ResourceSource& source,
    const ImportPlan& plan,
    ResourceSink& sink,
    RomFsExtractOptions options = {});

}  // namespace ykw2::core
