#include "ykw2/core/arc0_reader.hpp"

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

ParseResult<std::vector<std::uint8_t>> read_decoded_region(
    const ResourceStore& store, std::string_view path, std::uint64_t start,
    std::uint64_t end, std::uint64_t archive_size, const Arc0ReadOptions& options,
    std::string_view label) {
    if (end < start || end > archive_size) {
        return failure<std::vector<std::uint8_t>>("arc0_region_out_of_bounds",
                                                  static_cast<std::size_t>(start));
    }
    const auto compressed_size = end - start;
    if (compressed_size > options.max_compressed_table_bytes ||
        compressed_size > std::numeric_limits<std::size_t>::max()) {
        return failure<std::vector<std::uint8_t>>("arc0_compressed_table_limit_exceeded",
                                                  static_cast<std::size_t>(start));
    }
    std::vector<std::uint8_t> compressed(static_cast<std::size_t>(compressed_size));
    if (!store.read_exact(path, start, compressed.data(), compressed.size())) {
        return failure<std::vector<std::uint8_t>>("arc0_table_read_failed",
                                                  static_cast<std::size_t>(start));
    }
    const auto decoded = decode_level5(
        ByteView(compressed.data(), compressed.size()),
        Level5DecodeOptions{options.max_decoded_table_bytes});
    if (!decoded) {
        return failure<std::vector<std::uint8_t>>(decoded.error->code,
                                                  decoded.error->offset);
    }
    (void)label;
    return decoded;
}

ParseResult<std::string> read_c_string(ByteView table, std::uint64_t offset,
                                       std::string_view label) {
    if (offset >= table.size()) {
        return failure<std::string>("arc0_name_offset_out_of_bounds",
                                    static_cast<std::size_t>(offset));
    }
    const auto start = static_cast<std::size_t>(offset);
    std::size_t end = start;
    while (end < table.size() && table.data()[end] != 0) {
        ++end;
    }
    if (end == table.size()) {
        return failure<std::string>("arc0_unterminated_name",
                                    static_cast<std::size_t>(offset));
    }
    (void)label;
    return ParseResult<std::string>::success(
        std::string(reinterpret_cast<const char*>(table.data() + start), end - start));
}

}  // namespace

const Arc0File* Arc0Archive::find(std::string_view path) const {
    const auto found = std::find_if(
        files.begin(), files.end(),
        [path](const Arc0File& file) { return file.path == path; });
    return found == files.end() ? nullptr : &*found;
}

bool read_arc0_file_range(const ResourceStore& store, std::string_view archive_path,
                          const Arc0File& file, std::uint64_t offset,
                          std::uint8_t* destination, std::size_t length) {
    if (offset > file.size || length > static_cast<std::uint64_t>(file.size) - offset) {
        return false;
    }
    if (file.offset > std::numeric_limits<std::uint64_t>::max() - offset) {
        return false;
    }
    return store.read_exact(archive_path, file.offset + offset, destination, length);
}

