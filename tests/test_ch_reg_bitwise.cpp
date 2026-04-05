#include "catch_amalgamated.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/traits.h"
#include "core/context.h"
#include "core/operators.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;

// 测试 ch_reg 位操作 bug 复现
// 这些测试应该在仿真时触发 SIGSEGV，用于定位问题

TEST_CASE("ch_reg bitwise AND with simulation", "[ch_reg][bitwise][sigsegv]") {
    auto ctx = std::make_unique<ch::core::context>("test_ch_reg_bitwise_and");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_reg<ch_uint<32>> reg(0xFF_h);
    auto result = reg & ch_uint<32>(1_d);
    
    // 尝试仿真，这应该会触发 SIGSEGV
    ch::Simulator sim(ctx.get());
    sim.tick();
    
    REQUIRE(result.impl() != nullptr);
}

TEST_CASE("ch_reg bits extraction with simulation", "[ch_reg][bits][sigsegv]") {
    auto ctx = std::make_unique<ch::core::context>("test_ch_reg_bits");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_reg<ch_uint<32>> reg(0xFF_h);
    auto bit0 = bits<0, 0>(reg);
    
    // 尝试仿真，这应该会触发 SIGSEGV
    ch::Simulator sim(ctx.get());
    sim.tick();
    
    REQUIRE(bit0.impl() != nullptr);
}

TEST_CASE("ch_reg bitwise NOT with simulation", "[ch_reg][bitwise][sigsegv]") {
    auto ctx = std::make_unique<ch::core::context>("test_ch_reg_not");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_reg<ch_uint<32>> reg(0_h);
    auto result = ~reg;
    
    // 尝试仿真，这应该会触发 SIGSEGV
    ch::Simulator sim(ctx.get());
    sim.tick();
    
    REQUIRE(result.impl() != nullptr);
}

TEST_CASE("ch_reg bitwise OR with simulation", "[ch_reg][bitwise][sigsegv]") {
    auto ctx = std::make_unique<ch::core::context>("test_ch_reg_or");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_reg<ch_uint<32>> reg(0x0F_h);
    auto result = reg | ch_uint<32>(0xF0_h);
    
    // 尝试仿真，这应该会触发 SIGSEGV
    ch::Simulator sim(ctx.get());
    sim.tick();
    
    REQUIRE(result.impl() != nullptr);
}

TEST_CASE("ch_reg bitwise XOR with simulation", "[ch_reg][bitwise][sigsegv]") {
    auto ctx = std::make_unique<ch::core::context>("test_ch_reg_xor");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_reg<ch_uint<32>> reg(0xFF_h);
    auto result = reg ^ ch_uint<32>(0x0F_h);
    
    // 尝试仿真，这应该会触发 SIGSEGV
    ch::Simulator sim(ctx.get());
    sim.tick();
    
    REQUIRE(result.impl() != nullptr);
}

TEST_CASE("ch_reg bits extraction multi-bit with simulation", "[ch_reg][bits][sigsegv]") {
    auto ctx = std::make_unique<ch::core::context>("test_ch_reg_bits_multi");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_reg<ch_uint<32>> reg(0xFF_h);
    auto bits_3_0 = bits<3, 0>(reg);
    
    // 尝试仿真，这应该会触发 SIGSEGV
    ch::Simulator sim(ctx.get());
    sim.tick();
    
    REQUIRE(bits_3_0.impl() != nullptr);
}
