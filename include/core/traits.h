// include/traits.h
#ifndef TRAITS_H
#define TRAITS_H

#include <type_traits>
#include <cstdint>
#include <bit>

namespace ch::core {

// --- Primary template (fallback) ---
template<typename T, typename Enable = void>
struct ch_width_impl {
    static constexpr unsigned value = 0; // Unknown type
};

// --- Specialization: Standard integral types ---
template<typename T>
struct ch_width_impl<T, std::enable_if_t<std::is_integral_v<T>>> {
    static constexpr unsigned value = 32; // Default width for int, etc.
};


template<>
struct ch_width_impl<int> {
    static constexpr unsigned value = sizeof(int) * 8;
};

template<>
struct ch_width_impl<long> {
    static constexpr unsigned value = sizeof(long) * 8;
};

// --- Public variable template ---
template<typename T>
inline constexpr unsigned ch_width_v = ch_width_impl<std::remove_cv_t<T>>::value;

// 计算值的位宽
inline constexpr unsigned bit_width(uint64_t value) {
    return value == 0 ? 1 : static_cast<unsigned>(64 - std::countl_zero(value));
}

// 检查是否为2的幂
constexpr bool is_power_of_two(uint64_t value) {
    return (value != 0) && ((value & (value - 1)) == 0);
}

} // namespace ch::core

#endif // TRAITS_H
