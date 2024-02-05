#ifndef H_READ_PNG
#define H_READ_PNG

#include <span>
#include <cstdint>

std::span<const uint8_t> read_png(std::span<const uint8_t> data);

#endif