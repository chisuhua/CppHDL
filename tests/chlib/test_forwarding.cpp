#include "catch_amalgamated.hpp"
#include "cpu/forwarding.h"
#include "codegen_dag.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include "utils/format_utils.h"
#include <iostream>
#include <memory>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// ============================================================================
// 测试 1: ForwardingUnit 基本 RAW 冒险检测 (使用组件)
// ============================================================================
TEST_CASE("ForwardingUnit: Component RAW hazard detection", "[forwarding][raw_hazard][component]") {

    SECTION("No hazard - registers don't match") {
        auto ctx = std::make_unique<context>("test_fwd_no_hazard");
        ctx_swap ctx_swapper(ctx.get());

        // 使用函数式接口创建前推检测逻辑
        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入：RS1=5, RS2=6, EX_RD=10, MEM_RD=11 (无匹配)
        sim.set_value(rs1_addr, 5);
        sim.set_value(rs2_addr, 6);
        sim.set_value(ex_rd, 10);
        sim.set_value(ex_reg_write, 1);
        sim.set_value(mem_rd, 11);
        sim.set_value(mem_reg_write, 1);
        sim.tick();

        // 验证：无前推，应该选择 REG (0)
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 0);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_b)) == 0);
    }

    SECTION("EX stage forwarding - RS1 matches EX_RD") {
        auto ctx = std::make_unique<context>("test_fwd_ex_hazard");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入：RS1=5, EX_RD=5 (匹配), MEM_RD=11
        sim.set_value(rs1_addr, 5);
        sim.set_value(rs2_addr, 6);
        sim.set_value(ex_rd, 5);
        sim.set_value(ex_reg_write, 1);
        sim.set_value(mem_rd, 11);
        sim.set_value(mem_reg_write, 1);
        sim.tick();

        // 验证：RS1 应该前推到 EX (1), RS2 无前推 (0)
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 1);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_b)) == 0);
    }

    SECTION("MEM stage forwarding - RS1 matches MEM_RD") {
        auto ctx = std::make_unique<context>("test_fwd_mem_hazard");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入：RS1=7, EX_RD=10, MEM_RD=7 (匹配)
        sim.set_value(rs1_addr, 7);
        sim.set_value(rs2_addr, 8);
        sim.set_value(ex_rd, 10);
        sim.set_value(ex_reg_write, 1);
        sim.set_value(mem_rd, 7);
        sim.set_value(mem_reg_write, 1);
        sim.tick();

        // 验证：RS1 应该前推到 MEM (2), RS2 无前推 (0)
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 2);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_b)) == 0);
    }

    SECTION("Priority test - EX takes precedence over MEM") {
        auto ctx = std::make_unique<context>("test_fwd_priority");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入：RS1=5, EX_RD=5, MEM_RD=5 (都匹配)
        sim.set_value(rs1_addr, 5);
        sim.set_value(rs2_addr, 6);
        sim.set_value(ex_rd, 5);
        sim.set_value(ex_reg_write, 1);
        sim.set_value(mem_rd, 5);
        sim.set_value(mem_reg_write, 1);
        sim.tick();

        // 验证：EX 优先级高于 MEM，应该选择 EX (1)
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 1);
    }

    SECTION("Zero register - no forwarding for x0") {
        auto ctx = std::make_unique<context>("test_fwd_zero_reg");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入：RS1=0 (x0), EX_RD=0, MEM_RD=0
        sim.set_value(rs1_addr, 0);
        sim.set_value(rs2_addr, 6);
        sim.set_value(ex_rd, 0);
        sim.set_value(ex_reg_write, 1);
        sim.set_value(mem_rd, 0);
        sim.set_value(mem_reg_write, 1);
        sim.tick();

        // 验证：x0 寄存器不应该前推
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 0);
    }

    SECTION("Write disable - no forwarding when reg_write=0") {
        auto ctx = std::make_unique<context>("test_fwd_no_write");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入：地址匹配但 reg_write=0
        sim.set_value(rs1_addr, 5);
        sim.set_value(rs2_addr, 6);
        sim.set_value(ex_rd, 5);
        sim.set_value(ex_reg_write, 0);
        sim.set_value(mem_rd, 5);
        sim.set_value(mem_reg_write, 0);
        sim.tick();

        // 验证：无写使能，不应该前推
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 0);
    }
}

