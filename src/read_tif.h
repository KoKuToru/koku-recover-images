#ifndef H_READ_TIFF
#define H_READ_TIFF

#include <span>
#include <cstdint>

std::span<const uint8_t> read_tif(std::span<const uint8_t> data);

#endif