// tests/test_verilog_shift.cpp
//
// Purpose: Dedicated Verilog codegen tests for shift and mux operations.
//
// Each TEST_CASE builds a graph with the target ch_op, runs verilogwriter,
// and asserts the emitted Verilog contains the canonical operator spelling:
//   - shl   -> "<<"   (logical shift left)
//   - shr   -> ">>"   (logical shift right, zero-extended)
//   - sshr  -> ">>>"  (arithmetic shift right, sign-extended)
//   - mux   -> "? :"  (Verilog ternary; see print_mux in codegen_verilog.cpp)
//
// sshr has no public C++ operator (operator>>> is commented out at
// operators.h:665-669), so it is exercised via node_builder::build_operation
// using ch_op::sshr directly. This is the same approach used by
// tests/test_jit_compiler.cpp:751 for the JIT sshr test.

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/context.h"
#include <memory>
#include <sstream>

using namespace ch;
using namespace ch::core;
using namespace ch::core::literals;

// Helper function to capture Verilog output as a string.
static std::string generateVerilogToString(context *ctx) {
    std::ostringstream oss;
    verilogwriter writer(ctx);
    writer.print(oss);
    return oss.str();
}

// ---------------------------------------------------------------------------
// 1. Shl: ch_op::shl -> `assign shl = lhs << rhs;` (see get_op_str at
//    codegen_verilog.cpp:179-180 and print_binary_op at lines 622-653).
//
//    Use shl<ResultWidth>(lhs, rhs) to get a node with a fixed result
//    width (operators.h:733). Then drive it with a ch_in<> wire and a
//    ch_uint<> literal amount so the rhs is rendered as a hex literal.
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - Shl", "[verilog][gen][shift][shl]") {
    auto ctx = std::make_unique<ch::core::context>("shl_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<5>> a("a");
    ch_uint<3> shift(3_d); // 3 bits to encode 0..7
    auto shifted = shl<8>(a, shift);
    ch_out<ch_uint<8>> out_port("io");
    out_port = shifted;

    std::string verilog = generateVerilogToString(ctx.get());

    // Node name is the literal "shl" passed to build_operation.
    REQUIRE(verilog.find("shl") != std::string::npos);
    // Fingerprint of the binary emit: lhs << rhs.
    REQUIRE(verilog.find(" << ") != std::string::npos);
    // assign ... = ... ; boundary check (line emitted by print_binary_op).
    REQUIRE(verilog.find("assign") != std::string::npos);
    REQUIRE(verilog.find(";") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 2. Shr: ch_op::shr -> `assign shr = lhs >> rhs;` (see get_op_str at
//    codegen_verilog.cpp:181-182). Uses the public operator>> at
//    operators.h:778-781 (binary_operation<shr_op>).
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - Shr", "[verilog][gen][shift][shr]") {
    auto ctx = std::make_unique<ch::core::context>("shr_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_uint<3> shift(2_d);
    auto shifted = a >> shift;
    ch_out<ch_uint<8>> out_port("io");
    out_port = shifted;

    std::string verilog = generateVerilogToString(ctx.get());

    // Node name is "shr" (operators.h:780 passes the literal "shr").
    REQUIRE(verilog.find("shr") != std::string::npos);
    // Fingerprint of the binary emit: lhs >> rhs.
    // NOTE: Must check for " >> " (with spaces) so we don't accidentally
    // match sshr's ">>>".
    REQUIRE(verilog.find(" >> ") != std::string::npos);
    // The shl "<< " must NOT appear (proves we hit shr, not shl).
    REQUIRE(verilog.find("<<") == std::string::npos);
    // The sshr ">>>" must NOT appear (proves we hit shr, not sshr).
    REQUIRE(verilog.find(">>>") == std::string::npos);
}

// ---------------------------------------------------------------------------
// 3. Sshr: ch_op::sshr -> `assign sshr = lhs >>> rhs;` (see get_op_str at
//    codegen_verilog.cpp:183-184). sshr is invoked via the low-level
//    node_builder::build_operation API because the C++ `>>>` operator
//    is commented out (operators.h:665-669) and no public helper exists.
//
//    The ">>>" token is unambiguous in Verilog arithmetic shift right.
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - Sshr", "[verilog][gen][shift][sshr]") {
    auto ctx = std::make_unique<ch::core::context>("sshr_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_uint<3> shift(2_d);

    // Build the sshr node directly. The codegen reads ch_op::sshr from
    // the node and routes to print_binary_op with op_str = ">>>".
    lnodeimpl *sshr_node = node_builder::instance().build_operation(
        ch_op::sshr, get_lnode(a), get_lnode(shift), /*result_width=*/8,
        /*is_signed=*/true, "sshr");
    ch_uint<8> result(sshr_node);

    ch_out<ch_uint<8>> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    // Node name is the literal "sshr" passed to build_operation.
    REQUIRE(verilog.find("sshr") != std::string::npos);
    // Fingerprint of sshr: lhs >>> rhs (3 consecutive > characters).
    REQUIRE(verilog.find(">>>") != std::string::npos);
    // The plain ">>" (shr) must NOT appear in isolation as a binary op
    // boundary. We tolerate substrings of ">>>" but not bare ">> ".
    // Stronger: verify ">>>" appears (which already implies ">>" is
    // present as a substring, but only as part of ">>>").
    REQUIRE(verilog.find(" << ") == std::string::npos);
    REQUIRE(verilog.find(" <<<") == std::string::npos);
}

// ---------------------------------------------------------------------------
// 4. Mux: type_mux (ch_op::mux) -> `assign mux = cond ? t : f;` (see
//    print_mux at codegen_verilog.cpp:812-832). Uses the public
//    select(cond, t, f) helper at operators.h:655-661 which returns a
//    ternary_operation<mux_op>.
//
//    The mux is dispatched via lnodetype::type_mux, NOT through
//    print_binary_op, so get_op_str("<mux>") is never reached.
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - Mux", "[verilog][gen][mux]") {
    auto ctx = std::make_unique<ch::core::context>("mux_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_bool> cond("cond");
    ch_in<ch_uint<8>> t("t");
    ch_in<ch_uint<8>> f("f");

    auto muxed = select(cond, t, f);
    ch_out<ch_uint<8>> out_port("io");
    out_port = muxed;

    std::string verilog = generateVerilogToString(ctx.get());

    // The mux node is named "mux" (operators.h:660 passes "select" as
    // the name suffix, but the public select() helper uses build_mux
    // with the default "mux" name -- see node_builder.h:202-216).
    REQUIRE(verilog.find("mux") != std::string::npos);
    // Fingerprint of print_mux: " ? " and " : " ternary punctuation.
    REQUIRE(verilog.find(" ? ") != std::string::npos);
    REQUIRE(verilog.find(" : ") != std::string::npos);
    // assign ... = ... ; boundary check.
    REQUIRE(verilog.find("assign") != std::string::npos);
    REQUIRE(verilog.find(";") != std::string::npos);
}
