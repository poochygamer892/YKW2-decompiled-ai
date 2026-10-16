#include "ykw2/core/level5_decode.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include <zlib.h>

#include "ykw2/core/formats.hpp"

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset) {
    return ParseResult<T>::failure(code, offset);
}

class Cursor {
public:
    explicit Cursor(ByteView bytes) : bytes_(bytes) {}

    std::optional<std::uint8_t> read_u8() {
        const auto value = bytes_.u8(offset_);
        if (value) {
            ++offset_;
        }
        return value;
    }

    std::optional<std::uint16_t> read_u16_le() {
        const auto value = bytes_.u16_le(offset_);
        if (value) {
            offset_ += 2;
        }
        return value;
    }

    std::optional<std::uint32_t> read_u32_le() {
        const auto value = bytes_.u32_le(offset_);
        if (value) {
            offset_ += 4;
        }
        return value;
    }

    std::size_t offset() const { return offset_; }

private:
    ByteView bytes_;
    std::size_t offset_ = 0;
};

ParseResult<std::vector<std::uint8_t>> decode_none(Cursor& cursor,
                                                   std::uint32_t size,
                                                   ByteView compressed) {
    if (!compressed.contains(cursor.offset(), size)) {
        return failure<std::vector<std::uint8_t>>("level5_truncated_none", cursor.offset());
    }
    std::vector<std::uint8_t> output(size);
    if (size != 0) {
        std::copy_n(compressed.data() + cursor.offset(), size, output.data());
    }
    return ParseResult<std::vector<std::uint8_t>>::success(std::move(output));
}

ParseResult<std::vector<std::uint8_t>> decode_lz10(Cursor& cursor,
                                                   std::uint32_t size) {
    std::vector<std::uint8_t> output;
    output.reserve(size);
    std::array<std::uint8_t, 0x1000> circular{};
    std::size_t write_position = 0;
    std::uint8_t flags = 0;
    std::uint8_t mask = 1;

    while (output.size() < size) {
        if (mask == 1) {
            const auto next_flags = cursor.read_u8();
            if (!next_flags) {
                return failure<std::vector<std::uint8_t>>("level5_truncated_lz10_flags",
                                                           cursor.offset());
            }
            flags = *next_flags;
            mask = 0x80;
        } else {
            mask >>= 1;
        }

        if ((flags & mask) != 0) {
            const auto first = cursor.read_u8();
            const auto second = cursor.read_u8();
            if (!first || !second) {
                return failure<std::vector<std::uint8_t>>("level5_truncated_lz10_pair",
                                                           cursor.offset());
            }
            const auto length = static_cast<std::size_t>((*first >> 4) + 3);
            const auto displacement = static_cast<std::size_t>((*first & 0x0f) << 8 | *second) + 1;
            if (displacement > output.size()) {
                return failure<std::vector<std::uint8_t>>("level5_invalid_lz10_displacement",
                                                           cursor.offset() - 2);
            }
            for (std::size_t index = 0; index < length && output.size() < size; ++index) {
                const auto value = circular[(write_position + 0x1000 - displacement) & 0x0fff];
                output.push_back(value);
                circular[write_position] = value;
                write_position = (write_position + 1) & 0x0fff;
            }
        } else {
            const auto value = cursor.read_u8();
            if (!value) {
                return failure<std::vector<std::uint8_t>>("level5_truncated_lz10_literal",
                                                           cursor.offset());
            }
            output.push_back(*value);
            circular[write_position] = *value;
            write_position = (write_position + 1) & 0x0fff;
        }
    }
    return ParseResult<std::vector<std::uint8_t>>::success(std::move(output));
}

ParseResult<std::vector<std::uint8_t>> decode_huffman(Cursor& cursor,
                                                      std::uint32_t size,
                                                      std::uint8_t bit_depth) {
    const auto tree_size = cursor.read_u8();
    const auto tree_root = cursor.read_u8();
    if (!tree_size || !tree_root) {
        return failure<std::vector<std::uint8_t>>("level5_truncated_huffman_header",
                                                   cursor.offset());
    }
    const auto tree_bytes = static_cast<std::size_t>(*tree_size) * 2;
    if (tree_bytes > std::numeric_limits<std::size_t>::max() - cursor.offset()) {
        return failure<std::vector<std::uint8_t>>("level5_huffman_tree_overflow", cursor.offset());
    }
    std::vector<std::uint8_t> tree(tree_bytes);
    for (auto& value : tree) {
        const auto byte = cursor.read_u8();
        if (!byte) {
            return failure<std::vector<std::uint8_t>>("level5_truncated_huffman_tree",
                                                       cursor.offset());
        }
        value = *byte;
    }

    const auto symbol_count = static_cast<std::size_t>(size) * 8 / bit_depth;
    std::vector<std::uint8_t> symbols;
    symbols.reserve(symbol_count);
    std::uint32_t code = 0;
    std::size_t bit_index = 0;
    std::uint8_t position = *tree_root;
    std::size_t next_index = 0;
    while (symbols.size() < symbol_count) {
        if ((bit_index % 32) == 0) {
            const auto next_code = cursor.read_u32_le();
            if (!next_code) {
                return failure<std::vector<std::uint8_t>>("level5_truncated_huffman_bits",
                                                           cursor.offset());
            }
            code = *next_code;
        }

        const auto tree_step = static_cast<std::size_t>((position & 0x3f) << 1) + 2;
        if (next_index > std::numeric_limits<std::size_t>::max() - tree_step) {
            return failure<std::vector<std::uint8_t>>("level5_huffman_tree_index_overflow",
                                                       cursor.offset());
        }
        next_index += tree_step;
        const auto direction = ((code >> (31 - (bit_index % 32))) & 1u) == 0 ? 2u : 1u;
        const auto leaf = (((position >> 5) >> direction) & 1u) != 0;
        if (next_index < direction || next_index - direction >= tree.size()) {
            return failure<std::vector<std::uint8_t>>("level5_invalid_huffman_pointer",
                                                       cursor.offset());
        }
        position = tree[next_index - direction];
        if (leaf) {
            symbols.push_back(position);
            position = *tree_root;
            next_index = 0;
        }
        ++bit_index;
    }

    if (bit_depth == 8) {
        return ParseResult<std::vector<std::uint8_t>>::success(std::move(symbols));
    }
    std::vector<std::uint8_t> output(size);
    for (std::size_t index = 0; index < size; ++index) {
        output[index] = static_cast<std::uint8_t>(symbols[index * 2] |
                                                  (symbols[index * 2 + 1] << 4));
    }
    return ParseResult<std::vector<std::uint8_t>>::success(std::move(output));
}

