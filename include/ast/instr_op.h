// include/ast/instr_op.h
// Phase 7: Aggregator for AST op type structs (instr_op_*).
// After Phase 7 split, this file keeps:
//   - The 3 template wrapper classes (instr_op_binary, instr_op_unary, instr_op_ternary)
//   - Width/concat ops (BitSel, BitsExtract, Concat)
//   - Extension ops (Sext, Zext)
//   - Special ops (Assign, BitsUpdate, instr_op_bits_update class)
//   - All type aliases
// The per-category op structs (arith, logic, compare, shift) are in:
//   include/ast/instr_op_arith.h    (Add, Sub, Mul, Div, Mod, Neg)
//   include/ast/instr_op_logic.h    (And, Or, Xor, Not, And/Or/XorReduce)
//   include/ast/instr_op_compare.h  (Eq, Ne, Lt, Le, Gt, Ge)
//   include/ast/instr_op_shift.h    (Shl, Shr, RotateLeft, RotateRight, Sshr, PopCount)
#pragma once

#include "instr_base.h"
#include "logger.h"
#include <cstdint>
#include <iostream> // for std::cerr
#include <type_traits>

// Per-category op structs (each declares namespace ch { namespace op { ... } })
#include "instr_op_arith.h"
#include "instr_op_logic.h"
#include "instr_op_compare.h"
#include "instr_op_shift.h"

