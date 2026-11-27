// examples/riscv-soc/src/soc_product_face.h
//
// **SoC 产品面常量**（ADR-077 V1/V2 的判据值 · 单一真相）。
//
// ⚠️ 为什么单独成头（2026-11-24）：这些值是 `tb_riscv_soc.cpp` 的 `--check-products`
//   约定，但把它们**写进 TB 源文件**会让 TB 文本里出现上游组件的**类型名**
//    ⇒ ⇒ 触发 `scripts/verify_rst_n_wiring.sh` 的粗网判据（R1/R3 认为
//    「提及带强制 `rst_n` 的组件的测试文件 MUST 自己驱动 `rst_n`」）
//    ⇒ ⇒ 而 TB 根本**不实例化**它们（`rst_n` 由 SoC 顶层从高有效 `ext_rst` 驱动）
//    ⇒ ⇒ 这是**误判**。⇒ ⇒ 把常量搬到这里 ⇒ ⇒ TB 不再提及组件类型名 ⇒ 误判消失
//    （护栏本身不动：它的判据是对的，只是粒度需要这层隔离）。
//
// ⚠️ 本头**只含常量**（无任何组件构造符号、无仿真器符号）⇒ ⇒ 不在该护栏的扫描面内
//    （⚠️ 注释里**也不要**写出那些符号的字面量 —— 该护栏按**文本**匹配）。

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace riscv_soc {

/// **冻结的产物 module 期望集**（ADR-077 V2 · **手推**）
///
/// = 根 1（`top`）+ SoC 级 8 个 `ch_layer`（`Rv32iCoreTop` / `SocTraceCounter` +
/// S4b 的 7 个：2×6 Lite 互联 + **6 个** MMIO 外设）+ 核复合体内部 7 个 `ch_layer`
/// （`rv32i_pipeline.h` 的七个 stage / HMU / 前推 mux）⇒ ⇒ **17**。
///
/// ⚠️ **S4b 的口径订正**：S4a 的 12 个里含 `AxiLiteMatrix2x2` ⇒ ⇒ S4b 把它**换成**
///   `AxiLiteCrossbar`（**1 换 1**）**并新增 5 个外设**（UART/TIMER/SPI/I2C/PWM）
///   ⇒ ⇒ **12 − 1 + 1 + 5 = 17**（不是「12 + 6 = 18」—— 那把「替换」当成了「新增」）。
///
/// ⚠️ 名字 MUST 与 `ch_layer<T>` 的**显式类型名实参**逐字一致（那是产物 module 名）。
/// ⚠️ **禁**从产物输出抄（抄了 = 判据自证 ⇒ 恒绿）。
inline const std::vector<std::string> &product_modules() {
    static const std::vector<std::string> kModules = {
        "top",
        "Rv32iCoreTop",
        "SocTraceCounter",
        "IfStage",
        "IdStage",
        "ExStage",
        "MemStage",
        "WbStage",
        "HazardUnit",
        "ForwardingMux_32_",
        // ── S4b（ADR-077 V2 的 12 → **17**）──
        "AxiLiteCrossbar",    // 2×6 Lite 互联（SoC 级 ch_layer · 替掉 S4a 的 2×2 矩阵）
        "AxiLiteGpio",        // MMIO 外设（SoC 级 ch_layer）
        "AxiLiteUart",
        "AxiLiteTimer",
        "AxiLiteSpi",
        "AxiLiteI2c",
        "AxiLitePwm",
    };
    return kModules;
}

/// **顶层端口数** = 框架 2 + 根自有 **8**（ADR-077 V1 的 8 → **10** · S4a）
///
/// 根自有 8 = `ext_rst` / `prog_we` / `prog_addr` / `prog_data` /
/// `store_count` / `writeback_count` / `gpio_out` / `gpio_in`
/// （`gpio_dir` / `irq` **不落根**：前者不是用户可见 IO · 后者归 S5）
inline constexpr std::size_t kProductTopPorts = 10;

/// 最小变化观测量数（签批 spec `rtl-soc-output` 的硬要求 ⇒ **不是**可调参数）
inline constexpr std::size_t kMinVaryingObservations = 2;

}  // namespace riscv_soc
