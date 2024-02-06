#ifndef H_READ_JPEG
#define H_READ_JPEG

#include <span>
#include <cstdint>

constexpr uint8_t FIRST_BYTE_JPG = 0xFF;
std::span<const uint8_t> read_jpg(std::span<const uint8_t> data);

#endif