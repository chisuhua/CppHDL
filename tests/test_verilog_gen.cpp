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
    REQUIRE(verilog_code.find("1'b1") != std::string::npos);

    // Should not have unnecessary ports
    REQUIRE(verilog_code.find("io_1") == std::string::npos);
}
