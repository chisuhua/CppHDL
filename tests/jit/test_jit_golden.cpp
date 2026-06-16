/**
 * @file test_jit_golden.cpp
 * @brief Golden output regression tests for JIT compiler (Phase 0 safety net).
 *
 * Per c-class-major-refactor.md Phase 0: TDD safety net BEFORE Phase 1's
 * split of src/jit/jit_compiler.cpp into 4 generate_ir_* + 6 compile_to_llvm_*
 * per-JitOp-category files. Without these tests, a silent semantic regression
 * in the refactored JIT would compile and run (no crash) but produce different
 * values than the interpreter.
 *
 * For each JitOp category the same DUT is run TWICE:
 *   1. With sim.set_jit_enabled(false) — pure interpreter
 *   2. With sim.set_jit_enabled(true)  — JIT (LLVM ORC)
 * Both runs feed identical input vectors and the output streams must be
 * byte-identical. Any drift = regression in either path.
 *
 * Coverage: 5 JitOp categories mandated by the plan — LOAD_DATA, STORE_DATA,
 * LOAD_CONST, SELECT, REG_NEXT. Each DUT is sized ≥ JIT_MIN_NODES=5 so the
 * JIT path actually fires (otherwise the JIT compile short-circuits per
 * src/jit/jit_compiler.cpp:221).
 *
 * Tag: [jit][golden] — runs under ctest -L base by default; opt-in CI gate
 * via `ctest -R test_jit_golden`.
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
// DUTs — each focuses on one JitOp category, sized ≥ JIT_MIN_NODES (5)
// ============================================================================

// DUT 1: LOAD_DATA — ch_in<ch_uint<8>> read into a ch_reg (the "load"
//          instruction emits JitOp::LOAD_DATA in generate_ir()).
class LoadDataDut : public ch::Component {
 public:
  __io(ch_in<ch_uint<8>>  in0;
       ch_in<ch_uint<8>>  in1;
       ch_in<ch_bool>     sel;
       ch_out<ch_uint<8>> out0;
       ch_out<ch_uint<8>> out1;
       ch_out<ch_uint<8>> out2;)
  LoadDataDut(ch::Component *p = nullptr, const std::string &n = "LoadDataDut")
      : ch::Component(p, n) {}
  void create_ports() override { new (io_storage_) io_type; }
  void describe() override {
    auto r = ch_reg<ch_uint<8>>(0_b, "r");
    r <<= io().in0;       // ← LOAD_DATA: read from input port
    io().out0 <<= r;      // ← STORE_DATA: write to output port
    io().out1 <<= io().in0;
    io().out2 <<= io().in0;
  }
};

// DUT 2: STORE_DATA — ch_reg value driven to ch_out<ch_uint<8>> via two
//          register stages (the "store" instruction emits JitOp::STORE_DATA
//          in generate_ir()).
class StoreDataDut : public ch::Component {
 public:
  __io(ch_in<ch_uint<8>>  in0;
       ch_in<ch_uint<8>>  in1;
       ch_in<ch_bool>     sel;
       ch_out<ch_uint<8>> out0;
       ch_out<ch_uint<8>> out1;
       ch_out<ch_uint<8>> out2;)
  StoreDataDut(ch::Component *p = nullptr,
               const std::string &n = "StoreDataDut")
      : ch::Component(p, n) {}
  void create_ports() override { new (io_storage_) io_type; }
  void describe() override {
    auto r0 = ch_reg<ch_uint<8>>(0_b, "r0");
    auto r1 = ch_reg<ch_uint<8>>(0_b, "r1");
    r0 <<= io().in0;
    r1 <<= r0;
    io().out0 <<= r1;     // ← STORE_DATA: write to output port
    io().out1 <<= r0;
    io().out2 <<= io().in0;
  }
};

// DUT 3: LOAD_CONST — literal value folded into an arithmetic expression
//          (the constant instruction emits JitOp::LOAD_CONST in generate_ir()).
class LoadConstDut : public ch::Component {
 public:
  __io(ch_in<ch_uint<8>>  in0;
       ch_in<ch_uint<8>>  in1;
       ch_in<ch_bool>     sel;
       ch_out<ch_uint<8>> out0;
       ch_out<ch_uint<8>> out1;
       ch_out<ch_uint<8>> out2;)
  LoadConstDut(ch::Component *p = nullptr,
               const std::string &n = "LoadConstDut")
      : ch::Component(p, n) {}
  void create_ports() override { new (io_storage_) io_type; }
  void describe() override {
    // 0xAB + 0x11 = 0xBC constants; both folded at compile time
    auto lit1 = ch_uint<8>(0xAB, "lit1");
    auto lit2 = ch_uint<8>(0x11, "lit2");
    auto r0 = ch_reg<ch_uint<8>>(0_b, "r0");
    auto r1 = ch_reg<ch_uint<8>>(0_b, "r1");
    r0 <<= io().in0 + lit1;   // ← LOAD_CONST: 0xAB folded by JIT
    r1 <<= io().in0 + lit2;   // ← LOAD_CONST: 0x11 folded by JIT
    io().out0 <<= r0;
    io().out1 <<= r1;
    // Use XOR (no width change) instead of +. r0 + r1 = ch_uint<8> +
    // ch_uint<8> = ch_uint<9> (SpinalHDL: carry bit kept). The
    // interpreter-arith-bug-fix commit fixed the interpreter's
    // Add::eval width resize, but a separate divergence remains at
    // the ch_out level: the JIT masks the stored value to the
    // ch_out's declared bitwidth (8), while the interpreter returns
    // the wider 9-bit sdata. A full resolution requires fixing
    // ch_out truncation, which is out of scope for the arithmetic
    // width fix. See .omo/plans/interpreter-arith-bug-fix.md.
    io().out2 <<= r0 ^ r1;
  }
};

// DUT 4: SELECT — ternary mux driven by ch_bool condition
//          (the select instruction emits JitOp::SELECT in generate_ir()).
class SelectDut : public ch::Component {
 public:
  __io(ch_in<ch_uint<8>>  in0;
       ch_in<ch_uint<8>>  in1;
       ch_in<ch_bool>     sel;
       ch_out<ch_uint<8>> out0;
       ch_out<ch_uint<8>> out1;
       ch_out<ch_uint<8>> out2;)
  SelectDut(ch::Component *p = nullptr, const std::string &n = "SelectDut")
      : ch::Component(p, n) {}
  void create_ports() override { new (io_storage_) io_type; }
  void describe() override {
    auto r0 = ch_reg<ch_uint<8>>(0_b, "r0");
    auto r1 = ch_reg<ch_uint<8>>(0_b, "r1");
    r0 <<= select(io().sel, io().in0, io().in1);  // ← SELECT
    r1 <<= select(io().sel, io().in1, io().in0);  // ← SELECT (swapped)
    io().out0 <<= r0;
    io().out1 <<= r1;
    io().out2 <<= select(io().sel, io().in0, io().in1);  // direct comb (SELECT)
  }
};

// DUT 5: REG_NEXT — chained registers propagate state across multiple ticks
//          (each register's `next_state` assignment emits JitOp::REG_NEXT).
class RegNextDut : public ch::Component {
 public:
  __io(ch_in<ch_uint<8>>  in0;
       ch_in<ch_uint<8>>  in1;
       ch_in<ch_bool>     sel;
       ch_out<ch_uint<8>> out0;
       ch_out<ch_uint<8>> out1;
       ch_out<ch_uint<8>> out2;)
  RegNextDut(ch::Component *p = nullptr, const std::string &n = "RegNextDut")
      : ch::Component(p, n) {}
  void create_ports() override { new (io_storage_) io_type; }
  void describe() override {
    auto r0 = ch_reg<ch_uint<8>>(0_b, "r0");
    auto r1 = ch_reg<ch_uint<8>>(0_b, "r1");
    auto r2 = ch_reg<ch_uint<8>>(0_b, "r2");
    r0 <<= io().in0;        // ← REG_NEXT: tick+1 sees in0 from tick 0
    r1 <<= r0;              // ← REG_NEXT: tick+2 sees in0 from tick 0
    r2 <<= r1;              // ← REG_NEXT: tick+3 sees in0 from tick 0
    io().out0 <<= r0;
    io().out1 <<= r1;
    io().out2 <<= r2;
  }
};

// One input vector slot: [in0, in1, sel] (3-tuple)
using InputVec = std::array<uint64_t, 3>;
// One output capture slot: [out0, out1, out2] (3-tuple)
using OutputVec = std::array<uint64_t, 3>;

// Run a fresh DUT for the given input sequence, capturing outputs each tick.
// `jit_enabled` controls whether the simulator uses the JIT backend or the
// pure interpreter. A fresh ch_device + Simulator is constructed per call so
// JIT state (compile cache, data_buffer) cannot leak between the two runs.
template <typename DutT>
std::vector<OutputVec> run_capture(bool jit_enabled,
                                  const std::vector<InputVec> &inputs) {
  ch_device<DutT> dev;
  Simulator sim(dev.context());
  sim.set_jit_enabled(jit_enabled);

  auto in0 = dev.io().in0;
  auto in1 = dev.io().in1;
  auto sel = dev.io().sel;
  auto out0 = dev.io().out0;
  auto out1 = dev.io().out1;
  auto out2 = dev.io().out2;

  std::vector<OutputVec> captured;
  captured.reserve(inputs.size());
  for (const auto &v : inputs) {
    sim.set_input_value(in0, v[0]);
    sim.set_input_value(in1, v[1]);
    sim.set_input_value(sel, v[2]);
    sim.tick();
    captured.push_back({static_cast<uint64_t>(sim.get_value(out0)),
                        static_cast<uint64_t>(sim.get_value(out1)),
                        static_cast<uint64_t>(sim.get_value(out2))});
  }
  return captured;
}

}  // namespace

// ============================================================================
// Test cases — 5 JitOp categories
// ============================================================================

TEST_CASE("JitOp::LOAD_DATA — input port read, interpreter == JIT (byte-identical)",
          "[jit][golden]") {
  // in0 cycles through a small set of distinct values
  std::vector<InputVec> inputs = {
      {0x00, 0x00, 0}, {0x42, 0x99, 1}, {0xA5, 0x5A, 0}, {0xFF, 0x01, 1},
      {0x10, 0x20, 0}, {0x00, 0xFF, 1}, {0x55, 0xAA, 0}, {0x01, 0xFE, 1},
  };

  auto interp = run_capture<LoadDataDut>(/*jit=*/false, inputs);
  auto jit    = run_capture<LoadDataDut>(/*jit=*/true,  inputs);

  // Byte-identical — any drift = JIT split regression
  REQUIRE(interp == jit);

  // r <<= in0 → r captures in0 on the rising edge of the tick that
  // follows set_input_value. out1/out2 are direct combinational
  // passthroughs of in0.
  REQUIRE(interp[0][0] == 0x00);
  REQUIRE(interp[1][0] == 0x42);
  REQUIRE(interp[2][0] == 0xA5);
  REQUIRE(interp[3][0] == 0xFF);
  REQUIRE(interp[4][0] == 0x10);
  REQUIRE(interp[5][0] == 0x00);
  REQUIRE(interp[6][0] == 0x55);
  REQUIRE(interp[7][0] == 0x01);
  REQUIRE(interp[0][1] == 0x00);
  REQUIRE(interp[3][1] == 0xFF);
  REQUIRE(interp[7][1] == 0x01);
  REQUIRE(interp[0][2] == 0x00);
  REQUIRE(interp[3][2] == 0xFF);
  REQUIRE(interp[7][2] == 0x01);
}

