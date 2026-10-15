#include "ykw2/core/xq32_reader.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "ykw2/core/level5_decode.hpp"

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset = 0) {
    return ParseResult<T>::failure(code, offset);
}

bool is_symbol_byte(std::uint8_t byte) {
    // Names in the inspected YW2 opening program are ASCII. Preserve only
    // printable ASCII so arbitrary malformed bytes are never presented as UI.
    return byte >= 0x20 && byte <= 0x7e;
}

std::uint32_t crc32(std::string_view value) {
    std::uint32_t result = 0xffffffffu;
    for (const auto byte : value) {
        result ^= static_cast<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            result = (result & 1u) != 0 ? (result >> 1) ^ 0xedb88320u : result >> 1;
        }
    }
    return ~result;
}

ParseResult<std::vector<std::uint8_t>> decode_table_region(
    ByteView region, std::uint64_t expected_decoded_bytes,
    std::uint64_t max_decoded_bytes) {
    if (region.empty()) {
        return ParseResult<std::vector<std::uint8_t>>::success({});
    }

    const auto header = parse_level5_header(region);
    const bool is_expected_compressed_table =
        header && expected_decoded_bytes != 0 &&
        header.value->decoded_size == expected_decoded_bytes;
    const bool is_best_effort_stream =
        header && expected_decoded_bytes == 0 && header.value->method_id <= 5 &&
        header.value->decoded_size != 0 && header.value->decoded_size <= max_decoded_bytes;
    if (is_expected_compressed_table || is_best_effort_stream) {
        const auto decoded = decode_level5(region, Level5DecodeOptions{max_decoded_bytes});
        if (decoded || is_expected_compressed_table) {
            return decoded;
        }
    }

    // A raw table can begin with bytes that look like a Level-5 header. Only
    // use the decoded result when its size matched the table's proven row
    // layout. Strings use best-effort decoding and fall back to raw bytes when
    // their prefix is not a valid Level-5 stream.
    std::vector<std::uint8_t> raw(region.size());
    std::copy_n(region.data(), region.size(), raw.data());
    return ParseResult<std::vector<std::uint8_t>>::success(std::move(raw));
}

void append_records(ByteView bytes, std::size_t row_size,
                    std::vector<Xq32RawRecord>& destination) {
    if (row_size == 0 || bytes.size() % row_size != 0) {
        return;
    }
    destination.reserve(bytes.size() / row_size);
    for (std::size_t offset = 0; offset < bytes.size(); offset += row_size) {
        Xq32RawRecord record;
        record.words.reserve(row_size / sizeof(std::uint32_t));
        for (std::size_t word = 0; word < row_size; word += sizeof(std::uint32_t)) {
            const auto value = bytes.u32_le(offset + word);
            if (!value) {
                record.words.clear();
                break;
            }
            record.words.push_back(*value);
        }
        if (!record.words.empty()) {
            destination.push_back(std::move(record));
        }
    }
}

}  // namespace

