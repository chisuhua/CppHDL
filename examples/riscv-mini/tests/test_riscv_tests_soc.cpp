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
