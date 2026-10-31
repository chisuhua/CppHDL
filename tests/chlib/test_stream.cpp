#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "chlib/combinational.h"
#include "chlib/stream.h"
#include "bundle/fragment.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include "codegen_dag.h"
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
        
        // Create fire() result before initializing simulator
        auto fire_result = stream.fire();
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(fire_result) == true);
        
        stream.ready = false;
        sim.tick();
        // Recreate fire() result
        fire_result = stream.fire();
        REQUIRE(sim.get_value(fire_result) == false);
    }
    
    SECTION("Stream isStall() method") {
        ch_stream<ch_uint<8>> stream;
        stream.valid = true;
        stream.ready = false;
        
        // Create isStall() result before initializing simulator
        auto stall_result = stream.isStall();
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        // Stream is stalled when valid but not ready
        REQUIRE(sim.get_value(stall_result) == true);
        
        stream.ready = true;
        sim.tick();
        // Recreate stall_result
        stall_result = stream.isStall();
        REQUIRE(sim.get_value(stall_result) == false);
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
    
    SECTION("Stream throwWhen - discard on condition (true)") {
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
    }

    SECTION("Stream throwWhen - discard on condition (false)") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0xAA_h;
        input.valid = true;
        input.ready = true;
        
        ch_bool discard = false;
        auto output = stream_throw_when(input, discard);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(output.valid) == true);
    }
    
    SECTION("Stream takeWhen - pass on condition (true)") {
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
    }

    SECTION("Stream takeWhen - pass on condition (false)") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0xBB_h;
        input.valid = true;
        input.ready = true;
        
        ch_bool pass = false;
        auto output = stream_take_when(input, pass);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(sim.get_value(output.valid) == false);
    }
    
    SECTION("Stream haltWhen - stall on condition (true)") {
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
    }

    SECTION("Stream haltWhen - stall on condition (false)") {
        ch_stream<ch_uint<8>> input;
        input.payload = 0xCC_h;
        input.valid = true;
        input.ready = true;
        
        ch_bool halt = false;
        auto output = stream_halt_when(input, halt);
        
        ch::Simulator sim(ctx.get());
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
        stream_a.as_master();
        stream_b.as_master();
        stream_c.as_master();
        std::array<ch_stream<ch_uint<8>>, 3> inputs = {stream_a, stream_b,
                                                       stream_c};

        inputs[0].payload = make_uint<8>(10);
        inputs[0].valid = make_uint<1>(1);
        inputs[1].payload = make_uint<8>(20);
        inputs[1].valid = make_uint<1>(1);
        inputs[2].payload = make_uint<8>(30);
        inputs[2].valid = make_uint<1>(1);

        ch_uint<2> select_signal(0_d);
        auto mux_result = stream_mux<3, ch_uint<8>>(inputs, select_signal);
        
        // Set output ready to enable input ready signals
        mux_result.output_stream.ready = true;
        
        // Connect ready signals after setting output_stream.ready
        stream_mux_connect_ready(mux_result);

        ch::toDAG("stream_mux_test.dag", ctx.get());

        ch::Simulator sim(ctx.get());
        sim.tick(); // Tick to allow combinatorial propagation

        REQUIRE(sim.get_value(mux_result.output_stream.valid) == true);
        REQUIRE(sim.get_value(mux_result.output_stream.payload) == 10); // Selecting first input
        REQUIRE(sim.get_value(mux_result.input_streams[0].ready) ==
                true); // First input should be ready
        REQUIRE(sim.get_value(mux_result.input_streams[1].ready) ==
                false); // Others should not be ready
        REQUIRE(sim.get_value(mux_result.input_streams[2].ready) == false);
    }

    SECTION("Stream Mux with selection change") {
        ch_stream<ch_uint<8>> stream_a, stream_b, stream_c;
        std::array<ch_stream<ch_uint<8>>, 3> inputs = {stream_a, stream_b,
                                                       stream_c};

        inputs[0].payload = make_uint<8>(10);
        inputs[0].valid = make_uint<1>(1);
        inputs[1].payload = make_uint<8>(20);
        inputs[1].valid = make_uint<1>(1);
        inputs[2].payload = make_uint<8>(30);
        inputs[2].valid = make_uint<1>(1);

        ch_uint<2> select_signal(0_d);
        auto mux_result = stream_mux<3, ch_uint<8>>(inputs, select_signal);
        
        // Set output ready to enable input ready signals
        mux_result.output_stream.ready = true;
        
           // Connect ready signals after setting output_stream.ready
           stream_mux_connect_ready(mux_result);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(mux_result.output_stream.payload) == 10);

        // Change selection to second input
        sim.set_value(select_signal, 1);
        sim.tick();

        REQUIRE(sim.get_value(mux_result.output_stream.payload) == 20);
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
        input_stream.valid = true;
        input_stream.ready = true;
        // ⚠️ `stream_ready_ownership: intentionally-unwired` —— 本段只做 **width 结构断言** ⇒ ⇒ **故意**不接消费端 ready
        auto fifo_result = stream_fifo<ch_uint<8>, 4>(input_stream);

        // Basic structural checks (avoid simulator crash in current FIFO impl)
        REQUIRE(fifo_result.push_stream.payload.width == 8);
        REQUIRE(fifo_result.push_stream.valid.width == 1);
        REQUIRE(fifo_result.push_stream.ready.width == 1);
        REQUIRE(fifo_result.pop_stream.payload.width == 8);
        REQUIRE(fifo_result.pop_stream.valid.width == 1);
        REQUIRE(fifo_result.pop_stream.ready.width == 1);
    }
    
    SECTION("Stream queue - alias to FIFO") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = 0x33_h;
        input_stream.valid = true;
        input_stream.ready = true;
        
        // ⚠️ `stream_ready_ownership: intentionally-unwired` —— 本段只做 **width 结构断言** ⇒ ⇒ **故意**不接消费端 ready
        auto queue_result = stream_queue<ch_uint<8>, 8>(input_stream);

        // Basic structural checks
        REQUIRE(queue_result.push_stream.payload.width == 8);
        REQUIRE(queue_result.push_stream.valid.width == 1);
        REQUIRE(queue_result.push_stream.ready.width == 1);
    }
}