// ============================================================================
// 测试 2: ForwardingMux 数据选择 (使用函数式接口)
// ============================================================================
TEST_CASE("ForwardingMux: Data selection", "[forwarding][mux][functional]") {

    SECTION("Select register data (forward=0)") {
        auto ctx = std::make_unique<context>("test_mux_reg");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<2> forward_a = ch_uint<2>(0_d);
        ch_uint<2> forward_b = ch_uint<2>(0_d);
        ch_uint<32> rs1_data_reg = ch_uint<32>(0_d);
        ch_uint<32> rs2_data_reg = ch_uint<32>(0_d);
        ch_uint<32> ex_result = ch_uint<32>(0_d);
        ch_uint<32> mem_result = ch_uint<32>(0_d);

        auto result = forward_select<32>(forward_a, forward_b, rs1_data_reg, rs2_data_reg, ex_result, mem_result);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入数据
        sim.set_value(rs1_data_reg, 0x12345678);
        sim.set_value(rs2_data_reg, 0x87654321);
        sim.set_value(ex_result, 0xDEADBEEF);
        sim.set_value(mem_result, 0xCAFEBABE);
        sim.set_value(forward_a, 0);
        sim.set_value(forward_b, 0);
        sim.tick();

        // 验证：输出应该等于寄存器数据
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.alu_input_a)) == 0x12345678);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.alu_input_b)) == 0x87654321);
    }

    SECTION("Select EX result (forward=1)") {
        auto ctx = std::make_unique<context>("test_mux_ex");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<2> forward_a = ch_uint<2>(0_d);
        ch_uint<2> forward_b = ch_uint<2>(0_d);
        ch_uint<32> rs1_data_reg = ch_uint<32>(0_d);
        ch_uint<32> rs2_data_reg = ch_uint<32>(0_d);
        ch_uint<32> ex_result = ch_uint<32>(0_d);
        ch_uint<32> mem_result = ch_uint<32>(0_d);

        auto result = forward_select<32>(forward_a, forward_b, rs1_data_reg, rs2_data_reg, ex_result, mem_result);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入数据
        sim.set_value(rs1_data_reg, 0x12345678);
        sim.set_value(rs2_data_reg, 0x87654321);
        sim.set_value(ex_result, 0xDEADBEEF);
        sim.set_value(mem_result, 0xCAFEBABE);
        sim.set_value(forward_a, 1);
        sim.set_value(forward_b, 1);
        sim.tick();

        // 验证：输出应该等于 EX 结果
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.alu_input_a)) == 0xDEADBEEF);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.alu_input_b)) == 0xDEADBEEF);
    }

    SECTION("Select MEM result (forward=2)") {
        auto ctx = std::make_unique<context>("test_mux_mem");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<2> forward_a = ch_uint<2>(0_d);
        ch_uint<2> forward_b = ch_uint<2>(0_d);
        ch_uint<32> rs1_data_reg = ch_uint<32>(0_d);
        ch_uint<32> rs2_data_reg = ch_uint<32>(0_d);
        ch_uint<32> ex_result = ch_uint<32>(0_d);
        ch_uint<32> mem_result = ch_uint<32>(0_d);

        auto result = forward_select<32>(forward_a, forward_b, rs1_data_reg, rs2_data_reg, ex_result, mem_result);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入数据
        sim.set_value(rs1_data_reg, 0x12345678);
        sim.set_value(rs2_data_reg, 0x87654321);
        sim.set_value(ex_result, 0xDEADBEEF);
        sim.set_value(mem_result, 0xCAFEBABE);
        sim.set_value(forward_a, 2);
        sim.set_value(forward_b, 2);
        sim.tick();

        // 验证：输出应该等于 MEM 结果
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.alu_input_a)) == 0xCAFEBABE);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.alu_input_b)) == 0xCAFEBABE);
    }

    SECTION("Mixed selection - RS1 from EX, RS2 from REG") {
        auto ctx = std::make_unique<context>("test_mux_mixed");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<2> forward_a = ch_uint<2>(0_d);
        ch_uint<2> forward_b = ch_uint<2>(0_d);
        ch_uint<32> rs1_data_reg = ch_uint<32>(0_d);
        ch_uint<32> rs2_data_reg = ch_uint<32>(0_d);
        ch_uint<32> ex_result = ch_uint<32>(0_d);
        ch_uint<32> mem_result = ch_uint<32>(0_d);

        auto result = forward_select<32>(forward_a, forward_b, rs1_data_reg, rs2_data_reg, ex_result, mem_result);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置输入数据
        sim.set_value(rs1_data_reg, 0x11111111);
        sim.set_value(rs2_data_reg, 0x22222222);
        sim.set_value(ex_result, 0x33333333);
        sim.set_value(mem_result, 0x44444444);
        sim.set_value(forward_a, 1);
        sim.set_value(forward_b, 0);
        sim.tick();

        // 验证：RS1=EX 结果，RS2=寄存器数据
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.alu_input_a)) == 0x33333333);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.alu_input_b)) == 0x22222222);
    }
}

