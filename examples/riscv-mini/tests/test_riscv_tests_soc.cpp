#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "rv32i_tcm.h"
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
    sim.eval();

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
    sim.eval();

    auto d_tcm = sim.get_port_value(decoder.instance().io().d_tcm_cs);
    REQUIRE(d_tcm != 0);
}

TEST_CASE("address_decoder: CLINT detection", "[address-decoder]") {
    context ctx("addr_decoder_test3");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());

    sim.set_input_value(decoder.instance().io().addr, ADDR_CLINT_BASE);
    sim.eval();

    auto cs = sim.get_port_value(decoder.instance().io().clint_cs);
    REQUIRE(cs != 0);
}

TEST_CASE("address_decoder: UART detection", "[address-decoder]") {
    context ctx("addr_decoder_test4");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());

    sim.set_input_value(decoder.instance().io().addr, ADDR_UART_BASE);
    sim.eval();

    auto cs = sim.get_port_value(decoder.instance().io().uart_cs);
    REQUIRE(cs != 0);
}

TEST_CASE("address_decoder: invalid address", "[address-decoder]") {
    context ctx("addr_decoder_test5");
    ctx_swap swap(&ctx);
    ch::ch_device<AddressDecoder> decoder;
    Simulator sim(decoder.context());

    sim.set_input_value(decoder.instance().io().addr, 0x12345678);
    sim.eval();

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
    sim.eval();
    sim.set_input_value(itcm.instance().io().write_en, 0);

    sim.set_input_value(itcm.instance().io().addr, 0);
    sim.eval();
    auto data = sim.get_port_value(itcm.instance().io().data);
    REQUIRE(static_cast<uint32_t>(static_cast<uint64_t>(data)) == 0xDEADBEEF);
}
