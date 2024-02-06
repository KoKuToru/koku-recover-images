#ifndef H_READ_GIF
#define H_READ_GIF

#include <span>
#include <cstdint>

constexpr uint8_t FIRST_BYTE_GIF = 0x47;
std::span<const uint8_t> read_gif(std::span<const uint8_t> data);

#endif