ParseResult<std::vector<std::uint8_t>> decode_rle(Cursor& cursor,
                                                  std::uint32_t size) {
    std::vector<std::uint8_t> output;
    output.reserve(size);
    while (output.size() < size) {
        const auto flag = cursor.read_u8();
        if (!flag) {
            return failure<std::vector<std::uint8_t>>("level5_truncated_rle", cursor.offset());
        }
        if ((*flag & 0x80) != 0) {
            const auto value = cursor.read_u8();
            if (!value) {
                return failure<std::vector<std::uint8_t>>("level5_truncated_rle_repeat",
                                                           cursor.offset());
            }
            const auto repetitions = static_cast<std::size_t>((*flag & 0x7f) + 3);
            output.insert(output.end(), repetitions, *value);
        } else {
            const auto length = static_cast<std::size_t>(*flag) + 1;
            for (std::size_t index = 0; index < length; ++index) {
                const auto value = cursor.read_u8();
                if (!value) {
                    return failure<std::vector<std::uint8_t>>("level5_truncated_rle_literal",
                                                               cursor.offset());
                }
                output.push_back(*value);
            }
        }
        if (output.size() > size) {
            output.resize(size);
        }
    }
    return ParseResult<std::vector<std::uint8_t>>::success(std::move(output));
}

ParseResult<std::vector<std::uint8_t>> decode_zlib(Cursor& cursor,
                                                  std::uint32_t size,
                                                  ByteView compressed) {
    const auto input_size = compressed.size() - cursor.offset();
    if (input_size > std::numeric_limits<uInt>::max() ||
        size >= std::numeric_limits<uInt>::max()) {
        return failure<std::vector<std::uint8_t>>("level5_zlib_size_unsupported", 0);
    }

    // One extra byte lets us detect a stream whose real output exceeds the
    // size declared by the Level-5 header without allocating unbounded memory.
    std::vector<std::uint8_t> output(static_cast<std::size_t>(size) + 1);
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(compressed.data() + cursor.offset());
    stream.avail_in = static_cast<uInt>(input_size);
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    if (inflateInit(&stream) != Z_OK) {
        return failure<std::vector<std::uint8_t>>("level5_zlib_init_failed", 0);
    }
    const auto status = inflate(&stream, Z_FINISH);
    const auto actual_size = stream.total_out;
    inflateEnd(&stream);
    if (status != Z_STREAM_END) {
        return failure<std::vector<std::uint8_t>>("level5_zlib_invalid_stream", 0);
    }
    if (actual_size != size) {
        return failure<std::vector<std::uint8_t>>("level5_zlib_output_size_mismatch", 0);
    }
    output.resize(size);
    return ParseResult<std::vector<std::uint8_t>>::success(std::move(output));
}

}  // namespace

ParseResult<std::vector<std::uint8_t>> decode_level5(
    ByteView compressed, Level5DecodeOptions options) {
    const auto header = parse_level5_header(compressed);
    if (!header) {
        return failure<std::vector<std::uint8_t>>(header.error->code, header.error->offset);
    }
    if (header.value->decoded_size > options.max_output_size) {
        return failure<std::vector<std::uint8_t>>("level5_output_limit_exceeded", 0);
    }

    Cursor cursor(compressed.subview(4, compressed.size() - 4));
    ParseResult<std::vector<std::uint8_t>> decoded;
    switch (header.value->method) {
        case Level5Method::None:
            decoded = decode_none(cursor, header.value->decoded_size,
                                  compressed.subview(4, compressed.size() - 4));
            break;
        case Level5Method::Lz10:
            decoded = decode_lz10(cursor, header.value->decoded_size);
            break;
        case Level5Method::Huffman4:
            decoded = decode_huffman(cursor, header.value->decoded_size, 4);
            break;
        case Level5Method::Huffman8:
            decoded = decode_huffman(cursor, header.value->decoded_size, 8);
            break;
        case Level5Method::Rle:
            decoded = decode_rle(cursor, header.value->decoded_size);
            break;
        case Level5Method::Zlib:
            decoded = decode_zlib(cursor, header.value->decoded_size,
                                  compressed.subview(4, compressed.size() - 4));
            break;
        default:
            return failure<std::vector<std::uint8_t>>("level5_unknown_method", 0);
    }
    if (!decoded) {
        return decoded;
    }
    if (decoded.value->size() != header.value->decoded_size) {
        return failure<std::vector<std::uint8_t>>("level5_output_size_mismatch", 0);
    }
    return decoded;
}

}  // namespace ykw2::core
