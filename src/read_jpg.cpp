#include "read_jpg.h"
#include "utils.h"

namespace {
    uint16_t read(std::span<const uint8_t>& data) {
        return ::read<uint16_t, std::endian::big>(data);
    }
    uint16_t peek(const std::span<const uint8_t> data) {
        return ::peek<uint16_t, std::endian::big>(data);
    }

    // SOI in big-endian
    constexpr auto SIGNATURE = convert<uint16_t, std::endian::native, std::endian::big>(0xFFD8);
}

std::span<const uint8_t> read_jpg(std::span<const uint8_t> data) {
    if (peek<decltype(SIGNATURE)>(data) != SIGNATURE) [[likely]] {
        return {};
    }

    const auto start = data.data();

    // based on https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
    // and https://dev.exiv2.org/projects/exiv2/wiki/The_Metadata_in_JPEG_files

    bool found_dht = false;
    bool found_dqt = false;
    bool found_sos = false;
    bool found_eoi = false;
    bool found_soi = false;
    bool found_dac = false;
    while (!found_eoi && !data.empty()) {
        auto before_read = data;
        switch (read(data)) {
            case 0xFF00: // stuffed 0xFF in SOS
                if (!found_sos) {
                    return {};
                }
                break;
            case 0xFFE0: // APP0
            case 0xFFE1: // APP1 exif
            case 0xFFE2: // APP2
            case 0xFFE3: // APP3
            case 0xFFE4: // APP4
            case 0xFFE5: // APP5
            case 0xFFE6: // APP6
            case 0xFFE7: // APP7
            case 0xFFE8: // APP8
            case 0xFFE9: // APP9
            case 0xFFEA: // APP10
            case 0xFFEB: // APP11
            case 0xFFEC: // APP12
            case 0xFFED: // APP13
            case 0xFFEE: // APP14
            case 0xFFEF: // APP15
            case 0xFFC0: // SOF0
            case 0xFFC1: // SOF1
            case 0xFFC2: // SOF2
            case 0xFFC3: // SOF3
            case 0xFFC5: // SOF5
            case 0xFFC6: // SOF6
            case 0xFFC7: // SOF7
            case 0xFFC8: // JPG reserved
            case 0xFFC9: // SOF9
            case 0xFFCA: // SOF10
            case 0xFFCB: // SOF11
            case 0xFFCD: // SOF13
            case 0xFFCE: // SOF14
            case 0xFFCF: // SOF15
            case 0xFFFE: // COM
                skip<uint8_t>(data, peek(data));
                break;
            case 0xFFDD: // DRI
                skip<uint16_t>(data);
                break;
            case 0xFFD0: // RST0
            case 0xFFD1: // RST1
            case 0xFFD2: // RST2
            case 0xFFD3: // RST3
            case 0xFFD4: // RST4
            case 0xFFD5: // RST5
            case 0xFFD6: // RST6
            case 0xFFD7: // RST7
                if (!found_sos) {
                    return {};
                }
                break;
            case 0xFFD8: // SOI
                if (found_soi) {
                    return {};
                }
                found_soi = true;
                break;
            case 0xFFC4: // DHT
                found_dht = true;
                skip<uint8_t>(data, peek(data));
                break;
            case 0xFFDB: // DQT
                found_dqt = true;
                skip<uint8_t>(data, peek(data));
                break;
            case 0xFFCC: // DAC
                found_dac = true;
                skip<uint8_t>(data, peek(data));
                break;
            case 0xFFD9: // EOI
                if (
                    !found_sos ||
                    !(found_dht || found_dac) ||
                    !found_dqt
                ) {
                    return {};
                }
                found_eoi = true;
                break;
            case 0xFFDA: // SOS
                if (found_sos) {
                    return {};
                }
                found_sos = true;
                break;
            default:
                if (!found_sos) {
                    return {};
                }
                // seek back by 1 byte
                data = subspan(before_read, 1);
                break;
        }
    }

    if (!found_eoi) {
        return {};
    }

    const auto end = data.data();

    return { start, end };
}