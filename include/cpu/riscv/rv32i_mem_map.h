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
 * | `examples/riscv-mini/src/rv32i_core_top.h` | `kTcmAddrWidth = 12` | ⚠️ **容量真相在 example 侧** |
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
/// ⚠️ **改这里 = 改全平台的内存布局** ⇒ ⇒ MUST 同步 `address_decoder.h` / 核 IP 顶层 / 判据。
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

/// ======================================================================
/// **MMIO 段**（2026-11-24 · change `rv32i-soc-mmio-channel-2026-11` · G-6）
/// ======================================================================
///
/// **归属裁决（G-6）** = **库层全表** + SoC 侧只按 `Map::windows[]` **装配**
/// ⇒ ⇒ 本表是 MMIO 地址的**唯一真相** ⇒ ⇒ SoC 侧**禁**再手写基地址。
///
/// ```
/// kMmioBase (0x8010_0000)
///   ├─ kClint  (+0x0000)   ┐
///   ├─ kUart   (+0x1000)   │
///   ├─ kGpio   (+0x2000)   │ 每段 kMmioSegmentBytes（4 KB）
///   ├─ kSpi    (+0x3000)   │ ⇒ ⇒ **顺延** ⇒ ⇒ **禁手写 0x8000 之类总量
///   ├─ kI2c    (+0x4000)   │
///   ├─ kPwm    (+0x5000)   │
///   ├─ kDma    (+0x6000)   │
///   └─ kTimer  (+0x7000)   ┘  ← 2026-11-27 追加（D-1 · **既有 7 段零移动**）
/// ```

/// **单个 MMIO 段的字节容量**（4 KB）
inline constexpr uint64_t kMmioSegmentBytes = 0x1000ull;

/// **MMIO 窗口的起始字节地址**（**唯一**字面量 ⇒ ⇒ 其余全部派生）
inline constexpr uint64_t kMmioBase = 0x80100000ull;

/// **8 个 MMIO 段**（**顺延派生** ⇒ ⇒ 禁手写各自的基地址）
inline constexpr MemRegion kClint{kMmioBase + 0 * kMmioSegmentBytes, kMmioSegmentBytes};
inline constexpr MemRegion kUart {kMmioBase + 1 * kMmioSegmentBytes, kMmioSegmentBytes};
inline constexpr MemRegion kGpio {kMmioBase + 2 * kMmioSegmentBytes, kMmioSegmentBytes};
inline constexpr MemRegion kSpi  {kMmioBase + 3 * kMmioSegmentBytes, kMmioSegmentBytes};
inline constexpr MemRegion kI2c  {kMmioBase + 4 * kMmioSegmentBytes, kMmioSegmentBytes};
inline constexpr MemRegion kPwm  {kMmioBase + 5 * kMmioSegmentBytes, kMmioSegmentBytes};
inline constexpr MemRegion kDma  {kMmioBase + 6 * kMmioSegmentBytes, kMmioSegmentBytes};
/// **TIMER 段**（2026-11-27 追加 · D-1 ⇒ ⇒ **追加在末尾** ⇒ ⇒ **既有 7 段零移动**）
inline constexpr MemRegion kTimer{kMmioBase + 7 * kMmioSegmentBytes, kMmioSegmentBytes};

/// **MMIO 窗口的总字节数**（**派生** ⇒ ⇒ 禁手写 `0x8000`）
inline constexpr uint64_t kMmioBytes = 8ull * kMmioSegmentBytes;

/// 字节地址是否落在 **MMIO 窗口**内（**闭区间** `[kMmioBase, kMmioBase+kMmioBytes)`）
///
/// ⚠️ **这是「核内 pre-cache 分流」的**唯一**判据**（G-9.3）⇒ ⇒ 方向 MUST 是
/// 「地址 ∈ **MMIO 段**」（**不是**「地址 ∈ D-TCM 窗口」）⇒ ⇒ 取反形式会让
/// **字节地址 0**（核级程序 `sw x3, 0(x0)`）落进 MMIO 旁路 ⇒ ⇒ 核级 TB 永久挂死
/// ⇒ ⇒ **判据方向**已在 G-9 决策材料里 dated 订正。
constexpr bool in_mmio(uint64_t byte_addr) {
    return byte_addr >= kMmioBase && byte_addr < kMmioBase + kMmioBytes;
}

/// **MMIO 段的连续性 + 不重叠**（**编译期** ⇒ ⇒ 改表即编译失败 ⇒ ⇒ 零漂移）
static_assert(kUart.base == kClint.base + kClint.size, "MMIO 段 MUST 连续顺延");
static_assert(kGpio.base == kUart.base + kUart.size, "MMIO 段 MUST 连续顺延");
static_assert(kSpi.base == kGpio.base + kGpio.size, "MMIO 段 MUST 连续顺延");
static_assert(kI2c.base == kSpi.base + kSpi.size, "MMIO 段 MUST 连续顺延");
static_assert(kPwm.base == kI2c.base + kI2c.size, "MMIO 段 MUST 连续顺延");
static_assert(kDma.base == kPwm.base + kPwm.size, "MMIO 段 MUST 连续顺延");
static_assert(kTimer.base == kDma.base + kDma.size, "MMIO 段 MUST 连续顺延");
static_assert(kTimer.base + kTimer.size == kMmioBase + kMmioBytes, "MMIO 窗口 MUST 覆盖全部 8 段");
static_assert(kMmioBase >= kDtcm.base + kDtcm.size, "MMIO 窗口 MUST NOT 与 D-TCM 重叠");
static_assert(kMmioBase % kMmioSegmentBytes == 0, "MMIO 基址 MUST 按段大小对齐");
static_assert(kMmioBytes % kMmioSegmentBytes == 0, "MMIO 总长 MUST 是段大小的整数倍");

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
