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

void test_selects_scene_resources_by_role_and_path() {
    ykw2::core::Arc0Archive archive;
    archive.files = {
        {"data/map/z999999/z999999.xc", 300, 30, 0},
        {"data/character/c000010/c000010_p10.xc", 200, 20, 0},
        {"data/map/bg/bg02.xi", 500, 50, 0},
        {"data/map/e100g002/e100g002.xc", 100, 10, 0},
        {"data/map/bg/bg01.xi", 400, 40, 0},
        {"data/character/c000010/c000010_p00.xc", 150, 15, 0},
    };

    const auto resources = ykw2::core::select_initial_scene_resources(archive);
    assert(resources.size() == 2);
    assert(resources[0].role == ykw2::core::InitialSceneResourceRole::Map);
    assert(resources[0].path == "data/map/e100g002/e100g002.xc");
    assert(resources[1].role == ykw2::core::InitialSceneResourceRole::Character);
    assert(resources[1].path == "data/character/c000010/c000010_p00.xc");
}

void test_uses_only_title_related_image_for_menu() {
    ykw2::core::Arc0Archive archive;
    archive.files = {
        {"data/menu/img/battle_parts00.xi", 100, 10, 0},
        {"data/menu/img/esus/transfer_image00.xi", 200, 20, 0},
        {"data/menu/img/frca/transfer_image00.xi", 300, 30, 0},
        {"data/menu/img/title_back.xi", 400, 40, 0},
    };
    const auto resources = ykw2::core::select_initial_scene_resources(archive);
    assert(resources.size() == 1);
    assert(resources[0].role == ykw2::core::InitialSceneResourceRole::Background);
    assert(resources[0].path == "data/menu/img/title_back.xi");
}

void test_selects_runtime_content_manifest() {
    ykw2::core::Arc0Archive archive;
    for (const char* path : {
             "seq/menu/TitleMenu.xq",
             "seq/event/ev01_0100.xq",
             "data/res/battle/battle_config_0.09k.cfg.bin",
             "data/res/battle/battle_rule_config_0.03g.cfg.bin",
             "data/res/battle/battle_command_0.04d.cfg.bin",
         }) {
        archive.files.push_back({path, 0, 1, 0});
    }
    const auto manifest = ykw2::core::select_runtime_content_manifest(archive);
    assert(manifest.has_opening_event());
    assert(manifest.has_battle_definition());
}

}  // namespace

int main() {
    test_loads_canonical_archive_and_fallback_path();
    test_reports_missing_archive();
    test_selects_scene_resources_by_role_and_path();
    test_uses_only_title_related_image_for_menu();
    test_selects_runtime_content_manifest();
    return 0;
}
