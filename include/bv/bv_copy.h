#pragma once

#include "utils.h"
#include <algorithm>
#include <cstring>

namespace ch {
namespace internal {

template <typename T> void bv_clear_extra_bits(T *dst, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();

    uint32_t rem = size % WORD_SIZE;
    if (rem) {
        dst[size / WORD_SIZE] &= ~(WORD_MAX << rem);
    }
}

template <typename T> void bv_init(T *dst, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    std::fill_n(dst, num_words, T{0});
    bv_clear_extra_bits(dst, size);
}

template <typename T> void bv_reset(T *dst, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    std::fill_n(dst, num_words, 0);
}

template <typename T> bool bv_get(const T *bv, uint32_t offset) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    auto idx = offset / WORD_SIZE;
    auto lsb = offset % WORD_SIZE;
    return (bv[idx] >> lsb) & 0x1;
}

template <typename T> void bv_set(T *bv, uint32_t offset, bool value) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    auto idx = offset / WORD_SIZE;
    auto lsb = offset % WORD_SIZE;
    auto mask = T(1) << lsb;
    if (value) {
        bv[idx] |= mask;
    } else {
        bv[idx] &= ~mask;
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool bv_is_zero_scalar(const T *in) {
    return (0 == in[0]);
}

template <typename T> bool bv_is_zero_vector(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        if (in[i])
            return false;
    }
    return true;
}

template <typename T> bool bv_is_zero(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_is_zero_scalar(in);
    } else {
        return bv_is_zero_vector(in, size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool bv_is_one_scalar(const T *in) {
    return (1 == in[0]);
}

template <typename T> bool bv_is_one_vector(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    for (uint32_t i = num_words - 1; i > 0; --i) {
        if (in[i])
            return false;
    }
    return (1 == in[0]);
}

template <typename T> bool bv_is_one(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_is_one_scalar(in);
    } else {
        return bv_is_one_vector(in, size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool bv_is_ones_scalar(const T *in, uint32_t size) {
    auto max = std::numeric_limits<uint64_t>::max() >> (64 - size);
    return (max == in[0]);
}

template <typename T> bool bv_is_ones_vector(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words - 1; ++i) {
        if (in[i] != std::numeric_limits<T>::max())
            return false;
    }

    auto rem = size % WORD_SIZE;
    auto max = std::numeric_limits<uint64_t>::max() >> (64 - rem);
    return (max == in[num_words - 1]);
}

template <typename T> bool bv_is_ones(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_is_ones_scalar(in, size);
    } else {
        return bv_is_ones_vector(in, size);
    }
}

template <typename T>
void bv_copy_scalar(T *w_dst, uint32_t w_dst_lsb, const T *w_src,
                    uint32_t w_src_lsb, uint32_t length) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();
    assert((w_dst_lsb + length) <= WORD_SIZE);
    assert((w_src_lsb + length) <= WORD_SIZE);

    T src_block = (w_src[0] >> w_src_lsb) << w_dst_lsb;
    T mask = (WORD_MAX >> (WORD_SIZE - length)) << w_dst_lsb;
    w_dst[0] = blend<T>(mask, w_dst[0], src_block);
}

template <typename T>
void bv_copy_vector_small(T *w_dst, uint32_t w_dst_lsb, const T *w_src,
                          uint32_t w_src_lsb, uint32_t length) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();
    assert(length <= WORD_SIZE);
    assert(w_dst_lsb < WORD_SIZE);
    assert(w_src_lsb < WORD_SIZE);

    T src_block0 = w_src[0] >> w_src_lsb;
    if (w_src_lsb + length > WORD_SIZE) {
        src_block0 |= (w_src[1] << (WORD_SIZE - w_src_lsb));
    }
    T mask0 = (WORD_MAX >> (WORD_SIZE - length)) << w_dst_lsb;
    w_dst[0] = blend<T>(mask0, w_dst[0], (src_block0 << w_dst_lsb));
    if (w_dst_lsb > (WORD_SIZE - length)) {
        T src_block1 = src_block0 >> (WORD_SIZE - w_dst_lsb);
        T mask1 = T(~mask0) >> (WORD_SIZE - length);
        w_dst[1] = blend<T>(mask1, w_dst[1], src_block1);
    }
}

template <typename T>
void bv_copy_vector_aligned(T *w_dst, const T *w_src, uint32_t length) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    static constexpr unsigned WORD_MASK = WORD_SIZE - 1;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();
    assert(length > WORD_SIZE);

