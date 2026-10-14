#include "ykw2/core/imgc_reader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include "ykw2/core/formats.hpp"
#include "ykw2/core/level5_decode.hpp"

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset = 0) {
    return ParseResult<T>::failure(code, offset);
}

struct ImageHeader {
    std::uint8_t format = 0;
    std::uint8_t image_count = 0;
    std::uint8_t bit_depth = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t palette_info_offset = 0;
    std::uint16_t palette_info_count = 0;
    std::uint16_t image_info_offset = 0;
    std::uint16_t image_info_count = 0;
    std::uint32_t data_offset = 0;
    std::uint32_t platform = 0;
};

struct ImageEntry {
    std::uint32_t tile_offset = 0;
    std::uint32_t tile_size = 0;
    std::uint32_t image_offset = 0;
    std::uint32_t image_size = 0;
};

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

std::uint8_t scale_bits(std::uint32_t value, std::uint8_t bits) {
    if (bits == 0) {
        return 0;
    }
    if (bits == 8) {
        return static_cast<std::uint8_t>(value);
    }
    const auto maximum = (1u << bits) - 1u;
    return static_cast<std::uint8_t>((value * 255u + maximum / 2u) / maximum);
}

bool add_fits(std::uint64_t left, std::uint64_t right, std::uint64_t limit) {
    return left <= limit && right <= limit - left;
}

ParseResult<std::vector<std::uint8_t>> read_blob(
    const ResourceStore& store, std::string_view path, std::uint64_t offset,
    std::uint32_t size, const ImgcReadOptions& options, std::string_view label) {
    // The on-disk field is uint32_t, so it is representable by size_t on all
    // supported Android/iOS targets; the configurable limit is the relevant
    // allocation guard here.
    if (size > options.max_compressed_blob_bytes) {
        return failure<std::vector<std::uint8_t>>("imgc_compressed_blob_limit", 0);
    }
    std::vector<std::uint8_t> compressed(size);
    if (!store.read_exact(path, offset, compressed.data(), compressed.size())) {
        return failure<std::vector<std::uint8_t>>(label, static_cast<std::size_t>(offset));
    }
    const auto decoded = decode_level5(
        ByteView(compressed.data(), compressed.size()),
        Level5DecodeOptions{options.max_decoded_blob_bytes});
    if (!decoded) {
        return failure<std::vector<std::uint8_t>>(
            decoded.error->code, decoded.error->offset);
    }
    return decoded;
}

ImgcPixelFormat pixel_format_for(std::uint8_t format) {
    switch (format) {
        case 0x00: return ImgcPixelFormat::Rgba8888;
        case 0x01: return ImgcPixelFormat::Rgba4444;
        case 0x02: return ImgcPixelFormat::Rgba5551;
        case 0x03: return ImgcPixelFormat::Bgr888;
        case 0x04: return ImgcPixelFormat::Rgb565;
        // Yo-kai Watch 2 uses the second 3DS mapping from the mobile/Level-5
        // reference: 0x1b is ETC1 and 0x1c is ETC1+A4. The latter is stored
        // at 8 bits per pixel (four 16-byte blocks per 8x8 tile).
        case 0x0b: return ImgcPixelFormat::La88;
        case 0x0c: return ImgcPixelFormat::La44;
        case 0x0d: return ImgcPixelFormat::L8;
        case 0x0e: return ImgcPixelFormat::L4;
        case 0x0f: return ImgcPixelFormat::A8;
        case 0x10: return ImgcPixelFormat::A4;
        case 0x1b: return ImgcPixelFormat::Etc1;
        case 0x1c: return ImgcPixelFormat::Etc1A4;
        case 0x1d: return ImgcPixelFormat::Etc1A4;
        default: return ImgcPixelFormat::Unknown;
    }
}

