#include "catch_amalgamated.hpp"
#include "chlib/fifo.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("FIFO: sync_fifo", "[fifo][sync_fifo]") {
    auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic write and read operations") {
        ch_bool clk = true;
        ch_bool rst = true;
        ch_bool wren = true;
        ch_uint<8> din = 0x55_d;
        ch_bool rden = false;
        ch_uint<3> threshold = 0_d;
        
        SyncFifoResult<8, 3> result = sync_fifo<8, 3>(clk, rst, wren, din, rden, threshold);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // After reset, FIFO should be empty
        REQUIRE(simulator.get_value(result.empty) == true);
        REQUIRE(simulator.get_value(result.full) == false);
    }
    
    SECTION("Write single item and read it") {
        ch_bool clk = true;
        ch_bool rst = false;
        ch_bool wren = true;
        ch_uint<8> din = 0x55_d;
        ch_bool rden = false;
        
        SyncFifoResult<8, 3> result = sync_fifo<8, 3>(clk, rst, wren, din, rden);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // After writing, FIFO should not be empty
        REQUIRE(simulator.get_value(result.empty) == false);
        
        // Now read the item
        ch_bool read_clk = true;
        ch_bool read_rst = false;
        ch_bool read_wren = false;
        ch_uint<8> read_din = 0x00_d;
        ch_bool read_rden = true;
        
        SyncFifoResult<8, 3> read_result = sync_fifo<8, 3>(read_clk, read_rst, read_wren, read_din, read_rden);
        
        sim.tick();
        REQUIRE(simulator.get_value(read_result.q) == 0x55);
    }
}

TEST_CASE("FIFO: fwft_fifo", "[fifo][fwft_fifo]") {
    auto ctx = std::make_unique<ch::core::context>("test_fwft_fifo");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic write and read operations") {
        ch_bool clk = true;
        ch_bool rst = true;
        ch_bool wren = false;
        ch_uint<8> din = 0x00_d;
        ch_bool rden = false;
        
        FwftFifoResult<8, 3> result = fwft_fifo<8, 3>(clk, rst, wren, din, rden);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // After reset, FIFO should be empty
        REQUIRE(simulator.get_value(result.empty) == true);
        REQUIRE(simulator.get_value(result.full) == false);
    }
}

TEST_CASE("FIFO: lifo_stack", "[fifo][lifo_stack]") {
    auto ctx = std::make_unique<ch::core::context>("test_lifo_stack");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic push and pop operations") {
        ch_bool clk = true;
        ch_bool rst = false;
        ch_bool push = true;
        ch_uint<8> din = 0x12_d;
        ch_bool pop = false;
        
        LifoResult<8, 3> result = lifo_stack<8, 3>(clk, rst, push, din, pop);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // After pushing, FIFO should not be empty
        REQUIRE(simulator.get_value(result.empty) == false);
        
        // Push another value
        ch_bool push_clk = true;
        ch_bool push_rst = false;
        ch_bool push2 = true;
        ch_uint<8> din2 = 0x34_d;
        ch_bool pop2 = false;
        
        LifoResult<8, 3> result2 = lifo_stack<8, 3>(push_clk, push_rst, push2, din2, pop2);
        
        sim.tick();
        
        // Pop the last value
        ch_bool pop_clk = true;
        ch_bool pop_rst = false;
        ch_bool pop3 = true;
        ch_uint<8> din3 = 0x00_d;
        
        LifoResult<8, 3> result3 = lifo_stack<8, 3>(pop_clk, pop_rst, false, din3, pop3);
        
        sim.tick();
        REQUIRE(simulator.get_value(result3.q) == 0x34);
    }
    
    SECTION("Push and pop sequence") {
        ch_bool clk = true;
        ch_bool rst = false;
        
        // Push 3 values: 0x11, 0x22, 0x33
        LifoResult<8, 3> result1 = lifo_stack<8, 3>(clk, rst, true, 0x11_d, false);
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        LifoResult<8, 3> result2 = lifo_stack<8, 3>(clk, rst, true, 0x22_d, false);
        sim.tick();
        
        LifoResult<8, 3> result3 = lifo_stack<8, 3>(clk, rst, true, 0x33_d, false);
        sim.tick();
        
        // Now pop them - should come out in reverse order
        LifoResult<8, 3> pop1 = lifo_stack<8, 3>(clk, rst, false, 0x00_d, true);
        sim.tick();
        REQUIRE(simulator.get_value(pop1.q) == 0x33);
        
        LifoResult<8, 3> pop2 = lifo_stack<8, 3>(clk, rst, false, 0x00_d, true);
        sim.tick();
        REQUIRE(simulator.get_value(pop2.q) == 0x22);
        
        LifoResult<8, 3> pop3 = lifo_stack<8, 3>(clk, rst, false, 0x00_d, true);
        sim.tick();
        REQUIRE(simulator.get_value(pop3.q) == 0x11);
    }
}

TEST_CASE("FIFO: async_fifo", "[fifo][async_fifo]") {
    auto ctx = std::make_unique<ch::core::context>("test_async_fifo");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic write and read operations with different clocks") {
        ch_bool wr_clk = true;
        ch_bool wr_rst = true;
        ch_bool wren = false;
        ch_uint<8> din = 0x00_d;
        ch_bool rd_clk = true;
        ch_bool rd_rst = true;
        ch_bool rden = false;
        
        AsyncFifoResult<8, 3> result = async_fifo<8, 3>(wr_clk, wr_rst, wren, din, rd_clk, rd_rst, rden);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // After reset, FIFO should be empty
        REQUIRE(simulator.get_value(result.empty) == true);
        REQUIRE(simulator.get_value(result.full) == false);
    }
}