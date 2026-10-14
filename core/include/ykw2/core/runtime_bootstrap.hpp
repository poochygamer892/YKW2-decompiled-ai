#pragma once

#include <cstdint>

#include "ykw2/core/resource_catalog.hpp"

namespace ykw2::core {

enum class RuntimeBootState {
    ImportRequired,
    ImportIncomplete,
    Ready,
};

struct RuntimeBootInfo {
    RuntimeBootState state = RuntimeBootState::ImportRequired;
    std::uint64_t imported_files = 0;
    std::uint64_t imported_bytes = 0;
    std::uint64_t catalog_fingerprint = 0;
};

// The platform adapter reads the marker written after a successful extraction
// and supplies its values here. A missing marker always sends the user back to
// the import screen; a marker is accepted only when it matches the catalog.
RuntimeBootInfo evaluate_runtime_boot(const ResourceCatalog& catalog,
                                      bool marker_present,
                                      std::uint64_t marker_files,
                                      std::uint64_t marker_bytes,
                                      std::uint64_t marker_fingerprint);

}  // namespace ykw2::core
