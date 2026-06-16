#pragma once

#include "bitvector.h"

template <typename T>
inline std::string to_bitstring(const ch::internal::bitvector<T> &bv) {
    std::string s;
    for (size_t i = 0; i < bv.size(); ++i)
        s += bv[i] ? '1' : '0';
    // Reverse to show LSB on the right, MSB on the left (conventional
    // representation) Optional: std::reverse(s.begin(), s.end()); Or read
    // backwards for LSB-first indexing:
    std::string reversed_s;
    for (int i = s.length() - 1; i >= 0; --i) {
        reversed_s += s[i];
    }
    return reversed_s; // Now MSB is left, LSB is right in string
}