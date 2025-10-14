// include/core/literal.h
#ifndef CH_CORE_LITERAL_H
#define CH_CORE_LITERAL_H

#include <cstdint>
#include <type_traits>

#include "../lnode/literal.h"
#include "core/uint.h"

namespace ch { namespace core {

struct ch_literal {
    uint64_t value;
    uint32_t actual_width; // Minimum bits needed to represent `value`

    constexpr ch_literal(uint64_t v, uint32_t w) : value(v), actual_width(w) {
        if (actual_width == 0) actual_width = 1;
        if (actual_width > 64) actual_width = 64;
    }
    
    constexpr ch_literal() : value(0), actual_width(1) {}

    template<unsigned N>
    constexpr operator ch_uint<N>() const {
        //if constexpr (N < actual_width) {
            static_assert(N >= actual_width, "Literal width exceeds target width");
        //}
        return ch_uint<N>(value);
    }
};

// Trait to identify ch_literal
template<typename T>
struct is_ch_literal : std::false_type {};

template<>
struct is_ch_literal<ch_literal> : std::true_type {};

template<typename T>
constexpr bool is_ch_literal_v = is_ch_literal<std::remove_cvref_t<T>>::value;

namespace literals {

// 二进制字面量操作符 - 真正实现解析功能
template <char... Chars>
constexpr ch_literal operator "" _b() {
    constexpr unsigned width = lit_bin_size_v<Chars...>;
    constexpr uint64_t value = lit_bin_value_v<Chars...>;
    return ch_literal(value, width ? width : 1);
}

// 八进制字面量操作符
template <char... Chars>
constexpr ch_literal operator "" _o() {
    constexpr unsigned width = lit_oct_size_v<Chars...>;
    constexpr uint64_t value = lit_oct_value_v<Chars...>;
    return ch_literal(value, width ? width : 1);
}

// 十六进制字面量操作符
template <char... Chars>
constexpr ch_literal operator "" _h() {
    constexpr unsigned width = lit_hex_size_v<Chars...>;
    constexpr uint64_t value = lit_hex_value_v<Chars...>;
    return ch_literal(value, width ? width : 1);
}

// 十进制字面量操作符
template <char... Chars>
constexpr ch_literal operator "" _d() {
    constexpr uint64_t value = lit_dec_value_v<Chars...>;
    constexpr unsigned width = lit_dec_size_v<Chars...>;
    return ch_literal(value, width ? width : 1);
}
} // namespace literals

// 创建指定宽度的字面量
inline constexpr ch_literal make_literal(uint64_t value, uint32_t width) {
    return ch_literal(value, width);
}

// 自动推断宽度的字面量
inline constexpr ch_literal make_literal_auto(uint64_t value) {
    return ch_literal(value, bit_width(value));
}

}} // namespace ch::core

// 引入字面量操作符到全局命名空间
using namespace ch::core::literals;

#endif // CH_CORE_LITERAL_H
