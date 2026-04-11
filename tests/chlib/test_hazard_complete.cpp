/**
 * @file test_hazard_complete.cpp
 * @brief 完整 Hazard Unit 功能测试
 * 
 * 验证 rv32i_hazard_complete.h 组件的编译和正确性
 */

#include "catch_amalgamated.hpp"
#include "../examples/riscv-mini/src/rv32i_hazard_complete.h"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"
#include <memory>

using namespace riscv;
using namespace ch;

TEST_CASE("HazardUnitComplete - Type Check", "[hazard][complete]") {
    ch::core::context ctx("hazard_complete");
    ch::core::ctx_swap swap(&ctx);
    
    HazardUnitComplete hu{nullptr, "hazard"};
    
    REQUIRE(true);
}

TEST_CASE("HazardUnitComplete - Forwarding Logic", "[hazard][forwarding]") {
    auto ctx = std::make_unique<ch::core::context>("hazard_fwd");
    ch::core::ctx_swap swap(ctx.get());
    
    ch::ch_device<HazardUnitComplete> hazard_device;
    ch::Simulator sim(hazard_device.context());
    
    // 初始化输入
    sim.set_input_value(hazard_device.instance().io().id_rs1_addr, 5);
    sim.set_input_value(hazard_device.instance().io().id_rs2_addr, 6);
    sim.set_input_value(hazard_device.instance().io().ex_rd_addr, 5);
    sim.set_input_value(hazard_device.instance().io().ex_reg_write, 1);
    sim.set_input_value(hazard_device.instance().io().mem_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().mem_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().wb_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().wb_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch_taken, 0);
    sim.set_input_value(hazard_device.instance().io().mem_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().ex_alu_result, 42);
    sim.set_input_value(hazard_device.instance().io().mem_alu_result, 0);
    sim.set_input_value(hazard_device.instance().io().wb_write_data, 0);
    sim.set_input_value(hazard_device.instance().io().rs1_data_reg, 100);
    sim.set_input_value(hazard_device.instance().io().rs2_data_reg, 200);
    
    sim.tick();
    sim.tick();
    
    // 验证：RS1 应该前推到 EX (1)，因为 id_rs1_addr(5) == ex_rd_addr(5) && ex_reg_write == 1
    auto rs1_src = sim.get_port_value(hazard_device.instance().io().rs1_src);
    REQUIRE(static_cast<uint64_t>(rs1_src) == 1);
    
    // RS2 应该来自 REG (0)，因为 id_rs2_addr(6) 不匹配任何前推导出
    auto rs2_src = sim.get_port_value(hazard_device.instance().io().rs2_src);
    REQUIRE(static_cast<uint64_t>(rs2_src) == 0);
}

TEST_CASE("HazardUnitComplete - RS1 Forwarding", "[hazard][rs1]") {
    auto ctx = std::make_unique<ch::core::context>("hazard_rs1");
    ch::core::ctx_swap swap(ctx.get());
    
    ch::ch_device<HazardUnitComplete> hazard_device;
    ch::Simulator sim(hazard_device.context());
    
    sim.set_input_value(hazard_device.instance().io().id_rs1_addr, 10);
    sim.set_input_value(hazard_device.instance().io().ex_rd_addr, 10);
    sim.set_input_value(hazard_device.instance().io().ex_reg_write, 1);
    sim.set_input_value(hazard_device.instance().io().mem_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().mem_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().wb_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().wb_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch_taken, 0);
    sim.set_input_value(hazard_device.instance().io().mem_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().ex_alu_result, 999);
    sim.set_input_value(hazard_device.instance().io().mem_alu_result, 0);
    sim.set_input_value(hazard_device.instance().io().wb_write_data, 0);
    sim.set_input_value(hazard_device.instance().io().rs1_data_reg, 1);
    sim.set_input_value(hazard_device.instance().io().rs2_data_reg, 2);
    sim.set_input_value(hazard_device.instance().io().id_rs2_addr, 0);
    
    sim.tick();
    sim.tick();
    
    auto rs1_data = sim.get_port_value(hazard_device.instance().io().rs1_data);
    REQUIRE(static_cast<uint64_t>(rs1_data) == 999);
}

TEST_CASE("HazardUnitComplete - RS2 Forwarding", "[hazard][rs2]") {
    auto ctx = std::make_unique<ch::core::context>("hazard_rs2");
    ch::core::ctx_swap swap(ctx.get());
    
    ch::ch_device<HazardUnitComplete> hazard_device;
    ch::Simulator sim(hazard_device.context());
    
    sim.set_input_value(hazard_device.instance().io().id_rs2_addr, 20);
    sim.set_input_value(hazard_device.instance().io().ex_rd_addr, 20);
    sim.set_input_value(hazard_device.instance().io().ex_reg_write, 1);
    sim.set_input_value(hazard_device.instance().io().mem_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().mem_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().wb_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().wb_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch_taken, 0);
    sim.set_input_value(hazard_device.instance().io().mem_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().ex_alu_result, 888);
    sim.set_input_value(hazard_device.instance().io().mem_alu_result, 0);
    sim.set_input_value(hazard_device.instance().io().wb_write_data, 0);
    sim.set_input_value(hazard_device.instance().io().rs1_data_reg, 1);
    sim.set_input_value(hazard_device.instance().io().rs2_data_reg, 3);
    sim.set_input_value(hazard_device.instance().io().id_rs1_addr, 0);
    
    sim.tick();
    sim.tick();
    
    auto rs2_data = sim.get_port_value(hazard_device.instance().io().rs2_data);
    REQUIRE(static_cast<uint64_t>(rs2_data) == 888);
}

