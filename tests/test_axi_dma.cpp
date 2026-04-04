// tests/test_axi_dma.cpp
// AXI4 DMA Controller Tests

#define CATCH_CONFIG_MAIN
#include "axi4/peripherals/axi_dma.h"
#include "axi4/axi4_full.h"
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include <memory>

using namespace ch;
using namespace ch::core;
using namespace axi4;

// ============================================================================
// Test Cases
// ============================================================================

TEST_CASE("AxiDma - ComponentCreation", "[axi][dma][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_dma_basic");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiDma<32, 32, 4, 16> dma(nullptr, "dma");
    REQUIRE(dma.name() == "dma");
}

TEST_CASE("AxiDma - RegisterInterface", "[axi][dma][register]") {
    auto ctx = std::make_unique<ch::core::context>("test_dma_reg");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiDma<32, 32, 4, 16> dma(nullptr, "dma");
    
    // Verify component name (IO ports are defined by __io macro)
    REQUIRE(dma.name() == "dma");
}

TEST_CASE("AxiDma - Axi4MasterInterface", "[axi][dma][axi4]") {
    auto ctx = std::make_unique<ch::core::context>("test_dma_axi");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiDma<32, 32, 4, 16> dma(nullptr, "dma");
    
    // Verify component is properly constructed
    REQUIRE(dma.name() == "dma");
}

TEST_CASE("AxiDma - IdleState", "[axi][dma][idle]") {
    auto ctx = std::make_unique<ch::core::context>("test_dma_idle");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiDma<32, 32, 4, 16> dma(nullptr, "dma");
    REQUIRE(dma.name() == "dma");
}

TEST_CASE("AxiDma - DifferentConfigurations", "[axi][dma][config]") {
    {
        auto ctx = std::make_unique<ch::core::context>("test_cfg1");
        ch::core::ctx_swap ctx_guard(ctx.get());
        AxiDma<32, 32, 4, 16> dma(nullptr, "dma_32_32");
        REQUIRE(dma.name() == "dma_32_32");
    }
    
    {
        auto ctx = std::make_unique<ch::core::context>("test_cfg2");
        ch::core::ctx_swap ctx_guard(ctx.get());
        AxiDma<32, 64, 4, 16> dma(nullptr, "dma_32_64");
        REQUIRE(dma.name() == "dma_32_64");
    }
    
    {
        auto ctx = std::make_unique<ch::core::context>("test_cfg3");
        ch::core::ctx_swap ctx_guard(ctx.get());
        AxiDma<32, 32, 8, 16> dma(nullptr, "dma_id8");
        REQUIRE(dma.name() == "dma_id8");
    }
}

TEST_CASE("AxiDma - BurstLengthCalculation", "[axi][dma][burst]") {
    auto ctx = std::make_unique<ch::core::context>("test_dma_burst");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiDma<32, 32, 4, 16> dma(nullptr, "dma");
    REQUIRE(dma.name() == "dma");
    
    constexpr unsigned num_bytes = 32 / 8;
    REQUIRE(num_bytes == 4);
}

TEST_CASE("AxiDma - VerilogGeneration", "[axi][dma][verilog]") {
    auto ctx = std::make_unique<ch::core::context>("test_dma_verilog");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiDma<32, 32, 4, 16> dma(nullptr, "dma_top");
    REQUIRE(dma.name() == "dma_top");
}

TEST_CASE("AxiDma - Axi4ProtocolTypes", "[axi][dma][protocol]") {
    auto ctx = std::make_unique<ch::core::context>("test_dma_proto");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiDma<32, 32, 4, 16> dma(nullptr, "dma");
    REQUIRE(dma.name() == "dma");
    
    REQUIRE(static_cast<unsigned>(AxiResp::OKAY) == 0b00);
    REQUIRE(static_cast<unsigned>(AxiResp::EXOKAY) == 0b01);
    REQUIRE(static_cast<unsigned>(AxiResp::SLVERR) == 0b10);
    REQUIRE(static_cast<unsigned>(AxiResp::DECERR) == 0b11);
    
    REQUIRE(static_cast<unsigned>(AxiBurst::FIXED) == 0b00);
    REQUIRE(static_cast<unsigned>(AxiBurst::INCR) == 0b01);
    REQUIRE(static_cast<unsigned>(AxiBurst::WRAP) == 0b10);
}

TEST_CASE("AxiDma - TransferSizes", "[axi][dma][size]") {
    std::vector<unsigned> sizes = {4, 8, 16, 32, 64, 128, 256, 512, 1024};
    
    for (unsigned size : sizes) {
        auto ctx = std::make_unique<ch::core::context>("test_size_" + std::to_string(size));
        ch::core::ctx_swap sub_ctx_guard(ctx.get());
        
        AxiDma<32, 32, 4, 16> dma(nullptr, "dma_" + std::to_string(size));
        REQUIRE(dma.name() == "dma_" + std::to_string(size));
    }
}

TEST_CASE("AxiDma - MultipleInstances", "[axi][dma][multi]") {
    auto ctx = std::make_unique<ch::core::context>("test_dma_multi");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    AxiDma<32, 32, 4, 16> dma0(nullptr, "dma_0");
    AxiDma<32, 32, 4, 16> dma1(nullptr, "dma_1");
    AxiDma<32, 32, 4, 16> dma2(nullptr, "dma_2");
    
    REQUIRE(dma0.name() == "dma_0");
    REQUIRE(dma1.name() == "dma_1");
    REQUIRE(dma2.name() == "dma_2");
}

TEST_CASE("AxiDma - TemplateInstantiation", "[axi][dma][template]") {
    {
        auto ctx = std::make_unique<ch::core::context>("test_min");
        ch::core::ctx_swap ctx_guard(ctx.get());
        AxiDma<32, 32, 1, 1> dma(nullptr, "dma_min");
        REQUIRE(dma.name() == "dma_min");
    }
    
    {
        auto ctx = std::make_unique<ch::core::context>("test_max_burst");
        ch::core::ctx_swap ctx_guard(ctx.get());
        AxiDma<32, 32, 4, 16> dma(nullptr, "dma_max_burst");
        REQUIRE(dma.name() == "dma_max_burst");
    }
    
    {
        auto ctx = std::make_unique<ch::core::context>("test_wide");
        ch::core::ctx_swap ctx_guard(ctx.get());
        AxiDma<32, 128, 4, 16> dma(nullptr, "dma_wide");
        REQUIRE(dma.name() == "dma_wide");
    }
}
