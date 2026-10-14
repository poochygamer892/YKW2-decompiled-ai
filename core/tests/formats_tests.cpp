#include "ykw2/core/formats.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using ykw2::core::ByteView;

namespace {

void put16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffu);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void putMagic(std::vector<std::uint8_t>& bytes, const char* magic) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[index] = static_cast<std::uint8_t>(magic[index]);
    }
}

void test_byte_view_and_level5() {
    const std::vector<std::uint8_t> bytes = {0x34, 0x12, 0x78, 0x56, 0x00};
    const ByteView view(bytes.data(), bytes.size());
    assert(view.u16_le(0).value() == 0x1234);
    assert(view.u32_le(0).value() == 0x56781234);
    assert(view.contains(1, 4));
    assert(!view.contains(2, 4));
    assert(view.subview(1, 2).u16_le(0).value() == 0x7812);

    const std::uint32_t method_word = (0x120u << 3) | 1u;
    std::vector<std::uint8_t> compressed_header(4);
    put32(compressed_header, 0, method_word);
    const auto level5 = ykw2::core::parse_level5_header(
        ByteView(compressed_header.data(), compressed_header.size()));
    assert(level5);
    assert(level5.value->decoded_size == 0x120);
    assert(level5.value->method == ykw2::core::Level5Method::Lz10);
}

void test_arc0() {
    std::vector<std::uint8_t> bytes(0x90);
    putMagic(bytes, "ARC0");
    put32(bytes, 0x04, 0x48);
    put32(bytes, 0x08, 0x58);
    put32(bytes, 0x0c, 0x68);
    put32(bytes, 0x10, 0x78);
    put32(bytes, 0x14, 0x80);
    put16(bytes, 0x18, 2);
    put16(bytes, 0x1a, 1);
    put32(bytes, 0x1c, 3);
    put32(bytes, 0x20, 0x30);
    put32(bytes, 0x38, 2);
    put32(bytes, 0x3c, 3);

    const auto parsed = ykw2::core::parse_arc0_header(ByteView(bytes.data(), bytes.size()));
    assert(parsed);
    assert(parsed.value->directory_entries_count == 2);
    assert(parsed.value->file_count == 3);
    assert(ykw2::core::detect_resource_kind(ByteView(bytes.data(), bytes.size())) ==
           ykw2::core::ResourceKind::Arc0);
}

void test_xpck() {
    std::vector<std::uint8_t> bytes(0x40);
    putMagic(bytes, "XPCK");
    put16(bytes, 0x04, 0x2001);  // one entry, content type 2
    put16(bytes, 0x06, 5);       // info at 0x14
    put16(bytes, 0x08, 8);       // names at 0x20
    put16(bytes, 0x0a, 12);      // data at 0x30
    put16(bytes, 0x0c, 3);       // 0x0c bytes of info
    put16(bytes, 0x0e, 3);       // 0x0c bytes of names
    put32(bytes, 0x10, 4);       // 0x10 bytes of data

    const auto parsed = ykw2::core::parse_xpck_header(ByteView(bytes.data(), bytes.size()));
    assert(parsed);
    assert(parsed.value->entry_count == 1);
    assert(parsed.value->content_type == 2);
    assert(parsed.value->data_size == 0x10);

    std::vector<std::uint8_t> entry(12);
    put32(entry, 0, 0x12345678);
    put16(entry, 4, 7);
    put16(entry, 6, 3);
    put16(entry, 8, 0x20);
    entry[10] = 1;
    entry[11] = 0;
    const auto parsed_entry = ykw2::core::parse_xpck_entry(ByteView(entry.data(), entry.size()), 0);
    assert(parsed_entry);
    assert(parsed_entry.value->hash == 0x12345678);
    assert(parsed_entry.value->file_offset == 0x4000c);
    assert(parsed_entry.value->file_size == 0x20);
}

void test_xq32() {
    std::vector<std::uint8_t> bytes(0x40);
    putMagic(bytes, "XQ32");
    put16(bytes, 0x04, 1);   // function count
    put16(bytes, 0x06, 6);   // function at 0x18
    put16(bytes, 0x08, 9);   // jump at 0x24
    put16(bytes, 0x0a, 0);   // jump count
    put16(bytes, 0x0c, 9);   // instruction at 0x24
    put16(bytes, 0x0e, 1);   // instruction count
    put16(bytes, 0x10, 11);  // argument at 0x2c
    put16(bytes, 0x12, 1);   // argument count
    put16(bytes, 0x14, 2);   // globals
    put16(bytes, 0x16, 13);  // strings at 0x34

    const auto parsed = ykw2::core::parse_xq32_header(ByteView(bytes.data(), bytes.size()));
    assert(parsed);
    assert(parsed.value->function.offset == 0x18);
    assert(parsed.value->function.end == 0x24);
    assert(parsed.value->jump.entry_count == 0);
    assert(parsed.value->instruction.entry_count == 1);
    assert(parsed.value->global_variable_count == 2);
}

void test_imgc_and_dispatch() {
    std::vector<std::uint8_t> bytes(0x40);
    putMagic(bytes, "IMGC");
    bytes[4] = '0';
    bytes[5] = '0';
    put16(bytes, 0x0e, 0x100);
    put16(bytes, 0x10, 512);
    put16(bytes, 0x12, 256);
    put32(bytes, 0x14, 0x30);
    put16(bytes, 0x18, 0x30);
    put16(bytes, 0x1a, 1);
    put32(bytes, 0x1c, 0x48);
    put32(bytes, 0x20, 3);

    const auto parsed = ykw2::core::parse_imgc_header(ByteView(bytes.data(), bytes.size()));
    assert(parsed);
    assert(parsed.value->field_0x10_u16 == 512);
    assert(parsed.value->field_0x12_u16 == 256);
    const auto dispatched = ykw2::core::parse_resource_header(
        ByteView(bytes.data(), bytes.size()));
    assert(dispatched);
    assert(std::holds_alternative<ykw2::core::ImgcHeader>(*dispatched.value));
}

}  // namespace

int main() {
    test_byte_view_and_level5();
    test_arc0();
    test_xpck();
    test_xq32();
    test_imgc_and_dispatch();
    return 0;
}
