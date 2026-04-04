// tests/test_axi_i2c.cpp
// AXI4-Lite I2C Controller Tests - Basic

#define CATCH_CONFIG_MAIN
#include "axi4/peripherals/axi_i2c.h"
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_verilog.h"
#include <memory>
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace axi4;

// ============================================================================
// Test Cases
// ============================================================================

TEST_CASE("AxiLiteI2c - ComponentCreation", "[axi][i2c][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_i2c_basic");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    // Create I2C instance
    AxiLiteI2c<16> i2c(nullptr, "i2c");
    
    // Verify component creation
    REQUIRE(i2c.name() == "i2c");
}

TEST_CASE("AxiLiteI2c - AxiLiteInterface", "[axi][i2c][interface]") {
    auto ctx = std::make_unique<ch::core::context>("test_i2c_axi");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiLiteI2c<16> i2c(nullptr, "i2c");
    
    // Verify IO ports exist (just check they compile)
    (void)i2c.io().awaddr;
    (void)i2c.io().awready;
    (void)i2c.io().wdata;
    (void)i2c.io().wready;
    (void)i2c.io().bresp;
    (void)i2c.io().bvalid;
    (void)i2c.io().araddr;
    (void)i2c.io().arready;
    (void)i2c.io().rdata;
    (void)i2c.io().rvalid;
}

TEST_CASE("AxiLiteI2c - I2cPhysicalInterface", "[axi][i2c][interface]") {
    auto ctx = std::make_unique<ch::core::context>("test_i2c_phy");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiLiteI2c<16> i2c(nullptr, "i2c");
    
    // I2C Physical Interface (verify ports exist)
    (void)i2c.io().i2c_sda;
    (void)i2c.io().i2c_sda_in;
    (void)i2c.io().i2c_scl;
    (void)i2c.io().i2c_scl_in;
    (void)i2c.io().irq;
}

TEST_CASE("AxiLiteI2c - VerilogGeneration", "[axi][i2c][verilog]") {
    auto ctx = std::make_unique<ch::core::context>("test_i2c_verilog");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLiteI2c<16>> i2c_device;
    
    // Generate Verilog
    std::string verilog_file = "test_axi_i2c.v";
    toVerilog(verilog_file, i2c_device.context());
    
    // Check file was created
    std::ifstream f(verilog_file);
    REQUIRE(f.good());
    
    // Check for key Verilog constructs
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    
    CHECK(content.find("module") != std::string::npos);
    CHECK(content.find("awaddr") != std::string::npos);
    CHECK(content.find("i2c_sda") != std::string::npos);
    CHECK(content.find("i2c_scl") != std::string::npos);
    
    std::cout << "Verilog generated: " << verilog_file << " (" 
              << content.size() << " bytes)" << std::endl;
}

TEST_CASE("AxiLiteI2c - DifferentPrescaleWidths", "[axi][i2c][config]") {
    {
        auto ctx = std::make_unique<ch::core::context>("test_i2c_8");
        ch::core::ctx_swap ctx_guard(ctx.get());
        AxiLiteI2c<8> i2c(nullptr, "i2c_8");
        REQUIRE(i2c.name() == "i2c_8");
    }
    {
        auto ctx = std::make_unique<ch::core::context>("test_i2c_16");
        ch::core::ctx_swap ctx_guard(ctx.get());
        AxiLiteI2c<16> i2c(nullptr, "i2c_16");
        REQUIRE(i2c.name() == "i2c_16");
    }
    {
        auto ctx = std::make_unique<ch::core::context>("test_i2c_32");
        ch::core::ctx_swap ctx_guard(ctx.get());
        AxiLiteI2c<32> i2c(nullptr, "i2c_32");
        REQUIRE(i2c.name() == "i2c_32");
    }
}
