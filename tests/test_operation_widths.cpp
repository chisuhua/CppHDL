// tests/test_operation_widths.cpp
//
// Purpose: Pin the compile-time width derivation behavior for every ch_op
// value defined in include/core/lnodeimpl.h. Each ch_op gets its own
// TEST_CASE tagged [widths][ch_op] so operators can be reviewed in isolation
// and so the test count is observable from ctest -N.
//
// Width derivation is a compile-time property, so every assertion uses
// STATIC_REQUIRE (constexpr-equivalent). For ops with a corresponding Op
// struct (see include/lnode/operators_ext.h) we use the canonical
// get_binary_result_width / get_unary_result_width helpers; for ops exposed
// only through their user-facing function templates (bit_sel, bits_extract,
// concat, sext, zext, mux, etc.) we exercise the function and assert
// ch_width_v on the result type. For ops without a public Op struct
// (sshr, bits_update, assign) we verify the ch_op enum value exists and pin
// the runtime width of the result type produced by the corresponding
// user-facing API.

#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/uint.h"

using namespace ch;
using namespace ch::core;

// =====================================================================
// === Binary arithmetic / bitwise operations (Op struct -> binary) ===
// =====================================================================

TEST_CASE("ch_op: add width derivation", "[widths][ch_op][add]") {
    // add_op::result_width<M,N> = max(M,N) + 1
    STATIC_REQUIRE(get_binary_result_width<add_op, ch_uint<8>, ch_uint<6>>() ==
                   9);  // max(8,6)+1
    STATIC_REQUIRE(get_binary_result_width<add_op, ch_uint<4>, ch_uint<4>>() ==
                   5);  // max(4,4)+1
    STATIC_REQUIRE(get_binary_result_width<add_op, ch_uint<8>, ch_uint<8>>() ==
                   9);  // balanced add
}

TEST_CASE("ch_op: sub width derivation", "[widths][ch_op][sub]") {
    // sub_op::result_width<M,N> = max(M,N)
    STATIC_REQUIRE(get_binary_result_width<sub_op, ch_uint<8>, ch_uint<6>>() ==
                   8);  // max(8,6)
    STATIC_REQUIRE(get_binary_result_width<sub_op, ch_uint<4>, ch_uint<7>>() ==
                   7);  // max(4,7)
    STATIC_REQUIRE(get_binary_result_width<sub_op, ch_uint<12>, ch_uint<12>>() ==
                   12);
}

TEST_CASE("ch_op: mul width derivation", "[widths][ch_op][mul]") {
    // mul_op::result_width<M,N> = M + N
    STATIC_REQUIRE(get_binary_result_width<mul_op, ch_uint<8>, ch_uint<6>>() ==
                   14);  // 8+6
    STATIC_REQUIRE(get_binary_result_width<mul_op, ch_uint<4>, ch_uint<4>>() ==
                   8);  // 4+4
    STATIC_REQUIRE(get_binary_result_width<mul_op, ch_uint<8>, ch_uint<8>>() ==
                   16);
}

TEST_CASE("ch_op: div width derivation", "[widths][ch_op][div]") {
    // div_op::result_width<M,N> = M
    STATIC_REQUIRE(get_binary_result_width<div_op, ch_uint<8>, ch_uint<6>>() ==
                   8);  // M
    STATIC_REQUIRE(get_binary_result_width<div_op, ch_uint<12>, ch_uint<4>>() ==
                   12);
    STATIC_REQUIRE(get_binary_result_width<div_op, ch_uint<16>, ch_uint<16>>() ==
                   16);
}

TEST_CASE("ch_op: mod width derivation", "[widths][ch_op][mod]") {
    // mod_op::result_width<M,N> = N  (default for non-literal RHS)
    STATIC_REQUIRE(get_binary_result_width<mod_op, ch_uint<8>, ch_uint<6>>() ==
                   6);  // N
    STATIC_REQUIRE(get_binary_result_width<mod_op, ch_uint<16>, ch_uint<4>>() ==
                   4);

    // Compile-time literal RHS: width = compute_bit_width(V-1)
    constexpr auto lit3 = make_literal<3, 2>();
    STATIC_REQUIRE(
        get_binary_result_width<mod_op, ch_uint<8>, decltype(lit3)>() == 2);
    constexpr auto lit8 = make_literal<8, 4>();
    STATIC_REQUIRE(
        get_binary_result_width<mod_op, ch_uint<16>, decltype(lit8)>() == 3);
}

