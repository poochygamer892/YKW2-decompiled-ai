#include "ykw2/core/arc0_reader.hpp"
#include "ykw2/core/xpck_reader.hpp"

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
            if (count != 0) {
                std::copy_n(data.data(), count, signature.data());
            }
            inputs.push_back({path, data.size(), signature});
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

private:
    std::map<std::string, std::vector<std::uint8_t>> files_;
    ykw2::core::ResourceCatalog catalog_;
};

void test_arc0_reader() {
    // Header and four Level-5-none regions, with one directory and one file.
    std::vector<std::uint8_t> archive(0x100, 0);
    archive[0] = 'A'; archive[1] = 'R'; archive[2] = 'C'; archive[3] = '0';
    put32(archive, 0x04, 0x48);
    put32(archive, 0x08, 0x48 + 4 + 20);
    put32(archive, 0x0c, 0x48 + 4 + 20 + 4 + 4);
    put32(archive, 0x10, 0x48 + 4 + 20 + 4 + 4 + 4 + 16);
    put32(archive, 0x14, 0x90);
    put16(archive, 0x18, 1);
    put16(archive, 0x1a, 1);
    put32(archive, 0x1c, 1);
    put32(archive, 0x20, 0x10);
    put32(archive, 0x38, 1);
    put32(archive, 0x3c, 1);

    const std::vector<std::uint8_t> directory = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    };
    const std::vector<std::uint8_t> hashes = {0, 0, 0, 0};
    const std::vector<std::uint8_t> files = {0, 0, 0, 0, 5, 0, 0, 0,
                                             0, 0, 0, 0, 5, 0, 0, 0};
    const std::vector<std::uint8_t> names = {'d', 'i', 'r', '/', 0, 'h', 'i', 0};
    put_level5_none(archive, 0x48, directory);
    put_level5_none(archive, 0x48 + 4 + directory.size(), hashes);
    put_level5_none(archive, 0x48 + 4 + directory.size() + 4 + hashes.size(), files);
    put_level5_none(archive, 0x48 + 4 + directory.size() + 4 + hashes.size() +
                              4 + files.size(), names);
    std::copy_n(std::array<std::uint8_t, 5>{'w', 'o', 'r', 'l', 'd'}.data(), 5,
                archive.begin() + 0x90);

    MemoryStore store({{"archive.fa", archive}});
    const auto result = ykw2::core::read_arc0_archive(store, "archive.fa");
    assert(result);
    assert(result.value->files.size() == 1);
    assert(result.value->files[0].path == "dir/hi");
    assert(result.value->files[0].offset == 0x90);
    assert(result.value->files[0].size == 5);
}

void test_xpck_reader() {
    std::vector<std::uint8_t> archive(0x80, 0);
    archive[0] = 'X'; archive[1] = 'P'; archive[2] = 'C'; archive[3] = 'K';
    put16(archive, 0x04, 0x1001);
    put16(archive, 0x06, 5);   // info at 0x14
    put16(archive, 0x08, 8);   // names at 0x20
    put16(archive, 0x0a, 12);  // data at 0x30
    put16(archive, 0x0c, 3);   // info size 0x0c
    put16(archive, 0x0e, 5);   // name region size 0x14
    put32(archive, 0x10, 4);   // data size 0x10
    put16(archive, 0x14, 0x1234);
    put16(archive, 0x16, 0);
    put16(archive, 0x18, 0);
    put16(archive, 0x1a, 0);
    archive[0x1c] = 1;
    archive[0x1d] = 0;
    put32(archive, 0x20, (5u << 3));
    archive[0x24] = 'x'; archive[0x25] = 'p'; archive[0x26] = 'c'; archive[0x27] = 'k';
    archive[0x28] = 0;
    MemoryStore store({{"nested.xc", archive}});
    const auto result = ykw2::core::read_xpck_archive(store, "nested.xc");
    assert(result);
    assert(result.value->files.size() == 1);
    assert(result.value->files[0].name == "xpck");
    assert(result.value->files[0].offset == 0x30);
    assert(result.value->files[0].size == 1);
}

}  // namespace

int main() {
    test_arc0_reader();
    test_xpck_reader();
    return 0;
}
