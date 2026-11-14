/**
 * @file rv32i_fw_demo.h
 * @brief RV32I **带 `.data` 段的示例固件**（`firmware-load-chain` **L2 = 路线 A** / **L6**）
 *
 * ## 它证明什么
 *
 * 此前固件装载走 SoC 顶层的 `prog_*` 端口 ⇒ ⇒ **只接 I-TCM**
 * ⇒ ⇒ 有**初始化 `.data`** 的程序**装不进去**（D-TCM 永远是零）。
 *
 * ⇒ ⇒ 本文件把固件拆成**两段镜像**：
 *   · `kDemoText`  = **I-TCM** 的初值（指令）
 *   · `kDemoData`  = **D-TCM** 的初值（`.data`）
 * ⇒ ⇒ 两者都通过**构造参数**灌进 `ch_mem::init_data`
 * ⇒ ⇒ **产物里自动出现 `initial begin mem[i] = … end`**（`print_mem_init`）
 * ⇒ ⇒ **TB 零装载动作** · **零端口变更**。
 *
 * ## 程序（**手推** · 4 条指令）
 *
 * ```
 * 0: lw   x1, 0x100(x0)   # x1 = [0x100]  ← **来自 `.data` 初值**（0x5A）
 * 4: addi x2, x1, 1       # x2 = x1 + 1 = 0x5B
 * 8: sw   x2, 0x104(x0)   # [0x104] = 0x5B
 * 12: jal  x0, 0          # 自环（停机）
 * ```
 *
 * ⇒ ⇒ **判据（L6：走真实读通路读回）**：
 *   · `x1 == 0x5A` ⇒ ⇒ **证明 D-TCM 的初值真的落到了 0x100**（不是「loader 说成功」）
 *   · `x2 == 0x5B` ⇒ ⇒ 证明 ALU 拿到的是**真实值**
 *   ⇒ ⇒ ⚠️ 若 `.data` 没进 D-TCM ⇒ ⇒ `x1 = 0`（dmem 零初始化）⇒ ⇒ **两条断言同时红**。
 *
 * ⚠️ **地址口径**：`.data` 的 0x100 是 **D-TCM 区域内的偏移** ⇒ ⇒ 真地址 = `kDtcm.base + 0x100`
 * ⇒ ⇒ 但流水线把 `data_addr` 的**低 `kTcmAddrWidth+2` 位**送进 TCM
 * ⇒ ⇒ 所以固件里写 **0x100**（区域内偏移）即可 ⇒ ⇒ 与 `kDtcm.base` 的拼接由 SoC 的
 * `bits<kTcmAddrWidth+1, 2>` 完成（`kDtcm.base` 的低位恒 0 ⇒ ⇒ 等价）。
 */

#pragma once

#include "cpu/riscv/rv32i_asm_encode.h"

#include <cstdint>
#include <vector>

namespace rv32i_fw_demo {

/// `.data` 在 **D-TCM 内**的字索引（0x100 字节 ÷ 4）
inline constexpr unsigned kDataWordIndex = 0x100u / 4u;
/// `.data` 的值（**手推**：`.word 0x5A`）
inline constexpr uint32_t kDataValue = 0x5Au;

/// **I-TCM 初值**（指令镜像 · 4 个字）
inline std::vector<uint32_t> text() {
    using namespace riscv;
    return {
        LW(1, 0, 0x100),    // 0  x1 = [0x100]（**来自 .data**）
        ADDI(2, 1, 1),      // 4  x2 = x1 + 1
        SW(2, 0, 0x104),    // 8  [0x104] = x2
        JAL(0, 0),          // 12 自环
    };
}

/// **D-TCM 初值**（`.data` 镜像 ⇒ ⇒ 只覆盖到 `kDataWordIndex` 即可；其余由产物零填充）
inline std::vector<uint32_t> data() {
    std::vector<uint32_t> v(kDataWordIndex + 1, 0u);
    v[kDataWordIndex] = kDataValue;
    return v;
}

/// 期望的写回（**手推** · L6 的判据值）
inline constexpr uint64_t kExpectX1 = 0x5Au;   // lw  x1,[0x100]
inline constexpr uint64_t kExpectX2 = 0x5Bu;   // addi x2,x1,1

}  // namespace rv32i_fw_demo
