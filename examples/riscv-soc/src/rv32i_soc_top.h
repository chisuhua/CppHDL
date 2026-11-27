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
#include "axi4/axi_lite_crossbar.h"      // 2×6 Lite 互联（S4b：chlib stream 数据面）
#include "axi4/peripherals/axi_gpio.h"    // MMIO 外设（6 个 · S4b 全接）
#include "axi4/peripherals/axi_uart.h"
#include "axi4/peripherals/axi_timer.h"
#include "axi4/peripherals/axi_spi.h"
#include "axi4/peripherals/axi_i2c.h"
#include "axi4/peripherals/axi_pwm.h"

// 核复合体住在 `examples/riscv-mini/src/`（ADR-046 / ADR-077 D1）
// ⇒ CMake 侧以 target_include_directories 注入该目录（本文件不带 examples/ 前缀）。
#include "rv32i_core_top.h"

#include <cstdint>
#include <string>
#include <vector>

namespace riscv_soc {

using namespace ch::core;

/// **Lite 互联的地址映射**（S4b · G-6/D-1：基址与容量**全部派生**自库层地址表）
///
/// ⚠️ **8 个窗口 = 库层表的 8 段**（`kClint`/`kUart`/`kGpio`/`kSpi`/`kI2c`/`kPwm`/
///   `kDma`/`kTimer`）⇒ ⇒ **禁手写基地址字面量**（禁手写 ⇒ 由下面的 `static_assert` 钉住）。
///
/// · `target` = **从口索引**（0..5）或 `axi4::kErr`（⇒ 互连层自应答 DECERR）
/// · 从口编号（G-6 的 SoC 装配口径）：
///   `s0` = GPIO · `s1` = UART · `s2` = TIMER · `s3` = SPI · `s4` = I2C · `s5` = PWM
/// · **CLINT 段**（设备在 S5）与 **DMA 段**（设备在 S4c）⇒ ⇒ 映射到 `kErr`
///   ⇒ ⇒ **不留死从口**（不留「有窗口无设备」的洞 ⇒ 访问得到 DECERR 而非挂死）
struct SocCrossbarMap {
    static constexpr axi4::AxiLiteMapWindow windows[8] = {
        {riscv::kClint.base, riscv::kClint.size, axi4::kErr},
        {riscv::kUart.base, riscv::kUart.size, 1u},
        {riscv::kGpio.base, riscv::kGpio.size, 0u},
        {riscv::kSpi.base, riscv::kSpi.size, 3u},
        {riscv::kI2c.base, riscv::kI2c.size, 4u},
        {riscv::kPwm.base, riscv::kPwm.size, 5u},
        {riscv::kDma.base, riscv::kDma.size, axi4::kErr},
        {riscv::kTimer.base, riscv::kTimer.size, 2u},
    };
};

// ⚠️ **8 条 `static_assert` 逐段钉住「窗口 == 库层表」**（G-6 的**单一真相**护栏）
//   ⇒ ⇒ 若有人把上面的派生基址换成**手写字面量**（例如 `0x80102000`）⇒ ⇒ **编译失败**
//   ⇒ ⇒ 而不是「编译通过但地址路由错位」的静默漂移。
static_assert(SocCrossbarMap::windows[0].base == riscv::kClint.base &&
                  SocCrossbarMap::windows[0].size == riscv::kClint.size,
              "窗口 0 MUST 等于库层表 kClint");
static_assert(SocCrossbarMap::windows[1].base == riscv::kUart.base &&
                  SocCrossbarMap::windows[1].size == riscv::kUart.size,
              "窗口 1 MUST 等于库层表 kUart");
static_assert(SocCrossbarMap::windows[2].base == riscv::kGpio.base &&
                  SocCrossbarMap::windows[2].size == riscv::kGpio.size,
              "窗口 2 MUST 等于库层表 kGpio");
static_assert(SocCrossbarMap::windows[3].base == riscv::kSpi.base &&
                  SocCrossbarMap::windows[3].size == riscv::kSpi.size,
              "窗口 3 MUST 等于库层表 kSpi");
static_assert(SocCrossbarMap::windows[4].base == riscv::kI2c.base &&
                  SocCrossbarMap::windows[4].size == riscv::kI2c.size,
              "窗口 4 MUST 等于库层表 kI2c");
static_assert(SocCrossbarMap::windows[5].base == riscv::kPwm.base &&
                  SocCrossbarMap::windows[5].size == riscv::kPwm.size,
              "窗口 5 MUST 等于库层表 kPwm");
static_assert(SocCrossbarMap::windows[6].base == riscv::kDma.base &&
                  SocCrossbarMap::windows[6].size == riscv::kDma.size,
              "窗口 6 MUST 等于库层表 kDma");
static_assert(SocCrossbarMap::windows[7].base == riscv::kTimer.base &&
                  SocCrossbarMap::windows[7].size == riscv::kTimer.size,
              "窗口 7 MUST 等于库层表 kTimer");

// ⚠️ **target 映射的意图钉死**（编译期 ⇒ ⇒ 段序写错 / 从口错位必红）
static_assert(SocCrossbarMap::windows[0].target == axi4::kErr,
              "CLINT 段（设备在 S5）MUST 指向 kErr（互连自应答 DECERR）");
static_assert(SocCrossbarMap::windows[6].target == axi4::kErr,
              "DMA 段（设备在 S4c）MUST 指向 kErr（互连自应答 DECERR）");
static_assert(SocCrossbarMap::windows[2].target == 0u, "GPIO MUST 落 s0");
static_assert(SocCrossbarMap::windows[1].target == 1u, "UART MUST 落 s1");
static_assert(SocCrossbarMap::windows[7].target == 2u, "TIMER MUST 落 s2");
static_assert(SocCrossbarMap::windows[3].target == 3u, "SPI MUST 落 s3");
static_assert(SocCrossbarMap::windows[4].target == 4u, "I2C MUST 落 s4");
static_assert(SocCrossbarMap::windows[5].target == 5u, "PWM MUST 落 s5");

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
        // ── S4b（G-9）：MMIO 通道的 SoC 侧 = **2×6 Lite 互联** + **6 个外设**──
        // ⚠️ 成层 IP ⇒ 实例处给**模块类型名**（层次化产物里可辨识）
        // ⚠️ 互联的数据面 = `chlib` 的 stream 组合子（`stream_mux` / `stream_demux`）
        //    ⇒ ⇒ SoC 侧**只做装配**，不碰反压转发。
        ch::ch_layer<axi4::AxiLiteCrossbar<SocCrossbarMap>>
            lite{"lite", "AxiLiteCrossbar"};
        ch::ch_layer<axi4::AxiLiteGpio<8>> gpio{"gpio", "AxiLiteGpio"};
        ch::ch_layer<axi4::AxiLiteUart<>> uart{"uart", "AxiLiteUart"};
        ch::ch_layer<axi4::AxiLiteTimer<>> timer{"timer", "AxiLiteTimer"};
        ch::ch_layer<axi4::AxiLiteSpi<>> spi{"spi", "AxiLiteSpi"};
        ch::ch_layer<axi4::AxiLiteI2c<>> i2c{"i2c", "AxiLiteI2c"};
        ch::ch_layer<axi4::AxiLitePwm<>> pwm{"pwm", "AxiLitePwm"};

