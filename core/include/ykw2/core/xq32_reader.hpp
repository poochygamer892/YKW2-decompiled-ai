#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ykw2/core/formats.hpp"
#include "ykw2/core/resource_store.hpp"

namespace ykw2::core {

// Read-only description of an XQ32 program. The instruction encoding remains
// opaque until its opcode definitions have been recovered; this reader never
// executes bytes from an imported game image.
struct Xq32ProgramInfo {
    Xq32Header header;
    std::uint64_t file_size = 0;
    std::uint32_t function_table_bytes = 0;
    std::uint32_t jump_table_bytes = 0;
    std::uint32_t instruction_table_bytes = 0;
    std::uint32_t argument_table_bytes = 0;
    std::vector<std::string> symbols;
};

// Reads a whole bounded XQ32 resource, validates its table layout, and
// extracts NUL-terminated symbol strings from the trailing strings region.
// It is metadata only: it does not decrypt, execute, or reinterpret opcodes.
ParseResult<Xq32ProgramInfo> read_xq32_program(
    const ResourceStore& store, std::string_view path,
    std::uint64_t max_bytes = 16ull * 1024ull * 1024ull);

}  // namespace ykw2::core
