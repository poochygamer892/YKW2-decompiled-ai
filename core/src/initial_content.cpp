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
    return ParseResult<InitialContentInfo>::success(std::move(result));
}

}  // namespace ykw2::core