namespace ch {

// Forward declaration
class data_map_t;

// -----------------------------
// Generic Binary Operation Template
// -----------------------------
template <typename Op> class instr_op_binary : public instr_base {
public:
    instr_op_binary(core::sdata_type *dst, uint32_t size,
                    core::sdata_type *src0, core::sdata_type *src1)
        : instr_base(size), dst_(dst), src0_(src0), src1_(src1) {}

    void eval() override {
        if (!dst_ || !src0_ || !src1_) {
            std::cerr << "[" << Op::name()
                      << "] Error: Null pointer encountered!" << std::endl;
            return;
        }

        // Perform the operation
        Op::eval(dst_, src0_, src1_);
    }

private:
    core::sdata_type *dst_;
    core::sdata_type *src0_;
    core::sdata_type *src1_;
};

// -----------------------------
// Generic Unary Operation Template
// -----------------------------
template <typename Op> class instr_op_unary : public instr_base {
public:
    instr_op_unary(core::sdata_type *dst, uint32_t size,
                   core::sdata_type *src)
        : instr_base(size), dst_(dst), src_(src) {}

    void eval() override {
        if (!dst_ || !src_) {
            std::cerr << "[" << Op::name()
                      << "] Error: Null pointer encountered!" << std::endl;
            return;
        }

        Op::eval(dst_, src_);
    }

private:
    core::sdata_type *dst_;
    core::sdata_type *src_;
};

// -----------------------------
// Generic Ternary Operation Template
// -----------------------------
template <typename Op> class instr_op_ternary : public instr_base {
public:
    instr_op_ternary(core::sdata_type *dst, uint32_t size,
                     core::sdata_type *src0, core::sdata_type *src1,
                     core::sdata_type *src2)
        : instr_base(size), dst_(dst), src0_(src0), src1_(src1), src2_(src2) {}

    void eval() override {
        if (!dst_ || !src0_ || !src1_ || !src2_) {
            std::cerr << "[" << Op::name()
                      << "] Error: Null pointer encountered!" << std::endl;
            return;
        }

        // Perform the operation with 4 arguments for other operations
        Op::eval(dst_, src0_, src1_, src2_);
    }

private:
    core::sdata_type *dst_;
    core::sdata_type *src0_;
    core::sdata_type *src1_;
    core::sdata_type *src2_;
};

// === Per-category op structs ===
namespace op {

// BIT_SEL (位选择)
struct BitSel {
    static const char *name() { return "instr_op_bit_sel::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        uint64_t bit_index = static_cast<uint64_t>(*src1);
        if (bit_index < src0->bitwidth()) {
            bool bit_val = src0->get_bit(static_cast<uint32_t>(bit_index));
            *dst = bit_val ? 1 : 0;
        } else {
            std::cerr << "[instr_op_bit_sel] Error: Bit index out of range!"
                      << std::endl;
            *dst = 0;
        }
    }
};

// BITS_EXTRACT (位提取) - 提取 [msb:lsb] 范围的位
// src1 编码: 低32位 = lsb, 高32位 = msb
struct BitsExtract {
    static const char *name() { return "instr_op_bits_extract::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        uint64_t range_val = static_cast<uint64_t>(*src1);
        uint32_t lsb = static_cast<uint32_t>(range_val & 0xFFFFFFFF);
        uint32_t msb = static_cast<uint32_t>((range_val >> 32) & 0xFFFFFFFF);

        if (msb >= src0->bitwidth() || lsb > msb) {
            std::cerr << "[instr_op_bits_extract] Error: Invalid bit range ["
                      << msb << ":" << lsb << "]!" << std::endl;
            *dst = core::sdata_type(0, dst->bitwidth());
            return;
        }

        uint32_t result_width = msb - lsb + 1;
        if (result_width != dst->bitwidth()) {
            std::cerr << "[instr_op_bits_extract] Error: Destination width "
                         "mismatch! Expected: "
                      << result_width << ", Actual: " << dst->bitwidth()
                      << std::endl;
            *dst = core::sdata_type(0, dst->bitwidth());
            return;
        }

        for (uint32_t i = 0; i < result_width; ++i) {
            bool bit_val = src0->get_bit(lsb + i);
            dst->set_bit(i, bit_val);
        }
    }
};

// CONCAT (连接) - src0 (高位) + src1 (低位)
struct Concat {
    static const char *name() { return "instr_op_concat::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src0,
                     core::sdata_type *src1) {
        uint32_t src0_width = src0->bitwidth();
        uint32_t src1_width = src1->bitwidth();
        uint32_t expected_width = src0_width + src1_width;

        if (expected_width != dst->bitwidth()) {
            std::cerr
                << "[instr_op_concat] Warning: Destination width mismatch!"
                << " dest width=" << dst->bitwidth()
                << ", expected: " << expected_width
                << ", src0 width=" << src0_width << " src1_width=" << src1_width
                << std::endl;
            *dst = core::sdata_type(dst->bitwidth());
        }

        for (uint32_t i = 0; i < src1_width; ++i) {
            bool bit_val = src1->get_bit(i);
            dst->set_bit(i, bit_val);
        }
        for (uint32_t i = 0; i < src0_width; ++i) {
            bool bit_val = src0->get_bit(i);
            dst->set_bit(src1_width + i, bit_val);
        }
    }
};

// SEXT (符号扩展) - 一元操作
struct Sext {
    static const char *name() { return "instr_op_sext::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src) {
        uint32_t src_width = src->bitwidth();
        uint32_t dst_width = dst->bitwidth();

        if (src_width > dst_width) {
            std::cerr << "[instr_op_sext] Error: Source width larger than "
                         "destination!"
                      << std::endl;
            *dst = core::sdata_type(0, dst_width);
            return;
        }

        for (uint32_t i = 0; i < src_width; ++i) {
            bool bit_val = src->get_bit(i);
            dst->set_bit(i, bit_val);
        }

        bool sign_bit = (src_width > 0) ? src->get_bit(src_width - 1) : false;
        for (uint32_t i = src_width; i < dst_width; ++i) {
            dst->set_bit(i, sign_bit);
        }
    }
};

// ZEXT (零扩展) - 一元操作
struct Zext {
    static const char *name() { return "instr_op_zext::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src) {
        uint32_t src_width = src->bitwidth();
        uint32_t dst_width = dst->bitwidth();

        if (src_width > dst_width) {
            std::cerr << "[instr_op_zext] Error: Source width larger than "
                         "destination!"
                      << std::endl;
            *dst = core::sdata_type(0, dst_width);
            return;
        }

        for (uint32_t i = 0; i < src_width; ++i) {
            bool bit_val = src->get_bit(i);
            dst->set_bit(i, bit_val);
        }

        for (uint32_t i = src_width; i < dst_width; ++i) {
            dst->set_bit(i, false);
        }
    }
};

// ASSIGN 操作定义
struct Assign {
    static const char *name() { return "instr_op_assign::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *src) {
        dst->assign_truncate(*src);
    }
};

// BITS_UPDATE 操作定义 - 将源数据分配到位范围
struct BitsUpdate {
    static const char *name() { return "instr_op_bits_update::eval"; }
    static void eval(core::sdata_type *dst, core::sdata_type *target,
                     core::sdata_type *source,
                     core::sdata_type *range_info) {
        uint64_t range_val = static_cast<uint64_t>(*range_info);
        uint32_t lsb = static_cast<uint32_t>(range_val & 0xFFFFFFFF);
        uint32_t msb = static_cast<uint32_t>((range_val >> 32) & 0xFFFFFFFF);

        if (dst->bitwidth() <= msb) {
            std::cerr << "Destination width " << dst->bitwidth()
                      << " is less than required MSB " << msb << std::endl;
            return;
        }

        *dst = *target;
        for (uint32_t i = 0; i < (msb - lsb + 1) && i < source->bitwidth(); ++i) {
            bool bit_val = source->get_bit(i);
            dst->set_bit(lsb + i, bit_val);
        }
    }
};

} // namespace op

// bits_update 指令：dst = (target & clear_mask) | (source << lsb & keep_mask)
class instr_op_bits_update : public instr_base {
public:
    instr_op_bits_update(core::sdata_type *dst, uint32_t size,
                         core::sdata_type *target, core::sdata_type *source,
                         core::sdata_type *range_info)
        : instr_base(size), dst_(dst), target_(target), source_(source),
          range_(range_info) {}

