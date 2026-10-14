#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace ykw2::core {

// Non-owning bytes supplied by a platform adapter or a test fixture.
// The core never assumes that a resource is memory-mapped or heap-owned.
class ByteView {
public:
    ByteView() = default;
    ByteView(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    const std::uint8_t* data() const { return data_; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    bool contains(std::size_t offset, std::size_t length = 1) const;
    ByteView subview(std::size_t offset, std::size_t length) const;
    bool starts_with(std::string_view value) const;

    std::optional<std::uint8_t> u8(std::size_t offset) const;
    std::optional<std::uint16_t> u16_le(std::size_t offset) const;
    std::optional<std::uint32_t> u32_le(std::size_t offset) const;
    std::optional<std::uint64_t> u64_le(std::size_t offset) const;
    std::optional<std::int16_t> i16_le(std::size_t offset) const;

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
};

struct ParseError {
    std::string_view code;
    std::size_t offset = 0;
};

template <typename T>
struct ParseResult {
    std::optional<T> value;
    std::optional<ParseError> error;

    static ParseResult success(T parsed) {
        ParseResult result;
        result.value = std::move(parsed);
        return result;
    }

    static ParseResult failure(std::string_view code, std::size_t offset) {
        ParseResult result;
        result.error = ParseError{code, offset};
        return result;
    }

    explicit operator bool() const { return value.has_value(); }
};

}  // namespace ykw2::core
