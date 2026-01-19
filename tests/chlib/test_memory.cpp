#include "catch_amalgamated.hpp"
#include "chlib/memory.h"
#include "codegen_dag.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Memory: single port RAM", "[memory][single_port_ram]") {

    SECTION("Basic read and write operations") {
        auto ctx = std::make_unique<ch::core::context>("test_single_port_ram");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<4> addr = 0_d;
        ch_uint<8> din = 0_d;
        ch_bool we = false;

        ch_uint<8> dout = single_port_ram<8, 4>(addr, din, we, "test_ram");

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Write value 0x55 to address 0
        sim.set_value(addr, 0);
        sim.set_value(din, 0x55);
        sim.set_value(we, 1);
        sim.tick();
        // toDAG("mem0.dot", ctx.get(), sim);

        // Read from address 0
        sim.set_value(we, 0);
        sim.tick();

        // toDAG("mem1.dot", ctx.get(), sim);

        REQUIRE(sim.get_value(dout) == 0x55);
    }

    SECTION("Write to different addresses") {
        auto ctx = std::make_unique<ch::core::context>("test_single_port_ram");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> addr = 0_d;
        ch_uint<8> din = 0_d;
        ch_bool we = false;

        ch_uint<8> dout = single_port_ram<8, 4>(addr, din, we, "test_ram2");

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Write value 0xAA to address 5
        sim.set_value(addr, 5);
        sim.set_value(din, 0xAA);
        sim.set_value(we, 1);
        sim.tick();

        // Read from address 5
        sim.set_value(we, 0);
        sim.set_value(addr, 5);
        sim.tick();

        REQUIRE(sim.get_value(dout) == 0xAA);

        // Read from address 0 (should be 0 since never written)
        sim.set_value(addr, 0);
        sim.tick();

        REQUIRE(sim.get_value(dout) == 0);
    }
}

TEST_CASE("Memory: dual port RAM", "[memory][dual_port_ram]") {

    SECTION("Independent read and write operations") {
        auto ctx = std::make_unique<ch::core::context>("test_dual_port_ram");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<4> addr_a = 0_d;
        ch_uint<8> din_a = 0_d;
        ch_bool we_a = false;

        ch_uint<4> addr_b = 0_d;
        ch_uint<8> din_b = 0_d;
        ch_bool we_b = false;

        DualPortRAMResult<8, 4> result = dual_port_ram<8, 4>(
            addr_a, din_a, we_a, addr_b, din_b, we_b, "test_dpram");

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Write value 0x12 to address 3 from port A
        sim.set_value(addr_a, 3);
        sim.set_value(din_a, 0x12);
        sim.set_value(we_a, 1);
        sim.tick();

        // Read from address 3 from port B
        sim.set_value(addr_b, 3);
        sim.set_value(we_b, 0);
        sim.tick();

        // sim.set_value(we_a, 1);

        REQUIRE(sim.get_value(result.dout_a) ==
                0); // Port A doesn't read during write
        REQUIRE(sim.get_value(result.dout_b) == 0x12); // Port B reads the value
    }

    SECTION("Simultaneous operations on different addresses") {
        auto ctx = std::make_unique<ch::core::context>("test_dual_port_ram");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<4> addr_a = 1_d;
        ch_uint<8> din_a = 0x34_h;
        ch_bool we_a = true;

        ch_uint<4> addr_b = 2_d;
        ch_uint<8> din_b = 0x56_h;
        ch_bool we_b = false;

        DualPortRAMResult<8, 4> result = dual_port_ram<8, 4>(
            addr_a, din_a, we_a, addr_b, din_b, we_b, "test_dpram2");

        ch::Simulator sim(ctx.get());
        sim.tick();
        sim.set_value(we_a, 1);

        // Simultaneous operations
        sim.tick();

        REQUIRE(sim.get_value(result.dout_a) ==
                0); // Port A writing, not reading
        REQUIRE(sim.get_value(result.dout_b) ==
                0); // Port B reading address 2 (never written)

        // Now read from address 1 on port A
        sim.set_value(we_a, 0);
        sim.set_value(addr_a, 1);
        sim.tick();
        // toDAG("mem3.dot", ctx.get(), sim);

        REQUIRE(sim.get_value(result.dout_a) == 0x34); // Value written earlier
    }
}

