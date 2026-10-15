#include "ykw2/core/initial_content.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <utility>

#include "ykw2/core/arc0_reader.hpp"
#include "ykw2/core/imgc_reader.hpp"
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

bool is_catalog_identifier(std::string_view value) {
    // map_sound also stores serialized/base64 playback metadata as strings.
    // Expose only short symbolic IDs to the runtime catalog.
    if (value.empty() || value.size() > 128 || value.find('=') != std::string_view::npos) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char byte) {
        return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
               (byte >= '0' && byte <= '9') || byte == '_' || byte == '-' || byte == '.';
    });
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
    return (starts_with(path, "data/map/bg/") || starts_with(path, "data/menu/img/")) &&
           ends_with(path, ".xi");
}

const Arc0File* first_background_match(const Arc0Archive& archive) {
    // The 3DS dump uses a localized menu image for the first native screen;
    // keep a deterministic map-background fallback for older/sample archives.
    if (const auto* preferred = archive.find("data/menu/img/esus/transfer_image00.xi");
        preferred != nullptr) {
        return preferred;
    }
    if (const auto* map_background = first_path_match(archive, [](std::string_view path) {
            return starts_with(path, "data/map/bg/") && ends_with(path, ".xi");
        });
        map_background != nullptr) {
        return map_background;
    }
    return first_path_match(archive, is_background_resource);
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
        // Decode only the first bounded mesh preview. This keeps startup
        // cheap while proving that the map geometry is coming from YW2 data.
        for (const auto& child : parsed.value->files) {
            if (!ends_with(child.name, ".prm")) {
                continue;
            }
            std::vector<std::uint8_t> bytes(child.size);
            if (!window.value->read_exact(window.value->virtual_path(), child.offset,
                                          bytes.data(), bytes.size())) {
                return inspect_failure<InitialSceneResource>(
                    "initial_scene_mesh_read_failed");
            }
            const auto mesh = parse_xmpr_model(ByteView(bytes.data(), bytes.size()));
            if (!mesh) {
                continue;
            }
            resource.mesh_vertex_count += mesh.value->vertex_count;
            resource.mesh_triangle_index_count += mesh.value->triangle_index_count;
            if (resource.mesh_preview_positions.empty()) {
                resource.mesh_material_hash = mesh.value->material_hash;
                resource.mesh_preview_positions = mesh.value->preview_positions;
                resource.mesh_preview_indices = mesh.value->preview_indices;
            }
            break;
        }
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
    const auto image = decode_imgc_image(*window.value, window.value->virtual_path());
    if (!image) {
        return inspect_failure<InitialSceneResource>(
            image.error->code, image.error->offset);
    }
    resource.kind = ResourceKind::Imgc;
    resource.texture_width = image.value->width;
    resource.texture_height = image.value->height;
    resource.texture_format = image.value->format;
    resource.texture_rgba_bytes = image.value->rgba.size();
    return ParseResult<InitialSceneResource>::success(std::move(resource));
}