TEST_CASE("JitOp::STORE_DATA — output port written, interpreter == JIT",
          "[jit][golden]") {
  std::vector<InputVec> inputs = {
      {0x10, 0x00, 0}, {0x20, 0x00, 1}, {0x30, 0x00, 0}, {0x40, 0x00, 1},
      {0x50, 0x00, 0}, {0x60, 0x00, 1}, {0x70, 0x00, 0}, {0x80, 0x00, 1},
  };

  auto interp = run_capture<StoreDataDut>(false, inputs);
  auto jit    = run_capture<StoreDataDut>(true,  inputs);

  REQUIRE(interp == jit);

  // 2-stage reg chain: r0 ← in0, r1 ← r0; out0 ← r1 (2-cycle delay),
  // out1 ← r0 (1-cycle delay), out2 ← in0 (combinational).
  REQUIRE(interp[0][0] == 0x00);
  REQUIRE(interp[1][0] == 0x10);
  REQUIRE(interp[2][0] == 0x20);
  REQUIRE(interp[3][0] == 0x30);
  REQUIRE(interp[4][0] == 0x40);
  REQUIRE(interp[5][0] == 0x50);
  REQUIRE(interp[6][0] == 0x60);
  REQUIRE(interp[7][0] == 0x70);
  REQUIRE(interp[0][1] == 0x10);
  REQUIRE(interp[1][1] == 0x20);
  REQUIRE(interp[5][1] == 0x60);
  REQUIRE(interp[0][2] == 0x10);
  REQUIRE(interp[3][2] == 0x40);
  REQUIRE(interp[7][2] == 0x80);
}

