#pragma once

#include "utils.h"
#include "bv_copy.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace ch {
namespace internal {

template <typename U, typename T, CH_REQUIRES(std::is_integral_v<U>)>
void bv_assign_scalar(T *dst, U value) {
    dst[0] = value;
}

template <typename U, typename T,
          CH_REQUIRES(std::is_integral_v<U> &&std::is_unsigned_v<U>)>
void bv_assign_vector(T *dst, uint32_t size, U value) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    if constexpr (bitwidth_v<U> <= WORD_SIZE) {
        dst[0] = value;
        std::fill_n(dst + 1, num_words - 1, 0);
    } else {
        if (0 == value) {
            std::fill_n(dst, num_words, 0);
            return;
        }
        auto len = std::min<uint32_t>(num_words, bitwidth_v<U> / WORD_SIZE);
        std::copy_n(reinterpret_cast<const T *>(&value), len, dst);
        std::fill_n(dst + len, num_words - len, 0);
    }
}

template <typename U, typename T,
          CH_REQUIRES(std::is_integral_v<U> &&std::is_signed_v<U>)>
void bv_assign_vector(T *dst, uint32_t size, U value) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    T ext_value = ((value >= 0) ? 0 : WORD_MAX);
    if constexpr (bitwidth_v<U> <= WORD_SIZE) {
        dst[0] = value;
        std::fill_n(dst + 1, num_words - 1, ext_value);
    } else {
        auto len = std::min<uint32_t>(num_words, bitwidth_v<U> / WORD_SIZE);
        std::copy_n(reinterpret_cast<const T *>(&value), len, dst);
        std::fill_n(dst + len, num_words - len, ext_value);
    }
}

template <typename U, typename T,
          CH_REQUIRES(std::is_integral_v<U> &&std::is_unsigned_v<U>)>
void bv_assign(T *dst, uint32_t size, U value) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    if constexpr (std::numeric_limits<U>::digits > 1) {
        CH_CHECK(log2ceil(value + 1) <= size, "value out of range");
    }
    if (size <= WORD_SIZE) {
        bv_assign_scalar(dst, value);
    } else {
        bv_assign_vector(dst, size, value);
    }
}

template <typename U, typename T,
          CH_REQUIRES(std::is_integral_v<U> &&std::is_signed_v<U>)>
void bv_assign(T *dst, uint32_t size, U value) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    static_assert(std::numeric_limits<U>::digits > 1, "inavlid size");

    if (value >= 0) {
        auto u_value = std::make_unsigned_t<U>(value);
        if constexpr (std::numeric_limits<U>::digits > 1) {
            CH_CHECK(log2ceil(u_value + 1) <= size, "value out of range");
        }
        if (size <= WORD_SIZE) {
            bv_assign_scalar(dst, u_value);
        } else {
            bv_assign_vector(dst, size, u_value);
        }
    } else {
        CH_CHECK(1 + log2ceil(~value + 1) <= size, "value out of range");
        if (size <= WORD_SIZE) {
            bv_assign_scalar(dst, value);
        } else {
            bv_assign_vector(dst, size, value);
        }
        bv_clear_extra_bits(dst, size);
    }
}

template <typename U, std::size_t N, typename T>
void bv_assign(T *dst, uint32_t size, const std::array<U, N> &value) {
    static_assert(std::is_integral_v<U>, "invalid array type");
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    for (std::size_t i = 0; i < N; ++i) {
        auto j = N - 1 - i;
        auto length = bitwidth_v<U>;
        int rem = ((i + 1) * bitwidth_v<U>)-size;
        if (rem > 0) {
            length = bitwidth_v<U> - rem;
            CH_CHECK(length < bitwidth_v<U>, "buffer out of range");
            CH_CHECK(0 == (value[j] >> length), "value out of range");
        }
        bv_copy<U>(reinterpret_cast<U *>(dst), i * bitwidth_v<U>, value.data(),
                   j * bitwidth_v<U>, length);
    }
    auto src_num_words = ceildiv<uint32_t>(N * bitwidth_v<U>, WORD_SIZE);
    auto num_words = ceildiv(size, WORD_SIZE);
    std::fill_n(dst + src_num_words, num_words - src_num_words, 0);
}

template <typename U, typename T>
void bv_assign(T *dst, uint32_t size, const std::vector<U> &value) {
    static_assert(std::is_integral_v<U>, "invalid array type");
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    for (std::size_t i = 0, n = value.size(); i < n; ++i) {
        auto j = n - 1 - i;
        auto length = bitwidth_v<U>;
        int rem = ((i + 1) * bitwidth_v<U>)-size;
        if (rem > 0) {
            length = bitwidth_v<U> - rem;
            CH_CHECK(length < bitwidth_v<U>, "buffer out of range");
            CH_CHECK(0 == (value[j] >> length), "value out of range");
        }
        bv_copy<U>(reinterpret_cast<U *>(dst), i * bitwidth_v<U>, value.data(),
                   j * bitwidth_v<U>, length);
    }
    auto src_num_words =
        ceildiv<uint32_t>(value.size() * bitwidth_v<U>, WORD_SIZE);
    auto num_words = ceildiv(size, WORD_SIZE);
    std::fill_n(dst + src_num_words, num_words - src_num_words, 0);
}

