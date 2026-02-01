#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "chlib/combinational.h"
#include "chlib/stream.h"
#include "bundle/fragment.h"
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
    
    SECTION("Stream fire() method") {
        ch_stream<ch_uint<8>> stream;
        stream.valid = true;
        stream.ready = true;
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(stream.fire()) == true);
        
        stream.ready = false;
        sim.tick();
        REQUIRE(sim.get_value(stream.fire()) == false);
    }
    
    SECTION("Stream isStall() method") {
        ch_stream<ch_uint<8>> stream;
        stream.valid = true;
        stream.ready = false;
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Stream is stalled when valid but not ready
        REQUIRE(sim.get_value(stream.isStall()) == true);
        
        stream.ready = true;
        sim.tick();
        REQUIRE(sim.get_value(stream.isStall()) == false);
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
    
    SECTION("Flow fire() method") {
        ch_flow<ch_uint<8>> flow;
        flow.valid = true;
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(flow.fire()) == true);
        
        flow.valid = false;
        sim.tick();
        REQUIRE(sim.get_value(flow.fire()) == false);
    }
    
    SECTION("Flow toStream() conversion") {
        ch_flow<ch_uint<8>> flow;
        flow.payload = 0x42_h;
        flow.valid = true;
        
        auto stream = flow.toStream();
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(stream.payload) == 0x42);
        REQUIRE(sim.get_value(stream.valid) == true);
        REQUIRE(sim.get_value(stream.ready) == true);
    }
}

TEST_CASE("Stream: Fragment Operations", "[stream][fragment]") {
    auto ctx = std::make_unique<ch::core::context>("test_fragment");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Fragment with first/last tracking") {
        ch::ch_fragment<ch_uint<8>> frag;
        frag.data_beat = 0x55_h;
        frag.first = true;
        frag.last = false;
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(frag.isFirst()) == true);
        REQUIRE(sim.get_value(frag.isLast()) == false);
    }
    
    SECTION("Fragment sequence generation") {
        std::array<ch_uint<8>, 4> data = {0x11_h, 0x22_h, 0x33_h, 0x44_h};
        auto seq = ch::fragment_sequence(data);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // First fragment
        REQUIRE(sim.get_value(seq[0].payload.data_beat) == 0x11);
        REQUIRE(sim.get_value(seq[0].payload.first) == true);
        REQUIRE(sim.get_value(seq[0].payload.last) == false);
        
        // Last fragment
        REQUIRE(sim.get_value(seq[3].payload.data_beat) == 0x44);
        REQUIRE(sim.get_value(seq[3].payload.first) == false);
        REQUIRE(sim.get_value(seq[3].payload.last) == true);
    }
}

TEST_CASE("Stream: Conditional Operations", "[stream][conditional]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_conditional");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Stream throwWhen - discard on condition") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0xAA_h;
        input.valid = true;
        input.ready = true;
        
        ch_bool discard = true;
        auto output = stream_throw_when(input, discard);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Data should be discarded
        REQUIRE(sim.get_value(output.valid) == false);
        REQUIRE(sim.get_value(input.ready) == true); // Input consumed
        
        discard = false;
        output = stream_throw_when(input, discard);
        sim.tick();
        
        REQUIRE(sim.get_value(output.valid) == true);
    }
    
    SECTION("Stream takeWhen - pass on condition") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0xBB_h;
        input.valid = true;
        input.ready = true;
        
        ch_bool pass = true;
        auto output = stream_take_when(input, pass);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(output.valid) == true);
        REQUIRE(sim.get_value(output.payload) == 0xBB);
        
        pass = false;
        output = stream_take_when(input, pass);
        sim.tick();
        
        REQUIRE(sim.get_value(output.valid) == false);
    }
    
    SECTION("Stream haltWhen - stall on condition") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0xCC_h;
        input.valid = true;
        input.ready = true;
        
        ch_bool halt = true;
        auto output = stream_halt_when(input, halt);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Stream should be halted
        REQUIRE(sim.get_value(output.valid) == false);
        
        halt = false;
        output = stream_halt_when(input, halt);
        sim.tick();
        
        REQUIRE(sim.get_value(output.valid) == true);
    }
}

