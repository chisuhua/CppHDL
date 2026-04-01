/**
 * @file converter.h
 * @brief CppHDL 数据格式转换工具
 * 
 * 提供 BCD/二进制、字符串等格式之间的转换功能
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <bit>
#include <type_traits>

namespace ch::utils {

// ============================================================================
// BCD (Binary-Coded Decimal) 转换
// ============================================================================

/**
 * @brief BCD 转二进制
 * 
 * @param bcd BCD 编码值（每 4 位表示一个十进制数字）
 * @return uint32_t 二进制值
 * 
 * @example
 * bcd_to_binary(0x1234) = 1234
 */
inline uint32_t bcd_to_binary(uint32_t bcd) {
    uint32_t result = 0;
    uint32_t multiplier = 1;
    
    while (bcd > 0) {
        uint32_t digit = bcd & 0xF;
        result += digit * multiplier;
        bcd >>= 4;
        multiplier *= 10;
    }
    
    return result;
}

/**
 * @brief 二进制转 BCD
 * 
 * @param binary 二进制值
 * @return uint32_t BCD 编码值
 * 
 * @example
 * binary_to_bcd(1234) = 0x1234
 */
inline uint32_t binary_to_bcd(uint32_t binary) {
    uint32_t result = 0;
    uint32_t multiplier = 1;
    
    while (binary > 0) {
        uint32_t digit = binary % 10;
        result |= (digit * multiplier);
        binary /= 10;
        multiplier <<= 4;
    }
    
    return result;
}

/**
 * @brief 十进制字符串转 BCD
 * 
 * @param str 十进制字符串（如 "1234"）
 * @return uint32_t BCD 编码值
 * 
 * @example
 * string_to_bcd("1234") = 0x1234
 */
inline uint32_t string_to_bcd(const std::string& str) {
    uint32_t result = 0;
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            result = (result << 4) | (c - '0');
        }
    }
    return result;
}

/**
 * @brief BCD 转十进制字符串
 * 
 * @param bcd BCD 编码值
 * @return std::string 十进制字符串
 * 
 * @example
 * bcd_to_string(0x1234) = "1234"
 */
inline std::string bcd_to_string(uint32_t bcd) {
    std::string result;
    while (bcd > 0) {
        uint32_t digit = bcd & 0xF;
        result.insert(result.begin(), '0' + digit);
        bcd >>= 4;
    }
    return result.empty() ? "0" : result;
}

// ============================================================================
// 位宽计算工具
// ============================================================================

/**
 * @brief 编译时计算值的最小位宽
 * 
 * @tparam T 值类型
 * @param value 要计算的值
 * @return constexpr uint32_t 位宽
 * 
 * @example
 * compute_bit_width(0) = 1
 * compute_bit_width(1) = 1
 * compute_bit_width(255) = 8
 * compute_bit_width(256) = 9
 */
template <typename T>
constexpr uint32_t compute_bit_width(T value) {
    if (value == 0) return 1;
    
    if constexpr (std::is_integral_v<T>) {
        using unsigned_t = std::make_unsigned_t<T>;
        return static_cast<uint32_t>(std::bit_width(static_cast<unsigned_t>(value)));
    } else {
        // 非整数类型返回 64
        return 64;
    }
}

/**
 * @brief 编译时常量位宽计算
 * 
 * @tparam Value 编译时常数值
 * @return constexpr uint32_t 位宽
 */
template <uint64_t Value>
struct const_bit_width {
    static constexpr uint32_t value = [] {
        if constexpr (Value == 0) {
            return 1;
        } else {
            uint32_t width = 0;
            uint64_t temp = Value;
            while (temp > 0) {
                ++width;
                temp >>= 1;
            }
            return width;
        }
    }();
};

template <uint64_t Value>
inline constexpr uint32_t const_bit_width_v = const_bit_width<Value>::value;

// ============================================================================
// 字节序转换
// ============================================================================

/**
 * @brief 主机序转网络序（大端）
 */
inline uint32_t host_to_be(uint32_t value) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap32(value);
#else
    return value;
#endif
}

/**
 * @brief 网络序（大端）转主机序
 */
inline uint32_t be_to_host(uint32_t value) {
    return host_to_be(value);  // 对称操作
}

/**
 * @brief 主机序转小端
 */
inline uint32_t host_to_le(uint32_t value) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap32(value);
#else
    return value;
#endif
}

/**
 * @brief 小端转主机序
 */
inline uint32_t le_to_host(uint32_t value) {
    return host_to_le(value);  // 对称操作
}

// ============================================================================
// 位操作工具
// ============================================================================

/**
 * @brief 设置指定位
 */
template <typename T>
constexpr T set_bit(T value, unsigned position, bool bit) {
    if (bit) {
        return value | (T{1} << position);
    } else {
        return value & ~(T{1} << position);
    }
}

/**
 * @brief 提取指定位
 */
template <typename T>
constexpr bool get_bit(T value, unsigned position) {
    return (value >> position) & T{1};
}

/**
 * @brief 提取位范围 [hi:lo]
 */
template <typename T>
constexpr T extract_bits(T value, unsigned hi, unsigned lo) {
    if (lo > hi) return T{0};
    unsigned width = hi - lo + 1;
    T mask = (width >= sizeof(T) * 8) ? ~T{0} : ((T{1} << width) - 1);
    return (value >> lo) & mask;
}

/**
 * @brief 更新位范围 [hi:lo]
 */
template <typename T, typename U>
constexpr T update_bits(T value, unsigned hi, unsigned lo, U new_bits) {
    if (lo > hi) return value;
    unsigned width = hi - lo + 1;
    T mask = (width >= sizeof(T) * 8) ? ~T{0} : ~(((T{1} << width) - 1) << lo);
    return (value & mask) | (T(new_bits) << lo);
}

} // namespace ch::utils
