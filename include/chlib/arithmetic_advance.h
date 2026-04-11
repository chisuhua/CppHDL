#ifndef CHLIB_ARITHMETIC_ADVANCE_H
#define CHLIB_ARITHMETIC_ADVANCE_H

#include "ch.hpp"
#include "chlib/logic.h"
#include "chlib/switch.h"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include <cassert>

using namespace ch::core;

namespace chlib {

template <unsigned N> struct CLAResult {
    ch_uint<N> sum;
    ch_bool carry_out;
};

template <unsigned N>
CLAResult<N> carry_lookahead_adder(ch_uint<N> a, ch_uint<N> b,
                                   ch_bool carry_in = ch_bool(false)) {
    static_assert(N > 0, "Carry lookahead adder must have at least 1 bit");

    CLAResult<N> result;
    ch_uint<N + 1> a_ext(a);
    ch_uint<N + 1> b_ext(b);
    auto cin_val = select(carry_in, ch_uint<N + 1>(1_d), ch_uint<N + 1>(0_d));
    ch_uint<N + 1> total = a_ext + b_ext + cin_val;
    result.sum = bits<N, 0>(total);
    result.carry_out = ch_bool(bit_select<N>(total).impl());
    return result;
}

template <unsigned N> struct CSAResult {
    ch_uint<N> sum;
    ch_uint<N> carry;
};

template <unsigned N>
CSAResult<N> carry_save_adder(ch_uint<N> a, ch_uint<N> b, ch_uint<N> c) {
    static_assert(N > 0, "Carry save adder must have at least 1 bit");

    CSAResult<N> result;
    result.sum = a ^ b ^ c;
    ch_uint<N + 1> temp = ch_uint<N + 1>((a & b) | (b & c) | (a & c));
    result.carry = temp << 1_d;

    return result;
}

template <unsigned N>
ch_uint<2 * N> wallace_tree_multiplier(ch_uint<N> a, ch_uint<N> b) {
    static_assert(N > 0, "Wallace tree multiplier must have at least 1 bit");

    ch_uint<2 * N> a_ext(a);
    ch_uint<2 * N> b_ext(b);
    ch_uint<2 * N> result = a_ext * b_ext;
    return result;
}

template <unsigned N>
ch_uint<2 * N> booth_multiplier(ch_uint<N> a, ch_uint<N> b) {
    static_assert(N > 0, "Booth multiplier must have at least 1 bit");

    ch_uint<2 * N> a_ext(a);
    ch_uint<2 * N> b_ext(b);
    ch_uint<2 * N> result = a_ext * b_ext;
    return result;
}

template <unsigned N> struct DividerResult {
    ch_uint<N> quotient;
    ch_uint<N> remainder;
};

template <unsigned N>
DividerResult<N> non_restoring_divider(ch_uint<N> dividend,
                                       ch_uint<N> divisor) {
    static_assert(N > 0, "Non-restoring divider must have at least 1 bit");

    DividerResult<N> result;

    auto is_zero = divisor == 0_d;
    result.quotient = select(is_zero, ~ch_uint<N>(0_d),
                             dividend / select(is_zero, ch_uint<N>(1_d), divisor));
    result.remainder = select(is_zero, ch_uint<N>(0_d),
                              dividend - (result.quotient * divisor));

    return result;
}

template <unsigned N, unsigned Q> struct FixedPointResult {
    ch_uint<N> result;
};

template <unsigned N, unsigned Q>
FixedPointResult<N, Q> fixed_point_adder(ch_uint<N> a, ch_uint<N> b) {
    FixedPointResult<N, Q> result;
    result.result = a + b;
    return result;
}

template <unsigned N, unsigned Q>
FixedPointResult<N, Q> fixed_point_subtractor(ch_uint<N> a, ch_uint<N> b) {
    FixedPointResult<N, Q> result;
    result.result = a - b;
    return result;
}

template <unsigned N, unsigned Q>
FixedPointResult<N, Q> fixed_point_multiplier(ch_uint<N> a, ch_uint<N> b) {
    FixedPointResult<N, Q> result;
    ch_uint<2 * N> temp_result = ch_uint<2 * N>(a) * ch_uint<2 * N>(b);
    result.result = temp_result >> Q;
    return result;
}

template <unsigned N, unsigned Q>
FixedPointResult<N, Q> fixed_point_divider(ch_uint<N> a, ch_uint<N> b) {
    FixedPointResult<N, Q> result;
    if (b == 0_d) {
        result.result = ~ch_uint<N>(0_d);
        return result;
    }
    ch_uint<2 * N> shifted_a = ch_uint<2 * N>(a) << make_literal<Q>();
    ch_uint<2 * N> temp_result = shifted_a / ch_uint<2 * N>(b);
    result.result = temp_result;
    return result;
}

} // namespace chlib

#endif // CHLIB_ARITHMETIC_ADVANCE_H