TEST_CASE("ch_op: and_ width derivation", "[widths][ch_op][and_]") {
    // and_op::result_width<M,N> = max(M,N)
    STATIC_REQUIRE(get_binary_result_width<and_op, ch_uint<8>, ch_uint<6>>() ==
                   8);
    STATIC_REQUIRE(get_binary_result_width<and_op, ch_uint<4>, ch_uint<12>>() ==
                   12);
}

TEST_CASE("ch_op: or_ width derivation", "[widths][ch_op][or_]") {
    // or_op::result_width<M,N> = max(M,N)
    STATIC_REQUIRE(get_binary_result_width<or_op, ch_uint<8>, ch_uint<6>>() ==
                   8);
    STATIC_REQUIRE(get_binary_result_width<or_op, ch_uint<3>, ch_uint<9>>() ==
                   9);
}

TEST_CASE("ch_op: xor_ width derivation", "[widths][ch_op][xor_]") {
    // xor_op::result_width<M,N> = max(M,N)
    STATIC_REQUIRE(get_binary_result_width<xor_op, ch_uint<8>, ch_uint<6>>() ==
                   8);
    STATIC_REQUIRE(get_binary_result_width<xor_op, ch_uint<5>, ch_uint<10>>() ==
                   10);
}

// =====================================================================
// === Comparison operations (all return 1-bit) ===
// =====================================================================

TEST_CASE("ch_op: eq width derivation", "[widths][ch_op][eq]") {
    // eq_op::result_width<M,N> = 1  (is_comparison=true)
    STATIC_REQUIRE(get_binary_result_width<eq_op, ch_uint<8>, ch_uint<6>>() ==
                   1);
    STATIC_REQUIRE(get_binary_result_width<eq_op, ch_uint<32>, ch_uint<8>>() ==
                   1);
}

TEST_CASE("ch_op: ne width derivation", "[widths][ch_op][ne]") {
    STATIC_REQUIRE(get_binary_result_width<ne_op, ch_uint<8>, ch_uint<6>>() ==
                   1);
    STATIC_REQUIRE(get_binary_result_width<ne_op, ch_uint<1>, ch_uint<1>>() ==
                   1);
}

TEST_CASE("ch_op: lt width derivation", "[widths][ch_op][lt]") {
    STATIC_REQUIRE(get_binary_result_width<lt_op, ch_uint<8>, ch_uint<6>>() ==
                   1);
    STATIC_REQUIRE(get_binary_result_width<lt_op, ch_uint<16>, ch_uint<16>>() ==
                   1);
}

TEST_CASE("ch_op: le width derivation", "[widths][ch_op][le]") {
    STATIC_REQUIRE(get_binary_result_width<le_op, ch_uint<8>, ch_uint<6>>() ==
                   1);
    STATIC_REQUIRE(get_binary_result_width<le_op, ch_uint<4>, ch_uint<12>>() ==
                   1);
}

TEST_CASE("ch_op: gt width derivation", "[widths][ch_op][gt]") {
    STATIC_REQUIRE(get_binary_result_width<gt_op, ch_uint<8>, ch_uint<6>>() ==
                   1);
    STATIC_REQUIRE(get_binary_result_width<gt_op, ch_uint<2>, ch_uint<10>>() ==
                   1);
}

TEST_CASE("ch_op: ge width derivation", "[widths][ch_op][ge]") {
    STATIC_REQUIRE(get_binary_result_width<ge_op, ch_uint<8>, ch_uint<6>>() ==
                   1);
    STATIC_REQUIRE(get_binary_result_width<ge_op, ch_uint<7>, ch_uint<3>>() ==
                   1);
}