// ============================================================================
// 测试 3: ForwardingUnit + ForwardingMux 集成测试
// ============================================================================
TEST_CASE("ForwardingUnit + ForwardingMux: Integration test", "[forwarding][integration]") {

    SECTION("Complete RAW hazard resolution") {
        auto ctx = std::make_unique<context>("test_fwd_integration");
        ctx_swap ctx_swapper(ctx.get());

        // 创建 ForwardingUnit 的输入信号
        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        // 前推检测结果
        auto fwd_result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        // ForwardingMux 的输入信号
        ch_uint<32> rs1_data_reg = ch_uint<32>(0_d);
        ch_uint<32> rs2_data_reg = ch_uint<32>(0_d);
        ch_uint<32> ex_result = ch_uint<32>(0_d);
        ch_uint<32> mem_result = ch_uint<32>(0_d);

        // 前推后的 ALU 输入
        auto mux_result = forward_select<32>(fwd_result.forward_a, fwd_result.forward_b,
                                              rs1_data_reg, rs2_data_reg, ex_result, mem_result);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 模拟 RAW 冒险场景:
        // ADD x1, x2, x3  (EX 阶段，结果将写入 x1)
        // SUB x4, x1, x5  (ID 阶段，需要读取 x1)
        
        // 设置 ForwardingUnit 输入
        sim.set_value(rs1_addr, 1);   // RS1 = x1
        sim.set_value(rs2_addr, 5);   // RS2 = x5
        sim.set_value(ex_rd, 1);      // EX_RD = x1 (匹配!)
        sim.set_value(ex_reg_write, 1);
        sim.set_value(mem_rd, 10);
        sim.set_value(mem_reg_write, 0);

        // 设置 ForwardingMux 数据输入
        sim.set_value(rs1_data_reg, 0xAAAAAAAA);  // 寄存器文件中的旧值
        sim.set_value(rs2_data_reg, 0x55555555);
        sim.set_value(ex_result, 0x12345678);     // EX 阶段的新值 (应该前推)
        sim.set_value(mem_result, 0xDEADBEEF);
        sim.tick();

        // 验证：RS1 应该前推 EX 结果，RS2 使用寄存器数据
        REQUIRE(static_cast<uint64_t>(sim.get_value(fwd_result.forward_a)) == 1);
        REQUIRE(static_cast<uint64_t>(sim.get_value(fwd_result.forward_b)) == 0);
        REQUIRE(static_cast<uint64_t>(sim.get_value(mux_result.alu_input_a)) == 0x12345678);
        REQUIRE(static_cast<uint64_t>(sim.get_value(mux_result.alu_input_b)) == 0x55555555);
    }
}

