#include "ykw2/core/xpck_reader.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <utility>

#include "ykw2/core/level5_decode.hpp"

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset) {
    return ParseResult<T>::failure(code, offset);
}

ParseResult<std::vector<std::uint8_t>> read_region(
    const ResourceStore& store, std::string_view path, std::uint64_t offset,
    std::uint64_t size, std::uint64_t archive_size,
    std::uint64_t max_size, std::string_view error_code) {
    if (offset > archive_size || size > archive_size - offset ||
        size > max_size || size > std::numeric_limits<std::size_t>::max()) {
        return failure<std::vector<std::uint8_t>>(error_code,
                                                  static_cast<std::size_t>(offset));
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!store.read_exact(path, offset, bytes.data(), bytes.size())) {
        return failure<std::vector<std::uint8_t>>("xpck_table_read_failed",
                                                  static_cast<std::size_t>(offset));
    }
    return ParseResult<std::vector<std::uint8_t>>::success(std::move(bytes));
}

ParseResult<std::string> read_c_string(ByteView table, std::uint64_t offset) {
    if (offset >= table.size()) {
        return failure<std::string>("xpck_name_offset_out_of_bounds",
                                    static_cast<std::size_t>(offset));
    }
    const auto start = static_cast<std::size_t>(offset);
    std::size_t end = start;
    while (end < table.size() && table.data()[end] != 0) {
        ++end;
    }
    if (end == table.size()) {
        return failure<std::string>("xpck_unterminated_name", start);
    }
    return ParseResult<std::string>::success(
        std::string(reinterpret_cast<const char*>(table.data() + start), end - start));
}

}  // namespace

const XpckFile* XpckArchive::find(std::string_view name) const {
    const auto found = std::find_if(
        files.begin(), files.end(),
        [name](const XpckFile& file) { return file.name == name; });
    return found == files.end() ? nullptr : &*found;
}

ParseResult<XpckArchive> read_xpck_archive(
    const ResourceStore& store, std::string_view path, XpckReadOptions options) {
    const auto catalog_entry = store.catalog().find(path);
    if (catalog_entry == nullptr) {
        return failure<XpckArchive>("resource_not_in_catalog", 0);
    }
    if (catalog_entry->size < 0x14) {
        return failure<XpckArchive>("xpck_header_truncated", 0);
    }
    std::vector<std::uint8_t> header_bytes(0x14);
    if (!store.read_exact(path, 0, header_bytes.data(), header_bytes.size())) {
        return failure<XpckArchive>("xpck_header_read_failed", 0);
    }
    const auto parsed_header = parse_xpck_header(
        ByteView(header_bytes.data(), header_bytes.size()), catalog_entry->size);
    if (!parsed_header) {
        return failure<XpckArchive>(parsed_header.error->code, parsed_header.error->offset);
    }
    const auto& header = *parsed_header.value;
    if (header.entry_count > options.max_entries) {
        return failure<XpckArchive>("xpck_entry_limit_exceeded", 0x04);
    }

    const auto info = read_region(
        store, path, header.info_offset,
        static_cast<std::uint64_t>(header.entry_count) * 12u,
        catalog_entry->size, options.max_compressed_table_bytes,
        "xpck_info_out_of_bounds");
    if (!info) {
        return failure<XpckArchive>(info.error->code, info.error->offset);
    }
    const auto compressed_names = read_region(
        store, path, header.name_offset, header.name_size,
        catalog_entry->size, options.max_compressed_table_bytes,
        "xpck_names_out_of_bounds");
    if (!compressed_names) {
        return failure<XpckArchive>(compressed_names.error->code,
                                    compressed_names.error->offset);
    }
    const auto names = decode_level5(
        ByteView(compressed_names.value->data(), compressed_names.value->size()),
        Level5DecodeOptions{options.max_decoded_name_bytes});
    if (!names) {
        return failure<XpckArchive>(names.error->code, names.error->offset);
    }

    XpckArchive archive;
    archive.header = header;
    archive.files.reserve(header.entry_count);
    std::unordered_set<std::string> names_seen;
    for (std::size_t index = 0; index < header.entry_count; ++index) {
        const auto parsed_entry = parse_xpck_entry(
            ByteView(info.value->data(), info.value->size()), index);
        if (!parsed_entry) {
            return failure<XpckArchive>(parsed_entry.error->code, parsed_entry.error->offset);
        }
        const auto& entry = *parsed_entry.value;
        const auto name = read_c_string(
            ByteView(names.value->data(), names.value->size()), entry.name_offset);
        if (!name) {
            return failure<XpckArchive>(name.error->code, name.error->offset);
        }
        if (!names_seen.insert(*name.value).second) {
            return failure<XpckArchive>("xpck_duplicate_name", index);
        }
        if (entry.file_offset > header.data_size ||
            entry.file_size > header.data_size - entry.file_offset) {
            return failure<XpckArchive>("xpck_nested_file_out_of_bounds", index);
        }
        const auto absolute_offset = static_cast<std::uint64_t>(header.data_offset) +
                                     entry.file_offset;
        if (absolute_offset > catalog_entry->size ||
            entry.file_size > catalog_entry->size - absolute_offset) {
            return failure<XpckArchive>("xpck_file_out_of_bounds", index);
        }
        archive.files.push_back({*name.value, entry.hash, absolute_offset, entry.file_size});
    }
    return ParseResult<XpckArchive>::success(std::move(archive));
}

}  // namespace ykw2::core
