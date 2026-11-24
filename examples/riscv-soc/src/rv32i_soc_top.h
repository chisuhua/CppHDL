// examples/riscv-soc/src/rv32i_soc_top.h
//
// **SoC 级顶层**（ADR-077 D1 · change `riscv-soc-skeleton-2026-11`）。
//
// 两级顶层分工（ADR-077 D1）：
//   · **IP 级**（核复合体）= `examples/riscv-mini/src/rv32i_core_top.h`
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
#include "cpu/riscv/rv32i_mem_map.h"      // MMIO 段基址（单一真相 · G-6）
#include "axi4/axi4_lite_matrix.h"        // 2×2 Lite 互联（s0 = GPIO）
#include "axi4/peripherals/axi_gpio.h"    // 首个 MMIO 外设（S4a 只接它）

// 核复合体住在 `examples/riscv-mini/src/`（ADR-046 / ADR-077 D1）
// ⇒ CMake 侧以 target_include_directories 注入该目录（本文件不带 examples/ 前缀）。
#include "rv32i_core_top.h"

#include <cstdint>
#include <string>
#include <vector>

namespace riscv_soc {

using namespace ch::core;

/// **Lite 互联的地址映射**（S4a · G-6：基址**派生**自库层地址表 ⇒ ⇒ 禁手写字面量）
///
/// · `windows[0]` = GPIO 段（`kGpio`）⇒ ⇒ **唯一接上外设的从口**（s0）
/// · `windows[1]` = **最高 4 KB**（`0xFFFF_F000`）⇒ ⇒ **核永不发该地址**
///   ⇒ ⇒ s1 永不被选中 ⇒ ⇒ s1 的 8 根输入 tie 0 是**可证明的**死接线
///   ⇒ ⇒ ⚠️ **禁**用低位窗口（如 `{0, 4 KB}`）—— 字节地址 0 的访问会被路由到
///   **未接的 s1** ⇒ ⇒ 事务永不完成 ⇒ ⇒ 程序**挂死**（核级 `sw x3, 0(x0)` 就是这种访问）。
struct SocLiteMap {
    static constexpr axi4::AxiAddrWindow windows[2] = {
        {riscv::kGpio.base, riscv::kGpio.size},
        {0xFFFF'F000ull, 0x1000ull},
    };
};

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
        // ── S4a：首个 MMIO 外设（GPIO）的**根端口**（ADR-077 D1「根自有语义」）──
        ch_out<ch_uint<8>>  gpio_out{"gpio_out"};
        ch_in<ch_uint<8>>   gpio_in{"gpio_in"};
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
        ch::ch_layer<Rv32iCoreTop, const std::vector<uint32_t> &,
                     const std::vector<uint32_t> &>
            core{"core", "Rv32iCoreTop", itcm_init_, dtcm_init_};
        ch::ch_layer<SocTraceCounter> trace{"trace", "SocTraceCounter"};
        // ── S4a（G-9）：MMIO 通道的 SoC 侧 = 2×2 Lite 互联 + 首个外设（GPIO）──
        // ⚠️ 成层 IP ⇒ 实例处给**模块类型名**（层次化产物里可辨识）
        ch::ch_layer<axi4::AxiLiteMatrix2x2<32, 32, SocLiteMap>>
            lite{"lite", "AxiLiteMatrix2x2"};
        ch::ch_layer<axi4::AxiLiteGpio<8>> gpio{"gpio", "AxiLiteGpio"};

        // 复位（低有效）⇒ ⇒ 由高有效的 `ext_rst` 取反
        lite.io().rst_n <<= !io().ext_rst;
        gpio.io().rst_n <<= !io().ext_rst;

