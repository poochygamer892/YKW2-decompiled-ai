#include "ykw2/core/resource_catalog.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string>
#include <utility>

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset) {
    return ParseResult<T>::failure(code, offset);
}

bool valid_relative_path(std::string_view path) {
    if (path.empty() || path.size() > 4096 || path.front() == '/' ||
        path.front() == '\\' || path.find('\0') != std::string_view::npos ||
        path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos) {
        return false;
    }

    std::size_t component_start = 0;
    while (component_start <= path.size()) {
        const auto separator = path.find('/', component_start);
        const auto component_end = separator == std::string_view::npos ? path.size() : separator;
        const auto component = path.substr(component_start, component_end - component_start);
        if (component.empty() || component == "." || component == ".." ||
            std::any_of(component.begin(), component.end(),
                        [](char value) { return static_cast<unsigned char>(value) < 0x20; })) {
            return false;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        component_start = separator + 1;
    }
    return true;
}

void hash_byte(std::uint64_t& hash, std::uint8_t byte) {
    hash ^= byte;
    hash *= 1099511628211ull;
}

void hash_u64(std::uint64_t& hash, std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(value >> (index * 8)));
    }
}

std::uint64_t hash_entry(std::string_view path, std::uint64_t size, ResourceKind kind) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const auto byte : path) {
        hash_byte(hash, static_cast<std::uint8_t>(byte));
    }
    hash_byte(hash, 0);
    hash_u64(hash, size);
    hash_byte(hash, static_cast<std::uint8_t>(kind));
    return hash;
}

bool parse_u64(std::string_view text, std::uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool read_header_value(std::string_view line, std::string_view key,
                       std::uint64_t& value) {
    const auto prefix = std::string(key) + "=";
    if (line.substr(0, prefix.size()) != prefix) {
        return false;
    }
    return parse_u64(line.substr(prefix.size()), value);
}

bool next_line(std::string_view serialized, std::size_t& cursor, std::string_view& line) {
    if (cursor > serialized.size()) {
        return false;
    }
    const auto end = serialized.find('\n', cursor);
    if (end == std::string_view::npos) {
        line = serialized.substr(cursor);
        cursor = serialized.size() + 1;
    } else {
        line = serialized.substr(cursor, end - cursor);
        cursor = end + 1;
    }
    return true;
}

}  // namespace

const CatalogEntry* ResourceCatalog::find(std::string_view path) const {
    const auto found = std::lower_bound(
        entries.begin(), entries.end(), path,
        [](const CatalogEntry& entry, std::string_view value) { return entry.path < value; });
    if (found == entries.end() || found->path != path) {
        return nullptr;
    }
    return &*found;
}

std::uint64_t ResourceCatalog::total_bytes() const {
    std::uint64_t total = 0;
    for (const auto& entry : entries) {
        total += entry.size;
    }
    return total;
}

std::size_t ResourceCatalog::recognized_count() const {
    return static_cast<std::size_t>(std::count_if(
        entries.begin(), entries.end(),
        [](const CatalogEntry& entry) { return entry.kind != ResourceKind::Unknown; }));
}

std::uint64_t ResourceCatalog::fingerprint() const {
    std::uint64_t hash = 1469598103934665603ull;
    for (const auto& entry : entries) {
        hash_u64(hash, entry.fingerprint);
    }
    return hash;
}

ParseResult<ResourceCatalog> build_resource_catalog(std::vector<CatalogInput> inputs) {
    std::sort(inputs.begin(), inputs.end(),
              [](const CatalogInput& left, const CatalogInput& right) {
                  return left.path < right.path;
              });

    ResourceCatalog catalog;
    catalog.entries.reserve(inputs.size());
    std::uint64_t total_bytes = 0;
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        const auto& input = inputs[index];
        if (!valid_relative_path(input.path)) {
            return failure<ResourceCatalog>("catalog_invalid_path", index);
        }
        if (index != 0 && inputs[index - 1].path == input.path) {
            return failure<ResourceCatalog>("catalog_duplicate_path", index);
        }
        const auto kind = detect_resource_kind(ByteView(input.signature.data(), input.signature.size()));
        if (input.size > std::numeric_limits<std::uint64_t>::max() - total_bytes) {
            return failure<ResourceCatalog>("catalog_size_overflow", index);
        }
        total_bytes += input.size;
        catalog.entries.push_back({input.path, input.size, kind,
                                   hash_entry(input.path, input.size, kind)});
    }
    return ParseResult<ResourceCatalog>::success(std::move(catalog));
}

