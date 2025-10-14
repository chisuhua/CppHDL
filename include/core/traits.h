// include/traits.h
#ifndef TRAITS_H
#define TRAITS_H

#include <type_traits>
#include <cstdint>

namespace ch { namespace core {

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

}} // namespace ch::core

#endif // TRAITS_H
