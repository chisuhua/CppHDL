#ifndef CH_CORE_LITERAL_H
#define CH_CORE_LITERAL_H
#include <cstdint>
#include <type_traits>

#include "../lnode/literal_ext.h"
#include "core/uint.h"

namespace ch::core {

struct ch_literal {
    uint64_t value;
    uint32_t actual_width; // 由字面量字符决定（非十进制）或 bit_width（十进制）

    // 统一宽度计算：用于非字面量构造
    static constexpr uint32_t compute_width(uint64_t v) noexcept {
        return v == 0 ? 1u : static_cast<uint32_t>(std::bit_width(v));
    }

    // 主构造函数：确保宽度合法
    constexpr ch_literal(uint64_t v, uint32_t w) noexcept
        : value(v), actual_width(w == 0 ? 1u : (w > 64 ? 64u : w)) {}

    // 从值自动推断宽度（用于非字面量场景）
    constexpr ch_literal(uint64_t v) noexcept
        : ch_literal(v, compute_width(v)) {}

    constexpr ch_literal(int64_t v) noexcept
        : ch_literal(static_cast<uint64_t>(v), compute_width(v)) {}

    constexpr ch_literal(unsigned long long v) noexcept
        : ch_literal(static_cast<uint64_t>(v), compute_width(v)) {}

    constexpr ch_literal(int v) noexcept : ch_literal(v, compute_width(v)) {}

    constexpr ch_literal(uint32_t v) noexcept
        : ch_literal(v, compute_width(v)) {}

    constexpr ch_literal(bool b) noexcept
        : value(b ? 1u : 0u), actual_width(1u) {}

    constexpr ch_literal() noexcept : value(0), actual_width(1u) {}

    constexpr unsigned width() const noexcept { return actual_width; }
    constexpr bool is_zero() const noexcept { return value == 0; }

    // 安全计算全 1 值：避免 (1 << 64) 未定义行为
    constexpr bool is_ones() const noexcept {
        if (actual_width == 64) {
            return value == UINT64_MAX;
        }
        return value == ((uint64_t{1} << actual_width) - 1);
    }

    // 隐式转换到 ch_uint<N>（带编译期检查）
    template <unsigned N> constexpr operator ch_uint<N>() const {
        static_assert(N >= actual_width,
                      "Literal width exceeds target ch_uint width");
        return ch_uint<N>(value);
    }
};

namespace literals {

// FIXME: use literals width instead of compute bit width

template <char... Chars> constexpr ch_literal operator"" _b() {
    constexpr auto value = lit_bin_value_v<Chars...>;
    // constexpr auto width = lit_bin_size_v<Chars...>;
    constexpr auto width = compute_bit_width(value);
    return ch_literal(value, width); // width ≥ 1 for valid input
}

template <char... Chars> constexpr ch_literal operator"" _o() {
    constexpr auto value = lit_oct_value_v<Chars...>;
    // constexpr auto width = lit_oct_size_v<Chars...>;
    constexpr auto width = compute_bit_width(value);
    return ch_literal(value, width);
}

template <char... Chars> constexpr ch_literal operator"" _h() {
    constexpr auto value = lit_hex_value_v<Chars...>;
    // constexpr auto width = lit_hex_size_v<Chars...>;
    constexpr auto width = compute_bit_width(value);
    return ch_literal(value, width);
}

template <char... Chars> constexpr ch_literal operator"" _d() {
    constexpr auto value = lit_dec_value_v<Chars...>;
    constexpr auto width = lit_dec_size_v<Chars...>; // uses std::bit_width
    return ch_literal(value, width);
}

} // namespace literals

// 工厂函数
inline constexpr ch_literal make_literal(uint64_t value, uint32_t width) {
    return ch_literal(value, width);
}

inline constexpr ch_literal make_literal_auto(uint64_t value) {
    return ch_literal(value); // uses compute_width
}

template <typename T>
inline constexpr bool is_ch_literal_v =
    std::is_same_v<std::remove_cvref_t<T>, ch_literal>;

} // namespace ch::core

// 引入字面量操作符到全局命名空间
using namespace ch::core::literals;

#endif // CH_CORE_LITERAL_H
