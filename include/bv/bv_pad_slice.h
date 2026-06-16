#pragma once

#include "utils.h"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace ch {
namespace internal {

template <typename T> bool bv_is_neg_scalar(const T *in, uint32_t size) {
    auto mask = T(1) << (size - 1);
    return (in[0] & mask) != 0;
}

template <typename T> bool bv_is_neg_vector(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t index = (size - 1);
    uint32_t widx = index / WORD_SIZE;
    uint32_t wbit = index % WORD_SIZE;
    auto mask = T(1) << wbit;
    return (in[widx] & mask) != 0;
}

template <typename T> bool bv_is_neg(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_is_neg_scalar(in, size);
    } else {
        return bv_is_neg_vector(in, size);
    }
}

template <typename T>
bool bv_is_neg(const T *data, uint32_t bit_offset, uint32_t length) {
    if (length == 0)
        return false;
    uint32_t msb_idx = bit_offset + length - 1;
    uint32_t word_idx = msb_idx / bitwidth_v<T>;
    uint32_t bit_in_word = msb_idx % bitwidth_v<T>;
    return (data[word_idx] >> bit_in_word) & 1U;
}

template <typename T>
T extract_bits(const T *data, uint32_t bit_offset, uint32_t length) {
    if (length == 0)
        return T{0};
    assert(length <= bitwidth_v<T>);

    uint32_t word0_idx = bit_offset / bitwidth_v<T>;
    uint32_t bit_in_word0 = bit_offset % bitwidth_v<T>;

    T word0 = data[word0_idx];
    if (bit_in_word0 + length <= bitwidth_v<T>) {
        T mask = (length == bitwidth_v<T>) ? T(-1) : (T(1) << length) - 1;
        return (word0 >> bit_in_word0) & mask;
    } else {
        T word1 = data[word0_idx + 1];
        uint32_t low_bits = bitwidth_v<T> - bit_in_word0;
        T low_part = word0 >> bit_in_word0;
        T high_part = word1 << low_bits;
        T combined = low_part | high_part;
        T mask = (length == bitwidth_v<T>) ? T(-1) : (T(1) << length) - 1;
        return combined & mask;
    }
}

template <bool is_signed, typename T>
void bv_pad_scalar(T *out, uint32_t out_size, const T *in, uint32_t in_size) {
    if constexpr (is_signed) {
        out[0] = sign_ext(in[0], in_size);
        bv_clear_extra_bits(out, out_size);
    } else {
        CH_UNUSED(out_size, in_size);
        out[0] = in[0];
    }
}

template <bool is_signed, typename T>
void bv_pad_vector(T *out, uint32_t out_size, const T *in, uint32_t in_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t in_num_words = ceildiv(in_size, WORD_SIZE);
    uint32_t out_num_words = ceildiv(out_size, WORD_SIZE);

    if constexpr (is_signed) {
        static constexpr T WORD_MAX = std::numeric_limits<T>::max();
        auto is_neg = bv_is_neg_vector(in, in_size);
        if (is_neg) {
            uint32_t msb = in_size - 1;
            uint32_t msb_blk_idx = msb / WORD_SIZE;
            uint32_t msb_blk_rem = msb % WORD_SIZE;
            auto msb_blk = in[msb_blk_idx];
            std::copy_n(in, in_num_words, out);
            out[msb_blk_idx] = msb_blk | (WORD_MAX << msb_blk_rem);
            std::fill(out + in_num_words, out + out_num_words, WORD_MAX);
            bv_clear_extra_bits(out, out_size);
        } else {
            std::copy_n(in, in_num_words, out);
            std::fill(out + in_num_words, out + out_num_words, 0);
        }
    } else {
        std::copy_n(in, in_num_words, out);
        std::fill(out + in_num_words, out + out_num_words, 0);
    }
}

