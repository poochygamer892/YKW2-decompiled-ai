#pragma once

#include <cstddef>
#include <cstdint>

namespace ykw2::core {

// Platform-neutral random-access contract. Android can back this with an
// AAsset or file descriptor; iOS can back it with NSFileHandle or a bundle
// resource. The core only sees bounded reads and never owns the source.
class ResourceSource {
public:
    virtual ~ResourceSource() = default;

    virtual std::uint64_t size() const = 0;
    virtual bool read(std::uint64_t offset, std::uint8_t* destination,
                      std::size_t length) const = 0;

    bool contains(std::uint64_t offset, std::size_t length) const {
        return offset <= size() && length <= size() - offset;
    }
};

}  // namespace ykw2::core
