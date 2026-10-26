/**
 * @file axi_resp.h
 * @brief AXI 响应码（**全 AXI 家族的唯一权威定义**）
 *
 * 编码（AXI4 规范）：
 *   * `OKAY`   = 0b00 —— 正常完成
 *   * `EXOKAY` = 0b01 —— 独占访问被接受（AXI4 特有）
 *   * `SLVERR` = 0b10 —— 从设备侧错误
 *   * `DECERR` = 0b11 —— 译码阶段即被拒绝（**地址未映射**）
 *
 * ⚠️ 此前 `axi4_full.h` 定义 `AxiResp`、`axi_interconnect_4x4.h` 又定义了一个
 *   逐值相同的 `AxiResp4x4` —— 两者的 include guard 宏名不同
 *   （`AXI4_AXIRESP_DEFINED` vs `AXI4_AXIRESP_DEFINED_IC`）
 *   ⇒ 那个 guard **永远不可能碰撞** ⇒ 是**死 guard**，且类型重复。
 *   ⇒ 现统一到本头。
 */

#pragma once

#include <cstdint>

namespace axi4 {

// AXI 响应码（`ch_uint<2>` 的编码）
// 注：`#pragma once` 已保证本头只被展开一次 ⇒ 无需再套 `#ifndef` 宏 guard
//    （本头存在的理由之一正是消灭一个永不可能碰撞的死 guard 宏）。
enum class AxiResp : uint8_t {
    OKAY   = 0b00,
    EXOKAY = 0b01,
    SLVERR = 0b10,
    DECERR = 0b11
};

}  // namespace axi4