template <bool is_signed, typename T>
void bv_pad(T *out, uint32_t out_size, const T *in, uint32_t in_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    assert(out_size >= in_size);

    if (out_size <= WORD_SIZE) {
        bv_pad_scalar<is_signed>(out, out_size, in, in_size);
    } else {
        bv_pad_vector<is_signed>(out, out_size, in, in_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void bv_slice_scalar(T *w_dst, uint32_t dst_size, const T *w_src,
                     uint32_t w_src_lsb) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    assert((w_src_lsb + dst_size) <= WORD_SIZE);

    T src_block = w_src[0] >> w_src_lsb;
    uint32_t clamp = (WORD_SIZE - dst_size);
    w_dst[0] = T(src_block << clamp) >> clamp;
}

template <typename T>
void bv_slice_vector_small(T *w_dst, uint32_t dst_size, const T *w_src,
                           uint32_t w_src_lsb) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    assert(dst_size <= WORD_SIZE);
    assert(w_src_lsb < WORD_SIZE);

    T src_block = w_src[0] >> w_src_lsb;
    uint32_t clamp = (WORD_SIZE - dst_size);
    if (w_src_lsb > clamp) {
        src_block |= w_src[1] << (WORD_SIZE - w_src_lsb);
    }
    w_dst[0] = T(src_block << clamp) >> clamp;
}

template <typename T>
void bv_slice_vector_aligned(T *dst, uint32_t dst_size, const T *w_src) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    static constexpr unsigned WORD_MASK = WORD_SIZE - 1;
    assert(dst_size > WORD_SIZE);

    uint32_t dst_end = dst_size - 1;
    uint32_t w_dst_end_idx = (dst_end / WORD_SIZE);
    uint32_t w_dst_msb = dst_end % WORD_SIZE;

    // update aligned blocks
    if (WORD_MASK == w_dst_msb) {
        std::copy_n(w_src, w_dst_end_idx + 1, dst);
    } else {
        std::copy_n(w_src, w_dst_end_idx, dst);
        // update remining block
        uint32_t clamp = (WORD_MASK - w_dst_msb);
        dst[w_dst_end_idx] = T(w_src[w_dst_end_idx] << clamp) >> clamp;
    }
}

template <typename T>
void bv_slice_vector_unaligned(T *w_dst, uint32_t dst_size, const T *w_src,
                               uint32_t w_src_lsb) {
    static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
    static constexpr unsigned WORD_MASK = WORD_SIZE - 1;
    assert(dst_size > WORD_SIZE);
    assert(w_src_lsb && w_src_lsb < WORD_SIZE);

    uint32_t src_end = w_src_lsb + dst_size - 1;
    uint32_t w_src_end_idx = (src_end / WORD_SIZE);

    uint32_t dst_end = dst_size - 1;
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
        src_block |= w_src[1] << (WORD_SIZE - w_src_lsb);
    }
    uint32_t clamp = (WORD_MASK - w_dst_msb);
    w_dst[0] = T(src_block << clamp) >> clamp;
}

template <typename T>
void bv_slice_vector(T *w_dst, uint32_t dst_size, const T *w_src,
                     uint32_t w_src_lsb) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    assert(w_src_lsb < WORD_SIZE);

    if (dst_size <= WORD_SIZE) {
        bv_slice_vector_small(w_dst, dst_size, w_src, w_src_lsb);
    } else if (0 == (w_src_lsb % 8)) {
        auto b_dst = reinterpret_cast<uint8_t *>(w_dst);
        auto b_src = reinterpret_cast<const uint8_t *>(w_src) + (w_src_lsb / 8);
        bv_slice_vector_aligned(b_dst, dst_size, b_src);
    } else {
        bv_slice_vector_unaligned(w_dst, dst_size, w_src, w_src_lsb);
    }
}

template <typename T>
void bv_slice(T *w_dst, uint32_t dst_size, const T *w_src,
              uint32_t src_offset) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (src_offset + dst_size <= WORD_SIZE) {
        bv_slice_scalar(w_dst, dst_size, w_src, src_offset);
    } else {
        auto w_src_idx = src_offset / WORD_SIZE;
        auto w_src_lsb = src_offset % WORD_SIZE;
        bv_slice_vector(w_dst, dst_size, w_src + w_src_idx, w_src_lsb);
    }
}