        // ── 核的 AXI4-Lite 主口 → 互联的 m0 面（**19 根直连，无适配层**）──
        lite.io().m0_awaddr  <<= core.io().axil_awaddr;
        lite.io().m0_awprot  <<= core.io().axil_awprot;
        lite.io().m0_awvalid <<= core.io().axil_awvalid;
        core.io().axil_awready <<= lite.io().m0_awready;
        lite.io().m0_wdata   <<= core.io().axil_wdata;
        lite.io().m0_wstrb   <<= core.io().axil_wstrb;
        lite.io().m0_wvalid  <<= core.io().axil_wvalid;
        core.io().axil_wready <<= lite.io().m0_wready;
        core.io().axil_bresp  <<= lite.io().m0_bresp;
        core.io().axil_bvalid <<= lite.io().m0_bvalid;
        lite.io().m0_bready  <<= core.io().axil_bready;
        lite.io().m0_araddr  <<= core.io().axil_araddr;
        lite.io().m0_arprot  <<= core.io().axil_arprot;
        lite.io().m0_arvalid <<= core.io().axil_arvalid;
        core.io().axil_arready <<= lite.io().m0_arready;
        core.io().axil_rdata  <<= lite.io().m0_rdata;
        core.io().axil_rresp  <<= lite.io().m0_rresp;
        core.io().axil_rvalid <<= lite.io().m0_rvalid;
        lite.io().m0_rready  <<= core.io().axil_rready;

        // ── 互联的 s0 面 → GPIO（**唯一的从设备**）──
        gpio.io().awaddr  <<= lite.io().s0_awaddr;
        gpio.io().awprot  <<= lite.io().s0_awprot;
        gpio.io().awvalid <<= lite.io().s0_awvalid;
        lite.io().s0_awready <<= gpio.io().awready;
        gpio.io().wdata   <<= lite.io().s0_wdata;
        gpio.io().wstrb   <<= lite.io().s0_wstrb;
        gpio.io().wvalid  <<= lite.io().s0_wvalid;
        lite.io().s0_wready <<= gpio.io().wready;
        lite.io().s0_bresp  <<= gpio.io().bresp;
        lite.io().s0_bvalid <<= gpio.io().bvalid;
        gpio.io().bready  <<= lite.io().s0_bready;
        gpio.io().araddr  <<= lite.io().s0_araddr;
        gpio.io().arprot  <<= lite.io().s0_arprot;
        gpio.io().arvalid <<= lite.io().s0_arvalid;
        lite.io().s0_arready <<= gpio.io().arready;
        lite.io().s0_rdata   <<= gpio.io().rdata;
        lite.io().s0_rresp   <<= gpio.io().rresp;
        lite.io().s0_rvalid  <<= gpio.io().rvalid;
        gpio.io().rready  <<= lite.io().s0_rready;

        // ── GPIO 的物理面 → 根端口（`gpio_dir` / `irq` 本批**不引出** · G-5）──
        gpio.io().gpio_in <<= io().gpio_in;
        io().gpio_out <<= gpio.io().gpio_out;

        // ── **零未驱动**：m1 面（11 根）与 s1 面（8 根）**显式 tie 0** ──
        //   m1 永不发请求（根只有核一个 master）· s1 永不被选中
        //   （`windows[1]` = 最高 4 KB ⇒ 核永不发该地址）
        lite.io().m1_awaddr  <<= ch_uint<32>(0_d);
        lite.io().m1_awprot  <<= ch_uint<2>(0_d);
        lite.io().m1_awvalid <<= ch_bool(false);
        lite.io().m1_wdata   <<= ch_uint<32>(0_d);
        lite.io().m1_wstrb   <<= ch_uint<4>(0_d);
        lite.io().m1_wvalid  <<= ch_bool(false);
        lite.io().m1_bready  <<= ch_bool(false);
        lite.io().m1_araddr  <<= ch_uint<32>(0_d);
        lite.io().m1_arprot  <<= ch_uint<2>(0_d);
        lite.io().m1_arvalid <<= ch_bool(false);
        lite.io().m1_rready  <<= ch_bool(false);

        lite.io().s1_awready <<= ch_bool(false);
        lite.io().s1_wready  <<= ch_bool(false);
        lite.io().s1_bresp   <<= ch_uint<2>(0_d);
        lite.io().s1_bvalid  <<= ch_bool(false);
        lite.io().s1_arready <<= ch_bool(false);
        lite.io().s1_rdata   <<= ch_uint<32>(0_d);
        lite.io().s1_rresp   <<= ch_uint<2>(0_d);
        lite.io().s1_rvalid  <<= ch_bool(false);

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
