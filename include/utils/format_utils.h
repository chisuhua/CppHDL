#ifndef FORMAT_UTILS_H
#define FORMAT_UTILS_H
#include <bitset>
#include <cstdint>
#include <string>

template <typename T> std::string to_binary_string(T value, size_t width) {
    std::bitset<64> bs(static_cast<uint64_t>(value));
    std::string result = bs.to_string();
    // Return only the requested width bits
    return result.substr(64 - width, width);
}

#endif
