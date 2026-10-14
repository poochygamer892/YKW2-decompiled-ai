#include "ykw2/core/romfs_extract.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset) {
    return ParseResult<T>::failure(code, offset);
}

bool read_exact(const ResourceSource& source, std::uint64_t offset,
                std::uint8_t* destination, std::size_t size) {
    return source.contains(offset, size) && source.read(offset, destination, size);
}

bool bounded_region(std::uint32_t offset, std::uint32_t size,
                    std::uint64_t limit, std::uint64_t& end) {
    const auto finish = static_cast<std::uint64_t>(offset) + size;
    if (finish > limit) {
        return false;
    }
    end = finish;
    return true;
}

void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

bool decode_utf16le(ByteView bytes, std::size_t offset, std::size_t length,
                    std::string& output) {
    if ((length & 1u) != 0 || !bytes.contains(offset, length)) {
        return false;
    }
    output.clear();
    output.reserve(length);
    const auto end = offset + length;
    for (std::size_t cursor = offset; cursor < end; cursor += 2) {
        const auto first = bytes.u16_le(cursor);
        if (!first) {
            return false;
        }
        std::uint32_t codepoint = *first;
        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
            if (cursor + 4 > end) {
                return false;
            }
            const auto second = bytes.u16_le(cursor + 2);
            if (!second || *second < 0xdc00 || *second > 0xdfff) {
                return false;
            }
            codepoint = 0x10000u + ((codepoint - 0xd800u) << 10) + (*second - 0xdc00u);
            cursor += 2;
        } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
            return false;
        }
        append_utf8(output, codepoint);
    }
    return true;
}

bool valid_component(const std::string& component) {
    if (component.empty() || component == "." || component == ".." ||
        component.find('\0') != std::string::npos ||
        component.find('/') != std::string::npos ||
        component.find('\\') != std::string::npos) {
        return false;
    }
    return component.size() <= 4096;
}

std::string join_path(const std::string& parent, const std::string& component) {
    if (parent.empty()) {
        return component;
    }
    return parent + "/" + component;
}

class RomFsWalker {
public:
    RomFsWalker(const ResourceSource& source,
                std::uint64_t data_offset,
                std::uint64_t data_size,
                ByteView directory_metadata,
                ByteView file_metadata,
                ResourceSink& sink,
                RomFsExtractOptions options)
        : source_(source),
          data_offset_(data_offset),
          data_size_(data_size),
          directory_metadata_(directory_metadata),
          file_metadata_(file_metadata),
          sink_(sink),
          options_(options) {}

    bool walk() { return walk_directory(0, {}, 0); }

    const std::optional<ParseError>& error() const { return error_; }
    const RomFsExtractStats& stats() const { return stats_; }

private:
    bool fail(std::string_view code, std::size_t offset) {
        if (!error_) {
            error_ = ParseError{code, offset};
        }
        return false;
    }

    bool walk_directory(std::uint32_t offset, const std::string& path,
                        std::size_t depth) {
        if (depth > 1024) {
            return fail("romfs_directory_depth_exceeded", offset);
        }
        if (!visited_directories_.insert(offset).second) {
            return fail("romfs_directory_cycle", offset);
        }
        if (static_cast<std::uint64_t>(offset) + 0x18u > directory_metadata_.size()) {
            return fail("romfs_directory_out_of_bounds", offset);
        }
        const auto entry = directory_metadata_.subview(offset, 0x18);
        const auto first_child = entry.u32_le(0x08);
        const auto first_file = entry.u32_le(0x0c);
        if (!first_child || !first_file) {
            return fail("romfs_directory_entry_truncated", offset);
        }

        if (!walk_directory_chain(*first_child, path, depth)) {
            return false;
        }
        return walk_file_chain(*first_file, path);
    }

