#pragma once

#include "utils.h"
#include "bv_copy.h"
#include "bv_bit_accessor.h"
#include <algorithm>
#include <cstring>

namespace ch {
namespace internal {

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_inv_scalar(T *out, uint32_t out_size, const T *in, uint32_t in_size) {
    BitAccessor arg0(in, in_size, out_size);
    out[0] = ~arg0.get();
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_inv_vector(T *out, uint32_t out_size, const T *in, uint32_t in_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    BitAccessor arg0(in, in_size, out_size);

    uint32_t num_words = ceildiv(out_size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        out[i] = ~arg0.get(i);
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_inv(T *out, uint32_t out_size, const T *in, uint32_t in_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && in_size <= WORD_SIZE) {
        return bv_inv_scalar<is_signed, T, BitAccessor>(out, out_size, in,
                                                        in_size);
    } else {
        return bv_inv_vector<is_signed, T, BitAccessor>(out, out_size, in,
                                                        in_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_and_scalar(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    out[0] = arg0.get() & arg1.get();

    if constexpr (is_signed) {
        if (arg0.need_resize() || arg1.need_resize()) {
            bv_clear_extra_bits(out, out_size);
        }
    }
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_and_vector(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    uint32_t num_words = ceildiv(out_size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        out[i] = arg0.get(i) & arg1.get(i);
    }

    if constexpr (is_signed) {
        if (arg0.need_resize() || arg1.need_resize()) {
            bv_clear_extra_bits(out, out_size);
        }
    }
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_and(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
            const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && lhs_size <= WORD_SIZE &&
        rhs_size <= WORD_SIZE) {
        return bv_and_scalar<is_signed, T, BitAccessor>(
            out, out_size, lhs, lhs_size, rhs, rhs_size);
    } else {
        return bv_and_vector<is_signed, T, BitAccessor>(
            out, out_size, lhs, lhs_size, rhs, rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_or_scalar(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                  const T *rhs, uint32_t rhs_size) {
    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    out[0] = arg0.get() | arg1.get();

    if constexpr (is_signed) {
        if (arg0.need_resize() || arg1.need_resize()) {
            bv_clear_extra_bits(out, out_size);
        }
    }
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_or_vector(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                  const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    uint32_t num_words = ceildiv(out_size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        out[i] = arg0.get(i) | arg1.get(i);
    }

    if constexpr (is_signed) {
        if (arg0.need_resize() || arg1.need_resize()) {
            bv_clear_extra_bits(out, out_size);
        }
    }
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_or(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
           const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && lhs_size <= WORD_SIZE &&
        rhs_size <= WORD_SIZE) {
        return bv_or_scalar<is_signed, T, BitAccessor>(out, out_size, lhs,
                                                       lhs_size, rhs, rhs_size);
    } else {
        return bv_or_vector<is_signed, T, BitAccessor>(out, out_size, lhs,
                                                       lhs_size, rhs, rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_xor_scalar(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    out[0] = arg0.get() ^ arg1.get();

    if constexpr (is_signed) {
        if (arg0.need_resize() || arg1.need_resize()) {
            bv_clear_extra_bits(out, out_size);
        }
    }
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_xor_vector(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    uint32_t num_words = ceildiv(out_size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        out[i] = arg0.get(i) ^ arg1.get(i);
    }

    if constexpr (is_signed) {
        if (arg0.need_resize() || arg1.need_resize()) {
            bv_clear_extra_bits(out, out_size);
        }
    }
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_xor(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
            const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && lhs_size <= WORD_SIZE &&
        rhs_size <= WORD_SIZE) {
        return bv_xor_scalar<is_signed, T, BitAccessor>(
            out, out_size, lhs, lhs_size, rhs, rhs_size);
    } else {
        return bv_xor_vector<is_signed, T, BitAccessor>(
            out, out_size, lhs, lhs_size, rhs, rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_blend_scalar(T *out, uint32_t out_size, const T *mask,
                     uint32_t mask_size, const T *lhs, uint32_t lhs_size,
                     const T *rhs, uint32_t rhs_size) {
    BitAccessor arg0(mask, mask_size, out_size);
    BitAccessor arg1(lhs, lhs_size, out_size);
    BitAccessor arg2(rhs, rhs_size, out_size);

    out[0] = arg1.get() ^ ((arg1.get() ^ arg2.get()) & arg0.get());

    if constexpr (is_signed) {
        if (arg0.need_resize() || arg1.need_resize() || arg2.need_resize()) {
            bv_clear_extra_bits(out, out_size);
        }
    }
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_blend_vector(T *out, uint32_t out_size, const T *mask,
                     uint32_t mask_size, const T *lhs, uint32_t lhs_size,
                     const T *rhs, uint32_t rhs_size) {
    bv_xor_vector<is_signed, T, BitAccessor>(out, out_size, lhs, lhs_size, rhs,
                                             rhs_size);
    bv_and_vector<is_signed, T, BitAccessor>(out, out_size, out, out_size, mask,
                                             mask_size);
    bv_xor_vector<is_signed, T, BitAccessor>(out, out_size, out, out_size, lhs,
                                             lhs_size);
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_blend(T *out, uint32_t out_size, const T *mask, uint32_t mask_size,
              const T *lhs, uint32_t lhs_size, const T *rhs,
              uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && mask_size <= WORD_SIZE &&
        lhs_size <= WORD_SIZE && rhs_size <= WORD_SIZE) {
        return bv_blend_scalar<is_signed, T, BitAccessor>(
            out, out_size, mask, mask_size, lhs, lhs_size, rhs, rhs_size);
    } else {
        return bv_blend_vector<is_signed, T, BitAccessor>(
            out, out_size, mask, mask_size, lhs, lhs_size, rhs, rhs_size);
    }
}

} // namespace internal
} // namespace ch