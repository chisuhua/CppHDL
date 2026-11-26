/**
 * @file rv32i_pipeline.h
 * @brief RISC-V RV32I 5 级流水线顶层集成
 * 
 * 流水线结构:
 * ┌─────┬─────┬─────┬─────┐
 * │ IF  │ ID  │ EX  │ MEM │ WB │
 * ├─────┼─────┼─────┼─────┼────┤
 * │PC+4 │Decode│ALU  │Mem  │Write│
 * │IF/ID│ID/EX│EX/MEM│MEM/WB│back│
 * └─────┴─────┴─────┴─────┴────┘
 * 
 * 功能:
 * - 5 级流水线：取指 (IF) → 译码 (ID) → 执行 (EX) → 访存 (MEM) → 写回 (WB)
 * - 数据前推：EX→EX, MEM→EX, WB→EX
 * - Load-Use 冒险检测与停顿
 * - 分支预测错误冲刷
 */

#pragma once

#include "ch.hpp"
#include "component.h"
// ⚠️ **include 路径 MUST 是 `include/` 相对的**（2026-11-03 修「头文件自包含」）
//    —— 下面 7 个头都在**上一级**目录（`include/cpu/pipeline/`）⇒ ⇒
//    写 `"stages/if_stage.h"` 只能靠「`-I include/cpu/pipeline`」才找得到
//    ⇒ ⇒ **单独 include 本头会编译失败**（违反根 AGENTS.md 的
//    「Headers self-contained」）⇒ ⇒ 改成 `include/` 相对路径。
#include "cpu/pipeline/stages/if_stage.h"
#include "cpu/pipeline/stages/id_stage.h"
#include "cpu/pipeline/stages/ex_stage.h"
#include "cpu/pipeline/stages/mem_stage.h"
// ⚠️ **2026-11-13 新增**（ADR-069 A0 · V5）：load 的车道选择 / 扩展语义
//    从 `MemStage` 迁出 ⇒ ⇒ 载体 = 本组件（M2 的下游）。
#include "cpu/pipeline/load_lane_selector.h"
#include "cpu/pipeline/stages/wb_stage.h"
#include "cpu/pipeline/hazard_unit.h"
#include "cpu/pipeline/rv32i_pipeline_regs.h"
// ⚠️ **CSR bank**（2026-11-25 · P6-2 · change `rv32i-csr-instructions-2026-11`）：
//    本项把它的**生产消费者**从 0 变成 1（此前只有组件级测试实例化它）。
//    ⚠️ `CsrBank` 的 `__io` **一字未动** ⇒ ⇒ 零公开 API 变更。
#include "cpu/riscv/rv32i_csr.h"
// ⚠️ **异常 / 陷阱单元**（2026-11-26 · P6-3 · change `rv32i-trap-entry-2026-11`）：
//    本项把它的**生产消费者**从 0 变成 1（此前只有组件级测试实例化它）。
//    ⚠️ `ExceptionUnit` 的 `__io` **一字未动** ⇒ ⇒ 零公开 API 变更。
//    ⇒ ⇒ 架构依据 = `docs/adr/ADR-048-rv32i-fetch-skid-buffer.md` **§8**（DA1–DA5）
#include "cpu/riscv/rv32i_exception.h"
// ⚠️ 数据 mux（ADR-046 D3 #14 / ADR-047 D2）：**既有**的 `chlib::ForwardingMux`
//    —— MUST NOT 新建组件，也 MUST NOT 把 mux 放回 `HazardUnit`
//    （放进 `HazardUnit` 并接进真实流水线装配会成**组合环**，
//      `git show bf78367` 是当年的消环事故记录）。
#include "chlib/forwarding.h"

namespace riscv {

using namespace ch::core;

class Rv32iPipeline : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      rst;
        // ⚠️ **2026-11-14 · R8 改判**：`clk` 输入**已删**
        //   ⇒ ⇒ 它**零消费者**（`include/cpu/pipeline/**` 内无任何 `io().clk` 读取）
        //   ⇒ ⇒ 流水线寄存器全部锁在**框架 `default_clock`**（产物级实测）
        //   ⇒ ⇒ ZERO-DEBT：零消费者端口不得留存 ⇒ ⇒ 且删掉它避免 SoC 侧留下**悬空输入**
        //   （悬空 ⇒ ⇒ 产物里 undriven wire ⇒ ⇒ verilator 严格模式下 UNDRIVEN **致命**）
        //   ⇒ ⇒ 公开面移除登记：`.rddf/checks/public-api-removals.tsv`

        ch_out<ch_uint<32>> instr_addr;
        ch_in<ch_uint<32>>  instr_data;
        ch_in<ch_bool>      instr_ready;

        ch_out<ch_uint<32>> data_addr;
        ch_out<ch_uint<32>> data_write_data;
        ch_out<ch_uint<4>>  data_strbe;
        ch_out<ch_bool>     data_write_en;
        // ⚠️ **访存请求**（2026-11-23 · S3）：`load | store` 且 MEM 级有效。
        //    ⇒ ⇒ **为什么必需**：接 D-Cache 后，「读」有**副作用**（miss ⇒ 发 AXI burst）
        //    ⇒ ⇒ 若像 TCM 那样「无条件读」（`valid` 恒 1），**每条指令**都会触发一次
        //    cache 查找（含 ALU/NOP）⇒ miss 风暴。⇒ ⇒ MUST 只在真正访存时请求。
        ch_out<ch_bool>     data_req;
        ch_in<ch_uint<32>>  data_read_data;
        ch_in<ch_bool>      data_ready;

        ch_out<ch_uint<48>> perf_cycles;
        ch_out<ch_uint<48>> perf_instructions;
        ch_out<ch_uint<32>> perf_branch_count;
        ch_out<ch_uint<32>> perf_branch_mispredict;
        ch_out<ch_uint<32>> perf_load_use_stalls;
        ch_out<ch_uint<32>> perf_control_stalls;