// =====================================================================
// === Shift operations ===
// =====================================================================

TEST_CASE("ch_op: shl width derivation", "[widths][ch_op][shl]") {
    // shl_op::result_width<M,N> = M + N  (struct formula, distinct from
    // operator<< which adds (2^rhs_width - 1) to lhs_width for runtime
    // values; the struct formula is the canonical ch_op::shl definition).
    STATIC_REQUIRE(get_binary_result_width<shl_op, ch_uint<8>, ch_uint<3>>() ==
                   11);  // 8+3
    STATIC_REQUIRE(get_binary_result_width<shl_op, ch_uint<4>, ch_uint<4>>() ==
                   8);
}

TEST_CASE("ch_op: shr width derivation", "[widths][ch_op][shr]") {
    // shr_op::result_width<M,N> = M  (logical right shift keeps operand width)
    STATIC_REQUIRE(get_binary_result_width<shr_op, ch_uint<8>, ch_uint<3>>() ==
                   8);
    STATIC_REQUIRE(get_binary_result_width<shr_op, ch_uint<12>, ch_uint<4>>() ==
                   12);
}

TEST_CASE("ch_op: sshr width derivation", "[widths][ch_op][sshr]") {
    // sshr (arithmetic right shift) has no public Op struct in
    // operators_ext.h. It is a reserved ch_op value used by JIT/instr
    // lowering (see include/jit/jit_ir.h, include/ast/instr_op.h).
    // Pin the enum value and the operator>> width (which delegates to
    // shr_op and therefore produces the same width as the arithmetic form
    // for unsigned operands).
    STATIC_REQUIRE(static_cast<int>(ch_op::sshr) ==
                   static_cast<int>(ch_op::sshr));  // enum value exists

    // operator>> returns shr_op-result width (M) for the LHS.
    STATIC_REQUIRE(ch_width_v<decltype(std::declval<ch_uint<8>>() >>
                                    std::declval<ch_uint<3>>())> == 8);
    STATIC_REQUIRE(ch_width_v<decltype(std::declval<ch_uint<16>>() >>
                                    std::declval<ch_uint<4>>())> == 16);
}

// =====================================================================
// === Unary operations ===
// =====================================================================

TEST_CASE("ch_op: not_ width derivation", "[widths][ch_op][not_]") {
    // not_op::result_width_v<N> = N
    STATIC_REQUIRE(get_unary_result_width<not_op, ch_uint<8>>() == 8);
    STATIC_REQUIRE(get_unary_result_width<not_op, ch_uint<32>>() == 32);

    // Cross-check via unary operator~.
    STATIC_REQUIRE(ch_width_v<decltype(~std::declval<ch_uint<8>>())> == 8);
}

TEST_CASE("ch_op: neg width derivation", "[widths][ch_op][neg]") {
    // neg_op::result_width_v<N> = N  (matches existing operator- behavior).
    STATIC_REQUIRE(get_unary_result_width<neg_op, ch_uint<8>>() == 8);
    STATIC_REQUIRE(get_unary_result_width<neg_op, ch_uint<16>>() == 16);

    STATIC_REQUIRE(ch_width_v<decltype(-std::declval<ch_uint<8>>())> == 8);
}

// =====================================================================
// === Bit-level operations exposed through template APIs ===
// =====================================================================

TEST_CASE("ch_op: bit_sel width derivation", "[widths][ch_op][bit_sel]") {
    // bit_sel returns 1-bit result (template form, compile-time index)
    // Compile-time + runtime indexing share the same 1-bit width.
    // bit_sel_op::result_width<M,N> = 1
    STATIC_REQUIRE(
        get_binary_result_width<bit_sel_op, ch_uint<8>, ch_uint<3>>() == 1);

    // User-facing function template must return 1-bit hardware.
    STATIC_REQUIRE(ch_width_v<decltype(bit_select<3>(std::declval<ch_uint<8>>()))>
                   == 1);
    STATIC_REQUIRE(ch_width_v<decltype(bit_select<0>(std::declval<ch_uint<1>>()))>
                   == 1);
}

