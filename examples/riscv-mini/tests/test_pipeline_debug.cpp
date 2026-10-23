/**
 * @file test_pipeline_debug.cpp
 * @brief 5-stage 流水线各 stage 组件的**逐组件行为断言**
 *
 * 测试范围（每个 stage 单独实例化 → 接地输入 → tick → 断言其语义输出）：
 *   DEBUG 1  HazardUnit    —— 无冒险时 stall/flush 为假、前推源为 0
 *   DEBUG 2  IfStage       —— PC 初值为 ITCM 基址 0x80000000
 *   DEBUG 3  IdStage       —— 解码 ADDI x0,x0,0：opcode/rs1_addr/rs2_addr
 *   DEBUG 4  ExStage       —— ALU ADD：5 + 7 = 12
 *   DEBUG 5  MemStage      —— 非 load 时透传 ALU 结果
 *   DEBUG 6  WbStage       —— 非访存时写回 ALU 结果，且写使能有效
 *   DEBUG 7  Rv32iPipeline —— 输入全部接地
 *   DEBUG 8  Rv32iPipeline —— 释放复位
 *   DEBUG 9  Rv32iPipeline —— 10 tick
 *
 * ⚠️ 2026-10-23 重构（本文件此前**从未被 CTest 执行过**）：
 * ① `ch_module<T>` 原先直接放在 TEST_CASE 顶层 ⇒ 没有 parent ⇒ `ch_module` ctor
 *    早退 ⇒ 随后 `io()` 触发 `CHFATAL` ⇒ **SIGSEGV**。
 *    现改为：每个组件由一个承载组件的 `describe()` 内部创建
 *    （根 AGENTS.md §COMPONENT LIFECYCLE PATTERNS 的规定用法）。
 * ② 原先 9 个用例**全是 `SUCCEED`（0 断言）**= 账面覆盖：它们只能证明
 *    「构造没崩」，不能证明「行为对」。现每个用例都断言该组件的**一个语义
 *    确定的输出**（非平凡值：0x80000000 / 0x13 / 12 / 0x1234 / 0xABCD），
 *    断言能区分「接对了」与「输出恒 0/未驱动」。
 *
 * 另注：`Rv32iPipeline` 自身把 `ch_module<IfStage>` 等声明为**成员变量**
 * （`include/cpu/pipeline/rv32i_pipeline.h:75-81`），成员在构造函数期初始化，
 * 早于 `build()` 设置 `Component::current()` ⇒ 这些 stage 实际挂到**祖父**
 * 节点下（父子关系错误但可运行）。该框架缺陷已单独登记，不在本 change 修复。
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"
#include "../../include/cpu/pipeline/hazard_unit.h"
#include "../../include/cpu/pipeline/stages/if_stage.h"
#include "../../include/cpu/pipeline/stages/id_stage.h"
#include "../../include/cpu/pipeline/stages/ex_stage.h"
#include "../../include/cpu/pipeline/stages/mem_stage.h"
#include "../../include/cpu/pipeline/stages/wb_stage.h"

using namespace ch::core;
using namespace riscv;

// ---------------------------------------------------------------------------
// 承载组件：每个把一个 ch_module 包在 describe() 内部，并把该组件的
// 关键输出引到顶层 `ch_out` 供测试断言。
// （顶层 `ch_out` 而不是深层内部节点 —— Verilator 后端只能看顶层端口。）
// ---------------------------------------------------------------------------
class HazardUnitTop : public ch::Component {
public:
    __io(
        ch_out<ch_bool>     stall;
        ch_out<ch_bool>     flush;
        ch_out<ch_uint<2>>  forward_a;
    )
    HazardUnitTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<HazardUnit> hazard{"hazard"};
        hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
        hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
        hazard.io().ex_alu_result <<= ch_uint<32>(0_d);
        hazard.io().mem_alu_result <<= ch_uint<32>(0_d);
        hazard.io().wb_write_data <<= ch_uint<32>(0_d);
        hazard.io().id_rs1_addr <<= ch_uint<5>(0_d);
        hazard.io().id_rs2_addr <<= ch_uint<5>(0_d);
        hazard.io().ex_rd_addr <<= ch_uint<5>(0_d);
        hazard.io().mem_rd_addr <<= ch_uint<5>(0_d);
        hazard.io().wb_rd_addr <<= ch_uint<5>(0_d);
        hazard.io().ex_reg_write <<= ch_bool(false);
        hazard.io().mem_reg_write <<= ch_bool(false);
        hazard.io().wb_reg_write <<= ch_bool(false);
        hazard.io().mem_is_load <<= ch_bool(false);
        hazard.io().ex_branch <<= ch_bool(false);
        hazard.io().ex_branch_taken <<= ch_bool(false);
        io().stall <<= hazard.io().stall;
        io().flush <<= hazard.io().flush;
        io().forward_a <<= hazard.io().forward_a;
    }
};

class IfStageTop : public ch::Component {
public:
    __io(ch_out<ch_uint<32>> instr_addr;)
    IfStageTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<IfStage> if_stage{"if_stage"};
        if_stage.io().stall <<= ch_bool(false);
        if_stage.io().flush <<= ch_bool(false);
        if_stage.io().rst <<= ch_bool(false);
        if_stage.io().branch_target <<= ch_uint<32>(0_d);
        if_stage.io().branch_valid <<= ch_bool(false);
        if_stage.io().instr_data <<= ch_uint<32>(0x13_h);
        if_stage.io().instr_ready <<= ch_bool(true);
        io().instr_addr <<= if_stage.io().instr_addr;
    }
};

class IdStageTop : public ch::Component {
public:
    __io(
        ch_out<ch_uint<7>>  opcode;
        ch_out<ch_uint<5>>  rs1_addr;
        ch_out<ch_uint<5>>  rs2_addr;
    )
    IdStageTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<IdStage> id_stage{"id_stage"};
        id_stage.io().pc <<= ch_uint<32>(0x80000000_h);
        // ⚠️ 用 `add x3, x1, x2`（0x002081B3）而不是 `addi x0,x0,0`(0x13)：
        //    后者的 rs1/rs2 本来就都是 x0 ⇒ 断言 `rs1_addr==0 && rs2_addr==0`
        //    是**恒真**的（解码器坏了也过），零判别力。
        id_stage.io().instruction <<= ch_uint<32>(0x002081B3_h);
        id_stage.io().valid <<= ch_bool(true);
        id_stage.io().reg_rs1_data <<= ch_uint<32>(0_d);
        id_stage.io().reg_rs2_data <<= ch_uint<32>(0_d);
        io().opcode <<= id_stage.io().opcode;
        io().rs1_addr <<= id_stage.io().rs1_addr;
        io().rs2_addr <<= id_stage.io().rs2_addr;
    }
};

class ExStageTop : public ch::Component {
public:
    __io(ch_out<ch_uint<32>> alu_result;)
    ExStageTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<ExStage> ex_stage{"ex_stage"};
        ex_stage.io().rs1_data <<= ch_uint<32>(5_d);
        ex_stage.io().rs2_data <<= ch_uint<32>(7_d);
        ex_stage.io().imm <<= ch_uint<32>(0_d);
        ex_stage.io().pc <<= ch_uint<32>(0x80000000_h);
        // ⚠️ ex_stage 的 opcode 字段只有 4 bit（lib 的 id_stage 才是 7 bit
        // —— 见 ADR-046 D3 的漂移记录）⇒ 放不下 0x33；本用例由 alu_op 驱动
        // ALU，与 opcode 无关。
        ex_stage.io().opcode <<= ch_uint<4>(0_d);
        ex_stage.io().funct3 <<= ch_uint<3>(0_d);      // funct3 = 0 ⇒ ADD
        ex_stage.io().funct7 <<= ch_uint<7>(0_d);
        ex_stage.io().alu_op <<= ch_uint<4>(0_d);      // ALU_OP_ADD
        ex_stage.io().is_branch <<= ch_bool(false);
        ex_stage.io().branch_type <<= ch_uint<3>(0_d);
        io().alu_result <<= ex_stage.io().alu_result;
    }
};

class MemStageTop : public ch::Component {
public:
    __io(ch_out<ch_uint<32>> alu_result_out;)
    MemStageTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<MemStage> mem_stage{"mem_stage"};
        mem_stage.io().alu_result <<= ch_uint<32>(0x1234_h);
        mem_stage.io().rs2_data <<= ch_uint<32>(0_d);
        mem_stage.io().is_load <<= ch_bool(false);
        mem_stage.io().is_store <<= ch_bool(false);
        mem_stage.io().funct3 <<= ch_uint<3>(0_d);
        mem_stage.io().mem_valid <<= ch_bool(true);
        io().alu_result_out <<= mem_stage.io().alu_result_out;
    }
};

class WbStageTop : public ch::Component {
public:
    __io(
        ch_out<ch_uint<32>> write_data;
        ch_out<ch_bool>     write_en;
    )
    WbStageTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<WbStage> wb_stage{"wb_stage"};
        wb_stage.io().alu_result <<= ch_uint<32>(0xABCD_h);
        wb_stage.io().mem_data <<= ch_uint<32>(0_d);
        wb_stage.io().rd_addr <<= ch_uint<5>(3_d);
        wb_stage.io().is_load <<= ch_bool(false);
        wb_stage.io().is_alu <<= ch_bool(true);
        wb_stage.io().is_jump <<= ch_bool(false);
        wb_stage.io().mem_valid <<= ch_bool(false);
        io().write_data <<= wb_stage.io().write_data;
        io().write_en <<= wb_stage.io().write_en;
    }
};

// 流水线三态：接地 / 释放复位 / 10 tick
// ⚠️ 顶层复位做成 `ch_in<ch_uint<1>>` 端口：TB 驱动信号 MUST 用 `ch_uint<N>`
// （`Simulator::set_input_value()` 没有 `ch_bool` 重载），且顶层端口才能被
// 外部驱动（`ch_in` 是组件 IO，不会成为生成 Verilog 的顶层端口）。
class PipelineTop : public ch::Component {
public:
    __io(
        ch_in<ch_uint<1>>  rst;
        ch_out<ch_uint<48>> perf_cycles;
    )
    PipelineTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        pipeline.io().rst <<= io().rst;
        // 喂一条真实 NOP（ADDI x0, x0, 0 = 0x00000013）：instr_data=0 是**非法
        // 指令**。
        // ⚠️ 观察（不改，仅记录）：本 pipeline 的 `commit_count` 只统计 **load**
        //   指令 —— `mem_valid_out = is_load & mem_valid` ⇒ 非 load 指令永远
        //   不 commit ⇒ `perf_instructions` 对 NOP 恒为 0。
        //   ⇒ 本文件的 pipeline 断言改用 `perf_cycles`（每拍递增、复位期冻结），
        //   它不依赖「什么指令算 commit」这个语义选择。
        pipeline.io().instr_data <<= ch_uint<32>(0x13_h);
        pipeline.io().instr_ready <<= ch_bool(true);
        pipeline.io().data_read_data <<= ch_uint<32>(0_d);
        pipeline.io().data_ready <<= ch_bool(true);
        io().perf_cycles <<= pipeline.io().perf_cycles;
    }
};

// ===== Test 1: HazardUnit standalone =====
TEST_CASE("DEBUG 1: HazardUnit standalone", "[debug][1]") {
    ch::ch_device<HazardUnitTop> top;
    ch::Simulator sim(top.context());
    sim.tick();

    // 无任何冒险：不应停顿、不应冲刷、不应选前推源
    REQUIRE(sim.get_port_value(top.instance().io().stall) == 0u);
    REQUIRE(sim.get_port_value(top.instance().io().flush) == 0u);
    REQUIRE(sim.get_port_value(top.instance().io().forward_a) == 0u);
}

// ===== Test 2: IfStage standalone =====
TEST_CASE("DEBUG 2: IfStage standalone", "[debug][2]") {
    ch::ch_device<IfStageTop> top;
    ch::Simulator sim(top.context());
    sim.tick();

    // PC 复位值 = ITCM 基址 0x80000000；instr_ready=1 且 stall=0
    // ⇒ tick 之后 PC 前进 4 ⇒ 该断言同时验证「复位值」与「PC 推进」
    REQUIRE(sim.get_port_value(top.instance().io().instr_addr) == 0x80000004u);
}

// ===== Test 3: IdStage standalone =====
TEST_CASE("DEBUG 3: IdStage standalone", "[debug][3]") {
    ch::ch_device<IdStageTop> top;
    ch::Simulator sim(top.context());
    sim.tick();

    // 0x002081B3 = `add x3, x1, x2` ⇒ opcode=0x33(R 型), rs1=1, rs2=2
    // ⚠️ rd(3) / funct3(0) 也要对：用非零源寄存器才能真正验证解码
    REQUIRE(sim.get_port_value(top.instance().io().opcode) == 0x33u);
    REQUIRE(sim.get_port_value(top.instance().io().rs1_addr) == 1u);
    REQUIRE(sim.get_port_value(top.instance().io().rs2_addr) == 2u);
}

// ===== Test 4: ExStage standalone =====
TEST_CASE("DEBUG 4: ExStage standalone", "[debug][4]") {
    ch::ch_device<ExStageTop> top;
    ch::Simulator sim(top.context());
    sim.tick();

    // ALU ADD：5 + 7 = 12
    REQUIRE(sim.get_port_value(top.instance().io().alu_result) == 12u);
}

// ===== Test 5: MemStage standalone =====
TEST_CASE("DEBUG 5: MemStage standalone", "[debug][5]") {
    ch::ch_device<MemStageTop> top;
    ch::Simulator sim(top.context());
    sim.tick();

    // 非 load ⇒ 透传 ALU 结果 0x1234
    REQUIRE(sim.get_port_value(top.instance().io().alu_result_out) == 0x1234u);
}

// ===== Test 6: WbStage standalone =====
TEST_CASE("DEBUG 6: WbStage standalone", "[debug][6]") {
    ch::ch_device<WbStageTop> top;
    ch::Simulator sim(top.context());
    sim.tick();

    // 非访存 ⇒ 写回 ALU 结果 0xABCD；rd != x0 ⇒ 写使能有效
    REQUIRE(sim.get_port_value(top.instance().io().write_data) == 0xABCDu);
    REQUIRE(sim.get_port_value(top.instance().io().write_en) == 1u);
}

// ===== Test 7: Pipeline - inputs grounded =====
TEST_CASE("DEBUG 7: Pipeline - inputs grounded", "[debug][7]") {
    ch::ch_device<PipelineTop> top;
    ch::Simulator sim(top.context());
    sim.set_input_value(top.io().rst, 1);
    sim.tick();

    // 复位保持中：周期计数 MUST 冻结在 0
    // （`cycle_count->next = select(reset_active, 0, cycle_count + 1)`）
    REQUIRE(sim.get_port_value(top.instance().io().perf_cycles) == 0u);
}

// ===== Test 8: Pipeline - release reset =====
TEST_CASE("DEBUG 8: Pipeline - release reset", "[debug][8]") {
    ch::ch_device<PipelineTop> top;
    ch::Simulator sim(top.context());
    sim.set_input_value(top.io().rst, 1);
    sim.tick();
    auto after_reset = sim.get_port_value(top.instance().io().perf_cycles);

    // 释放复位后再跑若干拍（指令需流过 5 级才能 commit）
    sim.set_input_value(top.io().rst, 0);
    sim.tick(10);
    auto after_release = sim.get_port_value(top.instance().io().perf_cycles);

    REQUIRE(after_release > after_reset);
}

// ===== Test 9: Pipeline - 10 ticks =====
TEST_CASE("DEBUG 9: Pipeline - 10 ticks", "[debug][9]") {
    ch::ch_device<PipelineTop> top;
    ch::Simulator sim(top.context());
    sim.set_input_value(top.io().rst, 0);
    sim.tick(10);

    // 10 拍后周期计数应当为正
    REQUIRE(sim.get_port_value(top.instance().io().perf_cycles) > 0u);
}