ParseResult<ResourceCatalog> parse_resource_catalog(std::string_view serialized) {
    std::size_t cursor = 0;
    std::string_view line;
    if (!next_line(serialized, cursor, line) || line != "YKW2-CATALOG-1") {
        return failure<ResourceCatalog>("catalog_invalid_header", 0);
    }

    std::uint64_t expected_files = 0;
    std::uint64_t expected_bytes = 0;
    std::uint64_t expected_recognized = 0;
    std::uint64_t expected_fingerprint = 0;
    if (!next_line(serialized, cursor, line) ||
        !read_header_value(line, "files", expected_files)) {
        return failure<ResourceCatalog>("catalog_invalid_file_count", cursor);
    }
    if (!next_line(serialized, cursor, line) ||
        !read_header_value(line, "bytes", expected_bytes)) {
        return failure<ResourceCatalog>("catalog_invalid_byte_count", cursor);
    }
    if (!next_line(serialized, cursor, line) ||
        !read_header_value(line, "recognized", expected_recognized)) {
        return failure<ResourceCatalog>("catalog_invalid_recognized_count", cursor);
    }
    if (!next_line(serialized, cursor, line) ||
        !read_header_value(line, "fingerprint", expected_fingerprint)) {
        return failure<ResourceCatalog>("catalog_invalid_fingerprint", cursor);
    }

    if (expected_files > serialized.size()) {
        return failure<ResourceCatalog>("catalog_file_count_exceeded", cursor);
    }
    ResourceCatalog catalog;
    catalog.entries.reserve(static_cast<std::size_t>(expected_files));
    for (std::uint64_t index = 0; index < expected_files; ++index) {
        if (!next_line(serialized, cursor, line)) {
            return failure<ResourceCatalog>("catalog_missing_entry", cursor);
        }
        const auto first_tab = line.find('\t');
        const auto second_tab = first_tab == std::string_view::npos
                                    ? std::string_view::npos
                                    : line.find('\t', first_tab + 1);
        const auto third_tab = second_tab == std::string_view::npos
                                   ? std::string_view::npos
                                   : line.find('\t', second_tab + 1);
        const auto fourth_tab = third_tab == std::string_view::npos
                                    ? std::string_view::npos
                                    : line.find('\t', third_tab + 1);
        if (first_tab != 5 || second_tab == std::string_view::npos ||
            third_tab == std::string_view::npos || fourth_tab == std::string_view::npos ||
            line.substr(0, first_tab) != "entry") {
            return failure<ResourceCatalog>("catalog_invalid_entry", cursor);
        }
        const auto path = line.substr(first_tab + 1, second_tab - first_tab - 1);
        std::uint64_t size = 0;
        std::uint64_t kind_value = 0;
        std::uint64_t entry_fingerprint = 0;
        if (!valid_relative_path(path) ||
            !parse_u64(line.substr(second_tab + 1, third_tab - second_tab - 1), size) ||
            !parse_u64(line.substr(third_tab + 1, fourth_tab - third_tab - 1), kind_value) ||
            !parse_u64(line.substr(fourth_tab + 1), entry_fingerprint) ||
            kind_value > static_cast<std::uint64_t>(ResourceKind::Imgc)) {
            return failure<ResourceCatalog>("catalog_invalid_entry", cursor);
        }
        const auto kind = static_cast<ResourceKind>(kind_value);
        if (!catalog.entries.empty() && catalog.entries.back().path >= path) {
            return failure<ResourceCatalog>("catalog_unsorted_entries", cursor);
        }
        if (hash_entry(path, size, kind) != entry_fingerprint) {
            return failure<ResourceCatalog>("catalog_entry_fingerprint_mismatch", cursor);
        }
        catalog.entries.push_back({std::string(path), size, kind, entry_fingerprint});
    }

    if (cursor <= serialized.size()) {
        if (!next_line(serialized, cursor, line) || !line.empty()) {
            return failure<ResourceCatalog>("catalog_trailing_data", cursor);
        }
    }

    if (catalog.entries.size() != expected_files || catalog.total_bytes() != expected_bytes ||
        catalog.recognized_count() != expected_recognized ||
        catalog.fingerprint() != expected_fingerprint) {
        return failure<ResourceCatalog>("catalog_summary_mismatch", cursor);
    }
    return ParseResult<ResourceCatalog>::success(std::move(catalog));
}

std::string serialize_resource_catalog(const ResourceCatalog& catalog) {
    std::string output = "YKW2-CATALOG-1\n";
    output += "files=" + std::to_string(catalog.entries.size()) + "\n";
    output += "bytes=" + std::to_string(catalog.total_bytes()) + "\n";
    output += "recognized=" + std::to_string(catalog.recognized_count()) + "\n";
    output += "fingerprint=" + std::to_string(catalog.fingerprint()) + "\n";
    for (const auto& entry : catalog.entries) {
        output += "entry\t" + entry.path + "\t" + std::to_string(entry.size) + "\t" +
                  std::to_string(static_cast<unsigned int>(entry.kind)) + "\t" +
                  std::to_string(entry.fingerprint) + "\n";
    }
    return output;
}

}  // namespace ykw2::core
