#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "cpu/pipeline/rv32i_tcm.h"
#include "address_decoder.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace riscv;

TEST_CASE("address_decoder: ITCM base address", "[address-decoder]") {
    REQUIRE(ADDR_ITCM_BASE == 0x80000000);
    REQUIRE(ADDR_DTCM_BASE == 0x80010000);
    REQUIRE(ADDR_GPIO_BASE == 0x80102000);
}

TEST_CASE("address_decoder: hardware instantiation", "[address-decoder]") {
    context ctx("addr_decoder_test");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());

    sim.set_input_value(decoder.instance().io().addr, ADDR_ITCM_BASE);
    sim.tick();

    auto i_tcm = sim.get_port_value(decoder.instance().io().i_tcm_cs);
    auto d_tcm = sim.get_port_value(decoder.instance().io().d_tcm_cs);

    REQUIRE(i_tcm != 0);
    REQUIRE(d_tcm == 0);
}

TEST_CASE("address_decoder: DTCM detection", "[address-decoder]") {
    context ctx("addr_decoder_test2");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());

    sim.set_input_value(decoder.instance().io().addr, ADDR_DTCM_BASE);
    sim.tick();

    auto d_tcm = sim.get_port_value(decoder.instance().io().d_tcm_cs);
    REQUIRE(d_tcm != 0);
}

TEST_CASE("address_decoder: CLINT detection", "[address-decoder]") {
    context ctx("addr_decoder_test3");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());

    sim.set_input_value(decoder.instance().io().addr, ADDR_CLINT_BASE);
    sim.tick();

    auto cs = sim.get_port_value(decoder.instance().io().clint_cs);
    REQUIRE(cs != 0);
}

TEST_CASE("address_decoder: UART detection", "[address-decoder]") {
    context ctx("addr_decoder_test4");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());

    sim.set_input_value(decoder.instance().io().addr, ADDR_UART_BASE);
    sim.tick();

    auto cs = sim.get_port_value(decoder.instance().io().uart_cs);
    REQUIRE(cs != 0);
}

TEST_CASE("address_decoder: invalid address", "[address-decoder]") {
    context ctx("addr_decoder_test5");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());

    sim.set_input_value(decoder.instance().io().addr, 0x12345678);
    sim.tick();

    auto cs = sim.get_port_value(decoder.instance().io().error_cs);
    REQUIRE(cs != 0);
}

// ===========================================================================
// **G-6 的 MMIO 全表**（2026-11-24 · change `rv32i-soc-mmio-channel-2026-11`）
//
// ## 为什么既有 6 处消费者**判不了**新表
//
// 既有用例只覆盖 ITCM / DTCM / CLINT / UART / 非法地址 ⇒ ⇒ 新增的 **5 个片选**
// （`spi_cs` / `i2c_cs` / `pwm_cs` / `dma_cs` / `timer_cs`）与 **8 段表**的**派生性**
// ⇒ ⇒ **零覆盖** ⇒ ⇒ 「片选接错 / 段序错位 / 窗口越界」全部**不可见**。
//
// ## 期望值**手推**（禁抄实现）
//
// · 8 段**自 `0x8010_0000` 顺延**，每段 **4 KB** ⇒ ⇒
//   Clint `0x80100000` · Uart `0x80101000` · Gpio `0x80102000` · Spi `0x80103000`
//   · I2c `0x80104000` · Pwm `0x80105000` · Dma `0x80106000` · **Timer `0x80107000`**
// · 窗口**最后一字节** = `0x80107000 + 0x1000 - 1` = `0x80107FFF`
//   ⇒ ⇒ **第一个窗口外字节** = `0x80108000`
// ⚠️ **2026-11-27 · D-1**：TIMER 段**追加在末尾** ⇒ ⇒ **既有 7 段基址零移动**
//   ⇒ ⇒ 下表前 7 段的**期望值逐字不变**（只把「窗口末字节 / 第一个外字节」右移一段）。
// ===========================================================================
namespace {
struct MmioSight {
    uint64_t clint = 0, uart = 0, gpio = 0, spi = 0, i2c = 0, pwm = 0, dma = 0;
    uint64_t timer = 0;
    uint64_t error = 0;
    // `local_addr` 是**另一条**独立的 10 路 mux 链（与片选分开推导）⇒ ⇒
    // 只判片选时「某分支减错基址」（如 spi 分支减 i2c_base）**不可见** ⇒ ⇒ 一并观测。
    uint64_t local = 0;
};
MmioSight probe_mmio(uint32_t addr) {
    context ctx("addr_decoder_mmio");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());
    sim.set_input_value(decoder.instance().io().addr, addr);
    sim.tick();
    const auto &io = decoder.instance().io();
    MmioSight out;
    out.clint = static_cast<uint64_t>(sim.get_port_value(io.clint_cs));
    out.uart = static_cast<uint64_t>(sim.get_port_value(io.uart_cs));
    out.gpio = static_cast<uint64_t>(sim.get_port_value(io.gpio_cs));
    out.spi = static_cast<uint64_t>(sim.get_port_value(io.spi_cs));
    out.i2c = static_cast<uint64_t>(sim.get_port_value(io.i2c_cs));
    out.pwm = static_cast<uint64_t>(sim.get_port_value(io.pwm_cs));
    out.dma = static_cast<uint64_t>(sim.get_port_value(io.dma_cs));
    out.timer = static_cast<uint64_t>(sim.get_port_value(io.timer_cs));
    out.error = static_cast<uint64_t>(sim.get_port_value(io.error_cs));
    out.local = static_cast<uint64_t>(sim.get_port_value(io.local_addr));
    return out;
}
}  // namespace

