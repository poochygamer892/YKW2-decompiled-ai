#include "ykw2/core/level5_decode.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include <zlib.h>

namespace {

void put32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

void test_none() {
    std::vector<std::uint8_t> compressed;
    put32(compressed, 3u << 3);
    compressed.insert(compressed.end(), {'a', 'b', 'c'});
    const auto decoded = ykw2::core::decode_level5(
        {compressed.data(), compressed.size()});
    assert(decoded);
    assert(std::string(decoded.value->begin(), decoded.value->end()) == "abc");
}

void test_lz10() {
    std::vector<std::uint8_t> compressed;
    put32(compressed, (9u << 3) | 1u);
    compressed.insert(compressed.end(), {0x10, 'A', 'B', 'C', 0x30, 0x02});
    const auto decoded = ykw2::core::decode_level5(
        {compressed.data(), compressed.size()});
    assert(decoded);
    assert(std::string(decoded.value->begin(), decoded.value->end()) == "ABCABCABC");
}

void test_rle() {
    std::vector<std::uint8_t> compressed;
    put32(compressed, (6u << 3) | 4u);
    compressed.insert(compressed.end(), {0x02, 'a', 'b', 'c', 0x80, 'd'});
    const auto decoded = ykw2::core::decode_level5(
        {compressed.data(), compressed.size()});
    assert(decoded);
    assert(std::string(decoded.value->begin(), decoded.value->end()) == "abcddd");
}

void test_huffman8_and_huffman4() {
    std::vector<std::uint8_t> huffman8;
    put32(huffman8, (2u << 3) | 3u);
    huffman8.insert(huffman8.end(), {1, 0xC0, 'A', 'B', 0, 0, 0, 0x80});
    const auto decoded8 = ykw2::core::decode_level5(
        {huffman8.data(), huffman8.size()});
    assert(decoded8);
    assert(std::string(decoded8.value->begin(), decoded8.value->end()) == "BA");

    std::vector<std::uint8_t> huffman4;
    put32(huffman4, (1u << 3) | 2u);
    huffman4.insert(huffman4.end(), {1, 0xC0, 1, 2, 0, 0, 0, 0x40});
    const auto decoded4 = ykw2::core::decode_level5(
        {huffman4.data(), huffman4.size()});
    assert(decoded4);
    assert(decoded4.value->size() == 1);
    assert((*decoded4.value)[0] == 0x21);
}

void test_zlib() {
    const std::string input = "YW2 real resource decoder";
    uLongf compressed_size = compressBound(input.size());
    std::vector<std::uint8_t> payload(compressed_size);
    const auto status = compress2(payload.data(), &compressed_size,
                                  reinterpret_cast<const Bytef*>(input.data()),
                                  input.size(), Z_BEST_COMPRESSION);
    assert(status == Z_OK);
    payload.resize(compressed_size);

    std::vector<std::uint8_t> compressed;
    put32(compressed, (static_cast<std::uint32_t>(input.size()) << 3) | 5u);
    compressed.insert(compressed.end(), payload.begin(), payload.end());
    const auto decoded = ykw2::core::decode_level5(
        {compressed.data(), compressed.size()});
    assert(decoded);
    assert(std::string(decoded.value->begin(), decoded.value->end()) == input);
}

void test_rejects_corrupt_streams() {
    std::vector<std::uint8_t> bad_lz;
    put32(bad_lz, (3u << 3) | 1u);
    bad_lz.insert(bad_lz.end(), {0x80, 0x00, 0x00});
    const auto lz_result = ykw2::core::decode_level5(
        {bad_lz.data(), bad_lz.size()});
    assert(!lz_result);
    assert(lz_result.error->code == "level5_invalid_lz10_displacement");

    std::vector<std::uint8_t> zlib;
    put32(zlib, (1u << 3) | 5u);
    zlib.push_back(0);
    const auto zlib_result = ykw2::core::decode_level5(
        {zlib.data(), zlib.size()});
    assert(!zlib_result);
    assert(zlib_result.error->code == "level5_zlib_invalid_stream");
}

}  // namespace

int main() {
    test_none();
    test_lz10();
    test_rle();
    test_huffman8_and_huffman4();
    test_zlib();
    test_rejects_corrupt_streams();
    return 0;
}