// Preserved for reference (original bv_cmp implementation, kept commented-out per zero-debt policy):
/*
template <typename T>
int bv_cmp_scalar(const T* lhs,
                  uint32_t lhs_lsb,
                  const T* rhs,
                  uint32_t rhs_lsb,
                  uint32_t length) {
  static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
  assert((lhs_lsb + length) <= WORD_SIZE);
  assert((rhs_lsb + length) <= WORD_SIZE);

  uint32_t clamp = WORD_SIZE - length;
  T lhs_block = T((lhs[0] >> lhs_lsb) << clamp) >> clamp;
  T rhs_block = T((rhs[0] >> rhs_lsb) << clamp) >> clamp;
  return lhs_block - rhs_block;
}

template <typename T>
int bv_cmp_vector_small(const T* lhs, uint32_t lhs_lsb,
                        const T* rhs, uint32_t rhs_lsb,
                        uint32_t length) {
  static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
  assert(length <= WORD_SIZE);
  assert(lhs_lsb < WORD_SIZE);
  assert(rhs_lsb < WORD_SIZE);

  uint32_t clamp = WORD_SIZE - length;

  T lhs_block = lhs[0] >> lhs_lsb;
  if (lhs_lsb + length > WORD_SIZE) {
    lhs_block |= (lhs[1] << (WORD_SIZE - lhs_lsb));
  }
  lhs_block = T(lhs_block << clamp) >> clamp;

  T rhs_block = rhs[0] >> rhs_lsb;
  if (rhs_lsb + length > WORD_SIZE) {
    rhs_block |= (rhs[1] << (WORD_SIZE - rhs_lsb));
  }
  rhs_block = T(rhs_block << clamp) >> clamp;

  return lhs_block - rhs_block;
}

template <typename T>
int bv_cmp_vector_aligned(const T* lhs, const T* rhs, uint32_t length) {
  static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
  assert(length > WORD_SIZE);

  uint32_t wcount = length / WORD_SIZE;
  uint32_t rem    = length % WORD_SIZE;

  // update aligned blocks
  if (0 == rem) {
    return memcmp(lhs, rhs, wcount * sizeof(T));
  } else {
    auto test = memcmp(lhs, rhs, (wcount - 1) * sizeof(T));
    if (test)
      return test;
    // compare remining block
    return bv_cmp_scalar(lhs + wcount, 0, rhs + wcount, 0, rem);
  }
}

template <typename T>
int bv_cmp_vector_aligned_lhs(const T* lhs,
                              const T* rhs, uint32_t rhs_lsb,
                              uint32_t length) {
  static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
  assert(length > WORD_SIZE);
  assert(rhs_lsb && rhs_lsb < WORD_SIZE);

  uint32_t wcount = length / WORD_SIZE;
  uint32_t rem    = length % WORD_SIZE;

  // compare intermediate blocks
  for (uint32_t n = wcount; n--;) {
    T lhs_block = *lhs++;
    T rhs_block = *rhs++ >> rhs_lsb;
    rhs_block |= (rhs[0] << (WORD_SIZE - rhs_lsb));
    std::make_signed_t<T> test = lhs_block - rhs_block;
    if (test)
      return (test > 0) ? 1 : -1;
  }
  if (0 == rem)
    return 0;
  // compare remining block
  return bv_cmp_vector_small(lhs, 0, rhs, rhs_lsb, rem);
}

template <typename T>
int bv_cmp_vector_aligned_rhs(const T* lhs, uint32_t lhs_lsb,
                              const T* rhs,
                              uint32_t length) {
  static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
  assert(length > WORD_SIZE);
  assert(lhs_lsb && lhs_lsb < WORD_SIZE);

  uint32_t wcount = length / WORD_SIZE;
  uint32_t rem    = length % WORD_SIZE;

  // compare intermediate blocks
  for (uint32_t n = wcount; n--;) {
    T lhs_block = *lhs++ >> lhs_lsb;
    lhs_block |= (lhs[0] << (WORD_SIZE - lhs_lsb));
    T rhs_block = *rhs++;
    std::make_signed_t<T> test = lhs_block - rhs_block;
    if (test)
      return (test > 0) ? 1 : -1;
  }
  if (0 == rem)
    return 0;
  // compare remining block
  return bv_cmp_vector_small(lhs, lhs_lsb, rhs, 0, rem);
}

template <typename T>
int bv_cmp_vector_unaligned(const T* lhs, uint32_t lhs_lsb,
                            const T* rhs, uint32_t rhs_lsb,
                            uint32_t length) {
  static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
  assert(length > WORD_SIZE);
  assert(lhs_lsb && lhs_lsb < WORD_SIZE);
  assert(rhs_lsb && rhs_lsb < WORD_SIZE);

  uint32_t wcount = length / WORD_SIZE;
  uint32_t rem    = length % WORD_SIZE;

  // compare intermediate blocks
  for (uint32_t n = wcount; n--;) {
    T lhs_block = *lhs++ >> lhs_lsb;
    lhs_block |= (lhs[0] << (WORD_SIZE - lhs_lsb));
    T rhs_block = *rhs++ >> rhs_lsb;
    rhs_block |= (rhs[0] << (WORD_SIZE - rhs_lsb));
    std::make_signed_t<T> test = lhs_block - rhs_block;
    if (test)
      return (test > 0) ? 1 : -1;
  }
  if (0 == rem)
    return 0;
  // compare remining block
  return bv_cmp_vector_small(lhs, lhs_lsb, rhs, rhs_lsb, rem);
}

template <typename T>
int bv_cmp_vector(const T* lhs, uint32_t lhs_lsb,
                  const T* rhs, uint32_t rhs_lsb,
                  uint32_t length) {
  static unsigned constexpr WORD_SIZE = bitwidth_v<T>;
  assert(lhs_lsb < WORD_SIZE);
  assert(rhs_lsb < WORD_SIZE);

  if (length <= WORD_SIZE) {
    return bv_cmp_vector_small(lhs, lhs_lsb, rhs, rhs_lsb, length);
  } else
  if (0 == ((lhs_lsb % 8) | (rhs_lsb % 8))) {
    auto b_lhs = reinterpret_cast<const uint8_t*>(lhs) + (lhs_lsb / 8);
    auto b_rhs = reinterpret_cast<const uint8_t*>(rhs) + (rhs_lsb / 8);
    return bv_cmp_vector_aligned(b_lhs, b_rhs, length);
  } else
  if (0 == lhs_lsb) {
    return bv_cmp_vector_aligned_lhs(lhs, rhs, rhs_lsb, length);
  } else
  if (0 == rhs_lsb) {
    return bv_cmp_vector_aligned_rhs(lhs, lhs_lsb, rhs, length);
  } else {
    return bv_cmp_vector_unaligned(lhs, lhs_lsb, rhs, rhs_lsb, length);
  }
}

template <typename T>
int bv_cmp(const T* lhs, uint32_t lhs_offset,
           const T* rhs, uint32_t rhs_offset,
           uint32_t length) {
  static unsigned constexpr WORD_SIZE = bitwidth_v<T>;

  if ((lhs_offset + length) <= WORD_SIZE
   && (rhs_offset + length) <= WORD_SIZE) {
    return bv_cmp_scalar(lhs, lhs_offset, rhs, rhs_offset, length);
  } else {
    auto lhs_idx = lhs_offset / WORD_SIZE;
    auto lhs_lsb = lhs_offset % WORD_SIZE;
    auto rhs_idx = rhs_offset / WORD_SIZE;
    auto rhs_lsb = rhs_offset % WORD_SIZE;
    return bv_cmp_vector(lhs + lhs_idx, lhs_lsb, rhs + rhs_idx, rhs_lsb,
length);
  }
}
*/

