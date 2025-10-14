
// ==================== 字面量解析辅助结构 ====================

// 二进制字面量解析器
struct lit_bin {
    static constexpr bool is_digit(char c) {
        return c == '0' || c == '1';
    }
    static constexpr bool is_escape(char c) {
        return (c == '\'');
    }
    static constexpr unsigned size(char c, unsigned N) {
        return is_escape(c) ? N : (N + 1);
    }
    static constexpr unsigned chr2int(char c) {
        return static_cast<unsigned>(c - '0');
    }
};

// 八进制字面量解析器
struct lit_oct {
    static constexpr bool is_digit(char c) {
        return c >= '0' && c <= '7';
    }
    static constexpr bool is_escape(char c) {
        return (c == '\'');
    }
    static constexpr unsigned size(char c, unsigned N) {
        return is_escape(c) ? N : (N + 3);
    }
    static constexpr unsigned chr2int(char c) {
        return static_cast<unsigned>(c - '0');
    }
};

// 十六进制字面量解析器
struct lit_hex {
    static constexpr bool is_digit(char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    }
    static constexpr bool is_escape(char c) {
        return (c == '\'' || c == 'x' || c == 'X');
    }
    static constexpr unsigned size(char c, unsigned N) {
        return (c == 'x' || c == 'X') ? 0 : (is_escape(c) ? N : (N + 4));
    }
    static constexpr unsigned chr2int(char c) {
        return static_cast<unsigned>(
            (c >= '0' && c <= '9') ? (c - '0') :
            ((c >= 'A' && c <= 'F') ? (c - 'A' + 10) :
            ((c >= 'a' && c <= 'f') ? (c - 'a' + 10) : 0)));
    }
};

// 十进制字面量解析器
struct lit_dec {
    static constexpr bool is_digit(char c) {
        return c >= '0' && c <= '9';
    }
    static constexpr bool is_escape(char c) {
        return (c == '\''); // 支持数字分隔符，如 1'000'000
    }
    static constexpr unsigned size(char c, unsigned N) {
        return is_escape(c) ? N : (N + 1); // 每个十进制数字占1位？不，这里size用于位宽计算
        // 但十进制不能直接映射到位宽，我们需要特殊处理
    }
    static constexpr unsigned chr2int(char c) {
        return static_cast<unsigned>(c - '0');
    }
};
// ==================== 编译时字面量值解析 ====================

template <typename T, uint64_t Value, char... Chars>
struct lit_value;

// 递归终止条件
template <typename T, uint64_t Value>
struct lit_value<T, Value> {
    static constexpr uint64_t value = Value;
};

// 递归解析字符
template <typename T, uint64_t Value, char Char, char... Chars>
struct lit_value<T, Value, Char, Chars...> {
    static constexpr uint64_t new_value = 
        T::is_escape(Char) ? Value : 
        (Value << (T::size(Char, 0) - 1)) | T::chr2int(Char);
    static constexpr uint64_t value = lit_value<T, new_value, Chars...>::value;
};

// 变量模板简化使用
template <typename T, uint64_t Value, char... Chars>
inline constexpr uint64_t lit_value_v = lit_value<T, Value, Chars...>::value;

// ==================== 编译时字面量解析 ====================

template <typename T, unsigned N, char... Chars>
struct lit_size;

// 递归终止条件
template <typename T, unsigned N>
struct lit_size<T, N> {
    static constexpr unsigned value = N;
};

// 递归解析
template <typename T, unsigned N, char Char, char... Chars>
struct lit_size<T, N, Char, Chars...> {
    static_assert(T::is_digit(Char) || T::is_escape(Char), "invalid literal value");
    static constexpr unsigned new_size = T::is_escape(Char) ? N : T::size(Char, N);
    static constexpr unsigned value = lit_size<T, new_size, Chars...>::value;
};

// 变量模板简化使用
template <typename T, unsigned N, char... Chars>
inline constexpr unsigned lit_size_v = lit_size<T, N, Chars...>::value;

// ==================== 用户友好的字面量大小计算 ====================

template <char... Chars>
inline constexpr unsigned lit_bin_size_v = lit_size_v<lit_bin, 0, Chars...>;

template <char... Chars>
inline constexpr unsigned lit_oct_size_v = lit_size_v<lit_oct, 0, Chars...>;

template <char... Chars>
inline constexpr unsigned lit_hex_size_v = lit_size_v<lit_hex, 0, Chars...>;

// ==================== 用户友好的字面量值计算 ====================

template <char... Chars>
inline constexpr uint64_t lit_bin_value_v = lit_value_v<lit_bin, 0, Chars...>;

template <char... Chars>
inline constexpr uint64_t lit_oct_value_v = lit_value_v<lit_oct, 0, Chars...>;

template <char... Chars>
inline constexpr uint64_t lit_hex_value_v = lit_value_v<lit_hex, 0, Chars...>;

// ==================== 用户字面量操作符 ====================



// ==================== 辅助函数 ====================

// 计算值所需的最小位宽
inline constexpr unsigned bit_width(uint64_t value) {
    if (value == 0) return 1;
    unsigned width = 0;
    uint64_t temp = value;
    while (temp > 0) {
        width++;
        temp >>= 1;
    }
    return width;
}

// ==================== 十进制字面量专用解析 ====================

template <uint64_t Value, char... Chars>
struct lit_dec_value;

// 递归终止
template <uint64_t Value>
struct lit_dec_value<Value> {
    static constexpr uint64_t value = Value;
};

// 递归解析
template <uint64_t Value, char Char, char... Chars>
struct lit_dec_value<Value, Char, Chars...> {
    static constexpr uint64_t new_value = 
        (Char == '\'') ? Value : (Value * 10 + (Char - '0'));
    static constexpr uint64_t value = lit_dec_value<new_value, Chars...>::value;
};

template <char... Chars>
inline constexpr uint64_t lit_dec_value_v = lit_dec_value<0, Chars...>::value;

// 十进制字面量的位宽 = bit_width(数值)
template <char... Chars>
inline constexpr unsigned lit_dec_size_v = bit_width(lit_dec_value_v<Chars...>);