    bool walk_directory_chain(std::uint32_t offset, const std::string& parent,
                              std::size_t depth) {
        constexpr std::uint32_t kNoEntry = 0xffffffffu;
        auto current = offset;
        while (current != kNoEntry) {
            if (static_cast<std::uint64_t>(current) + 0x18u > directory_metadata_.size()) {
                return fail("romfs_child_directory_out_of_bounds", current);
            }
            const auto entry = directory_metadata_.subview(current, 0x18);
            const auto next = entry.u32_le(0x04);
            const auto name_size = entry.u32_le(0x14);
            if (!next || !name_size || *name_size > 4096 ||
                static_cast<std::uint64_t>(current) + 0x18u + *name_size > directory_metadata_.size()) {
                return fail("romfs_child_directory_name_out_of_bounds", current);
            }
            std::string name;
            if (!decode_utf16le(directory_metadata_, current + 0x18, *name_size, name) ||
                !valid_component(name)) {
                return fail("romfs_invalid_directory_name", current + 0x18);
            }
            if (!walk_directory(current, join_path(parent, name), depth + 1)) {
                return false;
            }
            current = *next;
        }
        return true;
    }

    bool walk_file_chain(std::uint32_t offset, const std::string& parent) {
        constexpr std::uint32_t kNoEntry = 0xffffffffu;
        auto current = offset;
        while (current != kNoEntry) {
            if (static_cast<std::uint64_t>(current) + 0x20u > file_metadata_.size()) {
                return fail("romfs_file_entry_out_of_bounds", current);
            }
            const auto entry = file_metadata_.subview(current, 0x20);
            const auto next = entry.u32_le(0x04);
            const auto data_offset = entry.u64_le(0x08);
            const auto file_size = entry.u64_le(0x10);
            const auto name_size = entry.u32_le(0x1c);
            if (!next || !data_offset || !file_size || !name_size || *name_size > 4096 ||
                static_cast<std::uint64_t>(current) + 0x20u + *name_size > file_metadata_.size()) {
                return fail("romfs_file_name_out_of_bounds", current);
            }
            std::string name;
            if (!decode_utf16le(file_metadata_, current + 0x20, *name_size, name) ||
                !valid_component(name)) {
                return fail("romfs_invalid_file_name", current + 0x20);
            }
            if (visited_files_.find(current) != visited_files_.end()) {
                return fail("romfs_file_cycle", current);
            }
            visited_files_.insert(current);
            if (!extract_file(join_path(parent, name), *data_offset, *file_size)) {
                return false;
            }
            current = *next;
        }
        return true;
    }

    bool extract_file(const std::string& path, std::uint64_t file_offset,
                      std::uint64_t file_size) {
        if (options_.max_files != 0 && stats_.files >= options_.max_files) {
            return fail("romfs_file_limit_exceeded", stats_.files);
        }
        if (options_.max_total_bytes != 0 &&
            (file_size > options_.max_total_bytes ||
             stats_.bytes > options_.max_total_bytes - file_size)) {
            return fail("romfs_byte_limit_exceeded", stats_.bytes);
        }
        if (file_offset > data_size_ || file_size > data_size_ - file_offset) {
            return fail("romfs_file_data_out_of_bounds", static_cast<std::size_t>(file_offset));
        }
        if (!sink_.begin_file(path, file_size)) {
            return fail("romfs_sink_begin_failed", 0);
        }

        constexpr std::size_t kChunkSize = 1024 * 1024;
        std::vector<std::uint8_t> chunk(kChunkSize);
        std::uint64_t remaining = file_size;
        std::uint64_t cursor = data_offset_ + file_offset;
        while (remaining != 0) {
            const auto request = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, chunk.size()));
            if (!read_exact(source_, cursor, chunk.data(), request) ||
                !sink_.write(chunk.data(), request)) {
                sink_.abort_file();
                return fail("romfs_sink_write_failed", 0);
            }
            cursor += request;
            remaining -= request;
        }
        if (!sink_.end_file()) {
            sink_.abort_file();
            return fail("romfs_sink_end_failed", 0);
        }
        ++stats_.files;
        stats_.bytes += file_size;
        return true;
    }

    const ResourceSource& source_;
    std::uint64_t data_offset_;
    std::uint64_t data_size_;
    ByteView directory_metadata_;
    ByteView file_metadata_;
    ResourceSink& sink_;
    RomFsExtractOptions options_;
    RomFsExtractStats stats_;
    std::unordered_set<std::uint32_t> visited_directories_;
    std::unordered_set<std::uint32_t> visited_files_;
    std::optional<ParseError> error_;
};

}  // namespace