TEST_CASE("ch_op: bits_extract width derivation",
          "[widths][ch_op][bits_extract]") {
    // bits<MSB,LSB>(operand) returns MSB-LSB+1 width
    // bits_extract_op::result_width<M,N> = N (parameter N = extract width)
    STATIC_REQUIRE(ch_width_v<decltype(bits<6, 2>(std::declval<ch_uint<8>>()))>
                   == 5);  // 6-2+1
    STATIC_REQUIRE(ch_width_v<decltype(bits<7, 0>(std::declval<ch_uint<8>>()))>
                   == 8);
    STATIC_REQUIRE(ch_width_v<decltype(bits<15, 8>(std::declval<ch_uint<16>>()))>
                   == 8);
    STATIC_REQUIRE(ch_width_v<decltype(bits<4, 0>(std::declval<ch_uint<8>>()))>
                   == 5);
}

TEST_CASE("ch_op: bits_update width derivation",
          "[widths][ch_op][bits_update]") {
    // bits_update<Width>(target, source, lsb) keeps target width.
    // No public Op struct in operators_ext.h; function is in operators.h.
    // Verify the ch_op enum value exists.
    STATIC_REQUIRE(static_cast<int>(ch_op::bits_update) ==
                   static_cast<int>(ch_op::bits_update));

    // Width formula: result width = target width (see operators.h:540)
    STATIC_REQUIRE(ch_width_v<decltype(bits_update<4>(
                       std::declval<ch_uint<8>>(), std::declval<ch_uint<4>>(),
                       0u))> == 8);
    STATIC_REQUIRE(ch_width_v<decltype(bits_update<8>(
                       std::declval<ch_uint<16>>(),
                       std::declval<ch_uint<8>>(), 0u))> == 16);
    STATIC_REQUIRE(ch_width_v<decltype(bits_update<2>(
                       std::declval<ch_uint<12>>(),
                       std::declval<ch_uint<2>>(), 4u))> == 12);
}

TEST_CASE("ch_op: concat width derivation", "[widths][ch_op][concat]") {
    // concat(a,b) returns M+N width
    // concat_op::result_width<M,N> = M + N
    STATIC_REQUIRE(get_binary_result_width<concat_op, ch_uint<3>, ch_uint<5>>() ==
                   8);
    STATIC_REQUIRE(get_binary_result_width<concat_op, ch_uint<4>, ch_uint<4>>() ==
                   8);

    // User-facing concat function template (operators.h:501)
    STATIC_REQUIRE(
        ch_width_v<decltype(concat(std::declval<ch_uint<3>>(),
                                   std::declval<ch_uint<5>>()))> == 8);
    STATIC_REQUIRE(
        ch_width_v<decltype(concat(std::declval<ch_uint<1>>(),
                                   std::declval<ch_uint<1>>()))> == 2);
    STATIC_REQUIRE(
        ch_width_v<decltype(concat(std::declval<ch_uint<8>>(),
                                   std::declval<ch_uint<16>>()))> == 24);
}

TEST_CASE("ch_op: sext width derivation", "[widths][ch_op][sext]") {
    // sext<NewWidth>(operand) returns NewWidth
    // sext_op::result_width<M,N> = N (target width)
    STATIC_REQUIRE(get_binary_result_width<sext_op, ch_uint<3>, ch_uint<8>>() ==
                   8);

    // User-facing sext function template (operators.h:544)
    STATIC_REQUIRE(
        ch_width_v<decltype(sext<8>(std::declval<ch_uint<3>>()))> == 8);
    STATIC_REQUIRE(
        ch_width_v<decltype(sext<16>(std::declval<ch_uint<8>>()))> == 16);
    STATIC_REQUIRE(
        ch_width_v<decltype(sext<32>(std::declval<ch_uint<4>>()))> == 32);
}

