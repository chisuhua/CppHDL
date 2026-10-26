/**
 * @file interconnect_common.h
 * @brief AXI **互连层**（4×4 crossbar / 2×2 matrix）共用的契约类型
 *
 * 契约：`openspec/specs/axi-interconnect-routing/spec.md`
 *
 * 本头只放**两个互连组件共享**的类型：
 *   * `AxiResp` —— AXI 响应码（互连层的 error responder 用它回 DECERR；
 *     定义在 `axi4/axi_resp.h`，**全 AXI 家族的唯一权威**）
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

#include "axi4/axi_resp.h"
#include <cstdint>

namespace axi4 {

// AXI 响应码来自 `axi4/axi_resp.h`（**全 AXI 家族的唯一权威定义**）

// 地址窗口：`size` MUST 是 2 的幂，`base` MUST 按 `size` 对齐。
// 窗口命中判据 = `(addr & ~(size-1)) == base`
//   —— MUST NOT 用运行期移位算 `2^N`（`size` 恒为 2 的幂，编译期常量掩码即可）。
struct AxiAddrWindow {
    uint64_t base;
    uint64_t size;
};

// 窗口合法性（**编译期**校验 —— 传错会得到静默的错误解码）：
//   * `size` MUST 非 0 且为 2 的幂（否则 `~(size-1)` 掩码不是「按 size 对齐」）
//   * `base` MUST 按 `size` 对齐（否则窗口会与其前一个窗口重叠）
constexpr bool axi_window_is_valid(const AxiAddrWindow &w) {
    return w.size != 0 && (w.size & (w.size - 1)) == 0 && (w.base & (w.size - 1)) == 0;
}

// 窗口 MUST **完整落在** `addr_width` 位地址空间内。
//
// ⚠️ 为什么必需：`window_hit` 会把 base / 掩码**截到 `addr_width` 位**再构造逻辑值
//   （否则调试构建触发 `CH_CHECK`「value out of range」）。截断的副作用是：
//   `base >= 2^addr_width` 的窗口会**静默别名**到低地址（与窗口 0 双命中 ⇒
//   按优先级静默错路由），而截断**消除**了原本唯一的配置错误检测。
//   ⇒ 用编译期断言把这个检测补回来（`decode_addr` 里实例化）。
//
// 用 `base | (size - 1)` 而不是 `base + size - 1`：后者在极端配置下会 64 位溢出。
constexpr bool axi_window_fits(const AxiAddrWindow &w, unsigned addr_width) {
    return addr_width >= 64 ? true : ((w.base | (w.size - 1)) >> addr_width) == 0;
}

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