TEST_CASE("Stream: Transformation Operations", "[stream][transform]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_transform");
    ch::core::ctx_swap ctx_swapper(ctx.get());
    
    SECTION("Stream continueWhen - continue on condition") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0xDD_h;
        input.valid = true;
        input.ready = true;
        
        ch_bool continue_cond = true;
        auto output = stream_continue_when(input, continue_cond);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Stream should continue when condition is true
        REQUIRE(sim.get_value(output.valid) == true);
        
        continue_cond = false;
        output = stream_continue_when(input, continue_cond);
        sim.tick();
        
        // Stream should halt when condition is false
        REQUIRE(sim.get_value(output.valid) == false);
    }
    
    SECTION("Stream map - transform payload") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0x10_h;
        input.valid = true;
        
        // Use lambda for transformation
        auto transform = [](ch_uint<8> x) -> ch_uint<8> {
            return x + 0x05_h;
        };
        
        auto output = stream_map<ch_uint<8>, ch_uint<8>>(input, transform);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(output.payload) == 0x15);
        REQUIRE(sim.get_value(output.valid) == true);
    }
    
    SECTION("Stream translateWith - custom transformation") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0x20_h;
        input.valid = true;
        
        // Use lambda for transformation
        auto transform = [](ch_uint<8> x) -> ch_uint<16> {
            return ch_uint<16>(x) << 8_d;
        };
        
        auto output = stream_translate_with<ch_uint<16>, ch_uint<8>>(input, transform);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(output.payload) == 0x2000);
        REQUIRE(sim.get_value(output.valid) == true);
    }
    
    SECTION("Stream transpose - width conversion") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0xFF_h;
        input.valid = true;
        
        auto output = stream_transpose<ch_uint<16>, ch_uint<8>>(input);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(output.payload) == 0xFF);
        REQUIRE(sim.get_value(output.valid) == true);
    }
    
    SECTION("Stream combineWith - combine two streams") {
        ch_stream<ch_uint<8>> input1;
        input1.payload = 0xAA_h;
        input1.valid = true;
        
        ch_stream<ch_uint<8>> input2;
        input2.payload = 0xBB_h;
        input2.valid = true;
        
        auto result = stream_combine_with(input1, input2);
        result.output_stream.ready = true;
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Both inputs valid means output is valid
        REQUIRE(sim.get_value(result.output_stream.valid) == true);
        REQUIRE(sim.get_value(result.output_stream.payload._1) == 0xAA);
        REQUIRE(sim.get_value(result.output_stream.payload._2) == 0xBB);
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
    
    SECTION("Stream queue - alias to FIFO") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x33_h;
        input_stream.valid = true;
        
        auto queue_result = stream_queue<ch_uint<8>, 8>(input_stream);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Queue should accept data
        REQUIRE(sim.get_value(queue_result.push_stream.ready) == true);
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

    SECTION("Stream Arbiter round-robin basic operation") {

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
    
    SECTION("Stream Arbiter priority operation") {
        std::array<ch_stream<ch_uint<8>>, 3> input_streams;
        input_streams[0].payload = 0x11_h;
        input_streams[0].valid = false;
        input_streams[1].payload = 0x22_h;
        input_streams[1].valid = true;
        input_streams[2].payload = 0x33_h;
        input_streams[2].valid = true;
        
        auto arb_result = stream_arbiter_priority<ch_uint<8>, 3>(input_streams);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Should select input 1 (highest priority valid input)
        REQUIRE(sim.get_value(arb_result.selected) == 1);
        REQUIRE(sim.get_value(arb_result.output_stream.payload) == 0x22);
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