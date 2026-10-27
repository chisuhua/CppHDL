/**
 * @file if_stage.h
 * @brief RISC-V 5 级流水线 - 取指级 (Instruction Fetch)
 * 
 * 功能:
 * - PC 寄存器 (32 位) 带时钟使能
 * - 指令存储器接口
 * - PC+4 计算
 * - 分支目标选择
 * - 停顿 (stall) 控制
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "rv32i_tcm.h"

namespace riscv {

using namespace ch::core;

/**
 * @brief IF 级 (取指级)
 * 
 * 从指令存储器中取指令，并传递给 ID 级
 */
class IfStage : public ch::Component {
public:
    __io(
        // 控制信号
        ch_in<ch_bool>      stall;          // 流水线停顿
        ch_in<ch_bool>      flush;          // 冲刷流水线 (分支预测错误)
        ch_in<ch_bool>      rst;            // 复位
        
        // 分支控制
        ch_in<ch_uint<32>>  branch_target;  // 分支目标地址
        ch_in<ch_bool>      branch_valid;   // 分支有效
        
        // 输出到 ID 级
        ch_out<ch_uint<32>> pc;             // 当前 PC
        ch_out<ch_uint<32>> instruction;    // 取出的指令
        ch_out<ch_bool>     valid;          // 有效信号
        
        // 指令存储器接口
        ch_out<ch_uint<32>> instr_addr;     // 指令地址输出
        ch_in<ch_uint<32>>  instr_data;     // 指令数据输入
        ch_in<ch_bool>      instr_ready;    // 指令存储器就绪
    )
    
    IfStage(ch::Component* parent = nullptr, const std::string& name = "if_stage")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // PC 寄存器 (32 位)
        // PC 寄存器存储当前指令地址
        // 复位值: 0x80000000 (ITCM 基地址, riscv-tests 入口点)
        ch_reg<ch_uint<32>> pc_reg(ch_uint<32>(0x80000000), "pc_reg");
        
        // ========================================================================
        // PC 更新逻辑
        // ========================================================================
        // 计算 PC+4 (RISC-V 指令固定 4 字节)
        auto pc_plus_4 = pc_reg + ch_uint<32>(4_d);
        
        // 选择下一个 PC 值：
        // - 分支有效时：使用分支目标地址
        // - 正常情况：使用 PC+4
        ch_uint<32> next_pc_target = select(
            io().branch_valid,
            io().branch_target,
            pc_plus_4
        );
        
        // PC 更新使能信号：
        // - 不复位 且 不 stall 时更新（**flush 不在此列** —— 见下方说明）
        //
        // ⚠️ `pc_update_en` **不看 `flush`**（2026-10-25，第 14 轮 critic NIT
        //    补注）：
        //    **隐式不变量**：flush 的**唯一**来源是
        //    `hazard.io().flush = ex_branch & ex_branch_taken`，而
        //    `next_pc_target` 的选择条件 `branch_valid` 接的正是同一个
        //    `ex_stage.io().branch_taken` ⇒ flush 为真时 `next_pc_target`
        //    **已经是**重定向目标 ⇒ PC 无需额外覆盖。
        //    ⚠️ **2026-10-26 更新（`JAL`/`JALR` 接入后）**：`ex_branch` 的成分
        //    从「仅条件分支（OP_BRANCH）」扩展为「条件分支 **+** 跳转
        //    （OP_JAL / OP_JALR）」，`ex_branch_taken` 也相应包含「JAL/JALR 恒 true」。
        //    ⇒ 不变量的**结构完全不变**（flush 仍只来自这一个表达式，且
        //    `branch_valid` 仍接同一个 `ex_stage.branch_taken`）；
        //    且 `ex_branch` 的两个成分**互斥**（一条指令不可能同时是 OP_BRANCH
        //    与 OP_JAL/OP_JALR）⇒ 不存在「两个来源同时成立」的情形。
        //    ⇒ 本 change **不需要**「第三路 PC 源」（`jump_valid` / `jump_target`）——
        //      新增第三路反而会引入**第二个**无条件跳转来源 ⇒ 必须重新论证本不变量。
        //    ⇒ 若将来引入**第三个** flush 来源（例如分支预测失败），此处 MUST
        //      改为「flush 时也更新 PC」。
        //
        // ⚠️ **flush 与 stall 不会同拍**（第 18 轮 critic 要求写明的互斥论证）：
        //    flush 由 **EX 级**的 `branch_taken` 产生 ⇒ 触发那一拍 EX 级只有
        //    那一条（分支）指令 ⇒ 它**不是** load ⇒ `ex_is_load` 为假 ⇒
        //    load-use 检测不可能同拍成立。
        //    ⇒ 若将来引入**第二个 flush 来源**（如预测失败）或让 stall 与
        //      flush 可同拍，MUST 重新审视此处（stall 冻结 PC 会让 PC 失去
        //      分支重定向）。
        //    ⚠️ **取指 skid buffer 也不改变本互斥论证**（ADR-048）：skid 的
        //    填充条件已排除 `flush | flush_d1`，且在写 `skid_valid` 时被
        //    flush 清空 ⇒ 「flush ∥ skid 非空」在**本微架构**下不可达
        //    （ADR-048 D5；组件级负向对照见
        //    `tests/test_rv32i_pipeline_behavior.cpp`）。
        auto pc_update_en = select(
            io().rst,
            ch_bool(false),
            select(io().stall, ch_bool(false), ch_bool(true))
        );
        
