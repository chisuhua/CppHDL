/**
 * @file rv32i_mem_map.h
 * @brief RV32I 的**地址映射表**（**单一真相** · L1）
 *
 * ## 为什么有这张表（`firmware-load-chain` **L1**）
 *
 * 此前仓内有**三处**互不相干的事实描述同一张内存图：
 *
 * | 位置 | 内容 | 问题 |
 * |---|---|---|
 * | `include/cpu/riscv/address_decoder.h` | `ADDR_ITCM_BASE/SIZE`、`ADDR_DTCM_BASE/SIZE` | ⚠️ **声明 64 KB** |
 * | `include/cpu/pipeline/stages/if_stage.h` | `pc_reg` 的复位值 `0x80000000` | ⚠️ **第二个字面量** |
 * | `examples/riscv-mini/src/rv32i_pipeline_top.h` | `kTcmAddrWidth = 12` | ⚠️ **容量真相在 example 侧** |
 *
 * ⇒ ⇒ 而**硬件实际容量** = `1 << (kTcmAddrWidth + 2)` = **16 KB**
 * ⇒ ⇒ 即「**声明 64 KB · 实现 16 KB**」⇒ ⇒ 地址落在
 * `[0x8000_4000, 0x8001_0000)` 的访问会被**静默截断**（PC 32 位 ⇒
 * TCM 索引 12 位 ⇒ 高位**丢**）⇒ ⇒ **零报错**。
 *
 * ⇒ ⇒ 本表把这三件事收敛成**一份**推导：
 * **容量由索引位宽派生** ⇒ ⇒ **禁止手写字面量** ⇒ ⇒ 声明与实现**不可能**再不一致。
 *
 * ## 派生关系（**唯一**真相链）
 *
 * ```
 * kTcmAddrWidth (12)          ← 字索引位宽（**唯一**字面量）
 *   └─ kTcmBytes = 1<<(kTcmAddrWidth+2) = 16 KB   ← 字节容量（**派生**）
 *        ├─ kItcm = { base = 0x8000_0000, size = kTcmBytes }
 *        └─ kDtcm = { base = 0x8001_0000, size = kTcmBytes }
 *             └─ kPcReset = kItcm.base            ← PC 复位值（**派生**）
 * ```
 *
 * ⚠️ **`kDtcm.base == 0x8001_0000`**（沿用 `address_decoder.h` 的既有取值 ·
 * **用户 2026-11-10 裁决②**：**不**改成 `0x8000_4000`）。
 *
 * ⚠️ **对齐要求是 16 KB 而不是 4 KB** ⇒ ⇒ 「截断等价的最小判据」 MUST 按 **16 KB** 取
 * （`0x8001_0000 % 0x4000 == 0` ✓ · `0x8000_5000 % 0x4000 == 4096` ✗ ⇒ ⇒
 * **4 KB 对齐的判据无判别力**）。
 */

#pragma once

#include <cstdint>

namespace riscv {

/// ⚠️ **I-TCM / D-TCM 的**字索引**位宽**（**全仓唯一的容量真相**）
/// ⇒ ⇒ 字节容量 = `1 << (kTcmAddrWidth + 2)` = **16 KB**（不是 4 KB！）
/// ⚠️ **改这里 = 改全平台的内存布局** ⇒ ⇒ MUST 同步 `address_decoder.h` / SoC 顶层 / 判据。
inline constexpr unsigned kTcmAddrWidth = 12;

/// **TCM 的字节容量**（**派生** ⇒ ⇒ 禁手写 16 KB 之类的字面量）
inline constexpr uint64_t kTcmBytes = 1ull << (kTcmAddrWidth + 2);

/// 一个内存区域（**字节**地址空间）
struct MemRegion {
    uint64_t base;   ///< 起始**字节**地址
    uint64_t size;   ///< 字节容量
};

/// **I-TCM 区域**（指令）
inline constexpr MemRegion kItcm{0x80000000ull, kTcmBytes};

/// **D-TCM 区域**（数据）
inline constexpr MemRegion kDtcm{0x80010000ull, kTcmBytes};

/// **PC 复位值**（**派生** ⇒ ⇒ 禁第二个字面量）
inline constexpr uint32_t kPcReset = static_cast<uint32_t>(kItcm.base);

/// 字节地址是否落在区域内（**闭区间** `[base, base+size)`）
constexpr bool in_region(const MemRegion &r, uint64_t byte_addr) {
    return byte_addr >= r.base && byte_addr < r.base + r.size;
}

/// 字节地址能否**无截断**地映射到 `r` 的**字索引**
///
/// ⚠️ **MUST 用区域相对偏移**（`byte_addr - r.base`），**不是**绝对地址 ——
/// 绝对地址的高位（`0x8000_0000`）恒大于 2^`kTcmAddrWidth` ⇒ ⇒ 用绝对地址判定**恒假**。
/// ⚠️ 这是「**PC 位宽 vs TCM 索引位宽**」的**判据面**：索引只有
/// `kTcmAddrWidth` 位 ⇒ ⇒ 超出即**高位被丢** ⇒ ⇒ MUST 诊断，**禁**静默截断。
constexpr bool word_index_fits(const MemRegion &r, uint64_t byte_addr) {
    return in_region(r, byte_addr) &&
           ((byte_addr - r.base) >> 2) < (1ull << kTcmAddrWidth);
}

}  // namespace riscv
