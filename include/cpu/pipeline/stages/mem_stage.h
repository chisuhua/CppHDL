/**
 * @file mem_stage.h
 * @brief RISC-V 5 级流水线 - MEM 级 (访存级)
 * 
 * 功能:
 * - 数据存储器接口 (加载/存储)
 * - ⚠️ **2026-11-13（ADR-069 A0）**：**load 的车道选择 / 扩展语义已迁出**
 *   ⇒ ⇒ 本组件现在只承担 **store 侧**（字节使能）与**元数据透传**
 * - 字节/半字/字处理（**store 侧**）
 * - 数据前向到 WB 级
 * 
 * 作者：DevMate
 * 最后修改：2026-04-09
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/arithmetic.h"
#include "chlib/logic.h"

namespace riscv {

using namespace ch::core;

/**
 * MEM 级 (访存级) 组件
 * 
 * 包含:
 * - 数据内存接口 (加载和存储)
 * - 支持 LB/LH/LW/LBU/LHU 加载指令
 * - 支持 SB/SH/SW 存储指令
 * - 符号扩展和零扩展处理
 * - 数据多选器 (ALU 结果或内存数据)
 */
class MemStage : public ch::Component {
public:
    __io(
        // ====================================================================
        // 输入端口：来自 EX/MEM 流水线寄存器
        // ====================================================================
        ch_in<ch_uint<32>> alu_result;   // ALU 结果 (内存地址 for load/store)
        ch_in<ch_uint<32>> rs2_data;     // RS2 数据 (存储数据)
        ch_in<ch_uint<5>>  rd_addr;     // 目的寄存器地址 (传递到 WB)
        // ⚠️ **2026-11-13（ADR-069 A0）**：`is_load` **保留**（公开端口）但本组件
        //    **内部不再消费它** —— load 的车道选择 / 扩展语义已迁出到
        //    `Rv32iPipeline` 的 `m2_load_data`（M2 的下游）。
        //    ⇒ ⇒ **保留**而非删除的理由：它是**公开 API 的一部分**
        //    （外部可读 / 可驱动）⇒ ⇒ 按根 `AGENTS.md` 的 ZERO-DEBT 判决边界
        //    （「零『仓内』消费者的『公开』符号**不算** dead code」）⇒ ⇒ 不删。
        ch_in<ch_bool>     is_load;      // 是否为加载指令（本组件**不**消费）
        ch_in<ch_bool>     is_store;     // 是否为存储指令
        ch_in<ch_uint<3>>  funct3;       // 功能码 (决定数据宽度)
        ch_in<ch_bool>     mem_valid;    // 内存访问有效信号 (来自控制单元)

        // ====================================================================
        // 输出端口：写入 MEM/WB 流水线寄存器
        // ====================================================================
        ch_out<ch_uint<32>> alu_result_out; // ALU 结果传递到 WB (流水线寄存器)
        ch_out<ch_uint<5>>  rd_addr_out;  // rd_addr 流水线寄存器
        // 存储字节使能（SB=0001/0010/0100/1000 · SH=0011/1100 · SW=1111）
        ch_out<ch_uint<4>>  store_byte_enable_out;
        ch_out<ch_bool>     mem_valid_out; // 内存数据有效 (用于 WB 级多选)
    )

    MemStage(ch::Component* parent = nullptr, const std::string& name = "mem_stage")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // ====================================================================
        // 1. 内存访问控制逻辑
        // ====================================================================
        // ⚠️ **load 的访存控制已迁出**（2026-11-13 · ADR-069 A0）⇒ ⇒ 本组件
        //    现在只承担 **store 侧**（字节使能）与**元数据透传**。
        // mem_valid 信号控制是否真正访问内存

        // ====================================================================
        // 2. ⚠️ **load 的车道选择 / 扩展语义已迁出本组件**（2026-11-13 · ADR-069 A0）
        // ====================================================================
        // 此前本组件负责 `LB`/`LH`/`LW`/`LBU`/`LHU` 的**车道选择**与**符号/零扩展**
        // （消费 `io().mem_read_data` + `io().alu_result` 的地址低位）。
        //
        // ⚠️ **为什么必须迁出**：`sread` 让 load 数据**晚 1 拍**可用 ⇒ ⇒
        //    **MEM 级（拍 N+1）已经拿不到正确数据** ⇒ ⇒ 若仍在这里做车道选择
        //    ⇒ ⇒ 选出来的是**上一拍**的数据 ⇒ ⇒ `LB`/`LH` **静默错车道**。
        // ⇒ ⇒ 车道选择 MUST 在 **M2 的下游**做（用 `M2` 锁存的**地址**与
        //    **funct3** + `M2` 锁存的**原始字**）⇒ ⇒ 载体 = `Rv32iPipeline`
        //    （见 `rv32i_pipeline.h` 的 `m2_load_data`）⇒ ⇒ **ADR-069 V5**。
        //
        // ⚠️ **store 侧的字道移位 / 字节使能仍留在本组件**（**零改动**）：
        //    store 的地址呈现拍**不变**（仍取 `EX/MEM` 级的 `io().alu_result`）
        //    ⇒ ⇒ 搬移它反而会**错一拍**（ADR-069 V5：「搬移了 ⇒ ⇒ 反而错」）。
        //
        // ⚠️ **公开面变更**（**2** 个端口）：删 `mem_read_data`（输入）/ `mem_data`（输出）
        //    ⇒ ⇒ `is_load` **保留**（见端口声明处的理由）
        //    ⇒ ⇒ 登记见 `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md`。
        //
        // ---- store 侧仍需要的地址位提取（**保留**）----
        // 字节道按 `addr[1:0]`（4 个车道）；半字道按 `addr[1]`（2 个车道）
        // ⇒ ⚠️ 半字道**只**看 `addr[1]` ⇒ **半字对齐**（`addr[0]` 被忽略）
        auto byte_offset = bits<1, 0>(io().alu_result);
        auto half_offset = bit_select(io().alu_result, 1_d);


