/**
 * @file test_pipeline.cpp
 * @brief CppHDL 流水线寄存器模板测试
 * 
 * 测试 chlib/pipeline.h 中定义的通用流水线寄存器组件
 * 
 * 测试覆盖:
 * - PipelineStage: 单数据类型流水线寄存器组件
 * - PipelineStageDual: 双数据类型流水线寄存器组件
 * - PipelineChain: 多级流水线链组件
 * - pipeline_reg: 单数据类型函数式接口
 * - pipeline_reg_dual: 双数据类型函数式接口
 */

#include "catch_amalgamated.hpp"
#include "chlib/pipeline.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <memory>
#include <string>

using namespace ch::core;
using namespace chlib;

// ============================================================================
// 测试用例：PipelineStage 单数据类型组件
// ============================================================================

TEST_CASE("PipelineStage: Component Compile Test", "[pipeline][stage][compile]") {
    SECTION("PipelineStage instantiates with various types") {
        // 验证模板可以实例化不同类型
        PipelineStage<ch_uint<8>> stage8(nullptr, "stage8");
        PipelineStage<ch_uint<16>> stage16(nullptr, "stage16");
        PipelineStage<ch_uint<32>> stage32(nullptr, "stage32");
        PipelineStage<ch_uint<64>> stage64(nullptr, "stage64");
        PipelineStage<ch_bool> stage_bool(nullptr, "stage_bool");
        
        (void)stage8;
        (void)stage16;
        (void)stage32;
        (void)stage64;
        (void)stage_bool;
        
        REQUIRE(true);
    }
}

// ============================================================================
// 测试用例：PipelineStageDual 双数据类型组件
// ============================================================================

TEST_CASE("PipelineStageDual: Component Compile Test", "[pipeline][dual][compile]") {
    SECTION("PipelineStageDual instantiates with various combinations") {
        PipelineStageDual<ch_uint<32>, ch_uint<8>> dual1(nullptr, "dual1");
        PipelineStageDual<ch_uint<64>, ch_uint<16>> dual2(nullptr, "dual2");
        PipelineStageDual<ch_uint<16>, ch_uint<32>> dual3(nullptr, "dual3");
        
        (void)dual1;
        (void)dual2;
        (void)dual3;
        
        REQUIRE(true);
    }
}

// ============================================================================
// 测试用例：PipelineChain 多级流水线组件
// ============================================================================

TEST_CASE("PipelineChain: Component Compile Test", "[pipeline][chain][compile]") {
    SECTION("PipelineChain instantiates with various stage counts") {
        PipelineChain<ch_uint<32>, 1> chain1(nullptr, "chain1");
        PipelineChain<ch_uint<32>, 2> chain2(nullptr, "chain2");
        PipelineChain<ch_uint<32>, 4> chain4(nullptr, "chain4");
        PipelineChain<ch_uint<32>, 8> chain8(nullptr, "chain8");
        
        (void)chain1;
        (void)chain2;
        (void)chain4;
        (void)chain8;
        
        REQUIRE(true);
    }
}

// ============================================================================
// 测试用例：pipeline_reg 函数式接口
// ============================================================================

