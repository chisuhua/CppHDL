#ifndef CH_CORE_LITERAL_H
#define CH_CORE_LITERAL_H
#include "core/traits.h"
#include <cstdint>
#include <type_traits>

#include "../lnode/literal_ext.h"

namespace ch::core {

// 前置声明ch_uint模板
template <unsigned N> struct ch_uint;

// 前置声明ch_uint模板
template <unsigned N> struct ch_uint;

// 定义模板化的字面量类，以便在编译时获取value和width
template <uint64_t V, uint32_t W> struct ch_literal_impl {
    static constexpr uint64_t value = V;
    static constexpr uint32_t actual_width = (W == 0 ? 1u : (W > 64 ? 64u : W));

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
    /*
    template <unsigned N> constexpr operator ch_uint<N>() const {
        static_assert(N >= actual_width,
                      "Literal width exceeds target ch_uint width");
        return ch_uint<N>(value);
    }
    */
};

// 保持原有的运行时版本，用于动态创建字面量
struct ch_literal_runtime {
    uint64_t value;
    uint32_t actual_width; // 由字面量字符决定（非十进制）或 bit_width（十进制）

    // 统一宽度计算：用于非字面量构造
    static constexpr uint32_t compute_width(uint64_t v) noexcept {
        return v == 0 ? 1u : static_cast<uint32_t>(std::bit_width(v));
    }

    // 主构造函数：确保宽度合法
    constexpr ch_literal_runtime(uint64_t v, uint32_t w) noexcept
        : value(v), actual_width(w == 0 ? 1u : (w > 64 ? 64u : w)) {}

    // 从值自动推断宽度（用于非字面量场景）
    constexpr ch_literal_runtime(uint64_t v) noexcept
        : ch_literal_runtime(v, compute_width(v)) {}

    constexpr ch_literal_runtime(int64_t v) noexcept
        : ch_literal_runtime(static_cast<uint64_t>(v), compute_width(v)) {}

    constexpr ch_literal_runtime(unsigned long long v) noexcept
        : ch_literal_runtime(static_cast<uint64_t>(v), compute_width(v)) {}

    constexpr ch_literal_runtime(int v) noexcept
        : ch_literal_runtime(v, compute_width(v)) {}

    constexpr ch_literal_runtime(uint32_t v) noexcept
        : ch_literal_runtime(v, compute_width(v)) {}

    constexpr ch_literal_runtime(bool b) noexcept
        : value(b ? 1u : 0u), actual_width(1u) {}

    constexpr ch_literal_runtime() noexcept : value(0), actual_width(1u) {}

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

// 定义通用的ch_literal类型别名，指向编译时版本
template <uint64_t V, uint32_t W> using ch_literal = ch_literal_impl<V, W>;

// 为了兼容现有的运行时使用方式，定义一个类型别名指向运行时版本
using ch_literal_dynamic = ch_literal_runtime;

namespace literals {

// 修改字面量操作符，使其返回编译时版本的ch_literal
template <char... Chars> constexpr auto operator"" _b() {
    constexpr auto value = lit_bin_value_v<Chars...>;
    constexpr auto width = lit_bin_size_v<Chars...>;
    return ch_literal_impl<value, width>();
}

template <char... Chars> constexpr auto operator"" _o() {
    constexpr auto value = lit_oct_value_v<Chars...>;
    constexpr auto width = lit_oct_size_v<Chars...>;
    return ch_literal_impl<value, width>();
}

template <char... Chars> constexpr auto operator"" _h() {
    constexpr auto value = lit_hex_value_v<Chars...>;
    constexpr auto width = lit_hex_size_v<Chars...>;
    return ch_literal_impl<value, width>();
}

template <char... Chars> constexpr auto operator"" _d() {
    constexpr auto value = lit_dec_value_v<Chars...>;
    constexpr auto width = lit_dec_size_v<Chars...>; // uses std::bit_width
    return ch_literal_impl<value, width>();
}

} // namespace literals

// 工厂函数
template <uint64_t V, uint32_t W> inline constexpr auto make_literal() {
    return ch_literal_impl<V, W>();
}

inline constexpr ch_literal_runtime make_literal(uint64_t value,
                                                 uint32_t width) {
    return ch_literal_runtime(value, width);
}

inline constexpr ch_literal_runtime make_literal_auto(uint64_t value) {
    return ch_literal_runtime(value); // uses compute_width
}

template <typename T>
inline constexpr bool is_ch_literal_v =
    std::is_same_v<std::remove_cvref_t<T>, ch_literal_runtime>;

// 添加模板特化以支持新的ch_literal_impl类型
template <uint64_t V, uint32_t W>
inline constexpr bool is_ch_literal_v<ch_literal_impl<V, W>> = true;

// 添加模板偏特化以支持cv限定的ch_literal_impl类型
template <uint64_t V, uint32_t W>
inline constexpr bool is_ch_literal_v<const ch_literal_impl<V, W>> = true;

template <uint64_t V, uint32_t W>
inline constexpr bool is_ch_literal_v<volatile ch_literal_impl<V, W>> = true;

template <uint64_t V, uint32_t W>
inline constexpr bool is_ch_literal_v<const volatile ch_literal_impl<V, W>> =
    true;

template <uint64_t V, uint32_t W> struct ch_width_impl<ch_literal_impl<V, W>> {
    static constexpr unsigned value = ch_literal_impl<V, W>::actual_width;
};

} // namespace ch::core

// 引入字面量操作符到全局命名空间
using namespace ch::core::literals;

#endif // CH_CORE_LITERAL_H