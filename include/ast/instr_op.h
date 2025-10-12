// include/sim/instr_op.h - 扩展版本
#ifndef INSTR_OP_H
#define INSTR_OP_H

#include "instr_base.h"
#include <cstdint>
#include <iostream>  // for std::cerr
#include <type_traits>

namespace ch {

// Forward declaration
class data_map_t;

// -----------------------------
// Generic Binary Operation Template
// -----------------------------
template<typename Op>
class instr_op_binary : public instr_base {
public:
    instr_op_binary(ch::core::sdata_type* dst, uint32_t size,
                    ch::core::sdata_type* src0, ch::core::sdata_type* src1)
        : instr_base(size), dst_(dst), src0_(src0), src1_(src1) {}

    void eval(const ch::data_map_t& data_map) override {
        (void)data_map; // Silence unused warning if not used

        if (!dst_ || !src0_ || !src1_) {
            std::cerr << "[" << Op::name() << "] Error: Null pointer encountered!" << std::endl;
            return;
        }

        // Perform the operation
        Op::eval(dst_, src0_, src1_);
    }

private:
    ch::core::sdata_type* dst_;
    ch::core::sdata_type* src0_;
    ch::core::sdata_type* src1_;
};

// -----------------------------
// Generic Unary Operation Template
// -----------------------------
template<typename Op>
class instr_op_unary : public instr_base {
public:
    instr_op_unary(ch::core::sdata_type* dst, uint32_t size,
                   ch::core::sdata_type* src)
        : instr_base(size), dst_(dst), src_(src) {}

    void eval(const ch::data_map_t& data_map) override {
        (void)data_map;

        if (!dst_ || !src_) {
            std::cerr << "[" << Op::name() << "] Error: Null pointer encountered!" << std::endl;
            return;
        }

        Op::eval(dst_, src_);
    }

private:
    ch::core::sdata_type* dst_;
    ch::core::sdata_type* src_;
};

// -----------------------------
// Operation Policies (Function Objects)
// -----------------------------
namespace op {

// Helper: Check if destination is 1-bit for comparisons
inline bool check_comparison_result_width(ch::core::sdata_type* dst) {
    if (dst->bitwidth() != 1) {
        std::cerr << "Error: Destination bitvector size must be 1 for comparison!" << std::endl;
        *dst = false;
        return false;
    }
    return true;
}

// === 现有的操作保持不变 ===
// ADD
struct Add {
    static const char* name() { return "instr_op_add::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        *dst = *src0 + *src1;
    }
};

// SUB
struct Sub {
    static const char* name() { return "instr_op_sub::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        *dst = *src0 - *src1;
    }
};

// MUL
struct Mul {
    static const char* name() { return "instr_op_mul::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        *dst = *src0 * *src1;
    }
};

// AND
struct And {
    static const char* name() { return "instr_op_and::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        *dst = *src0 & *src1;
    }
};

// OR
struct Or {
    static const char* name() { return "instr_op_or::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        *dst = *src0 | *src1;
    }
};

// XOR
struct Xor {
    static const char* name() { return "instr_op_xor::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        *dst = *src0 ^ *src1;
    }
};

// NOT
struct Not {
    static const char* name() { return "instr_op_not::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src) {
        *dst = ~(*src);
    }
};

// EQ ==
struct Eq {
    static const char* name() { return "instr_op_eq::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        if (!check_comparison_result_width(dst)) return;
        bool result_val = (*src0 == *src1);
        *dst = result_val ? 1 : 0;
    }
};

// NE !=
struct Ne {
    static const char* name() { return "instr_op_ne::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        if (!check_comparison_result_width(dst)) return;
        bool result_val = (*src0 != *src1);
        *dst = result_val ? 1 : 0;
    }
};

// LT <
struct Lt {
    static const char* name() { return "instr_op_lt::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        if (!check_comparison_result_width(dst)) return;
        bool result_val = (*src0 < *src1);
        *dst = result_val ? 1 : 0;
    }
};

// LE <=
struct Le {
    static const char* name() { return "instr_op_le::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        if (!check_comparison_result_width(dst)) return;
        bool result_val = (*src0 <= *src1);
        *dst = result_val ? 1 : 0;
    }
};

// GT >
struct Gt {
    static const char* name() { return "instr_op_gt::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        if (!check_comparison_result_width(dst)) return;
        bool result_val = (*src0 > *src1);
        *dst = result_val ? 1 : 0;
    }
};

// GE >=
struct Ge {
    static const char* name() { return "instr_op_ge::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        if (!check_comparison_result_width(dst)) return;
        bool result_val = (*src0 >= *src1);
        *dst = result_val ? 1 : 0;
    }
};

// === 新添加的操作 ===

// DIV (除法)
struct Div {
    static const char* name() { return "instr_op_div::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        if (src1->is_zero()) {
            std::cerr << "[instr_op_div] Error: Division by zero!" << std::endl;
            *dst = ch::core::sdata_type(0, dst->bitwidth());
            return;
        }
        *dst = *src0 / *src1;
    }
};

// MOD (取模)
struct Mod {
    static const char* name() { return "instr_op_mod::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        if (src1->is_zero()) {
            std::cerr << "[instr_op_mod] Error: Modulo by zero!" << std::endl;
            *dst = ch::core::sdata_type(0, dst->bitwidth());
            return;
        }
        *dst = *src0 % *src1;
    }
};

