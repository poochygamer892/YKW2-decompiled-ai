#include "ykw2/core/formats.hpp"

#include <algorithm>
#include <limits>

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset) {
    return ParseResult<T>::failure(code, offset);
}

bool scaled_u32(std::uint32_t value, std::uint32_t& result) {
    const auto scaled = static_cast<std::uint64_t>(value) * 4u;
    if (scaled > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    result = static_cast<std::uint32_t>(scaled);
    return true;
}

}  // namespace

ParseResult<Level5Header> parse_level5_header(ByteView bytes) {
    const auto word = bytes.u32_le(0);
    if (!word) {
        return failure<Level5Header>("truncated_level5_header", 0);
    }

    Level5Header header;
    header.method_size_word = *word;
    header.decoded_size = *word >> 3;
    header.method_id = static_cast<std::uint8_t>(*word & 0x07u);
    switch (header.method_id) {
        case 0:
            header.method = Level5Method::None;
            break;
        case 1:
            header.method = Level5Method::Lz10;
            break;
        case 2:
            header.method = Level5Method::Huffman4;
            break;
        case 3:
            header.method = Level5Method::Huffman8;
            break;
        case 4:
            header.method = Level5Method::Rle;
            break;
        case 5:
            header.method = Level5Method::Zlib;
            break;
        default:
            header.method = Level5Method::Unknown;
            break;
    }
    return ParseResult<Level5Header>::success(header);
}

ParseResult<Arc0Header> parse_arc0_header(ByteView bytes) {
    if (!bytes.starts_with("ARC0")) {
        return failure<Arc0Header>("invalid_arc0_magic", 0);
    }
    if (bytes.size() < 0x48) {
        return failure<Arc0Header>("truncated_arc0_header", bytes.size());
    }

    Arc0Header header;
    const auto d_entries = bytes.u32_le(0x04);
    const auto d_hash = bytes.u32_le(0x08);
    const auto f_entries = bytes.u32_le(0x0C);
    const auto names = bytes.u32_le(0x10);
    const auto data = bytes.u32_le(0x14);
    const auto d_count = bytes.i16_le(0x18);
    const auto d_hash_count = bytes.i16_le(0x1A);
    const auto f_count = bytes.u32_le(0x1C);
    const auto chunk_size = bytes.u32_le(0x20);
    const auto d_total = bytes.u32_le(0x38);
    const auto f_total = bytes.u32_le(0x3C);
    if (!d_entries || !d_hash || !f_entries || !names || !data || !d_count ||
        !d_hash_count || !f_count || !chunk_size || !d_total || !f_total) {
        return failure<Arc0Header>("truncated_arc0_fields", 0x04);
    }

    header.directory_entries_offset = *d_entries;
    header.directory_hash_offset = *d_hash;
    header.file_entries_offset = *f_entries;
    header.name_offset = *names;
    header.data_offset = *data;
    header.directory_entries_count = *d_count;
    header.directory_hash_count = *d_hash_count;
    header.file_entries_count = *f_count;
    header.table_chunk_size = *chunk_size;
    header.directory_count = *d_total;
    header.file_count = *f_total;

    const std::array<std::uint32_t, 5> offsets = {
        header.directory_entries_offset,
        header.directory_hash_offset,
        header.file_entries_offset,
        header.name_offset,
        header.data_offset,
    };
    if (!std::is_sorted(offsets.begin(), offsets.end())) {
        return failure<Arc0Header>("non_monotonic_arc0_offsets", 0x04);
    }
    for (const auto offset : offsets) {
        if (offset < 0x48 || static_cast<std::uint64_t>(offset) > bytes.size()) {
            return failure<Arc0Header>("arc0_offset_out_of_bounds", offset);
        }
    }
    return ParseResult<Arc0Header>::success(header);
}