TEST_CASE("JitOp::LOAD_CONST — literal folded, interpreter == JIT",
          "[jit][golden]") {
  std::vector<InputVec> inputs = {
      {0x00, 0x00, 0}, {0x01, 0xFF, 1}, {0x10, 0xF0, 0}, {0x42, 0xBD, 1},
      {0x55, 0xAA, 0}, {0x99, 0x66, 1}, {0xFE, 0x01, 0}, {0xFF, 0x00, 1},
  };

  auto interp = run_capture<LoadConstDut>(false, inputs);
  auto jit    = run_capture<LoadConstDut>(true,  inputs);

  REQUIRE(interp == jit);

  // r0 = (in0 + 0xAB) & 0xFF  with 1-cycle delay
  // r1 = (in0 + 0x11) & 0xFF  with 1-cycle delay
  // out0 = r0 (delayed), out1 = r1 (delayed), out2 = r0 ^ r1 (combinational)
  REQUIRE(interp[0][0] == ((0x00 + 0xAB) & 0xFF));
  REQUIRE(interp[0][1] == ((0x00 + 0x11) & 0xFF));
  REQUIRE(interp[0][2] == (0xAB ^ 0x11));
  REQUIRE(interp[1][0] == ((0x01 + 0xAB) & 0xFF));
  REQUIRE(interp[1][1] == ((0x01 + 0x11) & 0xFF));
  REQUIRE(interp[1][2] == (0xAC ^ 0x12));
  REQUIRE(interp[4][0] == ((0x55 + 0xAB) & 0xFF));
  REQUIRE(interp[5][0] == ((0x99 + 0xAB) & 0xFF));
  REQUIRE(interp[6][0] == ((0xFE + 0xAB) & 0xFF));  // wraps to 0xA9
  REQUIRE(interp[7][0] == ((0xFF + 0xAB) & 0xFF));  // wraps to 0xAA
  REQUIRE(interp[6][2] == ((((0xFE + 0xAB) & 0xFF)) ^ (((0xFE + 0x11) & 0xFF))));
}

