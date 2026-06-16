// include/core/io_arithmetic.h
// Phase 4: Arithmetic operators for port<,> types
// Split from io.h to keep each file <150 lines per Phase 4 plan.
#pragma once

#include "core/direction.h"
#include "core/lnode.h"
#include "core/operators.h"
#include "core/traits.h"
#include "core/uint.h"
#include <cstdint>
#include <type_traits>

namespace ch::core {

// 补全算术操作符 (port <-> port)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator+(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) + get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator-(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) - get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator*(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) * get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator/(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) / get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator%(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) % get_lnode(rhs);
}

} // namespace ch::core