TEST_CASE("Memory: dual port RAM single clock",
          "[memory][dual_port_ram_single_clk]") {
    auto ctx =
        std::make_unique<ch::core::context>("test_dual_port_ram_single_clk");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Single clock dual port operations") {
        ch_uint<4> addr_a = 0_d;
        ch_uint<8> din_a = 0_d;
        ch_bool we_a = false;
        ch_uint<4> addr_b = 0_d;
        ch_uint<8> din_b = 0_d;
        ch_bool we_b = false;

        DualPortRAMResult<8, 4> result = dual_port_ram_single_clk<8, 4>(
            addr_a, din_a, we_a, addr_b, din_b, we_b, "test_dpram_sc");

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Write value 0x78 to address 7 from port A
        sim.set_value(addr_a, 7);
        sim.set_value(din_a, 0x78);
        sim.set_value(we_a, 1);
        sim.tick();

        // Read from address 7 from port B
        sim.set_value(we_a, 0);
        sim.set_value(addr_b, 7);
        sim.set_value(we_b, 0);
        sim.tick();

        REQUIRE(sim.get_value(result.dout_a) ==
                0x78); // Port A reads from addr 7
        REQUIRE(sim.get_value(result.dout_b) ==
                0x78); // Port B reads from addr 7
    }
}

TEST_CASE("Memory: sync FIFO", "[memory][sync_fifo]") {

    SECTION("Basic FIFO operations") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din = 0_d;
        ch_bool wr_en = false;
        ch_bool rd_en = false;

        FIFOResult<8, 3> fifo = sync_fifo<8, 3>(din, wr_en, rd_en, "test_fifo");

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(fifo.empty) == true);
        REQUIRE(sim.get_value(fifo.full) == false);
        REQUIRE(sim.get_value(fifo.count) == 0);

        sim.tick();

        // Write first value
        sim.set_value(din, 0xAB);
        sim.set_value(wr_en, 1);
        sim.tick();

        REQUIRE(sim.get_value(fifo.count) == 1);
        REQUIRE(sim.get_value(fifo.empty) == false);

        // Write second value
        sim.set_value(din, 0xCD);
        sim.tick();

        REQUIRE(sim.get_value(fifo.count) == 2);

        // Read first value
        sim.set_value(wr_en, 0);
        sim.set_value(rd_en, 1);
        sim.tick(); // 4
        REQUIRE(sim.get_value(fifo.count) == 1);
        REQUIRE(sim.get_value(fifo.dout) == 0);
        sim.tick(); // 5

        REQUIRE(sim.get_value(fifo.dout) == 0xAB);

        // Read second value
        sim.tick();

        REQUIRE(sim.get_value(fifo.dout) == 0xCD);
        REQUIRE(sim.get_value(fifo.count) == 0);
        REQUIRE(sim.get_value(fifo.empty) == true);
    }

    SECTION("FIFO full and empty conditions") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din = 0_d;
        ch_bool wr_en = false;
        ch_bool rd_en = false;

        FIFOResult<8, 2> fifo =
            sync_fifo<8, 2>(din, wr_en, rd_en, "test_fifo_full");

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Fill FIFO completely (depth = 2^2 = 4)
        for (int i = 0; i < 4; ++i) {
            sim.set_value(din, i + 1);
            sim.set_value(wr_en, 1);
            sim.tick();
        }

        REQUIRE(sim.get_value(fifo.full) == true);
        REQUIRE(sim.get_value(fifo.count) == 4);

        // Try to write more (should not increase count)
        sim.set_value(din, 0xFF);
        sim.tick();

        REQUIRE(sim.get_value(fifo.full) == true);
        REQUIRE(sim.get_value(fifo.count) == 4);

        // Read all values
        for (int i = 0; i < 4; ++i) {
            sim.set_value(wr_en, 0);
            sim.set_value(rd_en, 1);
            sim.tick();
        }

        REQUIRE(sim.get_value(fifo.empty) == true);
        REQUIRE(sim.get_value(fifo.count) == 0);
    }

    SECTION("FIFO with combinatorial output") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo_comb");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din = 0_d;
        ch_bool wr_en = false;
        ch_bool rd_en = false;
        ch_bool rst = true;

        FIFOResult<8, 3> fifo =
            sync_fifo<8, 3, false>(din, wr_en, rd_en, "test_fifo_comb");

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(fifo.empty) == true);
        REQUIRE(sim.get_value(fifo.full) == false);
        REQUIRE(sim.get_value(fifo.count) == 0);

        // Deassert reset
        sim.tick();

        // Write value to FIFO
        sim.set_value(din, 0x99);
        sim.set_value(wr_en, 1);
        sim.tick();

        REQUIRE(sim.get_value(fifo.count) == 1);
        REQUIRE(sim.get_value(fifo.empty) == false);

        // Enable read - should see the value immediately with combinatorial
        // output
        sim.set_value(wr_en, 0);
        sim.set_value(rd_en, 1);
        sim.tick();

        REQUIRE(sim.get_value(fifo.dout) == 0x99);
        REQUIRE(sim.get_value(fifo.count) == 0);
        REQUIRE(sim.get_value(fifo.empty) == true);
    }
}