        // 复位（低有效）⇒ ⇒ 由高有效的 `ext_rst` 取反
        lite.io().rst_n <<= !io().ext_rst;
        gpio.io().rst_n <<= !io().ext_rst;
        uart.io().rst_n <<= !io().ext_rst;
        timer.io().rst_n <<= !io().ext_rst;
        spi.io().rst_n <<= !io().ext_rst;
        i2c.io().rst_n <<= !io().ext_rst;
        pwm.io().rst_n <<= !io().ext_rst;

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

        // ── s0 面 ↔ GPIO（**19 根** · 从口编号见 `SocCrossbarMap`）──
        // ⚠️ **方向 MUST 按 AXI4-Lite 的角色**（本批踩过一次 ⇒ 写进注释）：
        //   · **主 → 从**（互联驱动）：awaddr/awprot/awvalid · wdata/wstrb/wvalid ·
        //     **bready** · araddr/arprot/arvalid · **rready**
        //   · **从 → 主**（从设备驱动）：**awready** · **wready** · **bresp/bvalid** ·
        //     **arready** · **rdata/rresp/rvalid**
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
        // ── s1 面 ↔ UART（**19 根** · 从口编号见 `SocCrossbarMap`）──
        // ⚠️ **方向 MUST 按 AXI4-Lite 的角色**（本批踩过一次 ⇒ 写进注释）：
        //   · **主 → 从**（互联驱动）：awaddr/awprot/awvalid · wdata/wstrb/wvalid ·
        //     **bready** · araddr/arprot/arvalid · **rready**
        //   · **从 → 主**（从设备驱动）：**awready** · **wready** · **bresp/bvalid** ·
        //     **arready** · **rdata/rresp/rvalid**
        uart.io().awaddr  <<= lite.io().s1_awaddr;
        uart.io().awprot  <<= lite.io().s1_awprot;
        uart.io().awvalid <<= lite.io().s1_awvalid;
        lite.io().s1_awready <<= uart.io().awready;
        uart.io().wdata   <<= lite.io().s1_wdata;
        uart.io().wstrb   <<= lite.io().s1_wstrb;
        uart.io().wvalid  <<= lite.io().s1_wvalid;
        lite.io().s1_wready <<= uart.io().wready;
        lite.io().s1_bresp  <<= uart.io().bresp;
        lite.io().s1_bvalid <<= uart.io().bvalid;
        uart.io().bready  <<= lite.io().s1_bready;
        uart.io().araddr  <<= lite.io().s1_araddr;
        uart.io().arprot  <<= lite.io().s1_arprot;
        uart.io().arvalid <<= lite.io().s1_arvalid;
        lite.io().s1_arready <<= uart.io().arready;
        lite.io().s1_rdata   <<= uart.io().rdata;
        lite.io().s1_rresp   <<= uart.io().rresp;
        lite.io().s1_rvalid  <<= uart.io().rvalid;
        uart.io().rready  <<= lite.io().s1_rready;
        // ── s2 面 ↔ TIMER（**19 根** · 从口编号见 `SocCrossbarMap`）──
        // ⚠️ **方向 MUST 按 AXI4-Lite 的角色**（本批踩过一次 ⇒ 写进注释）：
        //   · **主 → 从**（互联驱动）：awaddr/awprot/awvalid · wdata/wstrb/wvalid ·
        //     **bready** · araddr/arprot/arvalid · **rready**
        //   · **从 → 主**（从设备驱动）：**awready** · **wready** · **bresp/bvalid** ·
        //     **arready** · **rdata/rresp/rvalid**
        timer.io().awaddr  <<= lite.io().s2_awaddr;
        timer.io().awprot  <<= lite.io().s2_awprot;
        timer.io().awvalid <<= lite.io().s2_awvalid;
        lite.io().s2_awready <<= timer.io().awready;
        timer.io().wdata   <<= lite.io().s2_wdata;
        timer.io().wstrb   <<= lite.io().s2_wstrb;
        timer.io().wvalid  <<= lite.io().s2_wvalid;
        lite.io().s2_wready <<= timer.io().wready;
        lite.io().s2_bresp  <<= timer.io().bresp;
        lite.io().s2_bvalid <<= timer.io().bvalid;
        timer.io().bready  <<= lite.io().s2_bready;
        timer.io().araddr  <<= lite.io().s2_araddr;
        timer.io().arprot  <<= lite.io().s2_arprot;
        timer.io().arvalid <<= lite.io().s2_arvalid;
        lite.io().s2_arready <<= timer.io().arready;
        lite.io().s2_rdata   <<= timer.io().rdata;
        lite.io().s2_rresp   <<= timer.io().rresp;
        lite.io().s2_rvalid  <<= timer.io().rvalid;
        timer.io().rready  <<= lite.io().s2_rready;
        // ── s3 面 ↔ SPI（**19 根** · 从口编号见 `SocCrossbarMap`）──
        // ⚠️ **方向 MUST 按 AXI4-Lite 的角色**（本批踩过一次 ⇒ 写进注释）：
        //   · **主 → 从**（互联驱动）：awaddr/awprot/awvalid · wdata/wstrb/wvalid ·
        //     **bready** · araddr/arprot/arvalid · **rready**
        //   · **从 → 主**（从设备驱动）：**awready** · **wready** · **bresp/bvalid** ·
        //     **arready** · **rdata/rresp/rvalid**
        spi.io().awaddr  <<= lite.io().s3_awaddr;
        spi.io().awprot  <<= lite.io().s3_awprot;
        spi.io().awvalid <<= lite.io().s3_awvalid;
        lite.io().s3_awready <<= spi.io().awready;
        spi.io().wdata   <<= lite.io().s3_wdata;
        spi.io().wstrb   <<= lite.io().s3_wstrb;
        spi.io().wvalid  <<= lite.io().s3_wvalid;
        lite.io().s3_wready <<= spi.io().wready;
        lite.io().s3_bresp  <<= spi.io().bresp;
        lite.io().s3_bvalid <<= spi.io().bvalid;
        spi.io().bready  <<= lite.io().s3_bready;
        spi.io().araddr  <<= lite.io().s3_araddr;
        spi.io().arprot  <<= lite.io().s3_arprot;
        spi.io().arvalid <<= lite.io().s3_arvalid;
        lite.io().s3_arready <<= spi.io().arready;
        lite.io().s3_rdata   <<= spi.io().rdata;
        lite.io().s3_rresp   <<= spi.io().rresp;
        lite.io().s3_rvalid  <<= spi.io().rvalid;
        spi.io().rready  <<= lite.io().s3_rready;
        // ── s4 面 ↔ I2C（**19 根** · 从口编号见 `SocCrossbarMap`）──
        // ⚠️ **方向 MUST 按 AXI4-Lite 的角色**（本批踩过一次 ⇒ 写进注释）：
        //   · **主 → 从**（互联驱动）：awaddr/awprot/awvalid · wdata/wstrb/wvalid ·
        //     **bready** · araddr/arprot/arvalid · **rready**
        //   · **从 → 主**（从设备驱动）：**awready** · **wready** · **bresp/bvalid** ·
        //     **arready** · **rdata/rresp/rvalid**
        i2c.io().awaddr  <<= lite.io().s4_awaddr;
        i2c.io().awprot  <<= lite.io().s4_awprot;
        i2c.io().awvalid <<= lite.io().s4_awvalid;
        lite.io().s4_awready <<= i2c.io().awready;
        i2c.io().wdata   <<= lite.io().s4_wdata;
        i2c.io().wstrb   <<= lite.io().s4_wstrb;
        i2c.io().wvalid  <<= lite.io().s4_wvalid;
        lite.io().s4_wready <<= i2c.io().wready;
        lite.io().s4_bresp  <<= i2c.io().bresp;
        lite.io().s4_bvalid <<= i2c.io().bvalid;
        i2c.io().bready  <<= lite.io().s4_bready;
        i2c.io().araddr  <<= lite.io().s4_araddr;
        i2c.io().arprot  <<= lite.io().s4_arprot;
        i2c.io().arvalid <<= lite.io().s4_arvalid;
        lite.io().s4_arready <<= i2c.io().arready;
        lite.io().s4_rdata   <<= i2c.io().rdata;
        lite.io().s4_rresp   <<= i2c.io().rresp;
        lite.io().s4_rvalid  <<= i2c.io().rvalid;
        i2c.io().rready  <<= lite.io().s4_rready;
        // ── s5 面 ↔ PWM（**19 根** · 从口编号见 `SocCrossbarMap`）──
        // ⚠️ **方向 MUST 按 AXI4-Lite 的角色**（本批踩过一次 ⇒ 写进注释）：
        //   · **主 → 从**（互联驱动）：awaddr/awprot/awvalid · wdata/wstrb/wvalid ·
        //     **bready** · araddr/arprot/arvalid · **rready**
        //   · **从 → 主**（从设备驱动）：**awready** · **wready** · **bresp/bvalid** ·
        //     **arready** · **rdata/rresp/rvalid**
        pwm.io().awaddr  <<= lite.io().s5_awaddr;
        pwm.io().awprot  <<= lite.io().s5_awprot;
        pwm.io().awvalid <<= lite.io().s5_awvalid;
        lite.io().s5_awready <<= pwm.io().awready;
        pwm.io().wdata   <<= lite.io().s5_wdata;
        pwm.io().wstrb   <<= lite.io().s5_wstrb;
        pwm.io().wvalid  <<= lite.io().s5_wvalid;
        lite.io().s5_wready <<= pwm.io().wready;
        lite.io().s5_bresp  <<= pwm.io().bresp;
        lite.io().s5_bvalid <<= pwm.io().bvalid;
        pwm.io().bready  <<= lite.io().s5_bready;
        pwm.io().araddr  <<= lite.io().s5_araddr;
        pwm.io().arprot  <<= lite.io().s5_arprot;
        pwm.io().arvalid <<= lite.io().s5_arvalid;
        lite.io().s5_arready <<= pwm.io().arready;
        lite.io().s5_rdata   <<= pwm.io().rdata;
        lite.io().s5_rresp   <<= pwm.io().rresp;
        lite.io().s5_rvalid  <<= pwm.io().rvalid;
        pwm.io().rready  <<= lite.io().s5_rready;
        // ── 物理面（D-3 · 2026-11-27 用户裁决 = **本批不引**）──
        //
        // ⚠️ **GPIO 的两根引脚是 S4a 已有的根端口** ⇒ ⇒ **保持不动**
        //    （`gpio_in` / `gpio_out` ⇒ 根端口面不变 ⇒ D-8 成立）。
        gpio.io().gpio_in <<= io().gpio_in;
        io().gpio_out <<= gpio.io().gpio_out;
        //
        // ⚠️ **其余 5 个外设的物理引脚本批不引** ⇒ ⇒
        //    ① **物理输入** MUST 显式 tie 常量（未驱动输入 = 未定义值 ⇒ 跨后端漂移）
        //    ② **物理输出**（`uart_tx` / `spi_*` / `i2c_*` / `pwm_out_*` / `irq`）
        //       不接根 ⇒ ⇒ 产物卫生发射器按「零引用」剪除 ⇒ ⇒ lint 干净。
        uart.io().uart_rx <<= ch_bool(false);   // 无外部设备 ⇒ 永远「无数据」
        spi.io().spi_miso <<= ch_bool(false);
        i2c.io().i2c_sda_in <<= ch_bool(false);
        i2c.io().i2c_scl_in <<= ch_bool(false);
        // （`gpio_dir` / 各外设 `irq` 本批**不引出** · G-5 归 S5）

        // ── **零未驱动**：m1 面（DMA 预留 · 11 根）**显式 tie 0** ──
        //   m1 是 **DMA 预留**（设备在 S4c 接入）⇒ ⇒ 本批根只有一个 master（核）
        //   ⇒ ⇒ m1 永不发请求 ⇒ ⇒ tie 0 是**可证明的**死接线。
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
