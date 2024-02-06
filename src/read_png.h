#ifndef H_READ_PNG
#define H_READ_PNG

#include <span>
#include <cstdint>

constexpr uint8_t FIRST_BYTE_PNG = 0x89;
std::span<const uint8_t> read_png(std::span<const uint8_t> data);

#endif