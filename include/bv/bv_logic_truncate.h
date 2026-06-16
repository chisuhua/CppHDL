#pragma once

#include "bitvector.h"
#include "bv.h"

namespace ch {
namespace internal {

template <typename T>
void bv_and_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return;

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return;

    uint32_t full_result_width = std::max(lhs_size, rhs_size);

    if (dst_size >= full_result_width) {
        bv_and<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_and<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
}

template <typename T>
void bv_or_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                    const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return;

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return;

    uint32_t full_result_width = std::max(lhs_size, rhs_size);

    if (dst_size >= full_result_width) {
        bv_or<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                     rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_or<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                 rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
}

template <typename T>
void bv_xor_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return;

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return;

    uint32_t full_result_width = std::max(lhs_size, rhs_size);

    if (dst_size >= full_result_width) {
        bv_xor<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_xor<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
}

template <typename T>
void bv_inv_truncate(T *dst, uint32_t dst_size, const T *src,
                     uint32_t src_size) {
    bv_inv<false, T>(dst, dst_size, src, src_size);
    bv_clear_extra_bits(dst, dst_size);
}

template <typename T>
void bv_inv_truncate(bitvector<T> *dst, const bitvector<T> *src) {
    bv_inv_truncate(dst->words(), dst->size(), src->words(), src->size());
}

template <typename T> bool bv_not_truncate(bitvector<T> *dst) {
    if (!dst)
        return false;

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();

    if (dst_size == 0)
        return false;
    return bv_not<T>(dst->words(), dst_size);
}

} // namespace internal
} // namespace ch