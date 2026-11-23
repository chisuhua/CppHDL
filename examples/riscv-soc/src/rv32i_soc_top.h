// examples/riscv-soc/src/rv32i_soc_top.h
//
// **SoC 级顶层**（ADR-077 D1 · change `riscv-soc-skeleton-2026-11`）。
//
// 两级顶层分工（ADR-077 D1）：
//   · **IP 级**（核复合体）= `examples/riscv-mini/src/rv32i_pipeline_top.h`
//     （pipeline + I-TCM + D-TCM · 34 端口 · 一字不改）
//   · **SoC 级**（本文件）= 把核复合体与观测 IP 装配成 SoC 根
//
// ⚠️ **本文件 MUST NOT 复制任何微架构实现**（ADR-077 D1）⇒ 只做实例化 + 连线。
//
// ⚠️ **产品端口面**（ADR-072 D1）= 框架 2（`default_clock` / `default_reset`）
//   + 根自有 6 ⇒ ⇒ **8** 根（ADR-077 V1）。
//   后代的 `dbg_*` / `obs_*` **不是**顶层端口 ⇒ ⇒ SoC 级观测面只能靠**根自有**语义。
//
// ⚠️ **根自有输出 = 两个计数**（不是后代信号直通）：`SocTraceCounter` 自己累计后
//   由根本身输出 ⇒ 符合 ADR-077 D1 的「根自有语义」要求。

#pragma once

#include "ch.hpp"
#include "component.h"
#include "soc_trace_counter.h"

// 核复合体住在 `examples/riscv-mini/src/`（ADR-046 / ADR-077 D1）
// ⇒ CMake 侧以 target_include_directories 注入该目录（本文件不带 examples/ 前缀）。
#include "rv32i_pipeline_top.h"

#include <cstdint>
#include <string>
#include <vector>

namespace riscv_soc {

using namespace ch::core;

/// 根的**实例名**（≠ 产物 module 名 `top` —— 后者由发射入口配置决定）。
inline constexpr const char *kSocRootName = "top";

class Rv32iSocTop : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      ext_rst{"ext_rst"};        // 核自有复位（高有效）
        ch_in<ch_bool>      prog_we{"prog_we"};        // 固件装载后门（ADR-077 D3：长期保留）
        ch_in<ch_uint<32>>  prog_addr{"prog_addr"};    // 字地址
        ch_in<ch_uint<32>>  prog_data{"prog_data"};
        ch_out<ch_uint<16>> store_count{"store_count"};
        ch_out<ch_uint<16>> writeback_count{"writeback_count"};
    )

    /// ⚠️ **固件初值透传**（`firmware-load-chain` L2 路线 A · **方案 a**）
    ///
    /// ⇒ ⇒ 走**构造参数** ⇒ ⇒ 核复合体的 `ch_mem::init_data`
    /// ⇒ ⇒ **零端口变更**（`__io` 一字未动 ⇒ ⇒ 根端口表与基线产物逐字相同）
    /// ⇒ ⇒ 空（默认）⇒ ⇒ 与基线**逐字节等价**
    ///
    /// ⚠️ 此前 SoC 级**整级没有**路线 A ⇒ ⇒ 产品单元只能跑纯 `.text` 程序
    /// ⇒ ⇒ 有初始化 `.data` 的固件**装不进去** ⇒ ⇒ 本参数补掉这一点。
    explicit Rv32iSocTop(ch::Component *parent = nullptr,
                         const std::string &name = kSocRootName,
                         const std::vector<uint32_t> &itcm_init = {},
                         const std::vector<uint32_t> &dtcm_init = {})
        : ch::Component(parent, name),
          itcm_init_(itcm_init),
          dtcm_init_(dtcm_init) {}

    /// 初值字镜像（**空 = 无初值** ⇒ ⇒ 零行为变化）
    std::vector<uint32_t> itcm_init_;
    std::vector<uint32_t> dtcm_init_;

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // ── 两个成层 IP（档 ③：实例处给模块类型名 —— 两个类都**没有**
        //    `CH_HIERARCHY_LAYER` ⇒ 两参数形态是唯一可用重载）──
        // ⚠️ **初值透传**：`ch_layer<T, Args…>` 的 `Args…` MUST **显式**给出
        //    （类模板参数**不**对构造函数实参做推导）⇒ ⇒ 只写 `ch_layer<T>`
        //    = 零参 ⇒ ⇒ 初值传不进去。
        ch::ch_layer<Rv32iPipelineTop, const std::vector<uint32_t> &,
                     const std::vector<uint32_t> &>
            core{"core", "Rv32iCoreComplex", itcm_init_, dtcm_init_};
        ch::ch_layer<SocTraceCounter> trace{"trace", "SocTraceCounter"};

        // ── 核的 4 个输入全由根驱动（ADR-073 D2：零未驱动后代输入）──
        core.io().ext_rst <<= io().ext_rst;
        core.io().prog_we <<= io().prog_we;
        core.io().prog_addr <<= io().prog_addr;
        core.io().prog_data <<= io().prog_data;

        // ── 两条**兄弟边**（child → child · ADR-070 D4 条件③）──
        trace.io().store_event <<= core.io().obs_data_write_en;
        trace.io().retire_event <<= core.io().dbg_wb_write_en;

        // ── 根 only 输出（ADR-066 ① · 根自有语义）──
        io().store_count <<= trace.io().store_count;
        io().writeback_count <<= trace.io().writeback_count;
    }
};

} // namespace riscv_soc
