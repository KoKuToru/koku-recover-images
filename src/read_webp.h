#ifndef H_READ_WEBP
#define H_READ_WEBP

#include <span>
#include <cstdint>

constexpr uint8_t FIRST_BYTE_WEBP = 0x52;
std::span<const uint8_t> read_webp(std::span<const uint8_t> data);

#endif