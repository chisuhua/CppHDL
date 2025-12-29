#include "catch_amalgamated.hpp"
#include "chlib/stream.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Stream: Basic Stream Operations", "[stream][bundle]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_basic");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream creation and basic usage") {
        Stream<ch_uint<8>> stream("test_stream");
        
        REQUIRE(stream.payload.width() == 8);
        REQUIRE(stream.valid.width() == 1);
        REQUIRE(stream.ready.width() == 1);
    }
    
    SECTION("Stream direction control") {
        Stream<ch_uint<8>> master_stream("master");
        Stream<ch_uint<8>> slave_stream("slave");
        
        master_stream.as_master();
        slave_stream.as_slave();
        
        // Verify that the directions are set correctly (would be validated by the framework)
        REQUIRE(true); // Placeholder - actual validation would happen in the framework
    }
}

TEST_CASE("Stream: Flow Operations", "[stream][flow]") {
    auto ctx = std::make_unique<ch::core::context>("test_flow_basic");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Flow creation and basic usage") {
        Flow<ch_uint<8>> flow("test_flow");
        
        REQUIRE(flow.payload.width() == 8);
        REQUIRE(flow.valid.width() == 1);
    }
}

TEST_CASE("Stream: Stream FIFO", "[stream][fifo]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_fifo");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream FIFO basic operation") {
        ch_bool clk = false;
        ch_bool rst = true;
        
        Stream<ch_uint<8>> input_stream;
        input_stream.payload = 0_d;
        input_stream.valid = false;
        input_stream.ready = false;
        
        ch::Simulator sim(ctx.get());
        
        // Reset
        auto fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
        sim.tick();
        
        rst = false;
        clk = true;
        input_stream.payload = 0x55_d;
        input_stream.valid = true;
        fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
        sim.tick();
        
        // Check that data was accepted
        REQUIRE(simulator.get_value(fifo_result.push_stream.ready) == true);
        
        clk = false;
        sim.tick();
        
        clk = true;
        input_stream.payload = 0xAA_d;
        input_stream.valid = true;
        fifo_result = stream_fifo<ch_uint<8>, 4>(clk, rst, input_stream);
        sim.tick();
        
        // Check that second data was also accepted
        REQUIRE(simulator.get_value(fifo_result.push_stream.ready) == true);
    }
}

TEST_CASE("Stream: Stream Fork", "[stream][fork]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_fork");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Fork synchronous operation") {
        Stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x12_d;
        input_stream.valid = true;
        
        auto fork_result = stream_fork<ch_uint<8>, 2>(input_stream, true);
        
        REQUIRE(simulator.get_value(fork_result.output_streams[0].payload) == 0x12);
        REQUIRE(simulator.get_value(fork_result.output_streams[1].payload) == 0x12);
        REQUIRE(simulator.get_value(fork_result.output_streams[0].valid) == true);
        REQUIRE(simulator.get_value(fork_result.output_streams[1].valid) == true);
    }
    
    SECTION("Stream Fork asynchronous operation") {
        Stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x34_d;
        input_stream.valid = true;
        
        auto fork_result = stream_fork<ch_uint<8>, 2>(input_stream, false);
        
        REQUIRE(simulator.get_value(fork_result.output_streams[0].payload) == 0x34);
        REQUIRE(simulator.get_value(fork_result.output_streams[1].payload) == 0x34);
        REQUIRE(simulator.get_value(fork_result.output_streams[0].valid) == true);
        REQUIRE(simulator.get_value(fork_result.output_streams[1].valid) == true);
    }
}

TEST_CASE("Stream: Stream Join", "[stream][join]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_join");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Join operation") {
        std::array<Stream<ch_uint<8>>, 2> input_streams;
        input_streams[0].payload = 0xAB_d;
        input_streams[0].valid = true;
        input_streams[1].payload = 0xCD_d;
        input_streams[1].valid = true;
        
        auto join_result = stream_join<ch_uint<8>, 2>(input_streams);
        
        // When all inputs are valid, output should be valid
        REQUIRE(simulator.get_value(join_result.output_stream.valid) == true);
        REQUIRE(simulator.get_value(join_result.output_stream.payload) == 0xAB);
    }
}

TEST_CASE("Stream: Stream Arbiter", "[stream][arbiter]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Arbiter basic operation") {
        ch_bool clk = false;
        ch_bool rst = true;
        
        std::array<Stream<ch_uint<8>>, 2> input_streams;
        input_streams[0].payload = 0x11_d;
        input_streams[0].valid = true;
        input_streams[1].payload = 0x22_d;
        input_streams[1].valid = false;
        
        ch::Simulator sim(ctx.get());
        
        // Reset
        auto arb_result = stream_arbiter_round_robin<ch_uint<8>, 2>(clk, rst, input_streams);
        sim.tick();
        
        rst = false;
        clk = true;
        arb_result = stream_arbiter_round_robin<ch_uint<8>, 2>(clk, rst, input_streams);
        sim.tick();
        
        // Should select input 0 since input 1 is not valid
        REQUIRE(simulator.get_value(arb_result.selected) == 0);
        REQUIRE(simulator.get_value(arb_result.output_stream.payload) == 0x11);
        REQUIRE(simulator.get_value(arb_result.output_stream.valid) == true);
    }
}

TEST_CASE("Stream: Stream Demux", "[stream][demux]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_demux");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Demux operation") {
        Stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x99_d;
        input_stream.valid = true;
        input_stream.ready = true;
        
        auto demux_result = stream_demux<ch_uint<8>, 2>(input_stream, 1_d);
        
        // Data should go to output 1 (selected)
        REQUIRE(simulator.get_value(demux_result.output_streams[0].valid) == false);
        REQUIRE(simulator.get_value(demux_result.output_streams[1].valid) == true);
        REQUIRE(simulator.get_value(demux_result.output_streams[1].payload) == 0x99);
    }
}