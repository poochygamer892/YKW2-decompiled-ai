#pragma once

#include <cstdint>
#include <vector>

#include "ykw2/core/resource_store.hpp"

namespace ykw2::core {

enum class ImgcPixelFormat {
    Unknown,
    Rgba8888,
    Rgba4444,
    Rgba5551,
    Bgr888,
    Rgb565,
    La88,
    La44,
    L8,
    L4,
    A8,
    A4,
    Etc1,
    Etc1A4,
};

struct ImgcReadOptions {
    std::uint64_t max_compressed_blob_bytes = 64ull * 1024ull * 1024ull;
    std::uint64_t max_decoded_blob_bytes = 64ull * 1024ull * 1024ull;
    std::uint64_t max_rgba_bytes = 64ull * 1024ull * 1024ull;
};

struct ImgcImage {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t format = 0;
    std::uint8_t bit_depth = 0;
    std::uint32_t platform = 0;
    ImgcPixelFormat pixel_format = ImgcPixelFormat::Unknown;

    // Row-major RGBA8 pixels, ready for an Android Bitmap, a Metal texture,
    // or an OpenGL/Vulkan upload. Mipmap surfaces are intentionally deferred.
    std::vector<std::uint8_t> rgba;
};

ParseResult<ImgcImage> decode_imgc_image(
    const ResourceStore& store, std::string_view path,
    ImgcReadOptions options = {});

}  // namespace ykw2::core
