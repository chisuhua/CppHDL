/**
 * @file hazard_unit.h
 * @brief RISC-V 流水线冒险检测单元（数据前推、停顿、刷新控制）
 * 
 * 参考：include/chlib/forwarding.h - 使用单独的 ch_uint 控制信号
 * 
 * 功能：
 * 1. 数据前推 (Forwarding): EX→EX, MEM→EX, WB→EX
 * 2. Load-Use 冒险检测：生成停顿信号
 * 3. 控制冒险处理：分支刷新
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */
#pragma once

#include "ch.hpp"
#include "component.h"

namespace riscv {

using namespace ch::core;

/**
 * @brief 冒险检测与前推单元
 * 
 * 参考 forwarding.h 模式：使用单独的控制信号端口而非 bundle 结构体
 */
class HazardUnit : public ch::Component {
public:
    __io(
        // ========================================================================
        // 流水线寄存器地址信息
        // ========================================================================
        ch_in<ch_uint<5>> id_rs1_addr;       // ID 阶段 RS1 地址
        ch_in<ch_uint<5>> id_rs2_addr;       // ID 阶段 RS2 地址
        ch_in<ch_uint<5>> ex_rd_addr;        // EX 阶段目标寄存器
        ch_in<ch_uint<5>> mem_rd_addr;       // MEM 阶段目标寄存器

        // ========================================================================
        // 写使能信号
        // ========================================================================
        ch_in<ch_bool> ex_reg_write;         // EX 级将写寄存器
        ch_in<ch_bool> mem_reg_write;        // MEM 级将写寄存器
        // ⚠️ **WB 级的写使能与目标寄存器已删除**（2026-10-27，ADR-047 D7）：
        //    它们曾是「预留输入」—— `ForwardingMux` 只有 3 档 ⇒ WB 档位用不上
        //    ⇒ 本组件内**零消费者** ⇒ 保留它们只是**公共 API 面的死重量**。
        //    ⇒ 顺带消掉一个**环饵**（`ex_alu_result` 曾被接到 EX 级**组合
        //      输出** ⇒ 留着就诱使后人「顺手消费它」⇒ **精确复现 bf78367**）。
        //    ⚠️ **距离 3** 的依赖**不靠 hazard 解决**（ADR-047 D11）⇒ 用
        //      「寄存器堆读点的 WB→ID 组合旁路」⇒ **不需要** WB 级端口。
        //
        // ========================================================================
        // 指令类型
        // ========================================================================
        ch_in<ch_bool> mem_is_load;          // MEM 级是加载指令
        // ⚠️ **EX 级**的加载标记（2026-10-25，change rv32i-pipeline-commit-path-2026-10）
        //    5 级流水线的标准 load-use 检测点是「**EX 级**的 load vs **ID 级**的
        //    消费者」：load 在 EX、消费者在 ID ⇒ 检测到的当拍，消费者的操作数
        //    **尚未锁存** ⇒ 停顿来得及生效。
        //    用 MEM 级（`mem_is_load`）会**晚一级**（消费者此时已进 EX、操作数
        //    已锁存）⇒ 停顿「停了个寂寞」（实测：`ld_stall` 恒 0）。
        ch_in<ch_bool> ex_is_load;           // EX 级是加载指令
        ch_in<ch_bool> ex_branch;            // EX 级是分支指令
        ch_in<ch_bool> ex_branch_taken;      // 分支已采取
        
        // ========================================================================
        // 原始寄存器数据
        // ========================================================================
        ch_in<ch_uint<32>> rs1_data_raw;     // 来自寄存器文件的 RS1
        ch_in<ch_uint<32>> rs2_data_raw;     // 来自寄存器文件的 RS2

        // ========================================================================
        // 前推控制输出 (参考 forwarding.h 模式：使用 ch_uint 而非 bundle)
        // ========================================================================
        // forward_a/b: 0=来自寄存器，1=来自 EX，2=来自 MEM
        // ⚠️ **档位编号表示「延迟一拍之后」的语义**（ADR-047 D5）：
        //    `hazard` 的比较点是「**EX 级** rd vs **ID 级** rs」⇒ 拍 T 算出的
        //    档位描述的是「T+1 消费者进 EX 时该选哪一级」⇒ 必须在**流水线级**
        //    **打一拍**后再喂给 mux ⇒ 本组件的输出**必须**被打拍。
        ch_out<ch_uint<2>> forward_a;        // RS1 的前推源选择
        ch_out<ch_uint<2>> forward_b;        // RS2 的前推源选择

        // ========================================================================
        // 直通数据输出
        // ========================================================================
        // ⚠️ **不是「经过前推 MUX 后的数据」**（旧注释这样说，是**假的** ——
        //    那时 `:198-199` 就是直通）⇒ 2026-10-27 如实更正。
        //    数据 mux 由**独立的** `chlib::ForwardingMux` 承担
        //    （ADR-046 D3 #14：mux 放进本组件并接进真实流水线装配会成**组合环**，
        //      `git show bf78367` 是当年的消环事故记录）。
        ch_out<ch_uint<32>> rs1_data;        // 直通的 RS1 数据（**不**含前推）
        ch_out<ch_uint<32>> rs2_data;        // 直通的 RS2 数据（**不**含前推）
        
        // ========================================================================
        // 冒险控制输出 (单独信号，不使用 bundle)
        // ========================================================================
        ch_out<ch_bool> stall;               // Load-Use 停顿信号
        // ⚠️ 「**这一拍**检测到 load-use 冒险」的脉冲（2026-10-25 补）：
        //    `stall` 是**保持型**（持续到被阻塞的 load 写回寄存器堆）⇒ 它计的是
        //    「**停顿拍数**」；而「**冒险次数**」需要单独的单拍脉冲信号。
        //    两者 MUST 可区分（capability `rv32i-commit-path` 的 Requirement）。
        ch_out<ch_bool> load_use_detected;   // Load-Use 冒险检测脉冲（单拍）
        ch_out<ch_bool> flush;               // 分支冲刷信号
        ch_out<ch_bool> pc_src;              // PC 源选择 (分支跳转)
    )
    
