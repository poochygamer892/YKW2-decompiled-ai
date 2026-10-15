#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ykw2/core/formats.hpp"
#include "ykw2/core/resource_store.hpp"

namespace ykw2::core {

// XQ32 table rows are intentionally exposed as raw 32-bit words until the
// instruction ABI has been recovered from the authorized executable. Keeping
// the decoded rows lets the native runtime trace real YW2 programs without
// pretending that an opcode meaning has already been proven.
struct Xq32RawRecord {
    std::vector<std::uint32_t> words;
};

// Read-only description of an XQ32 program. The instruction encoding remains
// opaque until its opcode definitions have been recovered; this reader only
// decodes table compression and never executes bytes from an imported image.
struct Xq32ProgramInfo {
    Xq32Header header;
    std::uint64_t file_size = 0;
    std::uint32_t function_table_bytes = 0;
    std::uint32_t jump_table_bytes = 0;
    std::uint32_t instruction_table_bytes = 0;
    std::uint32_t argument_table_bytes = 0;
    std::vector<std::string> symbols;
    std::vector<Xq32RawRecord> function_records;
    std::vector<Xq32RawRecord> jump_records;
    std::vector<Xq32RawRecord> instruction_records;
    std::vector<Xq32RawRecord> argument_records;
};

// Reads a whole bounded XQ32 resource, validates its table layout, and
// extracts NUL-terminated symbol strings from the trailing strings region.
// It is metadata only: it does not decrypt, execute, or reinterpret opcodes.
ParseResult<Xq32ProgramInfo> read_xq32_program(
    const ResourceStore& store, std::string_view path,
    std::uint64_t max_bytes = 16ull * 1024ull * 1024ull);

}  // namespace ykw2::core
