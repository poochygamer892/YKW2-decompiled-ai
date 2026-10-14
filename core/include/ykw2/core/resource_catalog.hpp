#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ykw2/core/formats.hpp"

namespace ykw2::core {

// Metadata supplied by a platform adapter after it has extracted one file.
// The catalog deliberately contains no game payload; it is only an index for
// the private cache and a deterministic boot fingerprint.
struct CatalogInput {
    std::string path;
    std::uint64_t size = 0;
    std::array<std::uint8_t, 4> signature{};
};

struct CatalogEntry {
    std::string path;
    std::uint64_t size = 0;
    ResourceKind kind = ResourceKind::Unknown;
    std::uint64_t fingerprint = 0;
};

struct ResourceCatalog {
    std::vector<CatalogEntry> entries;

    const CatalogEntry* find(std::string_view path) const;
    std::uint64_t total_bytes() const;
    std::size_t recognized_count() const;
    std::uint64_t fingerprint() const;
};

ParseResult<ResourceCatalog> build_resource_catalog(std::vector<CatalogInput> inputs);

ParseResult<ResourceCatalog> parse_resource_catalog(std::string_view serialized);

// Stable, line-oriented metadata for platform adapters. It contains paths and
// sizes only; resource payloads remain in the private cache.
std::string serialize_resource_catalog(const ResourceCatalog& catalog);

}  // namespace ykw2::core
