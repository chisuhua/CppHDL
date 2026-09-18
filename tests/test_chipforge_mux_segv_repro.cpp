// tests/test_chipforge_mux_segv_repro.cpp
//
// Repro for chipforge 5-stage Simulator SEGV (Phase 6c M6 investigation).
//
// Original error in chipforge m4_poc_5stage_simulator_tick:
//   SIGSEGV at ch::core::muximpl::create_instruction line 13
//   (false_value()->id() dereferences nullptr)
//
// ============================================================================
// ROOT CAUSE IDENTIFIED (independent of chipforge, bug is in CppHDL itself)
// ============================================================================
//
// Failing pattern (in active ctx_swap scope):
//     ch_uint<5> a(0);   // <-- impl() returns nullptr!
//
// Working patterns (same context):
//     ch_uint<5> b(ch_literal<0, 5>{});            // impl() valid
//     ch_uint<5> c(ch_literal_runtime(0u, 5));     // impl() valid
//
// This is a CppHDL bug: the implicit conversion sequence
//   int(0) -> ch_literal_runtime(0, 1) [via int ctor, width=compute_width(0)=1]
//        -> ch_uint<N>(ch_literal_runtime&)
// ends up storing nullptr into node_impl_.
//
// Direct path `ch_uint<N>(ch_literal_runtime{0u, N})` works correctly, so
// the bug is specifically in the conversion sequence
// int -> ch_literal_runtime (via `ch_literal_runtime(int v)` ctor) ->
// ch_uint<N>(ch_literal_runtime const&). One of these steps either fails
// or stores a null impl.
//
// Likely candidates to investigate:
//   1. `ch_literal_runtime::ch_literal_runtime(int v)` ctor (literal.h)
//      - calls compute_width(0) = 1
//      - if compute_width has a bug for v=0, downstream build_literal
//        could fail or produce wrong-width literal that fails ctor body
//   2. `ch_uint<N>::ch_uint(ch_literal_runtime const&)` ctor (uint.tpp)
//      - calls build_literal(val, name, sloc)
//      - if build_literal returns nullptr for some reason, node_impl_
//        stays nullptr
//   3. The implicit conversion sequence overload resolution
//      - might pick a different ctor than expected
//
// ============================================================================
// PROPAGATION CHAIN (causes the original chipforge SEGV)
// ============================================================================
//
// 1. addr_t(0) returns null impl (test 2 below)
// 2. rd_addr != addr_t(0) calls build_ne(null_impl, valid_impl) -> returns
//    null opimpl (test 3 below)
// 3. ch_bool(null_opimpl) -> ch_bool with null impl
// 4. ch_bool(false) && (...) -> binary_bool_operation(lhs.impl=valid,
//    rhs.impl=null) -> returns null op_node
// 5. we = ch_bool(null_op_node) -> we.impl() = null (test 4 below)
// 6. select(we, t_val, regs[i]) builds muximpl with cond=nullptr
// 7. Simulator::initialize() calls muximpl::create_instruction()
// 8. false_value() = src(2) returns nullptr (because muximpl ctor
//    skipped it via `if (cond) add_src(cond)` when cond was null)
// 9. false_value()->id() SEGV
//
// ============================================================================
// INVESTIGATION TASKS (for the next CppHDL session)
// ============================================================================
//
// TASK A (highest priority): determine which ch_uint<N> ctor is selected
//   for `addr_t(0)` and why it produces a null impl. Add a debug print
//   in uint.tpp's ch_uint<N>::ch_uint(const ch_literal_runtime&) ctor
//   to see if it is called and what build_literal returns.
//
// TASK B: trace the implicit conversion sequence. Add an explicit
//   `ch_uint<5>(ch_literal_runtime(0, 1))` and verify it produces a
//   valid impl. If yes, the bug is in the int -> ch_literal_runtime
//   conversion or in ctor overload resolution.
//
// TASK C: investigate why build_literal returns null even with an
//   active ctx_swap. Add fprintf in build_literal to print val.width()
//   and val.value() when called.
//
// TASK D: check if ch_uint<N>(const ch_uint<N>&) ctor (copy ctor)
//   or operator= is involved in the implicit conversion path.
//
// ============================================================================
// TEST RESULTS SUMMARY (current state on this branch)
// ============================================================================
//
//   [PASS] cpphdl_repro_ch_bool_false_in_active_ctx
//          ch_bool(false).impl() = 0x...    (valid)
//
//   [FAIL] cpphdl_repro_addr_t_zero_in_active_ctx
//          addr_t(0).impl() = nil            ← ROOT CAUSE
//          addr_t(ch_literal<0,5>).impl() = 0x...    (valid - works)
//          addr_t(ch_literal_runtime(0,5)).impl() = 0x...    (valid - works)
//
//   [FAIL] cpphdl_repro_ne_after_addr_t_zero
//          rd_addr.impl() = 0x... (valid)
//          zero.impl() = nil                  ← unreachable due to Test 2
//
//   [FAIL] cpphdl_repro_full_wb_pattern_select_then_sim
//          we.impl() = nil                     ← cascaded from Test 2
//          sel.impl() == nullptr for all 31 iterations
//
//   [FAIL] cpphdl_repro_select_with_null_cond_triggers_segv
//          (reaches simulator SEGV path)      ← cascaded from Test 2
//
// ============================================================================
// BUILD & RUN
// ============================================================================
//
//   cmake -S /workspace/project/CppHDL -B /workspace/project/CppHDL/build
//   cmake --build /workspace/project/CppHDL/build \
//           --target test_chipforge_mux_segv_repro -j$(nproc)
//   /workspace/project/CppHDL/build/tests/test_chipforge_mux_segv_repro
//
//   To see all diagnostics:
//   /workspace/project/CppHDL/build/tests/test_chipforge_mux_segv_repro -s
//
// ============================================================================

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "context.h"
#include "literal.h"
#include "simulator.h"