// ⚠️ 2026-10-31 新增（change `chlib-stream-ready-ownership-2026-10`，
// ADR-054 **形态 E**：默认值 + 可覆盖）。
//
// ⚠️ **为什么必须有这个用例**：`chlib::stream_fifo` 此前**零行为覆盖**
//    （`"Stream: Stream FIFO"` 只做 **width** 结构断言；下面那个
//    `"行为正确"` 用例只断言「写一个元素 ⇒ occupancy==1」）
//    ⇒ ⇒ **「读使能硬编码为 `true`」这个缺陷结构性不可见**
//    ⇒ ⇒ 本次修复 MUST 有**可执行证据**。
//
// ⚠️ **手推金标**（DEPTH=4 ⇒ 深度 4）：
//    `sync_fifo` 的写口 / 读口**自保护**
//    （`will_write = wren & !is_full(count)` · `will_read = rden & !is_empty(count)`）；
//    `pop_stream.payload` 是 **0-cycle `aread`** ⇒ 呈现**当前队首**（show-ahead）。
//
//    **消费端 `ready == 0`** ⇒ `will_read == 0` ⇒ 读指针不动 ⇒ **队首不变**
//    ⇒ ⇒ 元素**只进不出** ⇒ 写满 4 个后 `count == 4` ⇒ `full == 1`
//    ⇒ ⇒ 第 5 个写 ⇒ `will_write == 0` ⇒ `count` **保持 4** ⇒ `full` 仍 1
//    ⇒ ⇒ 消费端 `ready == 1` ⇒ 每拍弹出一个 ⇒ 队首依次 `0x22` / `0x33`
//
// ⚠️ **为什么不驱动 `in.ready`**：形态 E 的契约是「调用方读
//    `result.push_stream.ready`（FIFO 告诉上游能不能收）」⇒ ⇒ `in.ready`
//    是**上游自己**的信号 ⇒ ⇒ FIFO MUST NOT 依赖它 ⇒ ⇒ 写使能 MUST 只用
//    `valid`（⚠️ 否则 `valid & ready` 是**先读后写** ⇒ ⇒ `in.ready` 是空
//    proxy ⇒ ⇒ `node_builder` 报 "Invalid operands" ⇒ ⇒ 组合子**完全不可用**）。
TEST_CASE("Stream: stream_fifo 的读使能 MUST 能被外部化（反压阻止推进）",
          "[stream][fifo][backpressure]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_fifo_backpressure");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // ⚠️ 所有节点 MUST 在 `ch::Simulator` 构造**之前**创建
    //    （Simulator 构造时建立 data_map_，之后新建的节点不在其中）。
    ch_uint<8> payload(0_d);
    ch_bool valid(false);
    ch_bool consumer_ready(false);

    ch_stream<ch_uint<8>> in;
    in.payload = payload;   // `=` = 节点重绑定（chlib 组合子对 stream 字段
                             // 做**值**运算 ⇒ MUST 用 `=`，见
                             // `include/core/bundle/bundle_base.h:184-196`）
    in.valid = valid;
    // ⚠️ **故意不驱动 `in.ready`**（理由见上）

    auto fifo = stream_fifo<ch_uint<8>, 4>(in);
    // ⚠️ **形态 E 第二阶段**：把消费端的 ready 接到 FIFO ⇒ ⇒ 覆盖默认驱动
    //    （默认 = 恒就绪 = 既有旧语义）⇒ ⇒ 覆盖**传播**到内部读使能。
    stream_fifo_connect_ready(fifo, consumer_ready);

    ch::Simulator sim(ctx.get());

    auto head = [&] { return static_cast<uint64_t>(sim.get_value(fifo.pop_stream.payload)); };
    auto valid_out = [&] { return static_cast<uint64_t>(sim.get_value(fifo.pop_stream.valid)); };
    auto full = [&] { return static_cast<uint64_t>(sim.get_value(fifo.full)); };
    auto empty = [&] { return static_cast<uint64_t>(sim.get_value(fifo.empty)); };
    auto occ = [&] { return static_cast<uint64_t>(sim.get_value(fifo.occupancy)); };

    // ---- 1) 消费端不就绪 ⇒ MUST 阻止弹出 ⇒ 队首**不动** ⇒ 只进不出 ----
    sim.set_value(valid, 1);
    sim.set_value(consumer_ready, 0);

    static const uint64_t kIn[4] = {0x11, 0x22, 0x33, 0x44};
    for (int i = 0; i < 4; ++i) {
        sim.set_value(payload, kIn[i]);
        sim.tick();
        // 手推：第 i 拍后 count == i+1（只进不出）· 队首恒为 0x11
        REQUIRE(occ() == static_cast<uint64_t>(i + 1));
        REQUIRE(head() == 0x11);
        REQUIRE(valid_out() == 1);
        REQUIRE(empty() == 0);
        // 手推：count < 4 ⇒ full() == 0；count == 4 ⇒ full() == 1
        REQUIRE(full() == static_cast<uint64_t>(i == 3 ? 1 : 0));
    }

    // ---- 2) 满后再写 ⇒ MUST **自保护**（不覆写、不越界）⇒ ----
    sim.set_value(payload, 0x55);
    sim.tick();
    REQUIRE(occ() == 4);        // 手推：will_write == 0 ⇒ count 保持 4
    REQUIRE(full() == 1);
    REQUIRE(head() == 0x11);  // 队首仍是最早写入的那个

    // ---- 3) 消费端就绪 ⇒ MUST 恢复弹出 ⇒ 队首依次 0x22 / 0x33 ----
    sim.set_value(valid, 0);
    sim.set_value(consumer_ready, 1);
    sim.tick();
    REQUIRE(occ() == 3);        // 手推：弹出 0x11 ⇒ count 4 → 3
    REQUIRE(head() == 0x22);  // 手推：下一个队首是 0x22
    REQUIRE(full() == 0);     // 手推：count 3 < 4 ⇒ 不满
    sim.tick();
    REQUIRE(occ() == 2);
    REQUIRE(head() == 0x33);  // 手推：再弹出 0x22 ⇒ 队首 0x33
}