ParseResult<NcchHeader> parse_ncch_header(ByteView first_header,
                                          std::uint64_t partition_size) {
    if (first_header.size() < 0x200) {
        return failure<NcchHeader>("truncated_ncch_header", first_header.size());
    }
    if (!first_header.subview(0x100, 4).starts_with("NCCH")) {
        return failure<NcchHeader>("invalid_ncch_magic", 0x100);
    }
    const auto content_units = first_header.u32_le(0x104);
    const auto romfs_units = first_header.u32_le(0x1b0);
    const auto romfs_size_units = first_header.u32_le(0x1b4);
    const auto flags = first_header.u8(0x18f);
    if (!content_units || !romfs_units || !romfs_size_units || !flags) {
        return failure<NcchHeader>("truncated_ncch_fields", 0x104);
    }

    NcchHeader header;
    header.content_size_units = *content_units;
    header.content_size_bytes = static_cast<std::uint64_t>(*content_units) * 0x200u;
    header.romfs_offset_units = *romfs_units;
    header.romfs_size_units = *romfs_size_units;
    header.romfs_offset = static_cast<std::uint64_t>(*romfs_units) * 0x200u;
    header.romfs_size = static_cast<std::uint64_t>(*romfs_size_units) * 0x200u;
    header.no_romfs = (*flags & 0x02u) != 0;
    header.no_crypto = (*flags & 0x04u) != 0;
    if (header.content_size_bytes > partition_size ||
        header.romfs_offset > partition_size ||
        header.romfs_size > partition_size - header.romfs_offset) {
        return failure<NcchHeader>("ncch_region_out_of_bounds", 0x1b0);
    }
    if (header.no_romfs || header.romfs_size == 0) {
        return failure<NcchHeader>("ncch_has_no_romfs", 0x18f);
    }
    return ParseResult<NcchHeader>::success(header);
}

