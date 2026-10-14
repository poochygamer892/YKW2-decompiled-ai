#include "ykw2/core/initial_content.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <utility>

#include "ykw2/core/arc0_reader.hpp"
#include "ykw2/core/xpck_reader.hpp"

namespace ykw2::core {
namespace {

template <typename T>
ParseResult<T> failure(std::string_view code, std::size_t offset = 0) {
    return ParseResult<T>::failure(code, offset);
}

char lower_ascii(char value) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

bool equal_name(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    return std::equal(left.begin(), left.end(), right.begin(),
                      [](char a, char b) { return lower_ascii(a) == lower_ascii(b); });
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size()) == suffix;
}

const Arc0File* first_path_match(const Arc0Archive& archive,
                                 bool (*matches)(std::string_view)) {
    const Arc0File* selected = nullptr;
    for (const auto& file : archive.files) {
        if (!matches(file.path)) {
            continue;
        }
        if (selected == nullptr || file.path < selected->path) {
            selected = &file;
        }
    }
    return selected;
}

bool is_map_resource(std::string_view path) {
    return starts_with(path, "data/map/") && ends_with(path, ".xc");
}

bool is_character_resource(std::string_view path) {
    return starts_with(path, "data/character/") && ends_with(path, ".xc");
}

bool is_background_resource(std::string_view path) {
    return starts_with(path, "data/map/bg/") && ends_with(path, ".xi");
}

template <typename T>
ParseResult<T> inspect_failure(std::string_view code, std::size_t offset = 0) {
    return ParseResult<T>::failure(code, offset);
}

ParseResult<InitialSceneResource> inspect_scene_resource(
    const ResourceStore& store, std::string_view archive_path,
    const Arc0File& file, InitialSceneResource resource) {
    const auto window = ResourceWindowStore::create(
        store, archive_path, file.offset, file.size);
    if (!window) {
        return inspect_failure<InitialSceneResource>(
            window.error->code, window.error->offset);
    }

    if (resource.role == InitialSceneResourceRole::Map ||
        resource.role == InitialSceneResourceRole::Character) {
        const auto parsed = read_xpck_archive(*window.value, window.value->virtual_path());
        if (!parsed) {
            return inspect_failure<InitialSceneResource>(
                parsed.error->code, parsed.error->offset);
        }
        resource.kind = ResourceKind::Xpck;
        resource.child_count = parsed.value->files.size();
        return ParseResult<InitialSceneResource>::success(std::move(resource));
    }

    std::array<std::uint8_t, 0x40> header{};
    if (!window.value->read_exact(window.value->virtual_path(), 0,
                                  header.data(), header.size())) {
        return inspect_failure<InitialSceneResource>(
            "initial_scene_resource_header_read_failed");
    }
    const auto parsed = parse_imgc_header(ByteView(header.data(), header.size()));
    if (!parsed) {
        return inspect_failure<InitialSceneResource>(
            parsed.error->code, parsed.error->offset);
    }
    resource.kind = ResourceKind::Imgc;
    return ParseResult<InitialSceneResource>::success(std::move(resource));
}

std::string_view basename(std::string_view path) {
    const auto separator = path.find_last_of('/');
    return separator == std::string_view::npos ? path : path.substr(separator + 1);
}

const CatalogEntry* find_archive(const ResourceCatalog& catalog,
                                 std::string_view preferred_name) {
    if (const auto* exact = catalog.find(preferred_name);
        exact != nullptr && exact->kind == ResourceKind::Arc0) {
        return exact;
    }
    for (const auto& entry : catalog.entries) {
        if (entry.kind == ResourceKind::Arc0 &&
            equal_name(basename(entry.path), basename(preferred_name))) {
            return &entry;
        }
    }
    return nullptr;
}

}  // namespace

const char* initial_scene_resource_role_name(InitialSceneResourceRole role) {
    switch (role) {
        case InitialSceneResourceRole::Map:
            return "map";
        case InitialSceneResourceRole::Character:
            return "character";
        case InitialSceneResourceRole::Background:
            return "background";
    }
    return "unknown";
}

std::vector<InitialSceneResource> select_initial_scene_resources(
    const Arc0Archive& archive) {
    const struct Candidate {
        InitialSceneResourceRole role;
        bool (*matches)(std::string_view);
    } candidates[] = {
        {InitialSceneResourceRole::Map, is_map_resource},
        {InitialSceneResourceRole::Character, is_character_resource},
        {InitialSceneResourceRole::Background, is_background_resource},
    };

    std::vector<InitialSceneResource> resources;
    for (const auto& candidate : candidates) {
        const auto* file = first_path_match(archive, candidate.matches);
        if (file != nullptr) {
            resources.push_back({candidate.role, file->path, file->offset, file->size});
        }
    }
    return resources;
}

ParseResult<InitialContentInfo> load_initial_content(
    const ResourceStore& store, std::string_view preferred_name) {
    const auto* archive_entry = find_archive(store.catalog(), preferred_name);
    if (archive_entry == nullptr) {
        return failure<InitialContentInfo>("initial_content_not_found");
    }

    const auto archive = read_arc0_archive(store, archive_entry->path);
    if (!archive) {
        return failure<InitialContentInfo>(archive.error->code, archive.error->offset);
    }
    if (archive.value->files.empty()) {
        return failure<InitialContentInfo>("initial_content_empty");
    }

    const auto& first_file = archive.value->files.front();
    InitialContentInfo result;
    result.archive_path = archive_entry->path;
    result.archive_files = archive.value->files.size();
    result.first_file_path = first_file.path;
    result.first_file_size = first_file.size;
    result.scene_resources = select_initial_scene_resources(*archive.value);
    for (auto& resource : result.scene_resources) {
        const auto* file = archive.value->find(resource.path);
        if (file == nullptr) {
            return failure<InitialContentInfo>("initial_scene_resource_not_found");
        }
        const auto inspected = inspect_scene_resource(
            store, result.archive_path, *file, std::move(resource));
        if (!inspected) {
            return failure<InitialContentInfo>(
                inspected.error->code, inspected.error->offset);
        }
        resource = std::move(*inspected.value);
    }
    return ParseResult<InitialContentInfo>::success(std::move(result));
}

}  // namespace ykw2::core
