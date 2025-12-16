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

    void eval() override {
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

    void eval() override {
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
        if (sign_bit && shift > 0 && shift < src0->bitwidth()) {
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

// BITS_EXTRACT (位提取) - 提取 [msb:lsb] 范围的位
// src1 编码: 低32位 = lsb, 高32位 = msb
struct BitsExtract {
    static const char* name() { return "instr_op_bits_extract::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        uint64_t range_val = static_cast<uint64_t>(*src1);
        uint32_t lsb = static_cast<uint32_t>(range_val & 0xFFFFFFFF);
        uint32_t msb = static_cast<uint32_t>((range_val >> 32) & 0xFFFFFFFF);
        
        if (msb >= src0->bitwidth() || lsb > msb) {
            std::cerr << "[instr_op_bits_extract] Error: Invalid bit range [" << msb << ":" << lsb << "]!" << std::endl;
            *dst = ch::core::sdata_type(0, dst->bitwidth());
            return;
        }
        
        uint32_t result_width = msb - lsb + 1;
        if (result_width != dst->bitwidth()) {
            std::cerr << "[instr_op_bits_extract] Error: Destination width mismatch! Expected: " 
                      << result_width << ", Actual: " << dst->bitwidth() << std::endl;
            *dst = ch::core::sdata_type(0, dst->bitwidth());
            return;
        }
        
        // 提取位
        for (uint32_t i = 0; i < result_width; ++i) {
            bool bit_val = src0->get_bit(lsb + i);
            dst->set_bit(i, bit_val);
        }
    }
};

// CONCAT (连接) - src0 (高位) + src1 (低位)
struct Concat {
    static const char* name() { return "instr_op_concat::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src0, ch::core::sdata_type* src1) {
        uint32_t src0_width = src0->bitwidth();
        uint32_t src1_width = src1->bitwidth();
        uint32_t expected_width = src0_width + src1_width;
        
        // 检查目标位宽是否符合预期
        if (expected_width != dst->bitwidth()) {
            std::cerr << "[instr_op_concat] Warning: Destination width mismatch!" << " dest width=" << dst->bitwidth() << ", expected: " << expected_width << ", src0 width=" << src0_width << " src1_width=" << src1_width << std::endl;
            // 重新调整目标位宽以匹配期望值
            *dst = ch::core::sdata_type(dst->bitwidth()); // 重置为目标位宽的0值
        }
        
        // 将src1放在低位，src0放在高位
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
    static const char* name() { return "instr_op_sext::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src) {
        uint32_t src_width = src->bitwidth();
        uint32_t dst_width = dst->bitwidth();
        
        if (src_width > dst_width) {
            std::cerr << "[instr_op_sext] Error: Source width larger than destination!" << std::endl;
            *dst = ch::core::sdata_type(0, dst_width);
            return;
        }
        
        // 复制原始位
        for (uint32_t i = 0; i < src_width; ++i) {
            bool bit_val = src->get_bit(i);
            dst->set_bit(i, bit_val);
        }
        
        // 符号扩展
        bool sign_bit = (src_width > 0) ? src->get_bit(src_width - 1) : false;
        for (uint32_t i = src_width; i < dst_width; ++i) {
            dst->set_bit(i, sign_bit);
        }
    }
};

// ZEXT (零扩展) - 一元操作
struct Zext {
    static const char* name() { return "instr_op_zext::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src) {
        uint32_t src_width = src->bitwidth();
        uint32_t dst_width = dst->bitwidth();
        
        if (src_width > dst_width) {
            std::cerr << "[instr_op_zext] Error: Source width larger than destination!" << std::endl;
            *dst = ch::core::sdata_type(0, dst_width);
            return;
        }
        
        // 复制原始位
        for (uint32_t i = 0; i < src_width; ++i) {
            bool bit_val = src->get_bit(i);
            dst->set_bit(i, bit_val);
        }
        
        // 零扩展
        for (uint32_t i = src_width; i < dst_width; ++i) {
            dst->set_bit(i, false);
        }
    }
};

// === 规约操作 (Reduce Operations) ===

// AND_REDUCE
struct AndReduce {
    static const char* name() { return "instr_op_and_reduce::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src) {
        if (!check_comparison_result_width(dst)) return;
        
        bool result = true;
        for (uint32_t i = 0; i < src->bitwidth(); ++i) {
            result &= src->get_bit(i);
            if (!result) break; // 早期退出优化
        }
        *dst = result ? 1 : 0;
    }
};

// OR_REDUCE  
struct OrReduce {
    static const char* name() { return "instr_op_or_reduce::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src) {
        if (!check_comparison_result_width(dst)) return;
        
        bool result = false;
        for (uint32_t i = 0; i < src->bitwidth(); ++i) {
            result |= src->get_bit(i);
            if (result) break; // 早期退出优化
        }
        *dst = result ? 1 : 0;
    }
};

// XOR_REDUCE
struct XorReduce {
    static const char* name() { return "instr_op_xor_reduce::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src) {
        if (!check_comparison_result_width(dst)) return;
        
        bool result = false;
        for (uint32_t i = 0; i < src->bitwidth(); ++i) {
            result ^= src->get_bit(i);
        }
        *dst = result ? 1 : 0;
    }
};

// POPCOUNT (新增)
struct PopCount {
    static const char* name() { return "instr_op_popcount::eval"; }
    static void eval(ch::core::sdata_type* dst, ch::core::sdata_type* src) {
        if (!check_dst_width(dst, src->bitwidth())) return;
        
        uint64_t count = 0;
        for (uint32_t i = 0; i < src->bitwidth(); ++i) {
            if (src->get_bit(i)) {
                count++;
            }
        }
        *dst = count;
    }
    
private:
    static bool check_dst_width(ch::core::sdata_type* dst, uint32_t src_width) {
        // popcount的结果最大值是src_width，所以需要足够的位宽来表示这个值
        // 例如，对于8位输入，结果最大是8，需要4位来表示（0-8）
        uint32_t required_width = src_width > 1 ? static_cast<uint32_t>(std::bit_width(src_width)) : 1u;
        if (dst->bitwidth() < required_width) {
            CHERROR("Destination width {} is less than required width {} for popcount of {} bits",
                   dst->bitwidth(), required_width, src_width);
            return false;
        }
        return true;
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
using instr_op_bits_extract = instr_op_binary<op::BitsExtract>;
using instr_op_concat = instr_op_binary<op::Concat>;
using instr_op_sext = instr_op_unary<op::Sext>;
using instr_op_zext = instr_op_unary<op::Zext>;

// 在类型别名部分添加
using instr_op_and_reduce = instr_op_unary<op::AndReduce>;
using instr_op_or_reduce = instr_op_unary<op::OrReduce>;
using instr_op_xor_reduce = instr_op_unary<op::XorReduce>;

// 添加popcount别名
using instr_op_popcount = instr_op_unary<op::PopCount>;

} // namespace ch

#endif // INSTR_OP_H