#include "read_tif.h"
#include "utils.h"
#include <cctype>
#include <cstdio>
#include <deque>

namespace {
    // https://www.fileformat.info/format/tiff/egff.htm
    // https://www.itu.int/itudoc/itu-t/com16/tiff-fx/docs/tiff6.pdf

    // Identifier + Version
    constexpr auto SIGNATURE_LITTLE = convert<uint32_t, std::endian::native, std::endian::big>(0x49492A00);
    constexpr auto SIGNATURE_BIG    = convert<uint32_t, std::endian::native, std::endian::big>(0x4D4D002A);

    enum TYPES_ALLOWED {
        BYTE =  1 << 1,
        ASCII = 1 << 2,
        SHORT = 1 << 3,
        LONG = 1 << 4,
        RATIONAL = 1 << 5,
        SBYTE = 1 << 6,
        UNDEFINE = 1 << 7,
        SSHORT = 1 << 8,
        SLONG = 1 << 9,
        SRATIONAL = 1 << 10,
        FLOAT = 1 << 11,
        DOUBLE = 1 << 12,
        IFD = 1 << 13
    };

    template<std::endian endian>
    bool read_ifd(std::span<const uint8_t> start, std::span<const uint8_t> data, uint32_t& length, bool& has_image_data, bool private_ifd = false) {
        // read directory
        bool found_end = false;

        while (!data.empty()) {
            auto offset = peek<uint32_t, endian>(data);
            if (offset == 0) {
                // nothing left to read
                found_end = true;
                break;
            }
            if ((offset % sizeof(uint16_t)) != 0) {
                // must begin on a word boundary
                return false;
            }

            data = subspan(start, offset);
            // read tags
            auto entries = read<uint16_t, endian>(data);

            if (entries == 0) {
                return false;
            }

            length = std::max(length, uint32_t(offset + entries * sizeof(uint32_t) * 3 + sizeof(uint32_t)));

            std::deque<uint32_t> ImageDataOffsets;
            std::deque<uint32_t> ImageDataByteCounts;

            uint16_t last_tag_id = 0;
            for (auto i = 0; i < entries; ++i) {
                auto tag_id = read<uint16_t, endian>(data);

                if (!private_ifd && tag_id <= last_tag_id) {
                    // The entries in an IFD must be sorted in ascending order by Tag
                    return false;
                }
                last_tag_id = tag_id;

                auto data_type = read<uint16_t, endian>(data);
                auto data_count = read<uint32_t, endian>(data);
                auto data_offset_before_offset = data;
                auto data_offset = read<uint32_t, endian>(data);

                // https://www.awaresystems.be/imaging/tiff/tifftags.html
                uint32_t required_type = 0;
                uint32_t required_count = 0;
                int ifd = 0;
                if (!private_ifd) {
                    switch (tag_id) {
                        // Baseline Tags
                        case 0x00FE: required_type = LONG;         required_count = 1;  break; // NewSubfileType
                        case 0x00FF: required_type = SHORT;        required_count = 1;  break; // SubfileType
                        case 0x0100: required_type = SHORT | LONG; required_count = 1;  break; // ImageWidth
                        case 0x0101: required_type = SHORT | LONG; required_count = 1;  break; // ImageLength
                        case 0x0102: required_type = SHORT;                             break; // BitsPerSample
                        case 0x0103: required_type = SHORT;        required_count = 1;  break; // Compression
                        case 0x0106: required_type = SHORT;        required_count = 1;  break; // PhotometricInterpretation
                        case 0x0107: required_type = SHORT;        required_count = 1;  break; // Threshholding
                        case 0x0108: required_type = SHORT;        required_count = 1;  break; // CellWidth
                        case 0x0109: required_type = SHORT;        required_count = 1;  break; // CellLength
                        case 0x010A: required_type = SHORT;        required_count = 1;  break; // FillOrder
                        case 0x010E: required_type = ASCII;                             break; // ImageDescription
                        case 0x010F: required_type = ASCII;                             break; // Make
                        case 0x0110: required_type = ASCII;                             break; // Model
                        case 0x0111: required_type = SHORT | LONG;                      break; // StripOffsets
                        case 0x0112: required_type = SHORT;        required_count = 1;  break; // Orientation
                        case 0x0115: required_type = SHORT;        required_count = 1;  break; // SamplesPerPixel
                        case 0x0116: required_type = SHORT | LONG; required_count = 1;  break; // RowsPerStrip
                        case 0x0117: required_type = SHORT | LONG;                      break; // StripByteCounts
                        case 0x0118: required_type = SHORT;                             break; // MinSampleValue
                        case 0x0119: required_type = SHORT;                             break; // MaxSampleValue
                        case 0x011A: required_type = RATIONAL;     required_count = 1;  break; // XResolution
                        case 0x011B: required_type = RATIONAL;     required_count = 1;  break; // YResolution
                        case 0x011C: required_type = SHORT;        required_count = 1;  break; // PlanarConfiguration
                        case 0x0120: required_type = LONG;                              break; // FreeOffsets
                        case 0x0121: required_type = LONG;                              break; // FreeByteCounts
                        case 0x0122: required_type = SHORT;        required_count = 1;  break; // GrayResponseUnit
                        case 0x0123: required_type = SHORT;                             break; // GrayResponseCurve
                        case 0x0128: required_type = SHORT;        required_count = 1;  break; // ResolutionUnit
                        case 0x0131: required_type = ASCII;                             break; // Software
                        case 0x0132: required_type = ASCII;        required_count = 20; break; // DateTime
                        case 0x013B: required_type = ASCII;                             break; // Artist
                        case 0x013C: required_type = ASCII;                             break; // HostComputer
                        case 0x0140: required_type = SHORT;                             break; // ColorMap
                        case 0x0152: required_type = SHORT;                             break; // ExtraSamples
                        case 0x8298: required_type = ASCII;                             break; // Copyright
                        // Extension Tags
                        case 0x010D: required_type = ASCII;                             break; // DocumentName
                        case 0x011D: required_type = ASCII;                             break; // PageName
                        case 0x011E: required_type = RATIONAL;     required_count = 1;  break; // XPosition
                        case 0x011F: required_type = RATIONAL;     required_count = 1;  break; // YPosition
                        case 0x0124: required_type = LONG;         required_count = 1;  break; // T4Options
                        case 0x0125: required_type = LONG;         required_count = 1;  break; // T6Options
                        case 0x0129: required_type = SHORT;        required_count = 2;  break; // PageNumber
                        case 0x012D: required_type = SHORT;                             break; // TransferFunction
                        case 0x013D: required_type = SHORT;        required_count = 1;  break; // Predictor
                        case 0x013E: required_type = RATIONAL;     required_count = 2;  break; // WhitePoint
                        case 0x013F: required_type = RATIONAL;     required_count = 6;  break; // PrimaryChromaticities
                        case 0x0141: required_type = SHORT;        required_count = 2;  break; // HalftoneHints
                        case 0x0142: required_type = SHORT | LONG; required_count = 1;  break; // TileWidth
                        case 0x0143: required_type = SHORT | LONG; required_count = 1;  break; // TileLength
                        case 0x0144: required_type = LONG;                              break; // TileOffsets
                        case 0x0145: required_type = SHORT | LONG;                      break; // TileByteCounts
                        case 0x0146: required_type = SHORT | LONG; required_count = 1;  break; // BadFaxLines
                        case 0x0147: required_type = SHORT;        required_count = 1;  break; // CleanFaxData
                        case 0x0148: required_type = SHORT | LONG; required_count = 1;  break; // ConsecutiveBadFaxLines
                        case 0x014A: required_type = LONG | IFD;   ifd = 1;             break; // SubIFDs
                        case 0x014C: required_type = SHORT;        required_count = 1;  break; // InkSet
                        case 0x014D: required_type = ASCII;                             break; // InkNames
                        case 0x014E: required_type = SHORT;        required_count = 1;  break; // NumberOfInks
                        case 0x0150: required_type = BYTE | SHORT;                      break; // DotRange
                        case 0x0151: required_type = ASCII;                             break; // TargetPrinter
                        case 0x0153: required_type = SHORT;                             break; // SampleFormat
                        case 0x0154: required_type = BYTE | SHORT | LONG | RATIONAL | DOUBLE; break; // SMinSampleValue
                        case 0x0155: required_type = BYTE | SHORT | LONG | RATIONAL | DOUBLE; break; // SMaxSampleValue
                        case 0x0156: required_type = SHORT;        required_count = 6;  break; // TransferRange
                        case 0x0157: required_type = BYTE;                              break; // ClipPath
                        case 0x0158: required_type = LONG;         required_count = 1;  break; // XClipPathUnits
                        case 0x0159: required_type = LONG;         required_count = 1;  break; // YClipPathUnits
                        case 0x015A: required_type = SHORT;        required_count = 1;  break; // Indexed
                        case 0x015B: required_type = UNDEFINE;                          break; // JPEGTables
                        case 0x015F: required_type = SHORT;        required_count = 1;  break; // OPIProxy
                        case 0x0190: required_type = LONG | IFD;   required_count = 1; ifd = 1; break; // GlobalParametersIFD
                        case 0x0191: required_type = LONG;         required_count = 1;  break; // ProfileType
                        case 0x0192: required_type = BYTE;         required_count = 1;  break; // FaxProfile
                        case 0x0193: required_type = LONG;         required_count = 1;  break; // CodingMethods
                        case 0x0194: required_type = BYTE;         required_count = 4;  break; // VersionYear
                        case 0x0195: required_type = BYTE;         required_count = 1;  break; // ModeNumber
                        case 0x01B1: required_type = SRATIONAL;                         break; // Decode
                        case 0x01B2: required_type = SHORT;                             break; // DefaultImageColor
                        case 0x0200: required_type = SHORT;        required_count = 1;  break; // JPEGProc
                        case 0x0201: required_type = LONG;         required_count = 1;  break; // JPEGInterchangeFormat
                        case 0x0202: required_type = LONG;         required_count = 1;  break; // JPEGInterchangeFormatLength
                        case 0x0203: required_type = SHORT;        required_count = 1;  break; // JPEGRestartInterval
                        case 0x0205: required_type = SHORT;                             break; // JPEGLosslessPredictors
                        case 0x0206: required_type = SHORT;                             break; // JPEGPointTransforms
                        case 0x0207: required_type = LONG;                              break; // JPEGQTables
                        case 0x0208: required_type = LONG;                              break; // JPEGDCTables
                        case 0x0209: required_type = LONG;                              break; // JPEGACTables
                        case 0x0211: required_type = RATIONAL;     required_count = 3;  break; // YCbCrCoefficients
                        case 0x0212: required_type = SHORT;        required_count = 2;  break; // YCbCrSubSampling
                        case 0x0213: required_type = SHORT;        required_count = 1;  break; // YCbCrPositioning
                        case 0x0214: required_type = RATIONAL;     required_count = 6;  break; // ReferenceBlackWhite
                        case 0x022F: required_type = LONG;                              break; // StripRowCounts
                        case 0x02BC: required_type = BYTE;                              break; // XMP
                        case 0x800D: required_type = ASCII;                             break; // ImageID
                        case 0x87AC: required_type = SHORT | LONG; required_count = 2;  break; // ImageLayer
                        // private
                        case 0x8769: required_type = LONG | IFD; required_count = 1; ifd = 2; break; // Exif IFD
                        case 0x8825: required_type = LONG | IFD; required_count = 1; ifd = 2; break; // GPS IFD
                        case 0xA005: required_type = LONG | IFD; required_count = 1; ifd = 2; break; // Interoperability IFD
                        default:
                            if (tag_id >= 0x8000) {
                                break;
                            }
                            // unknown tag_id
                            return false;
                    }
                }

                // check types
                if (required_type && !(required_type & (1 << data_type))) {
                    // we check here for the corect type .. but
                    /*
                        TIFF readers should accept BYTE, SHORT, or LONG values for any unsigned
                        integer field. This allows a single procedure to retrieve any integer value, makes
                        reading more robust, and saves disk space in some situations.

                        ...
                    */
                    return false;
                }
                // check count
                if (required_count && data_count != required_count) {
                    return false;
                }

                uint32_t data_length = 0;
                switch (data_type) {
                    case 1: // BYTE
                    case 2: // ASCII
                        data_length = data_count * sizeof(uint8_t);
                        break;
                    case 3: // SHORT
                        data_length = data_count * sizeof(uint16_t);
                        break;
                    case 4:  // LONG
                    case 13: // IFD
                        data_length = data_count * sizeof(uint32_t);
                        break;
                    case 5: // RATIONAL
                        data_length = data_count * sizeof(uint64_t);
                        break;
                    case 6: // SBYTE
                        data_length = data_count * sizeof(int8_t);
                        break;
                    case 7: // UNDEFINE
                        data_length = data_count;
                        break;
                    case 8: // SSHORT
                        data_length = data_count * sizeof(int16_t);
                        break;
                    case 9: // SLONG
                        data_length = data_count * sizeof(int32_t);
                        break;
                    case 10: // SRATIONAL
                        data_length = data_count * sizeof(int64_t);
                        break;
                    case 11: // FLOAT
                        data_length = data_count * sizeof(float);
                        break;
                    case 12: // DOUBLE
                        data_length = data_count * sizeof(double);
                        break;
                }

                if (data_length > sizeof(uint32_t)) {
                    /*
                        it says
                        "Packing data within the DataOffset field is an optimization within the TIFF specification and is not required to be performed"
                        .. no idea how i should interpret the "not required to be performed"
                    */

                    length = std::max(length, data_offset + data_length);
                }

                if (ifd && !read_ifd<endian>(start, data_offset_before_offset, length, has_image_data, private_ifd || ifd == 2)) {
                    return false;
                }

                // image data
                if (!private_ifd) {
                    switch (tag_id) {
                        case 0x0111: // StripOffsets
                        case 0x0144: // TileOffsets
                        {
                            auto s = data_length > sizeof(uint32_t) ? subspan(start, data_offset) : data_offset_before_offset;
                            for (uint32_t j = 0; j < data_count; ++j) {
                                uint32_t o = data_type == 3 ? read<uint16_t, endian>(s) : read<uint32_t, endian>(s);
                                ImageDataOffsets.push_back(o);
                                s = subspan(s, data_type == 3 ? sizeof(uint16_t) : sizeof(uint32_t));
                            }
                        } break;

                        case 0x0117: // StripByteCount
                        case 0x0145: // TileByteCounts
                        {
                            auto s = data_length > sizeof(uint32_t) ? subspan(start, data_offset) : data_offset_before_offset;
                            for (uint32_t j = 0; j < data_count; ++j) {
                                uint32_t o = data_type == 3 ? read<uint16_t, endian>(s) : read<uint32_t, endian>(s);
                                ImageDataByteCounts.push_back(o);
                                s = subspan(s, data_type == 3 ? sizeof(uint16_t) : sizeof(uint32_t));
                            }
                        } break;
                    }
                }
            }

            // Each IFD defines a subfile
            if (!ImageDataOffsets.empty() || !ImageDataByteCounts.empty()) {
                if (ImageDataOffsets.size() != ImageDataByteCounts.size()) {
                    return false;
                }
                // update the max length
                for (size_t i = 0; i < ImageDataOffsets.size(); ++i) {
                    length = std::max(length, ImageDataOffsets[i] + ImageDataByteCounts[i]);
                }
                has_image_data = true;
            }
        }

        return found_end;
    }

    template<std::endian endian>
    std::span<const uint8_t> read_tif(std::span<const uint8_t> data) {
        const auto start = data;

        skip<decltype(SIGNATURE_BIG)>(data);

        uint32_t length = 0;
        bool has_image_data = false;
        if (!read_ifd<endian>(start, data, length, has_image_data)) {
            return {};
        }

        if (!has_image_data) {
            return {};
        }

        if (length > start.length) {
            // larger than allowed (but found end)
            return {};
        }

        // found something
        data = subspan(start, 0, length);
        return data;
    }
}

std::span<const uint8_t> read_tif(std::span<const uint8_t> data) {
    if (peek<decltype(SIGNATURE_BIG)>(data) == SIGNATURE_BIG) [[unlikely]] {
        return read_tif<std::endian::big>(data);
    } else if (peek<decltype(SIGNATURE_LITTLE)>(data) == SIGNATURE_LITTLE) [[unlikely]] {
        return read_tif<std::endian::little>(data);
    }
    return {};
}

