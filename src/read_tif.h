#ifndef H_READ_TIFF
#define H_READ_TIFF

#include <span>
#include <cstdint>

constexpr uint8_t FIRST_BYTE_TIF_LITTLE = 0x49;
constexpr uint8_t FIRST_BYTE_TIF_BIG    = 0x4D;
std::span<const uint8_t> read_tif(std::span<const uint8_t> data);

#endif