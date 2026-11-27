// examples/riscv-full-soc/src/rv32i_full_soc_top.h
//
// **full 产品线的 SoC 级顶层**（ADR-078 · change `rv32i-full-product-line-2026-11` · S4c-0）
//
// ⚠️ **本文件是「空壳」**（S4c-0 范围 · **零功能**）：
//   · **无核**（取指接口 fork + I-L1 = S4c-1）
//   · **无外设**（DMA boot = S4c-2）
//   · **无互连**（full 互连 + L2/DRAM = S4c-3）
//   ⇒ ⇒ 本文件**只**提供：① 顶层形态（两级顶层的**占位**）② 端口面（具名 · ADR-075 E1）
//   ③ 一个可被 ctest / 运行护栏执行的**验证载体**。
//   ⇒ 分批表（唯一权威）= ADR-078 的 D8。
//
// ⚠️ **为什么先有空壳**（ADR-078 的 D8 / S4c-0）：新线 MUST **立即进入**所有护栏的扫描面
//   ⇒ ⇒ 「漏登记」当场红（而不是静默不覆盖）⇒ ⇒ 空壳就是那把「立刻可见」的探针。
//
// ⚠️ **MUST NOT 复制任何微架构实现**（ADR-077 D1）⇒ 本文件零逻辑。

#pragma once

#include "ch.hpp"
#include "component.h"

#include <cstdint>
#include <string>

namespace riscv_full_soc {

using namespace ch::core;

/// **产品线序号**（本线的稳定标识 · 根端口 `line_id` 的值）
///
/// ⚠️ `riscv-mini` 线 = 1 · `riscv-full` 线 = 2 ⇒ ⇒ 序号是**产品线身份**的一部分 ⇒ ⇒
/// 未来新增产品线时 MUST 取**未用过的**序号（不得复用）。
inline constexpr uint32_t kFullLineId = 2u;

/// **顶层端口数** = 框架 2（`default_clock` / `default_reset`）+ 根自有 **2** = **4**
///
/// ⚠️ 框架 2 **不在** `__io` 里（由框架提供）⇒ ⇒ 根端口面 = 框架 2 + 本类的 `__io`。
inline constexpr std::size_t kFullSocTopPorts = 4;

/// **full 产品线的 SoC 级顶层**（两级顶层的 SoC 层 · ADR-077 D1 形态）
///
/// ⚠️ **S4c-0 空壳**：只有 2 根具名输出，**无任何输入** ⇒ ⇒ 顶层**恒定**（无复位态差异）
/// ⇒ ⇒ 它的存在只是「让新线进入护栏扫描面」，不是功能载体。
class Rv32iFullSocTop : public ch::Component {
public:
    __io(
        /// **产品线标识**（常量 · 见 `kFullLineId`）
        ch_out<ch_uint<32>> line_id{"line_id"};
        /// **空壳就绪**（恒 true · 表示「本线已进入全部护栏的扫描面」）
        ch_out<ch_bool> line_ready{"line_ready"};
    )

    explicit Rv32iFullSocTop(ch::Component *parent = nullptr,
                             const std::string &name = "Rv32iFullSocTop")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // ⚠️ **输出 MUST 用 `<<=` 驱动**（`io().x = v` 是反模式：编译通过但**什么都不做**）
        io().line_id <<= ch_uint<32>(kFullLineId);
        io().line_ready <<= ch_bool(true);
    }
};

} // namespace riscv_full_soc
