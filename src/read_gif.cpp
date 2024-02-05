#include "read_gif.h"
#include "utils.h"
#include <cctype>

namespace {
    template<typename T>
    T _read(const std::span<const uint8_t> data) {
        return ::read<T, std::endian::little>(data);
    }


    constexpr auto SIGNATURE = convert<uint32_t, std::endian::native, std::endian::big>(0x47494638);
    constexpr auto VERSION_87A = convert<uint16_t, std::endian::native, std::endian::big>(0x3761);
    constexpr auto VERSION_89A = convert<uint16_t, std::endian::native, std::endian::big>(0x3961);
}

std::span<const uint8_t> read_gif(std::span<const uint8_t> data) {
    // based on https://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html

    if (_read<decltype(SIGNATURE)>(data) != SIGNATURE) [[likely]] {
        return {};
    }

    const auto start = data.data();

    data = subspan(data, sizeof(decltype(SIGNATURE)));
    if (
        (_read<decltype(VERSION_87A)>(data) != VERSION_87A) &&
        (_read<decltype(VERSION_89A)>(data) != VERSION_89A)
    ) {
        return {};
    }
    data = subspan(data, sizeof(decltype(VERSION_89A)));

    data = subspan(data, sizeof(uint16_t)); // width
    data = subspan(data, sizeof(uint16_t)); // height
    auto flags = _read<uint8_t>(data);
    data = subspan(data, sizeof(uint8_t));
    data = subspan(data, sizeof(uint8_t)); // background color index
    data = subspan(data, sizeof(uint8_t)); // pixel aspect ratio

    bool global_color_table_flag = (flags >> 7) & 1;
    int  size_of_global_color_table = flags & 7;

    if (global_color_table_flag) {
        auto N = 1 << (size_of_global_color_table + 1);
        auto S = 3 * N;
        data = subspan(data, sizeof(uint8_t) * S);
    }

    bool found_trailer = false;
    bool found_imagedescriptor = false;

    while (!found_trailer && !data.empty()) {
        auto introducer = _read<uint8_t>(data);
        data = subspan(data, sizeof(uint8_t));

        switch (introducer) {
            case 0x21: // extension introducer
            {
                auto label = _read<uint8_t>(data);
                data = subspan(data, sizeof(uint8_t));
                switch (label) {
                    case 0x01: // plain text
                    case 0xFF: // application
                    {
                        data = subspan(data, _read<uint8_t>(data)); // skip
                        // read data
                        auto size = _read<uint8_t>(data);
                        subspan(data, sizeof(uint8_t));
                        while (size) {
                            data = subspan(data, sizeof(uint8_t) * size);
                            size = _read<uint8_t>(data);
                            data = subspan(data, sizeof(uint8_t));
                        }
                    } break;
                    case 0xF9: // graphic control
                    {
                        data = subspan(data, _read<uint8_t>(data)); // skip
                        if (_read<uint8_t>(data) != 0x00) {
                            // wrong block terminator
                            return {};
                        }
                        data = subspan(data, sizeof(uint8_t));
                    } break;
                    case 0xFE: // comment
                    {
                        // read data
                        auto size = _read<uint8_t>(data);
                        data = subspan(data, sizeof(uint8_t));
                        while (size) {
                            data = subspan(data, sizeof(uint8_t) * size);
                            size = _read<uint8_t>(data);
                            data = subspan(data, sizeof(uint8_t));
                        }
                    } break;
                    default:
                        return {};
                }
            } break;
            case 0x2C: // image descriptor
            {
                found_imagedescriptor = true;
                data = subspan(data, sizeof(uint16_t) * 4);
                auto flags = _read<uint8_t>(data);
                data = subspan(data, sizeof(uint8_t));
                bool local_color_table_flag = (flags >> 7) & 1;
                int  size_of_local_color_table = flags & 7;
                // read color table
                if (local_color_table_flag) {
                    auto N = 1 << (size_of_local_color_table);
                    auto S = 3 * N;
                    data = subspan(data, sizeof(uint8_t) * S);
                }
                // read data
                data = subspan(data, sizeof(uint8_t));
                auto size = _read<uint8_t>(data);
                data = subspan(data, sizeof(uint8_t));
                while (size) {
                    data = subspan(data, sizeof(uint8_t) * size);
                    size = _read<uint8_t>(data);
                    data = subspan(data, sizeof(uint8_t));
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

    auto end = data.data();

    return { start, end };
}