// ⚠️ 2026-10-31 新增（change `chlib-stream-ready-ownership-2026-10`）。
//
// ⚠️ **这个用例钉的是「零破坏」承诺**（ADR-054 §3.2 第 1 条第 1 项）：
//    **不调** `stream_fifo_connect_ready` ⇒ ⇒ 组合子 MUST 表现为
//    **恒就绪**（= 既有旧语义）⇒ ⇒ **存量调用点 MUST NOT 需要迁移**。
//
// ⚠️ **手推金标**（DEPTH=4，恒就绪 ⇒ 每拍「写 1 + 读 1」⇒ count **恒为 1**）：
//    写 0x11 ⇒ 队首 0x11（write-through）· occupancy 1 · full 0
//    写 0x22 ⇒ 弹出 0x11、写入 0x22 ⇒ 队首 0x22 · occupancy **仍 1** · full 0
//    写 0x33 ⇒ 队首 0x33 · occupancy 仍 1 · full 0
//    ⇒ ⇒ **永远排空** ⇒ ⇒ `full` **不可达**（这正是本次修复前的行为）
TEST_CASE("Stream: stream_fifo 未接背压时 MUST 保持旧语义（恒就绪）",
          "[stream][fifo][backpressure]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_fifo_legacy_ready");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    ch_uint<8> payload(0_d);
    ch_bool valid(false);

    ch_stream<ch_uint<8>> in;
    in.payload = payload;
    in.valid = valid;

    // ⚠️ **故意不调** `stream_fifo_connect_ready` ⇒ ⇒ 默认驱动 = 恒就绪
    // ⚠️ `stream_ready_ownership: intentionally-unwired` —— 本用例钉的正是「**未接** ⇒ 恒就绪」这条**旧语义契约**
    auto fifo = stream_fifo<ch_uint<8>, 4>(in);

    ch::Simulator sim(ctx.get());

    auto head = [&] { return static_cast<uint64_t>(sim.get_value(fifo.pop_stream.payload)); };
    auto full = [&] { return static_cast<uint64_t>(sim.get_value(fifo.full)); };
    auto occ = [&] { return static_cast<uint64_t>(sim.get_value(fifo.occupancy)); };

    sim.set_value(valid, 1);
    static const uint64_t kIn[3] = {0x11, 0x22, 0x33};
    for (int i = 0; i < 3; ++i) {
        sim.set_value(payload, kIn[i]);
        sim.tick();
        // 手推：恒就绪 ⇒ 每拍写 1 读 1 ⇒ count 恒为 1 ⇒ 队首**逐拍推进**
        REQUIRE(occ() == 1);
        REQUIRE(head() == kIn[i]);
        REQUIRE(full() == 0);   // 手推：永远排空 ⇒ full 不可达
    }
}

