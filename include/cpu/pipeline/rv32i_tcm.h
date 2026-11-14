/**
 * @file rv32i_tcm.h
 * @brief RV32I TCM (Tightly Coupled Memory)
 * 
 * 指令和数据紧耦合存储器:
 * - I-TCM: 指令存储器 (ROM 模型)
 * - D-TCM: 数据存储器 (SRAM 模型)
 * - 单周期访问
 * - 可选 AXI4 接口
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "axi4/byte_lane.h"

namespace riscv {

using namespace ch::core;

// ============================================================================
// I-TCM (Instruction TCM) - 指令存储器
// ============================================================================

template <unsigned ADDR_WIDTH = 20, unsigned DATA_WIDTH = 32>
class InstrTCM : public ch::Component {
public:
    static constexpr unsigned SIZE = 1ULL << ADDR_WIDTH;  // 存储器大小
    
    __io(
        // 指令接口 (简化)
        ch_in<ch_uint<ADDR_WIDTH>> addr;
        ch_out<ch_uint<DATA_WIDTH>> data;
        ch_out<ch_bool> ready;
        
        // 写接口 (用于加载程序)
        ch_in<ch_bool> write_en;
        ch_in<ch_uint<ADDR_WIDTH>> write_addr;
        ch_in<ch_uint<DATA_WIDTH>> write_data;
    )
    
    InstrTCM(ch::Component* parent = nullptr, const std::string& name = "itcm")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 简化 ROM 模型
        // 实际实现应该使用 ch_mem
        ch_mem<ch_uint<DATA_WIDTH>, SIZE> memory("itcm_memory");
        
        // 读端口
        auto read_data = memory.sread(io().addr, ch_bool(true));
        io().data <<= read_data;
        io().ready = ch_bool(true);  // 单周期访问
        
        // 写端口 (仅用于初始化)
        memory.write(io().write_addr, io().write_data, io().write_en);
    }
};

// ============================================================================
// D-TCM (Data TCM) - 数据存储器
// ============================================================================

template <unsigned ADDR_WIDTH = 20, unsigned DATA_WIDTH = 32>
class DataTCM : public ch::Component {
public:
    static constexpr unsigned SIZE = 1ULL << ADDR_WIDTH;  // 存储器大小
    
    __io(
        // 数据接口 (简化)
        ch_in<ch_uint<ADDR_WIDTH>> addr;
        ch_in<ch_bool> write;
        ch_in<ch_uint<DATA_WIDTH>> wdata;
        ch_in<ch_bool> valid;
        // ⚠️ **字节使能**（2026-10-26 新增）：`sb` / `sh` MUST 只写**选中车道**。
        //    ⚠️ 约定（与 AXI 侧一致，见 `include/axi4/byte_lane.h`）：
        //      ① `addr` 是**字地址**（SoC 侧已做 `addr >> 2`）；
        //      ② `wdata` MUST 已**按车道对齐**（`sb`/`sh` 的源数据低位对齐，
        //         由流水线按 `addr[1:0]` 左移后送来 —— 见 `rv32i_pipeline.h`）；
        //      ③ 本组件用 `strbe` 把**选中车道**合并进原字。
        ch_in<ch_uint<4>> strbe;
        ch_out<ch_bool> ready;
        ch_out<ch_uint<DATA_WIDTH>> rdata;
    )
    
    DataTCM(ch::Component* parent = nullptr, const std::string& name = "dtcm")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // SRAM 模型
        ch_mem<ch_uint<DATA_WIDTH>, SIZE> memory("dtcm_memory");
        
        // ⚠️ **字节写 MUST 是「读-改-写」**（2026-10-26 新增）：
        //    `ch_mem::write(addr, data, en)` 只写**整字**（`include/core/mem.h`）
        //    ⇒ `sb` / `sh` 若直接写，会把**整字**覆盖（此前 SoC 里字节写
        //    **完全不生效**：流水线算好的 `data_strbe` 全仓**无消费者**）。
        //    ⇒ 先组合读出该字（`aread`，0-cycle，与 MEM 级同拍采样一致），
        //      再按 `strbe` 合并出要写的字，最后整字写回。
        //
        // ⚠️ 读-改-写的**同拍**语义：组合读返回的是**边沿前**的值 ⇒ 合并结果
        //    正是「旧字 + 新字节」⇒ 与 RISC-V 的字节写语义一致。
        //    ⚠️ 与 imem 的约定**不冲突**（ADR-041 只强制 **imem** 用 `sread`）。
        ch_uint<DATA_WIDTH> old_word;
        old_word <<= memory.aread(io().addr);
        auto lane_mask = axi4::detail::byte_lane_mask<DATA_WIDTH>(io().strbe);
        auto merged_word = axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(
            old_word, io().wdata, lane_mask);
        memory.write(io().addr, merged_word, io().write & io().valid);
        
        // 读操作（**load 通路**）
        //
        // ⚠️ **2026-11-13 改 `sread`（ADR-069 · 决策 A0）** —— 此前是 `aread`
        //    （0-cycle 组合读）。改动理由 = **访存数据对齐**：
        //    A0 让 MEM 区的级数从 2 变 3（`EX/MEM → M2 → MEM/WB`）⇒ ⇒
        //    load 数据 MUST 晚 1 拍可用 ⇒ ⇒ 车道选择 MUST 搬到 **M2 的下游**做
        //    （用 **M2 锁存的地址 / funct3** + **本组件 `sread` 输出寄存器**里
        //    同一拍的原始字 ⇒ ⇒ **两个输入同拍稳定** ⇒ ⇒ 组合完成）
        //    ⇒ ⇒ ⚠️ **「原始字寄存进 M2」是错的**（采到**上一拍**的字 ⇒ ⇒ 静默错车道）
        //    ⇒ ⇒ 详见 **ADR-069 §7.6**（唯一落点）。
        //    ⇒ 本组件的 `rdata` 因此从「与地址同拍」变成「**晚 1 拍**」。
        //
        // ⚠️ **`sread` 的 enable MUST 恒真**（ADR-068 §2.2-R6）：
        //    地址端口**每拍**都在变（`addr` ← `EX/MEM` 的地址）⇒ ⇒ 若门控成
        //    「仅 load 时发起」，则**连续两拍的两条 load** 会让第二次读覆盖第一次
        //    的输出 ⇒ ⇒ 数据丢失 ⇒ ⇒ **恒真发起** ⇒ ⇒ 消费者（M2）在**下一拍沿前**
        //    捕获「上一拍发出的地址」的数据 ⇒ ⇒ 零冲突。
        //
        // ⚠️ **字节写的 RMW 仍用 `aread`**（**零改动**）：`ch_mem` 无字节使能写
        //    API ⇒ ⇒ 读-改-写需要**同拍**的旧字 ⇒ ⇒ `aread`（组合读）返回
        //    **边沿前**的值 ⇒ ⇒ 合并结果正是「旧字 + 新字节」⇒ ⇒ 与 RISC-V
        //    的字节写语义一致 ⇒ ⇒ **与 `sread` 读口并存互不干扰**
        //    （`ch_mem` 支持多读端口）。
        auto load_data = memory.sread(io().addr, ch_bool(true));
        io().rdata <<= load_data;
        io().ready = ch_bool(true);  // 单周期访问
    }
};

// ============================================================================
// Boot ROM - 启动 ROM
// ============================================================================

template <unsigned ADDR_WIDTH = 12, unsigned DATA_WIDTH = 32>
class BootROM : public ch::Component {
public:
    static constexpr unsigned SIZE = 1 << ADDR_WIDTH;  // 4KB
    
    __io(
        ch_in<ch_uint<ADDR_WIDTH>> addr;
        ch_out<ch_uint<DATA_WIDTH>> data;
        ch_out<ch_bool> ready;
    )
    
    BootROM(ch::Component* parent = nullptr, const std::string& name = "boot_rom")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 简化：返回固定 NOP 指令 (0x00000013 = 十进制 19)
        // 实际应该包含启动代码
        io().data <<= ch_uint<DATA_WIDTH>(19_d);  // NOP 指令
        io().ready = ch_bool(true);
    }
};

} // namespace riscv
