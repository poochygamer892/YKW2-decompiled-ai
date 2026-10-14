#include "ykw2/core/runtime_bootstrap.hpp"

namespace ykw2::core {

RuntimeBootInfo evaluate_runtime_boot(const ResourceCatalog& catalog,
                                      bool marker_present,
                                      std::uint64_t marker_files,
                                      std::uint64_t marker_bytes,
                                      std::uint64_t marker_fingerprint) {
    RuntimeBootInfo result;
    result.imported_files = marker_files;
    result.imported_bytes = marker_bytes;
    result.catalog_fingerprint = catalog.fingerprint();
    if (!marker_present) {
        result.state = RuntimeBootState::ImportRequired;
        return result;
    }

    result.state = marker_files == catalog.entries.size() &&
                           marker_bytes == catalog.total_bytes() &&
                           marker_fingerprint == catalog.fingerprint() &&
                           !catalog.entries.empty()
                       ? RuntimeBootState::Ready
                       : RuntimeBootState::ImportIncomplete;
    return result;
}

}  // namespace ykw2::core
