#ifndef H_READ_GIF
#define H_READ_GIF

#include <span>
#include <cstdint>

std::span<const uint8_t> read_gif(std::span<const uint8_t> data);

#endif