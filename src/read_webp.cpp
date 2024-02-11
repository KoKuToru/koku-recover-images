#include "read_webp.h"
#include "utils.h"
#include <cctype>

namespace {
    template<typename T>
    T _read(std::span<const uint8_t>& data) {
        return ::read<T, std::endian::little>(data);
    }

    constexpr auto SIGNATURE_RIFF = convert<uint32_t, std::endian::native, std::endian::big>(0x52494646);
    constexpr auto SIGNATURE_WEBP = convert<uint32_t, std::endian::native, std::endian::big>(0x57454250);
}

std::span<const uint8_t> read_webp(std::span<const uint8_t> data) {
    // based on https://developers.google.com/speed/webp/docs/riff_container

    if (peek<decltype(SIGNATURE_RIFF)>(data) != SIGNATURE_RIFF) [[likely]] {
        return {};
    }
    if (peek<decltype(SIGNATURE_WEBP)>(data, 0x08) != SIGNATURE_WEBP) [[likely]] {
        return {};
    }

    auto start = data.data();

    skip<decltype(SIGNATURE_RIFF)>(data);
    auto size = read<uint32_t, std::endian::little>(data);
    if (size % 2 != 0) {
        size += 1;
    }
    data = subspan(data, 0, size);
    if (data.size() != size) {
        return {};
    }

    auto end = data.data() + size;

    skip<decltype(SIGNATURE_WEBP)>(data);
    switch (read<uint32_t, std::endian::big>(data))
    {
        case 0x56503820: // VP8
        case 0x5650384C: // VP8L
        case 0x56503858: // VP8X
            break;
        default:
            return {};
    }

    return {start, end};
}

