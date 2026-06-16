// include/core/io_shift.h
// Phase 4: Shift operators for port<,> types
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

// 端口 << 端口
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator<<(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) << get_lnode(rhs);
}

// 端口 >> 端口
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator>>(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) >> get_lnode(rhs);
}

// === 添加对循环移位操作的支持 ===
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto rotate_left(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return rotate_left(get_lnode(lhs), get_lnode(rhs));
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto rotate_right(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return rotate_right(get_lnode(lhs), get_lnode(rhs));
}

// === 添加对popcount操作的支持 ===
template <typename T, typename Dir> auto popcount(const port<T, Dir> &operand) {
    return popcount(get_lnode(operand));
}

} // namespace ch::core