        // ====================================================================
        // 3. 存储指令处理 (Store)
        // ====================================================================
        // 存储指令不需要输出数据到 WB 级，但需要生成存储器写使能和写数据
        // 这里我们只输出一个存储有效的标志

        // 存储字节使能信号 (用于内存接口的 strbe)
        // 根据 funct3 生成对应的字节使能
        //
        // ⚠️ 2026-10-25（第 14 轮 critic SHOULD_FIX #5/#6）：此前这里算完
        //    **从不输出** ⇒ 死代码，且 `data_strbe` 在流水线里被**旁路**
        //    （恒 0xF）⇒ SB/SH 的字节使能**从未生效**。
        //    ⇒ 现在把它**接到输出端口** `store_byte_enable_out`，
        //      由 `Rv32iPipeline::data_strbe` 转出 ⇒ SB/SH 字节使能真正生效。
        ch_uint<4> store_byte_enable = ch_uint<4>(0_d);
        
        // SB (000): 存储单个字节
        auto sb_be = select(byte_offset == ch_uint<2>(0_d), ch_uint<4>(1_d),   // 0001
                       select(byte_offset == ch_uint<2>(1_d), ch_uint<4>(2_d),   // 0010
                       select(byte_offset == ch_uint<2>(2_d), ch_uint<4>(4_d),   // 0100
                              ch_uint<4>(8_d))));                                // 1000
        
        // SH (001): 存储半字 (2 字节)
        auto sh_be = select(half_offset == ch_bool(false), ch_uint<4>(3_d),     // 0011
                            ch_uint<4>(12_d));                                  // 1100
        
        // SW (010): 存储字 (4 字节)
        auto sw_be = ch_uint<4>(15_d);  // 1111

        // 存储字节使能选择器
        store_byte_enable = select(io().funct3 == ch_uint<3>(0_d), sb_be, store_byte_enable);
        store_byte_enable = select(io().funct3 == ch_uint<3>(1_d), sh_be, store_byte_enable);
        store_byte_enable = select(io().funct3 == ch_uint<3>(2_d), sw_be, store_byte_enable);


        // ====================================================================
        // 4. MEM 级输出多选器
        // ====================================================================
        // MEM 级输出：
        // - 对于加载指令：输出从内存读取的数据
        // - 对于存储指令：输出 0 (存储指令不写回寄存器)
        // - 对于其他指令：传递 ALU 结果到 WB 级 (在 WB 级处理)
        
        // 内存数据有效信号：只有加载指令才有效
        //
        // ⚠️ 2026-10-28（change `rv32i-load-byte-lane-2026-10`）：**删除**这个
        //    中间量。`io().mem_valid_out` 已改为直接接 `io().mem_valid`
        //    （见下方注释）⇒ `mem_data_valid` **再无消费者**
        //    ⇒ `-Wunused-but-set-variable` 警告 ⇒ 违反 ZERO-DEBT
        //    「changed files MUST warning-clean」。

        // ====================================================================
        // 5. 输出端口连接
        // ====================================================================

        // ⚠️ **load 数据输出端口已删**（2026-11-13 · ADR-069 A0）⇒ ⇒
        //    `mem_data` 的载体 = `Rv32iPipeline` 的 `m2_load_data`（M2 的下游）。

        // ⚠️ **`mem_valid_out` 是「MEM 级指令有效」信号**（MEM/WB 与 WB 都用它
        //    判断「WB 级那条指令有效」），**不是**「load 数据有效」：
        //    此前实现为 `mem_data_valid`（= is_load & mem_valid）⇒
        //      ① `perf_instructions`（= commit_count）统计的是 **load 次数**
        //         而不是退休指令数；
        //      ② MEM/WB 的 `memwb_is_alu` 永远不更新（它的使能是 mem_valid）
        //         ⇒ **ALU 指令永远不写回寄存器堆**（实测：x1/x2/x3/x4 恒 0，
        //            store 的写数据恒 0）。
        //    ⇒ 改为 MEM 级有效；「load 数据有效」只在内部数据选择器里用。
        io().mem_valid_out = io().mem_valid;
        // 字节使能输出（SB/SH/SW）
        io().store_byte_enable_out = store_byte_enable;

        // ALU 结果传递到 WB 级
        io().alu_result_out = io().alu_result;

        // rd_addr 流水线寄存器传递到 WB 级 (FIX: P0 bug - rd_addr_out was declared but never assigned)
        io().rd_addr_out = io().rd_addr;
    }
};

} // namespace riscv
