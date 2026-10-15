#include "ykw2/core/cfg_reader.hpp"

#include <array>

namespace ykw2::core {

ParseResult<CfgBinaryInfo> parse_cfg_binary_header(ByteView bytes,
                                                   std::uint64_t file_size) {
    constexpr std::size_t kHeaderBytes = 0x10;
    if (file_size < kHeaderBytes || !bytes.contains(0, kHeaderBytes)) {
        return ParseResult<CfgBinaryInfo>::failure("cfg_header_truncated", 0);
    }
    const auto field_0x00 = bytes.u32_le(0x00);
    const auto field_0x04 = bytes.u32_le(0x04);
    const auto field_0x08 = bytes.u32_le(0x08);
    const auto field_0x0c = bytes.u32_le(0x0c);
    if (!field_0x00 || !field_0x04 || !field_0x08 || !field_0x0c) {
        return ParseResult<CfgBinaryInfo>::failure("cfg_header_read_failed", 0);
    }
    return ParseResult<CfgBinaryInfo>::success(CfgBinaryInfo{
        file_size, *field_0x00, *field_0x04, *field_0x08, *field_0x0c,
        static_cast<std::uint32_t>(kHeaderBytes)});
}

ParseResult<CfgBinaryInfo> read_cfg_binary_info(const ResourceStore& store,
                                                std::string_view path) {
    const auto* entry = store.catalog().find(path);
    if (entry == nullptr) {
        return ParseResult<CfgBinaryInfo>::failure("cfg_not_in_catalog", 0);
    }
    constexpr std::size_t kHeaderBytes = 0x10;
    std::array<std::uint8_t, kHeaderBytes> header{};
    if (!store.read_exact(path, 0, header.data(), header.size())) {
        return ParseResult<CfgBinaryInfo>::failure("cfg_header_read_failed", 0);
    }
    return parse_cfg_binary_header(ByteView(header.data(), header.size()), entry->size);
}

}  // namespace ykw2::core
