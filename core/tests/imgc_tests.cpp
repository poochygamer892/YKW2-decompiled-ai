#include "ykw2/core/imgc_reader.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

void put16(std::vector<std::uint8_t>& bytes, std::size_t offset,
           std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset,
           std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void put_level5_none(std::vector<std::uint8_t>& bytes, std::size_t offset,
                     const std::vector<std::uint8_t>& data) {
    put32(bytes, offset, static_cast<std::uint32_t>(data.size()) << 3);
    std::copy(data.begin(), data.end(), bytes.begin() + offset + 4);
}

class MemoryStore final : public ykw2::core::ResourceStore {
public:
    explicit MemoryStore(std::map<std::string, std::vector<std::uint8_t>> files)
        : files_(std::move(files)) {
        std::vector<ykw2::core::CatalogInput> inputs;
        for (const auto& [path, data] : files_) {
            std::array<std::uint8_t, 4> signature{};
            const auto count = std::min<std::size_t>(4, data.size());
            if (count != 0) {
                std::copy_n(data.data(), count, signature.data());
            }
            inputs.push_back({path, data.size(), signature});
        }
        const auto parsed = ykw2::core::build_resource_catalog(std::move(inputs));
        assert(parsed);
        catalog_ = std::move(*parsed.value);
    }

    const ykw2::core::ResourceCatalog& catalog() const override { return catalog_; }

    bool read(std::string_view path, std::uint64_t offset,
              std::uint8_t* destination, std::size_t length) const override {
        const auto found = files_.find(std::string(path));
        if (found == files_.end() || offset > found->second.size() ||
            length > found->second.size() - offset) {
            return false;
        }
        std::copy_n(found->second.data() + offset, length, destination);
        return true;
    }

private:
    std::map<std::string, std::vector<std::uint8_t>> files_;
    ykw2::core::ResourceCatalog catalog_;
};

std::vector<std::uint8_t> make_imgc(std::uint8_t format, std::uint8_t bit_depth,
                                    const std::vector<std::uint8_t>& image_data) {
    const std::vector<std::uint8_t> tile_table = {0, 0};
    const auto tile_blob_size = 4 + tile_table.size();
    const auto image_blob_offset = 0x40 + tile_blob_size;
    std::vector<std::uint8_t> bytes(image_blob_offset + 4 + image_data.size(), 0);
    std::copy_n(std::array<std::uint8_t, 8>{'I', 'M', 'G', 'C', '0', '0', 0, 0}.data(),
                8, bytes.data());
    put16(bytes, 0x08, 0x30);
    bytes[0x0a] = format;
    bytes[0x0b] = 1;
    bytes[0x0c] = 1;
    bytes[0x0d] = bit_depth;
    put16(bytes, 0x0e, static_cast<std::uint16_t>(64u * bit_depth / 8u));
    put16(bytes, 0x10, 8);
    put16(bytes, 0x12, 8);
    put16(bytes, 0x14, 0x30);
    put16(bytes, 0x16, 0);
    put16(bytes, 0x18, 0x24);
    put16(bytes, 0x1a, 1);
    put32(bytes, 0x1c, 0x40);
    put32(bytes, 0x20, 3);
    put32(bytes, 0x24, 0);
    put32(bytes, 0x28, tile_blob_size);
    put32(bytes, 0x2c, tile_blob_size);
    put32(bytes, 0x30, image_data.size() + 4);
    put_level5_none(bytes, 0x40, tile_table);
    put_level5_none(bytes, image_blob_offset, image_data);
    return bytes;
}

void test_rgba8888_decodes_swizzled_tile() {
    std::vector<std::uint8_t> pixels(8 * 8 * 4);
    for (std::size_t index = 0; index < pixels.size(); index += 4) {
        pixels[index + 0] = 0x44;
        pixels[index + 1] = 0x33;
        pixels[index + 2] = 0x22;
        pixels[index + 3] = 0x11;
    }
    MemoryStore store({{"bg.xi", make_imgc(0x00, 32, pixels)}});
    const auto image = ykw2::core::decode_imgc_image(store, "bg.xi");
    assert(image);
    assert(image.value->width == 8 && image.value->height == 8);
    assert(image.value->pixel_format == ykw2::core::ImgcPixelFormat::Rgba8888);
    assert(image.value->rgba.size() == 8 * 8 * 4);
    assert(image.value->rgba[0] == 0x11);
    assert(image.value->rgba[1] == 0x22);
    assert(image.value->rgba[2] == 0x33);
    assert(image.value->rgba[3] == 0x44);
}

void test_rgba8888_uses_ctr_morton_order() {
    std::vector<std::uint8_t> pixels(8 * 8 * 4);
    for (std::size_t index = 0; index < 64; ++index) {
        pixels[index * 4 + 0] = 0xff;
        pixels[index * 4 + 1] = static_cast<std::uint8_t>(index);
        pixels[index * 4 + 2] = 0;
        pixels[index * 4 + 3] = 0;
    }
    MemoryStore store({{"bg.xi", make_imgc(0x00, 32, pixels)}});
    const auto image = ykw2::core::decode_imgc_image(store, "bg.xi");
    assert(image);
    // The source index is Y-low/X-high Morton order: (1,0) is source 2,
    // while (0,1) is source 1.
    assert(image.value->rgba[1 * 4 + 2] == 2);
    assert(image.value->rgba[8 * 4 + 2] == 1);
}

void test_etc1_decodes_a_color_block() {
    // Four ETC1 blocks, each with a red individual-mode base and the smallest
    // positive modifier. IMGC stores two 4x4 blocks per 8x8 tile.
    std::vector<std::uint8_t> blocks(32, 0);
    for (std::size_t block = 0; block < 4; ++block) {
        blocks[block * 8 + 7] = 0xf0;
    }
    MemoryStore store({{"bg.xi", make_imgc(0x1b, 4, blocks)}});
    const auto image = ykw2::core::decode_imgc_image(store, "bg.xi");
    assert(image);
    assert(image.value->pixel_format == ykw2::core::ImgcPixelFormat::Etc1);
    assert(image.value->rgba.size() == 8 * 8 * 4);
    assert(image.value->rgba[0] >= 250);
    assert(image.value->rgba[1] <= 3);
    assert(image.value->rgba[2] <= 3);
    assert(image.value->rgba[3] == 255);
}

void test_etc1a4_decodes_alpha_and_color_blocks() {
    std::vector<std::uint8_t> blocks(64, 0);
    for (std::size_t block = 0; block < 4; ++block) {
        const auto offset = block * 16;
        std::fill_n(blocks.begin() + offset, 8, 0xff);
        blocks[offset + 15] = 0xf0;
    }
    MemoryStore store({{"bg.xi", make_imgc(0x1c, 8, blocks)}});
    const auto image = ykw2::core::decode_imgc_image(store, "bg.xi");
    assert(image);
    assert(image.value->pixel_format == ykw2::core::ImgcPixelFormat::Etc1A4);
    assert(image.value->rgba.size() == 8 * 8 * 4);
    assert(image.value->rgba[0] >= 250);
    assert(image.value->rgba[1] <= 3);
    assert(image.value->rgba[2] <= 3);
    assert(image.value->rgba[3] == 255);
}

void test_unsupported_format_is_rejected() {
    std::vector<std::uint8_t> pixels(8 * 8 * 4, 0);
    MemoryStore store({{"bg.xi", make_imgc(0xff, 32, pixels)}});
    const auto image = ykw2::core::decode_imgc_image(store, "bg.xi");
    assert(!image);
    assert(image.error->code == "imgc_unsupported_format");
}

}  // namespace

int main() {
    test_rgba8888_decodes_swizzled_tile();
    test_rgba8888_uses_ctr_morton_order();
    test_etc1_decodes_a_color_block();
    test_etc1a4_decodes_alpha_and_color_blocks();
    test_unsupported_format_is_rejected();
    return 0;
}