TEST_CASE("address_decoder: MMIO 全表 MUST 派生 + 8 段 MUST 各自命中",
          "[address-decoder][mmio]") {
    // ① 基址**手推值** vs 派生值（**禁**抄实现常量 ⇒ ⇒ 派生写错必红）
    REQUIRE(ADDR_CLINT_BASE == 0x80100000u);
    REQUIRE(ADDR_UART_BASE == 0x80101000u);
    REQUIRE(ADDR_GPIO_BASE == 0x80102000u);
    REQUIRE(ADDR_SPI_BASE == 0x80103000u);
    REQUIRE(ADDR_I2C_BASE == 0x80104000u);
    REQUIRE(ADDR_PWM_BASE == 0x80105000u);
    REQUIRE(ADDR_DMA_BASE == 0x80106000u);
    // ⚠️ **2026-11-27 · D-1**：TIMER 段**追加在末尾** ⇒ ⇒ 前 7 段**零移动**
    REQUIRE(ADDR_TIMER_BASE == 0x80107000u);
    // ② 每段 **4 KB**（**派生**自库层表 ⇒ ⇒ 禁手写第二份）
    REQUIRE(ADDR_CLINT_SIZE == 0x1000u);
    REQUIRE(ADDR_DMA_SIZE == 0x1000u);
    REQUIRE(ADDR_TIMER_SIZE == 0x1000u);

    SECTION("每段的**首字节** MUST 只命中自己那一个片选") {
        const auto c = probe_mmio(ADDR_CLINT_BASE);
        REQUIRE(c.clint == 1u);
        REQUIRE(c.uart == 0u);
        REQUIRE(c.error == 0u);
        const auto u = probe_mmio(ADDR_UART_BASE);
        REQUIRE(u.uart == 1u);
        REQUIRE(u.clint == 0u);
        REQUIRE(u.error == 0u);
        const auto g = probe_mmio(ADDR_GPIO_BASE);
        REQUIRE(g.gpio == 1u);
        REQUIRE(g.error == 0u);
        const auto s = probe_mmio(ADDR_SPI_BASE);
        REQUIRE(s.spi == 1u);
        REQUIRE(s.gpio == 0u);
        REQUIRE(s.error == 0u);
        const auto i = probe_mmio(ADDR_I2C_BASE);
        REQUIRE(i.i2c == 1u);
        REQUIRE(i.spi == 0u);
        REQUIRE(i.error == 0u);
        const auto p = probe_mmio(ADDR_PWM_BASE);
        REQUIRE(p.pwm == 1u);
        REQUIRE(p.i2c == 0u);
        REQUIRE(p.error == 0u);
        const auto d = probe_mmio(ADDR_DMA_BASE);
        REQUIRE(d.dma == 1u);
        REQUIRE(d.pwm == 0u);
        REQUIRE(d.error == 0u);
        const auto t = probe_mmio(ADDR_TIMER_BASE);
        REQUIRE(t.timer == 1u);
        REQUIRE(t.dma == 0u);
        REQUIRE(t.error == 0u);
    }
    SECTION("窗口**最后一字节** MUST 命中 TIMER") {
        const auto t = probe_mmio(0x80107FFFu);
        REQUIRE(t.timer == 1u);
        REQUIRE(t.dma == 0u);
        REQUIRE(t.error == 0u);
        // 手推：`0x8010_7FFF - 0x8010_7000` = `0x1000 - 1` = `0xFFF`
        REQUIRE(t.local == 0xFFFu);
    }
    SECTION("DMA 段的**最后一字节** MUST 命中 DMA（TIMER 追加 ⇒ DMA 零移动）") {
        const auto d = probe_mmio(0x80106FFFu);
        REQUIRE(d.dma == 1u);
        REQUIRE(d.timer == 0u);
        REQUIRE(d.error == 0u);
        // 手推：`0x8010_6FFF - 0x8010_6000` = `0x1000 - 1` = `0xFFF`
        REQUIRE(d.local == 0xFFFu);
    }
    SECTION("每段的 **local_addr** MUST 减去**自己**的基址") {
        // `local_addr` 与片选是**两条独立的 mux 链** ⇒ ⇒ 只判片选时「某分支减错基址」
        // **不可见**。手推：`base + 0x40` ⇒ ⇒ local = `0x40`；若某分支错用相邻段基址
        // （如 spi 分支用 i2c_base）⇒ ⇒ 32 位回绕成 `0xFFFFF040` ⇒ ⇒ 必红。
        REQUIRE(probe_mmio(ADDR_CLINT_BASE + 0x40u).local == 0x40u);
        REQUIRE(probe_mmio(ADDR_UART_BASE  + 0x40u).local == 0x40u);
        REQUIRE(probe_mmio(ADDR_GPIO_BASE  + 0x40u).local == 0x40u);
        REQUIRE(probe_mmio(ADDR_SPI_BASE   + 0x40u).local == 0x40u);
        REQUIRE(probe_mmio(ADDR_I2C_BASE   + 0x40u).local == 0x40u);
        REQUIRE(probe_mmio(ADDR_PWM_BASE   + 0x40u).local == 0x40u);
        REQUIRE(probe_mmio(ADDR_DMA_BASE   + 0x40u).local == 0x40u);
        REQUIRE(probe_mmio(ADDR_TIMER_BASE + 0x40u).local == 0x40u);
        // 反向：地址不在任何段 ⇒ ⇒ local 恒 0（窗口外不泄漏基址差）
        REQUIRE(probe_mmio(0x90000000u).local == 0u);
    }
    SECTION("窗口**第一个外字节** MUST 报错且 MUST NOT 命中任何片选") {
        const auto x = probe_mmio(0x80108000u);
        REQUIRE(x.error == 1u);
        REQUIRE(x.timer == 0u);
        REQUIRE(x.dma == 0u);
        REQUIRE(x.pwm == 0u);
    }
    SECTION("MMIO 窗口**之前**的地址 MUST 报错（TCM 与 MMIO 之间是空洞）") {
        const auto x = probe_mmio(0x800FFFFFu);
        REQUIRE(x.error == 1u);
        REQUIRE(x.clint == 0u);
    }
}

