// include/core/io_lit.h
// Phase 4: Mixed port<->literal operators + concat with literal
// Split from io.h to keep each file <150 lines per Phase 4 plan.
//
// All overloads here are between port<,> and arithmetic literal types.
#pragma once

#include "core/direction.h"
#include "core/lnode.h"
#include "core/operators.h"
#include "core/traits.h"
#include "core/uint.h"
#include <cstdint>
#include <type_traits>

namespace ch::core {

// 端口与常量的 & 操作符
template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator&(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) & rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator&(Lit lhs, const port<T, Dir> &rhs) {
    return lhs & get_lnode(rhs);
}

// 混合 + 算术操作符
template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator+(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) + rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator+(Lit lhs, const port<T, Dir> &rhs) {
    return lhs + get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator-(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) - rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator-(Lit lhs, const port<T, Dir> &rhs) {
    return lhs - get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator*(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) * rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator*(Lit lhs, const port<T, Dir> &rhs) {
    return lhs * get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator/(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) / rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator/(Lit lhs, const port<T, Dir> &rhs) {
    return lhs / get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator%(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) % rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator%(Lit lhs, const port<T, Dir> &rhs) {
    return lhs % get_lnode(rhs);
}

// 混合 | 算术操作符
template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator|(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) | rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator|(Lit lhs, const port<T, Dir> &rhs) {
    return lhs | get_lnode(rhs);
}

// 混合 ^ 算术操作符
template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator^(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) ^ rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator^(Lit lhs, const port<T, Dir> &rhs) {
    return lhs ^ get_lnode(rhs);
}

// 混合 << 算术操作符
template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator<<(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) << rhs;
}

// 混合 >> 算术操作符
template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator>>(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) >> rhs;
}

// === 添加对concat操作的支持 (port with literal) ===
template <typename T, typename Dir, typename U>
    requires std::is_arithmetic_v<U>
auto concat(const port<T, Dir> &lhs, U rhs) {
    return concat(get_lnode(lhs), rhs);
}

template <typename T, typename Dir, typename U>
    requires std::is_arithmetic_v<U>
auto concat(U lhs, const port<T, Dir> &rhs) {
    return concat(lhs, get_lnode(rhs));
}

} // namespace ch::core
