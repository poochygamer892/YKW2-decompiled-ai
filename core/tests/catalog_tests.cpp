#include "ykw2/core/resource_catalog.hpp"
#include "ykw2/core/runtime_bootstrap.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

std::array<std::uint8_t, 4> signature(const char (&value)[5]) {
    return {static_cast<std::uint8_t>(value[0]), static_cast<std::uint8_t>(value[1]),
            static_cast<std::uint8_t>(value[2]), static_cast<std::uint8_t>(value[3])};
}

void test_catalog_is_sorted_and_stable() {
    std::vector<ykw2::core::CatalogInput> inputs = {
        {"zeta.bin", 12, signature("IMGC")},
        {"data/main.arc", 20, signature("ARC0")},
        {"script.xq", 8, signature("XQ32")},
    };
    const auto result = ykw2::core::build_resource_catalog(inputs);
    assert(result);
    assert(result.value->entries.size() == 3);
    assert(result.value->entries[0].path == "data/main.arc");
    assert(result.value->entries[1].path == "script.xq");
    assert(result.value->entries[2].path == "zeta.bin");
    assert(result.value->entries[0].kind == ykw2::core::ResourceKind::Arc0);
    assert(result.value->find("script.xq") != nullptr);
    assert(result.value->find("missing") == nullptr);
    assert(result.value->recognized_count() == 3);
    assert(result.value->total_bytes() == 40);
    assert(serialize_resource_catalog(*result.value).find("YKW2-CATALOG-1\n") == 0);

    const auto reversed = ykw2::core::build_resource_catalog(std::move(inputs));
    assert(reversed);
    assert(reversed.value->fingerprint() == result.value->fingerprint());
    const auto parsed = ykw2::core::parse_resource_catalog(
        ykw2::core::serialize_resource_catalog(*result.value));
    assert(parsed);
    assert(parsed.value->fingerprint() == result.value->fingerprint());
    const auto trailing = ykw2::core::parse_resource_catalog(
        ykw2::core::serialize_resource_catalog(*result.value) + "unexpected");
    assert(!trailing);
    assert(trailing.error->code == "catalog_trailing_data");
}

void test_catalog_rejects_unsafe_paths() {
    for (const std::string path : {"../outside", "/absolute", "dir/../file", "dir\\file"}) {
        const auto result = ykw2::core::build_resource_catalog({{path, 1, {}}});
        assert(!result);
        assert(result.error->code == "catalog_invalid_path");
    }
    const auto duplicate = ykw2::core::build_resource_catalog({
        {"same", 1, {}},
        {"same", 2, {}},
    });
    assert(!duplicate);
    assert(duplicate.error->code == "catalog_duplicate_path");
}

void test_boot_state_requires_matching_marker() {
    const auto result = ykw2::core::build_resource_catalog({
        {"a.arc", 10, signature("ARC0")},
        {"b.bin", 5, {}},
    });
    assert(result);
    const auto missing = ykw2::core::evaluate_runtime_boot(*result.value, false, 2, 15, 0);
    assert(missing.state == ykw2::core::RuntimeBootState::ImportRequired);
    const auto incomplete = ykw2::core::evaluate_runtime_boot(*result.value, true, 1, 10, 0);
    assert(incomplete.state == ykw2::core::RuntimeBootState::ImportIncomplete);
    const auto ready = ykw2::core::evaluate_runtime_boot(
        *result.value, true, 2, 15, result.value->fingerprint());
    assert(ready.state == ykw2::core::RuntimeBootState::Ready);
    assert(ready.catalog_fingerprint == result.value->fingerprint());
}

}  // namespace

int main() {
    test_catalog_is_sorted_and_stable();
    test_catalog_rejects_unsafe_paths();
    test_boot_state_requires_matching_marker();
    return 0;
}
