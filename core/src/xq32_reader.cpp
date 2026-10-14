#include "ykw2/core/xq32_reader.hpp"

#include <limits>
#include <string>
#include <vector>

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

    std::size_t current = static_cast<std::size_t>(result.header.strings.offset);
    while (current < bytes.size()) {
        const auto start = current;
        while (current < bytes.size() && bytes[current] != 0) {
            if (!is_symbol_byte(bytes[current])) {
                return failure<Xq32ProgramInfo>("xq32_symbol_not_printable", current);
            }
            ++current;
        }
        if (current == bytes.size()) {
            return failure<Xq32ProgramInfo>("xq32_symbol_not_terminated", start);
        }
        if (current != start) {
            result.symbols.emplace_back(reinterpret_cast<const char*>(bytes.data() + start),
                                        current - start);
        }
        ++current;
    }
    return ParseResult<Xq32ProgramInfo>::success(std::move(result));
}

}  // namespace ykw2::core