ParseResult<Xq32ProgramInfo> read_xq32_program(const ResourceStore& store,
                                                std::string_view path,
                                                std::uint64_t max_bytes) {
    const auto* entry = store.catalog().find(path);
    if (entry == nullptr) {
        return failure<Xq32ProgramInfo>("xq32_resource_not_in_catalog");
    }
    if (entry->size < 0x18) {
        return failure<Xq32ProgramInfo>("xq32_resource_truncated", entry->size);
    }
    if (entry->size > max_bytes || entry->size > std::numeric_limits<std::size_t>::max()) {
        return failure<Xq32ProgramInfo>("xq32_resource_size_limit_exceeded");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry->size));
    if (!store.read_exact(path, 0, bytes.data(), bytes.size())) {
        return failure<Xq32ProgramInfo>("xq32_resource_read_failed");
    }
    const auto header = parse_xq32_header(ByteView(bytes.data(), bytes.size()));
    if (!header) {
        return failure<Xq32ProgramInfo>(header.error->code, header.error->offset);
    }

    Xq32ProgramInfo result;
    result.header = *header.value;
    result.file_size = entry->size;
    result.function_table_bytes = result.header.function.end - result.header.function.offset;
    result.jump_table_bytes = result.header.jump.end - result.header.jump.offset;
    result.instruction_table_bytes =
        result.header.instruction.end - result.header.instruction.offset;
    result.argument_table_bytes = result.header.argument.end - result.header.argument.offset;

    const auto region = [&bytes](const Xq32TableRegion& table) {
        return ByteView(bytes.data() + table.offset, table.end - table.offset);
    };
    const auto function_table = decode_table_region(
        region(result.header.function),
        static_cast<std::uint64_t>(result.header.function.entry_count) * 24u,
        max_bytes);
    const auto jump_table = decode_table_region(
        region(result.header.jump),
        static_cast<std::uint64_t>(result.header.jump.entry_count) * 12u,
        max_bytes);
    const auto instruction_table = decode_table_region(
        region(result.header.instruction),
        static_cast<std::uint64_t>(result.header.instruction.entry_count) * 12u,
        max_bytes);
    const auto argument_table = decode_table_region(
        region(result.header.argument),
        static_cast<std::uint64_t>(result.header.argument.entry_count) * 8u,
        max_bytes);
    const auto string_table = decode_table_region(region(result.header.strings), 0, max_bytes);
    if (!function_table) {
        return failure<Xq32ProgramInfo>(function_table.error->code,
                                        function_table.error->offset);
    }
    if (!jump_table) {
        return failure<Xq32ProgramInfo>(jump_table.error->code, jump_table.error->offset);
    }
    if (!instruction_table) {
        return failure<Xq32ProgramInfo>(instruction_table.error->code,
                                        instruction_table.error->offset);
    }
    if (!argument_table) {
        return failure<Xq32ProgramInfo>(argument_table.error->code,
                                        argument_table.error->offset);
    }
    if (!string_table) {
        return failure<Xq32ProgramInfo>(string_table.error->code,
                                        string_table.error->offset);
    }
    append_records(ByteView(function_table.value->data(), function_table.value->size()),
                   24, result.function_records);
    append_records(ByteView(jump_table.value->data(), jump_table.value->size()),
                   12, result.jump_records);
    append_records(ByteView(instruction_table.value->data(), instruction_table.value->size()),
                   12, result.instruction_records);
    append_records(ByteView(argument_table.value->data(), argument_table.value->size()),
                   8, result.argument_records);

    result.arguments.reserve(result.argument_records.size());
    for (const auto& record : result.argument_records) {
        if (record.words.size() == 2) {
            result.arguments.push_back({record.words[0], record.words[1]});
        }
    }
    result.instructions.reserve(result.instruction_records.size());
    for (const auto& record : result.instruction_records) {
        if (record.words.size() != 3) {
            continue;
        }
        const auto argument_word = record.words[0];
        Xq32InstructionInfo instruction;
        instruction.argument_index = static_cast<std::uint16_t>(argument_word & 0xffffu);
        instruction.argument_count = static_cast<std::uint16_t>(argument_word >> 16);
        instruction.operation_token = record.words[1];
        instruction.operation_id = static_cast<std::uint16_t>(record.words[1] & 0xffffu);
        instruction.operation_group = static_cast<std::uint16_t>(record.words[1] >> 16);
        instruction.flags = record.words[2];
        instruction.argument_span_valid =
            static_cast<std::uint64_t>(instruction.argument_index) +
                instruction.argument_count <= result.arguments.size();
        result.instructions.push_back(instruction);
    }

    std::size_t current = 0;
    const auto& strings = *string_table.value;
    const auto string_bytes = ByteView(strings.data(), strings.size());
    while (current < string_bytes.size()) {
        const auto start = current;
        while (current < string_bytes.size()) {
            const auto value = string_bytes.u8(current);
            if (!value || *value == 0) {
                break;
            }
            if (!is_symbol_byte(*value)) {
                return failure<Xq32ProgramInfo>("xq32_symbol_not_printable", current);
            }
            ++current;
        }
        if (current == string_bytes.size()) {
            return failure<Xq32ProgramInfo>("xq32_symbol_not_terminated", start);
        }
        if (current != start) {
            result.symbols.emplace_back(reinterpret_cast<const char*>(strings.data() + start),
                                        current - start);
        }
        ++current;
    }

    result.functions.reserve(result.function_records.size());
    for (const auto& record : result.function_records) {
        if (record.words.size() != 6) {
            continue;
        }
        Xq32FunctionInfo function;
        function.instruction_index = record.words[0];
        function.name_hash = record.words[1];
        function.instruction_index_valid =
            function.instruction_index < result.instructions.size();
        for (const auto& symbol : result.symbols) {
            if (crc32(symbol) == function.name_hash) {
                function.symbol = symbol;
                function.symbol_hash_matches = true;
                break;
            }
        }
        result.functions.push_back(std::move(function));
    }
    return ParseResult<Xq32ProgramInfo>::success(std::move(result));
}

}  // namespace ykw2::core
