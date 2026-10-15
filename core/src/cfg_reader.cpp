#include "ykw2/core/cfg_reader.hpp"

#include <array>
#include <limits>
#include <utility>

namespace ykw2::core {
namespace {

constexpr std::size_t kHeaderBytes = 0x10;

std::size_t align4(std::size_t value) {
    if (value > std::numeric_limits<std::size_t>::max() - 3) {
        return std::numeric_limits<std::size_t>::max();
    }
    return (value + 3) & ~static_cast<std::size_t>(3);
}

ParseResult<CfgBinary> cfg_failure(std::string_view code, std::size_t offset) {
    return ParseResult<CfgBinary>::failure(code, offset);
}

}  // namespace

ParseResult<CfgBinaryInfo> parse_cfg_binary_header(ByteView bytes,
                                                   std::uint64_t file_size) {
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

ParseResult<CfgBinary> parse_cfg_binary(ByteView bytes, std::uint64_t file_size,
                                        CfgReadOptions options) {
    if (file_size > options.max_file_bytes) {
        return cfg_failure("cfg_file_too_large", 0);
    }
    if (file_size > bytes.size()) {
        return cfg_failure("cfg_payload_truncated", bytes.size());
    }
    const auto parsed_header = parse_cfg_binary_header(bytes, file_size);
    if (!parsed_header) {
        return cfg_failure(parsed_header.error->code, parsed_header.error->offset);
    }
    const auto& header = *parsed_header.value;
    if (header.field_0x00 > options.max_entries) {
        return cfg_failure("cfg_entry_count_too_large", 0);
    }
    if (header.field_0x04 > file_size ||
        header.field_0x08 > file_size - header.field_0x04 ||
        header.field_0x08 > options.max_string_bytes) {
        return cfg_failure("cfg_string_table_out_of_bounds", 0x04);
    }

    CfgBinary result;
    result.header = header;
    result.entries.reserve(header.field_0x00);
    std::size_t cursor = kHeaderBytes;
    for (std::uint32_t entry_index = 0; entry_index < header.field_0x00;
         ++entry_index) {
        if (!bytes.contains(cursor, 5)) {
            return cfg_failure("cfg_entry_header_truncated", cursor);
        }
        const auto hash = bytes.u32_le(cursor);
        const auto value_count = bytes.u8(cursor + 4);
        if (!hash || !value_count) {
            return cfg_failure("cfg_entry_header_read_failed", cursor);
        }
        const auto type_bytes = (static_cast<std::size_t>(*value_count) + 3) / 4;
        const auto type_offset = cursor + 5;
        if (type_offset < cursor || !bytes.contains(type_offset, type_bytes)) {
            return cfg_failure("cfg_entry_types_truncated", type_offset);
        }
        const auto value_offset = align4(type_offset + type_bytes);
        if (value_offset < type_offset ||
            static_cast<std::size_t>(*value_count) >
                (std::numeric_limits<std::size_t>::max() - value_offset) / 4) {
            return cfg_failure("cfg_entry_values_overflow", value_offset);
        }
        const auto value_bytes = static_cast<std::size_t>(*value_count) * 4;
        if (!bytes.contains(value_offset, value_bytes)) {
            return cfg_failure("cfg_entry_values_truncated", value_offset);
        }
        if (header.field_0x04 != 0 && value_offset + value_bytes > header.field_0x04) {
            return cfg_failure("cfg_entries_overlap_strings", value_offset);
        }

        CfgEntry entry;
        entry.hash = *hash;
        entry.values.reserve(*value_count);
        for (std::size_t value_index = 0; value_index < *value_count;
             ++value_index) {
            const auto type_byte = bytes.u8(type_offset + value_index / 4);
            if (!type_byte) {
                return cfg_failure("cfg_entry_type_read_failed", type_offset);
            }
            const auto type_value = static_cast<std::uint8_t>(
                (*type_byte >> ((value_index % 4) * 2)) & 0x3);
            if (type_value > static_cast<std::uint8_t>(CfgValueType::Float)) {
                return cfg_failure("cfg_value_type_unknown",
                                   type_offset + value_index / 4);
            }
            const auto raw = bytes.u32_le(value_offset + value_index * 4);
            if (!raw) {
                return cfg_failure("cfg_entry_value_read_failed",
                                   value_offset + value_index * 4);
            }
            CfgValue value;
            value.type = static_cast<CfgValueType>(type_value);
            value.raw_value = *raw;
            if (value.type == CfgValueType::String && *raw != 0xffffffffu) {
                if (*raw >= header.field_0x08) {
                    return cfg_failure("cfg_string_offset_out_of_bounds",
                                       value_offset + value_index * 4);
                }
                const auto string_begin = static_cast<std::size_t>(header.field_0x04) + *raw;
                const auto string_end = static_cast<std::size_t>(header.field_0x04) +
                                        header.field_0x08;
                const auto available = bytes.subview(string_begin, string_end - string_begin);
                const auto* data = available.data();
                std::size_t length = 0;
                while (length < available.size() && data[length] != 0) {
                    ++length;
                }
                if (length == available.size()) {
                    return cfg_failure("cfg_string_not_terminated", string_begin);
                }
                value.string_bytes.assign(reinterpret_cast<const char*>(data), length);
            }
            entry.values.push_back(std::move(value));
        }
        result.entries.push_back(std::move(entry));
        cursor = value_offset + value_bytes;
    }
    return ParseResult<CfgBinary>::success(std::move(result));
}

ParseResult<CfgBinaryInfo> read_cfg_binary_info(const ResourceStore& store,
                                                std::string_view path) {
    const auto* entry = store.catalog().find(path);
    if (entry == nullptr) {
        return ParseResult<CfgBinaryInfo>::failure("cfg_not_in_catalog", 0);
    }
    std::array<std::uint8_t, kHeaderBytes> header{};
    if (!store.read_exact(path, 0, header.data(), header.size())) {
        return ParseResult<CfgBinaryInfo>::failure("cfg_header_read_failed", 0);
    }
    return parse_cfg_binary_header(ByteView(header.data(), header.size()), entry->size);
}

ParseResult<CfgBinary> read_cfg_binary(const ResourceStore& store,
                                       std::string_view path,
                                       CfgReadOptions options) {
    const auto* entry = store.catalog().find(path);
    if (entry == nullptr) {
        return ParseResult<CfgBinary>::failure("cfg_not_in_catalog", 0);
    }
    if (entry->size > options.max_file_bytes || entry->size >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return ParseResult<CfgBinary>::failure("cfg_file_too_large", 0);
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry->size));
    if (!store.read_exact(path, 0, bytes.data(), bytes.size())) {
        return ParseResult<CfgBinary>::failure("cfg_payload_read_failed", 0);
    }
    return parse_cfg_binary(ByteView(bytes.data(), bytes.size()), entry->size, options);
}

}  // namespace ykw2::core
