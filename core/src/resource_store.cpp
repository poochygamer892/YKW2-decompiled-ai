#include "ykw2/core/resource_store.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset) {
    return ParseResult<T>::failure(code, offset);
}

}  // namespace

bool ResourceStore::read_exact(std::string_view path, std::uint64_t offset,
                               std::uint8_t* destination, std::size_t length) const {
    const auto* entry = catalog().find(path);
    return entry != nullptr && offset <= entry->size && length <= entry->size - offset &&
           read(path, offset, destination, length);
}

ParseResult<ResourceProbe> probe_resource(const ResourceStore& store,
                                          std::string_view path) {
    const auto* entry = store.catalog().find(path);
    if (entry == nullptr) {
        return failure<ResourceProbe>("resource_not_in_catalog", 0);
    }

    ResourceProbe probe{std::string(path), entry->size, entry->kind};
    if (entry->size == 0) {
        if (entry->kind != ResourceKind::Unknown) {
            return failure<ResourceProbe>("resource_signature_mismatch", 0);
        }
        return ParseResult<ResourceProbe>::success(std::move(probe));
    }

    std::array<std::uint8_t, 4> signature{};
    const auto signature_size = static_cast<std::size_t>(std::min<std::uint64_t>(
        entry->size, signature.size()));
    if (!store.read_exact(path, 0, signature.data(), signature_size)) {
        return failure<ResourceProbe>("resource_read_failed", 0);
    }
    const auto actual_kind = detect_resource_kind(ByteView(signature.data(), signature_size));
    if (actual_kind != entry->kind) {
        return failure<ResourceProbe>("resource_signature_mismatch", 0);
    }
    probe.kind = actual_kind;
    return ParseResult<ResourceProbe>::success(std::move(probe));
}

}  // namespace ykw2::core
