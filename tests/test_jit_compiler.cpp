#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/bool.h"
#include "core/context.h"
#include "core/reg.h"
#include "core/uint.h"
#include "simulator.h"

#if __has_include("jit/jit_compiler.h")
#include "jit/jit_compiler.h"
#endif

using namespace ch;
using namespace ch::core;
using namespace ch::core::literals;

TEST_CASE("JIT Compiler availability", "[jit][basic]") {
#if __has_include("jit/jit_compiler.h")
  jit::JitCompiler compiler;
  REQUIRE(compiler.is_available() == true);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler simple context", "[jit][basic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_test_ctx");
  ctx_swap swap(&ctx);

  auto reg = ch_reg<ch_uint<8>>(0_b, "test_reg");
  reg <<= ch_uint<8>(42);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();

  REQUIRE(static_cast<uint64_t>(sim.get_value(reg)) == 42);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler addition", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_add_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(10_d);
  ch_uint<8> b(5_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= a + b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 15);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler subtraction", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_sub_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(20_d);
  ch_uint<8> b(8_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= a - b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 12);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler multiplication", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_mul_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(7_d);
  ch_uint<8> b(6_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= a * b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 42);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler division", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_div_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(100_d);
  ch_uint<8> b(5_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= a / b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 20);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bitwise AND", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_and_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(12_d);
  ch_uint<8> b(10_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= a & b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 8);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bitwise OR", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_or_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(12_d);
  ch_uint<8> b(10_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= a | b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 14);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bitwise XOR", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_xor_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(12_d);
  ch_uint<8> b(10_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= a ^ b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 6);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison LT", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_lt_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(10_d);
  ch_uint<8> b(20_d);
  auto result_reg = ch_reg<ch_bool>(false, "result");

  result_reg <<= a < b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 1);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison GT", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_gt_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(10_d);
  ch_uint<8> b(20_d);
  auto result_reg = ch_reg<ch_bool>(false, "result");

  result_reg <<= a > b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 0);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison EQ", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_eq_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(10_d);
  auto result_reg = ch_reg<ch_bool>(false, "result");

  result_reg <<= (a == 10_d);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 1);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler select true", "[jit][select]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_sel_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(100_d);
  ch_uint<8> b(200_d);
  ch_bool condition(true);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= ch::core::select(condition, a, b);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 100);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler select false", "[jit][select]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_sel2_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(100_d);
  ch_uint<8> b(200_d);
  ch_bool condition(false);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= ch::core::select(condition, a, b);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 200);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler chained operations", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_chain_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(2_d);
  ch_uint<8> b(3_d);
  ch_uint<8> c(4_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= (a + b) * c;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 20);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler register set and get", "[jit][reg]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_reg_ctx");
  ctx_swap swap(&ctx);

  auto counter = ch_reg<ch_uint<8>>(0_b, "counter");
  counter <<= ch_uint<8>(42);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(counter)) == 42);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler sext", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_sext_ctx");
  ctx_swap swap(&ctx);

  ch_uint<4> a(5_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= sext<8>(a);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 5);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler zext", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_zext_ctx");
  ctx_swap swap(&ctx);

  ch_uint<4> a(5_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= zext<8>(a);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 5);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler neg", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_neg_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(5_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= -a;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 251);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bits_update", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_bits_update_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> target(0b11110000);
  ch_uint<4> source(0b1010);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= bits_update<4>(target, source, 2);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 232);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler and_reduce", "[jit][reduce]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_and_reduce_ctx");
  ctx_swap swap(&ctx);
  ch_uint<8> a(0xFF);
  auto result_reg = ch_reg<ch_bool>(false, "result");
  result_reg <<= and_reduce(a);
  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 1);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler or_reduce", "[jit][reduce]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_or_reduce_ctx");
  ctx_swap swap(&ctx);
  ch_uint<8> a(0x00);
  auto result_reg = ch_reg<ch_bool>(false, "result");
  result_reg <<= or_reduce(a);
  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 0);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler xor_reduce", "[jit][reduce]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_xor_reduce_ctx");
  ctx_swap swap(&ctx);
  ch_uint<8> a(0xFF);
  auto result_reg = ch_reg<ch_bool>(false, "result");
  result_reg <<= xor_reduce(a);
  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 0);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler rotate_left", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_rotate_left_ctx");
  ctx_swap swap(&ctx);
  ch_uint<8> a(0b00000001);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");
  result_reg <<= rotate_left(a, ch_uint<8>(1));
  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 2);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler rotate_left wrap", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_rotate_left_wrap_ctx");
  ctx_swap swap(&ctx);
  ch_uint<8> a(0b10000000);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");
  result_reg <<= rotate_left(a, ch_uint<8>(1));
  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 1);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler rotate_right", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_rotate_right_ctx");
  ctx_swap swap(&ctx);
  ch_uint<8> a(0b00000010);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");
  result_reg <<= rotate_right(a, ch_uint<8>(1));
  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 1);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler rotate_right wrap", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_rotate_right_wrap_ctx");
  ctx_swap swap(&ctx);
  ch_uint<8> a(0b00000001);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");
  result_reg <<= rotate_right(a, ch_uint<8>(1));
  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 128);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler popcount", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_popcount_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(0b10101010);
  auto result_reg = ch_reg<ch_uint<4>>(0_b, "result");

  result_reg <<= popcount(a);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);

  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 4);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

