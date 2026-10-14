#include "ykw2/core/resource_store.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

std::array<std::uint8_t, 4> signature(const char (&value)[5]) {
    return {static_cast<std::uint8_t>(value[0]), static_cast<std::uint8_t>(value[1]),
            static_cast<std::uint8_t>(value[2]), static_cast<std::uint8_t>(value[3])};
}

class MemoryStore final : public ykw2::core::ResourceStore {
public:
    explicit MemoryStore(std::map<std::string, std::vector<std::uint8_t>> files)
        : files_(std::move(files)) {
        std::vector<ykw2::core::CatalogInput> inputs;
        for (const auto& [path, data] : files_) {
            std::array<std::uint8_t, 4> signature_bytes{};
            const auto signature_size = std::min<std::size_t>(data.size(), signature_bytes.size());
            if (signature_size != 0) {
                std::copy_n(data.data(), signature_size, signature_bytes.data());
            }
            inputs.push_back({path, data.size(), signature_bytes});
        }
        const auto parsed = ykw2::core::build_resource_catalog(std::move(inputs));
        assert(parsed);
        catalog_ = std::move(*parsed.value);
    }

    const ykw2::core::ResourceCatalog& catalog() const override { return catalog_; }

    bool read(std::string_view path, std::uint64_t offset,
              std::uint8_t* destination, std::size_t length) const override {
        const auto found = files_.find(std::string(path));
        if (found == files_.end() || offset > found->second.size() ||
            length > found->second.size() - offset) {
            return false;
        }
        std::copy_n(found->second.data() + offset, length, destination);
        return true;
    }

    void replace(std::string path, std::vector<std::uint8_t> data) {
        files_[std::move(path)] = std::move(data);
    }

private:
    std::map<std::string, std::vector<std::uint8_t>> files_;
    ykw2::core::ResourceCatalog catalog_;
};

void test_probe_and_bounded_reads() {
    MemoryStore store({
        {"data/main.arc", {'A', 'R', 'C', '0', 1, 2, 3}},
        {"empty.bin", {}},
    });
    const auto arc = ykw2::core::probe_resource(store, "data/main.arc");
    assert(arc);
    assert(arc.value->kind == ykw2::core::ResourceKind::Arc0);
    assert(arc.value->size == 7);

    std::array<std::uint8_t, 4> bytes{};
    assert(store.read_exact("data/main.arc", 4, bytes.data(), bytes.size()) == false);
    assert(store.read_exact("data/main.arc", 0, bytes.data(), bytes.size()));
    assert(bytes[0] == 'A' && bytes[3] == '0');

    const auto empty = ykw2::core::probe_resource(store, "empty.bin");
    assert(empty);
    assert(empty.value->kind == ykw2::core::ResourceKind::Unknown);
}

void test_probe_rejects_missing_or_changed_resources() {
    MemoryStore store({{"data/main.arc", {'A', 'R', 'C', '0'}}});
    assert(ykw2::core::probe_resource(store, "missing") .error->code ==
           "resource_not_in_catalog");
    store.replace("data/main.arc", {'X', 'X', 'X', 'X'});
    const auto changed = ykw2::core::probe_resource(store, "data/main.arc");
    assert(!changed);
    assert(changed.error->code == "resource_signature_mismatch");
}

}  // namespace

int main() {
    test_probe_and_bounded_reads();
    test_probe_rejects_missing_or_changed_resources();
    return 0;
}