        // ⚠️ 历史注释「flush 时 PC 重置为 0 (简化处理，实际应该是分支目标)」是一个
        //    **占位实现**，它让每一次 flush 都把 PC 打回 0 ⇒ 程序每几拍就从头重启
        //    （SoC 级实测：41 条固件 200 拍只退休 18 条，`perf_branch_mispredict`
        //    与重启次数同步增长）。flush 的唯一来源是 `hazard.io().flush =
        //    is_branch & branch_taken`（`hazard_unit.h`），而 `branch_valid` 接的
        //    正是同一个 `ex_stage.branch_taken` ⇒ flush 时 `next_pc_target`
        //    已经是分支目标 ⇒ **不需要**任何 flush 覆盖。
        //
        // PC 寄存器更新
        pc_reg->next = select(
            pc_update_en,
            next_pc_target,
            pc_reg
        );
        
        // ========================================================================
        // 指令存储器接口
        // ========================================================================
        // 输出当前 PC 作为指令地址
        io().instr_addr = pc_reg;
        
        // 从指令存储器读取的指令 (组合逻辑)
        ch_uint<32> fetched_instr(0_d, "fetched_instr");
        fetched_instr = io().instr_data;
        
        // ========================================================================
        // 流水线寄存器 (IF 级到 ID 级的输出)
        // ========================================================================
        // IF/ID 流水线寄存器
        ch_reg<ch_uint<32>> if_id_pc_reg(0_d, "if_id_pc");
        ch_reg<ch_uint<32>> if_id_instr_reg(0_d, "if_id_instr");
        ch_reg<ch_bool>     if_id_valid_reg(false, "if_id_valid");

        // ⚠️⚠️ **flush MUST 多杀一拍**（2026-10-25，第 16 轮 critic BLOCKER B1）：
        //    imem 是 **1-cycle `sread`** ⇒ flush 那一拍（T）`pc_reg` 已经指向
        //    「分支 + 12 字节」处 ⇒ **该地址在 T 拍已经提交给 imem** ⇒
        //    T+1 拍数据才返回。
        //    ⇒ 只在 T 拍门控 IF/ID 装载**不够**：T+1 拍那条「错误路径」的
        //      指令会**正常锁存进 IF/ID** ⇒ 它被执行了（且在分支目标**之前**，
        //      顺序也乱）。
        //    ⇒ 用 `flush_d1`（flush 延迟一拍）把 T+1 拍的 IF/ID 装载**也**门掉。
        //    ⚠️ 代价：分支目标的指令要到 T+2 才进 ID ⇒ 冲刷气泡多 1 拍。
        ch_reg<ch_bool>     flush_d1(false, "flush_d1");