    void eval() override {
        if (!dst_ || !target_ || !source_ || !range_) {
            std::cerr << "[instr_op_bits_update] Error: Null pointer encountered!"
                      << std::endl;
            return;
        }
        ::ch::op::BitsUpdate::eval(dst_, target_, source_, range_);
    }

private:
    core::sdata_type *dst_;
    core::sdata_type *target_;
    core::sdata_type *source_;
    core::sdata_type *range_;
};

// -----------------------------
// Type Aliases (扩展的别名)
// -----------------------------

// 现有的别名保持不变
using instr_op_add = instr_op_binary<op::Add>;
using instr_op_sub = instr_op_binary<op::Sub>;
using instr_op_mul = instr_op_binary<op::Mul>;
using instr_op_and = instr_op_binary<op::And>;
using instr_op_or = instr_op_binary<op::Or>;
using instr_op_xor = instr_op_binary<op::Xor>;
using instr_op_eq = instr_op_binary<op::Eq>;
using instr_op_ne = instr_op_binary<op::Ne>;
using instr_op_lt = instr_op_binary<op::Lt>;
using instr_op_le = instr_op_binary<op::Le>;
using instr_op_gt = instr_op_binary<op::Gt>;
using instr_op_ge = instr_op_binary<op::Ge>;
using instr_op_not = instr_op_unary<op::Not>;

// 新添加的别名
using instr_op_div = instr_op_binary<op::Div>;
using instr_op_mod = instr_op_binary<op::Mod>;
using instr_op_shl = instr_op_binary<op::Shl>;
using instr_op_shr = instr_op_binary<op::Shr>;
using instr_op_sshr = instr_op_binary<op::Sshr>;
using instr_op_neg = instr_op_unary<op::Neg>;
using instr_op_bit_sel = instr_op_binary<op::BitSel>;
using instr_op_bits_extract = instr_op_binary<op::BitsExtract>;
using instr_op_concat = instr_op_binary<op::Concat>;
using instr_op_sext = instr_op_unary<op::Sext>;
using instr_op_zext = instr_op_unary<op::Zext>;

// 在类型别名部分添加
using instr_op_and_reduce = instr_op_unary<op::AndReduce>;
using instr_op_or_reduce = instr_op_unary<op::OrReduce>;
using instr_op_xor_reduce = instr_op_unary<op::XorReduce>;
using instr_op_assign = instr_op_unary<op::Assign>;

// 添加popcount别名
using instr_op_popcount = instr_op_unary<op::PopCount>;
using instr_op_rotate_left = instr_op_binary<op::RotateLeft>;
using instr_op_rotate_right = instr_op_binary<op::RotateRight>;

} // namespace ch