///////////////////////////////////////////////////////////////////////////////

inline int string_literal_base(const char *value, int len) {
    int base = 0;
    switch (value[len - 1]) {
    case 'b':
        base = 1;
        break;
    case 'o':
        base = 3;
        break;
    case 'h':
        base = 4;
        break;
    default:
        throw std::invalid_argument("invalid binary encoding type.");
    }
    return base;
}

inline int string_literal_size(const char *value, int len, int base) {
    int size = 0;
    for (const char *cur = value, *end = value + len; cur < end;) {
        char chr = *cur++;
        if (chr == '\'' || chr == '_') // skip number separator
            continue;
        if (0 == size) {
            // calculate exact bit coverage for the first non zero character
            int v = char2int(chr, 1 << base);
            if (v) {
                size += log2ceil(v + 1);
            }
        } else {
            size += base;
        }
    }
    return size;
}

template <typename T>
void bv_assign(T *dst, uint32_t size, const char *value, uint32_t length,
               int base) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    const char *buf = value;
    if (4 == base && value[0] == '0' && length > 1 && value[1] == 'x') {
        buf += 2; // remove hex prefix
        length -= 2;
    }

    auto str_size = string_literal_size(buf, static_cast<int>(length), base);
    CH_CHECK(str_size >= 0, "invalid string size");
    CH_CHECK(static_cast<int32_t>(size) >= str_size, "value out of range");

    // clear remaining words
    uint32_t num_words = ceildiv(size, WORD_SIZE);
    uint32_t src_num_words = ceildiv(str_size, WORD_SIZE);
    if (src_num_words < num_words) {
        std::fill_n(dst + src_num_words, num_words - src_num_words, 0x0);
    }

    // write the value
    T w(0);
    T *d = dst;
    for (int32_t i = static_cast<int32_t>(length) - 1, j = 0; i >= 0; --i) {
        char chr = buf[i];
        if (chr == '\'' || chr == '_') // skip number separator
            continue;
        T v = char2int(chr, 1 << base);
        w |= v << j;
        j += base;
        if (j >= int32_t(WORD_SIZE)) {
            *d++ = w;
            j -= WORD_SIZE;
            w = v >> (base - j);
        }
    }

    if (w) {
        *d = w;
    }
}

template <typename T>
void bv_assign(T *dst, uint32_t size, const std::string &value) {
    int len = static_cast<int>(value.length());
    CH_CHECK(len >= 3, "invalid string size");

    const char *buf = value.c_str();
    int base = string_literal_base(buf, len);
    len -= 2; // remove base postfix
    bv_assign(dst, size, buf, len, base);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> int bv_lsb_scalar(const T *in) {
    auto w = in[0];
    if (w) {
        return count_trailing_zeros<T>(w);
    }
    return -1;
}

template <typename T> int bv_lsb_vector(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        auto w = in[i];
        if (w) {
            int z = count_trailing_zeros<T>(w);
            return z + i * WORD_SIZE;
        }
    }
    return -1;
}

template <typename T> int bv_lsb(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_lsb_scalar(in);
    } else {
        return bv_lsb_vector(in, size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> int bv_msb_scalar(const T *in) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    auto w = in[0];
    if (w) {
        int z = count_leading_zeros<T>(w);
        return (WORD_SIZE - 1 - z);
    }
    return -1;
}

template <typename T> int bv_msb_vector(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    for (int32_t i = static_cast<int32_t>(num_words) - 1; i >= 0; --i) {
        auto w = in[i];
        if (w) {
            int z = count_leading_zeros<T>(w);
            return (WORD_SIZE - 1 - z) + i * WORD_SIZE;
        }
    }
    return -1;
}

template <typename T> int bv_msb(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_msb_scalar(in);
    } else {
        return bv_msb_vector(in, size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename U, typename T> U bv_cast(const T *in, uint32_t size) {
    static_assert(std::is_integral_v<T>, "invalid type");
    if constexpr (bitwidth_v<U> <= bitwidth_v<T>) {
        CH_UNUSED(size);
        return bit_cast<U>(in[0]);
    } else {
        U ret(0);
        memcpy(&ret, in, ceildiv(size, 8));
        return ret;
    }
}

} // namespace internal
} // namespace ch