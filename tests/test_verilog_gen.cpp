// tests/test_verilog_gen.cpp
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/context.h"
#include <memory>
#include <sstream>

using namespace ch;
using namespace ch::core;
using namespace ch::core::literals;

// Helper function to capture Verilog output as a string
std::string generateVerilogToString(context *ctx) {
    std::ostringstream oss;
    verilogwriter writer(ctx);
    writer.print(oss);
    return oss.str();
}

TEST_CASE("VerilogGen - EmptyModule", "[verilog][empty]") {
    // Create test context
    auto ctx = std::make_unique<ch::core::context>("empty_test");

    // Generate Verilog code for empty module
    std::string verilog_code = generateVerilogToString(ctx.get());

    // Check that the generated code contains expected elements
    REQUIRE(verilog_code.find("module top") != std::string::npos);
    REQUIRE(verilog_code.find("endmodule") != std::string::npos);
}

TEST_CASE("VerilogGen - WriterCreation", "[verilog][writer]") {
    // Create test context
    auto ctx = std::make_unique<ch::core::context>("writer_test");

    // Test that we can create a verilogwriter
    verilogwriter writer(ctx.get());
    std::ostringstream oss;
    writer.print(oss);

    std::string verilog_code = oss.str();
    REQUIRE(verilog_code.find("module top") != std::string::npos);
    REQUIRE(verilog_code.find("endmodule") != std::string::npos);
}

