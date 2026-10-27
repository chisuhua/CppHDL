#include "catch_amalgamated.hpp"
#include "chlib/forwarding.h"
#include "codegen_dag.h"
#include "core/context.h"
#include "core/literal.h"
#include "device.h"
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
// 测试 1b: ForwardingMux **组件级** —— 档位 1 的「跳转返回地址」
//
// ⚠️ 这是 ADR-047 D9 的「定向接线级单测」的**落点**（2026-10-27）：
//    档位 `1` 的源选择（ALU 结果 vs 跳转返回地址）**在 mux 组件内部**完成
//    ⇒ 可由**组件级**测试判别。
//    ⚠️ 若它留在流水线装配里**内联**（`exmem_is_jump` 是 pipeline 的**成员
//    `ch_reg`** ⇒ **测试台无法驱动**）⇒ 只能靠「实施者自己重建一份等价
//    逻辑」来测 ⇒ 测的是**副本** ⇒ 对**真实接线零信号** ⇒ **假绿**。
//    ⇒ 本用例是那条变异（「档位 1 忘接 `link_value`」）的**唯一**判别力来源。
//    ⇒ 该失效模式在**固件级不可达**（`JAL`/`JALR` 是**无条件**跳 ⇒ 它的
//      距离-1 消费者必在错误路径上 ⇒ 永远到不了真实消费者）。
// ============================================================================
TEST_CASE("ForwardingMux: level-1 source selects jump return address",
          "[forwarding][mux][component][jump]") {
    ch::ch_device<chlib::ForwardingMux<32>> mux;
    ch::Simulator sim(mux.context());

    // 档位 1（EX/MEM 级）
    sim.set_input_value(mux.instance().io().forward_a, 1);
    sim.set_input_value(mux.instance().io().forward_b, 1);
    sim.set_input_value(mux.instance().io().rs1_data_reg, 0x11111111u);
    sim.set_input_value(mux.instance().io().rs2_data_reg, 0x22222222u);
    sim.set_input_value(mux.instance().io().ex_mem_alu_result, 0xAAAAAAAAu);
    sim.set_input_value(mux.instance().io().mem_wb_result, 0xBBBBBBBBu);
    // 跳转的返回地址（= `pc + 4`）—— 与 ALU 结果**刻意不同** ⇒ 可区分
    sim.set_input_value(mux.instance().io().jump_result, 0x80000104u);

    // ---- ① 是跳转 ⇒ 档位 1 的数据 MUST 是**返回地址**（不是 ALU 结果）----
    sim.set_input_value(mux.instance().io().is_jump, 1);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(mux.instance().io().alu_input_a)) == 0x80000104u);
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(mux.instance().io().alu_input_b)) == 0x80000104u);

    // ---- ② 不是跳转 ⇒ 档位 1 的数据 MUST 是 **ALU 结果** ----
    sim.set_input_value(mux.instance().io().is_jump, 0);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(mux.instance().io().alu_input_a)) == 0xAAAAAAAAu);
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(mux.instance().io().alu_input_b)) == 0xAAAAAAAAu);

    // ---- ③ 档位 0（寄存器堆）MUST **不受** `is_jump` 影响 ----
    sim.set_input_value(mux.instance().io().is_jump, 1);
    sim.set_input_value(mux.instance().io().forward_a, 0);
    sim.set_input_value(mux.instance().io().forward_b, 0);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(mux.instance().io().alu_input_a)) == 0x11111111u);

    // ---- ④ 档位 2（MEM/WB）MUST **不受** `is_jump` 影响 ----
    sim.set_input_value(mux.instance().io().forward_a, 2);
    sim.set_input_value(mux.instance().io().forward_b, 2);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(mux.instance().io().alu_input_a)) == 0xBBBBBBBBu);
}

// ============================================================================
// 测试 1c: **负向对照** —— 组合环 MUST 仍然被拓扑排序抓到
//
// ⚠️ ADR-047 D9 的「负向对照」：事故形态（`git show bf78367`，2026-04-15）
//    是「前推的数据源取自 **EX 级组合输出**」⇒ mux 的输出又喂回 EX
//    ⇒ **纯组合环** ⇒ 拓扑排序 MUST 报环。
//    ⚠️ **为什么必须有这条**：若环检测本身失效（`type_reg` 不再是断路器 /
//    `type_mem_read_port` 被豁免）⇒「新形态不成环」这个结论**没有意义**
//    （它可能只是**环检测坏了**）⇒ ⇒ 负向对照 MUST 是**常驻**的
//    （一次性的人工检查 ⇒ 护栏失效无人守）。
//    ⇒ 本用例把「环检测仍有效」变成**常驻回归**。
// ============================================================================
class ForwardingCycleFixture : public ch::Component {
public:
    __io(ch_out<ch_uint<32>> y;)