// --- Scalar path (length <= WORD_BITS) ---
template <bool is_signed, typename T>
int bv_cmp_scalar(const T *lhs, uint32_t lhs_offset, const T *rhs,
                  uint32_t rhs_offset, uint32_t length) {
    if (length == 0)
        return 0;
    if constexpr (is_signed) {
        bool lhs_neg = bv_is_neg(lhs, lhs_offset, length);
        bool rhs_neg = bv_is_neg(rhs, rhs_offset, length);
        if (lhs_neg != rhs_neg)
            return lhs_neg ? -1 : 1;
    }
    T lhs_val = extract_bits(lhs, lhs_offset, length);
    T rhs_val = extract_bits(rhs, rhs_offset, length);
    if (lhs_val < rhs_val)
        return -1;
    if (lhs_val > rhs_val)
        return 1;
    return 0;
}

// --- Vector path (length > WORD_BITS) ---
template <bool is_signed, typename T>
int bv_cmp_vector(const T *lhs, uint32_t lhs_offset, const T *rhs,
                  uint32_t rhs_offset, uint32_t length) {
    static constexpr uint32_t WORD_BITS = bitwidth_v<T>;

    if constexpr (is_signed) {
        bool lhs_neg = bv_is_neg(lhs, lhs_offset, length);
        bool rhs_neg = bv_is_neg(rhs, rhs_offset, length);
        if (lhs_neg != rhs_neg)
            return lhs_neg ? -1 : 1;
    }

    // General unaligned vector comparison
    uint32_t num_words = ceildiv(length, WORD_BITS);
    for (int32_t i = static_cast<int32_t>(num_words) - 1; i >= 0; --i) {
        uint32_t bits_remaining = length - static_cast<uint32_t>(i) * WORD_BITS;
        uint32_t bits_in_this_word = std::min(WORD_BITS, bits_remaining);

        T lhs_word =
            extract_bits(lhs, lhs_offset + i * WORD_BITS, bits_in_this_word);
        T rhs_word =
            extract_bits(rhs, rhs_offset + i * WORD_BITS, bits_in_this_word);

        if (lhs_word < rhs_word)
            return -1;
        if (lhs_word > rhs_word)
            return 1;
    }
    return 0;
}

// --- Unified entry point ---
template <bool is_signed, typename T>
int bv_cmp(const T *lhs, uint32_t lhs_offset, const T *rhs,
           uint32_t rhs_offset, uint32_t length) {
    if (length == 0)
        return 0;
    if (length <= bitwidth_v<T>) {
        return bv_cmp_scalar<is_signed, T>(lhs, lhs_offset, rhs, rhs_offset,
                                           length);
    } else {
        return bv_cmp_vector<is_signed, T>(lhs, lhs_offset, rhs, rhs_offset,
                                           length);
    }
}

} // namespace internal
} // namespace ch