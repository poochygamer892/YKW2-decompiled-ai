#include "ykw2/core/initial_content.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

void put16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void put_level5_none(std::vector<std::uint8_t>& bytes, std::size_t offset,
                     const std::vector<std::uint8_t>& data) {
    put32(bytes, offset, static_cast<std::uint32_t>(data.size()) << 3);
    std::copy(data.begin(), data.end(), bytes.begin() + offset + 4);
}

class MemoryStore final : public ykw2::core::ResourceStore {
public:
    explicit MemoryStore(std::map<std::string, std::vector<std::uint8_t>> files)
        : files_(std::move(files)) {
        std::vector<ykw2::core::CatalogInput> inputs;
        for (const auto& [path, data] : files_) {
            std::array<std::uint8_t, 4> signature{};
            const auto count = std::min<std::size_t>(4, data.size());
            std::copy_n(data.data(), count, signature.data());
            inputs.push_back({path, data.size(), signature});
        }
        const auto result = ykw2::core::build_resource_catalog(std::move(inputs));
        assert(result);
        catalog_ = std::move(*result.value);
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

private:
    std::map<std::string, std::vector<std::uint8_t>> files_;
    ykw2::core::ResourceCatalog catalog_;
};

std::vector<std::uint8_t> archive() {
    std::vector<std::uint8_t> result(0x100, 0);
    result[0] = 'A'; result[1] = 'R'; result[2] = 'C'; result[3] = '0';
    put32(result, 0x04, 0x48);
    put32(result, 0x08, 0x48 + 4 + 20);
    put32(result, 0x0c, 0x48 + 4 + 20 + 4 + 4);
    put32(result, 0x10, 0x48 + 4 + 20 + 4 + 4 + 4 + 16);
    put32(result, 0x14, 0x90);
    put16(result, 0x18, 1);
    put16(result, 0x1a, 1);
    put32(result, 0x1c, 1);
    put32(result, 0x20, 0x10);
    put32(result, 0x38, 1);
    put32(result, 0x3c, 1);
    const std::vector<std::uint8_t> directory = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    };
    const std::vector<std::uint8_t> hashes = {0, 0, 0, 0};
    const std::vector<std::uint8_t> files = {0, 0, 0, 0, 5, 0, 0, 0,
                                             0, 0, 0, 0, 5, 0, 0, 0};
    const std::vector<std::uint8_t> names = {'d', 'i', 'r', '/', 0, 'h', 'i', 0};
    put_level5_none(result, 0x48, directory);
    put_level5_none(result, 0x48 + 4 + directory.size(), hashes);
    put_level5_none(result, 0x48 + 4 + directory.size() + 4 + hashes.size(), files);
    put_level5_none(result, 0x48 + 4 + directory.size() + 4 + hashes.size() +
                              4 + files.size(), names);
    return result;
}

void test_loads_canonical_archive_and_fallback_path() {
    MemoryStore store({{"data/YW2_A.FA", archive()}});
    const auto result = ykw2::core::load_initial_content(store);
    assert(result);
    assert(result.value->archive_path == "data/YW2_A.FA");
    assert(result.value->archive_files == 1);
    assert(result.value->first_file_path == "dir/hi");
    assert(result.value->first_file_size == 5);
}

void test_reports_missing_archive() {
    MemoryStore store({{"other.bin", {1, 2, 3, 4}}});
    const auto result = ykw2::core::load_initial_content(store);
    assert(!result);
    assert(result.error->code == "initial_content_not_found");
}

}  // namespace

int main() {
    test_loads_canonical_archive_and_fallback_path();
    test_reports_missing_archive();
    return 0;
}
