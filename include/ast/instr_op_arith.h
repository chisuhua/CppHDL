// include/ast/instr_op_arith.h
// Phase 7: Arithmetic op structs (Add, Sub, Mul, Div, Mod, Neg).
// Split from instr_op.h to keep each file <200 lines per Phase 7 plan.
#pragma once

#include "instr_base.h"
#include "logger.h"
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace ch {
namespace op {

// ADD
// dst is sized to the compile-time result bitwidth; bv_add_truncate
// truncates the natural max(lhs,rhs)+1 result to dst->size() (matches JIT
// mask to bitwidth).
struct Add {
    static const char *name() { return "instr_op_add::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        ::ch::internal::bv_add_truncate<uint64_t>(&dst->bv_, &src0->bv_, &src1->bv_);
    }
};

// SUB
struct Sub {
    static const char *name() { return "instr_op_sub::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        auto width = std::max(src0->bitwidth(), src1->bitwidth());
        dst->bv_.resize(width);
        ::ch::internal::bv_sub_truncate<uint64_t>(&dst->bv_, &src0->bv_, &src1->bv_);
    }
};

// MUL
// dst is sized to the compile-time result bitwidth; bv_mul_truncate
// truncates the natural lhs+lhs result to dst->size() (matches JIT mask
// to bitwidth).
struct Mul {
    static const char *name() { return "instr_op_mul::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        ::ch::internal::bv_mul_truncate<uint64_t>(&dst->bv_, &src0->bv_, &src1->bv_);
    }
};

// DIV (除法)
struct Div {
    static const char *name() { return "instr_op_div::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        if (src1->is_zero()) {
            std::cerr << "[instr_op_div] Error: Division by zero!" << std::endl;
            *dst = core::sdata_type(0, dst->bitwidth());
            return;
        }
        *dst = *src0 / *src1;
    }
};

// MOD (取模)
struct Mod {
    static const char *name() { return "instr_op_mod::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        if (src1->is_zero()) {
            std::cerr << "[instr_op_mod] Error: Modulo by zero!" << std::endl;
            *dst = core::sdata_type(0, dst->bitwidth());
            return;
        }
        *dst = *src0 % *src1;
    }
};

// NEG (负号)
// Two's complement negation: result = (~src + 1) & mask(dst.bw). We
// compute in uint64_t then assign with truncation to dst's compile-time
// bitwidth, avoiding the implicit +1 width growth of sdata::operator-
// (whose `~src + 1` uses add_width = max(bw,bw)+1).
struct Neg {
    static const char *name() { return "instr_op_neg::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src) {
        uint64_t src_val = static_cast<uint64_t>(*src);
        uint64_t neg_val = (~src_val) + 1ULL;
        if (dst->bitwidth() < 64) {
            uint64_t mask = (1ULL << dst->bitwidth()) - 1ULL;
            neg_val &= mask;
        }
        *dst = neg_val;
    }
};

} // namespace op
} // namespace ch
