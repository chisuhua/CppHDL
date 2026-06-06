// tests/test_verilog_compare.cpp
//
// Verilog code-generation tests dedicated to the comparison and bitwise-logic
// ch_op values: eq, ne, lt, le, gt, ge (binary), and_, or_, xor_ (binary),
// and not_ (unary). Each TEST_CASE exercises a single operator and asserts
// that the correct Verilog token appears in the emitted module body.
//
// Pattern mirrors tests/test_verilog_gen.cpp:
//   1. Build a ch::core::context.
//   2. Use ch::core::ctx_swap to install it as the active context.
//   3. Build a small combinational graph using ch_uint<> / ch_bool.
//   4. Pipe the context through verilogwriter and check the output.
//
// These tests do NOT modify src/codegen_verilog.cpp. They are pure observers
// of the existing get_op_str() / print_binary_op() / print_unary_op() paths.

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/context.h"
#include <memory>
#include <sstream>

using namespace ch;
using namespace ch::core;
using namespace ch::core::literals;

// ---------------------------------------------------------------------------
// Helper: capture the full Verilog module body for a context as a string.
// ---------------------------------------------------------------------------
static std::string generateVerilogToString(context *ctx) {
    std::ostringstream oss;
    verilogwriter writer(ctx);
    writer.print(oss);
    return oss.str();
}

// ---------------------------------------------------------------------------
// Compare ops (binary, return ch_bool):
//   eq  -> "=="
//   ne  -> "!="
//   lt  -> "<"
//   le  -> "<="
//   gt  -> ">"
//   ge  -> ">="
// ---------------------------------------------------------------------------