    HazardUnit(ch::Component* parent = nullptr, const std::string& name = "hazard") 
        : ch::Component(parent, name) {}
    
    void create_ports() override { 
        new (io_storage_) io_type; 
    }
    
    // -----------------------------------------------------------------------
    // 保持型停顿的状态（2026-10-25，change rv32i-pipeline-commit-path-2026-10）
    //
    // ⚠️ 为什么必须「保持」而不是单拍脉冲：
    //    在**无数据前推**的前提下，load 的数据要到 **WB** 拍才写回寄存器堆
    //    ⇒ 消费者至少要等 **3 拍**。单拍脉冲只能延迟 1 拍 ⇒ 仍读到旧值。
    //
    // ⚠️⚠️ 为什么用「**固定延迟的移位链**」而**不是**「按 rd 匹配解除」
    //    （第 14 轮 critic 的 BLOCKER 1，实测复现）：
    //    按 rd 匹配（WB 级写回的目标寄存器 == 被阻塞的 load 的 rd）**无法区分**
    //    「那条 load 写回了」与「**比它老一条**的指令写回了同一个 rd」：
    //      addi x5, x0, 7      ← 较老指令，写 x5
    //      lw   x5, 0x100(x0)  ← 被阻塞的 load，也写 x5
    //      add  x6, x5, x5     ← 紧邻消费者
    //    ⇒ 老指令在下一拍到达 WB ⇒ rd 匹配成立 ⇒ 停顿**提前一拍**解除
    //    ⇒ 消费者在 load 写回的**同一边沿**锁存操作数（`aread` 组合读 ⇒
    //      读到边沿前的旧值 7）⇒ **x6 = 14 而不是 0**（实测）。
    //
    //    ⇒ 改用**固定延迟**：load 从 EX 到 WB 是 **2 级**（EX/MEM → MEM/WB）
    //    ⇒ 它的数据在「检测拍 + 2」的边沿写回 ⇒ 消费者 MUST 等到
    //    「检测拍 + 3」才进 EX ⇒ 停顿 = **检测拍 + 3 拍**。
    //    ⇒ 用两级移位链实现，**不依赖 rd** ⇒
    //      ① 不会被「较老指令写同一 rd」提前解除；
    //      ② **load 被冲刷时也不会永久卡死**（固定延迟天然免疫 ——
    //         而「按写回解除」在 load 被冲刷时会**永远等不到写回** ⇒ 死锁）。
    // -----------------------------------------------------------------------
    ch_reg<ch_bool> stall_hold_d1{false, "stall_hold_d1"};
    ch_reg<ch_bool> stall_hold_d2{false, "stall_hold_d2"};