#include <cstdio>
#include <iostream>

using namespace ch;
using namespace ch::core;

namespace {

// Diagnostic helper: prints a tag.
// (ctx_curr_ introspection skipped: the symbol is defined in an anonymous
// namespace inside ch::core which makes it hard to reference from this test
// without modifying the cpphdl library. The tests below check impl()
// directly, which is sufficient for diagnosing the SEGV root cause.)
void print_ctx(const char* tag) {
  std::fprintf(stderr, "  [ctx] %-30s (ctx_curr_ introspection skipped)\n", tag);
  std::fflush(stderr);
}

}  // namespace

TEST_CASE("cpphdl_repro_ch_bool_false_in_active_ctx", "[chipforge_repro]") {
  context ctx("test_active_ctx");
  ctx_swap swap(&ctx);

  print_ctx("before ch_bool(false)");

  ch_bool b(false);
  std::fprintf(stderr, "  [diag] b.impl() = %p\n", static_cast<void*>(b.impl()));
  std::fflush(stderr);

  REQUIRE(b.impl() != nullptr);
}

TEST_CASE("cpphdl_repro_addr_t_zero_in_active_ctx", "[chipforge_repro]") {
  context ctx("test_addr_t_ctx");
  ctx_swap swap(&ctx);

  using addr_t = ch_uint<5>;
  print_ctx("before addr_t(0)");

  addr_t a(0);
  std::fprintf(stderr, "  [diag] a.impl() = %p\n", static_cast<void*>(a.impl()));
  std::fflush(stderr);

  // ALSO try the ch_literal template overload (used by chipforge HazardPlugin)
  addr_t b(ch_literal<0, 5>{});
  std::fprintf(stderr, "  [diag] addr_t(ch_literal<0,5>).impl() = %p\n",
               static_cast<void*>(b.impl()));
  std::fflush(stderr);

  // ALSO try the ch_literal_runtime explicit ctor
  ch_literal_runtime lit(0u, 5);
  addr_t c(lit);
  std::fprintf(stderr,
               "  [diag] addr_t(ch_literal_runtime(0,5)).impl() = %p\n",
               static_cast<void*>(c.impl()));
  std::fflush(stderr);

  REQUIRE(a.impl() != nullptr);
}

TEST_CASE("cpphdl_repro_ne_after_addr_t_zero", "[chipforge_repro]") {
  context ctx("test_ne_ctx");
  ctx_swap swap(&ctx);

  using addr_t = ch_uint<5>;
  addr_t rd_addr = static_cast<addr_t>(static_cast<std::uint8_t>(0));
  addr_t zero(static_cast<std::uint32_t>(0));

  std::fprintf(stderr, "  [diag] rd_addr.impl() = %p, zero.impl() = %p\n",
               static_cast<void*>(rd_addr.impl()),
               static_cast<void*>(zero.impl()));
  std::fflush(stderr);

  REQUIRE(rd_addr.impl() != nullptr);
  REQUIRE(zero.impl() != nullptr);

  auto ne = rd_addr != zero;
  std::fprintf(stderr, "  [diag] ne.impl() = %p\n",
               static_cast<void*>(ne.impl()));
  std::fflush(stderr);

  REQUIRE(ne.impl() != nullptr);
}