ParseResult<ImageHeader> read_header(const ResourceStore& store, std::string_view path) {
    const auto* entry = store.catalog().find(path);
    if (entry == nullptr) {
        return failure<ImageHeader>("resource_not_in_catalog");
    }
    if (entry->size < 0x40) {
        return failure<ImageHeader>("imgc_header_truncated");
    }
    std::array<std::uint8_t, 0x40> bytes{};
    if (!store.read_exact(path, 0, bytes.data(), bytes.size())) {
        return failure<ImageHeader>("imgc_header_read_failed");
    }
    const auto parsed = parse_imgc_header(ByteView(bytes.data(), bytes.size()));
    if (!parsed) {
        return failure<ImageHeader>(parsed.error->code, parsed.error->offset);
    }
    const ByteView view(bytes.data(), bytes.size());
    const auto format = view.u8(0x0a);
    const auto image_count = view.u8(0x0c);
    const auto bit_depth = view.u8(0x0d);
    const auto width = view.u16_le(0x10);
    const auto height = view.u16_le(0x12);
    const auto palette_offset = view.u16_le(0x14);
    const auto palette_count = view.u16_le(0x16);
    const auto image_offset = view.u16_le(0x18);
    const auto image_count_info = view.u16_le(0x1a);
    const auto data_offset = view.u32_le(0x1c);
    const auto platform = view.u32_le(0x20);
    if (!format || !image_count || !bit_depth || !width || !height ||
        !palette_offset || !image_offset || !data_offset || !platform ||
        *image_count == 0 || *image_count_info == 0) {
        return failure<ImageHeader>("imgc_header_fields_truncated", 0x0a);
    }
    if (*data_offset > entry->size) {
        return failure<ImageHeader>("imgc_data_offset_out_of_bounds", 0x1c);
    }
    return ParseResult<ImageHeader>::success({
        *format, *image_count, *bit_depth, *width, *height,
        *palette_offset, *palette_count, *image_offset, *image_count_info,
        *data_offset, *platform});
}

ParseResult<ImageEntry> read_image_entry(
    const ResourceStore& store, std::string_view path,
    const ImageHeader& header, std::uint64_t image_size) {
    const auto entry_offset = static_cast<std::uint64_t>(header.image_info_offset);
    if (!add_fits(entry_offset, 16, image_size)) {
        return failure<ImageEntry>("imgc_image_entry_out_of_bounds", header.image_info_offset);
    }
    std::array<std::uint8_t, 16> bytes{};
    if (!store.read_exact(path, entry_offset, bytes.data(), bytes.size())) {
        return failure<ImageEntry>("imgc_image_entry_read_failed", header.image_info_offset);
    }
    const ByteView view(bytes.data(), bytes.size());
    const auto tile_offset = view.u32_le(0);
    const auto tile_size = view.u32_le(4);
    const auto image_offset = view.u32_le(8);
    const auto image_size_value = view.u32_le(12);
    if (!tile_size || !image_size_value) {
        return failure<ImageEntry>("imgc_image_entry_truncated", header.image_info_offset);
    }
    const auto tile_payload_offset = static_cast<std::uint64_t>(header.data_offset) +
                                     *tile_offset;
    const auto image_payload_offset = static_cast<std::uint64_t>(header.data_offset) +
                                      *image_offset;
    if (!add_fits(header.data_offset, *tile_offset, image_size) ||
        !add_fits(tile_payload_offset, *tile_size, image_size) ||
        !add_fits(header.data_offset, *image_offset, image_size) ||
        !add_fits(image_payload_offset, *image_size_value, image_size)) {
        return failure<ImageEntry>("imgc_payload_out_of_bounds", header.image_info_offset);
    }
    return ParseResult<ImageEntry>::success({
        *tile_offset, *tile_size, *image_offset, *image_size_value});
}

std::size_t morton_pixel_index(std::size_t x, std::size_t y) {
    // CTR textures store each 8x8 tile in Morton order. The least
    // significant bit belongs to Y, followed by X, then Y, then X.
    std::size_t result = 0;
    for (std::size_t bit = 0; bit < 3; ++bit) {
        result |= ((y >> bit) & 1u) << (bit * 2);
        result |= ((x >> bit) & 1u) << (bit * 2 + 1);
    }
    return result;
}

int sign3(std::uint8_t value) {
    return (static_cast<int>(value) + 4) % 8 - 4;
}

int clamp_color(int value) {
    return std::max(0, std::min(255, value));
}

