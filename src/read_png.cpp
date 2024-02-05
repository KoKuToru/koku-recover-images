#include "read_png.h"
#include "utils.h"
#include <cctype>

namespace {
    uint32_t read(const std::span<const uint8_t> data) {
        return ::read<uint32_t, std::endian::big>(data);
    }

    // https://www.w3.org/TR/PNG-CRCAppendix.html

    uint32_t crc32_table[256];

    void make_crc32_table(void)
    {
        uint32_t c;
        int n, k;

        for (n = 0; n < 256; n++) {
            c = (uint32_t) n;
            for (k = 0; k < 8; k++) {
                if (c & 1)
                c = 0xedb88320 ^ (c >> 1);
                else
                c = c >> 1;
            }
            crc32_table[n] = c;
        }
    }

    uint32_t update_crc32(uint32_t crc, const uint8_t* buf, int len)
    {
        uint32_t c = crc;
        int n;

        static bool crc32_table_computed = false;
        if (!crc32_table_computed) [[unlikely]] {
            make_crc32_table();
            crc32_table_computed = true;
        }
        for (n = 0; n < len; n++) {
            c = crc32_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
        }
        return c;
    }

    uint32_t crc32(const uint8_t* buf, int len)
    {
        return update_crc32(0xffffffff, buf, len) ^ 0xffffffff;
    }

    // SOI in big-endian
    constexpr auto SIGNATURE = convert<uint64_t, std::endian::native, std::endian::big>(0x89504E470D0A1A0A);
}

std::span<const uint8_t> read_png(std::span<const uint8_t> data) {
    if (read<decltype(SIGNATURE)>(data) != SIGNATURE) [[likely]] {
        return {};
    }

    const auto start = data.data();

    // based on https://en.wikipedia.org/wiki/PNG

    data = subspan(data, sizeof(uint64_t));

    bool found_ihdr = false;
    bool found_idat = false;
    bool found_iend = false;
    while (!found_iend && !data.empty()) {
        const auto length = read(data);
        data = subspan(data, sizeof(uint32_t));
        const auto type = read(data);
        const auto crcdata = subspan(data, 0, sizeof(uint32_t) + length);
        data = subspan(data, sizeof(uint32_t) + length);
        const auto crc = read(data);
        data = subspan(data, sizeof(uint32_t));
        switch (type) {
            case 0x49484452: // IHDR
                if (found_ihdr) {
                    return {};
                }
                found_ihdr = true;
                break;
            case 0x49444154: // IDAT
                if (!found_ihdr) {
                    return {};
                }
                found_idat = true;
                break;
            case 0x49454E44: // IEND
                if (
                    !found_ihdr ||
                    !found_idat
                ) {
                    return {};
                }
                found_iend = true;
                break;
            case 0x504C5445: // PLTE
                if (!found_ihdr) {
                    return {};
                }
                // not always required
                break;
            default:
                if (!found_ihdr) {
                    return {};
                }
                if (!std::islower(type >> 24)) {
                    return {};
                }
                break;
        }
        // check crc
        uint32_t crc_calculated = crc32(crcdata.data(), crcdata.size());
        if (crc != crc_calculated) {
            return {};
        }
    }

    if (!found_iend) {
        return {};
    }

    const auto end = data.data();

    return { start, end };
}

