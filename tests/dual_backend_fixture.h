// tests/dual_backend_fixture.h
//
// `DUAL_BACKEND_TEST_PAIR` macro for running the same TEST_CASE body
// across InterpreterBackend and VerilatorBackend.
//
// Capability spec: openspec/specs/dual-backend-test-coverage/spec.md
//
// Invariants enforced by the macro (cross-referenced to spec requirements):
//   * R1: interpreter side calls `set_jit_enabled(false)` +
//         `set_backend(make_unique<InterpreterBackend>())` (double-pin
//         against CH_JIT_ENABLE=ON silently routing through JIT).
//   * R2: TWO independent `ch::ch_device<DutType>` instances (one per
//         backend) — `data_map_` and `node_id` are context-local, so a
//         single device cannot drive two backends.
//   * R3: per-cycle `REQUIRE(get_port_value(...) == ...)` is the user's
//         responsibility inside the lambda body; the macro does NOT
//         collect a full trace and diff afterwards (would miss mid-test
//         REQUIRE failures on stream/AXI handshake signals).
//   * R4: if `command -v verilator` fails, the macro emits a single
//         Catch2 `WARN` and the verilator half returns early (does NOT
//         SKIP the whole TEST_CASE — that would cause Catch2 to return
//         exit code 4).
//   * R5: if VerilatorBackend::initialize returns false (e.g. port
//         width > 64 per ADR-035 R3 64-bit cap), the macro emits a
//         single WARN and the verilator half returns early; the
//         interpreter half still runs in its own TEST_CASE.
//
// Usage pattern (lambda-passed body — required by C preprocessor
// limitations on brace-body macros):
//
//   DUAL_BACKEND_TEST_PAIR(MyAdder, "MyAdder 8-bit basic add",
//                          "[dual][arith]", [&]() {
//       sim.set_input_value(dev.instance().io().a, 3);
//       sim.set_input_value(dev.instance().io().b, 5);
//       sim.tick();
//       REQUIRE(static_cast<uint64_t>(
//           sim.get_port_value(dev.instance().io().sum)) == 8u);
//   });
//
// The macro injects `dev` (ch::ch_device<DutType>&) and `sim`
// (ch::Simulator&) as local variables BEFORE invoking the lambda. The
// user MUST NOT re-declare them. Each TEST_CASE runs once, picking
// the appropriate backend based on a per-thread flag
// (`ch_test::dual_backend::current_kind()`).
//
// Out of scope (documented in spec R6 exclusion list):
//   - STATIC_REQUIRE / pure compile-time tests
//   - bare-context tests (no ch_device / ch_module)
//   - aread-based designs (mem-read-timing spec 1-cycle skew)
//   - Catch2 SECTION re-entry (each backend-run re-enters SECTIONs)
//   - >64-bit port fixtures (R5 above; tests must use the dedicated
//     test_dual_backend_64_bit fixture with [verilator-skip-wide] tag)

#pragma once

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/context.h"
#include "core/eval_backend.h"
#include "core/interpreter_backend.h"
#include "core/lnodeimpl.h"
#include "core/verilator_backend.h"
#include "device.h"
#include "external_tool_helpers.h"
#include "simulator.h"

#include <memory>
#include <string>
#include <utility>

namespace ch_test::dual_backend {

enum class BackendKind { Interpreter, Verilator };

// Per-thread current backend (set by the macro before invoking the
// user body). Default is Interpreter so any out-of-macro call (e.g.
// helper functions written in the lambda that need to know which
// backend they're running under) sees a sane value.
inline BackendKind &current_kind_ref() {
    static thread_local BackendKind k = BackendKind::Interpreter;
    return k;
}

inline BackendKind current_kind() { return current_kind_ref(); }

// Pre-flight width scan (spec R5). Walks the context's node_storage_
// and returns the maximum bitwidth found on any `type_output` node
// (i.e. top-level ch_out ports and any ch_out promoted via the codegen
// hier-output pass). Returns 0 if no output nodes are present
// (verilator safe).
//
// IMPORTANT: this is a best-effort scan. The VerilatorBackend's
// own `build_port_access_table()` is the source of truth for the
// cap (see `src/core/verilator_backend.cpp:632-644`). The macro
// uses this helper to skip the verilator half BEFORE invoking
// verilator compile (which can be minutes for cold cache) — but
// the final acceptance still relies on try_initialize returning
// `PortBitwidthOverflow` for defensive double-check.
//
// Returns the maximum bitwidth seen; the caller compares against 64.
inline unsigned max_top_output_bitwidth(ch::core::context *ctx) {
    if (!ctx) return 0;
    unsigned max_bw = 0;
    for (const auto &n : ctx->get_nodes()) {
        if (!n) continue;
        if (n->type() == ch::core::lnodetype::type_output) {
            if (n->size() > max_bw) max_bw = n->size();
        }
    }
    return max_bw;
}

// True if the design's ch_out port widths fit the VerilatorBackend
// 64-bit cap (ADR-035 R3).
inline bool port_width_compatible_for_verilator(ch::core::context *ctx) {
    return max_top_output_bitwidth(ctx) <= 64;
}

} // namespace ch_test::dual_backend