constexpr int kEtcModifiers[8][4] = {
    {2, 8, -2, -8}, {5, 17, -5, -17}, {9, 29, -9, -29},
    {13, 42, -13, -42}, {18, 60, -18, -60}, {24, 80, -24, -80},
    {33, 106, -33, -106}, {47, 183, -47, -183}};

constexpr std::array<int, 16> kEtcZOrder = {
    0, 4, 1, 5, 8, 12, 9, 13, 2, 6, 3, 7, 10, 14, 11, 15};

std::array<Rgba, 16> decode_etc_block(const std::uint8_t* bytes, bool alpha) {
    const auto alpha_word = alpha
                                ? (static_cast<std::uint64_t>(bytes[0]) |
                                   static_cast<std::uint64_t>(bytes[1]) << 8 |
                                   static_cast<std::uint64_t>(bytes[2]) << 16 |
                                   static_cast<std::uint64_t>(bytes[3]) << 24 |
                                   static_cast<std::uint64_t>(bytes[4]) << 32 |
                                   static_cast<std::uint64_t>(bytes[5]) << 40 |
                                   static_cast<std::uint64_t>(bytes[6]) << 48 |
                                   static_cast<std::uint64_t>(bytes[7]) << 56)
                                : 0;
    const auto color_base = bytes + (alpha ? 8 : 0);
    const auto lsb = static_cast<std::uint16_t>(color_base[0] | color_base[1] << 8);
    const auto msb = static_cast<std::uint16_t>(color_base[2] | color_base[3] << 8);
    const auto flags = color_base[4];
    const auto blue = color_base[5];
    const auto green = color_base[6];
    const auto red = color_base[7];
    const bool diff = (flags & 2) != 0;
    const bool flip = (flags & 1) != 0;
    const auto table0 = (flags >> 5) & 7;
    const auto table1 = (flags >> 2) & 7;

    Rgba color0{};
    Rgba color1{};
    if (diff) {
        color0 = {scale_bits(red >> 3, 5), scale_bits(green >> 3, 5),
                  scale_bits(blue >> 3, 5), 255};
        color1 = {
            static_cast<std::uint8_t>(clamp_color(color0.r + sign3(red & 7))),
            static_cast<std::uint8_t>(clamp_color(color0.g + sign3(green & 7))),
            static_cast<std::uint8_t>(clamp_color(color0.b + sign3(blue & 7))), 255};
    } else {
        color0 = {scale_bits(red >> 4, 4), scale_bits(green >> 4, 4),
                  scale_bits(blue >> 4, 4), 255};
        color1 = {scale_bits(red & 0x0f, 4), scale_bits(green & 0x0f, 4),
                  scale_bits(blue & 0x0f, 4), 255};
    }

    std::array<Rgba, 16> result{};
    const auto flip_mask = flip ? 2 : 8;
    for (std::size_t output = 0; output < result.size(); ++output) {
        const auto selector = kEtcZOrder[output];
        const auto base = (selector & flip_mask) == 0 ? color0 : color1;
        const auto table = (selector & flip_mask) == 0 ? table0 : table1;
        const auto modifier = ((msb >> selector) & 1u) * 2u |
                              ((lsb >> selector) & 1u);
        auto pixel = base;
        pixel.r = static_cast<std::uint8_t>(clamp_color(
            pixel.r + kEtcModifiers[table][modifier]));
        pixel.g = static_cast<std::uint8_t>(clamp_color(
            pixel.g + kEtcModifiers[table][modifier]));
        pixel.b = static_cast<std::uint8_t>(clamp_color(
            pixel.b + kEtcModifiers[table][modifier]));
        if (alpha) {
            pixel.a = static_cast<std::uint8_t>(((alpha_word >> (selector * 4)) & 0x0f) * 17);
        }
        result[output] = pixel;
    }
    return result;
}

