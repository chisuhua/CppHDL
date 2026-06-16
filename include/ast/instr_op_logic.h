// include/ast/instr_op_logic.h
// Phase 7: Logic op structs (And, Or, Xor, Not, AndReduce, OrReduce, XorReduce).
// Split from instr_op.h to keep each file <200 lines per Phase 7 plan.
#pragma once

#include "instr_base.h"
#include "logger.h"
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace ch {
namespace op {

// AND
struct And {
    static const char *name() { return "instr_op_and::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        auto width = std::max(src0->bitwidth(), src1->bitwidth());
        dst->bv_.resize(width);
        ::ch::internal::bv_and_truncate<uint64_t>(&dst->bv_, &src0->bv_, &src1->bv_);
    }
};

// OR
struct Or {
    static const char *name() { return "instr_op_or::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        auto width = std::max(src0->bitwidth(), src1->bitwidth());
        dst->bv_.resize(width);
        ::ch::internal::bv_or_truncate<uint64_t>(&dst->bv_, &src0->bv_, &src1->bv_);
    }
};

// XOR
struct Xor {
    static const char *name() { return "instr_op_xor::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        auto width = std::max(src0->bitwidth(), src1->bitwidth());
        dst->bv_.resize(width);
        ::ch::internal::bv_xor_truncate<uint64_t>(&dst->bv_, &src0->bv_, &src1->bv_);
    }
};

// NOT
struct Not {
    static const char *name() { return "instr_op_not::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src) {
        dst->bv_.resize(src->bitwidth());
        ::ch::internal::bv_inv_truncate<uint64_t>(&dst->bv_, &src->bv_);
    }
};

// === Reduce Operations ===

// AND_REDUCE
struct AndReduce {
    static const char *name() { return "instr_op_and_reduce::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src) {
        bool result = true;
        for (uint32_t i = 0; i < src->bitwidth(); ++i) {
            result &= src->get_bit(i);
            if (!result)
                break;
        }
        *dst = result ? 1 : 0;
    }
};

// OR_REDUCE
struct OrReduce {
    static const char *name() { return "instr_op_or_reduce::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src) {
        bool result = false;
        for (uint32_t i = 0; i < src->bitwidth(); ++i) {
            result |= src->get_bit(i);
            if (result)
                break;
        }
        *dst = result ? 1 : 0;
    }
};

// XOR_REDUCE
struct XorReduce {
    static const char *name() { return "instr_op_xor_reduce::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src) {
        bool result = false;
        for (uint32_t i = 0; i < src->bitwidth(); ++i) {
            result ^= src->get_bit(i);
        }
        *dst = result ? 1 : 0;
    }
};

} // namespace op
} // namespace ch
