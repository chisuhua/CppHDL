/**
 * @file load_lane_selector.h
 * @brief RV32I 的 **load 车道选择器**（字节道 / 半字道 + 符号扩展 / 零扩展）
 *
 * ⚠️ **2026-11-13 新增**（ADR-069 决策 **A0** · §3.3 A0-① + §5 **V5**）：
 *   load 的车道选择语义从 `riscv::MemStage` **迁出**到本组件。
 *
 *   **为什么迁出**：`sread` 让 load 数据**晚 1 拍**可用 ⇒ ⇒ **MEM 级（拍 N+1）
 *   已经拿不到正确数据** ⇒ ⇒ 若仍由 `MemStage`（位于 MEM 级）做车道选择
 *   ⇒ ⇒ 选出来的是**上一拍**的数据 ⇒ ⇒ `LB`/`LH` **静默错车道**。
 *   ⇒ ⇒ 车道选择 MUST 在 **M2 的下游**做（用 M2 锁存的**地址** + **funct3**
 *   + M2 锁存的**原始字**）⇒ ⇒ **M2 的下游 = 本组件**。
 *
 *   **为什么是独立组件**（而不是内联在 `Rv32iPipeline` 里）：
 *   ① **单一真相**（ADR-046 的收敛方向）—— 车道选择只有**一份**实现；
 *   ② **组件级可测** —— `MemStage` 迁出后，组件级的双后端覆盖 MUST 有载体
 *      （载体 = `tests/test_rv32i_stage_dual.cpp` 的 `LoadLaneSelector` pair）
 *      ⇒ ⇒ **覆盖不缩面**（若内联 ⇒ 组件级覆盖消失 ⇒ 只能靠固件级 ⇒ 判别力更弱）。
 *
 * **语义**（逐字迁自 `MemStage`，**零改动**）：
 *   - 字节道按 `addr[1:0]`（4 个车道）· 半字道按 `addr[1]`（2 个车道）
 *     ⇒ ⚠️ 半字道**只**看 `addr[1]` ⇒ **半字对齐**（`addr[0]` 被忽略）
 *   - `LB`/`LH` **符号**扩展 · `LBU`/`LHU` **零**扩展 · `LW` 整字
 *   - ⚠️ **符号扩展 MUST 用 `sext<32>` 原语**（`include/core/operators_width.h`），
 *     **MUST NOT** 手写填充掩码 —— 此前 `MemStage` 用
 *     `ch_uint<32>(4278190080_d)`（= `0xFF000000`，而注释写 `0xFFFFFF00`）
 *     ⇒ `lb` 返回 `0xFF0000C3`（应为 `0xFFFFFFC3`）
 *     ⇒ ⇒ `sext` 在 interpreter / JIT / Verilog codegen **三后端都支持**
 */

#pragma once

#include "ch.hpp"
#include "component.h"

namespace riscv {

using namespace ch::core;

/**
 * load 车道选择器（组合逻辑 · **无状态**）
 *
 * 输入 = **M2 锁存的**三项（地址 / funct3 / 原始字）
 * 输出 = 车道选择 + 扩展后的 32 位值
 */
class LoadLaneSelector : public ch::Component {
public:
    __io(
        // ⚠️ **原始字**（未做车道选择）—— 来源 = 存储器的 `sread` 输出
        // ⚠️ **端口具名**（2026-11-14 · 端口命名棘轮 **N1** 约定）⇒ ⇒ 新组件的端口
        //    MUST **自带名字** ⇒ ⇒ 产物端口名可读（`..._raw_word` / `..._addr` /
        //    `..._funct3` / `..._data`）⇒ ⇒ **不进**棘轮豁免表（棘轮只许减）
        ch_in<ch_uint<32>> raw_word{"raw_word"};
        // ⚠️ **访存地址**（**字节**地址；低 2/1 位决定车道）
        ch_in<ch_uint<32>> addr{"addr"};
        // ⚠️ **功能码**（决定宽度与扩展方式）
        //    000 LB · 001 LH · 010 LW · 100 LBU · 101 LHU
        ch_in<ch_uint<3>>  funct3{"funct3"};
        // ⚠️ 车道选择 + 扩展后的值（**零扩展/符号扩展**已施加）
        ch_out<ch_uint<32>> data{"data"};
    )

    LoadLaneSelector(ch::Component* parent = nullptr,
                     const std::string& name = "load_lane_selector")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // ---- 字节道（4 个）----
        // 提取地址的低位 2bit 来确定字节位置
        auto byte_offset = bits<1, 0>(io().addr);

        auto byte0 = bits<7, 0>(io().raw_word);    // 地址 [1:0] == 00
        auto byte1 = bits<15, 8>(io().raw_word);   // 地址 [1:0] == 01
        auto byte2 = bits<23, 16>(io().raw_word);  // 地址 [1:0] == 10
        auto byte3 = bits<31, 24>(io().raw_word);  // 地址 [1:0] == 11

        // 字节选择器
        ch_uint<8> selected_byte = ch_uint<8>(0_d);
        selected_byte = select(byte_offset == ch_uint<2>(0_d), byte0, selected_byte);
        selected_byte = select(byte_offset == ch_uint<2>(1_d), byte1, selected_byte);
        selected_byte = select(byte_offset == ch_uint<2>(2_d), byte2, selected_byte);
        selected_byte = select(byte_offset == ch_uint<2>(3_d), byte3, selected_byte);

        // LB: 符号扩展到 32 位
        // ⚠️ MUST 用 `sext<32>` 原语（理由见文件头注释）
        auto byte_sign = bits<7, 7>(selected_byte);
        ch_uint<32> lb_result = select(byte_sign,
            sext<32>(selected_byte),
            zext<32>(selected_byte));

        // LBU: 零扩展到 32 位
        ch_uint<32> lbu_result = zext<32>(selected_byte);

        // ---- 半字道（2 个）----
        // 提取地址的低位 1bit 来确定半字位置 (半字对齐)
        auto half_offset = bit_select(io().addr, 1_d);

        auto half0 = bits<15, 0>(io().raw_word);   // 地址 [1] == 0
        auto half1 = bits<31, 16>(io().raw_word);  // 地址 [1] == 1

        // 半字选择器
        ch_uint<16> selected_half = select(half_offset == ch_bool(false), half0, half1);

        // LH: 符号扩展到 32 位
        auto half_sign = bits<15, 15>(selected_half);
        ch_uint<32> lh_result = select(half_sign,
            sext<32>(selected_half),
            zext<32>(selected_half));

        // LHU: 零扩展到 32 位
        ch_uint<32> lhu_result = zext<32>(selected_half);

        // ---- 字加载 (LW) ----
        // 直接读取 32 位数据，无需扩展
        ch_uint<32> lw_result = io().raw_word;

        // ---- 结果多选器 ----
        // 根据 funct3 选择正确的加载结果
        ch_uint<32> load_result = ch_uint<32>(0_d);

        // LB (000) - 符号扩展字节
        load_result = select(io().funct3 == ch_uint<3>(0_d), lb_result, load_result);
        // LH (001) - 符号扩展半字
        load_result = select(io().funct3 == ch_uint<3>(1_d), lh_result, load_result);
        // LW (010) - 字
        load_result = select(io().funct3 == ch_uint<3>(2_d), lw_result, load_result);
        // LBU (100) - 零扩展字节
        load_result = select(io().funct3 == ch_uint<3>(4_d), lbu_result, load_result);
        // LHU (101) - 零扩展半字
        load_result = select(io().funct3 == ch_uint<3>(5_d), lhu_result, load_result);

        io().data = load_result;
    }
};

} // namespace riscv