ParseResult<Arc0Archive> read_arc0_archive(
    const ResourceStore& store, std::string_view path, Arc0ReadOptions options) {
    const auto catalog_entry = store.catalog().find(path);
    if (catalog_entry == nullptr) {
        return failure<Arc0Archive>("resource_not_in_catalog", 0);
    }
    if (catalog_entry->size < 0x48) {
        return failure<Arc0Archive>("arc0_header_truncated", 0);
    }
    std::vector<std::uint8_t> header_bytes(0x48);
    if (!store.read_exact(path, 0, header_bytes.data(), header_bytes.size())) {
        return failure<Arc0Archive>("arc0_header_read_failed", 0);
    }
    const auto parsed_header = parse_arc0_header(
        ByteView(header_bytes.data(), header_bytes.size()), catalog_entry->size);
    if (!parsed_header) {
        return failure<Arc0Archive>(parsed_header.error->code, parsed_header.error->offset);
    }
    const auto& header = *parsed_header.value;
    if (static_cast<std::uint64_t>(header.directory_entries_count) > options.max_files ||
        header.file_entries_count > options.max_files) {
        return failure<Arc0Archive>("arc0_file_limit_exceeded", 0x1c);
    }

    const auto directory_data = read_decoded_region(
        store, path, header.directory_entries_offset, header.directory_hash_offset,
        catalog_entry->size, options, "directory entries");
    if (!directory_data) {
        return failure<Arc0Archive>(directory_data.error->code, directory_data.error->offset);
    }
    const auto hash_data = read_decoded_region(
        store, path, header.directory_hash_offset, header.file_entries_offset,
        catalog_entry->size, options, "directory hashes");
    if (!hash_data) {
        return failure<Arc0Archive>(hash_data.error->code, hash_data.error->offset);
    }
    const auto file_data = read_decoded_region(
        store, path, header.file_entries_offset, header.name_offset,
        catalog_entry->size, options, "file entries");
    if (!file_data) {
        return failure<Arc0Archive>(file_data.error->code, file_data.error->offset);
    }
    const auto names = read_decoded_region(
        store, path, header.name_offset, header.data_offset,
        catalog_entry->size, options, "names");
    if (!names) {
        return failure<Arc0Archive>(names.error->code, names.error->offset);
    }

    const auto directory_count = static_cast<std::size_t>(header.directory_entries_count);
    const auto hash_count = static_cast<std::size_t>(header.directory_hash_count);
    const auto file_count = static_cast<std::size_t>(header.file_entries_count);
    if (directory_count > directory_data.value->size() / 20u ||
        hash_count > hash_data.value->size() / 4u ||
        file_count > file_data.value->size() / 16u) {
        return failure<Arc0Archive>("arc0_table_count_exceeds_data", 0x18);
    }

    struct DirectoryRecord {
        std::uint16_t first_file_index = 0;
        std::int16_t file_count = 0;
        std::uint32_t file_name_start_offset = 0;
        std::uint32_t directory_name_start_offset = 0;
    };
    struct FileRecord {
        std::uint32_t crc32 = 0;
        std::uint32_t name_offset_in_folder = 0;
        std::uint32_t file_offset = 0;
        std::uint32_t file_size = 0;
    };
    std::vector<DirectoryRecord> directories;
    directories.reserve(directory_count);
    for (std::size_t index = 0; index < directory_count; ++index) {
        const auto bytes = ByteView(directory_data.value->data() + index * 20, 20);
        const auto first_file = bytes.u16_le(0x08);
        const auto file_count_value = bytes.i16_le(0x0a);
        const auto file_name_start = bytes.u32_le(0x0c);
        const auto directory_name_start = bytes.u32_le(0x10);
        if (!first_file || !file_count_value || !file_name_start || !directory_name_start ||
            *file_count_value < 0) {
            return failure<Arc0Archive>("arc0_directory_record_invalid", index * 20);
        }
        directories.push_back({*first_file, *file_count_value, *file_name_start,
                               *directory_name_start});
    }

    std::vector<FileRecord> files;
    files.reserve(file_count);
    for (std::size_t index = 0; index < file_count; ++index) {
        const auto bytes = ByteView(file_data.value->data() + index * 16, 16);
        const auto crc32 = bytes.u32_le(0);
        const auto name_offset = bytes.u32_le(0x04);
        const auto file_offset = bytes.u32_le(0x08);
        const auto file_size = bytes.u32_le(0x0c);
        if (!crc32 || !name_offset || !file_offset || !file_size) {
            return failure<Arc0Archive>("arc0_file_record_invalid", index * 16);
        }
        files.push_back({*crc32, *name_offset, *file_offset, *file_size});
    }

    Arc0Archive archive;
    archive.header = header;
    archive.files.reserve(file_count);
    std::unordered_set<std::string> paths;
    for (const auto& directory : directories) {
        const auto directory_name = read_c_string(
            ByteView(names.value->data(), names.value->size()),
            directory.directory_name_start_offset, "directory name");
        if (!directory_name) {
            return failure<Arc0Archive>(directory_name.error->code,
                                        directory_name.error->offset);
        }
        const auto first = static_cast<std::size_t>(directory.first_file_index);
        const auto count = static_cast<std::size_t>(directory.file_count);
        if (first > files.size() || count > files.size() - first) {
            return failure<Arc0Archive>("arc0_directory_file_range_invalid", first);
        }
        for (std::size_t index = first; index < first + count; ++index) {
            const auto& file = files[index];
            const auto name_offset = static_cast<std::uint64_t>(file.name_offset_in_folder) +
                                     static_cast<std::uint64_t>(directory.file_name_start_offset);
            const auto file_name = read_c_string(
                ByteView(names.value->data(), names.value->size()), name_offset, "file name");
            if (!file_name) {
                return failure<Arc0Archive>(file_name.error->code, file_name.error->offset);
            }
            std::string full_path = *directory_name.value;
            if (!full_path.empty() && full_path.back() != '/') {
                full_path.push_back('/');
            }
            full_path += *file_name.value;
            if (!paths.insert(full_path).second) {
                return failure<Arc0Archive>("arc0_duplicate_path", index);
            }
            const auto absolute_offset = static_cast<std::uint64_t>(header.data_offset) +
                                         file.file_offset;
            if (absolute_offset > catalog_entry->size ||
                file.file_size > catalog_entry->size - absolute_offset) {
                return failure<Arc0Archive>("arc0_file_out_of_bounds", index);
            }
            archive.files.push_back({std::move(full_path), absolute_offset,
                                     file.file_size, file.crc32});
        }
    }
    return ParseResult<Arc0Archive>::success(std::move(archive));
}

}  // namespace ykw2::core