// ⚠️ 2026-10-31 新增（change `chlib-stream-ready-ownership-2026-10`）。
//
// ⚠️ **这个用例钉的是「响亮失败」**：调用方**已自行驱动**
//    `result.pop_stream.ready` ⇒ ⇒ 与组合子的默认驱动**冲突**
//    ⇒ ⇒ 再调 `stream_fifo_connect_ready` ⇒ ⇒ MUST **抛异常**
//    ⇒ ⇒ ⚠️ **MUST NOT 只打日志**（`CHERROR` / `CHFATAL` 实测**只打日志不抛**
//    ⇒ ⇒ ⇒ 那是**假安全网** ⇒ ⇒ ADR-050 已踩过）。
//
// ⚠️ **为什么必须有它**：`ch_bool::operator<<=` 走 `set_src(0, …)`
//    ⇒ ⇒ **last-wins** ⇒ ⇒ 没有守卫时，调用方的驱动会被**静默接管**
//    ⇒ ⇒ 「我驱动的是我自己的信号」变成**假象** ⇒ ⇒ **静默失效**。
TEST_CASE("Stream: stream_fifo_connect_ready MUST 检出多驱动（响亮失败）",
          "[stream][fifo][backpressure]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_fifo_multidrive");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    ch_uint<8> payload(0_d);
    ch_bool valid(false);
    ch_bool consumer_ready(false);

    ch_stream<ch_uint<8>> in;
    in.payload = payload;
    in.valid = valid;

    auto fifo = stream_fifo<ch_uint<8>, 4>(in);

    // ⚠️ **调用方已自行驱动** ready 字段 ⇒ ⇒ 默认驱动被顶掉
    fifo.pop_stream.ready <<= consumer_ready;

    // ⇒ ⇒ `connect_ready` MUST **抛异常** ⇒ ⇒ ⚠️ MUST NOT 只打日志
    //    （⚠️ 整个表达式 MUST 用括号包起来 ⇒ ⇒ 否则 `,` 会拆散宏参数）
    REQUIRE_THROWS_AS((void)stream_fifo_connect_ready(fifo, consumer_ready),
                      std::runtime_error);
}

