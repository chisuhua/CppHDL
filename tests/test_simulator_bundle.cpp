// tests/test_simulator_bundle.cpp
// T401: Simulator API扩展测试 - 不使用 Component，直接测试 API

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"

using namespace ch::core;

// T401 测试：直接测试 ch_uint<N> 的 set_input_value/get_input_value
TEST_CASE("T401 - ch_uint set_input_value without Component", "[T401][simulator]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    // 创建一个独立的 ch_uint 变量（不是 Component IO）
    ch_uint<8> signal(0_d);
    
    // 创建 Simulator
    Simulator sim(ctx.get());
    
    // T401.2: 使用扩展的 set_input_value
    sim.set_input_value(signal, 0xA5);
    
    // T401.3: 读取值
    auto val = sim.get_value(signal);
    
    REQUIRE(static_cast<uint64_t>(val) == 0xA5);
}

TEST_CASE("T401 - get_input_value for ch_uint", "[T401][simulator]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_uint<16> signal(0_d);
    Simulator sim(ctx.get());
    
    const uint64_t TEST_VALUE = 0x1234;
    
    // 设置值
    sim.set_value(signal, TEST_VALUE);
    
    // T401.3: 使用 get_input_value
    auto val = sim.get_input_value(signal);
    
    REQUIRE(static_cast<uint64_t>(val) == TEST_VALUE);
}

TEST_CASE("T401 - ch_uint access different widths", "[T401][simulator]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_uint<4> n4(0_d);
    ch_uint<8> n8(0_d);
    ch_uint<32> n32(0_d);
    
    Simulator sim(ctx.get());
    
    // 测试不同位宽
    sim.set_input_value(n4, 0xA);
    sim.set_input_value(n8, 0xA5);
    sim.set_input_value(n32, 0xA5A5A5A5);
    
    REQUIRE(static_cast<uint64_t>(sim.get_input_value(n4)) == 0xA);
    REQUIRE(static_cast<uint64_t>(sim.get_input_value(n8)) == 0xA5);
    REQUIRE(static_cast<uint64_t>(sim.get_input_value(n32)) == 0xA5A5A5A5);
}