TEST_CASE("pipeline_reg: Functional Interface", "[pipeline][functional]") {
    auto ctx = std::make_unique<ch::core::context>("test_pipeline_func");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Basic pipeline_reg functionality") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<16> data_in = make_uint<16>(4660U); // 0x1234
        
        ch_uint<16> data_out = pipeline_reg<16>(data_in, rst, stall, flush, "test_func_reg");
        
        ch::Simulator sim(ctx.get());
        
        sim.tick();
        sim.tick();
        
        REQUIRE(sim.get_value(data_out) == 4660);
    }
    
    SECTION("pipeline_reg with reset active") {
        // 测试复位激活时寄存器清零
        ch_bool rst = true;  // 复位激活
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<8> data_in = make_uint<8>(100U);
        
        ch_uint<8> data_out = pipeline_reg<8>(data_in, rst, stall, flush, "test_reset_active");
        
        ch::Simulator sim(ctx.get());
        
        // 复位状态下输出应为 0
        sim.tick();
        REQUIRE(sim.get_value(data_out) == 0);
        
        sim.tick();
        REQUIRE(sim.get_value(data_out) == 0);
    }
    
    SECTION("pipeline_reg reset deassertion") {
        // 测试复位释放后数据可以锁存
        ch_bool rst = false;  // 复位释放
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<8> data_in = make_uint<8>(100U);
        
        ch_uint<8> data_out = pipeline_reg<8>(data_in, rst, stall, flush, "test_reset_deassert");
        
        ch::Simulator sim(ctx.get());
        
        // 初始输出为 0 (tick 前)
        REQUIRE(sim.get_value(data_out) == 0);
        
        // 第 1 个 tick：数据锁存到输出
        sim.tick();
        REQUIRE(sim.get_value(data_out) == 100);
        
        // 第 2 个 tick：输出保持
        sim.tick();
        REQUIRE(sim.get_value(data_out) == 100);
    }
    
    SECTION("pipeline_reg with stall") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<16> data_in = make_uint<16>(50U);
        
        ch_uint<16> data_out = pipeline_reg<16>(data_in, rst, stall, flush, "test_stall");
        
        ch::Simulator sim(ctx.get());
        
        // 先写入数据
        sim.tick();
        sim.tick();
        REQUIRE(sim.get_value(data_out) == 50);
        
        // 激活停顿，改变输入
        sim.set_value(stall, 1_b);
        sim.set_value(data_in, 75U);
        sim.tick();
        
        // 输出应保持原值
        REQUIRE(sim.get_value(data_out) == 50);
        
        // 解除停顿
        sim.set_value(stall, 0_b);
        sim.tick();
        
        // 输出应更新为新值
        REQUIRE(sim.get_value(data_out) == 75);
    }
    
    SECTION("pipeline_reg with flush") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<16> data_in = make_uint<16>(200U);
        
        ch_uint<16> data_out = pipeline_reg<16>(data_in, rst, stall, flush, "test_flush");
        
        ch::Simulator sim(ctx.get());
        
        // 先写入数据
        sim.tick();
        sim.tick();
        REQUIRE(sim.get_value(data_out) == 200);
        
        // 激活冲刷
        sim.set_value(flush, 1_b);
        sim.tick();
        
        // 输出应被清零
        REQUIRE(sim.get_value(data_out) == 0);
    }
}

// ============================================================================
// 测试用例：pipeline_reg_dual 函数式接口
// ============================================================================

TEST_CASE("pipeline_reg_dual: Functional Interface", "[pipeline][dual][functional]") {
    auto ctx = std::make_unique<ch::core::context>("test_pipeline_dual_func");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Basic pipeline_reg_dual functionality") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<32> data_in = make_uint<32>(2882400000U); // 0xABCDEF00
        ch_uint<8> ctrl_in = make_uint<8>(66U); // 0x42
        
        auto result = pipeline_reg_dual<32, 8>(data_in, ctrl_in, rst, stall, flush, "test_dual_func");
        
        ch::Simulator sim(ctx.get());
        
        sim.tick();
        sim.tick();
        
        REQUIRE(sim.get_value(result.data_out) == 2882400000U);
        REQUIRE(sim.get_value(result.ctrl_out) == 66);
    }
    
    SECTION("pipeline_reg_dual with stall holds both fields") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<32> data_in = make_uint<32>(3735928559U); // 0xDEADBEEF
        ch_uint<8> ctrl_in = make_uint<8>(90U); // 0x5A
        
        auto result = pipeline_reg_dual<32, 8>(data_in, ctrl_in, rst, stall, flush, "test_dual_stall");
        
        ch::Simulator sim(ctx.get());
        
        // 先写入数据
        sim.tick();
        sim.tick();
        
        // 激活停顿，改变输入
        sim.set_value(stall, 1_b);
        sim.set_value(data_in, 3405691582U); // 0xCAFEBABE
        sim.set_value(ctrl_in, 165U); // 0xA5
        sim.tick();
        
        // 两者都应保持
        REQUIRE(sim.get_value(result.data_out) == 3735928559U);
        REQUIRE(sim.get_value(result.ctrl_out) == 90);
    }
    
    SECTION("pipeline_reg_dual with flush clears both fields") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<32> data_in = make_uint<32>(4027583230U); // 0xF00DCAFE
        ch_uint<8> ctrl_in = make_uint<8>(240U); // 0xF0
        
        auto result = pipeline_reg_dual<32, 8>(data_in, ctrl_in, rst, stall, flush, "test_dual_flush");
        
        ch::Simulator sim(ctx.get());
        
        // 先写入数据
        sim.tick();
        sim.tick();
        
        // 激活冲刷
        sim.set_value(flush, 1_b);
        sim.tick();
        
        // 两者都应清零
        REQUIRE(sim.get_value(result.data_out) == 0);
        REQUIRE(sim.get_value(result.ctrl_out) == 0);
    }
}

