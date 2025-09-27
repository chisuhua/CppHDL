// include/traits.h
#ifndef TRAITS_H
#define TRAITS_H

#include <type_traits> // For type traits like std::integral, std::void_t
#include <cstdint>     // For uint32_t

namespace ch { namespace core {

// --- Internal helper to detect if a type T has a static member 'ch_width' ---
// This is kept for potential future use or for types that directly define ch_width,
// but the SFINAE specialization using it is REMOVED to prevent ambiguity.
//template <typename T, typename = void>
//struct has_ch_width_member : std::false_type {};

//template <typename T>
//struct has_ch_width_member<T, std::void_t<decltype(T::ch_width)>> : std::true_type {};


// --- Internal implementation struct for ch_width_v ---
// --- ONLY Primary template (fallback) ---
template <typename T, typename Enable = void>
struct ch_width_impl {
    // --- Fallback: Default width is 0 for unknown/unhandled types ---
    static constexpr unsigned value = 0;
};
// --- END ONLY Primary template ---


// --- ONLY Specialization 1: Standard integral types ---
// Provides a default width for C++ arithmetic types.
template <typename T>
struct ch_width_impl<T, std::enable_if_t<std::is_integral_v<T>>> {
    static constexpr unsigned value = 32; // Default width for integers
    // Alternative using size: static_assert(sizeof(T) * 8 <= 64, "Integral type too large"); return sizeof(T) * 8;
};
// --- END ONLY Specialization 1 ---


// --- Specific Specializations for Wrapper Types (Defined in their respective headers) ---
// These are defined in reg.h, io.h, bitbase.h.
// Examples (these are actually defined elsewhere):
// template <typename T> struct ch_width_impl<ch_uint<N>, void> { ... }; // In bitbase.h
// template <typename T> struct ch_width_impl<ch_reg_impl<T>, void> { ... }; // In reg.h
// template <typename T> struct ch_width_impl<ch_logic_out<T>, void> { ... }; // In io.h
// template <typename T> struct ch_width_impl<ch_logic_in<T>, void> { ... }; // In io.h

// --- Public Variable Template Interface ---
template <typename T>
inline constexpr unsigned ch_width_v = ch_width_impl<std::remove_cv_t<T>>::value;

}} // namespace ch::core

#endif // TRAITS_H