        // ⚠️ **pc 必须与指令同拍进 ID**（实测缺陷，见下）
        //
        // imem 是 **1-cycle `sread`**（ADR-041 强制：RV32I imem MUST sread）。
        // 时序：周期 T 提交地址 word W_T ⇒ 周期 T+1 `instr_data` = word W_T
        //       ⇒ 周期 T+2 `if_id_instr_reg` 装 word W_T。
        // 而 `pc_reg` 已在 T→T+1 边沿前进 ⇒ 直接锁存 `pc_reg` 会让
        // `if_id_pc_reg`(T+2) = pc_reg(T+1) = **word W_{T+1}** ⇒ 比它译码的
        // 指令**晚一个字**（实测：ID 解码 word T-1 而 dbg_id_pc 是 word T）。
        // ⇒ 分支目标 `pc + imm` 会算在错误的 pc 上（差 4 字节）。
        //
        // ⇒ pc 必须与指令**同样延迟一拍**：这里加一个 `fetch_pc_reg` 把 pc
        //    推迟一拍，再由 IF/ID 寄存器锁存它。
        ch_reg<ch_uint<32>> fetch_pc_reg(0_d, "fetch_pc");
        
        // flush 延迟一拍（见 `flush_d1` 的说明）
        flush_d1->next = io().flush;

        // IF/ID 寄存器更新使能：
        // - 指令存储器就绪
        // - 不复位
        // - 不 stall
        // - 不 flush（**也不在 flush 的下一拍** —— 见 `flush_d1`）
        auto if_id_update_en = select(
            io().rst,
            ch_bool(false),
            select(
                io().stall | io().flush | flush_d1,
                ch_bool(false),
                ch_bool(true)
            )
        );
        
        // fetch_pc_reg：把 pc 推迟一拍（与 imem 的 1-cycle sread 对齐）
        fetch_pc_reg->next = pc_reg;

        // ========================================================================
        // 取指 skid buffer（1 级）—— ADR-048 D1/D2，change
        // `rv32i-fetch-skid-buffer-2026-10`
        // ========================================================================
        // 为什么需要它（缺陷 A）：imem 是 1-cycle `sread` ⇒ 停顿首拍
        // `fetched_instr` / `fetch_pc_reg` 恰好给出「**已到达但 IF/ID 装不进**」
        // 的那一对；而 `pc_reg` 已越过它 ⇒ 其地址**不会**再次提交
        // ⇒ 没有存放点就等于**永久丢弃**（实测：既有固件 word 22 / 45 从未进 ID）。
        //
        // 逐拍（停顿窗口 T..T+d-1，ID 持有消费者 A_c，`pc_reg` 冻结在 A_c+8）：
        //   T      : stall ⇒ **填充** skid ← (fetched_instr = mem[A_c+4], fetch_pc_reg = A_c+4)
        //   T+1..  : skid 已满 ⇒ 不再填充（流上是 A_c+8，pc 冻结 ⇒ 可重读）
        //   T+d    : stall 释放 ⇒ **排空** IF/ID ← skid（= A_c+4）；流上数据被丢弃
        //   T+d+1  : 正常流（A_c+8）⇒ 下一拍 A_c+12
        //   ⇒ 序列 A_c → **A_c+4** → A_c+8 ⇒ 无丢失、无重复
        //
        // ⚠️ **不做 PC 回退**（ADR-048 D3）：停顿已让 `pc_reg` 冻结 ⇒ 地址流自我
        //    重复 ⇒ 落在 PC 之后的在途数据**至多一条**（D4）⇒ 1 级足够。
        //    `PC ← skid_pc` 会把已排空的指令**再取一次**（制造双装）；
        //    `PC ← skid_pc + 4` 恒等于冻结的 `pc_reg`（空操作）。
        ch_reg<ch_uint<32>> skid_instr(0_d, "skid_instr");
        ch_reg<ch_uint<32>> skid_pc(0_d, "skid_pc");
        ch_reg<ch_bool>     skid_valid(false, "skid_valid");

        // 排空 MUST 与 IF/ID 的**实际装载**同拍原子 ⇒ 直接由 `if_id_update_en`
        // 派生（该使能已含 `!rst` / `!stall` / `!flush` / `!flush_d1`）
        auto skid_drain_en = if_id_update_en & skid_valid;

        // 填充：只在**停顿拍**且 skid 空、且不在 flush 路径上
        // （`flush` 拍的数据属于错误路径 ⇒ 存进 skid 等于之后把它塞进 ID）
        auto skid_fill_en = select(
            io().rst,
            ch_bool(false),
            select(
                io().stall & io().instr_ready & !skid_valid &
                    !(io().flush | flush_d1),
                ch_bool(true),
                ch_bool(false)
            )
        );