    uint32_t dst_end = length - 1;
    uint32_t w_dst_end_idx = (dst_end / WORD_SIZE);
    uint32_t w_dst_msb = dst_end % WORD_SIZE;

    // update aligned blocks
    if (WORD_MASK == w_dst_msb) {
        std::copy_n(w_src, w_dst_end_idx + 1, w_dst);
    } else {
        std::copy_n(w_src, w_dst_end_idx, w_dst);
        // update remining block
        T src_block = w_src[w_dst_end_idx];
        T mask = (WORD_MAX << 1) << w_dst_msb;
        w_dst[w_dst_end_idx] = blend<T>(mask, src_block, w_dst[w_dst_end_idx]);
    }
}

template <typename T>
void bv_copy_vector_aligned_dst(T *w_dst, const T *w_src, uint32_t w_src_lsb,
                                uint32_t length) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();
    assert(length > WORD_SIZE);
    assert(w_src_lsb < WORD_SIZE);

    uint32_t src_end = w_src_lsb + length - 1;
    uint32_t w_src_end_idx = (src_end / WORD_SIZE);

    uint32_t dst_end = length - 1;
    uint32_t w_dst_end_idx = (dst_end / WORD_SIZE);
    uint32_t w_dst_msb = dst_end % WORD_SIZE;

    // update first block
    T src_block = *w_src++ >> w_src_lsb;
    src_block |= w_src[0] << (WORD_SIZE - w_src_lsb);
    w_dst[0] = src_block;

    // update intermediate blocks
    auto w_dst_end = (w_dst++) + w_dst_end_idx;
    while (w_dst < w_dst_end) {
        T tmp = *w_src++ >> w_src_lsb;
        *w_dst++ = tmp | (w_src[0] << (WORD_SIZE - w_src_lsb));
    }

    // update remining blocks
    src_block = w_src[0] >> w_src_lsb;
    if (w_src_end_idx > w_dst_end_idx) {
        src_block |= (w_src[1] << (WORD_SIZE - w_src_lsb));
    }
    T mask = (WORD_MAX << 1) << w_dst_msb;
    w_dst[0] = blend<T>(mask, src_block, w_dst[0]);
}

template <typename T>
void bv_copy_vector_aligned_src(T *w_dst, uint32_t w_dst_lsb, const T *w_src,
                                uint32_t length) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    static constexpr unsigned WORD_MASK = WORD_SIZE - 1;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();
    assert(length > WORD_SIZE);
    assert(w_dst_lsb < WORD_SIZE);

    uint32_t src_end = length - 1;
    uint32_t w_src_end_idx = (src_end / WORD_SIZE);

    uint32_t dst_end = w_dst_lsb + length - 1;
    uint32_t w_dst_end_idx = (dst_end / WORD_SIZE);
    uint32_t w_dst_msb = dst_end % WORD_SIZE;

    // update first block
    T src_block = *w_src++;
    T mask = WORD_MAX << w_dst_lsb;
    w_dst[0] = blend<T>(mask, w_dst[0], (src_block << w_dst_lsb));

    // update intermediate blocks
    auto w_dst_end = (w_dst++) + w_src_end_idx;
    while (w_dst < w_dst_end) {
        auto tmp = *w_src++;
        *w_dst++ =
            (tmp << w_dst_lsb) | ((src_block >> 1) >> (WORD_MASK - w_dst_lsb));
        src_block = tmp;
    }

    // update remining blocks
    src_block =
        (w_src[0] << w_dst_lsb) | ((src_block >> 1) >> (WORD_MASK - w_dst_lsb));
    if (w_src_end_idx < w_dst_end_idx) {
        *w_dst++ = src_block;
        src_block = (w_src[0] >> 1) >> (WORD_MASK - w_dst_lsb);
    }
    mask = (WORD_MAX << 1) << w_dst_msb;
    w_dst[0] = blend<T>(mask, src_block, w_dst[0]);
}

