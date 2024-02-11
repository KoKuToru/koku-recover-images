#ifndef H_UTILS
#define H_UTILS

#include <bit>
#include <cstring>
#include <span>
#include <type_traits>

template<typename T> std::span<T> subspan(const std::span<T> span, std::size_t offset, std::size_t count = std::dynamic_extent) {
    const auto n_offset_start = std::min(offset, span.size());
    const auto n_offset_end   = std::max(n_offset_start, std::min(offset + count, span.size()));
    if (count == std::dynamic_extent) [[likely]] {
        return span.subspan(n_offset_start);
    }
    return span.subspan(n_offset_start, n_offset_end - n_offset_start);
}

template<typename T, std::endian source, std::endian target = std::endian::native> constexpr T convert(const T value) {
    if constexpr (source == target) {
        return value;
    }
    return std::byteswap(value);
}

template<typename T, std::endian endian = std::endian::native> T peek(std::span<const uint8_t> data, size_t offset = 0) {
    data = subspan(data, offset);
    if (data.size() < sizeof(T)) [[unlikely]] {
        return T{};
    }
    typename std::remove_cvref<T>::type value;
    std::memcpy(&value, data.data(), sizeof(T));
    return convert<T, endian, std::endian::native>(value);
}

template<typename T, std::endian endian = std::endian::native> T read(std::span<const uint8_t>& data) {
    T tmp = peek<T, endian>(data);
    data = subspan(data, sizeof(T));
    return tmp;
}

template<typename T> void skip(std::span<const uint8_t>& data, size_t n = 1) {
    data = subspan(data, sizeof(T) * n);
}

#endif