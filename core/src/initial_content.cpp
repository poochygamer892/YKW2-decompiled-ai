#include "ykw2/core/initial_content.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#include "ykw2/core/arc0_reader.hpp"

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
    return ParseResult<InitialContentInfo>::success(std::move(result));
}

}  // namespace ykw2::core