        skid_instr->next = select(skid_fill_en, fetched_instr, skid_instr);
        // ⚠️ `skid_pc` MUST 取 `fetch_pc_reg`（**不是** `pc_reg`，两者差 4 字节 ——
        //    ADR-048 D2.1）⇒ 否则 skid 里的指令与它的 pc 错开一个字，
        //    分支目标会算错（与 `flush_d1` 缺陷同型）。
        skid_pc->next = select(skid_fill_en, fetch_pc_reg, skid_pc);
        // 排空优先于填充且**同拍互斥**（排空要求 `!stall`、填充要求 `stall`）；
        // flush（含 `flush_d1`）与 `rst` 一律清空（防御性契约，见上）
        skid_valid->next = select(
            skid_fill_en,
            ch_bool(true),
            select(
                skid_drain_en | io().flush | flush_d1 | io().rst,
                ch_bool(false),
                skid_valid
            )
        );

        // IF/ID 寄存器的取数源：`skid_valid ? skid : 流上`（pc 同理）
        auto if_id_instr_src = select(skid_valid, skid_instr, fetched_instr);
        auto if_id_pc_src    = select(skid_valid, skid_pc, fetch_pc_reg);

        // IF/ID 寄存器更新（pc 取**延迟一拍**的值，与指令同拍）
        if_id_pc_reg->next = select(
            if_id_update_en,
            if_id_pc_src,
            if_id_pc_reg
        );
        
        if_id_instr_reg->next = select(
            if_id_update_en,
            if_id_instr_src,
            if_id_instr_reg
        );
        
        // 有效信号在 flush 时清零（**含 flush 的下一拍** —— 见 `flush_d1`）
        //
        // ⚠️ **`primed` 门控**（ADR-048 D6，change `rv32i-fetch-skid-buffer-2026-10`
        //    的 C2）：取指填充期的「同一条指令被装两次」（缺陷 B）。
        //
        // 机制（实测，ADR-048 §1.3.3）：`load_firmware` 的**加载拍**（每 word 一拍，
        // `ext_rst=1`）让 imem 的 **sync 读端口**在复位期就照常采样 —— 解释器
        // **无 reset 意识**，Verilog 的 reset 分支只对框架 `default_reset` 生效
        // 而 `ext_rst` 是 fabric 端口 ⇒ **两后端对称**地把 proxy 预填成 word 0。
        // ⇒ 复位撤除后的**第 0 拍**，IF/ID 就会装进 word 0（幻影），
        //    第 1 拍再装一次真取指的 word 0 ⇒ 同一指令进流水线**两次**。
        //
        // ⇒ 判据 MUST **内生于 IfStage**：`instr_ready` **恒为 1**
        //    （`rv32i_tcm.h:58`）⇒ 不能由它导出「已就绪」。
        //
        // ⚠️ 默认只门控 **`valid`**（ADR-048 D6 方案 1）：幻影装载的 `pc` / `instruction`
        //    仍会装入（其 `pc` = 0x80000000、指令 = word 0，与下一拍的真取指
        //    **逐字相同**）⇒ `dbg_id_pc` 观测序列**零变化**；只有 `valid = 0`
        //    ⇒ `idex_update_en = io().valid`（`id_stage.h:248`）⇒ 幻影不进 EX
        //    ⇒ **退休数 −1**，且消除「自依赖首条指令 + 前推 ⇒ 非幂等」的正确性地雷。
        ch_reg<ch_bool>     primed(false, "primed");

        // 复位后**首个非复位周期**结束即置 1 ⇒ 第 0 拍的幻影装载 `primed = 0`
        primed->next = select(io().rst, ch_bool(false), ch_bool(true));

        if_id_valid_reg->next = select(
            if_id_update_en & primed,
            io().instr_ready,
            select(
                // ⚠️ `!primed` 也清零（不只是 flush）：复位后的首个周期
                // MUST 是**气泡** —— 否则「`valid` 保持复位前的旧值 1」而
                // `pc` / `instruction` 已装进幻影 ⇒ 有效位与数据**错配**。
                io().flush | flush_d1 | !primed,
                ch_bool(false),
                if_id_valid_reg
            )
        );
        
        // ========================================================================
        // 输出到 ID 级
        // ========================================================================
        io().pc = if_id_pc_reg;
        io().instruction = if_id_instr_reg;
        io().valid = if_id_valid_reg;
    }
};

} // namespace riscv