TEST_CASE("mem_map: MMIO 窗口 MUST 连续且不与 TCM 重叠", "[address-decoder][mmio]") {
    // ① 窗口**连续**（8 段 × 4 KB = `0x80100000` + `0x8000`）
    REQUIRE(kMmioBase == 0x80100000ull);
    REQUIRE(kMmioBytes == 0x8000ull);
    // ② 段序**不重叠**（逐段断言 ⇒ ⇒ 段序写错必红）
    REQUIRE(kClint.base + kClint.size == kUart.base);
    REQUIRE(kUart.base + kUart.size == kGpio.base);
    REQUIRE(kGpio.base + kGpio.size == kSpi.base);
    REQUIRE(kSpi.base + kSpi.size == kI2c.base);
    REQUIRE(kI2c.base + kI2c.size == kPwm.base);
    REQUIRE(kPwm.base + kPwm.size == kDma.base);
    REQUIRE(kDma.base + kDma.size == kTimer.base);
    // ③ **不与 TCM 重叠**（MMIO 基址 MUST ≥ D-TCM 末地址 + 1）
    REQUIRE(kMmioBase >= kDtcm.base + kDtcm.size);
    // ④ `in_mmio` 的**边界**（**闭区间** `[base, base+bytes)`）
    REQUIRE(in_mmio(kMmioBase));
    REQUIRE(in_mmio(kMmioBase + kMmioBytes - 1));
    REQUIRE(!in_mmio(kMmioBase + kMmioBytes));
    // ⚠️ **方向判据**：字节地址 **0** MUST **不**落在 MMIO 窗口 ⇒ ⇒
    //    否则核级程序 `sw x3, 0(x0)` 会落进 MMIO 旁路 ⇒ ⇒ 核级 TB 永久挂死
    //    （G-9 订正块：判据方向 MUST 是「∈ MMIO 段」）
    REQUIRE(!in_mmio(0ull));
}

