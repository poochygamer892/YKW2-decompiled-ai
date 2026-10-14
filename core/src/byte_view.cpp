#include "ykw2/core/byte_view.hpp"

#include <cstring>

namespace ykw2::core {

bool ByteView::contains(std::size_t offset, std::size_t length) const {
    return offset <= size_ && length <= size_ - offset;
}

ByteView ByteView::subview(std::size_t offset, std::size_t length) const {
    if (!contains(offset, length)) {
        return {};
    }
    return ByteView(data_ + offset, length);
}

bool ByteView::starts_with(std::string_view value) const {
    return value.size() <= size_ &&
           (value.empty() || std::memcmp(data_, value.data(), value.size()) == 0);
}

std::optional<std::uint8_t> ByteView::u8(std::size_t offset) const {
    if (!contains(offset)) {
        return std::nullopt;
    }
    return data_[offset];
}

std::optional<std::uint16_t> ByteView::u16_le(std::size_t offset) const {
    if (!contains(offset, 2)) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(data_[offset]) |
           (static_cast<std::uint16_t>(data_[offset + 1]) << 8);
}

std::optional<std::uint32_t> ByteView::u32_le(std::size_t offset) const {
    if (!contains(offset, 4)) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(data_[offset]) |
           (static_cast<std::uint32_t>(data_[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data_[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data_[offset + 3]) << 24);
}

std::optional<std::int16_t> ByteView::i16_le(std::size_t offset) const {
    const auto value = u16_le(offset);
    if (!value) {
        return std::nullopt;
    }
    return static_cast<std::int16_t>(*value);
}

}  // namespace ykw2::core