// ============================================================================
// DUAL_BACKEND_TEST_PAIR — recommended macro
// ============================================================================
//
// Expands to TWO TEST_CASE declarations sharing the same lambda body.
// Each TEST_CASE runs once, picking its backend based on a captured
// per-thread flag (`current_kind_ref()`). The user passes the body
// as a lambda (4th macro argument). The interpreter half ALWAYS runs
// (the source of truth per spec R3); the verilator half may WARN-skip
// if the tool is missing or initialize() rejects the design.
//
// IMPORTANT: the user MUST NOT use Catch2 SECTION inside the lambda
// — each backend-run would re-enter the section tree, breaking
// semantics (spec R6 #5).
//
// Alias: DUAL_BACKEND_TEST is provided as a backwards-compatible
// alias for spec text that references that name (the spec was
// updated to use `_PAIR` after the lambda-passed body design was
// finalized; both names work).
#define DUAL_BACKEND_TEST_PAIR(DUT_TYPE, NAME, TAGS, BODY)                       \
    /* === interpreter half (source of truth, R1+R2+R3) === */                  \
    TEST_CASE(NAME " [interpreter]", TAGS) {                                     \
        using DutType = DUT_TYPE;                                                 \
        ch_test::dual_backend::current_kind_ref() =                              \
            ch_test::dual_backend::BackendKind::Interpreter;                      \
        ch::ch_device<DutType> dev;                                              \
        ch::Simulator sim(dev.context());                                        \
        sim.set_jit_enabled(false);                                              \
        sim.set_backend(std::make_unique<ch::InterpreterBackend>());              \
        BODY();                                                                  \
    }                                                                            \
    /* === verilator half (may WARN-skip; R4+R5) === */                         \
    TEST_CASE(NAME " [verilator]", TAGS) {                                       \
        using DutType = DUT_TYPE;                                                 \
        if (!ch_test::tool_available("verilator")) {                              \
            WARN("verilator not on PATH; verilator half skipped "                \
                 "(interpreter half still runs in its own TEST_CASE)");          \
            return;                                                               \
        }                                                                        \
        auto workdir = ch_test::make_temp_dir("_dual");                          \
        if (workdir.empty()) {                                                    \
            WARN("make_temp_dir failed; verilator half skipped");                 \
            return;                                                               \
        }                                                                        \
        ch_test::dual_backend::current_kind_ref() =                              \
            ch_test::dual_backend::BackendKind::Verilator;                        \
        ch::ch_device<DutType> dev;                                              \
        /* R5 pre-flight width scan: skip verilator half BEFORE invoking */     \
        /* verilator compile (which can be minutes for cold cache) when the */   \
        /* design exceeds the 64-bit cap. The VerilatorBackend's own */         \
        /* build_port_access_table is the source of truth for the cap; this */  \
        /* scan is an optimization to avoid expensive compile attempts. */      \
        if (!ch_test::dual_backend::                                            \
                port_width_compatible_for_verilator(dev.context())) {           \
            WARN("fixture has ch_out wider than 64-bit; "                        \
                 "skipping verilator half (interpreter still runs under "       \
                 "its own TEST_CASE). See ADR-035 R3 64-bit cap.");             \
            return;                                                               \
        }                                                                        \
        ch::data_map_t data_map;                                                  \
        auto backend = std::make_unique<ch::VerilatorBackend>(workdir);          \
        if (!backend->try_initialize(dev.context(), data_map).has_value()) {     \
            WARN("VerilatorBackend::initialize returned false; "                  \
                 "either codegen/compile/dlopen failed or fixture has "          \
                 "ch_out wider than 64-bit (pre-flight scan missed). "            \
                 "Verilator half skipped (interpreter still runs under its "       \
                 "own TEST_CASE).");                                              \
            return;                                                               \
        }                                                                        \
        if (backend->port_access_snapshot().empty()) {                          \
            WARN("verilator dlopen did not populate port_access_ "               \
                 "(verilator build may have failed in test env)");               \
            return;                                                               \
        }                                                                        \
        ch::Simulator sim(dev.context());                                        \
        sim.set_backend(std::move(backend));                                     \
        BODY();                                                                  \
    }

// Backwards-compatible alias: spec R8 / design.md §2.1 references
// `DUAL_BACKEND_TEST(DutType, name, tags)` (without the `_PAIR`
// suffix). The implementation is identical to DUAL_BACKEND_TEST_PAIR;
// both names are valid.
#define DUAL_BACKEND_TEST(DUT_TYPE, NAME, TAGS, BODY) \
    DUAL_BACKEND_TEST_PAIR(DUT_TYPE, NAME, TAGS, BODY)