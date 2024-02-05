#ifndef H_READ_JPEG
#define H_READ_JPEG

#include <span>
#include <cstdint>

std::span<const uint8_t> read_jpg(std::span<const uint8_t> data);

#endif