    ForwardingCycleFixture(ch::Component *parent = nullptr,
                           const std::string &name = "cycle_fixture")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<chlib::ForwardingMux<32>> mux{"forwarding_mux"};
        mux.io().forward_a <<= ch_uint<2>(1_d);
        mux.io().forward_b <<= ch_uint<2>(0_d);
        mux.io().rs1_data_reg <<= ch_uint<32>(0_d);
        mux.io().rs2_data_reg <<= ch_uint<32>(0_d);
        mux.io().is_jump <<= ch_bool(false);
        mux.io().jump_result <<= ch_uint<32>(0_d);
        mux.io().mem_wb_result <<= ch_uint<32>(0_d);
        // ⚠️ **事故形态**：档位 `1` 的数据源 ← mux **自己的输出**
        //    ⇒ 纯组合环（无寄存器节点可断环）⇒ 拓扑排序 MUST 报环。
        mux.io().ex_mem_alu_result <<= mux.io().alu_input_a;
        io().y <<= mux.io().alu_input_a;
    }
};

TEST_CASE("ForwardingMux: negative control - combinational cycle MUST be detected",
          "[forwarding][negative][cycle]") {
    REQUIRE_THROWS_AS(
        [] {
            ch::ch_device<ForwardingCycleFixture> dev;
            ch::Simulator sim(dev.context());
            sim.tick();
        }(),
        std::runtime_error);
}

// ============================================================================
// 测试 1d: 函数式接口 `forward_select<>` 的 **is_jump** 分支
//
// ⚠️ 2026-10-27（ADR-047 D4 修订，第 1 轮 critic SHOULD_FIX #4）：
//    `forward_select<>` 与 `ForwardingMux::describe()` 是**同一份逻辑的两处实现**
//    ⇒ 头注释明文要求「MUST 同步修改」⇒ **只改组件侧**会让两个 API **行为分叉**
//    ⇒ 函数式接口的 `is_jump` 分支 MUST 有**独立的覆盖**
//    （此前 6 处调用**全走默认参数** `is_jump=false` ⇒ 该分支**零调用点覆盖**）。
// ============================================================================
TEST_CASE("forward_select: jump return address (functional interface)",
          "[forwarding][mux][functional][jump]") {
    auto ctx = std::make_unique<context>("test_mux_jump");
    ctx_swap ctx_swapper(ctx.get());

    ch_uint<2> forward_a = ch_uint<2>(1_d);
    ch_uint<2> forward_b = ch_uint<2>(1_d);
    ch_uint<32> rs1_data_reg = ch_uint<32>(0_d);
    ch_uint<32> rs2_data_reg = ch_uint<32>(0_d);
    ch_uint<32> ex_result = ch_uint<32>(0_d);
    ch_uint<32> mem_result = ch_uint<32>(0_d);
    ch_uint<32> jump_result = ch_uint<32>(0_d);

    // ⚠️ `is_jump` 是**按值**传入的（函数式接口）⇒ 无法在调用后改
    //    ⇒ MUST 调**两次**（两个结果结构）来覆盖两个分支。
    auto result_alu = forward_select<32>(forward_a, forward_b, rs1_data_reg,
                                          rs2_data_reg, ex_result, mem_result,
                                          ch_bool(false), jump_result);
    auto result_jump = forward_select<32>(forward_a, forward_b, rs1_data_reg,
                                           rs2_data_reg, ex_result, mem_result,
                                           ch_bool(true), jump_result);

    ch::Simulator sim(ctx.get());
    sim.tick();

    sim.set_value(rs1_data_reg, 0x11111111);
    sim.set_value(rs2_data_reg, 0x22222222);
    sim.set_value(ex_result, 0xAAAAAAAA);
    sim.set_value(mem_result, 0xBBBBBBBB);
    sim.set_value(jump_result, 0x80000104);
    sim.set_value(forward_a, 1);
    sim.set_value(forward_b, 1);
    sim.tick();

    // ---- ① 不是跳转 ⇒ 档位 1 的数据 MUST 是 **ALU 结果** ----
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_alu.alu_input_a)) == 0xAAAAAAAAu);
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_alu.alu_input_b)) == 0xAAAAAAAAu);

    // ---- ② 是跳转 ⇒ 档位 1 的数据 MUST 是**返回地址**（不是 ALU 结果）----
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_jump.alu_input_a)) == 0x80000104u);
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_jump.alu_input_b)) == 0x80000104u);
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
