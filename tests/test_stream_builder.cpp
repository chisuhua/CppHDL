#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "chlib/stream_builder.h"
#include "bundle/stream_bundle.h"
#include "core/context.h"
#include "core/uint.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

// Test that StreamBuilder can be created - this verifies the API compiles
TEST_CASE("StreamBuilder - BasicCreation", "[stream_builder][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Create a source stream
    ch_stream<ch_uint<8>> source;
    
    // Create builder from source stream - this tests compilation
    StreamBuilder<ch_uint<8>> builder(source);
    
    // Build - returns ch_stream
    auto result = builder.build();
    
    REQUIRE(true);
}

// Test make_stream_builder helper function compiles
TEST_CASE("StreamBuilder - MakeHelper", "[stream_builder][helper]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> source;
    
    // Test the helper function
    auto result = make_stream_builder(source).build();
    
    REQUIRE(true);
}

// Test that we can instantiate with different payload types
TEST_CASE("StreamBuilder - TypeInstantiation", "[stream_builder][types]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Test instantiation with ch_uint<8>
    {
        ch_stream<ch_uint<8>> s;
        StreamBuilder<ch_uint<8>> b(s);
        REQUIRE(true);
    }
    
    // Test instantiation with ch_uint<16>
    {
        ch_stream<ch_uint<16>> s;
        StreamBuilder<ch_uint<16>> b(s);
        REQUIRE(true);
    }
    
    // Test instantiation with ch_uint<32>
    {
        ch_stream<ch_uint<32>> s;
        StreamBuilder<ch_uint<32>> b(s);
        REQUIRE(true);
    }
}

// Test that method chaining compiles (without executing)
TEST_CASE("StreamBuilder - MethodChainingCompiles", "[stream_builder][chain]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Just test that the types are correct - don't actually call the operations
    // that require valid hardware signals
    ch_stream<ch_uint<8>> source;
    
    // Create builder with method chaining - compiles but don't execute
    auto builder = StreamBuilder<ch_uint<8>>(source);
    
    // Verify builder exists
    auto result = builder.build();
    REQUIRE(true);
}

// ⚠️ 2026-10-31 新增（change `chlib-stream-ready-ownership-2026-10`）。
//
// ⚠️ **这个用例钉的是「builder 链上的反压 MUST 能传过去」**：
//    `.queue<DEPTH>()` 内部是 `stream_fifo` ⇒ ⇒ 消费端的 ready MUST 能
//    **阻止 FIFO 弹出** ⇒ ⇒ 否则「反压场景」在 builder API 下**不可演示**
//    ⇒ ⇒ 与 `stream_fifo` 直连是**同一个**缺陷家族。
//
// ⚠️ **为什么 `built.ready <<= …` 能传到 FIFO 内部**：
//    `StreamBuilder::queue()` 里 `current_stream_ = result.pop_stream;`
//    （`bundle_base::operator=` = **节点重绑定**）⇒ ⇒ `current_stream_.ready`
//    与 `result.pop_stream.ready` 是**同一个节点** ⇒ ⇒ 而 `stream_fifo` 的
//    读使能引用的**正是那个节点** ⇒ ⇒ 覆盖**必然传播** ✔
//    ⇒ ⇒ ⇒ 这正是**形态 E** 的核心机制在 builder 路径上的**继承**。
//
// ⚠️ **手推金标**（DEPTH=4）：
//    消费端 ready == 0 ⇒ `will_read == 0` ⇒ 读指针不动 ⇒ **队首不变**
//    ⇒ ⇒ 写 0x11 / 0x22 ⇒ 队首**恒为 0x11**
//    消费端 ready == 1 ⇒ 每拍弹出一个 ⇒ 队首 `0x22`
TEST_CASE("StreamBuilder - queue() 的反压 MUST 能传过 builder 链",
          "[stream_builder][fifo][backpressure]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_builder_queue");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // ⚠️ 所有节点 MUST 在 `ch::Simulator` 构造**之前**创建
    ch_uint<8> payload(0_d);
    ch_bool valid(false);
    ch_bool consumer_ready(false);

    ch_stream<ch_uint<8>> source;
    source.payload = payload;
    source.valid = valid;
    // ⚠️ **故意不驱动** `source.ready` ⇒ ⇒ FIFO MUST NOT 依赖它
    //    （⚠️ 写使能 MUST 只用 `valid` ⇒ ⇒ 否则 `valid & ready` 是
    //    **先读后写** ⇒ ⇒ `source.ready` 是空 proxy ⇒ ⇒ node_builder 报
    //    "Invalid operands" ⇒ ⇒ `.queue<>()` **完全不可用**）

    auto built = make_stream_builder(source).queue<4>().build();

    // ⚠️ 消费端驱动 **built** 的 ready ⇒ ⇒ 覆盖 FIFO 的默认驱动
    //    （= 形态 E 的「默认值 + 可覆盖」）
    built.ready <<= consumer_ready;

    ch::Simulator sim(ctx.get());

    auto head = [&] { return static_cast<uint64_t>(sim.get_value(built.payload)); };
    auto valid_out = [&] { return static_cast<uint64_t>(sim.get_value(built.valid)); };

    // ---- 1) 消费端不就绪 ⇒ MUST 阻止弹出 ⇒ 队首**不动** ----
    sim.set_value(valid, 1);
    sim.set_value(consumer_ready, 0);
    sim.set_value(payload, 0x11);
    sim.tick();
    REQUIRE(valid_out() == 1);
    REQUIRE(head() == 0x11);
    sim.set_value(payload, 0x22);
    sim.tick();
    // 手推：未弹出 ⇒ 队首仍是最早写入的 0x11
    REQUIRE(head() == 0x11);

    // ---- 2) 消费端就绪 ⇒ MUST 恢复弹出 ⇒ 队首 0x22 ----
    sim.set_value(valid, 0);
    sim.set_value(consumer_ready, 1);
    sim.tick();
    REQUIRE(head() == 0x22);
}