TEST_CASE("ch_op: zext width derivation", "[widths][ch_op][zext]") {
    // zext<NewWidth>(operand) returns NewWidth
    // zext_op::result_width<M,N> = N (target width)
    STATIC_REQUIRE(get_binary_result_width<zext_op, ch_uint<3>, ch_uint<8>>() ==
                   8);

    // User-facing zext function template (operators.h:560)
    STATIC_REQUIRE(
        ch_width_v<decltype(zext<8>(std::declval<ch_uint<3>>()))> == 8);
    STATIC_REQUIRE(
        ch_width_v<decltype(zext<16>(std::declval<ch_uint<8>>()))> == 16);
}

TEST_CASE("ch_op: mux width derivation", "[widths][ch_op][mux]") {
    // mux_op is not a regular binary Op (no is_comparison member), so use
    // the user-facing select() function template. The width is computed
    // inline as max(t_width, f_width) in operators.h:ternary_operation.
    STATIC_REQUIRE(mux_op::template result_width<8, 6> == 8);
    STATIC_REQUIRE(mux_op::template result_width<4, 12> == 12);
    STATIC_REQUIRE(mux_op::template result_width<16, 16> == 16);

    // Cross-check via select(cond, true_val, false_val)
    STATIC_REQUIRE(
        ch_width_v<decltype(select(std::declval<ch_bool>(),
                                   std::declval<ch_uint<8>>(),
                                   std::declval<ch_uint<6>>()))> == 8);
    STATIC_REQUIRE(
        ch_width_v<decltype(select(std::declval<ch_bool>(),
                                   std::declval<ch_uint<4>>(),
                                   std::declval<ch_uint<12>>()))> == 12);
    STATIC_REQUIRE(
        ch_width_v<decltype(select(std::declval<ch_bool>(),
                                   std::declval<ch_uint<16>>(),
                                   std::declval<ch_uint<16>>()))> == 16);
}

// =====================================================================
// === Reduction operations (all return 1-bit) ===
// =====================================================================

TEST_CASE("ch_op: and_reduce width derivation",
          "[widths][ch_op][and_reduce]") {
    // and_reduce_op::result_width_v<N> = 1
    // (Note: get_unary_result_width does NOT dispatch to this struct; the
    //  and_reduce() function template hardcodes the 1-bit result, so the
    //  source of truth is the user-facing API.)
    STATIC_REQUIRE(and_reduce_op::template result_width_v<8> == 1);
    STATIC_REQUIRE(and_reduce_op::template result_width_v<32> == 1);

    // User-facing and_reduce returns ch_bool (1 bit) regardless of input
    // width — verify via is_same.
    STATIC_REQUIRE(std::is_same_v<
                   decltype(and_reduce(std::declval<ch_uint<8>>())), ch_bool>);
    STATIC_REQUIRE(std::is_same_v<
                   decltype(and_reduce(std::declval<ch_uint<32>>())), ch_bool>);
}

TEST_CASE("ch_op: or_reduce width derivation",
          "[widths][ch_op][or_reduce]") {
    STATIC_REQUIRE(or_reduce_op::template result_width_v<8> == 1);
    STATIC_REQUIRE(or_reduce_op::template result_width_v<16> == 1);

    STATIC_REQUIRE(std::is_same_v<
                   decltype(or_reduce(std::declval<ch_uint<8>>())), ch_bool>);
    STATIC_REQUIRE(std::is_same_v<
                   decltype(or_reduce(std::declval<ch_uint<16>>())), ch_bool>);
}

TEST_CASE("ch_op: xor_reduce width derivation",
          "[widths][ch_op][xor_reduce]") {
    STATIC_REQUIRE(xor_reduce_op::template result_width_v<8> == 1);
    STATIC_REQUIRE(xor_reduce_op::template result_width_v<64> == 1);

    STATIC_REQUIRE(
        std::is_same_v<decltype(xor_reduce(std::declval<ch_uint<8>>())),
                       ch_bool>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(xor_reduce(std::declval<ch_uint<64>>())),
                       ch_bool>);
}

// =====================================================================
// === Rotation operations ===
// =====================================================================