TEST_CASE("HazardUnitComplete - Load-Use Detection", "[hazard][load-use]") {
    auto ctx = std::make_unique<ch::core::context>("hazard_load");
    ch::core::ctx_swap swap(ctx.get());
    
    ch::ch_device<HazardUnitComplete> hazard_device;
    ch::Simulator sim(hazard_device.context());
    
    // 模拟 Load-Use 冒险：MEM 是 Load，且目的寄存器与 RS1 匹配
    sim.set_input_value(hazard_device.instance().io().mem_is_load, 1);
    sim.set_input_value(hazard_device.instance().io().mem_rd_addr, 15);
    sim.set_input_value(hazard_device.instance().io().id_rs1_addr, 15);
    sim.set_input_value(hazard_device.instance().io().id_rs2_addr, 0);
    sim.set_input_value(hazard_device.instance().io().ex_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().ex_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().mem_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().wb_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().wb_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch_taken, 0);
    sim.set_input_value(hazard_device.instance().io().ex_alu_result, 0);
    sim.set_input_value(hazard_device.instance().io().mem_alu_result, 0);
    sim.set_input_value(hazard_device.instance().io().wb_write_data, 0);
    sim.set_input_value(hazard_device.instance().io().rs1_data_reg, 1);
    sim.set_input_value(hazard_device.instance().io().rs2_data_reg, 2);
    
    sim.tick();
    sim.tick();
    
    auto stall = sim.get_port_value(hazard_device.instance().io().stall);
    REQUIRE(static_cast<uint64_t>(stall) == 1);
}

TEST_CASE("HazardUnitComplete - Branch Flush", "[hazard][branch]") {
    auto ctx = std::make_unique<ch::core::context>("hazard_branch");
    ch::core::ctx_swap swap(ctx.get());
    
    ch::ch_device<HazardUnitComplete> hazard_device;
    ch::Simulator sim(hazard_device.context());
    
    sim.set_input_value(hazard_device.instance().io().ex_branch, 1);
    sim.set_input_value(hazard_device.instance().io().id_rs1_addr, 0);
    sim.set_input_value(hazard_device.instance().io().id_rs2_addr, 0);
    sim.set_input_value(hazard_device.instance().io().ex_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().ex_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().mem_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().mem_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().wb_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().wb_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch_taken, 0);
    sim.set_input_value(hazard_device.instance().io().mem_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().ex_alu_result, 0);
    sim.set_input_value(hazard_device.instance().io().mem_alu_result, 0);
    sim.set_input_value(hazard_device.instance().io().wb_write_data, 0);
    sim.set_input_value(hazard_device.instance().io().rs1_data_reg, 1);
    sim.set_input_value(hazard_device.instance().io().rs2_data_reg, 2);
    
    sim.tick();
    sim.tick();
    
    auto flush = sim.get_port_value(hazard_device.instance().io().flush);
    auto pc_src = sim.get_port_value(hazard_device.instance().io().pc_src);
    REQUIRE(static_cast<uint64_t>(flush) == 1);
    REQUIRE(static_cast<uint64_t>(pc_src) == 1);
}

TEST_CASE("HazardUnitComplete - x0 Protection", "[hazard][x0]") {
    auto ctx = std::make_unique<ch::core::context>("hazard_x0");
    ch::core::ctx_swap swap(ctx.get());
    
    ch::ch_device<HazardUnitComplete> hazard_device;
    ch::Simulator sim(hazard_device.context());
    
    // 即使 ex_rd_addr 是 0（x0），也不应该前推
    sim.set_input_value(hazard_device.instance().io().id_rs1_addr, 0);  // x0
    sim.set_input_value(hazard_device.instance().io().ex_rd_addr, 0);   // x0
    sim.set_input_value(hazard_device.instance().io().ex_reg_write, 1);
    sim.set_input_value(hazard_device.instance().io().mem_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().mem_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().wb_rd_addr, 0);
    sim.set_input_value(hazard_device.instance().io().wb_reg_write, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().id_is_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch, 0);
    sim.set_input_value(hazard_device.instance().io().ex_branch_taken, 0);
    sim.set_input_value(hazard_device.instance().io().mem_is_load, 0);
    sim.set_input_value(hazard_device.instance().io().ex_alu_result, 42);
    sim.set_input_value(hazard_device.instance().io().mem_alu_result, 0);
    sim.set_input_value(hazard_device.instance().io().wb_write_data, 0);
    sim.set_input_value(hazard_device.instance().io().rs1_data_reg, 99);
    sim.set_input_value(hazard_device.instance().io().rs2_data_reg, 0);
    sim.set_input_value(hazard_device.instance().io().id_rs2_addr, 0);
    
    sim.tick();
    sim.tick();
    
    // x0 不应该匹配前推，因此 rs1_src 应该是 0 (来自 REG)
    auto rs1_src = sim.get_port_value(hazard_device.instance().io().rs1_src);
    REQUIRE(static_cast<uint64_t>(rs1_src) == 0);
}