// ============================================================================
// 测试 4: 不同配置参数测试
// ============================================================================
TEST_CASE("Forwarding: Different configurations", "[forwarding][config]") {

    SECTION("8-bit register address width") {
        auto ctx = std::make_unique<context>("test_fwd_8bit");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<8> rs1_addr = ch_uint<8>(0_d);
        ch_uint<8> rs2_addr = ch_uint<8>(0_d);
        ch_uint<8> ex_rd = ch_uint<8>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<8> mem_rd = ch_uint<8>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<8>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 测试大地址值
        sim.set_value(rs1_addr, 100);
        sim.set_value(rs2_addr, 200);
        sim.set_value(ex_rd, 100);
        sim.set_value(ex_reg_write, 1);
        sim.set_value(mem_rd, 0);
        sim.set_value(mem_reg_write, 0);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 1);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_b)) == 0);
    }

    SECTION("64-bit data width") {
        auto ctx = std::make_unique<context>("test_fwd_64bit");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<2> forward_a = ch_uint<2>(0_d);
        ch_uint<2> forward_b = ch_uint<2>(0_d);
        ch_uint<64> rs1_data_reg = ch_uint<64>(0_d);
        ch_uint<64> rs2_data_reg = ch_uint<64>(0_d);
        ch_uint<64> ex_result = ch_uint<64>(0_d);
        ch_uint<64> mem_result = ch_uint<64>(0_d);

        auto result = forward_select<64>(forward_a, forward_b, rs1_data_reg, rs2_data_reg, ex_result, mem_result);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 设置 64 位数据
        sim.set_value(rs1_data_reg, 0x1234567890ABCDEFULL);
        sim.set_value(ex_result, 0xFEDCBA0987654321ULL);
        sim.set_value(mem_result, 0x1111111111111111ULL);
        sim.set_value(forward_a, 1);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(sim.get_value(result.alu_input_a)) == 0xFEDCBA0987654321ULL);
    }
}

// ============================================================================
// 测试 5: 边界条件测试
// ============================================================================
TEST_CASE("ForwardingUnit: Edge cases", "[forwarding][edge]") {

    SECTION("Maximum register address (5-bit)") {
        auto ctx = std::make_unique<context>("test_fwd_max_addr");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 测试最大地址 31 (5 位)
        sim.set_value(rs1_addr, 31);
        sim.set_value(rs2_addr, 30);
        sim.set_value(ex_rd, 31);
        sim.set_value(ex_reg_write, 1);
        sim.set_value(mem_rd, 0);
        sim.set_value(mem_reg_write, 0);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 1);
    }

    SECTION("Both stages disabled") {
        auto ctx = std::make_unique<context>("test_fwd_both_disabled");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 两个阶段都禁用写
        sim.set_value(rs1_addr, 5);
        sim.set_value(rs2_addr, 6);
        sim.set_value(ex_rd, 5);
        sim.set_value(ex_reg_write, 0);
        sim.set_value(mem_rd, 5);
        sim.set_value(mem_reg_write, 0);
        sim.tick();

        // 验证：无前推
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 0);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_b)) == 0);
    }

    SECTION("RS2 hazard only") {
        auto ctx = std::make_unique<context>("test_fwd_rs2_only");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<5> rs1_addr = ch_uint<5>(0_d);
        ch_uint<5> rs2_addr = ch_uint<5>(0_d);
        ch_uint<5> ex_rd = ch_uint<5>(0_d);
        ch_bool ex_reg_write = ch_bool(false);
        ch_uint<5> mem_rd = ch_uint<5>(0_d);
        ch_bool mem_reg_write = ch_bool(false);

        auto result = forward_detect<5>(rs1_addr, rs2_addr, ex_rd, ex_reg_write, mem_rd, mem_reg_write);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 只有 RS2 有冒险
        sim.set_value(rs1_addr, 10);
        sim.set_value(rs2_addr, 5);
        sim.set_value(ex_rd, 5);
        sim.set_value(ex_reg_write, 1);
        sim.set_value(mem_rd, 0);
        sim.set_value(mem_reg_write, 0);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_a)) == 0);
        REQUIRE(static_cast<uint64_t>(sim.get_value(result.forward_b)) == 1);
    }
}
