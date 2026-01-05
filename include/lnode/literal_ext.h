#ifndef CH_LNODE_LITERAL_H
#define CH_LNODE_LITERAL_H

#include <bit>
#include <cstdint>

inline constexpr unsigned compute_bit_width(uint64_t value) {
    return value == 0 ? 1u : static_cast<unsigned>(std::bit_width(value));
}

// 计算指向数据位指针的指针位宽
// 例如：对于N位宽的数据，索引范围是0到N-1，需要多少位来表示这些索引
// 1位宽数据：索引范围0，需要1位（特殊处理）
// 2位宽数据：索引范围0-1，需要1位
// 3位宽数据：索引范围0-2，需要2位
// 4位宽数据：索引范围0-3，需要2位
// 5-8位宽数据：索引范围0-4/0-7，需要3位
// 与compute_bit_width的区别：compute_bit_width计算表示数值所需的位宽
// compute_idx_width计算表示0到N-1索引所需的位宽
inline constexpr unsigned compute_idx_width(size_t data_width) {
    if (data_width <= 1) {
        return 1u;  // 对于0或1位宽的数据，索引只需要1位
    }
    // 计算ceil(log2(data_width))，即表示data_width个索引所需的位数
    // std::bit_width(data_width - 1)会给出表示0到data_width-1所需的最大位数
    return static_cast<unsigned>(std::bit_width(data_width - 1));
}

// ==================== 字面量解析辅助结构 ====================

struct lit_bin {
    static constexpr bool is_digit(char c) { return c == '0' || c == '1'; }
    static constexpr bool is_escape(char c) { return c == '\''; }
    static constexpr unsigned digit_width() { return 1u; }
    static constexpr unsigned chr2int(char c) {
        return static_cast<unsigned>(c - '0');
    }
    static constexpr unsigned base() { return 2u; }
};

struct lit_oct {
    static constexpr bool is_digit(char c) { return c >= '0' && c <= '7'; }
    static constexpr bool is_escape(char c) { return c == '\''; }
    static constexpr unsigned digit_width() { return 3u; }
    static constexpr unsigned chr2int(char c) {
        return static_cast<unsigned>(c - '0');
    }
    static constexpr unsigned base() { return 8u; }
};

struct lit_hex {
    static constexpr bool is_digit(char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
               (c >= 'a' && c <= 'f');
    }
    static constexpr bool is_escape(char c) {
        return (c == '\'' || c == 'x' || c == 'X');
    }
    static constexpr unsigned digit_width() { return 4u; }
    static constexpr unsigned chr2int(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10u;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10u;
        return 0u;
    }
    static constexpr unsigned base() { return 16u; }
};

struct lit_dec {
    static constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }
    static constexpr bool is_escape(char c) { return c == '\''; }
    static constexpr unsigned chr2int(char c) {
        return static_cast<unsigned>(c - '0');
    }
    static constexpr unsigned base() { return 10u; }
    // digit_width not used for decimal size
};

// ==================== 编译时字面量值解析 ====================

template <typename Lit, uint64_t Value, char... Chars> struct lit_value;

template <typename Lit, uint64_t Value> struct lit_value<Lit, Value> {
    static constexpr uint64_t value = Value;
};

template <typename Lit, uint64_t Value, char Char, char... Chars>
struct lit_value<Lit, Value, Char, Chars...> {
    static_assert(Lit::is_digit(Char) || Lit::is_escape(Char),
                  "invalid character in literal");
    static constexpr uint64_t new_value =
        Lit::is_escape(Char) ? Value
                             : (Value * Lit::base() + Lit::chr2int(Char));
    static constexpr uint64_t value =
        lit_value<Lit, new_value, Chars...>::value;
};

template <typename Lit, uint64_t Init, char... Chars>
inline constexpr uint64_t lit_value_v = lit_value<Lit, Init, Chars...>::value;

// ==================== 编译时字面量位宽解析 ====================

template <typename Lit, unsigned Acc, char... Chars> struct lit_size;

template <typename Lit, unsigned Acc> struct lit_size<Lit, Acc> {
    static constexpr unsigned value = Acc;
};

template <typename Lit, unsigned Acc, char Char, char... Chars>
struct lit_size<Lit, Acc, Char, Chars...> {
    static_assert(Lit::is_digit(Char) || Lit::is_escape(Char),
                  "invalid character in literal");
    static constexpr unsigned add =
        Lit::is_escape(Char) ? 0u : Lit::digit_width();
    static constexpr unsigned value = lit_size<Lit, Acc + add, Chars...>::value;
};

template <typename Lit, unsigned Init, char... Chars>
inline constexpr unsigned lit_size_v = lit_size<Lit, Init, Chars...>::value;

// ==================== 用户接口 ====================

template <char... Chars>
inline constexpr uint64_t lit_bin_value_v = lit_value_v<lit_bin, 0, Chars...>;

template <char... Chars>
inline constexpr uint64_t lit_oct_value_v = lit_value_v<lit_oct, 0, Chars...>;

template <char... Chars>
inline constexpr uint64_t lit_hex_value_v = lit_value_v<lit_hex, 0, Chars...>;

template <char... Chars>
inline constexpr uint64_t lit_dec_value_v = lit_value_v<lit_dec, 0, Chars...>;

// template <char... Chars>
// inline constexpr unsigned lit_bin_size_v = lit_size_v<lit_bin, 0, Chars...>;

template <char... Chars>
inline constexpr unsigned lit_bin_size_v =
    compute_bit_width(lit_bin_value_v<Chars...>);

template <char... Chars>
inline constexpr unsigned lit_oct_size_v =
    compute_bit_width(lit_oct_value_v<Chars...>);

template <char... Chars>
inline constexpr unsigned lit_hex_size_v =
    compute_bit_width(lit_hex_value_v<Chars...>);

template <char... Chars>
inline constexpr unsigned lit_dec_size_v =
    compute_bit_width(lit_dec_value_v<Chars...>);

#endif // CH_LNODE_LITERAL_H