ParseResult<CfgBinary> inspect_cfg_resource(
    const ResourceStore& store, std::string_view archive_path,
    const Arc0File& file, std::string_view path) {
    const auto window = ResourceWindowStore::create(
        store, archive_path, file.offset, file.size, std::string(path));
    if (!window) {
        return ParseResult<CfgBinary>::failure(
            window.error->code, window.error->offset);
    }
    return read_cfg_binary(*window.value, path);
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
        const auto* file = candidate.role == InitialSceneResourceRole::Background
                               ? first_background_match(archive)
                               : first_path_match(archive, candidate.matches);
        if (file != nullptr) {
            InitialSceneResource resource;
            resource.role = candidate.role;
            resource.path = file->path;
            resource.offset = file->offset;
            resource.size = file->size;
            resources.push_back(std::move(resource));
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
    result.runtime_content = select_runtime_content_manifest(*archive.value);
    const auto inspect_manifest_cfg = [&](std::string_view path,
                                          std::optional<CfgBinaryInfo>& destination,
                                          std::vector<std::string>* strings)
        -> bool {
        if (path.empty()) {
            return true;
        }
        const auto* file = archive.value->find(path);
        if (file == nullptr) {
            return false;
        }
        const auto parsed = inspect_cfg_resource(store, result.archive_path, *file, path);
        if (!parsed) {
            return false;
        }
        destination = parsed.value->header;
        if (strings != nullptr) {
            for (const auto& entry : parsed.value->entries) {
                for (const auto& value : entry.values) {
                    if (value.type == CfgValueType::String &&
                        is_catalog_identifier(value.string_bytes)) {
                        strings->push_back(value.string_bytes);
                    }
                }
            }
        }
        return true;
    };
    if (!inspect_manifest_cfg(result.runtime_content.battle_config_path,
                              result.battle_config_header,
                              &result.battle_content.battle_sequence_ids) ||
        !inspect_manifest_cfg(result.runtime_content.battle_rule_path,
                              result.battle_rule_header, nullptr) ||
        !inspect_manifest_cfg(result.runtime_content.battle_command_path,
                              result.battle_command_header,
                              &result.battle_content.command_ids) ||
        !inspect_manifest_cfg(result.runtime_content.battle_view_config_path,
                              result.battle_view_config_header, nullptr) ||
        !inspect_manifest_cfg(result.runtime_content.battle_ai_path,
                              result.battle_ai_header, nullptr) ||
        !inspect_manifest_cfg(result.runtime_content.map_sound_config_path,
                              result.map_sound_config_header,
                              &result.battle_content.sound_ids) ||
        !inspect_manifest_cfg(result.runtime_content.opening_event_sound_config_path,
                              result.opening_event_sound_config_header, nullptr)) {
        return failure<InitialContentInfo>("initial_runtime_config_invalid");
    }
    result.battle_content.battle_config_entries =
        result.battle_config_header ? result.battle_config_header->field_0x00 : 0;
    result.battle_content.battle_rule_entries =
        result.battle_rule_header ? result.battle_rule_header->field_0x00 : 0;
    result.battle_content.battle_command_entries =
        result.battle_command_header ? result.battle_command_header->field_0x00 : 0;
    result.battle_content.battle_view_entries =
        result.battle_view_config_header ? result.battle_view_config_header->field_0x00 : 0;
    result.battle_content.battle_ai_entries =
        result.battle_ai_header ? result.battle_ai_header->field_0x00 : 0;
    result.battle_content.ready = result.runtime_content.has_battle_definition() &&
                                  result.battle_config_header.has_value() &&
                                  result.battle_rule_header.has_value() &&
                                  result.battle_command_header.has_value();
    if (result.runtime_content.has_opening_event()) {
        const auto* opening_file = archive.value->find(result.runtime_content.opening_event_path);
        if (opening_file == nullptr) {
            return failure<InitialContentInfo>("initial_opening_event_not_found");
        }
        const auto opening_window = ResourceWindowStore::create(
            store, result.archive_path, opening_file->offset, opening_file->size,
            result.runtime_content.opening_event_path);
        if (!opening_window) {
            return failure<InitialContentInfo>(opening_window.error->code,
                                               opening_window.error->offset);
        }
        const auto opening_program = read_xq32_program(
            *opening_window.value, opening_window.value->virtual_path());
        if (!opening_program) {
            return failure<InitialContentInfo>(opening_program.error->code,
                                               opening_program.error->offset);
        }
        result.opening_event_program = *opening_program.value;
    }
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
        if (resource.role == InitialSceneResourceRole::Background) {
            const auto window = ResourceWindowStore::create(
                store, result.archive_path, resource.offset, resource.size,
                resource.path);
            if (!window) {
                return failure<InitialContentInfo>(
                    window.error->code, window.error->offset);
            }
            const auto image = decode_imgc_image(*window.value, resource.path);
            if (!image) {
                return failure<InitialContentInfo>(
                    image.error->code, image.error->offset);
            }
            result.background_width = image.value->width;
            result.background_height = image.value->height;
            result.background_format = image.value->format;
            result.background_rgba = std::move(image.value->rgba);
        }
    }
    return ParseResult<InitialContentInfo>::success(std::move(result));
}

}  // namespace ykw2::core
