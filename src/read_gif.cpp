#include "read_gif.h"
#include "utils.h"
#include <cctype>

namespace {
    template<typename T>
    T _read(std::span<const uint8_t>& data) {
        return ::read<T, std::endian::little>(data);
    }

    constexpr auto SIGNATURE = convert<uint32_t, std::endian::native, std::endian::big>(0x47494638);
    constexpr auto VERSION_87A = convert<uint16_t, std::endian::native, std::endian::big>(0x3761);
    constexpr auto VERSION_89A = convert<uint16_t, std::endian::native, std::endian::big>(0x3961);
}

std::span<const uint8_t> read_gif(std::span<const uint8_t> data) {
    // based on https://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html

    if (peek<decltype(SIGNATURE)>(data) != SIGNATURE) [[likely]] {
        return {};
    }

    const auto start = data.data();

    skip<decltype(SIGNATURE)>(data);
    if (
        (peek<decltype(VERSION_87A)>(data) != VERSION_87A) &&
        (peek<decltype(VERSION_89A)>(data) != VERSION_89A)
    ) {
        return {};
    }
    skip<decltype(VERSION_89A)>(data);

    skip<uint16_t>(data); // width
    skip<uint16_t>(data); // height
    auto flags = _read<uint8_t>(data);
    skip<uint8_t>(data); // background color index
    skip<uint8_t>(data); // pixel aspect ratio

    bool global_color_table_flag = (flags >> 7) & 1;
    int  size_of_global_color_table = flags & 7;

    if (global_color_table_flag) {
        auto N = 1 << (size_of_global_color_table + 1);
        auto S = 3 * N;
        skip<uint8_t>(data, S);
    }

    bool found_trailer = false;
    bool found_imagedescriptor = false;

    while (!found_trailer && !data.empty()) {
        auto introducer = _read<uint8_t>(data);

        switch (introducer) {
            case 0x21: // extension introducer
            {
                auto label = _read<uint8_t>(data);
                switch (label) {
                    case 0x01: // plain text
                    case 0xFF: // application
                    {
                        skip<uint8_t>(data, peek<uint8_t>(data));
                        // read data
                        auto size = _read<uint8_t>(data);
                        while (size) {
                            skip<uint8_t>(data, size);
                            size = _read<uint8_t>(data);
                        }
                    } break;
                    case 0xF9: // graphic control
                    {
                        skip<uint8_t>(data, peek<uint8_t>(data));
                        if (_read<uint8_t>(data) != 0x00) {
                            // wrong block terminator
                            return {};
                        }
                    } break;
                    case 0xFE: // comment
                    {
                        // read data
                        auto size = _read<uint8_t>(data);
                        while (size) {
                            skip<uint8_t>(data, size);
                            size = _read<uint8_t>(data);
                        }
                    } break;
                    default:
                        return {};
                }
            } break;
            case 0x2C: // image descriptor
            {
                found_imagedescriptor = true;
                skip<uint16_t>(data, 4);
                auto flags = _read<uint8_t>(data);
                bool local_color_table_flag = (flags >> 7) & 1;
                int  size_of_local_color_table = flags & 7;
                // read color table
                if (local_color_table_flag) {
                    auto N = 1 << (size_of_local_color_table);
                    auto S = 3 * N;
                    skip<uint8_t>(data, S);
                }
                // read data
                skip<uint8_t>(data);
                auto size = _read<uint8_t>(data);
                while (size) {
                    skip<uint8_t>(data, size);
                    size = _read<uint8_t>(data);
                }
            } break;
            case 0x3B:
                if (!found_imagedescriptor) {
                    return {};
                }
                found_trailer = true;
                break;
            default:
                return {};
        }
    }

    if (!found_trailer) {
        return {};
    }

    auto end = data.data();

    return { start, end };
}

