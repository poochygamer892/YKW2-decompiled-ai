#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "ykw2/core/byte_view.hpp"

namespace ykw2::core {

enum class Level5Method : std::uint8_t {
    None = 0,
    Lz10 = 1,
    Huffman4 = 2,
    Huffman8 = 3,
    Rle = 4,
    Zlib = 5,
    Unknown = 255,
};

struct Level5Header {
    std::uint32_t method_size_word = 0;
    std::uint32_t decoded_size = 0;
    std::uint8_t method_id = 0;
    Level5Method method = Level5Method::Unknown;
};

ParseResult<Level5Header> parse_level5_header(ByteView bytes);

struct Arc0Header {
    std::uint32_t directory_entries_offset = 0;
    std::uint32_t directory_hash_offset = 0;
    std::uint32_t file_entries_offset = 0;
    std::uint32_t name_offset = 0;
    std::uint32_t data_offset = 0;
    std::int16_t directory_entries_count = 0;
    std::int16_t directory_hash_count = 0;
    std::uint32_t file_entries_count = 0;
    std::uint32_t table_chunk_size = 0;
    std::uint32_t directory_count = 0;
    std::uint32_t file_count = 0;
};

ParseResult<Arc0Header> parse_arc0_header(ByteView bytes);
ParseResult<Arc0Header> parse_arc0_header(ByteView header, std::uint64_t archive_size);

struct XpckHeader {
    std::uint16_t file_count_and_type = 0;
    std::uint16_t entry_count = 0;
    std::uint16_t content_type = 0;
    std::uint32_t info_offset = 0;
    std::uint32_t name_offset = 0;
    std::uint32_t data_offset = 0;
    std::uint32_t info_size = 0;
    std::uint32_t name_size = 0;
    std::uint32_t data_size = 0;
};

struct XpckEntry {
    std::uint32_t hash = 0;
    std::uint16_t name_offset = 0;
    std::uint32_t file_offset = 0;
    std::uint32_t file_size = 0;
};

ParseResult<XpckHeader> parse_xpck_header(ByteView bytes);
ParseResult<XpckHeader> parse_xpck_header(ByteView header, std::uint64_t archive_size);
ParseResult<XpckEntry> parse_xpck_entry(ByteView bytes, std::size_t index);

struct Xq32TableRegion {
    std::uint32_t offset = 0;
    std::uint32_t end = 0;
    std::uint16_t entry_count = 0;
};

struct Xq32Header {
    std::uint16_t function_count = 0;
    std::uint16_t jump_count = 0;
    std::uint16_t instruction_count = 0;
    std::uint16_t argument_count = 0;
    std::uint16_t global_variable_count = 0;
    Xq32TableRegion function;
    Xq32TableRegion jump;
    Xq32TableRegion instruction;
    Xq32TableRegion argument;
    Xq32TableRegion strings;
};

ParseResult<Xq32Header> parse_xq32_header(ByteView bytes);

struct ImgcHeader {
    std::array<std::uint8_t, 8> signature{};
    std::uint32_t field_0x08 = 0;
    std::uint32_t field_0x0c = 0;
    std::uint16_t field_0x0e_u16 = 0;
    std::uint16_t field_0x10_u16 = 0;
    std::uint16_t field_0x12_u16 = 0;
    std::uint32_t field_0x14 = 0;
    std::uint16_t field_0x18_u16 = 0;
    std::uint16_t field_0x1a_u16 = 0;
    std::uint32_t field_0x1c = 0;
    std::uint32_t field_0x20 = 0;
    std::uint32_t field_0x34 = 0;
    std::uint32_t field_0x38 = 0;
    std::uint32_t field_0x3c = 0;
};

ParseResult<ImgcHeader> parse_imgc_header(ByteView bytes);

enum class ResourceKind {
    Unknown,
    Arc0,
    Xpck,
    Xq32,
    Imgc,
};

ResourceKind detect_resource_kind(ByteView bytes);

using ResourceHeader = std::variant<Arc0Header, XpckHeader, Xq32Header, ImgcHeader>;
ParseResult<ResourceHeader> parse_resource_header(ByteView bytes);

}  // namespace ykw2::core
