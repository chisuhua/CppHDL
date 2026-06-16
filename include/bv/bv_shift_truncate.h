#pragma once

#include "bitvector.h"
#include "bv.h"

namespace ch {
namespace internal {

template <typename T>
void bv_shl_truncate(bitvector<T> *dst, const bitvector<T> *src,
                     uint32_t dist) {
    if (!dst || !src)
        return;

    const uint32_t dst_size = dst->size();
    const uint32_t src_size = src->size();

    if (dst_size == 0)
        return;

    bv_shl<false>(dst->words(), dst_size, src->words(), src_size, dist);
    bv_clear_extra_bits(dst->words(), dst_size);
}

template <typename T>
void bv_shl(bitvector<T> *dst, const bitvector<T> *src, uint32_t dist) {
    if (!dst || !src)
        return;

    const uint32_t src_size = src->size();
    const uint32_t result_size = src_size + dist;

    bv_shl<false>(dst->words(), result_size, src->words(), src_size, dist);
}

template <typename T>
void bv_shr_truncate(bitvector<T> *dst, const bitvector<T> *src,
                     uint32_t dist) {
    if (!dst || !src)
        return;

    const uint32_t dst_size = dst->size();
    const uint32_t src_size = src->size();

    if (dst_size == 0)
        return;

    bv_shr<false>(dst->words(), dst_size, src->words(), src_size, dist);
    bv_clear_extra_bits(dst->words(), dst_size);
}

} // namespace internal
} // namespace ch