#pragma once

#include "bitvector.h"
#include "bv.h"

namespace ch {
namespace internal {

// Preserved for reference (commented-out older bv_assign_truncate/bv_add_truncate
// implementations from the original bitvector.h, retained per zero-debt policy).
// These reference implementations were superseded by the active versions below.

template <typename T>
void bv_assign_truncate(T *dst, uint32_t dst_size, const T *src,
                        uint32_t src_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (dst_size == 0) {
        return;
    }

    if (src_size == 0 || src == nullptr) {
        uint32_t num_dst_words_for_size = ceildiv(dst_size, WORD_SIZE);
        if (dst && num_dst_words_for_size > 0) {
            std::fill_n(dst, num_dst_words_for_size, T(0));
        }
        bv_clear_extra_bits(dst, dst_size);
        return;
    }

    if (dst == nullptr) {
        return;
    }

    uint32_t copy_bits = std::min(src_size, dst_size);
    uint32_t copy_full_words = copy_bits / WORD_SIZE;
    uint32_t copy_rem_bits = copy_bits % WORD_SIZE;

    if (copy_full_words > 0) {
        std::memcpy(dst, src, copy_full_words * sizeof(T));
    }

    if (copy_rem_bits > 0) {
        uint32_t partial_word_index = copy_full_words;
        T mask = (T(1) << copy_rem_bits) - 1;
        T src_partial_word = src[partial_word_index];
        dst[partial_word_index] = src_partial_word & mask;
    }

    if (dst_size > copy_bits) {
        uint32_t zero_start_word = ceildiv(copy_bits, WORD_SIZE);
        uint32_t zero_end_word = ceildiv(dst_size, WORD_SIZE);
        if (zero_end_word > zero_start_word && dst != nullptr) {
            std::fill(dst + zero_start_word, dst + zero_end_word, T(0));
        }
    }

    bv_clear_extra_bits(dst, dst_size);
}

// Performs assignment with truncation: dst = src
template <typename T>
void bv_assign_truncate(bitvector<T> *dst, const bitvector<T> *src) {
    bv_assign_truncate(dst->words(), dst->size(), src->words(), src->size());
}

// Performs addition: dst = lhs + rhs
// Truncates or zero-extends the result to fit exactly dst->size() bits.
template <typename T>
void bv_add_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return;

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return;

    uint32_t full_result_width = std::max(lhs_size, rhs_size) + 1;

    if (dst_size >= full_result_width) {
        bv_add<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_add<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
}

// Performs subtraction: dst = lhs - rhs
template <typename T>
void bv_sub_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
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
        bv_sub<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_sub<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
}

// Performs multiplication: dst = lhs * rhs
template <typename T>
void bv_mul_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return;

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return;

    uint32_t full_result_width = lhs_size + rhs_size;

    if (dst_size >= full_result_width) {
        bv_mul<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_mul<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
}

// Performs division: dst = lhs / rhs
template <typename T>
void bv_div_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return;

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return;

    uint32_t full_result_width = lhs_size;

    if (dst_size >= full_result_width) {
        bv_div<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_div<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
}

// Performs modulo: dst = lhs % rhs
template <typename T>
void bv_mod_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return;

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return;

    uint32_t full_result_width = rhs_size;

    if (dst_size >= full_result_width) {
        bv_mod<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_mod<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
}

} // namespace internal
} // namespace ch