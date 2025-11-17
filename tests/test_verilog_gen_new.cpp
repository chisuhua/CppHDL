// tests/test_verilog_gen_new.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "core/context.h"
#include "codegen.h"
#include <memory>
#include <sstream>

using namespace ch;
using namespace ch::core;

// Helper function to capture Verilog output as a string
std::string generateVerilogToString(context* ctx) {
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