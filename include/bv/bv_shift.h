#pragma once

#include "utils.h"
#include "bv_copy.h"
#include "bv_pad_slice.h"
#include <algorithm>
#include <cstring>

namespace ch {
namespace internal {

template <bool is_signed, typename T>
void bv_shl_scalar(T *out, uint32_t out_size, const T *in, uint32_t in_size,
                   uint32_t dist) {
    T value;
    if constexpr (is_signed) {
        value = sign_ext(in[0], in_size);
    } else {
        // 对 unsigned，只保留低 in_size 位，清除高位垃圾
        value = in[0] & low_bit_mask<T>(in_size);
    }
    out[0] = (dist < bitwidth_v<T>) ? (value << dist) : 0;
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_shl_vector(T *out, uint32_t out_size, const T *in, uint32_t in_size,
                   uint32_t dist) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    auto in_num_words = ceildiv<int32_t>(in_size, WORD_SIZE);
    auto out_num_words = ceildiv<int32_t>(out_size, WORD_SIZE);
    int32_t shift_words = dist / WORD_SIZE;
    uint32_t shift_bits = dist % WORD_SIZE;

    std::fill_n(out, out_num_words, T(0));

    if (shift_words >= out_num_words) {
        // Result is all zeros
        bv_clear_extra_bits(out, out_size);
        return;
    }

    if (shift_bits == 0) {
        for (int32_t i = out_num_words - 1; i >= shift_words; --i) {
            out[i] =
                (i - shift_words < in_num_words) ? in[i - shift_words] : T(0);
        }
    } else {
        for (int32_t i = out_num_words - 1; i > shift_words; --i) {
            out[i] =
                (i - shift_words < in_num_words) ? in[i - shift_words] : T(0);
            if (i - shift_words - 1 >= 0) {
                out[i] |= in[i - shift_words - 1] << (WORD_SIZE - shift_bits);
            }
        }
        if (shift_words < out_num_words) {
            out[shift_words] =
                (0 < in_num_words) ? (in[0] << shift_bits) : T(0);
        }
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_shl(T *out, uint32_t out_size, const T *in, uint32_t in_size,
            uint32_t dist) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    if (out_size <= WORD_SIZE && in_size <= WORD_SIZE) {
        bv_shl_scalar<is_signed>(out, out_size, in, in_size, dist);
    } else {
        bv_shl_vector<is_signed>(out, out_size, in, in_size, dist);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T>
void bv_shr_scalar(T *out, uint32_t out_size, const T *in, uint32_t in_size,
                   uint32_t dist) {
    if constexpr (is_signed) {
        auto value = sign_ext<T>(in[0], in_size);
        out[0] = (dist < bitwidth_v<T>) ? (value >> dist)
                                        : (value >> (bitwidth_v<T> - 1));
        bv_clear_extra_bits(out, out_size);
    } else {
        CH_UNUSED(out_size, in_size);
        out[0] = (dist < bitwidth_v<T>) ? (in[0] >> dist) : 0;
    }
}

template <bool is_signed, typename T>
void bv_shr_vector(T *out, uint32_t out_size, const T *in, uint32_t in_size,
                   uint32_t dist) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    auto in_num_words = ceildiv<int32_t>(in_size, WORD_SIZE);
    auto out_num_words = ceildiv<int32_t>(out_size, WORD_SIZE);

    int32_t shift_words = ceildiv<int32_t>(dist, WORD_SIZE);
    int32_t shift_bits = dist % WORD_SIZE;

    auto r = std::min(in_num_words - shift_words, out_num_words);
    T ext = 0;
    if constexpr (is_signed) {
        ext = bv_is_neg_vector(in, in_size) ? std::numeric_limits<T>::max() : 0;
    }

    if (shift_bits) {
        T prev = (shift_words <= in_num_words)
                     ? (in[shift_words - 1] >> shift_bits)
                     : ext;
        int32_t i = 0;
        if constexpr (is_signed) {
            if (r > 0) {
                for (; i < r - 1; ++i) {
                    auto curr = in[i + shift_words];
                    out[i] = (curr << (WORD_SIZE - shift_bits)) | prev;
                    prev = curr >> shift_bits;
                }
                auto src = in[i + shift_words];
                auto rem = in_size % WORD_SIZE;
                auto curr = rem ? sign_ext<T>(src, rem) : src;
                out[i++] = (curr << (WORD_SIZE - shift_bits)) | prev;
                prev = curr >> shift_bits;
            }
        } else {
            for (; i < r; ++i) {
                auto curr = in[i + shift_words];
                out[i] = (curr << (WORD_SIZE - shift_bits)) | prev;
                prev = curr >> shift_bits;
            }
        }
        for (; i < out_num_words; ++i) {
            out[i] = (ext << (WORD_SIZE - shift_bits)) | prev;
            prev = ext;
        }
    } else {
        int32_t i = 0;
        if constexpr (is_signed) {
            if (r > 0) {
                for (; i < r - 1; ++i) {
                    out[i] = in[i + shift_words];
                }
                auto src = in[i + shift_words];
                auto rem = in_size % WORD_SIZE;
                auto curr = rem ? sign_ext(src, rem) : src;
                out[i++] = curr;
            }
        } else {
            for (; i < r; ++i) {
                out[i] = in[i + shift_words];
            }
        }
        for (; i < out_num_words; ++i) {
            out[i] = ext;
        }
    }

    if constexpr (is_signed) {
        bv_clear_extra_bits(out, out_size);
    }
}

template <bool is_signed, typename T>
void bv_shr(T *out, uint32_t out_size, const T *in, uint32_t in_size,
            uint32_t dist) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && in_size <= WORD_SIZE) {
        bv_shr_scalar<is_signed>(out, out_size, in, in_size, dist);
    } else {
        bv_shr_vector<is_signed>(out, out_size, in, in_size, dist);
    }
}

} // namespace internal
} // namespace ch