// ============================================================================
// 测试用例：PipelineChain 多级流水线功能
// ============================================================================

TEST_CASE("PipelineChain: Functional Test", "[pipeline][chain][functional]") {
    auto ctx = std::make_unique<ch::core::context>("test_pipeline_chain_func");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("2-stage pipeline propagation") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<16> data_in = make_uint<16>(1234U);
        
        // 使用 2 级流水线
        ch_uint<16> stage1_out = pipeline_reg<16>(data_in, rst, stall, flush, "chain_stage1");
        ch_uint<16> stage2_out = pipeline_reg<16>(stage1_out, rst, stall, flush, "chain_stage2");
        
        ch::Simulator sim(ctx.get());
        
        // 初始输出为 0 (tick 0)
        REQUIRE(sim.get_value(stage1_out) == 0);
        REQUIRE(sim.get_value(stage2_out) == 0);
        
        // 第 1 个 tick：数据进入第 1 级
        sim.tick();
        REQUIRE(sim.get_value(stage1_out) == 1234);
        REQUIRE(sim.get_value(stage2_out) == 0);
        
        // 第 2 个 tick：数据到达第 2 级输出
        sim.tick();
        REQUIRE(sim.get_value(stage1_out) == 1234);
        REQUIRE(sim.get_value(stage2_out) == 1234);
    }
    
    SECTION("4-stage pipeline latency") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<32> data_in = make_uint<32>(0x12345678U);
        
        // 手动构建 4 级流水线
        ch_uint<32> s1 = pipeline_reg<32>(data_in, rst, stall, flush, "s1");
        ch_uint<32> s2 = pipeline_reg<32>(s1, rst, stall, flush, "s2");
        ch_uint<32> s3 = pipeline_reg<32>(s2, rst, stall, flush, "s3");
        ch_uint<32> s4 = pipeline_reg<32>(s3, rst, stall, flush, "s4");
        
        ch::Simulator sim(ctx.get());
        
        // 初始全为 0
        sim.tick();
        REQUIRE(sim.get_value(s4) == 0);
        
        // 4 个周期后数据到达输出
        sim.tick(); // s1 = input
        sim.tick(); // s2 = s1
        sim.tick(); // s3 = s2
        sim.tick(); // s4 = s3
        
        REQUIRE(sim.get_value(s4) == 0x12345678U);
    }
    
    SECTION("Pipeline chain with stall") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        ch_uint<16> data_in = make_uint<16>(100U);
        
        ch_uint<16> s1 = pipeline_reg<16>(data_in, rst, stall, flush, "s1");
        ch_uint<16> s2 = pipeline_reg<16>(s1, rst, stall, flush, "s2");
        
        ch::Simulator sim(ctx.get());
        
        // 填充流水线
        sim.tick();
        sim.tick();
        sim.tick();
        
        REQUIRE(sim.get_value(s2) == 100);
        
        // 激活停顿
        sim.set_value(stall, 1_b);
        sim.set_value(data_in, 200U);
        sim.tick();
        
        // 输出应保持不变
        REQUIRE(sim.get_value(s2) == 100);
    }
}

// ============================================================================
// 使用示例：5 级 CPU 流水线寄存器链
// ============================================================================