ParseResult<XpckHeader> parse_xpck_header(ByteView bytes) {
    if (!bytes.starts_with("XPCK")) {
        return failure<XpckHeader>("invalid_xpck_magic", 0);
    }
    if (bytes.size() < 0x14) {
        return failure<XpckHeader>("truncated_xpck_header", bytes.size());
    }

    const auto count_type = bytes.u16_le(0x04);
    const auto info_units = bytes.u16_le(0x06);
    const auto name_units = bytes.u16_le(0x08);
    const auto data_units = bytes.u16_le(0x0A);
    const auto info_size_units = bytes.u16_le(0x0C);
    const auto name_size_units = bytes.u16_le(0x0E);
    const auto data_size_units = bytes.u32_le(0x10);
    if (!count_type || !info_units || !name_units || !data_units ||
        !info_size_units || !name_size_units || !data_size_units) {
        return failure<XpckHeader>("truncated_xpck_fields", 0x04);
    }

    XpckHeader header;
    header.file_count_and_type = *count_type;
    header.entry_count = static_cast<std::uint16_t>(*count_type & 0x0FFFu);
    header.content_type = static_cast<std::uint16_t>(*count_type >> 12);
    if (!scaled_u32(*info_units, header.info_offset) ||
        !scaled_u32(*name_units, header.name_offset) ||
        !scaled_u32(*data_units, header.data_offset) ||
        !scaled_u32(*info_size_units, header.info_size) ||
        !scaled_u32(*name_size_units, header.name_size) ||
        !scaled_u32(*data_size_units, header.data_size)) {
        return failure<XpckHeader>("xpck_size_overflow", 0x06);
    }

    const auto info_end = static_cast<std::uint64_t>(header.info_offset) + header.info_size;
    const auto name_end = static_cast<std::uint64_t>(header.name_offset) + header.name_size;
    const auto data_end = static_cast<std::uint64_t>(header.data_offset) + header.data_size;
    if (info_end > bytes.size() || name_end > bytes.size() || data_end > bytes.size() + 4u) {
        return failure<XpckHeader>("xpck_region_out_of_bounds", 0x06);
    }
    if (static_cast<std::uint64_t>(header.entry_count) * 12u > header.info_size) {
        return failure<XpckHeader>("xpck_info_table_too_small", 0x04);
    }
    return ParseResult<XpckHeader>::success(header);
}

ParseResult<XpckEntry> parse_xpck_entry(ByteView bytes, std::size_t index) {
    if (index >= (bytes.size() / 12u)) {
        return failure<XpckEntry>("xpck_entry_out_of_bounds", index * 12u);
    }
    const auto offset = index * 12u;
    const auto hash = bytes.u32_le(offset);
    const auto name_offset = bytes.u16_le(offset + 4);
    const auto file_offset_low = bytes.u16_le(offset + 6);
    const auto file_size_low = bytes.u16_le(offset + 8);
    const auto file_offset_high = bytes.u8(offset + 10);
    const auto file_size_high = bytes.u8(offset + 11);
    if (!hash || !name_offset || !file_offset_low || !file_size_low ||
        !file_offset_high || !file_size_high) {
        return failure<XpckEntry>("truncated_xpck_entry", offset);
    }

    XpckEntry entry;
    entry.hash = *hash;
    entry.name_offset = *name_offset;
    entry.file_offset = (static_cast<std::uint32_t>(*file_offset_high) << 16 |
                         static_cast<std::uint32_t>(*file_offset_low)) * 4u;
    entry.file_size = static_cast<std::uint32_t>(*file_size_high) << 16 |
                      static_cast<std::uint32_t>(*file_size_low);
    return ParseResult<XpckEntry>::success(entry);
}

ParseResult<Xq32Header> parse_xq32_header(ByteView bytes) {
    if (!bytes.starts_with("XQ32")) {
        return failure<Xq32Header>("invalid_xq32_magic", 0);
    }
    if (bytes.size() < 0x18) {
        return failure<Xq32Header>("truncated_xq32_header", bytes.size());
    }

    const auto function_count = bytes.u16_le(0x04);
    const auto function_units = bytes.u16_le(0x06);
    const auto jump_units = bytes.u16_le(0x08);
    const auto jump_count = bytes.u16_le(0x0A);
    const auto instruction_units = bytes.u16_le(0x0C);
    const auto instruction_count = bytes.u16_le(0x0E);
    const auto argument_units = bytes.u16_le(0x10);
    const auto argument_count = bytes.u16_le(0x12);
    const auto global_variables = bytes.u16_le(0x14);
    const auto strings_units = bytes.u16_le(0x16);
    if (!function_count || !function_units || !jump_units || !jump_count ||
        !instruction_units || !instruction_count || !argument_units ||
        !argument_count || !global_variables || !strings_units) {
        return failure<Xq32Header>("truncated_xq32_fields", 0x04);
    }

    std::uint32_t function_offset = 0;
    std::uint32_t jump_offset = 0;
    std::uint32_t instruction_offset = 0;
    std::uint32_t argument_offset = 0;
    std::uint32_t strings_offset = 0;
    if (!scaled_u32(*function_units, function_offset) ||
        !scaled_u32(*jump_units, jump_offset) ||
        !scaled_u32(*instruction_units, instruction_offset) ||
        !scaled_u32(*argument_units, argument_offset) ||
        !scaled_u32(*strings_units, strings_offset)) {
        return failure<Xq32Header>("xq32_offset_overflow", 0x06);
    }

    const std::array<std::uint32_t, 5> offsets = {
        function_offset, jump_offset, instruction_offset, argument_offset, strings_offset,
    };
    if (!std::is_sorted(offsets.begin(), offsets.end())) {
        return failure<Xq32Header>("non_monotonic_xq32_offsets", 0x06);
    }
    for (const auto offset : offsets) {
        if (offset < 0x18 || static_cast<std::uint64_t>(offset) > bytes.size()) {
            return failure<Xq32Header>("xq32_offset_out_of_bounds", offset);
        }
    }

    Xq32Header header;
    header.function_count = *function_count;
    header.jump_count = *jump_count;
    header.instruction_count = *instruction_count;
    header.argument_count = *argument_count;
    header.global_variable_count = *global_variables;
    header.function = {function_offset, jump_offset, *function_count};
    header.jump = {jump_offset, instruction_offset, *jump_count};
    header.instruction = {instruction_offset, argument_offset, *instruction_count};
    header.argument = {argument_offset, strings_offset, *argument_count};
    header.strings = {strings_offset, static_cast<std::uint32_t>(bytes.size()), 0};
    return ParseResult<Xq32Header>::success(header);
}

