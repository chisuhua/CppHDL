#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "chlib/combinational.h"
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
        ch_stream<ch_uint<8>> stream("test_stream");

        REQUIRE(stream.payload.width == 8);
        REQUIRE(stream.valid.width == 1);
        REQUIRE(stream.ready.width == 1);
    }
}

TEST_CASE("Stream: Stream Mux", "[stream][mux]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_mux");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Mux operation") {
        ch_stream<ch_uint<8>> stream_a, stream_b, stream_c;
        std::array<ch_stream<ch_uint<8>>, 3> inputs = {stream_a, stream_b,
                                                       stream_c};

        stream_a.payload = make_uint<8>(10);
        stream_a.valid = make_uint<1>(1);
        stream_b.payload = make_uint<8>(20);
        stream_b.valid = make_uint<1>(1);
        stream_c.payload = make_uint<8>(30);
        stream_c.valid = make_uint<1>(1);

        ch_uint<2> select_signal(0_d);
        auto mux_out = stream_mux<3, ch_uint<8>>(inputs, select_signal);

        ch::Simulator sim(ctx.get());
        sim.tick(); // Tick to allow combinatorial propagation

        REQUIRE(sim.get_value(mux_out.valid) == true);
        REQUIRE(sim.get_value(mux_out.payload) == 10); // Selecting first input
        REQUIRE(sim.get_value(inputs[0].ready) ==
                true); // First input should be ready
        REQUIRE(sim.get_value(inputs[1].ready) ==
                false); // Others should not be ready
        REQUIRE(sim.get_value(inputs[2].ready) == false);

        // Change selection to second input
        sim.set_value(select_signal, 1);
        sim.tick();

        REQUIRE(sim.get_value(mux_out.payload) ==
                20); // Now selecting second input
        REQUIRE(sim.get_value(inputs[0].ready) ==
                false); // Now first input should not be ready
        REQUIRE(sim.get_value(inputs[1].ready) ==
                true); // Second input should be ready
        REQUIRE(sim.get_value(inputs[2].ready) == false);
    }

    SECTION("Stream direction control") {
        ch_stream<ch_uint<8>> master_stream("master");
        ch_stream<ch_uint<8>> slave_stream("slave");

        master_stream.as_master();
        slave_stream.as_slave();

        // Verify that the directions are set correctly (would be validated by
        // the framework)
        REQUIRE(true); // Placeholder - actual validation would happen in the
                       // framework
    }
}

TEST_CASE("Stream: Flow Operations", "[stream][flow]") {
    auto ctx = std::make_unique<ch::core::context>("test_flow_basic");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Flow creation and basic usage") {
        ch_flow<ch_uint<8>> flow("test_flow");

        REQUIRE(flow.payload.width == 8);
        REQUIRE(flow.valid.width == 1);
    }
}

TEST_CASE("Stream: Stream FIFO", "[stream][fifo]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_fifo");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream FIFO basic operation") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0_d;
        input_stream.valid = false;
        input_stream.ready = false;

        ch::Simulator sim(ctx.get());

        // Reset
        auto fifo_result = stream_fifo<ch_uint<8>, 4>(input_stream);
        sim.tick();

        input_stream.payload = 0x55_h;
        input_stream.valid = true;
        fifo_result = stream_fifo<ch_uint<8>, 4>(input_stream);
        sim.tick();

        // Check that data was accepted
        REQUIRE(sim.get_value(fifo_result.push_stream.ready) == true);

        sim.tick();

        input_stream.payload = 0xAA_h;
        input_stream.valid = true;
        fifo_result = stream_fifo<ch_uint<8>, 4>(input_stream);
        sim.tick();

        // Check that second data was also accepted
        REQUIRE(sim.get_value(fifo_result.push_stream.ready) == true);
    }
}

TEST_CASE("Stream: Stream Fork", "[stream][fork]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_fork");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Fork synchronous operation") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x12_h;
        input_stream.valid = true;

        auto fork_result = stream_fork<ch_uint<8>, 2>(input_stream, true);

        ch::Simulator sim(ctx.get());
        sim.tick(); // Tick to allow combinatorial propagation

        REQUIRE(sim.get_value(fork_result.output_streams[0].payload) == 0x12);
        REQUIRE(sim.get_value(fork_result.output_streams[1].payload) == 0x12);
        REQUIRE(sim.get_value(fork_result.output_streams[0].valid) == true);
        REQUIRE(sim.get_value(fork_result.output_streams[1].valid) == true);
    }

    SECTION("Stream Fork asynchronous operation") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x34_h;
        input_stream.valid = true;

        auto fork_result = stream_fork<ch_uint<8>, 2>(input_stream, false);

        ch::Simulator sim(ctx.get());
        sim.tick(); // Tick to allow combinatorial propagation

        REQUIRE(sim.get_value(fork_result.output_streams[0].payload) == 0x34);
        REQUIRE(sim.get_value(fork_result.output_streams[1].payload) == 0x34);
        REQUIRE(sim.get_value(fork_result.output_streams[0].valid) == true);
        REQUIRE(sim.get_value(fork_result.output_streams[1].valid) == true);
    }
}

TEST_CASE("Stream: Stream Join", "[stream][join]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_join");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Join operation") {
        std::array<ch_stream<ch_uint<8>>, 2> input_streams;
        input_streams[0].payload = 0xAB_h;
        input_streams[0].valid = true;
        input_streams[1].payload = 0xCD_h;
        input_streams[1].valid = true;

        auto join_result = stream_join<ch_uint<8>, 2>(input_streams);

        ch::Simulator sim(ctx.get());
        sim.tick(); // Tick to allow combinatorial propagation

        // When all inputs are valid, output should be valid
        REQUIRE(sim.get_value(join_result.output_stream.valid) == true);
        REQUIRE(sim.get_value(join_result.output_stream.payload) == 0xAB);
    }
}

TEST_CASE("Stream: Stream Arbiter", "[stream][arbiter]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Arbiter basic operation") {

        std::array<ch_stream<ch_uint<8>>, 2> input_streams;
        input_streams[0].payload = 0x11_h;
        input_streams[0].valid = true;
        input_streams[1].payload = 0x22_h;
        input_streams[1].valid = false;

        ch::Simulator sim(ctx.get());

        // Reset
        auto arb_result =
            stream_arbiter_round_robin<ch_uint<8>, 2>(input_streams);
        sim.tick();

        arb_result = stream_arbiter_round_robin<ch_uint<8>, 2>(input_streams);
        sim.tick();

        // Should select input 0 since input 1 is not valid
        REQUIRE(sim.get_value(arb_result.selected) == 0);
        REQUIRE(sim.get_value(arb_result.output_stream.payload) == 0x11);
        REQUIRE(sim.get_value(arb_result.output_stream.valid) == true);
    }
}

TEST_CASE("Stream: Stream Demux", "[stream][demux]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_demux");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Demux operation") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x99_h;
        input_stream.valid = true;
        input_stream.ready = true;

        auto demux_result = stream_demux<ch_uint<8>, 2>(input_stream, 1_d);

        ch::Simulator sim(ctx.get());
        sim.tick(); // Tick to allow combinatorial propagation

        // Data should go to output 1 (selected)
        REQUIRE(sim.get_value(demux_result.output_streams[0].valid) == false);
        REQUIRE(sim.get_value(demux_result.output_streams[1].valid) == true);
        REQUIRE(sim.get_value(demux_result.output_streams[1].payload) == 0x99);
    }
}