ParseResult<RomFsExtractStats> extract_romfs(
    const ResourceSource& source,
    std::uint64_t romfs_offset,
    std::uint64_t romfs_size,
    ResourceSink& sink,
    RomFsExtractOptions options) {
    if (romfs_offset > source.size() || romfs_size > source.size() - romfs_offset ||
        romfs_size < 0x28) {
        return failure<RomFsExtractStats>("romfs_region_out_of_bounds", 0);
    }

    std::array<std::uint8_t, 0x60> ivfc_buffer{};
    const auto ivfc_read_size = static_cast<std::size_t>(std::min<std::uint64_t>(romfs_size, ivfc_buffer.size()));
    if (!read_exact(source, romfs_offset, ivfc_buffer.data(), ivfc_read_size)) {
        return failure<RomFsExtractStats>("romfs_header_read_failed", 0);
    }
    const ByteView ivfc_view(ivfc_buffer.data(), ivfc_read_size);
    std::uint64_t lv3_offset = 0;
    if (ivfc_view.starts_with("IVFC")) {
        if (ivfc_view.size() < 0x50) {
            return failure<RomFsExtractStats>("truncated_ivfc_header", 0);
        }
        const auto magic_number = ivfc_view.u32_le(0x04);
        const auto master_hash_size = ivfc_view.u32_le(0x08);
        const auto block_exponent = ivfc_view.u32_le(0x4c);
        if (!magic_number || !master_hash_size || !block_exponent || *magic_number != 0x10000 ||
            *block_exponent >= 63) {
            return failure<RomFsExtractStats>("invalid_ivfc_header", 0x04);
        }
        const auto block_size = std::uint64_t{1} << *block_exponent;
        const auto base = std::uint64_t{0x60} + *master_hash_size;
        lv3_offset = ((base + block_size - 1) / block_size) * block_size;
    }
    if (lv3_offset > romfs_size || romfs_size - lv3_offset < 0x28) {
        return failure<RomFsExtractStats>("romfs_level3_out_of_bounds", lv3_offset);
    }

    std::array<std::uint8_t, 0x28> lv3_buffer{};
    if (!read_exact(source, romfs_offset + lv3_offset, lv3_buffer.data(), lv3_buffer.size())) {
        return failure<RomFsExtractStats>("romfs_level3_header_read_failed", lv3_offset);
    }
    const ByteView lv3(lv3_buffer.data(), lv3_buffer.size());
    const auto header_size = lv3.u32_le(0x00);
    const auto dirhash_offset = lv3.u32_le(0x04);
    const auto dirhash_size = lv3.u32_le(0x08);
    const auto dirmeta_offset = lv3.u32_le(0x0c);
    const auto dirmeta_size = lv3.u32_le(0x10);
    const auto filehash_offset = lv3.u32_le(0x14);
    const auto filehash_size = lv3.u32_le(0x18);
    const auto filemeta_offset = lv3.u32_le(0x1c);
    const auto filemeta_size = lv3.u32_le(0x20);
    const auto filedata_offset = lv3.u32_le(0x24);
    if (!header_size || !dirhash_offset || !dirhash_size || !dirmeta_offset || !dirmeta_size ||
        !filehash_offset || !filehash_size || !filemeta_offset || !filemeta_size || !filedata_offset ||
        *header_size != 0x28) {
        return failure<RomFsExtractStats>("invalid_romfs_level3_header", lv3_offset);
    }
    const auto lv3_size = romfs_size - lv3_offset;
    std::uint64_t ignored_end = 0;
    if (*dirhash_offset < 0x28 || !bounded_region(*dirhash_offset, *dirhash_size, lv3_size, ignored_end) ||
        *dirmeta_offset < ignored_end || !bounded_region(*dirmeta_offset, *dirmeta_size, lv3_size, ignored_end) ||
        *filehash_offset < ignored_end || !bounded_region(*filehash_offset, *filehash_size, lv3_size, ignored_end) ||
        *filemeta_offset < ignored_end || !bounded_region(*filemeta_offset, *filemeta_size, lv3_size, ignored_end) ||
        *filedata_offset < ignored_end || *filedata_offset > lv3_size) {
        return failure<RomFsExtractStats>("invalid_romfs_level3_regions", lv3_offset);
    }
    constexpr std::uint64_t kMaxMetadataBytes = 512ull * 1024ull * 1024ull;
    if (static_cast<std::uint64_t>(*dirmeta_size) + *filemeta_size > kMaxMetadataBytes) {
        return failure<RomFsExtractStats>("romfs_metadata_limit_exceeded", lv3_offset);
    }

    std::vector<std::uint8_t> directory_metadata(*dirmeta_size);
    std::vector<std::uint8_t> file_metadata(*filemeta_size);
    const auto metadata_base = romfs_offset + lv3_offset;
    if (!read_exact(source, metadata_base + *dirmeta_offset, directory_metadata.data(), directory_metadata.size()) ||
        !read_exact(source, metadata_base + *filemeta_offset, file_metadata.data(), file_metadata.size())) {
        return failure<RomFsExtractStats>("romfs_metadata_read_failed", lv3_offset);
    }

    const auto data_base = metadata_base + *filedata_offset;
    const auto data_size = lv3_size - *filedata_offset;
    RomFsWalker walker(source, data_base, data_size,
                       ByteView(directory_metadata.data(), directory_metadata.size()),
                       ByteView(file_metadata.data(), file_metadata.size()), sink, options);
    if (!walker.walk()) {
        return failure<RomFsExtractStats>(walker.error()->code, walker.error()->offset);
    }
    return ParseResult<RomFsExtractStats>::success(walker.stats());
}

ParseResult<RomFsExtractStats> extract_application_romfs(
    const ResourceSource& source,
    const ImportPlan& plan,
    ResourceSink& sink,
    RomFsExtractOptions options) {
    if (plan.application_partition.size == 0 ||
        !source.contains(plan.application_partition.offset, 0x200)) {
        return failure<RomFsExtractStats>("application_partition_unavailable", 0);
    }
    std::array<std::uint8_t, 0x200> header{};
    if (!read_exact(source, plan.application_partition.offset, header.data(), header.size())) {
        return failure<RomFsExtractStats>("ncch_header_read_failed", plan.application_partition.offset);
    }
    const auto parsed = parse_ncch_header(
        ByteView(header.data(), header.size()), plan.application_partition.size);
    if (!parsed) {
        return failure<RomFsExtractStats>(parsed.error->code, parsed.error->offset);
    }
    const auto romfs_absolute = plan.application_partition.offset + parsed.value->romfs_offset;
    return extract_romfs(source, romfs_absolute, parsed.value->romfs_size, sink, options);
}

}  // namespace ykw2::core