    void describe() override {
        // 将 IO 端口赋值给 auto 变量（避免类型转换问题）
        auto ex_reg_wr = io().ex_reg_write;
        auto mem_reg_wr = io().mem_reg_write;
        auto is_load = io().mem_is_load;
        auto is_branch = io().ex_branch;
        auto branch_taken = io().ex_branch_taken;
        
        auto ex_rd = io().ex_rd_addr;
        auto mem_rd = io().mem_rd_addr;
        auto id_rs1 = io().id_rs1_addr;
        auto id_rs2 = io().id_rs2_addr;
        
        // ========================================================================
        // 数据前推逻辑（参考 forwarding.h 模式）
        // ========================================================================
        
        // 检查非零寄存器 (x0 不需要前推)
        auto ex_rd_nz = ex_rd != ch_uint<5>(0_d);
        auto mem_rd_nz = mem_rd != ch_uint<5>(0_d);
        
        // RS1 地址匹配检测
        auto rs1_match_ex = ex_rd == id_rs1;
        auto rs1_match_mem = mem_rd == id_rs1;
        
        // RS2 地址匹配检测
        auto rs2_match_ex = ex_rd == id_rs2;
        auto rs2_match_mem = mem_rd == id_rs2;
        
        // 前推使能条件：reg_write && rd!=0 && match
        //
        // ⚠️ **EX 级的前推 MUST 排除 load**（ADR-047 D6「保守边界」）：
        //    `ex_reg_wr`（= `ex_reg_write`）**包含 load** ⇒ 若不排除，EX 级
        //    的 load 会命中档位 `1` ⇒ 而档位 `1` 的数据是 **EX/MEM 级的 ALU
        //    结果** ⇒ 对 load 而言那是**访存地址**，不是数据 ⇒ **前推错值**。
        //    ⇒ 保守形态下档位 `1` **不覆盖 load** ⇒ load 的数据仍要到 WB 拍
        //    才写回 ⇒ **load-use 停顿契约不变**（停顿是**选择**，不是无奈）。
        //    ⚠️ 该门控在**固件级不可达**（误选只打进气泡 ⇒ 结果被丢弃）
        //    ⇒ 它的验收 MUST 走**组件级真值表**（独立驱动 `ex_is_load=1`）。
        auto ex_is_load_n = io().ex_is_load;
        auto forward_ex_to_a = ex_reg_wr & ex_rd_nz & rs1_match_ex & !ex_is_load_n;
        auto forward_mem_to_a = mem_reg_wr & mem_rd_nz & rs1_match_mem;

        auto forward_ex_to_b = ex_reg_wr & ex_rd_nz & rs2_match_ex & !ex_is_load_n;
        auto forward_mem_to_b = mem_reg_wr & mem_rd_nz & rs2_match_mem;
        
        // 前推控制信号输出 (优先级：EX=1 > MEM=2 > REG=0)
        io().forward_a = select(forward_ex_to_a,
                                ch_uint<2>(1_d),
                                select(forward_mem_to_a,
                                       ch_uint<2>(2_d),
                                       ch_uint<2>(0_d)));
        
        io().forward_b = select(forward_ex_to_b,
                                ch_uint<2>(1_d),
                                select(forward_mem_to_b,
                                       ch_uint<2>(2_d),
                                       ch_uint<2>(0_d)));
        
        // ========================================================================
        // 直通数据输出（**不含前推**）
        // ⚠️ 数据 mux 由**独立的** `chlib::ForwardingMux` 承担
        //    （ADR-046 D3 #14 / ADR-038）⇒ 本组件**只输出档位选择**，
        //    数据选择**不在**这里做 ⇒ 这里没有组合环风险。
        // ========================================================================
        io().rs1_data = io().rs1_data_raw;
        io().rs2_data = io().rs2_data_raw;
        
        // ========================================================================
        // Load-Use 冒险检测
        // ========================================================================
        // ---- EX 级检测（**流水线装配走这一路**）----
        // 「EX 级的 load」与「ID 级的消费者」比较 ⇒ 检测当拍消费者的操作数
        // 尚未锁存 ⇒ 停顿来得及生效。
        //
        // ⚠️ 已知局限（2026-10-25，第 15 轮 critic NIT）：本检测**不校验
        //    ID 侧是否真的在消费**（`io().valid` / rs 是否真被使用）⇒ 若 IF/ID
        //    里 valid=0 的残留指令字**碰巧**匹配 `ex_rd` ⇒ 会产生一次
        //    **假停顿**（3 拍，纯延迟，无语义影响）⇒ 固件一变，钉死的
        //    「停顿拍数 == 6」会漂移 ⇒ 断言时 MUST 留意这一点。
        auto ex_rs1_match = ex_rd == id_rs1;
        auto ex_rs2_match = ex_rd == id_rs2;
        auto ex_any_match = ex_rs1_match | ex_rs2_match;
        auto ex_load_use = io().ex_is_load & ex_rd_nz & ex_any_match;

        // ---- MEM 级检测（保留给**独立使用**本组件的场景）----
        auto rs1_match_load = mem_rd == id_rs1;
        auto rs2_match_load = mem_rd == id_rs2;
        auto any_rs_match = rs1_match_load | rs2_match_load;
        auto mem_load_use = is_load & mem_rd_nz & any_rs_match;

        // ⚠️ 两条检测路**或**在一起（组件级语义）：
        //    流水线装配只驱动 `ex_is_load`（EX 级那一路）；`mem_is_load` 那一路
        //    在流水线里**永不触发**（MEM 级 load 到达时消费者早已进 EX），
        //    但它让 `HazardUnit` 作为**独立组件**仍支持 MEM 级输入
        //    （既有测试 `test_rv32i_pipeline.cpp` / `test_pipeline_exec.cpp`
        //    直接驱动 `mem_is_load`）。
        auto load_use_hazard = ex_load_use | mem_load_use;
        
        // ========================================================================
        // 控制冒险处理
        // ========================================================================
        auto branch_flush = is_branch & branch_taken;
        
        // ========================================================================
        // 保持型停顿（固定延迟的移位链）
        // ========================================================================
        // 检测拍 T：`ex_load_use` 为真 ⇒
        //   T   ：组合置位 ⇒ 停顿当拍生效
        //   T+1 ：`stall_hold_d1` 为真
        //   T+2 ：`stall_hold_d2` 为真（此刻那条 load 正在 WB ⇒ 边沿写回）
        //   T+3 ：全假 ⇒ 放行（消费者在 T+3 的边沿进 ID/EX ⇒ 读到**新值**）
        stall_hold_d1->next = ex_load_use;
        stall_hold_d2->next = stall_hold_d1;

        // ========================================================================
        // 控制信号输出
        // ========================================================================
        // ⚠️ `stall` = 「组合置位（当拍生效）+ 两级移位保持（固定 3 拍）」
        //    组合部分用 `load_use_hazard`（EX 级 + MEM 级两路）：
        //    流水线装配只接 EX 级那一路（`mem_is_load` 保持未连接）⇒
        //    在流水线里 `stall` 完全由 EX 级检测驱动；
        //    而「独立使用 HazardUnit」的场景可以直接驱动 `mem_is_load`
        //    得到一个单拍停顿（既有测试即此用法）。
        io().stall = load_use_hazard | stall_hold_d1 | stall_hold_d2;
        // 冒险脉冲（单拍）—— 与上面的「停顿拍数」区分开。
        // ⚠️ 只反映 **EX 级**那一路（流水线装配走的就是它）⇒ 「独立使用
        //    HazardUnit」并驱动 `mem_is_load` 的场景会得到 stall 但**没有脉冲**
        //    ⇒ 「次数 vs 拍数可区分」对那种用法**不成立**。
        io().load_use_detected = ex_load_use;
        io().flush = branch_flush;
        io().pc_src = branch_flush;
    }
};

} // namespace riscv
