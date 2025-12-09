#ifndef CH_LNODE_OPERATORS_H
#define CH_LNODE_OPERATORS_H

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
    static constexpr unsigned result_width = M;
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

} // namespace ch::core
#endif // CH_LNODE_OPERATORS_H