// SHL (左移)
struct Shl {
    static const char* name() { return "instr_op_shl::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        uint64_t shift = static_cast<uint64_t>(*src1);  // 修复：使用转换操作符
        *dst = (*src0) << static_cast<uint32_t>(shift);
    }
};

// SHR (逻辑右移)
struct Shr {
    static const char* name() { return "instr_op_shr::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        uint64_t shift = static_cast<uint64_t>(*src1);  // 修复：使用转换操作符
        *dst = (*src0) >> static_cast<uint32_t>(shift);
    }
};

// SSHR (算术右移)
struct Sshr {
    static const char* name() { return "instr_op_sshr::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        uint64_t shift = static_cast<uint64_t>(*src1);  // 修复：使用转换操作符
        bool sign_bit = src0->get_bit(src0->bitwidth() - 1);
        *dst = (*src0) >> static_cast<uint32_t>(shift);  // 先逻辑右移
        
        // 符号扩展
        if (sign_bit && shift > 0) {
            for (uint32_t i = src0->bitwidth() - static_cast<uint32_t>(shift); i < src0->bitwidth(); ++i) {
                dst->set_bit(i, true);
            }
        }
    }
};

// NEG (负号)
struct Neg {
    static const char* name() { return "instr_op_neg::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src) {
        *dst = -(*src);
    }
};

// BIT_SEL (位选择)
struct BitSel {
    static const char* name() { return "instr_op_bit_sel::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        uint64_t bit_index = static_cast<uint64_t>(*src1);  // 修复：使用转换操作符
        if (bit_index < src0->bitwidth()) {
            bool bit_val = src0->get_bit(static_cast<uint32_t>(bit_index));
            *dst = bit_val ? 1 : 0;
        } else {
            std::cerr << "[instr_op_bit_sel] Error: Bit index out of range!" << std::endl;
            *dst = 0;
        }
    }
};

} // namespace op

// -----------------------------
// Type Aliases (扩展的别名)
// -----------------------------

// 现有的别名保持不变
using instr_op_add = instr_op_binary<op::Add>;
using instr_op_sub = instr_op_binary<op::Sub>;
using instr_op_mul = instr_op_binary<op::Mul>;
using instr_op_and = instr_op_binary<op::And>;
using instr_op_or  = instr_op_binary<op::Or>;
using instr_op_xor = instr_op_binary<op::Xor>;
using instr_op_eq  = instr_op_binary<op::Eq>;
using instr_op_ne  = instr_op_binary<op::Ne>;
using instr_op_lt  = instr_op_binary<op::Lt>;
using instr_op_le  = instr_op_binary<op::Le>;
using instr_op_gt  = instr_op_binary<op::Gt>;
using instr_op_ge  = instr_op_binary<op::Ge>;
using instr_op_not = instr_op_unary<op::Not>;

// 新添加的别名
using instr_op_div   = instr_op_binary<op::Div>;
using instr_op_mod   = instr_op_binary<op::Mod>;
using instr_op_shl   = instr_op_binary<op::Shl>;
using instr_op_shr   = instr_op_binary<op::Shr>;
using instr_op_sshr  = instr_op_binary<op::Sshr>;
using instr_op_neg   = instr_op_unary<op::Neg>;
using instr_op_bit_sel = instr_op_binary<op::BitSel>;

} // namespace ch

#endif // INSTR_OP_H