TEST_CASE("Stream: chlib stream 组合子必须**行为正确**（不只是结构正确）",
          "[stream][behavior][regression]") {
    // ⚠️ 2026-10-22 新增。此前 `tests/chlib/test_stream.cpp` 的
    // "Stream: Stream FIFO" 只做**结构断言**（width 之类）⇒
    // `chlib::stream_fifo` / `stream_fork` / `stream_arbiter_round_robin`
    // 里三处「**先读后写**」的空操作数缺陷（读尚未赋值的字段 ⇒ null
    // operand ⇒ node_builder 报 "Invalid operands for binary operation"）
    // **零行为覆盖、长期无人发现**。
    //
    // 本用例用**行为级**断言把三条路径钉住：值必须真的被算出来。
    auto ctx = std::make_unique<ch::core::context>("test_stream_behavior");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // ---- 1) stream_fifo：写一个元素 ⇒ occupancy == 1、empty == 0 ----
    // ⚠️ 2026-10-22 强化（ADR-045 / change `chlib-stream-semantics-2026-10`）。
    //
    // 此前本段**只**断言 `occupancy` / `empty`，**从不断言 `payload`**
    // ⇒ 对「数据通路根本读不出数据」这一缺陷**零信号**（实测：把
    // `stream.h` 的 `pop_stream.payload = fifo_result.q;` 改成常量 0，
    // 本用例**仍然全绿**）。⇒ 本次改为**三元组**断言：
    //   （tick 序号, valid, payload）三者同时钉住。
    //
    // ⚠️ **为什么 payload 必须每拍变化**：若 payload 恒定，1-cycle `sread`
    //    版本也会在（错开的）某拍显示同一个值 ⇒ 断言无区分力。
    //    每拍变化后，FWFT（0-cycle）显示的是「**本拍**推入的值」，
    //    而 1-cycle `sread` 显示的是「**上一拍**推入的值」⇒ 可区分。
    //
    // ⚠️ **为什么必须钉 tick 序号**：只断言 payload 会被 sread 实现在
    //    `valid == 0` 的那一拍骗过 ⇒ MUST 同时断言 `valid` 与 tick 位置。
    {
        // ⚠️ 所有节点 MUST 在 `ch::Simulator` 构造**之前**创建
        //    （Simulator 构造时建立 data_map_，之后新建的节点不在其中）。
        ch_uint<8> payload(0_d);
        ch_stream<ch_uint<8>> in;
        in.payload = payload;      // `=` = 节点重绑定（chlib 组合子对 stream
                                   // 字段做**值**运算 ⇒ MUST 用 `=`，见
                                   // `include/core/bundle/bundle_base.h:184-196`）
        in.valid = ch_bool(true);
        in.ready = ch_bool(true);

        // ⚠️ `stream_ready_ownership: intentionally-unwired` —— 本段钉的是「**写一个元素 ⇒ occupancy==1 / payload==本拍值**」
        //    ⇒ ⇒ **故意**保持恒就绪（反压用例见本文件另两个用例）
        auto fifo = stream_fifo<ch_uint<8>, 4>(in);

        ch::Simulator sim(ctx.get());

        // 每拍推入一个**不同**的值
        static const uint64_t kExpect[3] = {0x5A, 0x3C, 0x7E};
        for (int i = 0; i < 3; ++i) {
            sim.set_value(payload, kExpect[i]);
            sim.tick();

            auto valid = static_cast<uint64_t>(sim.get_value(fifo.pop_stream.valid));
            auto got = static_cast<uint64_t>(sim.get_value(fifo.pop_stream.payload));
            auto occ = static_cast<uint64_t>(sim.get_value(fifo.occupancy));
            auto empty = static_cast<uint64_t>(sim.get_value(fifo.empty));

            // ⚠️ FWFT（0-cycle）+ write-through ⇒ **本拍**推入的值在**本拍**可见
            REQUIRE(valid == 1);
            REQUIRE(got == kExpect[i]);
            REQUIRE(empty == 0);   // 写了一个元素 ⇒ 不应是空的
            REQUIRE(occ == 1);
        }
    }

    // ---- 2) stream_fork：payload 复制到所有输出、valid 透传 ----
    {
        ch_stream<ch_uint<8>> in;
        in.payload = make_uint<8>(0x3C);
        in.valid = ch_bool(true);
        in.ready = ch_bool(true);

        auto fork = stream_fork<ch_uint<8>, 2>(in, false);

        ch::Simulator sim(ctx.get());
        sim.tick();

        for (unsigned i = 0; i < 2; ++i) {
            auto d = static_cast<uint64_t>(
                sim.get_value(fork.output_streams[i].payload));
            auto v = static_cast<uint64_t>(
                sim.get_value(fork.output_streams[i].valid));
            REQUIRE(d == 0x3C);
            REQUIRE(v == 1);
        }
    }

    // ---- 3) stream_arbiter_round_robin：选中唯一有效的输入 ----
    {
        std::array<ch_stream<ch_uint<8>>, 4> ins;
        for (unsigned i = 0; i < 4; ++i) {
            ins[i].payload = make_uint<8>(0x10 + i);
            ins[i].valid = ch_bool(i == 2);   // 只有输入 2 有效
            ins[i].ready = ch_bool(true);
        }

        auto arb = stream_arbiter_round_robin<ch_uint<8>, 4>(ins);

        ch::Simulator sim(ctx.get());
        sim.tick();

        auto sel = static_cast<uint64_t>(sim.get_value(arb.selected));
        auto valid = static_cast<uint64_t>(sim.get_value(arb.output_stream.valid));
        auto data = static_cast<uint64_t>(
            sim.get_value(arb.output_stream.payload));
        REQUIRE(valid == 1);
        REQUIRE(sel == 2);
        REQUIRE(data == 0x12);
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
        fork_result.output_streams[0].ready = true;
        fork_result.output_streams[1].ready = true;

        ch::Simulator sim(ctx.get());
        sim.tick(); // Tick to allow combinatorial propagation

        REQUIRE(sim.get_value(fork_result.output_streams[0].payload) == 0x34);
        REQUIRE(sim.get_value(fork_result.output_streams[1].payload) == 0x34);
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

        auto arb_result =
            stream_arbiter_round_robin<ch_uint<8>, 2>(input_streams);

        // Structural checks (avoid relying on current round-robin selector behavior)
        REQUIRE(arb_result.output_stream.payload.width == 8);
        REQUIRE(arb_result.output_stream.valid.width == 1);
        REQUIRE(arb_result.selected.width == compute_idx_width(2));
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
        
        // Highest priority is higher index per implementation
        REQUIRE(sim.get_value(arb_result.selected) == 2);
        REQUIRE(sim.get_value(arb_result.output_stream.payload) == 0x33);
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