#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ykw2/core/resource_catalog.hpp"

namespace ykw2::core {

// A platform adapter implements this over the app-private extracted directory.
// The store owns neither the catalog nor the payloads; it only supplies
// bounded random reads after the runtime has resolved a catalog path.
class ResourceStore {
public:
    virtual ~ResourceStore() = default;

    virtual const ResourceCatalog& catalog() const = 0;
    virtual bool read(std::string_view path, std::uint64_t offset,
                      std::uint8_t* destination, std::size_t length) const = 0;

    bool read_exact(std::string_view path, std::uint64_t offset,
                    std::uint8_t* destination, std::size_t length) const;
};

// Presents a bounded byte range as a one-file ResourceStore. This lets the
// existing XPCK/IMGC readers consume files nested in ARC0 without a temporary
// extraction or a second full-size buffer.
class ResourceWindowStore final : public ResourceStore {
public:
    static ParseResult<ResourceWindowStore> create(
        const ResourceStore& source, std::string_view source_path,
        std::uint64_t source_offset, std::uint64_t window_size,
        std::string virtual_path = "window");

    const ResourceCatalog& catalog() const override { return catalog_; }
    bool read(std::string_view path, std::uint64_t offset,
              std::uint8_t* destination, std::size_t length) const override;
    std::string_view virtual_path() const { return virtual_path_; }

private:
    ResourceWindowStore(const ResourceStore& source, std::string source_path,
                        std::uint64_t source_offset, std::uint64_t window_size,
                        std::string virtual_path, ResourceCatalog catalog);

    const ResourceStore* source_;
    std::string source_path_;
    std::uint64_t source_offset_;
    std::uint64_t window_size_;
    std::string virtual_path_;
    ResourceCatalog catalog_;
};

struct ResourceProbe {
    std::string path;
    std::uint64_t size = 0;
    ResourceKind kind = ResourceKind::Unknown;
};

// Resolves a path through the catalog, reads only its signature and confirms
// that the platform adapter is serving the same resource that was indexed.
ParseResult<ResourceProbe> probe_resource(const ResourceStore& store,
                                          std::string_view path);

}  // namespace ykw2::core
