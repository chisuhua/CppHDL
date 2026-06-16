/**
 * @file test_interpreter_jit_compat.cpp
 * @brief Regression test: interpreter and JIT must produce byte-identical
 *        results for arithmetic width overflow cases.
 *
 * Background (see .omo/plans/interpreter-arith-bug-fix.md):
 * Phase 0 of c-class-major-refactor found that interpreter Add::eval resized
 * dst to max(src0, src1) + 1 and Mul::eval resized dst to src0 + src1,
 * while the JIT correctly masked to the compile-time dst bitwidth with
 * `(1<<bw)-1`. Task 1's investigation found two additional ops with the
 * same bug pattern (Shl, Neg). All four were fixed in include/ast/instr_op.h.
 *
 * The subsequent ch_out-truncation-fix (commit `fix(io): truncate ch_out
 * value to compile-time bitwidth`) corrected the architectural divergence at
 * STORE_DATA: ch_out<ch_uint<8>> driven by a 9-bit `Add` result was being
 * silently widened to 9 bits by the interpreter (via `bitvector::operator=`
 * resize) but correctly masked to 8 bits by the JIT (via `store_node` mask).
 * The fix made `instr_output::eval` use `assign_truncate`, matching
 * `instr_proxy::eval` and `instr_reg::eval`.
 *
 * This test pins the new semantics for ADD, SUB, MUL, SHL, and NEG
 * by running each operation in BOTH interpreter and JIT modes and
 * asserting byte-identical output. If either backend silently
 * reintroduces a width bug, the two outputs diverge and the test fails.
 *
 * Coverage: 8-bit overflow + non-overflow cases. The same DUT is run
 * twice (interp / JIT) on a sweep of input vectors. JIT_MIN_NODES=5 is
 * satisfied by the 5-output DUT.
 *
 * Tag: [arith][compat] — runs under ctest -L base; opt-in gate via
 * `ctest -R test_interpreter_jit_compat`.
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "simulator.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/literal.h"
#include "core/reg.h"
#include "core/uint.h"

#include <array>
#include <cstdint>
#include <vector>

using namespace ch;
using namespace ch::core;
using namespace ch::core::literals;

namespace {

// ============================================================================
// DUT: combinational arithmetic width-overflow test (5 ops)
// Sized >= JIT_MIN_NODES=5 so the JIT path actually fires.
//
// Each ch_out's bitwidth matches the operator's NATURAL result width
// (from include/lnode/operators_ext.h) so the ch_out itself does not
// truncate — this test pins the OPERATOR width semantics, not ch_out
// truncation. ch_out truncation is exercised separately by ChOutTruncDut
// in this file.
// ============================================================================
class CompatDut : public ch::Component {
 public:
  __io(ch_in<ch_uint<8>>  a;
       ch_in<ch_uint<8>>  b;
       ch_in<ch_uint<8>>  sh;     // shift amount (used for SHL)
       ch_out<ch_uint<9>>  sum;     // a + b → 9-bit (N+1)
       ch_out<ch_uint<8>>  diff;    // a - b → 8-bit (N)
       ch_out<ch_uint<16>> prod;    // a * b → 16-bit (2N)
       ch_out<ch_uint<16>> shifted; // a << sh → 16-bit (2N for N=8)
       ch_out<ch_uint<8>>  neg;)    // -a → 8-bit (two's complement)
  CompatDut(ch::Component *p = nullptr, const std::string &n = "CompatDut")
      : ch::Component(p, n) {}
  void create_ports() override { new (io_storage_) io_type; }
  void describe() override {
    io().sum     <<= io().a + io().b;
    io().diff    <<= io().a - io().b;
    io().prod    <<= io().a * io().b;
    io().shifted <<= io().a << io().sh;
    io().neg     <<= -io().a;
  }
};

// ============================================================================
// DUT: ch_out truncation test.
//
// Drives a wider arithmetic result (9-bit Add of 8-bit values) into a
// NARROWER ch_out<ch_uint<8>>. The ch_out must truncate to 8 bits, matching
// the SpinalHDL semantic: the ch_out's declared bitwidth is the contract.
// This pins the ch_out truncation behavior across both interpreter and JIT.
// ============================================================================
class ChOutTruncDut : public ch::Component {
 public:
  __io(ch_in<ch_uint<8>>  a;
       ch_in<ch_uint<8>>  b;
       ch_out<ch_uint<8>> out_sum;    // 9-bit Add truncated to 8 bits
       ch_out<ch_uint<8>> out_add3;   // 10-bit (8+8+1) Add truncated to 8
       ch_out<ch_uint<8>> out_mul;    // 16-bit Mul truncated to 8
       ch_out<ch_uint<8>> out_shl;    // 16-bit Shl truncated to 8
       ch_out<ch_uint<8>> out_noop;)  // passthrough — no truncation
  ChOutTruncDut(ch::Component *p = nullptr, const std::string &n = "ChOutTruncDut")
      : ch::Component(p, n) {}
  void create_ports() override { new (io_storage_) io_type; }
  void describe() override {
    io().out_sum  <<= io().a + io().b;          // 9-bit → 8-bit
    io().out_add3 <<= io().a + io().b + io().a;  // 10-bit → 8-bit
    io().out_mul  <<= io().a * io().b;          // 16-bit → 8-bit
    // SHL by constant: result width is N + shift_amount bits. Use shift
    // amount 4 → 12-bit result → 8-bit truncation.
    io().out_shl  <<= io().a << ch_uint<8>(4);
    io().out_noop <<= io().a;                   // 8-bit → 8-bit (no trunc)
  }
};

// Input vector: [a, b, sh]
using InVec = std::array<uint64_t, 3>;
// Output vector: [sum, diff, prod, shifted, neg]
using OutVec = std::array<uint64_t, 5>;
// ChOutTruncDut input vector: [a, b]
using InVec2 = std::array<uint64_t, 2>;
// ChOutTruncDut output vector: [out_sum, out_add3, out_mul, out_shl, out_noop]
using OutVec2 = std::array<uint64_t, 5>;

// Run the DUT for a given input sequence under either backend.
// Fresh ch_device + Simulator per call — no state leaks between modes.
template <typename DutT>
std::vector<OutVec> run_capture(bool jit_enabled,
                                const std::vector<InVec> &inputs) {
  ch_device<DutT> dev;
  Simulator sim(dev.context());
  sim.set_jit_enabled(jit_enabled);
  // First tick triggers JIT compile (untimed).
  sim.tick();

  std::vector<OutVec> captured;
  captured.reserve(inputs.size());
  for (const auto &v : inputs) {
    sim.set_input_value(dev.io().a, v[0]);
    sim.set_input_value(dev.io().b, v[1]);
    sim.set_input_value(dev.io().sh, v[2]);
    sim.tick();
    captured.push_back({
        static_cast<uint64_t>(sim.get_value(dev.io().sum)),
        static_cast<uint64_t>(sim.get_value(dev.io().diff)),
        static_cast<uint64_t>(sim.get_value(dev.io().prod)),
        static_cast<uint64_t>(sim.get_value(dev.io().shifted)),
        static_cast<uint64_t>(sim.get_value(dev.io().neg)),
    });
  }
  return captured;
}

// Run ChOutTruncDut for a given input sequence under either backend.
// Fresh ch_device + Simulator per call.
std::vector<OutVec2> run_capture_chout_trunc(
    bool jit_enabled, const std::vector<InVec2> &inputs) {
  ch_device<ChOutTruncDut> dev;
  Simulator sim(dev.context());
  sim.set_jit_enabled(jit_enabled);
  sim.tick();

  std::vector<OutVec2> captured;
  captured.reserve(inputs.size());
  for (const auto &v : inputs) {
    sim.set_input_value(dev.io().a, v[0]);
    sim.set_input_value(dev.io().b, v[1]);
    sim.tick();
    captured.push_back({
        static_cast<uint64_t>(sim.get_value(dev.io().out_sum)),
        static_cast<uint64_t>(sim.get_value(dev.io().out_add3)),
        static_cast<uint64_t>(sim.get_value(dev.io().out_mul)),
        static_cast<uint64_t>(sim.get_value(dev.io().out_shl)),
        static_cast<uint64_t>(sim.get_value(dev.io().out_noop)),
    });
  }
  return captured;
}

}  // namespace

// ============================================================================
// Test cases
// ============================================================================

// Each case runs the same DUT once as interpreter and once as JIT, on the
// same input sweep, and asserts:
//   1. The two runs are byte-identical (the key invariant).
//   2. Specific values match the expected arithmetic (sanity check that
//      the fix is *correct*, not just *consistent*).
//
// Compile-time width semantics (from include/lnode/operators_ext.h):
//   - ch_uint<N> + ch_uint<N> → ch_uint<N+1>   (SpinalHDL: carry bit kept)
//   - ch_uint<N> * ch_uint<N> → ch_uint<2N>
//   - ch_uint<N> << ch_uint<N> → ch_uint<2N>
//   - ch_uint<N> - ch_uint<N> → ch_uint<N>
//   - -ch_uint<N>              → ch_uint<N>
//
// So overflow values are NOT wrapped to 8 bits — they remain in the
// wider result type (9 for +, 16 for *, 16 for <<). Both interpreter
// and JIT must agree on this. The previous bug had the interpreter
// producing a 9-bit value while the JIT produced an 8-bit value (the
// JIT's `(1<<bw)-1` mask where `bw` was the compile-time dst width).
// After Task 2's fix, both produce the same 9-bit value.
TEST_CASE("ADD (8-bit, widens to 9-bit) — interpreter == JIT",
          "[arith][compat]") {
  std::vector<InVec> inputs = {
      {0xFF, 0x01, 0x00},  // 0xFF + 0x01 = 0x100 (9-bit natural carry)
      {0x80, 0x80, 0x00},  // 0x80 + 0x80 = 0x100
      {0x01, 0xFF, 0x00},  // symmetric
      {0x10, 0x20, 0x00},  // normal: 0x30
      {0x00, 0x00, 0x00},  // edge: 0x00
  };
  auto interp = run_capture<CompatDut>(false, inputs);
  auto jit    = run_capture<CompatDut>(true,  inputs);

  REQUIRE(interp == jit);  // byte-identical

  // 9-bit natural width carries the carry bit out (was 8-bit before fix).
  REQUIRE(interp[0][0] == 0x100);  // sum: 0xFF + 0x01 → 0x100
  REQUIRE(interp[1][0] == 0x100);  // sum: 0x80 + 0x80 → 0x100
  REQUIRE(interp[2][0] == 0x100);
  REQUIRE(interp[3][0] == 0x030);  // sum: 0x10 + 0x20
  REQUIRE(interp[4][0] == 0x000);
}

TEST_CASE("MUL (8-bit, widens to 16-bit) — interpreter == JIT",
          "[arith][compat]") {
  std::vector<InVec> inputs = {
      {0xFF, 0xFF, 0x00},  // 0xFF * 0xFF = 0xFE01
      {0x80, 0x02, 0x00},  // 0x80 * 0x02 = 0x0100
      {0x10, 0x10, 0x00},  // 0x10 * 0x10 = 0x0100
      {0x03, 0x04, 0x00},  // normal: 0x0C
      {0x00, 0xFF, 0x00},  // zero × all
  };
  auto interp = run_capture<CompatDut>(false, inputs);
  auto jit    = run_capture<CompatDut>(true,  inputs);

  REQUIRE(interp == jit);

  REQUIRE(interp[0][2] == 0xFE01);  // prod: 0xFF * 0xFF (no wrap)
  REQUIRE(interp[1][2] == 0x0100);  // prod: 0x80 * 0x02
  REQUIRE(interp[2][2] == 0x0100);
  REQUIRE(interp[3][2] == 0x000C);  // prod: 0x03 * 0x04
  REQUIRE(interp[4][2] == 0x0000);
}

TEST_CASE("SHL (8-bit, widens to 16-bit) — interpreter == JIT",
          "[arith][compat]") {
  std::vector<InVec> inputs = {
      {0x01, 0x00, 0x08},  // 0x01 << 8 = 0x0100
      {0x01, 0x00, 0x09},  // 0x01 << 9 = 0x0200
      {0x80, 0x00, 0x01},  // 0x80 << 1 = 0x0100
      {0x01, 0x00, 0x03},  // 0x01 << 3 = 0x0008
      {0xFF, 0x00, 0x00},  // 0xFF << 0 = 0x00FF
      {0x00, 0x00, 0x08},  // 0 << 8 = 0x00
  };
  auto interp = run_capture<CompatDut>(false, inputs);
  auto jit    = run_capture<CompatDut>(true,  inputs);

  REQUIRE(interp == jit);

  REQUIRE(interp[0][3] == 0x0100);  // shifted: 0x01 << 8
  REQUIRE(interp[1][3] == 0x0200);  // shifted: 0x01 << 9
  REQUIRE(interp[2][3] == 0x0100);  // shifted: 0x80 << 1
  REQUIRE(interp[3][3] == 0x0008);
  REQUIRE(interp[4][3] == 0x00FF);
  REQUIRE(interp[5][3] == 0x0000);
}

TEST_CASE("SUB (8-bit, stays 8-bit) — interpreter == JIT",
          "[arith][compat]") {
  std::vector<InVec> inputs = {
      {0x80, 0x01, 0x00},  // 0x80 - 0x01 = 0x7F
      {0x00, 0x01, 0x00},  // 0x00 - 0x01 = 0xFF (underflow in 8-bit)
      {0xFF, 0xFF, 0x00},  // 0xFF - 0xFF = 0x00
      {0x10, 0x10, 0x00},  // 0x00
      {0x00, 0x00, 0x00},
  };
  auto interp = run_capture<CompatDut>(false, inputs);
  auto jit    = run_capture<CompatDut>(true,  inputs);

  REQUIRE(interp == jit);

  REQUIRE(interp[0][1] == 0x7F);
  REQUIRE(interp[1][1] == 0xFF);
  REQUIRE(interp[2][1] == 0x00);
  REQUIRE(interp[3][1] == 0x00);
  REQUIRE(interp[4][1] == 0x00);
}

TEST_CASE("NEG (8-bit two's complement) — interpreter == JIT",
          "[arith][compat]") {
  std::vector<InVec> inputs = {
      {0x00, 0x00, 0x00},  // -0x00 = 0x00
      {0x01, 0x00, 0x00},  // -0x01 = 0xFF
      {0x80, 0x00, 0x00},  // -0x80 = 0x80 (two's complement MSB)
      {0xFF, 0x00, 0x00},  // -0xFF = 0x01
      {0x7F, 0x00, 0x00},  // -0x7F = 0x81
  };
  auto interp = run_capture<CompatDut>(false, inputs);
  auto jit    = run_capture<CompatDut>(true,  inputs);

  REQUIRE(interp == jit);

  REQUIRE(interp[0][4] == 0x00);
  REQUIRE(interp[1][4] == 0xFF);
  REQUIRE(interp[2][4] == 0x80);  // The classic MSB negation (8-bit)
  REQUIRE(interp[3][4] == 0x01);
  REQUIRE(interp[4][4] == 0x81);
}

// ============================================================================
// ch_out truncation tests
//
// SpinalHDL semantic: the ch_out's declared bitwidth is the contract. A
// wider source value MUST be truncated to fit. The interpreter previously
// resized the ch_out buffer to match the source (a 9-bit Add of 8-bit values
// would silently widen the ch_out<ch_uint<8>> to 9 bits), while the JIT
// correctly masked the stored value to the ch_out's declared bitwidth.
//
// The fix in src/ast/instr_io.cpp (instr_output::eval) replaced
// `*dst_ = *src_` (which resizes via bitvector::operator=) with
// `dst_->assign_truncate(*src_)` (which preserves dst bitwidth and
// truncates the source). This test pins the new behavior in BOTH
// interpreter and JIT modes.
// ============================================================================
TEST_CASE("ch_out truncation (8-bit ch_out, 9/10/12/16-bit sources) — "
          "interpreter == JIT", "[arith][compat][chout]") {
  std::vector<InVec2> inputs = {
      {0xFF, 0x01},  // overflow: 9-bit Add sum = 0x100 → 8-bit trunc → 0x00
      {0x80, 0x80},  // overflow: 9-bit Add sum = 0x100 → 0x00
      {0x01, 0xFF},  // symmetric
      {0x10, 0x20},  // normal:  0x30 (no truncation)
      {0xFF, 0xFF},  // MUL 0xFF*0xFF = 0xFE01 (16-bit) → 8-bit trunc → 0x01
      {0x00, 0x00},  // zeros
  };
  auto interp = run_capture_chout_trunc(false, inputs);
  auto jit    = run_capture_chout_trunc(true,  inputs);

  REQUIRE(interp == jit);  // byte-identical across both backends

  // out_sum = (a + b) & 0xFF — 9-bit Add result truncated to 8 bits
  REQUIRE(interp[0][0] == 0x00);  // 0xFF + 0x01 = 0x100 → 0x00
  REQUIRE(interp[1][0] == 0x00);  // 0x80 + 0x80 = 0x100 → 0x00
  REQUIRE(interp[2][0] == 0x00);  // 0x01 + 0xFF = 0x100 → 0x00
  REQUIRE(interp[3][0] == 0x30);  // 0x10 + 0x20 = 0x30 (no overflow)
  REQUIRE(interp[4][0] == 0xFE);  // 0xFF + 0xFF = 0x1FE → 0xFE
  REQUIRE(interp[5][0] == 0x00);

  // out_add3 = (a + b + a) & 0xFF — 10-bit Add result truncated to 8 bits
  // For 8-bit values the max is 0xFF + 0xFF + 0xFF = 0x2FD (10-bit) → 0xFD
  REQUIRE(interp[0][1] == ((0xFF + 0x01 + 0xFF) & 0xFF));  // = 0xFF
  REQUIRE(interp[1][1] == ((0x80 + 0x80 + 0x80) & 0xFF));  // = 0x80
  REQUIRE(interp[2][1] == ((0x01 + 0xFF + 0x01) & 0xFF));  // = 0x01
  REQUIRE(interp[3][1] == ((0x10 + 0x20 + 0x10) & 0xFF));  // = 0x40
  REQUIRE(interp[4][1] == ((0xFF + 0xFF + 0xFF) & 0xFF));  // = 0xFD
  REQUIRE(interp[5][1] == 0x00);

  // out_mul = (a * b) & 0xFF — 16-bit Mul result truncated to 8 bits
  REQUIRE(interp[0][2] == ((0xFF * 0x01) & 0xFF));  // 0xFF
  REQUIRE(interp[1][2] == ((0x80 * 0x80) & 0xFF));  // 0x00 (0x4000)
  REQUIRE(interp[2][2] == ((0x01 * 0xFF) & 0xFF));  // 0xFF
  REQUIRE(interp[3][2] == ((0x10 * 0x20) & 0xFF));  // 0x00 (0x0200)
  REQUIRE(interp[4][2] == ((0xFF * 0xFF) & 0xFF));  // 0x01 (0xFE01)
  REQUIRE(interp[5][2] == 0x00);

  // out_shl = (a << 4) & 0xFF — 12-bit Shl result truncated to 8 bits
  REQUIRE(interp[0][3] == ((0xFF << 4) & 0xFF));  // 0xF0
  REQUIRE(interp[1][3] == ((0x80 << 4) & 0xFF));  // 0x00 (0x800)
  REQUIRE(interp[2][3] == ((0x01 << 4) & 0xFF));  // 0x10
  REQUIRE(interp[3][3] == ((0x10 << 4) & 0xFF));  // 0x00 (0x100)
  REQUIRE(interp[4][3] == ((0xFF << 4) & 0xFF));  // 0xF0
  REQUIRE(interp[5][3] == 0x00);

  // out_noop = a — passthrough, no truncation
  REQUIRE(interp[0][4] == 0xFF);
  REQUIRE(interp[1][4] == 0x80);
  REQUIRE(interp[2][4] == 0x01);
  REQUIRE(interp[3][4] == 0x10);
  REQUIRE(interp[4][4] == 0xFF);
  REQUIRE(interp[5][4] == 0x00);
}