template <typename T>
void bv_copy_vector_unaligned(T *w_dst, uint32_t w_dst_lsb, const T *w_src,
                              uint32_t w_src_lsb, uint32_t length) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    static constexpr unsigned WORD_MASK = WORD_SIZE - 1;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();
    assert(length > WORD_SIZE);
    assert(w_dst_lsb && w_dst_lsb < WORD_SIZE);
    assert(w_src_lsb && w_src_lsb < WORD_SIZE);

    uint32_t src_end = w_src_lsb + length - 1;
    uint32_t w_src_end_idx = (src_end / WORD_SIZE);

    uint32_t dst_end = w_dst_lsb + length - 1;
    uint32_t w_dst_end_idx = (dst_end / WORD_SIZE);
    uint32_t w_dst_msb = dst_end % WORD_SIZE;

    // update first block
    T src_block = *w_src++ >> w_src_lsb;
    src_block |= (w_src[0] << (WORD_SIZE - w_src_lsb));
    T mask = WORD_MAX << w_dst_lsb;
    w_dst[0] = blend<T>(mask, w_dst[0], (src_block << w_dst_lsb));

    // update intermediate blocks
    auto w_dst_end = (w_dst++) + std::min(w_src_end_idx, w_dst_end_idx);
    while (w_dst < w_dst_end) {
        T tmp = *w_src++ >> w_src_lsb;
        tmp |= (w_src[0] << (WORD_SIZE - w_src_lsb));
        *w_dst++ =
            (tmp << w_dst_lsb) | ((src_block >> 1) >> (WORD_MASK - w_dst_lsb));
        src_block = tmp;
    }

    // update remining blocks
    T tmp = w_src[0] >> w_src_lsb;
    if (w_src_end_idx < w_dst_end_idx) {
        src_block =
            (tmp << w_dst_lsb) | ((src_block >> 1) >> (WORD_MASK - w_dst_lsb));
        *w_dst++ = src_block;
        src_block = (tmp >> 1) >> (WORD_MASK - w_dst_lsb);
    } else if (w_src_end_idx == w_dst_end_idx) {
        src_block =
            (tmp << w_dst_lsb) | ((src_block >> 1) >> (WORD_MASK - w_dst_lsb));
    } else {
        src_block = (tmp | (w_src[1] << (WORD_SIZE - w_src_lsb))) << w_dst_lsb;
    }
    mask = (WORD_MAX << 1) << w_dst_msb;
    w_dst[0] = blend<T>(mask, src_block, w_dst[0]);
}

template <typename T>
void bv_copy_vector(T *w_dst, uint32_t w_dst_lsb, const T *w_src,
                    uint32_t w_src_lsb, uint32_t length) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    assert(w_dst_lsb < WORD_SIZE);
    assert(w_src_lsb < WORD_SIZE);

    if (length <= WORD_SIZE) {
        bv_copy_vector_small(w_dst, w_dst_lsb, w_src, w_src_lsb, length);
    } else if (0 == ((w_dst_lsb % 8) | (w_src_lsb % 8))) {
        auto b_dst = reinterpret_cast<uint8_t *>(w_dst) + (w_dst_lsb / 8);
        auto b_src = reinterpret_cast<const uint8_t *>(w_src) + (w_src_lsb / 8);
        bv_copy_vector_aligned(b_dst, b_src, length);
    } else if (0 == w_dst_lsb) {
        bv_copy_vector_aligned_dst(w_dst, w_src, w_src_lsb, length);
    } else if (0 == w_src_lsb) {
        bv_copy_vector_aligned_src(w_dst, w_dst_lsb, w_src, length);
    } else {
        bv_copy_vector_unaligned(w_dst, w_dst_lsb, w_src, w_src_lsb, length);
    }
}

template <typename T>
void bv_copy(T *w_dst, uint32_t dst_offset, const T *w_src, uint32_t src_offset,
             uint32_t length) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;

    if ((dst_offset + length) <= WORD_SIZE &&
        (src_offset + length) <= WORD_SIZE) {
        bv_copy_scalar(w_dst, dst_offset, w_src, src_offset, length);
    } else {
        auto w_dst_idx = dst_offset / WORD_SIZE;
        auto w_dst_lsb = dst_offset % WORD_SIZE;
        auto w_src_idx = src_offset / WORD_SIZE;
        auto w_src_lsb = src_offset % WORD_SIZE;
        bv_copy_vector(w_dst + w_dst_idx, w_dst_lsb, w_src + w_src_idx,
                       w_src_lsb, length);
    }
}

} // namespace internal
} // namespace ch