TEST_CASE("itcm: load and read instruction", "[itcm]") {
    context ctx("itcm_test");
    ctx_swap swap(&ctx);
    ch::ch_device<InstrTCM<10, 32>> itcm;
    Simulator sim(itcm.context());

    sim.set_input_value(itcm.instance().io().write_addr, 0);
    sim.set_input_value(itcm.instance().io().write_data, 0xDEADBEEF);
    sim.set_input_value(itcm.instance().io().write_en, 1);
    sim.tick();
    sim.set_input_value(itcm.instance().io().write_en, 0);

    sim.set_input_value(itcm.instance().io().addr, 0);
    sim.tick();
    auto data = sim.get_port_value(itcm.instance().io().data);
    REQUIRE(static_cast<uint32_t>(static_cast<uint64_t>(data)) == 0xDEADBEEF);
}

// ===========================================================================
// **T4 / L1 的越界投毒判据**（`firmware-load-chain` L1 · `memory-timing-contract` T4）
//
// ## 为什么既有用例**判不了**这个问题
//
// 既有「invalid address」用例只测 `0x12345678` ⇒ ⇒ 那个地址在**旧 64 KB 声明**下
// **也**是区域外 ⇒ ⇒ **对「声明 64 KB · 实现 16 KB」这个缺陷零判别力**。
//
// ⇒ ⇒ 有判别力的地址 = **落在旧声明内、落在新实现外**：`0x8000_4000`
//   ⇒ ⇒ 旧声明（64 KB）：`i_tcm_cs = 1`（**静默截断**成 ITCM 的字 0 ⇒ 无任何报错）
//   ⇒ ⇒ 新实现（16 KB）：`error_cs = 1` 且 `i_tcm_cs = 0`（**可诊断**）
//
// ## 判据（**正反两面**）
//
//   · 正面：`0x8000_3FFC`（最后一个合法字）⇒ `i_tcm_cs = 1` 且 `error_cs = 0`
//   · 反面：`0x8000_4000`（越界第一字）⇒ `error_cs = 1` 且 `i_tcm_cs = 0`
//   ⇒ ⇒ 只判反面不够：「把所有地址都判成越界」也能让反面绿（⇒ 正面钉住不误判）
// ===========================================================================

namespace {
struct DecoderSight {
    uint64_t itcm = 0, dtcm = 0, error = 0;
};
DecoderSight probe_decoder(uint32_t addr) {
    context ctx("addr_decoder_boundary");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());
    sim.set_input_value(decoder.instance().io().addr, addr);
    sim.tick();
    DecoderSight out;
    out.itcm = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().i_tcm_cs));
    out.dtcm = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().d_tcm_cs));
    out.error = static_cast<uint64_t>(sim.get_port_value(decoder.instance().io().error_cs));
    return out;
}
}  // namespace

TEST_CASE("address_decoder: TCM boundary MUST be diagnosed, not truncated",
          "[address-decoder][boundary][poison]") {
    // ⚠️ 前提：TCM 是 **16 KB**（派生自 `kTcmAddrWidth = 12`）
    REQUIRE(ADDR_ITCM_SIZE == 16384u);
    REQUIRE(ADDR_DTCM_SIZE == 16384u);
    REQUIRE(ADDR_ITCM_END == 0x80003FFFu);
    REQUIRE(ADDR_DTCM_END == 0x80013FFFu);

    SECTION("正面：最后一个合法字 MUST 命中 ITCM 且不报错") {
        const auto s = probe_decoder(0x80003FFCu);
        REQUIRE(s.itcm == 1u);
        REQUIRE(s.dtcm == 0u);
        REQUIRE(s.error == 0u);
    }
    SECTION("反面：越界第一字 MUST 报错且 MUST NOT 命中 ITCM") {
        // ⚠️ 这就是「旧 64 KB 声明」下**不会红**的那一格
        const auto s = probe_decoder(0x80004000u);
        REQUIRE(s.error == 1u);
        REQUIRE(s.itcm == 0u);
    }
    SECTION("D-TCM 同理（最后合法 / 越界第一字）") {
        const auto ok = probe_decoder(0x80013FFCu);
        REQUIRE(ok.dtcm == 1u);
        REQUIRE(ok.error == 0u);
        const auto bad = probe_decoder(0x80014000u);
        REQUIRE(bad.error == 1u);
        REQUIRE(bad.dtcm == 0u);
    }
    SECTION("区域之间的空洞（ITCM 尾与 DTCM 头之间）MUST 报错") {
        const auto s = probe_decoder(0x8000FFFCu);
        REQUIRE(s.error == 1u);
        REQUIRE(s.itcm == 0u);
        REQUIRE(s.dtcm == 0u);
    }
}
