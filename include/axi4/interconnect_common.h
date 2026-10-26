/**
 * @file interconnect_common.h
 * @brief AXI **互连层**（4×4 crossbar / 2×2 matrix）共用的契约类型
 *
 * 契约：`openspec/specs/axi-interconnect-routing/spec.md`
 *
 * 本头只放**两个互连组件共享**的类型：
 *   * `AxiResp4x4` —— AXI 响应码（互连层的 error responder 用它回 DECERR）
 *   * `AxiAddrWindow` + 两个默认地址映射 —— **编译期可配置**的地址窗口
 *
 * ⚠️ **为什么地址映射 MUST 可配置**：
 *   旧实现把 slave 索引硬编码成「地址的某个固定位」（4×4 是 `awaddr >> 30`，
 *   2×2 是 `awaddr >> (ADDR_WIDTH-1)`）⇒ **整个地址空间都被映射** ⇒
 *   DECERR 分支**永远不可达**（静默的死代码）。
 *
 * ⚠️ **默认映射的覆盖度**（契约的一部分，MUST 被记录）：
 *   下面两个默认映射都把**全部**地址空间铺满 ⇒ **默认配置下 DECERR 不可触发**。
 *   ⇒ 测 DECERR 的用例 MUST 用**非默认映射**实例化（见各测试文件的 `SparseTestMap`）。
 */

#pragma once

#include <cstdint>

namespace axi4 {

#ifndef AXI4_AXIRESP_DEFINED_IC
#define AXI4_AXIRESP_DEFINED_IC
// AXI 响应码（`ch_uint<2>` 的编码）
enum class AxiResp4x4 : uint8_t {
    OKAY   = 0b00,
    EXOKAY = 0b01,
    SLVERR = 0b10,
    DECERR = 0b11
};
#endif

// 地址窗口：`size` MUST 是 2 的幂，`base` MUST 按 `size` 对齐。
// 窗口命中判据 = `(addr & ~(size-1)) == base`
//   —— MUST NOT 用运行期移位算 `2^N`（`size` 恒为 2 的幂，编译期常量掩码即可）。
struct AxiAddrWindow {
    uint64_t base;
    uint64_t size;
};

// 默认映射（4×4 互连）：4 个 1 GiB 窗口把 32 位地址空间完整铺满。
// ⚠️ 与旧的硬编码 `awaddr >> 30` **逐位等价**（避免破坏既有实例化点）。
struct AxiDefaultInterconnectMap {
    static constexpr AxiAddrWindow windows[4] = {
        {0x0000'0000ull, 0x4000'0000ull},
        {0x4000'0000ull, 0x4000'0000ull},
        {0x8000'0000ull, 0x4000'0000ull},
        {0xC000'0000ull, 0x4000'0000ull},
    };
};

// 默认映射（2×2 矩阵）：2 个 2 GiB 窗口 —— 与旧实现的
// `awaddr >> (ADDR_WIDTH-1)` **逐位等价**（最高位 0 ⇒ s0，最高位 1 ⇒ s1）。
struct AxiLiteDefaultMatrixMap {
    static constexpr AxiAddrWindow windows[2] = {
        {0x0000'0000ull, 0x8000'0000ull},
        {0x8000'0000ull, 0x8000'0000ull},
    };
};

}  // namespace axi4