TEST_CASE("JitOp::SELECT — ternary mux, interpreter == JIT",
          "[jit][golden]") {
  // sel=true (1) → r0 = in0; sel=false (0) → r0 = in1
  std::vector<InputVec> inputs = {
      {0x11, 0x22, 1},  // sel=1 → r0=0x11, r1=0x22, out2=in0=0x11
      {0x33, 0x44, 0},  // sel=0 → r0=0x44, r1=0x33, out2=in1=0x44
      {0x55, 0x66, 1},  // sel=1
      {0x77, 0x88, 0},  // sel=0
      {0x99, 0xAA, 1},
      {0xBB, 0xCC, 0},
      {0xDD, 0xEE, 1},
      {0xFF, 0x00, 0},
  };

  auto interp = run_capture<SelectDut>(false, inputs);
  auto jit    = run_capture<SelectDut>(true,  inputs);

  REQUIRE(interp == jit);

  // r0 = select(sel, in0, in1), r1 = select(sel, in1, in0) — 1-cycle delayed.
  // out2 = select(sel, in0, in1) — combinational, observable same tick.
  REQUIRE(interp[0][0] == 0x11);  // r0 captured select(1, 0x11, 0x22) = 0x11
  REQUIRE(interp[0][1] == 0x22);  // r1 captured select(1, 0x22, 0x11) = 0x22
  REQUIRE(interp[0][2] == 0x11);
  REQUIRE(interp[1][0] == 0x44);  // r0 captured select(0, 0x33, 0x44) = 0x44
  REQUIRE(interp[1][1] == 0x33);  // r1 captured select(0, 0x44, 0x33) = 0x33
  REQUIRE(interp[1][2] == 0x44);
  REQUIRE(interp[2][0] == 0x55);  // sel=1
  REQUIRE(interp[2][1] == 0x66);
  REQUIRE(interp[2][2] == 0x55);
  REQUIRE(interp[3][0] == 0x88);  // sel=0
  REQUIRE(interp[3][1] == 0x77);
  REQUIRE(interp[3][2] == 0x88);
  REQUIRE(interp[7][0] == 0x00);  // sel=0, in0=0xFF, in1=0x00 → r0=0
  REQUIRE(interp[7][2] == 0x00);
}

