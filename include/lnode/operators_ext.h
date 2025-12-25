#ifndef CH_LNODE_OPERATORS_H
#define CH_LNODE_OPERATORS_H

#include "lnodeimpl.h"
#include <algorithm>

namespace ch::core {

// ===================================================================
// === 操作策略定义 ===
// ===================================================================

// 加法操作策略
struct add_op {
    static constexpr ch_op op_type = ch_op::add;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = std::max(M, N) + 1;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "add"; }
};

// 减法操作策略
struct sub_op {
    static constexpr ch_op op_type = ch_op::sub;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = std::max(M, N);
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "sub"; }
};

// 乘法操作策略
struct mul_op {
    static constexpr ch_op op_type = ch_op::mul;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = M + N;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "mul"; }
};

// 按位与操作策略
struct and_op {
    static constexpr ch_op op_type = ch_op::and_;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = std::max(M, N);
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "and"; }
};

// 按位或操作策略
struct or_op {
    static constexpr ch_op op_type = ch_op::or_;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = std::max(M, N);
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "or"; }
};

// 按位异或操作策略
struct xor_op {
    static constexpr ch_op op_type = ch_op::xor_;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = std::max(M, N);
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "xor"; }
};

// 左移操作策略
struct shl_op {
    static constexpr ch_op op_type = ch_op::shl;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = M + N;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "shl"; }
};

// 右移操作策略（逻辑右移）
struct shr_op {
    static constexpr ch_op op_type = ch_op::shr;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = M;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "shr"; }
};

// 算术右移操作策略
struct sshr_op {
    static constexpr ch_op op_type = ch_op::sshr;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = M;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "sshr"; }
};

// 相等比较策略
struct eq_op {
    static constexpr ch_op op_type = ch_op::eq;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = 1;
    static constexpr bool is_comparison = true;
    static constexpr const char *name() { return "eq"; }
};

// 不等比较策略
struct ne_op {
    static constexpr ch_op op_type = ch_op::ne;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = 1;
    static constexpr bool is_comparison = true;
    static constexpr const char *name() { return "ne"; }
};

// 小于比较策略
struct lt_op {
    static constexpr ch_op op_type = ch_op::lt;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = 1;
    static constexpr bool is_comparison = true;
    static constexpr const char *name() { return "lt"; }
};

// 小于等于比较策略
struct le_op {
    static constexpr ch_op op_type = ch_op::le;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = 1;
    static constexpr bool is_comparison = true;
    static constexpr const char *name() { return "le"; }
};

// 大于比较策略
struct gt_op {
    static constexpr ch_op op_type = ch_op::gt;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = 1;
    static constexpr bool is_comparison = true;
    static constexpr const char *name() { return "gt"; }
};

// 大于等于比较策略
struct ge_op {
    static constexpr ch_op op_type = ch_op::ge;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = 1;
    static constexpr bool is_comparison = true;
    static constexpr const char *name() { return "ge"; }
};

// 取反操作策略
struct not_op {
    static constexpr ch_op op_type = ch_op::not_;
    static constexpr bool is_comparison = false;
    static constexpr bool is_bitwise = true;

    template <unsigned N>
    static constexpr unsigned result_width_v = N; // 位取反保持相同宽度

    static constexpr const char *name() { return "not"; }
};

// 负号操作策略
struct neg_op {
    static constexpr ch_op op_type = ch_op::neg;
    static constexpr bool is_comparison = false;
    static constexpr bool is_bitwise = false;

    template <unsigned N>
    static constexpr unsigned result_width_v = N; // 负号保持相同宽度

    static constexpr const char *name() { return "neg"; }
};

// 布尔专用操作类型
struct logical_and_op {
    static constexpr ch_op op_type = ch_op::and_;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "logical_and"; }

    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = 1;
};

struct logical_or_op {
    static constexpr ch_op op_type = ch_op::or_;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "logical_or"; }

    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = 1;
};

struct logical_not_op {
    static constexpr ch_op op_type = ch_op::not_;
    static constexpr const char *name() { return "logical_not"; }
};

// 多路选择器操作
struct mux_op {
    static constexpr ch_op op_type = ch_op::mux;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "mux"; }

    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = std::max(M, N);
};

// 除法操作策略
struct div_op {
    static constexpr ch_op op_type = ch_op::div;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = M;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "div"; }
};

// 取模操作策略
struct mod_op {
    static constexpr ch_op op_type = ch_op::mod;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = N;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "mod"; }
};

// 位选择操作策略
struct bit_sel_op {
    static constexpr ch_op op_type = ch_op::bit_sel;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = 1;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "bit_sel"; }
};

// 位提取操作策略
struct bits_extract_op {
    static constexpr ch_op op_type = ch_op::bits_extract;
    // 注意：这里的宽度计算需要根据具体的提取范围来定
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = N; // 这里假设N代表提取的位数
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "bits_extract"; }
};

// 连接操作策略
struct concat_op {
    static constexpr ch_op op_type = ch_op::concat;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = M + N;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "concat"; }
};

// 符号扩展操作策略
struct sext_op {
    static constexpr ch_op op_type = ch_op::sext;
    // 扩展后的宽度取决于目标宽度
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = N; // 这里假设N是目标宽度
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "sext"; }
};

// 零扩展操作策略
struct zext_op {
    static constexpr ch_op op_type = ch_op::zext;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = N; // 这里假设N是目标宽度
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "zext"; }
};

// 与归约操作策略
struct and_reduce_op {
    static constexpr ch_op op_type = ch_op::and_reduce;
    template <unsigned N> static constexpr unsigned result_width_v = 1;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "and_reduce"; }
};

// 或归约操作策略
struct or_reduce_op {
    static constexpr ch_op op_type = ch_op::or_reduce;
    template <unsigned N> static constexpr unsigned result_width_v = 1;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "or_reduce"; }
};

// 异或归约操作策略
struct xor_reduce_op {
    static constexpr ch_op op_type = ch_op::xor_reduce;
    template <unsigned N> static constexpr unsigned result_width_v = 1;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "xor_reduce"; }
};

// 循环左移操作策略
struct rotate_l_op {
    static constexpr ch_op op_type = ch_op::rotate_l;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = M;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "rotate_l"; }
};

// 循环右移操作策略
struct rotate_r_op {
    static constexpr ch_op op_type = ch_op::rotate_r;
    template <unsigned M, unsigned N>
    static constexpr unsigned result_width = M;
    static constexpr bool is_commutative = false;
    static constexpr bool is_bitwise = false;
    static constexpr bool is_comparison = false;
    static constexpr const char *name() { return "rotate_r"; }
};

// 添加popcount操作结构体
struct popcount_op {
    static constexpr ch_op op_type = ch_op::popcount;
    static constexpr bool is_commutative = false;
    static constexpr bool is_bitwise = false;
    static constexpr bool is_comparison = false;

    template <unsigned N>
    static constexpr unsigned result_width_v = (N <= 1) ? 1 : std::bit_width(N);

    static constexpr const char *name() { return "popcount"; }
};

} // namespace ch::core
#endif // CH_LNODE_OPERATORS_H