ParseResult<Rgba> decode_uncompressed_pixel(
    const std::uint8_t* data, ImgcPixelFormat format, std::size_t pixel) {
    switch (format) {
        case ImgcPixelFormat::Rgba8888: {
            const auto* value = data + pixel * 4;
            return ParseResult<Rgba>::success({value[3], value[2], value[1], value[0]});
        }
        case ImgcPixelFormat::Rgba4444: {
            const auto value = static_cast<std::uint16_t>(data[pixel * 2] |
                                                           data[pixel * 2 + 1] << 8);
            return ParseResult<Rgba>::success({
                scale_bits(value >> 12, 4), scale_bits(value >> 8, 4),
                scale_bits(value >> 4, 4), scale_bits(value, 4)});
        }
        case ImgcPixelFormat::Rgba5551: {
            const auto value = static_cast<std::uint16_t>(data[pixel * 2] |
                                                           data[pixel * 2 + 1] << 8);
            return ParseResult<Rgba>::success({
                scale_bits(value >> 11, 5), scale_bits(value >> 6, 5),
                scale_bits(value >> 1, 5), scale_bits(value, 1)});
        }
        case ImgcPixelFormat::Bgr888: {
            const auto* value = data + pixel * 3;
            return ParseResult<Rgba>::success({value[0], value[1], value[2], 255});
        }
        case ImgcPixelFormat::Rgb565: {
            const auto value = static_cast<std::uint16_t>(data[pixel * 2] |
                                                           data[pixel * 2 + 1] << 8);
            return ParseResult<Rgba>::success({
                scale_bits(value >> 11, 5), scale_bits(value >> 5, 6),
                scale_bits(value, 5), 255});
        }
        case ImgcPixelFormat::La88: {
            const auto* value = data + pixel * 2;
            return ParseResult<Rgba>::success({value[1], value[1], value[1], value[0]});
        }
        case ImgcPixelFormat::La44: {
            const auto value = data[pixel];
            return ParseResult<Rgba>::success({
                scale_bits(value >> 4, 4), scale_bits(value >> 4, 4),
                scale_bits(value >> 4, 4), scale_bits(value, 4)});
        }
        case ImgcPixelFormat::L8:
            return ParseResult<Rgba>::success({data[pixel], data[pixel], data[pixel], 255});
        case ImgcPixelFormat::L4: {
            const auto value = data[pixel / 2];
            const auto luminance = static_cast<std::uint8_t>(
                ((pixel & 1) == 0 ? value >> 4 : value & 0x0f) * 17);
            return ParseResult<Rgba>::success({luminance, luminance, luminance, 255});
        }
        case ImgcPixelFormat::A8:
            return ParseResult<Rgba>::success({255, 255, 255, data[pixel]});
        case ImgcPixelFormat::A4: {
            const auto value = data[pixel / 2];
            const auto alpha = static_cast<std::uint8_t>(
                ((pixel & 1) == 0 ? value >> 4 : value & 0x0f) * 17);
            return ParseResult<Rgba>::success({255, 255, 255, alpha});
        }
        default:
            return failure<Rgba>("imgc_not_uncompressed_format");
    }
}

}  // namespace