        // ---- 诊断输出（dbg_*）----
        // ⚠️ 这些**不是**功能接口，是为了让「指令流 / 有效链 / 分支判定」
        //    可被外部（含 verilator）观测而存在的诊断面。
        //    存在的理由：`perf_instructions` 等计数器**不足以**区分
        //    「没取到指令」/「取到但没解码」/「解码了但没退休」三种状态。
        //    诊断面让逐拍追踪能定位到具体哪一级断了。
        ch_out<ch_uint<32>> dbg_id_instr;      // ID 级正在译码的指令字
        ch_out<ch_uint<32>> dbg_id_pc;         // ID 级 PC
        ch_out<ch_bool>     dbg_id_valid;      // ID 级有效
        // ⚠️⚠️ **命名有误导**（2026-10-25，第 16 轮 critic SHOULD_FIX #1）：
        //    这三个信号接的是 `id_stage.io().is_*` = **ID/EX 寄存后**的值
        //    ⇒ 它们描述的是「**EX 级**那条指令」的类型，**不是**「ID 级正在
        //    译码的那条」。
        //    （同组的 `dbg_id_instr` / `dbg_id_pc` / `dbg_id_valid` 才是真 ID 级
        //    —— 它们接 `id_stage` 的**输入**端口。）
        //    ⇒ **不改名**（端口集合已被护栏 C6 快照契约化），但注释 MUST 如实。
        ch_out<ch_bool>     dbg_id_is_branch;  // ⚠️ **EX 级**那条指令：是不是分支
        ch_out<ch_bool>     dbg_id_is_load;    // ⚠️ **EX 级**那条指令：是不是 load
        ch_out<ch_bool>     dbg_id_is_store;   // ⚠️ **EX 级**那条指令：是不是 store
        ch_out<ch_bool>     dbg_branch_taken;  // EX 级：分支是否成立
        ch_out<ch_bool>     dbg_exmem_valid;   // EX/MEM 有效寄存器
        ch_out<ch_bool>     dbg_exmem_is_store;// EX/MEM 的 store 标记
        ch_out<ch_bool>     dbg_mem_valid_out; // MEM 级输出给 WB 的有效位
        // ⚠️ **G1/G2（2026-11-15 · B4）**：未知 opcode 的可观测面
        //    0 = 合法 · 1 = **已知未实现**（`OP_SYSTEM`）· 2 = **未知 opcode**
        ch_out<ch_uint<2>>  dbg_illegal_reason{"dbg_illegal_reason"};
        // ⚠️ 「**这一拍**检测到 load-use 冒险」的脉冲（2026-10-25，A1-a）：
        //    `perf_load_use_stalls` 计的是「**停顿拍数**」（保持型停顿 ⇒
        //    一个冒险持续 N 拍就 +N）；而「**冒险次数**」需要单独的单拍脉冲
        //    ⇒ capability `rv32i-commit-path` 要求两者**可区分**。
        ch_out<ch_bool>     dbg_load_use_detected;
        // 临时诊断（2026-10-25）：WB 写回通路
        ch_out<ch_bool>     dbg_wb_write_en;
        ch_out<ch_uint<32>> dbg_wb_write_data;
        ch_out<ch_uint<5>>  dbg_wb_rd_addr;
        ch_out<ch_uint<32>> dbg_exmem_alu_result;
        ch_out<ch_uint<32>> dbg_exmem_imm;
        ch_out<ch_uint<32>> dbg_exmem_rs2_data;
    )
    
    Rv32iPipeline(ch::Component* parent = nullptr, const std::string& name = "rv32i_pipeline")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    // Phase 5: describe() split into 3 helper methods (datapath, control_signals,
    // forwarding_paths). All pipeline stage instances and ch_reg<> state live as
    // member fields so the helpers can be plain void methods.

    // Stage instances + reg file + pipeline regs (all shared state)
    //
    // ⚠️⚠️ **实例级成层**（ADR-070 **D2** · 选项 **①** · 2026-11-15 落地）：
    //    stage 实例 MUST 用 `ch::ch_layer<T>` + **显式模块类型名**（这些组件**未**做
    //    类级声明 ⇒ ⇒ 走档③ 双参形态 ⇒ ⇒ 缺名 = 编译期错误）。
    //    · **只影响层次化产物**：`ch_layer` 只写 `is_layer` / `module_type_name` 两个
    //      标志 ⇒ ⇒ **扁平产物逐字节不变**（结构性恒真 ⇒ 只作跳线，不作承重证据）
    //    · ⚠️ **7 个模块类型名 = 产物级契约名**（ADR-070 §2.3 · 改名 = **产物级破坏性变更**）
    //    · ⚠️ ADR-070 点名的 **7 处**不含 `load_lane`（它由 B3 在 ADR 之后新增）
    //      ⇒ ⇒ 本层**按 ADR 的字面集合**实施 ⇒ `load_lane` 保持 `ch_module`（inline）
    //      ⇒ ⇒ 见 change 的 design.md「实施期事实订正」
    ch::ch_layer<IfStage>  if_stage{"if_stage", "IfStage"};
    ch::ch_layer<IdStage>  id_stage{"id_stage", "IdStage"};
    ch::ch_layer<ExStage>  ex_stage{"ex_stage", "ExStage"};
    ch::ch_layer<MemStage> mem_stage{"mem_stage", "MemStage"};
    // ⚠️ **load 车道选择器**（2026-11-13 · ADR-069 A0 · V5）⇒ ⇒ 它的
    //    输入 MUST 是 **M2 锁存的**地址 / funct3 / 原始字 ⇒ ⇒ **不是**
    //    `EX/MEM` 的 ⇒ ⇒ 位置 = **M2 的下游**。
    ch::ch_module<LoadLaneSelector> load_lane{"load_lane_selector"};
    ch::ch_layer<WbStage>  wb_stage{"wb_stage", "WbStage"};
    ch::ch_layer<HazardUnit> hazard{"hazard_unit", "HazardUnit"};
    // ⚠️ **CSR bank**（2026-11-25 · P6-2）：**inline**（`ch_module`）——
    //    ADR-070 D2 点名的 7 处成层集合**不含**它 ⇒ ⇒ 本层按字面集合实施
    //    （与 `load_lane` 同型）⇒ ⇒ 层次化产物的 module 数**不变**。
    ch::ch_module<CsrBank> csr_bank{"csr_bank"};
    // ⚠️ **异常 / 陷阱单元**（2026-11-26 · P6-3）：**inline**（`ch_module`）
    //    ⇒ ⇒ 与 `csr_bank` **共享同一实例**（父层连线 ⇒ ⇒ 零端口面变更）
    //    ⇒ ⇒ **不是** ADR-070 D2 点名的 7 处成层集合成员 ⇒ ⇒ 层次化 module 数**不变**
    ch::ch_module<ExceptionUnit> exception{"exception"};
    // ⚠️ **数据 mux**（ADR-047 D2）：既有组件，**零新增组件**。
    //    档位语义与当年被移出 `HazardUnit` 的 mux **逐档等价**
    //    （0→REG / 1→EX/MEM / 2→MEM/WB）。
    ch::ch_layer<chlib::ForwardingMux<32>> fwd_mux{"forwarding_mux", "ForwardingMux<32>"};

    // ⚠️ **档位 select 的打拍寄存器**（ADR-047 D5）：
    //    `hazard` 的比较点是「**EX 级** rd vs **ID 级** rs」（那是 load-use
    //    检测点）⇒ 拍 T 算出的档位描述的是「**T+1** 消费者进 EX 时该选哪一级」
    //    ⇒ 若 mux 在拍 T 就用它 ⇒ **错一级** ⇒ 后果**不是「不前推」**，
    //    而是把**错误数据**塞进当前 EX 指令 ⇒ **静默错值路径**。
    //    ⇒ MUST 打一拍 ⇒ 拍 T 的档位在 T+1 生效。
    ch_reg<ch_uint<2>> fwd_a_d1{0_d, "fwd_a_d1"};
    ch_reg<ch_uint<2>> fwd_b_d1{0_d, "fwd_b_d1"};

    ch_mem<ch_uint<32>, 32> reg_file{"reg_file"};

    ch_reg<ch_bool>     exmem_is_store{false, "exmem_is_store"};
    ch_reg<ch_uint<32>> exmem_alu_result{0_d, "exmem_alu_result"};
    // ⚠️ **返回地址通路**（2026-10-26 新增）：JAL/JALR 的写回值是 `pc + 4`，
    //    而 WB 级没有 `pc` 通路 ⇒ MUST 在 **EX 级**算好并逐级锁存过来。
    ch_reg<ch_uint<32>> exmem_link_value{0_d, "exmem_link_value"};
    ch_reg<ch_uint<32>> exmem_rs2_data{0_d, "exmem_rs2_data"};
    ch_reg<ch_uint<5>>  exmem_rd_addr{0_d, "exmem_rd_addr"};
    ch_reg<ch_uint<3>>  exmem_funct3{0_d, "exmem_funct3"};
    ch_reg<ch_bool>     exmem_is_load{false, "exmem_is_load"};
    ch_reg<ch_uint<32>> exmem_imm{0_d, "exmem_imm"};
    ch_reg<ch_bool>     exmem_valid{false, "exmem_valid"};
    // ⚠️ EX/MEM 的「指令类型」寄存器（2026-10-25 补）：
    //    MEM/WB 寄存器需要知道「WB 级那条指令是 ALU / jump / load」，
    //    而 MEM 级那条指令的类型只能从 EX/MEM 寄存器取 ——
    //    直接用 `id_stage.io().is_alu`（**EX 级**的视图）会**串一级**：
    //    MEM/WB 会在 T+1 装上「T 拍在 EX 级那条指令」的类型。
    ch_reg<ch_bool>     exmem_is_alu{false, "exmem_is_alu"};
    ch_reg<ch_bool>     exmem_is_jump{false, "exmem_is_jump"};

    // ======================================================================
    // ⚠️ **M2 寄存器组**（2026-11-13 · ADR-069 决策 **A0**）
    // ======================================================================
    // MEM 区裂为 **EX/MEM → M2 → MEM/WB**（EX→WB 级数 2 → 3）⇒ ⇒ load 的
    // 数据晚 1 拍可用 ⇒ ⇒ **数据入端 MUST 在 MEM/WB′ 的装载沿前稳定**
    // ⇒ ⇒ 那「1 拍」由 **MEM/WB′** 承担（它锁存**车道选择后的值**）。
    //
    // ⚠️ **M2 = 8 个元数据成员**（`valid` / `is_alu` / `is_jump` / `is_load` /
    //   `rd_addr` / `link_value` / `alu_result` / `funct3`）—— ADR-069 §3.3 A0-①
    //   的**实施形态**（**经 §7.6 订正**）：
    //   因为**车道选择**（字节道 / 半字道 / 符号扩展）MUST 用**同一拍**的
    //   **地址**与 **funct3** ⇒ ⇒ 而原始字只在下一拍可用 ⇒ ⇒ 地址与 funct3
    //   MUST 先锁存 ⇒ ⇒ ⇒ 元数据 MUST 在 M2 里。
    //
    // ⚠️⚠️ **原始字 NOT 是 M2 的成员**（ADR-069 §7.6 · **订正 §7.2**）：
    //   §7.2 的「原始字 MUST 寄存 1 拍 ⇒ ⇒ 那个寄存器就是 M2 的 `m2_word`」
    //   **不可实施**（`sread` 输出寄存器在**同一条沿**上被更新 ⇒ ⇒ 在 M2 的
    //   装载沿采它 ⇒ ⇒ 采到**上一拍**的字 ⇒ ⇒ 与元数据**错配一拍**
    //   ⇒ ⇒ **静默错车道**）⇒ ⇒ 原始字**组合**取自 `sread` 的输出寄存器
    //   ⇒ ⇒ 「9 个成员」**作废** ⇒ ⇒ **详见 §7.6.3**（唯一落点）。
    //
    // ⚠️ **装载使能恒定**（ADR-069 **V4**）：M2 **不随停顿冻结** ⇒ ⇒ 否则死锁。
    //   ⇒ ⇒ 下面所有 `m2_*.next = ...` 都是**无条件**赋值（**零 select**）。
    //
    // ⚠️ **`m2_link_value` MUST 同拍 transit M2**（A0-①）：`wb_stage` 的跳转写回
    //   支取 `link_value` ⇒ ⇒ 若 MEM/WB 在 E_{T+2} 装载时取 `exmem_link_value`
    //   ⇒ ⇒ 那是**后继指令**的返回地址（垃圾）⇒ ⇒ JAL/JALR 写回垃圾。
    ch_reg<ch_bool>     m2_valid{false, "m2_valid"};
    ch_reg<ch_bool>     m2_is_alu{false, "m2_is_alu"};
    ch_reg<ch_bool>     m2_is_jump{false, "m2_is_jump"};
    ch_reg<ch_bool>     m2_is_load{false, "m2_is_load"};
    ch_reg<ch_uint<5>>  m2_rd_addr{0_d, "m2_rd_addr"};
    ch_reg<ch_uint<32>> m2_link_value{0_d, "m2_link_value"};
    // ⚠️ `m2_alu_result` **兼**访存地址（load/store 时 = 地址）⇒ ⇒ 车道选择
    //    用它的低 2/1 位。
    ch_reg<ch_uint<32>> m2_alu_result{0_d, "m2_alu_result"};
    ch_reg<ch_uint<3>>  m2_funct3{0_d, "m2_funct3"};
    // ⚠️⚠️ **原始字（raw word）NOT 寄存进 M2** —— 它**组合**取自
    //    `io().data_read_data`（= D-TCM 的 `sread` **输出寄存器**）。
    //
    //    ⚠️ **为什么不能是 M2 的第 9 个成员**（2026-11-13 实测 · 实施期发现）：
    //      `sread` 的输出寄存器在**同一条沿**上被更新 ⇒ ⇒ 在 M2 的装载沿
    //      （posedge N+2）采到的仍是**上一拍**的值 ⇒ ⇒ 采到的是
    //      **data(A_{i-1})**（**前一条指令**的地址）⇒ ⇒ 与 M2 的元数据
    //      （指令 i）**错配一拍** ⇒ ⇒ `LB`/`LH` **静默错车道**、
    //      `LW` **静默读回别人的字** ⇒ ⇒ **零报错**。
    //      ⇒ ⇒ 拍级事实：`sread` 在**地址呈现拍**发出 ⇒ 数据在**下一拍**可用
    //      ⇒ ⇒ M2 的元数据（EX/MEM 在拍 N+1 持有 i ⇒ M2 在拍 N+2 持有 i）
    //      与**原始字**（拍 N+2 = data(A_i)）**同拍**可用 ⇒ ⇒
    //      ⇒ ⇒ 车道选择 MUST 是**组合**的（输入 = M2 元数据 + sread 输出寄存器）
    //      ⇒ ⇒ 「原始字 MUST 寄存 1 拍」这条义务由 **MEM/WB′** 承担
    //      （MEM/WB′ 在 posedge N+3 锁存**车道选择后的值** ⇒ 那才是「寄存 1 拍」）
    //      ⇒ ⇒ 与 ADR-069 **V1**（地址仍取自 EX/MEM，**未**加地址寄存器）相容
    //      ⇒ ⇒ ⚠️ 若日后有人「补上」`m2_word` 寄存器 ⇒ ⇒ **必错**（错车道）
    //      ⇒ ⇒ 该缺陷已登记进 ADR-069 §7.6（dated addendum · 实施形态订正）。

    ch_reg<ch_bool>     memwb_is_alu{false, "memwb_is_alu"};
    // ⚠️ MEM/WB 补齐（2026-10-25）：WB 需要的**写回数据**与 **is_load**
    //    必须与 WB 级那条指令同拍 ⇒ 必须过 MEM/WB 寄存器。
    //    此前 WB 直接组合直连 MEM（`wb_stage.io().alu_result <<= mem_stage...`）
    //    ⇒ 写回的是**上一条**指令的 ALU 结果 / 内存数据。
    ch_reg<ch_bool>     memwb_is_load{false, "memwb_is_load"};
    ch_reg<ch_uint<32>> memwb_alu_result{0_d, "memwb_alu_result"};
    ch_reg<ch_uint<32>> memwb_link_value{0_d, "memwb_link_value"};
    ch_reg<ch_uint<32>> memwb_mem_data{0_d, "memwb_mem_data"};
    ch_reg<ch_bool>     memwb_is_jump{false, "memwb_is_jump"};
    ch_reg<ch_uint<5>>  memwb_rd_addr{0_d, "memwb_rd_addr"};
    ch_reg<ch_bool>     memwb_valid{false, "memwb_valid"};

    // ======================================================================
    // ⚠️ **CSR 指令的流水线寄存器**（2026-11-25 · P6-2 · change
    //    `rv32i-csr-instructions-2026-11`）
    // ======================================================================
    // 为什么这些**不是** `IdExRegs` / `MemWbRegs` 的字段（design §2.2 的订正）：
    //   `rv32i_pipeline_regs.h` 的 4 个结构体**在流水线实现里零使用**（实测 grep）
    //   ⇒ ⇒ 真实载体 = **本文件顶层的 `ch_reg`** ⇒ ⇒ 顶层可自由读写
    //   ⇒ ⇒ **零新增端口**（C1/C2）⇒ ⇒ 产物端口表零变化。
    //
    // ⚠️ **读值的取法（关键 · 决定 RAW 停顿为什么必需）**：
    //   `CsrBank` 只有**一个**地址口（读与写共用）⇒ ⇒
    //     · **读**在 **ID 级**（`csr_bank.csr_addr` ← ID 指令的 CSR 地址）
    //       ⇒ 读值锁存进 `csr_idex_read_value`（`csr_old`）
    //     · **写**在 **EX 级**（`csr_bank.csr_write_en/csr_wdata` ← EX 级那条指令）
    //       ⇒ 地址口在「EX 级有 CSR」时**切到 EX 的地址**（否则会写到 ID 那条的地址）
    //   ⇒ ⇒ 若相邻两条 CSR 都访问同一 CSR：读者在 ID 的那一拍，写者的写**尚未落在沿上**
    //       ⇒ ⇒ 直接锁存会读到**旧值** ⇒ ⇒ ⇒ **必须停 1 拍**（`csr_stall`）。
    //   ⇒ ⇒ 停顿 1 拍后：写者已离开 EX（写落在沿上）⇒ 读者在 ID 重新读到**新值** ✔
    //
    // ⚠️ **ID/EX 级的使能 MUST 与 `id_stage` 的 `idex_*` 一致**（否则两者错拍）：
    //   数据字段 = `valid & !freeze`；控制位在 `flush | stall` 时清 0（气泡语义）。
    ch_reg<ch_bool>     csr_idex_valid{false, "csr_idex_valid"};
    ch_reg<ch_uint<12>> csr_idex_addr{0_d, "csr_idex_addr"};
    ch_reg<ch_uint<32>> csr_idex_read_value{0_d, "csr_idex_read_value"};
    // EX/MEM → M2 → MEM/WB：CSR 标记与**读值**逐级透传（A0 的 3 段 MEM 区）
    ch_reg<ch_bool>     exmem_is_csr{false, "exmem_is_csr"};
    ch_reg<ch_uint<32>> exmem_csr_read_value{0_d, "exmem_csr_read_value"};
    ch_reg<ch_bool>     m2_is_csr{false, "m2_is_csr"};
    ch_reg<ch_uint<32>> m2_csr_read_value{0_d, "m2_csr_read_value"};
    ch_reg<ch_bool>     memwb_is_csr{false, "memwb_is_csr"};
    ch_reg<ch_uint<32>> memwb_csr_read_value{0_d, "memwb_csr_read_value"};

    ch_reg<ch_uint<48>> cycle_count{0_d, "cycle_count"};
    ch_reg<ch_uint<48>> commit_count{0_d, "commit_count"};
    ch_reg<ch_uint<32>> branch_count{0_d, "branch_count"};
    ch_reg<ch_uint<32>> branch_mispredict{0_d, "branch_mispredict"};
    ch_reg<ch_uint<32>> load_use_stalls{0_d, "load_use_stalls"};
    ch_reg<ch_uint<32>> control_stalls{0_d, "control_stalls"};

    // Datapath: data flow wiring between the 5 pipeline stages + reg file + memory
    void build_datapath() {
        // ⚠️ **MEM 级反压源**（2026-11-23 · change `rv32i-mem-backpressure-2026-11` ·
        //    S2 · 补齐已签批 `specs/rv32i-memory-timing` 的
        //    「MEM 级的反压接口 MUST 有可观测的语义」）。
        //    ⇒ ⇒ `data_ready` **无条件下**驱动冻结（spec 原文是「上游拉低它就绪信号
        //    时」⇒ 不含「仅当 MEM 级有访存」这个前提）⇒ ⇒ 若加 `& mem_valid`
        //    就是**新语义**（超出 spec），且会让「任意拍拉低」不可测。
        //    ⚠️ `build_control_signals()` 里**另建一个同值表达式**（两处 helper 各自
        //    可见；值相同、无状态 ⇒ 不引入顺序依赖 —— 同 `illegal_active` 的做法）。
        auto mem_stall = !io().data_ready;

        // Reg file read ports
        //
        // ⚠️ MUST 用 **aread（0-cycle 组合读）**，不能用 sread（1-cycle）：
        //    ID 级的 `idex_rs1_data` / `idex_rs2_data` 寄存器**已经**提供了
        //    「读地址 → 读数据」的那 1 拍延迟（地址在 ID 拍组合给出，数据在
        //    ID/EX 寄存器里与指令同拍进 EX）。
        //    若这里再用 sread（再晚 1 拍）⇒ EX 拿到的操作数属于**上一条**指令
        //    （实测：store 的写数据恒为 0 —— 读到的是上一条 NOP 的 rs2=x0）。
        //    ⚠️ 与 imem 的约定**不冲突**：ADR-041 强制的是 **imem** 用 sread
        //    （取指地址要 1 拍），寄存器堆不是 imem。
        //    ⚠️ 读地址 MUST 用 **当前 ID 级**的地址（`rs1_addr_id` / `rs2_addr_id`），
        //    不得用寄存后的 `rs1_addr` / `rs2_addr`（那是**上一条**指令的地址）
        auto rs1_data_result = reg_file.aread(id_stage.io().rs1_addr_id,
                                               "reg_file_rs1");
        auto rs2_data_result = reg_file.aread(id_stage.io().rs2_addr_id,
                                               "reg_file_rs2");

        // WB writes back to reg file
        auto wb_write_en = wb_stage.io().write_en & (wb_stage.io().rd_addr_out != ch_uint<5>(0_d));
        reg_file.write(wb_stage.io().rd_addr_out, wb_stage.io().write_data, wb_write_en, "reg_file_write");

        // ==================================================================
        // ⚠️ **M2 的装载 + load 车道选择 + M2 级写回 mux**
        // （2026-11-13 · ADR-069 决策 A0 · §7.1）
        // ==================================================================
        //
        // ⚠️ **M2 的装载使能恒定**（ADR-069 **V4**）⇒ ⇒ **无条件**赋值
        //   （**零 select**）⇒ ⇒ 否则停顿期间 M2 冻结 ⇒ ⇒ 死锁。
        //   拍级：EX/MEM 在 posedge N+1 锁存 i ⇒ M2 在 posedge N+2 锁存 i
        //   ⇒ ⇒ M2 的元数据源 = `exmem_*`（**MEM 级的输入**）。
        //
        // ⚠️⚠️ **T3 反压是 V4 的显式例外**（2026-11-23 · S2）：
        //   ADR-069 `:134` 明文「不做 MEM 级反压（`data_ready` 真反压）⇒ 归
        //   `memory-timing-contract` T3 ⇒ 本 ADR **零** T3 内容」⇒ ⇒ V4 的
        //   「停顿」在 ADR 语境里 = **load-use 停顿**。本 change 只在
        //   `mem_stall`（外部反压）时冻结，**load-use 期间 `mem_stall=0`
        //   ⇒ 照常前进** ⇒ ⇒ V4 的**意图**（防 load-use 死锁）完整保留。
        //   ⇒ ⇒ 判据口径的限定见 change 的 `design.md` §4。
        m2_valid->next      = select(mem_stall, m2_valid,      exmem_valid);
        m2_is_alu->next     = select(mem_stall, m2_is_alu,     exmem_is_alu);
        m2_is_jump->next    = select(mem_stall, m2_is_jump,    exmem_is_jump);
        m2_is_load->next    = select(mem_stall, m2_is_load,    exmem_is_load);
        m2_rd_addr->next    = select(mem_stall, m2_rd_addr,    exmem_rd_addr);
        m2_link_value->next = select(mem_stall, m2_link_value, exmem_link_value);
        m2_alu_result->next = select(mem_stall, m2_alu_result, exmem_alu_result);
        m2_funct3->next     = select(mem_stall, m2_funct3,     exmem_funct3);
        // ⚠️ **CSR 标记与读值逐级透传**（2026-11-25 · P6-2）：
        //    MEM 区是 3 段（EX/MEM → M2 → MEM/WB）⇒ ⇒ 漏掉 M2 这一段
        //    ⇒ ⇒ WB 级拿到的是**后继**指令的值（错拍）。
        m2_is_csr->next         = select(mem_stall, m2_is_csr,         exmem_is_csr);
        m2_csr_read_value->next = select(mem_stall, m2_csr_read_value, exmem_csr_read_value);
        // ⚠️ **原始字 NOT 寄存**（理由见 `m2_funct3` 处的注释）：`sread` 的输出
        //   寄存器在同一条沿上更新 ⇒ ⇒ 在本沿采到的是**上一拍**的地址的数据
        //   ⇒ ⇒ 采进 M2 会与 M2 的元数据**错配一拍** ⇒ ⇒ 静默错车道。
        //   ⇒ ⇒ 原始字**组合**取自 `io().data_read_data`（见下方车道选择段）。

        // ---- load 的车道选择（**M2 的下游** · ADR-069 **V5**）----
        //
        // ⚠️ **为什么在 M2 的下游**：`sread` ⇒ 原始字在拍 N+2 才可用 ⇒ ⇒
        //   **MEM 级（拍 N+1）已无数据可选** ⇒ ⇒ 车道选择 MUST 搬到 T+1 ⇒ ⇒
        //   而 T+1 的 `EX/MEM` 已是**后继**指令 ⇒ ⇒ MUST 用 **M2 锁存的**
        //   **地址** / **funct3** ⇒ ⇒ ⇒ 这正是 V5 明写的那条。
        // ⚠️ **载体 = `riscv::LoadLaneSelector` 组件**（单一真相 + 组件级可测）
        //   ⇒ ⇒ 语义**逐字**迁自原 `MemStage`（4 字节道 / 2 半字道 +
        //   符号扩展 / 零扩展）⇒ ⇒ 组件级双后端覆盖**不缩面**。
        //
        // ⚠️⚠️ **原始字 = `io().data_read_data`（组合）· NOT `m2_word`**：
        //   `sread` 的输出**寄存器**在拍 N+1 的沿上被更新 ⇒ ⇒ 拍 N+2 的值
        //   = **data(A_i)** ⇒ ⇒ 与 M2 的元数据（拍 N+2 = 指令 i）**同拍**
        //   ⇒ ⇒ 车道选择在本拍**组合**完成 ⇒ ⇒ MEM/WB′ 在 posedge N+3 锁存它。
        //   ⚠️ **M2 档的数据叶 = M2 寄存器 + `sread` 输出寄存器**（两者都是
        //   **寄存器**）⇒ ⇒ 不成环 ⇒ ⇒ 满足 ADR-069 **V15**。
        load_lane.io().raw_word <<= io().data_read_data;
        load_lane.io().addr      <<= m2_alu_result;
        load_lane.io().funct3    <<= m2_funct3;
        auto m2_load_data = load_lane.io().data;

        // ---- M2 级的**合并写回值**（与 `wb_stage` 的 mux **同构**）----
        //
        // ⚠️ **优先级 = load > alu > jump**（**逐字对齐** `wb_stage.h` 的三段
        //   `select`：后者覆盖前者 ⇒ jump 最优先）。
        // ⚠️ **数据叶 = M2 寄存器**（+ `m2_word` 寄存器）⇒ ⇒ **不成环**
        //   （ADR-047「数据源的叶子 MUST 是流水线寄存器」）。
        // ⚠️ **CSR 段**（2026-11-25 · P6-2）：与 load/alu/jump **互斥** ⇒ ⇒
        //   放最前（最低优先级）⇒ ⇒ 与 `wb_stage` 的同构关系**保持逐条一致**。
        ch_uint<32> m2_write_data = ch_uint<32>(0_d);
        m2_write_data = select(m2_is_csr, m2_csr_read_value, m2_write_data);
        m2_write_data = select(m2_is_load, m2_load_data, m2_write_data);
        m2_write_data = select(m2_is_alu, m2_alu_result, m2_write_data);
        m2_write_data = select(m2_is_jump, m2_link_value, m2_write_data);

        // ---- 前推**档位 2** 的数据源（**改指 M2**）----
        // ⚠️ **2026-11-13 改指 M2**（ADR-069 A0-②）：基线是 MEM/WB 级
        //    （`wb_stage.io().write_data`）⇒ ⇒ A0 下 MEM/WB 在 posedge N+3 才
        //    装上 i ⇒ ⇒ **距离 2** 的消费者（在拍 N+2 进 EX）拿不到 ⇒ ⇒
        //    **必须**改指 M2（M2 在 posedge N+2 装上 i ⇒ ⇒ 拍 N+2 起可用
        //    ⇒ ⇒ 恰好赶上消费者的 EX 拍）⇒ ⇒ 依据 = ADR-069 §7.1.3。
        // ⚠️ **M2 档的写回 mux 与 `wb_stage` 的 mux 同构**（load > alu > jump）
        //    ⇒ ⇒ 档位 2 的值与 WB 级最终写回的值**一致**（同一份语义）。
        fwd_mux.io().mem_wb_result <<= m2_write_data;

        // ==================================================================
        // ⚠️⚠️ **D11：两级优先旁路**（2026-11-13 · ADR-069 §7.1.2 修订）
        // ==================================================================
        //
        // ⚠️ **为什么是「两级」而不是「源替换」**（ADR-069 §7.1.1 的拍级表）：
        //   A0 **没有消灭**同沿竞态 ⇒ ⇒ 它把它从「**距离 3**」**推到了**
        //   「**距离 4**」⇒ ⇒
        //     · **距离 3** 的消费者在 ID 读拍读时，生产者**尚未写回**
        //       ⇒ ⇒ **WB 档够不着** ⇒ ⇒ MUST 新增 **M2 档**；
        //     · **距离 4** 的消费者与生产者的写回沿**同沿** ⇒ ⇒ **仍需 WB 档**
        //       ⇒ ⇒ **WB 档 MUST 保留**（删掉它 ⇒ 距离 4 与「4 拍停顿后的
        //       load-use 消费者」**静默读旧值**）。
        // ⇒ ⇒ **WB 档是承重件，不是遗留件**（ADR-047 §5.5.2.1 dated note）。
        //
        // 缺陷背景：`idex_rs1_data` **锁存**了寄存器堆的**组合读**
        //      （`id_stage.h:255`）⇒ 「ID→EX 锁存沿」与「WB 写回沿」是
        //      **同一条 posedge** ⇒ 锁存到的是**写回前**的旧值。
        //      ⚠️ Verilog 侧**同构**（两个 `always_ff` 同沿 + 非阻塞 ⇒ 都读旧值）
        //      ⇒ **两后端一致地错** ⇒ 双后端 parity **抓不到** ⇒ 只能靠
        //      **判别性固件**抓。
        //
        // 不成环：两档的**数据叶**都是**流水线寄存器**
        //      （M2 寄存器 + `sread` 输出寄存器 / `memwb_*` 寄存器）
        //      ⇒ 叶子是**寄存器** ⇒ 环断开（`context.cpp` 的拓扑排序对
        //      `type_reg` **只遍历 `init_val`**）。
        // 与 D7 正交：**不需要** `HazardUnit` 的 WB 级端口（那 5 个已删）。
        // `x0` 无需特判：两档各自的判据都含 `rd != 0`
        //      ⇒ 命中时 `rs1_addr_id` 必然非 0。
        //
        // ---- M2 档（新增 · **优先**）----
        // ⚠️ **CSR 也是「写 rd」的指令**（2026-11-25 · P6-2）⇒ ⇒ MUST 含 `m2_is_csr`
        //    （否则距离 2 的消费者读不到 CSR 的写回值）。
        auto m2_write_en = m2_valid &
                           (m2_is_alu | m2_is_load | m2_is_jump | m2_is_csr) &
                           (m2_rd_addr != ch_uint<5>(0_d));
        auto m2_bypass_hit_a = m2_write_en &
                               (m2_rd_addr == id_stage.io().rs1_addr_id);
        auto m2_bypass_hit_b = m2_write_en &
                               (m2_rd_addr == id_stage.io().rs2_addr_id);
        // ---- WB 档（**逐字保留**）----
        auto wb_bypass_hit_a = wb_write_en &
                               (wb_stage.io().rd_addr_out == id_stage.io().rs1_addr_id);
        auto wb_bypass_hit_b = wb_write_en &
                               (wb_stage.io().rd_addr_out == id_stage.io().rs2_addr_id);
        // ---- 优先级 = **M2 档 > WB 档**（同拍命中同一 rd 时 M2 更年轻）----
        id_stage.io().reg_rs1_data <<= select(m2_bypass_hit_a,
                                              m2_write_data,
                                              select(wb_bypass_hit_a,
                                                     wb_stage.io().write_data,
                                                     rs1_data_result));
        id_stage.io().reg_rs2_data <<= select(m2_bypass_hit_b,
                                              m2_write_data,
                                              select(wb_bypass_hit_b,
                                                     wb_stage.io().write_data,
                                                     rs2_data_result));

        // IF stage connections
        if_stage.io().branch_target <<= ex_stage.io().branch_target;
        if_stage.io().branch_valid <<= ex_stage.io().branch_taken;
        io().instr_addr <<= if_stage.io().instr_addr;
        if_stage.io().instr_data <<= io().instr_data;
        if_stage.io().instr_ready <<= io().instr_ready;

        // IF -> ID
        id_stage.io().pc <<= if_stage.io().pc;
        id_stage.io().instruction <<= if_stage.io().instruction;
        id_stage.io().valid <<= if_stage.io().valid;
        // ⚠️ 控制/数据冒险的反馈必须进 ID 级：
        //    - `flush`（taken 分支）⇒ ID/EX 清空（错误路径不进 EX）
        //    - `stall`（load-use）⇒ ID/EX 冻结（消费者留在 ID）
        id_stage.io().flush <<= hazard.io().flush;
        // ⚠️⚠️ **G2 · 行为策略 = 冻结**（2026-11-15 · B4）：未知 opcode 进入 ID 级
        //    ⇒ ⇒ `illegal_active` 拉高 ⇒ ⇒ **IF 与 ID 同时 stall**（PC 不前进 /
        //    ID/EX 装**气泡**）⇒ ⇒ 该指令**永不进入 EX、永不退休**。
        //    ⇒ ⇒ **MUST NOT** 当作 NOP 继续跑（那正是本项要改掉的旧行为）。
        //    ⚠️ **无组合环**：`illegal_active` 的来源是 `id_stage.io().illegal_reason`
        //      = `io().valid & f(opcode)`（ID 级的**输入**）⇒ 而 `if_stage.io().valid`
        //      = `if_id_valid_reg`（**寄存** ⇒ `if_stage.h:329`）⇒ ⇒ 反馈环被打断。
        auto illegal_active = (id_stage.io().illegal_reason != ch_uint<2>(0_d));

        // ==================================================================
        // ⚠️ **CSR 指令的数据通路**（2026-11-25 · P6-2 · change
        //    `rv32i-csr-instructions-2026-11`）
        // ==================================================================
        // 要点（推导见 change 的 design.md §3 / §4 / §8）：
        //   · **读 = ID 级**（`CsrBank` 读口是组合）· **写 = EX 级**（沿级）
        //   · `CsrBank` 只有**一个地址口**（读/写共用）⇒ ⇒ 地址在「EX 级有 CSR」时
        //     切到 **EX 的地址**（否则写会落到 ID 那条指令的 CSR 地址上 ⇒ 静默写错）
        //   · **相邻两条 CSR ⇒ 停 1 拍**（`csr_stall`）：写者的写尚未落沿时，
        //     读者若锁存读值 ⇒ 读到**旧值**（违反 RISC-V 的 CSR 顺序要求）
        //   · `flush` 优先（与 `if_stage` 的 PC 更新门一致 · ADR-071）
        //   · ⚠️ **零新增 `HazardUnit` 端口**（根 `AGENTS.md:256` 红线）
        auto csr_stall = csr_in_id() & csr_idex_valid & !hazard.io().flush;
        id_stage.io().stall <<= hazard.io().stall | illegal_active | csr_stall;

        // ---- CsrBank 的地址 / 中断输入 ----
        // ⚠️ CSR 地址 = `instr[31:20]`（**12 位**）—— MUST NOT 取 `[11:0]`
        //    （那是 `rd` / `funct3` / `opcode`，恒非合法 CSR 地址 ⇒ 读 0 / 写被忽略）。
        auto id_csr_addr = bits<31, 20>(if_stage.io().instruction);
        // ⚠️ **地址口的驱动在下方（写口仲裁段）** —— **MUST NOT** 在这里再驱动一次
        //    （同一端口被 `<<=` 两次 ⇒ ⇒ `set_driver` 替换 driver 但 `add_src`
        //    **累积** ⇒ ⇒ IR 图残留**两条 src 边** ⇒ ⇒ 值不确定 ⇒ ⇒ ADR-047 D8）
        // ⚠️ `mtip` / `meip` **显式 tie 0**：本项阶段无中断源（P6-6 才接）
        //    ⇒ ⇒ 未驱动端口读到的是**未定义值** ⇒ ⇒ MUST 显式驱动。
        csr_bank.io().mtip <<= ch_bool(false);
        csr_bank.io().meip <<= ch_bool(false);

        // ---- EX 级的 `csr_wdata`（6 种 `funct3` 的逐条语义）----
        // ⚠️ `csr_old` MUST 是**同一个**锁存值（ID 级读到的旧值）：
        //    写路径若改用「写后新值」⇒ `CSRRS` 会把新值再 OR 一遍 ⇒ 错值。
        const auto& csr_old = csr_idex_read_value;
        // ⚠️ `funct3` 用 **EX 级视图**（`id_stage.io().funct3` = ID/EX 寄存后的值）
        auto ex_f3 = id_stage.io().funct3;
        auto csr_is_imm     = (ex_f3 == ch_uint<3>(5)) | (ex_f3 == ch_uint<3>(6))
                            | (ex_f3 == ch_uint<3>(7));
        auto csr_is_set     = (ex_f3 == ch_uint<3>(2)) | (ex_f3 == ch_uint<3>(6));
        auto csr_is_clear   = (ex_f3 == ch_uint<3>(3)) | (ex_f3 == ch_uint<3>(7));
        auto csr_always_write = (ex_f3 == ch_uint<3>(1)) | (ex_f3 == ch_uint<3>(5));
        // 源操作数：立即数型 = `zimm`（5 位零扩展 · **不是** rs1 的值）；
        //           寄存器型 = **前推后的** rs1（`ex_stage.io().rs1_data`）
        ch_uint<32> csr_zimm32 = zext<32>(id_stage.io().rs1_addr);
        // ⚠️ 寄存器型源 = **前推 mux 的输出端口**（`fwd_mux.io().alu_input_a`）
        //    ⇒ ⇒ MUST NOT 读 `ex_stage.io().rs1_data`（**子组件输入** ⇒ 跨模块边）。
        auto csr_src = select(csr_is_imm, csr_zimm32, fwd_mux.io().alu_input_a);
        ch_uint<32> csr_wdata = csr_src;                                    // CSRRW / CSRRWI
        csr_wdata = select(csr_is_set,   csr_old | csr_src, csr_wdata);     // CSRRS / CSRRSI
        // ⚠️ clear 的掩码 MUST 是「**按位取反后 AND**」（`~`），**MUST NOT** 用 XOR
        csr_wdata = select(csr_is_clear, csr_old & ~csr_src, csr_wdata);    // CSRRC / CSRRCI
        // 写使能：`CSRRW` / `CSRRWI` **总是**写；set / clear 型在源值为 0 时**不写**
        //（规范：`rs1=x0` / `uimm=0` ⇒ 不写；寄存器型「值为 0」同样不触发写副作用）
        // ⚠️ `rd = 0` **不影响**写（`CSRRW` 的 `rd=0` 只是**不读**旧值）。
        auto csr_instr_write_en =
            csr_idex_valid & id_stage.io().valid_out &
            (csr_always_write | (csr_src != ch_uint<32>(0_d)));

        // ==================================================================
        // ⚠️ **trap 入口接线**（2026-11-26 · P6-3 · ADR-048 §8）
        // ==================================================================
        // 架构依据 = ADR-048 §8 的 **DA1–DA5**；审稿报告 =
        // `.rddf/reviews/ADR-048-pc-redirect-second-path-review-2026-11.md`。
        //
        // ⚠️ **触发集 = `illegal_reason ∈ {2,3}`**（DA5）——**不是** `illegal_active`
        //    （`!= 0`）⇒ ⇒ `reason = 1`（`ECALL`/`EBREAK`/`MRET`/`WFI`）**维持冻结**
        //    ⇒ ⇒ `illegal_active`（`!= 0`）**继续只服务 stall**（ID/EX 装气泡）。
        // ⚠️ **`!flush` 是承重条款**：taken 分支的 fall-through 槽里的非法字若在
        //    flush 同拍触发 trap ⇒ ⇒ **用错误路径的 PC 写坏 `mepc`**（DA5 · V6）。
        auto trap_trigger =
            ((id_stage.io().illegal_reason == ch_uint<2>(2_d)) |
             (id_stage.io().illegal_reason == ch_uint<2>(3_d))) &
            if_stage.io().valid & !hazard.io().flush;
        exception.io().exception_valid <<= trap_trigger;
        // ⚠️ `current_pc` = **ID 级那条指令**的 PC（IF/ID 的 PC）⇒ ⇒ `mepc` 指向
        //    **该指令自身**（**不是** `id_stage.io().pc_out` —— 那是 EX 级视图 ⇒
        //    ⇒ 会指向**后一条**指令）⇒ ⇒ MUST 读 `if_stage` 的**输出端口**
        //    （C1：读子组件的*输入*端口 = 跨模块边 ⇒ 层次化发射失败）
        exception.io().current_pc <<= if_stage.io().pc;
        // ⚠️ 原因码 = **illegal instruction（2）**（RISC-V 规范）⇒ ⇒ **常量**
        //    （`ECALL` = 11 / `EBREAK` = 3 归 **P6-4** ⇒ ⇒ 本项**不**涉及）
        exception.io().exception_cause <<= ch_uint<32>(2_d);
        // ⚠️ `MRET` 通道留空但接通（**P6-5** 才拉高）
        exception.io().mret_instruction <<= ch_bool(false);
        // ---- ExceptionUnit 的 CSR 读输入 ← `csr_bank` 的**输出端口** ----
        // （`CsrBank` 已有这 5 个输出 ⇒ ⇒ **零端口变更**）
        exception.io().csr_mstatus <<= csr_bank.io().mstatus_value;
        exception.io().csr_mtvec <<= csr_bank.io().mtvec_value;
        exception.io().csr_mepc <<= csr_bank.io().mepc_value;
        exception.io().csr_mie <<= csr_bank.io().mie_value;
        exception.io().csr_mcause <<= csr_bank.io().mcause_value;

        // ---- `CsrBank` 单写口的**仲裁**（**`ExceptionUnit` 优先**）----
        // ⚠️ **冲突窗口 = trap 那一拍本身**：`ExceptionUnit` 的 `csr_write_en` 在 trap
        //    **同拍**就拉高（`start_trap` 是组合量）⇒ ⇒ 与 EX 级那条 CSR 写指令
        //    **同拍冲突** ⇒ ⇒ 仲裁是**承重**的。
        // ⚠️ 优先理由：trap 上下文是「**一次异常**」的**成对**写入 ⇒ ⇒ 丢它 ⇒ ⇒
        //    软件永远看不到原因 ⇒ ⇒ 比「一条 CSR 指令的写被吞」更严重。
        // ⚠️ 被抑制的那次写**MUST NOT** 补偿重试（单写口无队列）⇒ ⇒ 已知边界。
        auto exc_write_en = exception.io().csr_write_en;
        auto csr_instr_addr = select(csr_idex_valid, csr_idex_addr, id_csr_addr);
        csr_bank.io().csr_addr <<= select(exc_write_en, exception.io().csr_write_addr,
                                          csr_instr_addr);
        csr_bank.io().csr_wdata <<= select(exc_write_en, exception.io().csr_write_data,
                                           csr_wdata);
        csr_bank.io().csr_write_en <<= exc_write_en |
                                       (csr_instr_write_en & !exc_write_en);

        // ---- 取指级的**第二路**重定向（trap / mret 内部 mux ⇒ 父层做）----
        // ⚠️ `mret_valid` 恒 0（**P6-5**）⇒ ⇒ 本项恒取 trap 路 ⇒ 零行为变化。
        //    ⚠️ 读 `mret_pc` / `mret_valid` **同时**消费了这两个输出端口
        //    （未被消费的子组件 `ch_out` 会**冒泡** ⇒ ⇒ 顶层端口面变化）。
        auto trap_pc_selected = select(exception.io().mret_valid,
                                       exception.io().mret_pc,
                                       exception.io().trap_entry_pc);
        if_stage.io().trap_target <<= trap_pc_selected;
        // ⚠️ `trap_entry_valid` 与 `trap_flush` **同源**（ExceptionUnit 内部都是
        //    `do_trap_entry`）⇒ ⇒ 这里是**恒等**门（零行为变化）⇒ ⇒ 同时**消费**
        //    了 `trap_entry_valid`（否则它会冒泡）。
        if_stage.io().trap_flush <<= exception.io().trap_flush &
                                    exception.io().trap_entry_valid;

        // ---- ID/EX：CSR 判定 + 地址 + 读值（与 `id_stage` 的 idex 同拍）----
        // 使能与 `id_stage` 的 `idex_*` **逐条同构**：数据字段 = `valid & !freeze`，
        // 控制位在 `flush | stall` 时清 0（气泡语义）；`freeze` 优先级最高。
        // ⚠️ `mem_stall` / `hazard.io().flush` 都是**本层已建的表达式**（或子组件**输出**）
        //    ⇒ ⇒ MUST NOT 读 `id_stage.io().freeze` / `id_stage.io().flush`（**子组件输入**
        //    ⇒ 跨模块边 ⇒ 层次化发射失败）。
        auto csr_idex_en = if_stage.io().valid & !mem_stall;
        auto csr_idex_bubble =
            (hazard.io().flush | hazard.io().stall | illegal_active | csr_stall)
            & !mem_stall;
        csr_idex_valid->next = select(
            mem_stall, csr_idex_valid,
            select(csr_idex_bubble, ch_bool(false),
                   select(csr_idex_en, csr_in_id(), csr_idex_valid)));
        csr_idex_addr->next = select(csr_idex_en, id_csr_addr, csr_idex_addr);
        csr_idex_read_value->next =
            select(csr_idex_en, csr_bank.io().csr_rdata, csr_idex_read_value);

        // ID -> EX
        // ⚠️ `ex_stage.io().rs1_data` / `rs2_data` 的**驱动源**在
        //    `build_forwarding_paths()` 里（**前推 mux 的输出**）⇒
        //    此处**不再**直连 `id_stage`（否则同一端口被 `<<=` **两次** ⇒
        //    `set_driver` 替换 `driver_` 但 `add_src` **累积** ⇒ IR 图残留
        //    **两条 src 边** ⇒ ZERO-DEBT / 拓扑排序 hygiene 问题，
        //    ADR-047 D8）。
        ex_stage.io().imm <<= id_stage.io().imm;
        ex_stage.io().funct3 <<= id_stage.io().funct3;
        // ⚠️ **分支条件类型（funct3）MUST 接进 EX**（2026-10-25 实测修复）：
        //    `ex_stage.io().branch_type` 此前**从未被驱动** ⇒ 恒为 0 ⇒
        //    **所有分支都按 BEQ 判定**（实测：`bne`（x0 != x0 恒假）被当成
        //    BEQ ⇒ 被判为「成立」⇒ 程序跳进了错误路径）。
        ex_stage.io().branch_type <<= id_stage.io().funct3;
        ex_stage.io().is_branch <<= id_stage.io().is_branch;
        ex_stage.io().is_store <<= id_stage.io().is_store;
        // ⚠️ EX 级必须拿**它自己那条指令**的 pc（ID/EX 寄存后的），
        //    而不是 IF/ID 的 pc 输入（那是当前 ID 级那条指令的 pc）
        ex_stage.io().pc <<= id_stage.io().pc_out;
        ex_stage.io().funct7 <<= id_stage.io().funct7_out;

        // ⚠️ **ALU 操作码 MUST 由 funct3 / funct7 推导并接线**（2026-10-25 实测修复）：
        //    `ex_stage.io().alu_op` 此前**从未被驱动**（全仓无连接）⇒ 恒为 0
        //    ⇒ 所有 ALU 操作都算 ADD（实测：`sub x4,x3,x2` 得 16 = 13+3，
        //    而非 13-3 = 10）。
        //    映射（RV32I）：funct3 000=ADD/SUB · 001=SLL · 010=SLT · 011=SLTU ·
        //    100=XOR · 101=SRL/SRA · 110=OR · 111=AND；funct7[5] 区分 SUB / SRA。
        {
            auto alu_f3 = id_stage.io().funct3;
            // ⚠️⚠️ **`funct7[5]`（= `instr[30]`）只对 R 型 与 SRAI 有意义**
            //    （2026-10-26 实测修复）：
            //      对 **I 型的 `ADDI` / `ANDI` / …** 而言，`instr[30]` 是 **`imm[10]`**
            //      的一部分 ⇒ 直接用它判 SUB/SRA 会把 **`imm >= 0x400`** 或
            //      负数误判成 SUB：
            //        `addi x21, x0, -8`（`0xFF800A93`）⇒ `0 - (-8)` = **8**
            //        `addi x21, x0, 0x7FF`（`0x7FF00A93`）⇒ `0 - 0x7FF` = **0xFFFFF801**
            //      ⇒ 本固件原来的 imm 全部 < 0x400（`0x100` / `0x122`）⇒ 侥幸未暴露。
            //    ⇒ MUST 用「**R 型** 或 **`OP_OPIMM` 且 funct3=101（SRAI）**」门控。
            auto alu_is_r_type = (id_stage.io().opcode == ch_uint<7>(OP_OP));
            auto alu_is_srai = (id_stage.io().opcode == ch_uint<7>(OP_OPIMM)) &
                               (alu_f3 == ch_uint<3>(5_d));
            auto alu_is_sub_sra = bits<5, 5>(id_stage.io().funct7_out) &
                                  (alu_is_r_type | alu_is_srai);
            // ⚠️ **访存指令的 funct3 编码的是「访存宽度」而不是 ALU 操作**
            //    （S/L 型：000=byte 001=half 010=word）⇒ LOAD/STORE 的
            //    ALU 结果必须是 **ADD**（rs1 + imm = 访存地址）。
            //    （实测：按 funct3 直接映射 ⇒ `sw` 被当成 SLT ⇒ 地址算成
            //    「0 < 10」= 1。）
            auto alu_is_mem = (id_stage.io().opcode == ch_uint<7>(OP_LOAD)) |
                              (id_stage.io().opcode == ch_uint<7>(OP_STORE));
            ch_uint<4> alu_op_calc = ch_uint<4>(ALU_ADD);
            alu_op_calc = select(alu_f3 == ch_uint<3>(7_d),
                                 ch_uint<4>(ALU_AND), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(6_d),
                                 ch_uint<4>(ALU_OR), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(4_d),
                                 ch_uint<4>(ALU_XOR), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(3_d),
                                 ch_uint<4>(ALU_SLTU), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(2_d),
                                 ch_uint<4>(ALU_SLT), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(1_d),
                                 ch_uint<4>(ALU_SLL), alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(5_d),
                                 select(alu_is_sub_sra, ch_uint<4>(ALU_SRA),
                                        ch_uint<4>(ALU_SRL)),
                                 alu_op_calc);
            alu_op_calc = select(alu_f3 == ch_uint<3>(0_d),
                                 select(alu_is_sub_sra, ch_uint<4>(ALU_SUB),
                                        ch_uint<4>(ALU_ADD)),
                                 alu_op_calc);
            alu_op_calc = select(alu_is_mem, ch_uint<4>(ALU_ADD), alu_op_calc);
            ex_stage.io().alu_op <<= alu_op_calc;
        }
        ex_stage.io().opcode <<= id_stage.io().opcode;
        // ⚠️ **跳转标志 MUST 走 `idex_is_jump`（气泡清零）而非 opcode 比较**：
        //    `idex_opcode` 在气泡拍不清零 ⇒ 错误路径的 JAL/JALR 会让气泡拍
        //    继续拉高 `branch_taken`（而 `ex_branch=0` ⇒ flush=0 ⇒ 无人拦）
        //    ⇒ **静默劫持 PC**（实测复现，见 `ex_stage.h` 的端口注释）。
        ex_stage.io().is_jump <<= id_stage.io().is_jump;

        // EX -> EX/MEM pipeline regs
        //
        // ⚠️⚠️ **EX/MEM 的装载使能 MUST 与 ID/EX 的装载使能一致**
        //    （`id_stage` 内部：`idex_update_en = io().valid`（**不含** `!stall`），
        //      而气泡（stall / flush）由 `idex_valid` 的**清零分支**承担 ⇒
        //      ID/EX 在气泡拍装的是「valid=0 的气泡」。）。
        //    否则会出现「**ID/EX 被气泡冻结**（数据停在旧指令）而
        //    **EX/MEM 照常装载**（有效位前进）」⇒ **有效位与数据错开一拍**
        //    ⇒ 被冲刷/停顿的**陈旧指令**最终被写回寄存器堆
        //    （实测：被 taken 分支冲刷的 `addi x6,x0,0x7f` 写回了 x6=0x7f）。
        //    ⇒ 这里对每个 EX/MEM 寄存器都用 `select(exmem_en, 新值, 保持)`。
        // ⚠️⚠️ **EX/MEM 的装载使能 MUST NOT 因停顿而冻结**（2026-10-25 改）：
        //    停顿被触发的当拍，「处于 EX 级的那条指令」**正是**那条 load
        //    ⇒ 若 EX/MEM 此时冻结，**load 永远进不了 MEM 级** ⇒ 它的数据
        //    永远到不了 WB ⇒ **停顿永不解除 ⇒ 死锁**。
        //    ⇒ 停顿的效果是「ID/EX 装气泡」（消费者留在 ID 级），
        //       而 EX/MEM **照常装载 EX 级的结果**（load 正常前进）。
        //    ⚠️ `id_stage.io().valid_out` 在 stall/flush 的**边沿**被清零
        //       ⇒ 下一拍 `exmem_en` 自动为 0 ⇒ EX/MEM 装气泡 ✔
        // ⚠️⚠️ **MUST NOT 因 `flush` 而清零**（2026-10-25，第 15 轮 critic
        //    SHOULD_FIX #8）：flush 由 **EX 级**的 `branch_taken` 产生 ⇒
        //    flush 那一拍**正在 EX 级的那条指令就是分支本身** ⇒ 若此时
        //    EX/MEM 装气泡 ⇒ **taken 分支从不退休**（实测：退休数里没有
        //    `beq`）⇒ 与 capability R1「计数器 MUST 对 ALU / load / store /
        //    branch **一视同仁**」有张力。
        //    ⇒ 教科书语义：flush 只杀「**比分支年轻**」的指令 ⇒
        //       分支本身 MUST 正常退休。
        //    ⇒ 错误路径那条（在 ID 级）由 `id_stage` 的 flush 通路拦住
        //       （ID/EX 装气泡）⇒ 不进 EX ⇒ 不会被写回 ✔
        auto exmem_en = id_stage.io().valid_out & !mem_stall;
        // ⚠️ **气泡时 EX/MEM 的「控制信号」MUST 清零**（不能保持旧值）：
        //    `mem_stage` 的访存使能是 `do_mem_access = is_load | is_store`
        //    —— **它不看 `mem_valid`** ⇒ 若气泡拍残留了上一条 store 的
        //    `is_store`，MEM 级会**真的写一次内存**（地址是旧值）⇒
        //    **静默的内存误写**。
        //    ⇒ `is_store` / `is_load` 在气泡时清零；其余数据字段保持
        //       （valid=0 ⇒ 不会被写回，故无害）。
        exmem_is_store <<= select(mem_stall, exmem_is_store,
                                  select(exmem_en, ex_stage.io().is_store_alu,
                                         ch_bool(false)));
        exmem_alu_result <<= select(exmem_en, ex_stage.io().alu_result,
                                    exmem_alu_result);
        // 返回地址（JAL/JALR 的写回值）—— 与 ALU 结果**同拍**锁存
        exmem_link_value <<= select(exmem_en, ex_stage.io().link_value,
                                    exmem_link_value);
        exmem_rs2_data <<= select(exmem_en, ex_stage.io().rs2_data,
                                  exmem_rs2_data);
        exmem_rd_addr <<= select(exmem_en, id_stage.io().rd_addr,
                                 exmem_rd_addr);
        exmem_funct3 <<= select(exmem_en, id_stage.io().funct3, exmem_funct3);
        exmem_is_load <<= select(mem_stall, exmem_is_load,
                                 select(exmem_en, id_stage.io().is_load,
                                        ch_bool(false)));
        exmem_imm <<= select(exmem_en, id_stage.io().imm, exmem_imm);
        // ⚠️ 气泡时 EX/MEM 的 valid MUST **清零**（而不是保持）：
        //    数据保持 + valid 保持 ⇒ 同一条指令在 MEM 级**停留两拍**
        //    ⇒ MEM/WB 抓两次 ⇒ WB **写回两次**（实测：x1 连续两拍各写回一次）。
        //    ⇒ 气泡以「valid=0」的形式**逐级传播**（EX/MEM → MEM/WB → WB）。
        // ⚠️⚠️ **EX/MEM 的 valid MUST 取 `valid_out`（ID/EX 寄存后的）**
        //    （2026-10-25 补）：此前取的是 `id_stage.io().valid`（**IF/ID 的
        //    valid** = **ID 级**那条指令的有效性）⇒ 与 `exmem_en`（用
        //    `valid_out` = **EX 级**那条指令的有效性）**错位一级**
        //    ⇒ EX/MEM 的「有效位」与「数据」可能来自**不同的指令**
        //    ⇒ 同一条指令在 MEM 级**停留两拍** ⇒ MEM/WB 抓两次
        //    ⇒ WB **写回两次**（实测：x1 在连续两拍各写回一次 0x0a）。
        exmem_valid <<= select(mem_stall, exmem_valid,
                               select(exmem_en, id_stage.io().valid_out, ch_bool(false)));
        // EX 级的指令类型（`id_stage.io().*` 是 **ID/EX 寄存后**的值
        //  ⇒ 正是 EX 级看到的指令类型）⇒ 进 EX/MEM 后成为 MEM 级的类型
        exmem_is_alu <<= select(mem_stall, exmem_is_alu,
                                select(exmem_en, id_stage.io().is_alu, ch_bool(false)));
        exmem_is_jump <<= select(mem_stall, exmem_is_jump,
                                 select(exmem_en, id_stage.io().is_jump, ch_bool(false)));
        // ---- CSR：EX/MEM 级（标记 + **读值** · 2026-11-25 · P6-2）----
        // ⚠️ `is_csr` 是**控制位** ⇒ 气泡时清 0（同 `is_load` / `is_jump`）；
        //    `csr_read_value` 是**数据** ⇒ 气泡时保持（`valid=0` ⇒ 不会被写回）。
        exmem_is_csr <<= select(mem_stall, exmem_is_csr,
                                select(exmem_en, csr_idex_valid, ch_bool(false)));
        exmem_csr_read_value <<= select(exmem_en, csr_idex_read_value,
                                        exmem_csr_read_value);

        // EX/MEM -> MEM
        mem_stage.io().alu_result <<= exmem_alu_result;
        mem_stage.io().rs2_data <<= exmem_rs2_data;
        mem_stage.io().rd_addr <<= exmem_rd_addr;
        mem_stage.io().is_store <<= exmem_is_store;
        // ⚠️ `is_load` **保留驱动**（2026-11-13）：端口仍在（公开契约）⇒ ⇒
        //    按「新增输入端口 MUST 显式驱动」的同款纪律**继续显式驱动**
        //    （未驱动端口读到的是**未定义值**⇒ ⇒ 即便当前无消费者也不留隐患）。
        mem_stage.io().is_load <<= exmem_is_load;
        mem_stage.io().funct3 <<= exmem_funct3;
        mem_stage.io().mem_valid <<= exmem_valid;
        // ⚠️ **2026-11-13（ADR-069 A0）**：`mem_stage.io().mem_read_data` 的
        //    驱动**已删** ⇒ ⇒ 该端口**已从 `MemStage` 移除**（load 的车道选择
        //    / 扩展语义迁到 `m2_load_data`，见上方「M2 的装载 + load 车道选择」段）。
        //    ⚠️ **store 侧的 `alu_result` / `funct3` 接线**（字节使能）**零改动**
        //    ⇒ ⇒ store 的地址呈现拍**不变**（ADR-069 V5）。

        // MEM/WB pipeline regs
        //
        // ⚠️⚠️ **2026-11-13（ADR-069 A0）：装载源全部改指 M2**
        // （`EX/MEM → M2 → MEM/WB` ⇒ MEM/WB 在 posedge N+3 装载 ⇒ 拍 N+3 起
        //   持有 i ⇒ ⇒ WB 级在拍 N+3 写回 i ⇒ ⇒ **写回沿 = posedge N+3**）。
        //
        // ⚠️ **使能恒定**（ADR-069 **V4**）⇒ ⇒ **无条件**赋值（**零 select**）
        //   ⇒ ⇒ 停顿期间 MEM/WB **照常前进** ⇒ ⇒ 否则死锁。
        //   ⚠️ 与基线的差异：基线是 `select(exmem_valid, X, memwb_X)`（无效时
        //   **保持**旧值）⇒ ⇒ 现在无效时 M2 的元数据本身就是「无效」⇒ ⇒
        //   MEM/WB 装的是**零/无效**（而非陈旧数据）⇒ ⇒ **更安全**。
        // ⚠️ **T3 反压冻结**（2026-11-23 · S2）：`mem_stall` 时 MEM/WB **保持**
        //   ⇒ ⇒ 这是 V4「使能恒定」的**显式例外**（ADR-069 `:134` 把 T3 排除在
        //   本 ADR 范围外 ⇒ ⇒ V4 的「停顿」= load-use；load-use 期间
        //   `mem_stall=0` ⇒ 照常前进 ⇒ V4 意图保留）。口径见 `design.md` §4。
        memwb_is_alu->next     = select(mem_stall, memwb_is_alu,     m2_is_alu);
        memwb_is_jump->next    = select(mem_stall, memwb_is_jump,    m2_is_jump);
        memwb_is_load->next    = select(mem_stall, memwb_is_load,    m2_is_load);
        memwb_rd_addr->next    = select(mem_stall, memwb_rd_addr,    m2_rd_addr);
        memwb_alu_result->next = select(mem_stall, memwb_alu_result, m2_alu_result);
        // ⚠️ **返回地址同拍 transit M2**（A0-①）⇒ ⇒ 否则 JAL/JALR 写回
        //   **后继**指令的返回地址（垃圾）。
        memwb_link_value->next = select(mem_stall, memwb_link_value, m2_link_value);
        // ⚠️ **load 数据 = M2 下游的车道选择结果**（`m2_load_data`）⇒ ⇒
        //   **不是** `mem_stage.io().mem_data`（该端口已删 · 语义已迁出）。
        memwb_mem_data->next   = select(mem_stall, memwb_mem_data,
                                        select(m2_valid & m2_is_load, m2_load_data,
                                               ch_uint<32>(0_d)));
        // ⚠️ **CSR 标记与读值 transit MEM/WB**（2026-11-25 · P6-2）
        memwb_is_csr->next         = select(mem_stall, memwb_is_csr,         m2_is_csr);
        memwb_csr_read_value->next = select(mem_stall, memwb_csr_read_value, m2_csr_read_value);
        // MEM/WB 的 valid = M2 的 valid（气泡以 valid=0 逐级传播 ⇒
        // 数据保持、valid 清零 ⇒ WB 不会把陈旧指令再写回一次）
        memwb_valid->next      = select(mem_stall, memwb_valid, m2_valid);

        // MEM/WB -> WB stage（WB 只能看到 MEM/WB 寄存器 ⇒ 与 WB 级指令同拍）
        wb_stage.io().alu_result <<= memwb_alu_result;
        wb_stage.io().link_value <<= memwb_link_value;
        wb_stage.io().rd_addr <<= memwb_rd_addr;
        wb_stage.io().mem_data <<= memwb_mem_data;
        wb_stage.io().is_load <<= memwb_is_load;
        wb_stage.io().is_alu <<= memwb_is_alu;
        wb_stage.io().is_jump <<= memwb_is_jump;
        wb_stage.io().mem_valid <<= memwb_valid;
        // ⚠️ **CSR 写回段**（2026-11-25 · P6-2）：`csr_read_value` = 读到的旧值
        wb_stage.io().is_csr <<= memwb_is_csr;
        wb_stage.io().csr_read_value <<= memwb_csr_read_value;

        // Data memory interface
        io().data_addr <<= exmem_alu_result;
        // ⚠️ **SB/SH 的写数据 MUST「lane 对齐」**（2026-10-26 新增）：
        //    RISC-V 规定 store 的源数据**低位对齐**，由**总线**按 `addr[1:0]`
        //    选车道 ⇒ `sb` 到偏移 1 时，源字节 MUST 落在 **lane 1**
        //    （即 `rs2 << 8`）⇒ 否则「掩码对了、数据在错 lane」——
        //    那是一个比「字节写没生效」**更隐蔽**的错误。
        //
        //    ⚠️ **只对 SB/SH 位移**（funct3 = 000 / 001）：
        //    `sw` MUST 保持原样（**含非对齐 `sw` 的既有行为**：整字写到
        //    所在字地址）⇒ 这里的改动**不改变**任何既有 `sw` 行为。
        //    ⚠️ 移位量 MUST 用**运行期节点** ⇒ 用 `shl<N>`（显式结果宽度）；
        //    `operator<<` 会按 RHS 的**类型宽度**推算结果宽度 ⇒ 运行期移位量
        //    会把结果宽度算错（见 `include/axi4/byte_lane.h` 的同型警告）。
        auto store_byte_offset = bits<1, 0>(exmem_alu_result);
        // SB/SH = funct3 000/001；SW = 010 ⇒ `!= 010` 即「部分写」
        auto store_is_partial = exmem_is_store &
                                (exmem_funct3 != ch_uint<3>(2_d));
        auto lane_shift = select(!store_is_partial, ch_uint<5>(0_d),
                          select(store_byte_offset == ch_uint<2>(0_d), ch_uint<5>(0_d),
                          select(store_byte_offset == ch_uint<2>(1_d), ch_uint<5>(8_d),
                          select(store_byte_offset == ch_uint<2>(2_d), ch_uint<5>(16_d),
                                                 ch_uint<5>(24_d)))));
        io().data_write_data <<= shl<32>(exmem_rs2_data, lane_shift);
        // ⚠️ 字节使能 MUST 来自 **MEM 级**（2026-10-25，第 14 轮 critic
        //    SHOULD_FIX #5/#6）：此前**恒接 0xF** ⇒ SB/SH 的字节使能
        //    **从未生效**（mem_stage 里算好的 `store_byte_enable` 是死代码）。
        //    ⇒ 改接 MEM 级的输出（SW 仍是 0xF ⇒ 整字写行为不变）。
        io().data_strbe <<= mem_stage.io().store_byte_enable_out;
        // ⚠️ **store 写使能 = 「电平」且 MUST 保持电平**（2026-11-24 · change
        //    `rv32i-commit-event-pulse-2026-11` 的**订正**）：
        //    ① 它在**功能路径**上（核顶层 → D-L1 的 `we` → `aw_valid` → `aw_fire`
        //    → `ready` → `data_ready` → `mem_stall`）⇒ ⇒ **不可**用 `!mem_stall`
        //    门控（**组合环** ⇒ 仿真 `FATAL: Cycle detected`，2026-11-24 实测）；
        //    ② 「停顿期间电平持续」的**观测面**缺陷由**核顶层**在总线侧派生
        //    （`obs_data_write_en = data_write_en & data_ready`）修掉
        //    ⇒ ⇒ 本信号的**功能语义零变化**（D-L1 的写握手次数仍是 1）。
        io().data_write_en <<= exmem_is_store;
        // 访存请求 = MEM 级有效 且 是 load/store（⚠️ 不含 ALU/NOP ⇒ 避免 cache miss 风暴）
        io().data_req <<= exmem_valid & (exmem_is_load | exmem_is_store);
    }

    // Control signals: hazard unit + reset/stall/flush to all stages
    void build_control_signals() {
        // IF stage stall/flush from hazard
        // ⚠️ **G2 冻结的 IF 侧**：与 ID 级**同一判据**（`illegal_reason != 0`）
        //    ⇒ ⇒ PC 与 skid 一并冻结（否则 PC 会继续前进、把非法指令之后的
        //    指令取进来）
        //    ⚠️ 表达式在**两处各建一个比较节点**（`wire_id_ex()` 与这里）
        //      ⇒ 值**相同**、无状态 ⇒ 不引入顺序依赖（用局部变量则依赖调用顺序）。
        //
        // ⚠️ **MEM 反压也冻结 IF 侧**（2026-11-23 · S2）：`mem_stall` 来自
        //   `!io().data_ready`（与 `build_datapath()` 里同值 ⇒ 无状态、无环）。
        //   三者（load-use / 非法指令 / MEM 反压）都是「前级不许前进」⇒ 取 OR。
        auto mem_stall = !io().data_ready;
        // ⚠️ **CSR→CSR 的 1 拍停顿**（2026-11-25 · P6-2）：判据落**顶层**
        //    （**零新增 `HazardUnit` 端口** · 根 `AGENTS.md:256` 红线）。
        //    与 `build_datapath()` 内同值（两处各建一个节点 ⇒ 无状态 ⇒ 无顺序依赖）。
        auto csr_stall = csr_in_id() & csr_idex_valid & !hazard.io().flush;
        if_stage.io().stall <<= hazard.io().stall |
            (id_stage.io().illegal_reason != ch_uint<2>(0_d)) | mem_stall | csr_stall;
        // ⚠️ **ID/EX 用 `freeze`（保持）而不是 `stall`（装气泡）**：
        //   反压期间 EX 级那条指令 MUST NOT 丢失 ⇒ 见 `id_stage.h` 的端口注释。
        id_stage.io().freeze <<= mem_stall;
        if_stage.io().flush <<= hazard.io().flush;
        if_stage.io().rst <<= io().rst;

        // Hazard unit inputs
        // ⚠️ MUST 用**当前 ID 级**的地址（`rs1_addr_id` / `rs2_addr_id`），
        //    不得用寄存后的 `id_stage.io().rs1_addr`（那是 **EX 级**的视图）：
        //    load-use 检测要比较的是「**正在 ID 级**的消费者」的 rs1/rs2 与
        //    「MEM 级的 load」的 rd ⇒ 用 EX 级的地址会**晚一级**触发
        //    （此时消费者的操作数**已经锁存**）⇒ 停顿「停了个寂寞」
        //    （实测：`lw` 紧邻 `add` 时，停顿计数 +1 但读到的仍是旧值）。
        hazard.io().id_rs1_addr <<= id_stage.io().rs1_addr_id;
        hazard.io().id_rs2_addr <<= id_stage.io().rs2_addr_id;
        // ⚠️ **冒险单元的「MEM 级」输入 MUST 取自 MEM 级**（EX/MEM 寄存器），
        //    不得取自 `id_stage.io().*`（那是 EX 级的视图）：
        //    - `mem_rd_addr` 原接 `id_stage.rd_addr` ⇒ load-use 检测拿「EX 级
        //      指令的 rd」当「MEM 级 load 的 rd」⇒ 检测退化成「与自身比较」；
        //    - `mem_is_load` 原接 `id_stage.is_load` ⇒ 同上。
        //    （`ex_rd_addr` / `ex_reg_write` 接 `id_stage.*` 是**对的** ——
        //      `id_stage.io().*` 是 ID/EX 寄存后的值 = EX 级的视图。）
        hazard.io().ex_rd_addr <<= id_stage.io().rd_addr;
        hazard.io().mem_rd_addr <<= exmem_rd_addr;
        hazard.io().ex_reg_write <<= id_stage.io().is_alu | id_stage.io().is_load |
                                    id_stage.io().is_jump | csr_idex_valid;
        // ⚠️ **CSR 指令也写 rd**（2026-11-25 · P6-2）⇒ ⇒ 两级写使能 MUST 含它，
        //    否则距离 1 / 2 的消费者读不到 CSR 的写回值（前推档位不成立）。
        hazard.io().mem_reg_write <<= exmem_is_alu | exmem_is_load | exmem_is_jump |
                                      exmem_is_csr;
        // ⚠️ **load-use 检测 MUST 走 EX 级**（`ex_is_load`）：
        //    5 级流水线的标准检测点是「EX 级的 load vs ID 级的消费者」。
        //    `mem_is_load`（MEM 级那一路）**故意不连接**（保持 0）：
        //    它晚一级 —— MEM 级 load 到达时消费者早已进 EX、操作数已锁存
        //    ⇒ 停顿「停了个寂寞」；而且在流水线里它会**多停一拍**且是
        //    **不必要**的（消费者进 EX 时那条 load 已在 WB 写回）。
        //    `mem_is_load` 只给「独立使用 HazardUnit」的场景用。
        // ⚠️ **2026-11-11 · B0 批 C8（装配点 tie-off）** —— 此前此处**故意不连接**
        //    `mem_is_load` ⇒ ⇒ 该 `ch_in` **无 driver** ⇒ ⇒ 按**已签批契约**
        //    （`openspec/specs/rtl-hierarchical-output/spec.md:309-316`
        //    「顶层端口表 MUST 是 … **无 driver 的** `input` → …」）它**必须**出现在
        //    顶层端口面 ⇒ ⇒ 产物里是一个**活跃**的顶层 input（实测变异 tie 1 ⇒
        //    **9 项 golden 转红**、只退休 **21** 条指令（正常 **178**）
        //    ⇒ ⇒ ⇒ 在**4 态仿真器**里不接 ⇒ 读 **X** ⇒ `stall` 变 **X** ⇒ **整个核变 X**）。
        //    ⇒ ⇒ ⇒ **处置**：在**装配点**显式 tie-off ⇒ ⇒ 该 `ch_in` **有 driver**
        //    ⇒ ⇒ 按契约它**不再是顶层端口**（有 driver ⇒ 内部 wire）⇒ ⇒ 顶层 input **少 1 个**。
        //    ⚠️ **契约相容性**：契约说的是「**无 driver 的** input」⇒ ⇒ 给一个 input
        //    **补上 driver** = **修缺陷**，**不是**改契约 ⇒ ⇒ ⇒ 与契约**相容**。
        //    ⚠️ **行为零变化**：MEM 级那一路在流水线里**永不触发**
        //    （`hazard_unit.h:236-239`：「在流水线里**永不触发**」）⇒ ⇒ tie-off `false`
        //    与「未连接」的**可观测行为相同**（两者都读 0 ⇒ `mem_load_use` 恒假）。
        hazard.io().mem_is_load <<= ch_bool(false);
        hazard.io().ex_is_load <<= id_stage.io().is_load;
        // ⚠️ `ex_branch` = **条件分支 + 跳转**（2026-10-26 扩展）：
        //    `JAL`/`JALR` 是**无条件**跳 ⇒ 也必须 flush 错误路径
        //    （否则 PC 跳了、错误路径的指令却继续流下去 ⇒ 效果泄漏）。
        //    ⚠️ 两者**互斥**（一条指令不可能同时是 OP_BRANCH 与 OP_JAL/OP_JALR）
        //    ⇒ `if_stage` 的隐式不变量（flush 的唯一来源是
        //    `ex_branch & ex_branch_taken`，且 `branch_valid` 接同一个
        //    `ex_stage.branch_taken`）**结构不变**。
        hazard.io().ex_branch <<= id_stage.io().is_branch | id_stage.io().is_jump;
        hazard.io().ex_branch_taken <<= ex_stage.io().branch_taken;
        // ⚠️ **5 个「预留输入」端口的驱动已删**（ADR-047 D7）：
        //    `wb_reg_write` / `wb_rd_addr` / `ex_alu_result` /
        //    `mem_alu_result` / `wb_write_data` —— 它们在 `HazardUnit` 内
        //    **零消费者** ⇒ 留着只是**公共 API 面的死重量**；
        //    且 `ex_alu_result` 曾被接到 EX 级**组合输出** ⇒ 它是**环饵**
        //    ⇒ 删掉让 ADR **自执行**（否则后人「顺手消费它」就会
        //    **精确复现 bf78367** 的组合环事故）。
    }

    // Forwarding paths: data forwarding + hazard detection + perf counters
    void build_forwarding_paths() {
        // Raw register values from ID (forwarded to hazard unit)
        // ⚠️ **保留**这两处驱动（ADR-047 D7 修订）：接上 mux 后
        //    `hazard.rs1_data` 在流水线里**无消费者** ⇒ 按 ZERO-DEBT 看是
        //    「喂了没人读」；但**删除**它与本仓反模式
        //    「新增组件输入端口 ⇒ **全部**实例化点 MUST 显式驱动」
        //    （未驱动端口读到的是**未定义值** ⇒ 静默错值）**正面相撞**
        //    ⇒ **正确性优先于整洁** ⇒ 保留驱动（驱动来源
        //    `id_stage.io().rs1_data` 是**合法**的 ⇒ 零成本）。
        //    ⚠️ 例外 MUST **显式登记**（本注释）⇒ 不能靠沉默。
        hazard.io().rs1_data_raw <<= id_stage.io().rs1_data;
        hazard.io().rs2_data_raw <<= id_stage.io().rs2_data;

        // ---- 前推档位的**打拍**（ADR-047 D5）----
        // `hazard` 的组合逻辑**一字不改** ⇒ 既有真值表测试**因打拍而全部保留**。
        auto reset_active = io().rst;
        //
        // ⚠️⚠️⚠️ **2026-11-25 · 反压冻结补丁**（change `riscv-tests-gate-2026-11`）
        //
        // **问题**：`fwd_a_d1` / `fwd_b_d1` 是**派生状态**（前推**档位**），
        //   它服务的对象是 **ID→EX 边界上那条指令**。MEM 级反压（`mem_stall`）时：
        //     · EX 级被**冻结**（`id_stage.freeze` ⇒ `idex_*` 全字段保持）
        //     · ID 的**输入**已翻到**后继指令**（IF 侧也被 `mem_stall` 停住 ⇒ 停在后继）
        //   ⇒ 而 `hazard.io().forward_a/b` 的判据用的是 **ID 输入那条指令**的 rs 地址
        //      （`id_stage.rs1_addr_id` = **组合**译码 ⇒ `rv32i_pipeline.h:734-735`）
        //      ⇒ ⇒ 每拍刷新出来的档位是**后继指令的决策**
        //      ⇒ ⇒ 却被 `fwd_mux` 施加到 EX 级那条**被冻结的**指令上
        //      ⇒ ⇒ 它的操作数被**别的指令的旁路值**改写（实测：官方 riscv-tests 的
        //         `rv32ui-p-lb` / `lbu` / `lh` / `lhu` 四个因此失败）。
        //
        // **修法**：反压期间**冻结档位**（保持不变）⇒ 档位与它服务的指令**同冻结**
        //   （这正是 SpinalHDL / VexRiscv 的做法：派生状态随它服务的级一起冻结 ——
        //   `RiscvCore.scala:666` 的 `m2sPipe` + `Stream.scala:463-481` 的
        //   `RegNextWhen(payload, self.ready)`；VexRiscv `Pipeline.scala:124`）。
        //
        // ⚠️ **MUST NOT 在反压期间清零档位**（那是「看起来保守」的错法）：
        //   被冻结指令若**自身**在冻结入口档位就非 0（距离-2 生产者此刻在 M2），
        //   清零会把「错前推」换成「读 `idex_rs1_data` 的**陈旧**值」⇒ 同样是错
        //   （判据 = `examples/riscv-mini/tests/test_rv32i_stall_integrity.cpp` 的 A2 用例）。
        // ⚠️ **MUST NOT** 改成「用 ID 读拍的 M2/WB 旁路冻结」：那条通路服务的是
        //   **解冻沿锁存的后继指令**（`idex_update_en` 在反压期间为 0 ⇒ 其输出
        //   **无消费者**），冻结它反而会请回 ADR-069 §7.1.2（D11）处理的同沿竞态。
        // ⚠️ **解冻沿无缝隙**：`mem_stall` 解除的那一拍，寄存器装入「该拍按后继指令
        //   地址算出的档位」，而后继指令于**下一拍**进 EX 消费它 ⇒ d1 的对齐关系
        //   逐拍保持（被冻结指令在该拍**仍在 EX** ⇒ 持有档位施加给它**正确**）。
        auto mem_stall_fwd = !io().data_ready;   // 与 `build_datapath()` 内同值（无状态 ⇒ 组合等价）
        fwd_a_d1->next = select(reset_active, ch_uint<2>(0_d),
                                select(mem_stall_fwd, fwd_a_d1, hazard.io().forward_a));
        fwd_b_d1->next = select(reset_active, ch_uint<2>(0_d),
                                select(mem_stall_fwd, fwd_b_d1, hazard.io().forward_b));

        // ---- 前推 mux 的接线（ADR-047 D2 / D4）----
        // ⚠️ 三个数据源的**叶子** MUST 是**流水线寄存器** ⇒ 不成环
        //    （`context.cpp` 的拓扑排序对 `type_reg` 只遍历 `init_val`）。
        fwd_mux.io().forward_a <<= fwd_a_d1;
        fwd_mux.io().forward_b <<= fwd_b_d1;
        // 档位 0 = 寄存器堆（**ID/EX 锁存值** —— `idex_rs1_data`）
        fwd_mux.io().rs1_data_reg <<= id_stage.io().rs1_data;
        fwd_mux.io().rs2_data_reg <<= id_stage.io().rs2_data;
        // 档位 1 = EX/MEM 级（**EX/MEM 寄存器**）
        // ⚠️ **CSR 的读值**与 ALU 结果**共用档位 1**（2026-11-25 · P6-2）：
        //    CSR 与 jump / alu / load 互斥 ⇒ ⇒ 按 `exmem_is_csr` 选择即可
        //    （`ForwardingMux` 内部再按 `is_jump` 选返回地址 ⇒ 两层互斥）。
        fwd_mux.io().ex_mem_alu_result <<=
            select(exmem_is_csr, exmem_csr_read_value, exmem_alu_result);
        // ⚠️ 跳转的**返回地址**：`JAL`/`JALR` 的写回值是 `pc + 4`，不是 ALU 结果
        //    ⇒ MUST 含 `link_value`。三个源同受 `exmem_en` 守卫 ⇒ **对齐**。
        fwd_mux.io().is_jump <<= exmem_is_jump;
        fwd_mux.io().jump_result <<= exmem_link_value;
        // ⚠️ 档位 2 的数据源接线见 `build_datapath()`（⚠️ **必须同函数** ——
        //    `m2_write_data` 是该函数的**局部**组合节点 ⇒ ⇒ 跨函数不可见
        //    ⇒ ⇒ 驱动 `fwd_mux.io().mem_wb_result` 的地方也在那里）。

        // 前推 mux 的输出**独占**驱动 EX 级的操作数（单一驱动 ⇒ ADR-047 D8）
        ex_stage.io().rs1_data <<= fwd_mux.io().alu_input_a;
        ex_stage.io().rs2_data <<= fwd_mux.io().alu_input_b;

        // Performance counters
        cycle_count->next = select(reset_active, ch_uint<48>(0_d), cycle_count + 1_d);

        // ⚠️ **退休计数 MUST 被反压门控**（2026-11-23 · S2）：`commit_valid` 取
        //    `mem_stage.io().mem_valid_out`，而它在反压时**冻结为 1**（MEM 级
        //    那条指令停留）⇒ ⇒ 若不门控就**每拍 +1**（同一条指令重复计数）。
        //    语义：反压期间 MEM 级**不前进** ⇒ **不退休** ⇒ 计数不增。
        //    ⚠️ `mem_stall` 在本函数**另建一个同值节点**（同 `illegal_active` 的
        //       做法：两处各建一个 ⇒ 值相同、无状态 ⇒ 不引入顺序依赖）。
        auto mem_stall = !io().data_ready;
        auto commit_valid = mem_stage.io().mem_valid_out & !mem_stall;
        commit_count->next = select(reset_active, ch_uint<48>(0_d), commit_count + commit_valid);

        // ⚠️⚠️ **三个同族计数器 MUST 也被反压门控**（2026-11-23 · S2 · critic
        //    SHOULD_FIX #1 的实质缺陷）：反压让 EX 级指令**多拍驻留** ⇒
        //    `id_stage.io().is_branch`（ID/EX 寄存 ⇒ 冻结保持 1）·
        //    `ex_stage.branch_taken` · `hazard.io().flush` 全部稳定为 1
        //    ⇒ ⇒ 不门控就**每停一拍各 +1**（与 `commit_valid` 的重复计数**同族**）。
        //    ⇒ ⇒ 判据见 `test_rv32i_mem_backpressure` 的「同族计数器不得被污染」用例
        //    （变异：去掉任一 `& !mem_stall` ⇒ 该用例必红）。
        auto branch_decoded = id_stage.io().is_branch & !mem_stall;
        branch_count->next = select(reset_active, ch_uint<32>(0_d), branch_count + branch_decoded);

        // ⚠️ **控制重定向**（2026-10-26）：`ex_branch` 已扩为「条件分支 **+** 跳转」
        //    ⇒ `branch_taken & flush` 现在同时包含 `JAL`/`JALR` 的重定向。
        auto control_redirect =
            ex_stage.io().branch_taken & if_stage.io().valid & hazard.io().flush & !mem_stall;
        // ⚠️ **误预测**只统计**条件分支**（`JAL`/`JALR` 是**无条件**跳 ⇒ 不是
        //    「预测错误」）⇒ MUST 用 `id_stage.io().is_branch` 门控
        //    （它是 **EX 级**那条指令的类型，与 `ex_stage.branch_taken` 同拍）。
        //    ⚠️ 不加这个门控 ⇒ 每执行一条 `JAL`/`JALR` 都会被记一次「误预测」
        //    ⇒ 性能计数器语义被污染（实测会多出 2 次）。
        auto branch_mispred_detected = id_stage.io().is_branch & control_redirect;
        branch_mispredict->next = select(reset_active, ch_uint<32>(0_d), branch_mispredict + branch_mispred_detected);

        // ⚠️ **反压 ALSO 门控 `load_use_stalls`**（2026-11-23 · S2 · critic
        //    SHOULD_FIX #2）：反压期间若 load 卡在 EX，`hazard.io().stall` 会
        //    **持续为 1** ⇒ ⇒ 不门控就把「反压拍」记成「load-use 拍」⇒
        //    与 ADR-069 `:176`「T3 反压不得混进该计数器」在字面上冲突。
        //    ⚠️ 纯 load-use 场景 `mem_stall=0` ⇒ 行为不变（`== 4` 的手推断言不受影响）。
        load_use_stalls->next = select(reset_active, ch_uint<32>(0_d),
                                       load_use_stalls + (hazard.io().stall & !mem_stall));
        // ⚠️ `control_stalls` 统计**全部**控制重定向造成的停顿拍数
        //    （含 `JAL`/`JALR` —— 它们同样让流水线空 3 拍）⇒ 与「误预测」**分离**
        //    （此前两者同源，只因固件里只有条件分支）。
        control_stalls->next = select(reset_active, ch_uint<32>(0_d), control_stalls + control_redirect);

        io().perf_cycles <<= cycle_count;
        io().perf_instructions <<= commit_count;
        io().perf_branch_count <<= branch_count;
        io().perf_branch_mispredict <<= branch_mispredict;
        io().perf_load_use_stalls <<= load_use_stalls;
        io().perf_control_stalls <<= control_stalls;
    }

    // 诊断面：把「指令流 / 有效链 / 分支判定」逐级引出
    void build_debug_outputs() {
        io().dbg_id_instr <<= id_stage.io().instruction;
        io().dbg_id_pc <<= id_stage.io().pc;
        io().dbg_id_valid <<= id_stage.io().valid;
        io().dbg_id_is_branch <<= id_stage.io().is_branch;
        io().dbg_id_is_load <<= id_stage.io().is_load;
        io().dbg_id_is_store <<= id_stage.io().is_store;
        io().dbg_branch_taken <<= ex_stage.io().branch_taken;
        io().dbg_exmem_valid <<= exmem_valid;
        io().dbg_exmem_is_store <<= exmem_is_store;
        io().dbg_mem_valid_out <<= mem_stage.io().mem_valid_out;
        io().dbg_load_use_detected <<= hazard.io().load_use_detected;
        // ⚠️⚠️ **写回事件端口 MUST 是「单拍脉冲」**（2026-11-24 · change
        //    `rv32i-commit-event-pulse-2026-11`）：
        //    `wb_stage.write_en = needs_writeback & rd_not_zero & mem_valid`
        //    （`stages/wb_stage.h:110`）⇒ ⇒ 而 `mem_valid` 来自 **MEM/WB 寄存器**
        //    ⇒ ⇒ MEM/WB 在 `mem_stall`（数据通路反压）时**冻结**（`:637-652`）
        //    ⇒ ⇒ 同一条指令在 WB 级**停留多拍** ⇒ ⇒ 事件电平**持续为高**
        //    ⇒ ⇒ 「事件次数」被污染成「事件时长之和」。
        //    ⇒ ⇒ 与 `commit_count` **同口径**（`commit_valid = … & !mem_stall`）
        //    ⇒ ⇒ **恰好一次**（不丢不重）：MEM/WB 的内容**只在非停顿沿换入**
        //    （`memwb_*->next = select(mem_stall, memwb_*, m2_*)`）⇒ ⇒ 一次驻留内
        //    **恰有一拍** `mem_stall = 0`（即该驻留的**最后一拍**）⇒ ⇒ 脉冲**恰好一次**
        //    ⇒ ⇒ 严格表述 = 「驻留内恰有一拍非停顿」，**不是**「驻留首拍必非停顿」
        //    （首拍之后 `mem_stall` 可**组合式**重新拉高 ⇒ 首拍**可能**是停顿拍）。
        //    ⚠️ **本端口是纯观测面**（零内部消费者）⇒ ⇒ 门控 `mem_stall` **不成环**。
        //    ⚠️ **对照**：`io().data_write_en`（store 事件）**不可**在这里门控 ——
        //    它在**功能路径**上（核顶层 → D-L1 的 `we`）⇒ ⇒ 门控它会形成
        //    `data_write_en → we → aw_fire → ready → data_ready → mem_stall →
        //    data_write_en` 的**组合环** ⇒ ⇒ 仿真直接 `FATAL: Cycle detected in IR
        //    graph during topological sort`（2026-11-24 实测）⇒ ⇒ store 事件
        //    MUST 在**总线侧**（核顶层）派生 ⇒ 见 `examples/riscv-mini/src/
        //    rv32i_core_top.h` 的 `obs_data_write_en`。
        auto mem_stall = !io().data_ready;
        io().dbg_wb_write_en <<= wb_stage.io().write_en & !mem_stall;
        io().dbg_wb_write_data <<= wb_stage.io().write_data;
        io().dbg_wb_rd_addr <<= wb_stage.io().rd_addr_out;
        io().dbg_exmem_alu_result <<= exmem_alu_result;
        io().dbg_exmem_imm <<= exmem_imm;
        io().dbg_exmem_rs2_data <<= exmem_rs2_data;
        // ⚠️ **MUST 在最后**（端口节点按赋值顺序编号 ⇒ 插中间会让既有
        //    `pipeline_unnamed_output_N` 整体位移 ⇒ 独立 TB 端口表错位）
        io().dbg_illegal_reason <<= id_stage.io().illegal_reason;
    }

    // ⚠️ **ID 级是否是「合法 CSR 指令」**（2026-11-25 · P6-2）
    //
    // ⚠️ **合法性判定的单一权威 = `Rv32iDecoder`**（`include/cpu/AGENTS.md` 的
    //    「非法指令判定唯一权威」纪律）⇒ ⇒ 这里**不重写** `funct3` 合法表，
    //    而是复用 ID 级已转发的 `illegal_reason`（= `valid & decoder.reason`）：
    //    `OP_SYSTEM` 且 `reason == 0` ⟺ 合法 CSR 指令（6 种 `funct3`）。
    //    ⇒ ⇒ 这样「`funct3 = 0` / `4` 仍冻结」与 CSR 判定**不会静默分叉**。
    // ⚠️ 两处调用（`build_datapath` / `build_control_signals`）各建一个比较节点
    //    ⇒ 值相同、无状态 ⇒ 不引入顺序依赖（与 `illegal_active` 同做法）。
    // ⚠️ **MUST 读 `if_stage` 的输出（父侧落地网）**，MUST NOT 读
    //    `id_stage.io().instruction` / `id_stage.io().valid` —— 那是**子组件的输入端口**
    //    ⇒ ⇒ 父层读它会产生**跨模块边**（层次化发射：ADR-060 §2.1 / ADR-066 D1
    //    「父层表达式 MUST 读父侧落地网」⇒ 实测报 `child side is not an output port`）。
    //    `if_stage.io().instruction` 与本层驱动给 `id_stage` 的是**同一条网**。
    ch_bool csr_in_id() {
        auto id_op = bits<6, 0>(if_stage.io().instruction);
        return if_stage.io().valid & (id_op == ch_uint<7>(OP_SYSTEM)) &
               (id_stage.io().illegal_reason == ch_uint<2>(0_d));
    }

    void describe() override {
        build_datapath();
        build_control_signals();
        build_forwarding_paths();
        build_debug_outputs();
    }
};

} // namespace riscv
