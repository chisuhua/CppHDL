// include/ast/instr_op_compare.h
// Phase 7: Comparison op structs (Eq, Ne, Lt, Le, Gt, Ge).
// Split from instr_op.h to keep each file <200 lines per Phase 7 plan.
#pragma once

#include "instr_base.h"
#include "logger.h"
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace ch {
namespace op {

// Helper: Check if destination is 1-bit for comparisons
inline bool check_comparison_result_width(core::sdata_type *dst) {
    if (dst->bitwidth() != 1) {
        std::cerr
            << "Error: Destination bitvector size must be 1 for comparison!"
            << std::endl;
        *dst = false;
        return false;
    }
    return true;
}

// EQ ==
struct Eq {
    static const char *name() { return "instr_op_eq::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        if (!check_comparison_result_width(dst))
            return;
        bool result_val = (*src0 == *src1);
        *dst = result_val ? 1 : 0;
    }
};

// NE !=
struct Ne {
    static const char *name() { return "instr_op_ne::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        if (!check_comparison_result_width(dst))
            return;
        bool result_val = (*src0 != *src1);
        *dst = result_val ? 1 : 0;
    }
};

// LT <
struct Lt {
    static const char *name() { return "instr_op_lt::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        if (!check_comparison_result_width(dst))
            return;
        bool result_val = (*src0 < *src1);
        *dst = result_val ? 1 : 0;
    }
};

// LE <=
struct Le {
    static const char *name() { return "instr_op_le::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        if (!check_comparison_result_width(dst))
            return;
        bool result_val = (*src0 <= *src1);
        *dst = result_val ? 1 : 0;
    }
};

// GT >
struct Gt {
    static const char *name() { return "instr_op_gt::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        if (!check_comparison_result_width(dst))
            return;
        bool result_val = (*src0 > *src1);
        *dst = result_val ? 1 : 0;
    }
};

// GE >=
struct Ge {
    static const char *name() { return "instr_op_ge::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        if (!check_comparison_result_width(dst))
            return;
        bool result_val = (*src0 >= *src1);
        *dst = result_val ? 1 : 0;
    }
};

} // namespace op
} // namespace ch