TEST_CASE("VerilogGen - CounterModule", "[verilog][counter]") {
    // Create test context
    auto ctx = std::make_unique<ch::core::context>("counter_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Create a simple 4-bit counter module like in the sample
    ch_out<ch_uint<4>> out_port("io");
    ch_reg<ch_uint<4>> reg_counter(ch_uint<4>(0));

    // Connect the register to increment by 1 each cycle
    reg_counter->next = reg_counter + ch_uint<4>(1);
    out_port = reg_counter;

    ch::toVerilog("verilog_gen.v", ctx.get());

    // Generate Verilog code
    std::string verilog_code = generateVerilogToString(ctx.get());

    // Check that the generated code contains expected elements
    REQUIRE(verilog_code.find("module top") != std::string::npos);
    REQUIRE(verilog_code.find("output [3:0] io") != std::string::npos);
    REQUIRE(verilog_code.find("reg [3:0] reg") != std::string::npos);
    REQUIRE(verilog_code.find("always @(posedge default_clock)") !=
            std::string::npos);
    REQUIRE(verilog_code.find("assign io = reg") != std::string::npos);

    // Check that there are no extraneous ports like io_1
    REQUIRE(verilog_code.find("io_1") == std::string::npos);
}

TEST_CASE("VerilogGen - MinimalOutputDeclaration", "[verilog][minimal]") {
    // Create test context
    auto ctx = std::make_unique<ch::core::context>("minimal_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Create a simple output with no complex logic
    ch_out<ch_uint<8>> simple_out("data_out");
    ch_uint<8> constant_value(42_d);
    simple_out = constant_value;

    std::string verilog_code = generateVerilogToString(ctx.get());

    // Check basic structure
    REQUIRE(verilog_code.find("module top") != std::string::npos);
    REQUIRE(verilog_code.find("output [7:0] data_out") != std::string::npos);

    // Should not have unnecessary ports
    REQUIRE(verilog_code.find("io_1") == std::string::npos);
}

TEST_CASE("VerilogGen - RegisterWithComplexLogic", "[verilog][complex]") {
    // Create test context
    auto ctx = std::make_unique<ch::core::context>("complex_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Create inputs, outputs and registers
    ch_in<ch_uint<8>> in_a("input_a");
    ch_in<ch_uint<8>> in_b("input_b");
    ch_out<ch_uint<8>> out_result("result");
    ch_reg<ch_uint<8>> reg_acc(0_d);

    // Complex logic: accumulator with add/multiply based on a condition
    ch_bool condition = (in_a > 10_d);
    reg_acc->next = select(condition, reg_acc + in_a, reg_acc * in_b);
    out_result = reg_acc;

    std::string verilog_code = generateVerilogToString(ctx.get());

    // Check basic structure
    REQUIRE(verilog_code.find("module top") != std::string::npos);
    REQUIRE(verilog_code.find("input [7:0] input_a") != std::string::npos);
    REQUIRE(verilog_code.find("input [7:0] input_b") != std::string::npos);
    REQUIRE(verilog_code.find("output [7:0] result") != std::string::npos);
    REQUIRE(verilog_code.find("reg [7:0] reg") != std::string::npos);

    // Check for conditional logic (mux)
    REQUIRE(verilog_code.find("always @(posedge default_clock)") !=
            std::string::npos);

    // Should not have unnecessary ports
    REQUIRE(verilog_code.find("io_1") == std::string::npos);
}

TEST_CASE("VerilogGen - LiteralFormatting", "[verilog][literals]") {
    // Create test context
    auto ctx = std::make_unique<ch::core::context>("literal_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> out_port("data");
    ch_uint<8> val_1bit(1_d);   // Should be formatted as 1'b1
    ch_uint<8> val_8bit(255_d); // Should be formatted appropriately

    // Simple assignments
    ch_reg<ch_uint<8>> reg1(0_d);
    reg1->next = reg1 + val_1bit; // This should use 1'b1
    out_port = reg1;

    std::string verilog_code = generateVerilogToString(ctx.get());

    // Check for correct literal formatting
    // FIXME
    // REQUIRE(verilog_code.find("1'b1") != std::string::npos);

    // Should not have unnecessary ports
    REQUIRE(verilog_code.find("io_1") == std::string::npos);
}

TEST_CASE("VerilogGen - BitsUpdateLnodetype", "[verilog][bitsupdate]") {
    auto ctx = std::make_unique<ch::core::context>("bitsupdate_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> target(0_d);
    ch_uint<4> source(15_d);
    auto updated = bits_update<4>(target, source, 0);
    ch_out<ch_uint<8>> out_port("io");
    out_port = updated;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("[7:4]") != std::string::npos);
    REQUIRE(verilog.find("assign") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Wave 3: 12 new tests for the 12 new ch_op mappings added in Wave 2
// (and_reduce, or_reduce, xor_reduce, popcount, rotate_l, rotate_r,
//  bit_sel, bits_extract, zext, sext, assign)
// ---------------------------------------------------------------------------

// 1. AndReduce: ch_op::and_reduce -> `assign name = &src;` (1-bit output)
TEST_CASE("VerilogGen - AndReduce", "[verilog][reduce]") {
    auto ctx = std::make_unique<ch::core::context>("andreduce_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> a(255_d);
    auto reduced = and_reduce(a);
    ch_out<ch_bool> out_port("io");
    out_port = reduced;

    std::string verilog = generateVerilogToString(ctx.get());

    // The unary op node is named "and_reduce"; its emitted line is
    // `assign and_reduce = &uint_lit;` (or with _0 / _1 counter suffix).
    REQUIRE(verilog.find("and_reduce") != std::string::npos);
    REQUIRE(verilog.find(" = &") != std::string::npos);
    REQUIRE(verilog.find(";") != std::string::npos);
}

// 2. OrReduce: ch_op::or_reduce -> `assign name = |src;`
TEST_CASE("VerilogGen - OrReduce", "[verilog][reduce]") {
    auto ctx = std::make_unique<ch::core::context>("orreduce_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> a(0_d);
    auto reduced = or_reduce(a);
    ch_out<ch_bool> out_port("io");
    out_port = reduced;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("or_reduce") != std::string::npos);
    REQUIRE(verilog.find(" = |") != std::string::npos);
}

// 3. XorReduce: ch_op::xor_reduce -> `assign name = ^src;`
TEST_CASE("VerilogGen - XorReduce", "[verilog][reduce]") {
    auto ctx = std::make_unique<ch::core::context>("xorreduce_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> a(170_d); // 0b10101010 -> parity == 0
    auto reduced = xor_reduce(a);
    ch_out<ch_bool> out_port("io");
    out_port = reduced;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("xor_reduce") != std::string::npos);
    REQUIRE(verilog.find(" = ^") != std::string::npos);
}

// 4. Popcount: ch_op::popcount -> `assign name = $countones(src);`
//    For 8-bit input, result width is std::bit_width(8) = 4.
TEST_CASE("VerilogGen - Popcount", "[verilog][popcount]") {
    auto ctx = std::make_unique<ch::core::context>("popcount_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> a(15_d); // 0b00001111 -> popcount = 4
    auto count = popcount(a);
    ch_out<ch_uint<4>> out_port("io");
    out_port = count;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("popcount") != std::string::npos);
    REQUIRE(verilog.find("$countones(") != std::string::npos);
    REQUIRE(verilog.find("$countones(uint_lit)") != std::string::npos);
}

// 5. RotateLeft: ch_op::rotate_l with literal amount -> `assign name =
//    {src[N-1:N-amt], src[N-amt-1:0]};`. For 8-bit rotate by 3: {src[7:5],
//    src[4:0]}.
TEST_CASE("VerilogGen - RotateLeft", "[verilog][rotate]") {
    auto ctx = std::make_unique<ch::core::context>("rotatel_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> a(170_d);
    auto rotated = rotate_left(a, 3_d);
    ch_out<ch_uint<8>> out_port("io");
    out_port = rotated;

    std::string verilog = generateVerilogToString(ctx.get());

    // Slice [7:5] (high part) and [4:0] (low part) prove literal-amount
    // emit. Concat braces confirm the rotate_l path (not bits_extract).
    REQUIRE(verilog.find("[7:5]") != std::string::npos);
    REQUIRE(verilog.find("[4:0]") != std::string::npos);
    REQUIRE(verilog.find("{") != std::string::npos);
}

// 6. RotateRight: ch_op::rotate_r with literal amount -> `assign name =
//    {src[amt-1:0], src[N-1:amt]};`. For 8-bit rotate by 3: {src[2:0],
//    src[7:3]}.
TEST_CASE("VerilogGen - RotateRight", "[verilog][rotate]") {
    auto ctx = std::make_unique<ch::core::context>("rotater_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> a(170_d);
    auto rotated = rotate_right(a, 3_d);
    ch_out<ch_uint<8>> out_port("io");
    out_port = rotated;

    std::string verilog = generateVerilogToString(ctx.get());

    REQUIRE(verilog.find("[2:0]") != std::string::npos);
    REQUIRE(verilog.find("[7:3]") != std::string::npos);
    REQUIRE(verilog.find("{") != std::string::npos);
}

// 7. BitSelectLiteral: ch_op::bit_sel with compile-time index -> `assign
//    name = src[IDX];`.
TEST_CASE("VerilogGen - BitSelectLiteral", "[verilog][bitsel]") {
    auto ctx = std::make_unique<ch::core::context>("bitsel_lit_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> a(170_d);
    auto sel = bit_select<3>(a); // extract bit 3
    ch_out<ch_uint<1>> out_port("io");
    out_port = sel;

    std::string verilog = generateVerilogToString(ctx.get());

    // Compile-time index inlines the literal "3" inside the brackets.
    REQUIRE(verilog.find("bit_select") != std::string::npos);
    REQUIRE(verilog.find("[3]") != std::string::npos);
}

// 8. BitSelectVariable: ch_op::bit_sel with variable index -> `assign name
//    = src[idx];`. Use a ch_in<ch_uint<2>> as a non-constant index.
TEST_CASE("VerilogGen - BitSelectVariable", "[verilog][bitsel]") {
    auto ctx = std::make_unique<ch::core::context>("bitsel_var_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<2>> idx("idx");
    ch_uint<8> a(255_d);
    auto sel = bit_select(a, idx);
    ch_out<ch_uint<1>> out_port("io");
    out_port = sel;

    std::string verilog = generateVerilogToString(ctx.get());

    // Variable index means rhs is non-literal, so print_bit_select emits
    // the index node's name (here, the input port name "idx") verbatim.
    REQUIRE(verilog.find("bit_select") != std::string::npos);
    REQUIRE(verilog.find("[idx]") != std::string::npos);
}

// 9. BitsExtract: ch_op::bits_extract with template range -> `assign name
//    = src[MSB:LSB];`. Use bits<5, 2>(a) for slice [5:2].
TEST_CASE("VerilogGen - BitsExtract", "[verilog][bitsextract]") {
    auto ctx = std::make_unique<ch::core::context>("bitsextract_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> a(170_d);
    auto slice = bits<5, 2>(a); // 4-bit slice [5:2]
    ch_out<ch_uint<4>> out_port("io");
    out_port = slice;

    std::string verilog = generateVerilogToString(ctx.get());

    // The bits_extract op is named "bits" (not "bits_extract") — see
    // operators.h:585,598 where build_bits is called with literal name "bits".
    REQUIRE(verilog.find("bits") != std::string::npos);
    REQUIRE(verilog.find("[5:2]") != std::string::npos);
}

// 10. Zext: ch_op::zext with width growth -> `assign name = {{ext{1'b0}},
//     src};`. For 4->8 the replication is 4{1'b0}.
TEST_CASE("VerilogGen - Zext", "[verilog][extend]") {
    auto ctx = std::make_unique<ch::core::context>("zext_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<4> a(15_d);
    auto ext = zext<8>(a);
    ch_out<ch_uint<8>> out_port("io");
    out_port = ext;

    std::string verilog = generateVerilogToString(ctx.get());

    // The zero-fill replication proves the zext (not sext) path.
    REQUIRE(verilog.find("zext") != std::string::npos);
    REQUIRE(verilog.find("{1'b0}") != std::string::npos);
    REQUIRE(verilog.find("4{1'b0}") != std::string::npos);
}

// 11. Sext: ch_op::sext with width growth -> `assign name = {{ext{src[N-1]}},
//     src};`. For 4->8 the replication is 4{src[3]}.
TEST_CASE("VerilogGen - Sext", "[verilog][extend]") {
    auto ctx = std::make_unique<ch::core::context>("sext_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<4> a(15_d); // MSB (bit 3) = 1, so sign bit is 1
    auto ext = sext<8>(a);
    ch_out<ch_uint<8>> out_port("io");
    out_port = ext;

    std::string verilog = generateVerilogToString(ctx.get());

    // The sign-bit replication `{<ext>{src[<N-1>]}}` is the sext fingerprint.
    REQUIRE(verilog.find("sext") != std::string::npos);
    REQUIRE(verilog.find("4{uint_lit[3]}") != std::string::npos);
}

// 12. Assign: ch_op::assign is the ch_uint<>::operator<<= passthrough ->
//     `assign name = src;`. Triggers when the LHS is a default-constructed
//     ch_uint (no existing node_impl_), so a fresh wire is built.
TEST_CASE("VerilogGen - Assign", "[verilog][assign]") {
    auto ctx = std::make_unique<ch::core::context>("assign_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_uint<8> a(170_d);
    ch_uint<8> b;
    b <<= a; // ch_op::assign
    ch_out<ch_uint<8>> out_port("io");
    out_port = b;

    std::string verilog = generateVerilogToString(ctx.get());

    // The assign op node is named `<src_name>_wire`. Its emit is a pure
    // passthrough: `assign <src_name>_wire = <src_name>;`.
    REQUIRE(verilog.find("_wire = uint_lit;") != std::string::npos);
}