ParseResult<ImgcHeader> parse_imgc_header(ByteView bytes) {
    if (!bytes.starts_with("IMGC")) {
        return failure<ImgcHeader>("invalid_imgc_magic", 0);
    }
    if (bytes.size() < 0x40) {
        return failure<ImgcHeader>("truncated_imgc_header", bytes.size());
    }

    ImgcHeader header;
    std::copy_n(bytes.data(), header.signature.size(), header.signature.begin());
    const auto field_0x08 = bytes.u32_le(0x08);
    const auto field_0x0c = bytes.u32_le(0x0C);
    const auto field_0x0e = bytes.u16_le(0x0E);
    const auto field_0x10 = bytes.u16_le(0x10);
    const auto field_0x12 = bytes.u16_le(0x12);
    const auto field_0x14 = bytes.u32_le(0x14);
    const auto field_0x18 = bytes.u16_le(0x18);
    const auto field_0x1a = bytes.u16_le(0x1A);
    const auto field_0x1c = bytes.u32_le(0x1C);
    const auto field_0x20 = bytes.u32_le(0x20);
    const auto field_0x34 = bytes.u32_le(0x34);
    const auto field_0x38 = bytes.u32_le(0x38);
    const auto field_0x3c = bytes.u32_le(0x3C);
    if (!field_0x08 || !field_0x0c || !field_0x0e || !field_0x10 || !field_0x12 ||
        !field_0x14 || !field_0x18 || !field_0x1a || !field_0x1c || !field_0x20 ||
        !field_0x34 || !field_0x38 || !field_0x3c) {
        return failure<ImgcHeader>("truncated_imgc_fields", 0x08);
    }
    header.field_0x08 = *field_0x08;
    header.field_0x0c = *field_0x0c;
    header.field_0x0e_u16 = *field_0x0e;
    header.field_0x10_u16 = *field_0x10;
    header.field_0x12_u16 = *field_0x12;
    header.field_0x14 = *field_0x14;
    header.field_0x18_u16 = *field_0x18;
    header.field_0x1a_u16 = *field_0x1a;
    header.field_0x1c = *field_0x1c;
    header.field_0x20 = *field_0x20;
    header.field_0x34 = *field_0x34;
    header.field_0x38 = *field_0x38;
    header.field_0x3c = *field_0x3c;
    return ParseResult<ImgcHeader>::success(header);
}

ResourceKind detect_resource_kind(ByteView bytes) {
    if (bytes.starts_with("ARC0")) {
        return ResourceKind::Arc0;
    }
    if (bytes.starts_with("XPCK")) {
        return ResourceKind::Xpck;
    }
    if (bytes.starts_with("XQ32")) {
        return ResourceKind::Xq32;
    }
    if (bytes.starts_with("IMGC")) {
        return ResourceKind::Imgc;
    }
    return ResourceKind::Unknown;
}

ParseResult<ResourceHeader> parse_resource_header(ByteView bytes) {
    switch (detect_resource_kind(bytes)) {
        case ResourceKind::Arc0: {
            const auto parsed = parse_arc0_header(bytes);
            if (parsed) {
                return ParseResult<ResourceHeader>::success(*parsed.value);
            }
            return failure<ResourceHeader>(parsed.error->code, parsed.error->offset);
        }
        case ResourceKind::Xpck: {
            const auto parsed = parse_xpck_header(bytes);
            if (parsed) {
                return ParseResult<ResourceHeader>::success(*parsed.value);
            }
            return failure<ResourceHeader>(parsed.error->code, parsed.error->offset);
        }
        case ResourceKind::Xq32: {
            const auto parsed = parse_xq32_header(bytes);
            if (parsed) {
                return ParseResult<ResourceHeader>::success(*parsed.value);
            }
            return failure<ResourceHeader>(parsed.error->code, parsed.error->offset);
        }
        case ResourceKind::Imgc: {
            const auto parsed = parse_imgc_header(bytes);
            if (parsed) {
                return ParseResult<ResourceHeader>::success(*parsed.value);
            }
            return failure<ResourceHeader>(parsed.error->code, parsed.error->offset);
        }
        case ResourceKind::Unknown:
            return failure<ResourceHeader>("unknown_resource_magic", 0);
    }
    return failure<ResourceHeader>("unknown_resource_kind", 0);
}

}  // namespace ykw2::core
