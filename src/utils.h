#ifndef H_UTILS
#define H_UTILS

#include <bit>
#include <cstring>
#include <span>
#include <type_traits>

template<typename T> std::span<T> subspan(const std::span<T> span, std::size_t offset, std::size_t count = std::dynamic_extent) {
    const auto n_offset = std::min(offset, span.size());
    if (count == std::dynamic_extent) [[likely]] {
        return span.subspan(n_offset);
    }
    const auto n_offset_size = std::max(
        n_offset,
        std::min(offset + count, span.size())
    ) - n_offset;

    return span.subspan(n_offset, n_offset_size);
}

template<typename T, std::endian source, std::endian target = std::endian::native> constexpr T convert(const T value) {
    if constexpr (source == target) {
        return value;
    }
    return std::byteswap(value);
}

template<typename T, std::endian endian = std::endian::native> T read(const std::span<const uint8_t> data) {
    if (data.size() < sizeof(T)) [[unlikely]] {
        return 0;
    }
    typename std::remove_cvref<T>::type value;
    std::memcpy(&value, data.data(), sizeof(T));
    return convert<T, endian, std::endian::native>(value);
}

#endif