// 1. eq: ch_op::eq -> "assign <name> = lhs == rhs;"
TEST_CASE("VerilogGen - CompareEq", "[verilog][gen][compare][eq]") {
    auto ctx = std::make_unique<ch::core::context>("cmp_eq_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_bool result = (a == b);
    ch_out<ch_bool> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    // The op node is named "eq" (see operators.h:704). The emit uses
    // "lhs op rhs" with single-char padding, so "a == b" is the unique
    // fingerprint.
    REQUIRE(verilog.find("eq") != std::string::npos);
    REQUIRE(verilog.find(" == ") != std::string::npos);
    REQUIRE(verilog.find("!=") == std::string::npos);
}

// 2. ne: ch_op::ne -> "assign <name> = lhs != rhs;"
TEST_CASE("VerilogGen - CompareNe", "[verilog][gen][compare][ne]") {
    auto ctx = std::make_unique<ch::core::context>("cmp_ne_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_bool result = (a != b);
    ch_out<ch_bool> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("ne") != std::string::npos);
    REQUIRE(verilog.find(" != ") != std::string::npos);
    REQUIRE(verilog.find("==") == std::string::npos);
}

// 3. lt: ch_op::lt -> "assign <name> = lhs < rhs;"
//    NOTE: We use a literal RHS (ch_uint<8>(50_d)) so the emit becomes
//    "a < <lit_str>". The "< " substring on the lhs side still appears
//    because "<" is the operator between lhs and rhs.
TEST_CASE("VerilogGen - CompareLt", "[verilog][gen][compare][lt]") {
    auto ctx = std::make_unique<ch::core::context>("cmp_lt_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_uint<8> threshold(50_d);
    ch_bool result = (a < threshold);
    ch_out<ch_bool> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("lt") != std::string::npos);
    REQUIRE(verilog.find(" < ") != std::string::npos);
    // "<=" must NOT appear (lt, not le).
    REQUIRE(verilog.find("<=") == std::string::npos);
    // ">" must NOT appear (lt, not gt).
    REQUIRE(verilog.find(" > ") == std::string::npos);
}

// 4. le: ch_op::le -> "assign <name> = lhs <= rhs;"
TEST_CASE("VerilogGen - CompareLe", "[verilog][gen][compare][le]") {
    auto ctx = std::make_unique<ch::core::context>("cmp_le_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_uint<8> threshold(50_d);
    ch_bool result = (a <= threshold);
    ch_out<ch_bool> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("le") != std::string::npos);
    REQUIRE(verilog.find(" <= ") != std::string::npos);
    // "= " alone is too generic — but ">=" must not appear.
    REQUIRE(verilog.find(">=") == std::string::npos);
}

// 5. gt: ch_op::gt -> "assign <name> = lhs > rhs;"
TEST_CASE("VerilogGen - CompareGt", "[verilog][gen][compare][gt]") {
    auto ctx = std::make_unique<ch::core::context>("cmp_gt_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_uint<8> threshold(50_d);
    ch_bool result = (a > threshold);
    ch_out<ch_bool> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("gt") != std::string::npos);
    REQUIRE(verilog.find(" > ") != std::string::npos);
    // ">=" must NOT appear (gt, not ge).
    REQUIRE(verilog.find(">=") == std::string::npos);
}

// 6. ge: ch_op::ge -> "assign <name> = lhs >= rhs;"
TEST_CASE("VerilogGen - CompareGe", "[verilog][gen][compare][ge]") {
    auto ctx = std::make_unique<ch::core::context>("cmp_ge_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_uint<8> threshold(50_d);
    ch_bool result = (a >= threshold);
    ch_out<ch_bool> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("ge") != std::string::npos);
    REQUIRE(verilog.find(" >= ") != std::string::npos);
    // "<" alone must NOT appear in the operator position. (Port names like
    // "a" are < 1 char so no "<" appears elsewhere either.)
    REQUIRE(verilog.find(" < ") == std::string::npos);
}

// ---------------------------------------------------------------------------
// Bitwise logic ops (binary): and_ ("&"), or_ ("|"), xor_ ("^")
// All three follow the same emit pattern as the compare ops, so we check
// for "<lhs> <op> <rhs>" with single-space padding to avoid false positives
// from the &[7:0] bit-range notation (none in this file) and the "endmodule"
// text.
// ---------------------------------------------------------------------------

// 7. and_: ch_op::and_ -> "assign <name> = lhs & rhs;"
//    Bitwise AND on ch_uint<>. Uses ch_in<> for both sides so the rhs is
//    a wire, not a literal; this keeps "&" from appearing inside any
//    literal format like "8'hff".
TEST_CASE("VerilogGen - LogicAnd", "[verilog][gen][logic][and]") {
    auto ctx = std::make_unique<ch::core::context>("logic_and_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_uint<8> result = (a & b);
    ch_out<ch_uint<8>> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    // The binary emit is `assign <name> = a & b;` — check the operator
    // token between the two named wires. (Node name is "and_and" via
    // binary_operation's `<Op::name>_<name_suffix>` scheme, so don't
    // grep for the bare op name.)
    REQUIRE(verilog.find("a & b") != std::string::npos);
    // "&&" (logical AND) must NOT appear — this is bitwise "&".
    REQUIRE(verilog.find("&&") == std::string::npos);
}

// 8. or_: ch_op::or_ -> "assign <name> = lhs | rhs;"
TEST_CASE("VerilogGen - LogicOr", "[verilog][gen][logic][or]") {
    auto ctx = std::make_unique<ch::core::context>("logic_or_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_uint<8> result = (a | b);
    ch_out<ch_uint<8>> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    // Binary emit: `assign <name> = a | b;` — check the operator token.
    REQUIRE(verilog.find("a | b") != std::string::npos);
    // "||" (logical OR) must NOT appear — this is bitwise "|".
    REQUIRE(verilog.find("||") == std::string::npos);
}

// 9. xor_: ch_op::xor_ -> "assign <name> = lhs ^ rhs;"
TEST_CASE("VerilogGen - LogicXor", "[verilog][gen][logic][xor]") {
    auto ctx = std::make_unique<ch::core::context>("logic_xor_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_uint<8> result = (a ^ b);
    ch_out<ch_uint<8>> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    // Binary emit: `assign <name> = a ^ b;` — check the operator token.
    REQUIRE(verilog.find("a ^ b") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Bitwise logic ops (unary): not_ ("~")
// ---------------------------------------------------------------------------

// 10. not_: ch_op::not_ -> "assign <name> = ~src;"
//     Bitwise NOT (NOT logical NOT — `!` produces a separate logical_not
//     op node). Uses operator~ on a ch_in<> wire so the rhs is named
//     after the port, not a literal.
TEST_CASE("VerilogGen - LogicNot", "[verilog][gen][logic][not]") {
    auto ctx = std::make_unique<ch::core::context>("logic_not_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_uint<8> result = ~a;
    ch_out<ch_uint<8>> out_port("io");
    out_port = result;

    std::string verilog = generateVerilogToString(ctx.get());

    // Unary emit: `assign <name> = ~a;`. The prefix " = ~" is the
    // bitwise-NOT fingerprint (analogous to AndReduce's " = &" pattern
    // in test_verilog_gen.cpp). Node name is "not_not" via the
    // `<Op::name>_<name_suffix>` scheme, so don't grep for " not ".
    REQUIRE(verilog.find(" = ~") != std::string::npos);
    // The src wire name must appear immediately after the tilde.
    REQUIRE(verilog.find("~a") != std::string::npos);
}
