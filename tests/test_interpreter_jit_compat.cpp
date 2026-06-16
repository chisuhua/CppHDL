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
// ============================================================================
class CompatDut : public ch::Component {
 public:
  __io(ch_in<ch_uint<8>>  a;
       ch_in<ch_uint<8>>  b;
       ch_in<ch_uint<8>>  sh;     // shift amount (used for SHL)
       ch_out<ch_uint<8>> sum;    // a + b
       ch_out<ch_uint<8>> diff;   // a - b
       ch_out<ch_uint<8>> prod;   // a * b
       ch_out<ch_uint<8>> shifted;  // a << sh
       ch_out<ch_uint<8>> neg;)   // -a (two's complement)
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

// Input vector: [a, b, sh]
using InVec = std::array<uint64_t, 3>;
// Output vector: [sum, diff, prod, shifted, neg]
using OutVec = std::array<uint64_t, 5>;

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