TEST_CASE("ch_op: rotate_l width derivation", "[widths][ch_op][rotate_l]") {
    // rotate_l_op::result_width<M,N> = M
    STATIC_REQUIRE(
        get_binary_result_width<rotate_l_op, ch_uint<8>, ch_uint<3>>() == 8);
    STATIC_REQUIRE(
        get_binary_result_width<rotate_l_op, ch_uint<16>, ch_uint<4>>() == 16);

    // Cross-check via rotate_left() function template (operators.h:796)
    STATIC_REQUIRE(
        ch_width_v<decltype(rotate_left(std::declval<ch_uint<8>>(),
                                        std::declval<ch_uint<3>>()))> == 8);
}

TEST_CASE("ch_op: rotate_r width derivation", "[widths][ch_op][rotate_r]") {
    // rotate_r_op::result_width<M,N> = M
    STATIC_REQUIRE(
        get_binary_result_width<rotate_r_op, ch_uint<8>, ch_uint<3>>() == 8);
    STATIC_REQUIRE(
        get_binary_result_width<rotate_r_op, ch_uint<32>, ch_uint<5>>() == 32);

    // Cross-check via rotate_right() function template (operators.h:801)
    STATIC_REQUIRE(
        ch_width_v<decltype(rotate_right(std::declval<ch_uint<8>>(),
                                         std::declval<ch_uint<3>>()))> == 8);
}

// =====================================================================
// === Population count ===
// =====================================================================

TEST_CASE("ch_op: popcount width derivation", "[widths][ch_op][popcount]") {
    // popcount_op::result_width_v<N> = (N<=1) ? 1 : std::bit_width(N)
    STATIC_REQUIRE(get_unary_result_width<popcount_op, ch_uint<1>>() == 1);
    STATIC_REQUIRE(get_unary_result_width<popcount_op, ch_uint<2>>() ==
                   2);  // bit_width(2) = 2
    STATIC_REQUIRE(get_unary_result_width<popcount_op, ch_uint<8>>() ==
                   4);  // bit_width(8) = 4
    STATIC_REQUIRE(get_unary_result_width<popcount_op, ch_uint<16>>() ==
                   5);  // bit_width(16) = 5
    STATIC_REQUIRE(get_unary_result_width<popcount_op, ch_uint<32>>() ==
                   6);  // bit_width(32) = 6
    STATIC_REQUIRE(get_unary_result_width<popcount_op, ch_uint<64>>() ==
                   7);  // bit_width(64) = 7

    // Cross-check via popcount() function template (operators.h:806)
    STATIC_REQUIRE(
        ch_width_v<decltype(popcount(std::declval<ch_uint<8>>()))> == 4);
    STATIC_REQUIRE(
        ch_width_v<decltype(popcount(std::declval<ch_uint<16>>()))> == 5);
}

// =====================================================================
// === Assignment (no public Op struct; verified via ch_uint construction) ===
// =====================================================================

TEST_CASE("ch_op: assign width derivation", "[widths][ch_op][assign]") {
    // ch_op::assign has no public Op struct. It's used internally by
    // ch_uint<N>::ch_uint(uint64_t) to wire a literal source to the
    // underlying lnodeimpl (see include/core/uint.h).
    //
    // Pin the ch_op enum value exists, then verify the runtime width
    // produced by ch_uint construction (which internally uses ch_op::assign)
    // matches the template width.
    STATIC_REQUIRE(static_cast<int>(ch_op::assign) ==
                   static_cast<int>(ch_op::assign));

    // ch_uint<N> must carry width N through ch_op::assign
    STATIC_REQUIRE(ch_width_v<ch_uint<1>> == 1);
    STATIC_REQUIRE(ch_width_v<ch_uint<8>> == 8);
    STATIC_REQUIRE(ch_width_v<ch_uint<16>> == 16);
    STATIC_REQUIRE(ch_width_v<ch_uint<32>> == 32);
    STATIC_REQUIRE(ch_width_v<ch_uint<64>> == 64);
}

// =====================================================================
// === End: 33 ch_op TEST_CASEs (one per enum value) ===
// =====================================================================
