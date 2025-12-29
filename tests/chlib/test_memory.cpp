#include "catch_amalgamated.hpp"
#include "chlib/memory.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Memory: single port RAM", "[memory][single_port_ram]") {
    auto ctx = std::make_unique<ch::core::context>("test_single_port_ram");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic read and write operations") {
        ch_uint<8> addr = 0_d;
        ch_uint<8> din = 0_d;
        ch_bool we = false;
        ch_bool clk = false;
        
        ch_uint<8> dout = single_port_ram<8, 4>(addr, din, we, clk, "test_ram");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Write value 0x55 to address 0
        addr = 0_d;
        din = 0x55_d;
        we = true;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        // Read from address 0
        we = false;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(dout) == 0x55);
    }
    
    SECTION("Write to different addresses") {
        ch_uint<8> addr = 0_d;
        ch_uint<8> din = 0_d;
        ch_bool we = false;
        ch_bool clk = false;
        
        ch_uint<8> dout = single_port_ram<8, 4>(addr, din, we, clk, "test_ram2");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Write value 0xAA to address 5
        addr = 5_d;
        din = 0xAA_d;
        we = true;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        // Read from address 5
        we = false;
        addr = 5_d;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(dout) == 0xAA);
        
        // Read from address 0 (should be 0 since never written)
        addr = 0_d;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(dout) == 0);
    }
}

TEST_CASE("Memory: dual port RAM", "[memory][dual_port_ram]") {
    auto ctx = std::make_unique<ch::core::context>("test_dual_port_ram");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Independent read and write operations") {
        ch_uint<4> addr_a = 0_d;
        ch_uint<8> din_a = 0_d;
        ch_bool we_a = false;
        ch_bool clk_a = false;
        
        ch_uint<4> addr_b = 0_d;
        ch_uint<8> din_b = 0_d;
        ch_bool we_b = false;
        ch_bool clk_b = false;
        
        DualPortRAMResult<8, 4> result = dual_port_ram<8, 4>(
            addr_a, din_a, we_a, clk_a,
            addr_b, din_b, we_b, clk_b, "test_dpram");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Write value 0x12 to address 3 from port A
        addr_a = 3_d;
        din_a = 0x12_d;
        we_a = true;
        clk_a = true;
        sim.tick();
        clk_a = false;
        sim.eval();
        
        // Read from address 3 from port B
        addr_b = 3_d;
        we_b = false;
        clk_b = true;
        sim.tick();
        clk_b = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(result.dout_a) == 0);  // Port A doesn't read during write
        REQUIRE(simulator.get_value(result.dout_b) == 0x12);  // Port B reads the value
    }
    
    SECTION("Simultaneous operations on different addresses") {
        ch_uint<4> addr_a = 1_d;
        ch_uint<8> din_a = 0x34_d;
        ch_bool we_a = true;
        ch_bool clk_a = false;
        
        ch_uint<4> addr_b = 2_d;
        ch_uint<8> din_b = 0x56_d;
        ch_bool we_b = false;
        ch_bool clk_b = false;
        
        DualPortRAMResult<8, 4> result = dual_port_ram<8, 4>(
            addr_a, din_a, we_a, clk_a,
            addr_b, din_b, we_b, clk_b, "test_dpram2");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Simultaneous operations
        clk_a = true;
        clk_b = true;
        sim.tick();
        clk_a = false;
        clk_b = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(result.dout_a) == 0);  // Port A writing, not reading
        REQUIRE(simulator.get_value(result.dout_b) == 0);  // Port B reading address 2 (never written)
        
        // Now read from address 1 on port A
        we_a = false;
        addr_a = 1_d;
        clk_a = true;
        sim.tick();
        clk_a = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(result.dout_a) == 0x34);  // Value written earlier
    }
}

TEST_CASE("Memory: dual port RAM single clock", "[memory][dual_port_ram_single_clk]") {
    auto ctx = std::make_unique<ch::core::context>("test_dual_port_ram_single_clk");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Single clock dual port operations") {
        ch_uint<4> addr_a = 0_d;
        ch_uint<8> din_a = 0_d;
        ch_bool we_a = false;
        ch_uint<4> addr_b = 0_d;
        ch_uint<8> din_b = 0_d;
        ch_bool we_b = false;
        ch_bool clk = false;
        
        DualPortRAMResult<8, 4> result = dual_port_ram_single_clk<8, 4>(
            addr_a, din_a, we_a,
            addr_b, din_b, we_b, clk, "test_dpram_sc");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Write value 0x78 to address 7 from port A
        addr_a = 7_d;
        din_a = 0x78_d;
        we_a = true;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        // Read from address 7 from port B
        we_a = false;
        addr_b = 7_d;
        we_b = false;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(result.dout_a) == 0x78);  // Port A reads from addr 7
        REQUIRE(simulator.get_value(result.dout_b) == 0x78);  // Port B reads from addr 7
    }
}

TEST_CASE("Memory: sync FIFO", "[memory][sync_fifo]") {
    auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic FIFO operations") {
        ch_uint<8> din = 0_d;
        ch_bool wr_en = false;
        ch_bool rd_en = false;
        ch_bool clk = false;
        ch_bool rst = true;
        
        FIFOResult<8, 3> fifo = sync_fifo<8, 3>(din, wr_en, rd_en, clk, rst, "test_fifo");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Reset FIFO
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(fifo.empty) == true);
        REQUIRE(simulator.get_value(fifo.full) == false);
        REQUIRE(simulator.get_value(fifo.count) == 0);
        
        // Deassert reset
        rst = false;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        // Write first value
        din = 0xAB_d;
        wr_en = true;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(fifo.count) == 1);
        REQUIRE(simulator.get_value(fifo.empty) == false);
        
        // Write second value
        din = 0xCD_d;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(fifo.count) == 2);
        
        // Read first value
        wr_en = false;
        rd_en = true;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(fifo.dout) == 0xAB);
        REQUIRE(simulator.get_value(fifo.count) == 1);
        
        // Read second value
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(fifo.dout) == 0xCD);
        REQUIRE(simulator.get_value(fifo.count) == 0);
        REQUIRE(simulator.get_value(fifo.empty) == true);
    }
    
    SECTION("FIFO full and empty conditions") {
        ch_uint<8> din = 0_d;
        ch_bool wr_en = false;
        ch_bool rd_en = false;
        ch_bool clk = false;
        ch_bool rst = false;
        
        FIFOResult<8, 2> fifo = sync_fifo<8, 2>(din, wr_en, rd_en, clk, rst, "test_fifo_full");
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Fill FIFO completely (depth = 2^2 = 4)
        for (int i = 0; i < 4; ++i) {
            din = ch_uint<8>(i + 1);
            wr_en = true;
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
        }
        
        REQUIRE(simulator.get_value(fifo.full) == true);
        REQUIRE(simulator.get_value(fifo.count) == 4);
        
        // Try to write more (should not increase count)
        din = 0xFF_d;
        clk = true;
        sim.tick();
        clk = false;
        sim.eval();
        
        REQUIRE(simulator.get_value(fifo.full) == true);
        REQUIRE(simulator.get_value(fifo.count) == 4);
        
        // Read all values
        for (int i = 0; i < 4; ++i) {
            wr_en = false;
            rd_en = true;
            clk = true;
            sim.tick();
            clk = false;
            sim.eval();
        }
        
        REQUIRE(simulator.get_value(fifo.empty) == true);
        REQUIRE(simulator.get_value(fifo.count) == 0);
    }
}