// ============================================================================
// 2026-06-05: Tests for ch_op operations previously missing [jit] test coverage.
// See test plan: ch_op::sshr is exercised via internal node_builder API
// because no public `>>>` operator is exposed (operators.h:665-669 commented).
// ============================================================================

TEST_CASE("JIT Compiler modulo", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_mod_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(100_d);
  ch_uint<8> b(7_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= a % b;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 2);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bitwise NOT", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_not_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(0xF0);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= ~a;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 0x0F);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison NE (true)", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_ne_true_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(5_d);
  ch_uint<8> b(3_d);
  auto result_reg = ch_reg<ch_bool>(false, "result");

  result_reg <<= (a != b);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 1);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison NE (false)", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_ne_false_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(5_d);
  ch_uint<8> b(5_d);
  auto result_reg = ch_reg<ch_bool>(false, "result");

  result_reg <<= (a != b);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 0);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison LE", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_le_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(5_d);
  ch_uint<8> b(5_d);
  auto eq_reg = ch_reg<ch_bool>(false, "eq");
  eq_reg <<= (a <= b);

  ch_uint<8> c(5_d);
  ch_uint<8> d(3_d);
  auto gt_reg = ch_reg<ch_bool>(false, "gt");
  gt_reg <<= (c <= d);

  ch_uint<8> e(3_d);
  ch_uint<8> f(5_d);
  auto lt_reg = ch_reg<ch_bool>(false, "lt");
  lt_reg <<= (e <= f);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(eq_reg)) == 1);
  REQUIRE(static_cast<uint64_t>(sim.get_value(gt_reg)) == 0);
  REQUIRE(static_cast<uint64_t>(sim.get_value(lt_reg)) == 1);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison GE", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_ge_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(5_d);
  ch_uint<8> b(5_d);
  auto eq_reg = ch_reg<ch_bool>(false, "eq");
  eq_reg <<= (a >= b);

  ch_uint<8> c(3_d);
  ch_uint<8> d(5_d);
  auto lt_reg = ch_reg<ch_bool>(false, "lt");
  lt_reg <<= (c >= d);

  ch_uint<8> e(5_d);
  ch_uint<8> f(3_d);
  auto gt_reg = ch_reg<ch_bool>(false, "gt");
  gt_reg <<= (e >= f);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(eq_reg)) == 1);
  REQUIRE(static_cast<uint64_t>(sim.get_value(lt_reg)) == 0);
  REQUIRE(static_cast<uint64_t>(sim.get_value(gt_reg)) == 1);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler shift left", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  // Use shl<8>() function form (operators.h:733) instead of operator<<
  // because the latter over-estimates result width for ch_uint RHS.
  context ctx("jit_shl_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(3_d);
  ch_uint<8> b(2_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= shl<8>(a, b);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 12);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler shift right (logical)", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_shr_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(0xFF);
  ch_uint<8> b(4_d);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  result_reg <<= (a >> b);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 0x0F);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler arithmetic shift right (signed)",
          "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  // ch_op::sshr has no public `>>>` operator (commented out at
  // operators.h:665-669). Test via internal node_builder API to
  // directly exercise the JIT SSHRSHN lowering (jit_compiler.cpp:946).
  context ctx("jit_sshr_ctx");
  ctx_swap swap(&ctx);

  using namespace ch::core;
  ch_uint<8> src(0xF8);
  ch_uint<8> shift(2_d);

  lnodeimpl *sshr_node = node_builder::instance().build_operation(
      ch_op::sshr, get_lnode(src), get_lnode(shift), 8,
      /*is_signed=*/true, "sshr_test");

  ch_uint<8> result(sshr_node);
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");
  result_reg <<= result;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 0xFE);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bit select", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_bit_sel_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(0xFF);
  auto result_reg = ch_reg<ch_bool>(false, "result");

  result_reg <<= bit_select<7>(a);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 1);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bits extract", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  context ctx("jit_bits_extract_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> a(0xAB);
  auto result_reg = ch_reg<ch_uint<4>>(0_b, "result");

  result_reg <<= bits<7, 4>(a);

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 0xA);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler assign (wire pass-through)", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
  // ch_op::assign is created when a ch_uint with node_impl_ == nullptr
  // receives a <<= from a ch_uint with a backing node. See
  // include/core/uint.h:111-128 and the comment in
  // src/jit/jit_compiler.cpp explaining why this MUST be JIT native
  // (otherwise downstream JIT-native ops would see stale src0).
  context ctx("jit_assign_ctx");
  ctx_swap swap(&ctx);

  ch_uint<8> source(42_d);
  ch_uint<8> wire;
  auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

  wire <<= source;
  result_reg <<= wire;

  Simulator sim(&ctx);
  REQUIRE(sim.is_jit_compiled() == true);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 42);
#else
  SUCCEED("JIT not enabled - skipping");
#endif
}