TEST_CASE("Pipeline: Example - 5-Stage CPU Pipeline Registers", "[pipeline][example]") {
    auto ctx = std::make_unique<ch::core::context>("test_cpu_pipeline_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("IF/ID -> ID/EX -> EX/MEM -> MEM/WB chain simulation") {
        // 模拟 5 级 CPU 流水线的 4 个寄存器级
        // IF → [IF/ID] → ID → [ID/EX] → EX → [EX/MEM] → MEM → [MEM/WB] → WB
        
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        
        // 模拟指令通过流水线
        ch_uint<32> if_instr = make_uint<32>(12288U); // 0x00003000 (示例指令)
        
        // 4 级流水线寄存器
        ch_uint<32> if_id = pipeline_reg<32>(if_instr, rst, stall, flush, "if_id");
        ch_uint<32> id_ex = pipeline_reg<32>(if_id, rst, stall, flush, "id_ex");
        ch_uint<32> ex_mem = pipeline_reg<32>(id_ex, rst, stall, flush, "ex_mem");
        ch_uint<32> mem_wb = pipeline_reg<32>(ex_mem, rst, stall, flush, "mem_wb");
        
        ch::Simulator sim(ctx.get());
        
        // 初始状态
        sim.tick();
        REQUIRE(sim.get_value(mem_wb) == 0);
        
        // 4 个周期后指令到达 WB 阶段
        sim.tick(); // 进入 IF/ID
        sim.tick(); // 进入 ID/EX
        sim.tick(); // 进入 EX/MEM
        sim.tick(); // 进入 MEM/WB
        
        REQUIRE(sim.get_value(mem_wb) == 12288);
    }
    
    SECTION("Pipeline flush on branch mispredict") {
        ch_bool rst = false;
        ch_bool stall = false;
        ch_bool flush = false;
        
        ch_uint<32> if_instr = make_uint<32>(1000U);
        
        ch_uint<32> if_id = pipeline_reg<32>(if_instr, rst, stall, flush, "if_id");
        ch_uint<32> id_ex = pipeline_reg<32>(if_id, rst, stall, flush, "id_ex");
        ch_uint<32> ex_mem = pipeline_reg<32>(id_ex, rst, stall, flush, "ex_mem");
        
        ch::Simulator sim(ctx.get());
        
        // 填充流水线
        sim.tick();
        sim.tick();
        sim.tick();
        sim.tick();
        
        REQUIRE(sim.get_value(ex_mem) == 1000);
        
        // 模拟分支预测错误，冲刷流水线
        sim.set_value(flush, 1_b);
        sim.tick();
        
        // 冲刷后输出清零
        REQUIRE(sim.get_value(ex_mem) == 0);
    }
}

// ============================================================================
// 辅助控制逻辑测试
// ============================================================================

TEST_CASE("Pipeline: Control Logic Helpers", "[pipeline][helpers]") {
    auto ctx = std::make_unique<ch::core::context>("test_pipeline_helpers");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("pipeline_stall_ctrl logic") {
        // stall = !ready || stall_req
        ch_bool stall_req = false;
        ch_bool ready = true;
        
        ch_bool stall = pipeline_stall_ctrl(stall_req, ready);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // !true || false = false
        REQUIRE(sim.get_value(stall) == 0);
        
        // 当下游未就绪时
        sim.set_value(ready, 0_b);
        sim.tick();
        REQUIRE(sim.get_value(stall) == 1);
        
        // 当有停顿请求时
        sim.set_value(ready, 1_b);
        sim.set_value(stall_req, 1_b);
        sim.tick();
        REQUIRE(sim.get_value(stall) == 1);
    }
    
    SECTION("pipeline_flush_ctrl logic") {
        // flush = flush_req || exception
        ch_bool flush_req = false;
        ch_bool exception = false;
        
        ch_bool flush = pipeline_flush_ctrl(flush_req, exception);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(flush) == 0);
        
        // 当有冲刷请求时
        sim.set_value(flush_req, 1_b);
        sim.tick();
        REQUIRE(sim.get_value(flush) == 1);
        
        // 当有异常时
        sim.set_value(flush_req, 0_b);
        sim.set_value(exception, 1_b);
        sim.tick();
        REQUIRE(sim.get_value(flush) == 1);
    }
}