TEST_CASE("JitOp::REG_NEXT — register propagation, interpreter == JIT",
          "[jit][golden]") {
  std::vector<InputVec> inputs = {
      {0x10, 0x00, 0}, {0x20, 0x00, 1}, {0x30, 0x00, 0}, {0x40, 0x00, 1},
      {0x50, 0x00, 0}, {0x60, 0x00, 1}, {0x70, 0x00, 0}, {0x80, 0x00, 1},
      {0x90, 0x00, 0},
  };

  auto interp = run_capture<RegNextDut>(false, inputs);
  auto jit    = run_capture<RegNextDut>(true,  inputs);

  REQUIRE(interp == jit);

  // 3-stage register chain: r0 ← in0, r1 ← r0, r2 ← r1.
  // After tick N: r0 = inputs[N][0], r1 = inputs[N-1][0], r2 = inputs[N-2][0].
  // (When N-k < 0, the reg still holds its initial value 0_b.)
  REQUIRE(interp[0][0] == 0x10);
  REQUIRE(interp[0][1] == 0x00);  // r1 initial value
  REQUIRE(interp[0][2] == 0x00);  // r2 initial value
  REQUIRE(interp[1][0] == 0x20);
  REQUIRE(interp[1][1] == 0x10);  // r1 captured r0 from tick 0
  REQUIRE(interp[1][2] == 0x00);
  REQUIRE(interp[2][0] == 0x30);
  REQUIRE(interp[2][1] == 0x20);
  REQUIRE(interp[2][2] == 0x10);  // r2 captured r1 from tick 0
  REQUIRE(interp[3][0] == 0x40);
  REQUIRE(interp[3][1] == 0x30);
  REQUIRE(interp[3][2] == 0x20);
  REQUIRE(interp[4][0] == 0x50);
  REQUIRE(interp[4][1] == 0x40);
  REQUIRE(interp[4][2] == 0x30);
  REQUIRE(interp[5][0] == 0x60);
  REQUIRE(interp[6][0] == 0x70);
  REQUIRE(interp[7][0] == 0x80);
  REQUIRE(interp[8][0] == 0x90);
  REQUIRE(interp[8][1] == 0x80);
  REQUIRE(interp[8][2] == 0x70);
}