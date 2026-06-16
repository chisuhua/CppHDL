// include/ast/instr_op_shift.h
// Phase 7: Shift + popcount op structs (Shl, Shr, RotateLeft, RotateRight, Sshr, PopCount).
// Split from instr_op.h to keep each file <200 lines per Phase 7 plan.
#pragma once

#include "instr_base.h"
#include "logger.h"
#include <bit>
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace ch {
namespace op {

// SHL (左移)
// dst is sized to the compile-time result bitwidth; bv_shl_truncate
// shifts into dst->size() bits (matches JIT mask to bitwidth). The
// previous `*dst = (*src0) << shift` path was buggy because
// sdata::operator=(sdata) resizes dst to the operator<< result width
// (max(compute_bw+shift, lhs.bw)), which can exceed dst.bw.
struct Shl {
    static const char *name() { return "instr_op_shl::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        uint64_t shift = static_cast<uint64_t>(*src1);
        ::ch::internal::bv_shl_truncate<uint64_t>(&dst->bv_, &src0->bv_,
                                                static_cast<uint32_t>(shift));
    }
};

// SHR (逻辑右移)
struct Shr {
    static const char *name() { return "instr_op_shr::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        uint64_t shift = static_cast<uint64_t>(*src1);
        *dst = (*src0) >> static_cast<uint32_t>(shift);
    }
};

// ROTATE_LEFT (循环左移)
struct RotateLeft {
    static const char *name() { return "instr_op_rotate_left::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        uint32_t bw = src0->bitwidth();
        uint64_t count = static_cast<uint64_t>(*src1) % bw;
        uint64_t val = static_cast<uint64_t>(*src0);
        uint64_t mask = (bw < 64) ? ((1ULL << bw) - 1) : ~0ULL;
        uint64_t result = ((val << count) | (val >> (bw - count))) & mask;
        *dst = result;
    }
};

// ROTATE_RIGHT (循环右移)
struct RotateRight {
    static const char *name() { return "instr_op_rotate_right::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        uint32_t bw = src0->bitwidth();
        uint64_t count = static_cast<uint64_t>(*src1) % bw;
        uint64_t val = static_cast<uint64_t>(*src0);
        uint64_t mask = (bw < 64) ? ((1ULL << bw) - 1) : ~0ULL;
        uint64_t result = ((val >> count) | (val << (bw - count))) & mask;
        *dst = result;
    }
};

// SSHR (算术右移)
struct Sshr {
    static const char *name() { return "instr_op_sshr::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        uint64_t shift = static_cast<uint64_t>(*src1);
        bool sign_bit = src0->get_bit(src0->bitwidth() - 1);
        *dst = (*src0) >> static_cast<uint32_t>(shift);

        if (sign_bit && shift > 0 && shift < src0->bitwidth()) {
            for (uint32_t i = src0->bitwidth() - static_cast<uint32_t>(shift);
                 i < src0->bitwidth(); ++i) {
                dst->set_bit(i, true);
            }
        }
    }
};

// POPCOUNT
struct PopCount {
    static const char *name() { return "instr_op_popcount::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src) {
        if (!check_dst_width(dst, src->bitwidth()))
            return;

        uint64_t count = 0;
        for (uint32_t i = 0; i < src->bitwidth(); ++i) {
            if (src->get_bit(i)) {
                count++;
            }
        }
        *dst = count;
    }

private:
    static bool check_dst_width(core::sdata_type *dst, uint32_t src_width) {
        uint32_t required_width =
            src_width > 1 ? static_cast<uint32_t>(std::bit_width(src_width))
                          : 1u;
        if (dst->bitwidth() < required_width) {
            CHERROR("Destination width {} is less than required width {} for "
                    "popcount of {} bits",
                    dst->bitwidth(), required_width, src_width);
            return false;
        }
        return true;
    }
};

} // namespace op
} // namespace ch