ParseResult<ImgcImage> decode_imgc_image(
    const ResourceStore& store, std::string_view path, ImgcReadOptions options) {
    const auto* catalog_entry = store.catalog().find(path);
    if (catalog_entry == nullptr) {
        return failure<ImgcImage>("resource_not_in_catalog");
    }
    const auto header = read_header(store, path);
    if (!header) {
        return failure<ImgcImage>(header.error->code, header.error->offset);
    }
    const auto pixel_format = pixel_format_for(header.value->format);
    if (pixel_format == ImgcPixelFormat::Unknown) {
        return failure<ImgcImage>("imgc_unsupported_format", 0x0a);
    }
    if (header.value->palette_info_count != 0) {
        return failure<ImgcImage>("imgc_palette_not_supported", 0x16);
    }
    const auto image_entry = read_image_entry(
        store, path, *header.value, catalog_entry->size);
    if (!image_entry) {
        return failure<ImgcImage>(image_entry.error->code, image_entry.error->offset);
    }

    const auto tile_blob_offset = static_cast<std::uint64_t>(header.value->data_offset) +
                                  image_entry.value->tile_offset;
    const auto image_blob_offset = static_cast<std::uint64_t>(header.value->data_offset) +
                                   image_entry.value->image_offset;
    const auto tile_data = read_blob(
        store, path, tile_blob_offset, image_entry.value->tile_size, options,
        "imgc_tile_blob_read_failed");
    if (!tile_data) {
        return failure<ImgcImage>(tile_data.error->code, tile_data.error->offset);
    }
    const auto image_data = read_blob(
        store, path, image_blob_offset, image_entry.value->image_size, options,
        "imgc_image_blob_read_failed");
    if (!image_data) {
        return failure<ImgcImage>(image_data.error->code, image_data.error->offset);
    }

    const auto padded_width = (static_cast<std::size_t>(header.value->width) + 7) & ~7u;
    const auto padded_height = (static_cast<std::size_t>(header.value->height) + 7) & ~7u;
    const auto tile_bytes = static_cast<std::size_t>(64u * header.value->bit_depth / 8u);
    if (tile_bytes == 0 || padded_width == 0 || padded_height == 0) {
        return failure<ImgcImage>("imgc_invalid_dimensions");
    }
    const auto tile_count = tile_data.value->size() >= 8 &&
                                    ((*tile_data.value)[0] == 0x53 &&
                                     (*tile_data.value)[1] == 0x04)
                                ? (tile_data.value->size() - 8) / 4
                                : tile_data.value->size() / 2;
    if (tile_count == 0 || tile_count > std::numeric_limits<std::size_t>::max() / tile_bytes) {
        return failure<ImgcImage>("imgc_invalid_tile_table");
    }
    const auto combined_size = tile_count * tile_bytes;
    if (combined_size > options.max_decoded_blob_bytes) {
        return failure<ImgcImage>("imgc_combined_blob_limit");
    }
    std::vector<std::uint8_t> combined(combined_size, 0);
    const bool legacy_tiles = tile_data.value->size() >= 8 &&
                              (*tile_data.value)[0] == 0x53 &&
                              (*tile_data.value)[1] == 0x04;
    const auto tile_table_offset = legacy_tiles ? 8u : 0u;
    const auto tile_entry_size = legacy_tiles ? 4u : 2u;
    for (std::size_t index = 0; index < tile_count; ++index) {
        const auto offset = tile_table_offset + index * tile_entry_size;
        std::int64_t source_tile = 0;
        if (legacy_tiles) {
            const auto value = static_cast<std::uint32_t>((*tile_data.value)[offset]) |
                               static_cast<std::uint32_t>((*tile_data.value)[offset + 1]) << 8 |
                               static_cast<std::uint32_t>((*tile_data.value)[offset + 2]) << 16 |
                               static_cast<std::uint32_t>((*tile_data.value)[offset + 3]) << 24;
            source_tile = static_cast<std::int32_t>(value);
        } else {
            const auto value = static_cast<std::uint16_t>((*tile_data.value)[offset]) |
                               static_cast<std::uint16_t>((*tile_data.value)[offset + 1]) << 8;
            source_tile = static_cast<std::int16_t>(value);
        }
        if (source_tile < 0) {
            continue;
        }
        const auto source_offset = static_cast<std::uint64_t>(source_tile) * tile_bytes;
        const auto destination_offset = static_cast<std::uint64_t>(index) * tile_bytes;
        if (source_offset > image_data.value->size() ||
            tile_bytes > image_data.value->size() - source_offset) {
            return failure<ImgcImage>("imgc_tile_data_out_of_bounds", offset);
        }
        std::copy_n(image_data.value->data() + source_offset, tile_bytes,
                    combined.data() + destination_offset);
    }

    const auto rgba_size = static_cast<std::uint64_t>(header.value->width) *
                           header.value->height * 4u;
    if (rgba_size > options.max_rgba_bytes ||
        rgba_size > std::numeric_limits<std::size_t>::max()) {
        return failure<ImgcImage>("imgc_rgba_output_limit");
    }
    ImgcImage result;
    result.width = header.value->width;
    result.height = header.value->height;
    result.format = header.value->format;
    result.bit_depth = header.value->bit_depth;
    result.platform = header.value->platform;
    result.pixel_format = pixel_format;
    result.rgba.resize(static_cast<std::size_t>(rgba_size));

    const bool etc = pixel_format == ImgcPixelFormat::Etc1 ||
                     pixel_format == ImgcPixelFormat::Etc1A4;
    if (!etc) {
        const auto expected_bit_depth = pixel_format == ImgcPixelFormat::Rgba8888 ? 32u :
                                        pixel_format == ImgcPixelFormat::Bgr888 ? 24u :
                                        pixel_format == ImgcPixelFormat::L4 ||
                                                pixel_format == ImgcPixelFormat::A4 ? 4u :
                                        pixel_format == ImgcPixelFormat::L8 ||
                                                pixel_format == ImgcPixelFormat::A8 ||
                                                pixel_format == ImgcPixelFormat::La44 ? 8u :
                                        16u;
        if (header.value->bit_depth != expected_bit_depth ||
            (static_cast<std::uint64_t>(padded_width) * padded_height *
                 header.value->bit_depth + 7u) / 8u > combined.size()) {
            return failure<ImgcImage>("imgc_pixel_data_out_of_bounds");
        }
        for (std::size_t y = 0; y < header.value->height; ++y) {
            for (std::size_t x = 0; x < header.value->width; ++x) {
                const auto tile_x = x / 8;
                const auto tile_y = y / 8;
                const auto tile_index = tile_y * (padded_width / 8) + tile_x;
                if (tile_index >= tile_count) {
                    return failure<ImgcImage>("imgc_tile_index_out_of_bounds");
                }
                const auto source_pixel = static_cast<std::uint64_t>(tile_index) * 64u +
                                           morton_pixel_index(x & 7u, y & 7u);
                const auto byte_depth = header.value->bit_depth / 8u;
                const auto* source_data = combined.data();
                auto source_index = static_cast<std::size_t>(source_pixel);
                if (byte_depth != 0) {
                    source_data += source_pixel * byte_depth;
                    source_index = 0;
                }
                const auto pixel = decode_uncompressed_pixel(
                    source_data, pixel_format, source_index);
                if (!pixel) {
                    return failure<ImgcImage>(pixel.error->code, pixel.error->offset);
                }
                const auto destination = (y * header.value->width + x) * 4;
                result.rgba[destination + 0] = pixel.value->r;
                result.rgba[destination + 1] = pixel.value->g;
                result.rgba[destination + 2] = pixel.value->b;
                result.rgba[destination + 3] = pixel.value->a;
            }
        }
        return ParseResult<ImgcImage>::success(std::move(result));
    }

    if (header.value->bit_depth != (pixel_format == ImgcPixelFormat::Etc1A4 ? 8 : 4) ||
        combined.size() < (padded_width / 8) * (padded_height / 8) *
                              static_cast<std::size_t>(pixel_format == ImgcPixelFormat::Etc1A4 ? 64 : 32)) {
        return failure<ImgcImage>("imgc_etc_data_out_of_bounds");
    }
    const auto tiles_wide = padded_width / 8;
    for (std::size_t y = 0; y < header.value->height; ++y) {
        for (std::size_t x = 0; x < header.value->width; ++x) {
            const auto tile_index = (y / 8) * tiles_wide + x / 8;
            const auto tile_size = pixel_format == ImgcPixelFormat::Etc1A4 ? 64u : 32u;
            const auto local_x = x & 7u;
            const auto local_y = y & 7u;
            const auto block_index = (local_y / 4) * 2 + local_x / 4;
            const auto block_size = pixel_format == ImgcPixelFormat::Etc1A4 ? 16u : 8u;
            const auto block_offset = tile_index * tile_size + block_index * block_size;
            if (block_offset > combined.size() || block_size > combined.size() - block_offset) {
                return failure<ImgcImage>("imgc_etc_block_out_of_bounds");
            }
            const auto block = decode_etc_block(
                combined.data() + block_offset, pixel_format == ImgcPixelFormat::Etc1A4);
            const auto color = block[(local_y & 3u) * 4 + (local_x & 3u)];
            const auto destination = (y * header.value->width + x) * 4;
            result.rgba[destination + 0] = color.r;
            result.rgba[destination + 1] = color.g;
            result.rgba[destination + 2] = color.b;
            result.rgba[destination + 3] = color.a;
        }
    }
    return ParseResult<ImgcImage>::success(std::move(result));
}

}  // namespace ykw2::core