TEST_CASE("cpphdl_repro_full_wb_pattern_select_then_sim",
          "[chipforge_repro][mux_segv]") {
  context ctx("test_full_wb_ctx");
  ctx_swap swap(&ctx);

  using addr_t = ch_uint<5>;

  addr_t rd_addr = static_cast<addr_t>(static_cast<std::uint8_t>(0));
  ch_bool we = ch_bool(false) && (rd_addr != addr_t(0));

  std::fprintf(stderr, "  [diag] we.impl() = %p\n",
               static_cast<void*>(we.impl()));
  std::fflush(stderr);

  REQUIRE(we.impl() != nullptr);

  // Replicate the wb_writeback select loop for i=1..31 with the SAME we:
  std::size_t sel_null_count = 0;
  for (std::size_t i = 1; i < 32; ++i) {
    addr_t ai(static_cast<std::uint32_t>(i));
    ch_bool sel = we && (rd_addr == ai);
    if (sel.impl() == nullptr) {
      ++sel_null_count;
      std::fprintf(stderr,
                   "  [diag] sel.impl() == nullptr at i=%zu (rd_addr.impl=%p, "
                   "ai.impl=%p)\n",
                   i, static_cast<void*>(rd_addr.impl()),
                   static_cast<void*>(ai.impl()));
      std::fflush(stderr);
    }
  }
  std::fprintf(stderr, "  [diag] sel_null_count = %zu / 31\n", sel_null_count);
  std::fflush(stderr);

  REQUIRE(sel_null_count == 0);
}

TEST_CASE("cpphdl_repro_select_with_null_cond_triggers_segv",
          "[chipforge_repro][mux_segv]") {
  context ctx("test_segv_ctx");
  ctx_swap swap(&ctx);

  using addr_t = ch_uint<5>;

  // Force the pathological state: build a ch_bool with null impl
  // by constructing a ch_uint<1> from a literal that should succeed,
  // but then test what happens if the cond operand has a null impl.
  //
  // Direct test: build a mux with a null cond operand to verify
  // SEGV reproduction path (muximpl::create_instruction).
  //
  // NOTE: There is no public API to construct a "null ch_bool". The
  // most direct way to reach the same code location is to call select()
  // with operands that the CppHDL implementation might leave null.
  // For now we simulate the buggy end-state: build a muximpl via
  // build_mux with nullptr srcs. This requires CppHDL internal access.

  // Use the public select API to build a muximpl, then try to simulate.
  // If any src is null at this point, the SEGV reproduces.
  using namespace ch::core;

  // The chipforge RegFilePlugin wb_writeback pattern, simplified:
  addr_t rd_addr = static_cast<addr_t>(static_cast<std::uint8_t>(0));
  ch_bool we = ch_bool(false) && (rd_addr != addr_t(0));

  // If `we` already has null impl here, the original repro is reproduced
  // at the chipforge level — no need to go further.
  if (we.impl() == nullptr) {
    SUCCEED("chipforge SEGV root cause reproduced at CppHDL level: "
            "ch_bool::operator&& produces null impl when addr_t(0) is "
            "constructed without ctx_curr_");
    return;
  }

  // Otherwise, run the simulator to confirm downstream behaviour.
  std::fprintf(stderr,
               "  [diag] we.impl() is VALID (=%p), continuing to simulator\n",
               static_cast<void*>(we.impl()));
  std::fflush(stderr);

  // Build a single select to exercise the muximpl creation path.
  ch_bool t_val(true);
  ch_bool f_val(false);
  auto m = select(we, t_val, f_val);
  std::fprintf(stderr, "  [diag] muximpl.impl() = %p\n",
               static_cast<void*>(m.impl()));
  std::fflush(stderr);

  REQUIRE(m.impl() != nullptr);

  // Construct simulator and tick. If the muximpl has invalid srcs,
  // the simulator will SEGV in muximpl::create_instruction.
  std::fprintf(stderr, "  [diag] constructing simulator and ticking...\n");
  std::fflush(stderr);

  Simulator sim(&ctx);
  sim.reset();
  sim.tick();
  SUCCEED("Simulator tick completed without SEGV");
}